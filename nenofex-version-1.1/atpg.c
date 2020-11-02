/*
 This file is part of Nenofex.

 Nenofex, an expansion-based QBF solver for negation normal form.        
 Copyright 2008, 2012, 2017 Florian Lonsing.

 Nenofex is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or (at
 your option) any later version.

 Nenofex is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Nenofex.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "stack.h"
#include "queue.h"
#include "nenofex_types.h"
#include "mem.h"

#define DEFAULT_STACK_SIZE 128
#define DEFAULT_QUEUE_SIZE 128

/*
- if sum of fwd- and bwd-propagations exceeds limit then abort
- default values; actual ones set dynamically depending on subgraph size 
*/
static unsigned int ATPG_PROPAGATION_LIMIT = 50000;
static unsigned int GLOBAL_FLOW_PROPAGATION_LIMIT = 50000;

/* 
- disabling recommended: print detailed information 
*/
#define PRINT_INFO_DETAILS 0

/*
- reset 'changed_subformula' only if it is necessary
- UNKNOWN: might slow down ATPG, possibly has no positive effects?
*/
#define KEEP_CHANGED_SUBFORMULA 0

/* 
- enable skipping of non-promising fault nodes in optimizations
- could save needless propagations but also miss redundancies / implications
- use with care
  - observation: enabling all four causes largest number of 
    redundancies/implications to be detected but incurs time/space overhead
    -> need to tune propagation limits
*/
#define ATPG_SKIP_FAULT_NODES_NO_LIT_CHILDREN 1
#define ATPG_SKIP_FAULT_NODES_OP_CHILDREN 1
#define GLOBAL_FLOW_SKIP_FAULT_NODES_NO_LIT_CHILDREN 1
#define GLOBAL_FLOW_SKIP_FAULT_NODES_OP_CHILDREN 1

/* 
- BUGGY: IF ENABLED, ASSUMPTIONS FOR RELINKING IMPLICANTS RATHER 
   THAN COPYING  D O  N O T  HOLD ANY LONGER
*/
#define GLOBAL_FLOW_LITERALS_BOTH_PHASES 0      /* DO NOT ENABLE */

/* 
- disabling recommended: will not update watchers assigned and justified nodes 
*/
#define ALWAYS_UPDATE_WATCHER 0

/* 
- if disabled: non-recursive version using a stack is used
- seems to be slower than recursive version?
*/
#define RECURSIVE_BWD_PROP 0

/*
- remove invalid subformula-occurrences of a variable on the fly 
- STILL TO BE TESTED
*/
#define CLEAN_UP_SUBFORMULA_OCC_LIST 1

/* 
- disabling recommended: option NDEBUG overules the following
- if enabled: possibly expensive, especially on large instances
*/
#define ASSERT_ALL_ATPG_INFO_RESET 0
#define ASSERT_CHILDREN_ASSIGNMENTS 0


#define collect_faults_marked(node) ((node)->mark2)
#define collect_faults_mark(node) ((node)->mark2 = 1)
#define collect_faults_unmark(node) ((node)->mark2 = 0)


void
mark_fault_node_as_deleted (FaultNode * fault_node)
{
  fault_node->deleted = 1;
}


static FaultNode *
create_fault_node (ATPGRedundancyRemover * atpg_rr, Node * node)
{
  size_t bytes = sizeof (FaultNode);
  FaultNode *result = (FaultNode *) mem_malloc (atpg_rr->mm, bytes);
  memset (result, 0, bytes);
  result->node = node;
  return result;
}


static void
delete_fault_node (ATPGRedundancyRemover * atpg_rr, FaultNode * fault_node)
{
  mem_free (atpg_rr->mm, fault_node, sizeof (FaultNode));
}


ATPGRedundancyRemover *
create_atpg_redundancy_remover (MemManager *mm)
{
  ATPGRedundancyRemover *result;
  size_t bytes = sizeof (ATPGRedundancyRemover);
  result = (ATPGRedundancyRemover *) mem_malloc (mm, bytes);
  assert (result);
  memset (result, 0, bytes);

  result->mm = mm;

  result->subformula_vars = create_stack (mm, DEFAULT_STACK_SIZE);
  result->fault_queue = create_queue (mm, DEFAULT_QUEUE_SIZE);
  result->propagation_queue = create_queue (mm, DEFAULT_QUEUE_SIZE);
  result->touched_nodes = create_stack (mm, DEFAULT_STACK_SIZE);
  result->bwd_prop_stack = create_stack (mm, DEFAULT_STACK_SIZE);
  result->fault_path_nodes = create_stack (mm, DEFAULT_STACK_SIZE);
  result->propagated_vars = create_stack (mm, DEFAULT_STACK_SIZE);
  return result;
}


static void
reset_atpg_redundancy_remover (ATPGRedundancyRemover * atpg_rr)
{
  reset_queue (atpg_rr->fault_queue);
  reset_queue (atpg_rr->propagation_queue);

  atpg_rr->conflict =
    atpg_rr->prop_cutoff =
    atpg_rr->global_flow_prop_cutoff =
    atpg_rr->atpg_prop_cutoff = atpg_rr->restricted_clean_up = 0;

  atpg_rr->collect_faults = 0;

  memset (&(atpg_rr->stats), 0, sizeof (atpg_rr->stats));

  atpg_rr->global_flow_fwd_prop_cnt =
    atpg_rr->global_flow_bwd_prop_cnt =
    atpg_rr->atpg_fwd_prop_cnt = atpg_rr->atpg_bwd_prop_cnt = 0;

  reset_stack (atpg_rr->touched_nodes);
  reset_stack (atpg_rr->fault_path_nodes);
  reset_stack (atpg_rr->bwd_prop_stack);
  reset_stack (atpg_rr->propagated_vars);

  ATPGInfo *atpg_info_p;

#ifndef NDEBUG
  ATPGInfo *end = atpg_rr->end_atpg_info;
#endif
  assert (atpg_rr->end_atpg_info == atpg_rr->atpg_info_array +
          atpg_rr->byte_size_atpg_info_array / sizeof (ATPGInfo));

  for (atpg_info_p = atpg_rr->atpg_info_array;
       atpg_info_p->fault_node; atpg_info_p++)
    {
      assert (atpg_info_p < end);

      FaultNode *fault_node = atpg_info_p->fault_node;

      /* reset atpg_info-pointer in graph nodes */
      if (!fault_node->deleted)
        {
          Node *node = fault_node->node;
          node->atpg_info = 0;
        }

      if (atpg_info_p->atpg_ch)
        delete_stack (atpg_rr->mm, atpg_info_p->atpg_ch);

      delete_fault_node (atpg_rr, fault_node);
    }                           /* end: for */
  assert (atpg_info_p == atpg_rr->cur_atpg_info);

  mem_free (atpg_rr->mm, atpg_rr->atpg_info_array, atpg_rr->byte_size_atpg_info_array);
  atpg_rr->atpg_info_array = 0;
  atpg_rr->cur_atpg_info = 0;
  atpg_rr->end_atpg_info = 0;
  atpg_rr->byte_size_atpg_info_array = 0;

  /* check and reset subformula-variables - could merge with loop before */
  void **var_p;
  for (var_p = atpg_rr->subformula_vars->elems;
       var_p < atpg_rr->subformula_vars->top; var_p++)
    {
      Var *var = (Var *) * var_p;
      var_unassign (var);
      var->atpg_mark = 0;

      delete_stack (atpg_rr->mm, var->subformula_pos_occs);
      var->subformula_pos_occs = 0;

      delete_stack (atpg_rr->mm, var->subformula_neg_occs);
      var->subformula_neg_occs = 0;
    }                           /* end: for all variables in subformula */

  reset_stack (atpg_rr->subformula_vars);
  atpg_rr->global_atpg_test_node_mark = 0;
}


void
free_atpg_redundancy_remover (ATPGRedundancyRemover * atpg_rr)
{
  delete_stack (atpg_rr->mm, atpg_rr->subformula_vars);
  delete_queue (atpg_rr->mm, atpg_rr->fault_queue);
  delete_queue (atpg_rr->mm, atpg_rr->propagation_queue);
  delete_stack (atpg_rr->mm, atpg_rr->touched_nodes);
  delete_stack (atpg_rr->mm, atpg_rr->bwd_prop_stack);
  delete_stack (atpg_rr->mm, atpg_rr->fault_path_nodes);
  delete_stack (atpg_rr->mm, atpg_rr->propagated_vars);

  assert (!atpg_rr->atpg_info_array);

  mem_free (atpg_rr->mm, atpg_rr, sizeof (ATPGRedundancyRemover));
}


/*
- each node in "changed-subformula" gets a pointer to an ATPGInfo-structure
- watchers are assigned to operator nodes
- special case: not all children of a node occur in "changed-subformula" 
	-> must maintain list of watchers
*/
static void
assign_node_atpg_info (ATPGRedundancyRemover * atpg_rr, Node * new_node)
{
  assert (atpg_rr->end_atpg_info ==
          (atpg_rr->atpg_info_array +
           atpg_rr->byte_size_atpg_info_array / sizeof (ATPGInfo)));

  if (atpg_rr->cur_atpg_info == atpg_rr->end_atpg_info - 1)
    {                           /* could (should NOT) happen */
      fprintf (stderr, "We have run out of ATPGInfo pointers...\n");
      exit (1);
    }

  assert (!new_node->atpg_info);
  new_node->atpg_info = atpg_rr->cur_atpg_info++;

  assert (!new_node->atpg_info->fault_node);
  new_node->atpg_info->fault_node = create_fault_node (atpg_rr, new_node);

  if (!is_literal_node (new_node))
    {                           /* average case: assign watchers for operator nodes */
      new_node->atpg_info->watcher = new_node->child_list.first;
      new_node->atpg_info->unassigned_ch_cnt = new_node->num_children;
      assert (!new_node->atpg_info->atpg_ch);
    }
}


/* 
- visit all nodes of subformula rooted at 'root'
- assign pointer to ATPGInfo
- collect var-occs
*/
static void
init_subformula_atpg_info (Nenofex * nenofex)
{
  Node *root = nenofex->changed_subformula.lca;
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;

  assert (root);
  assert (!is_literal_node (root));
  assert (!root->atpg_info);

  assign_node_atpg_info (atpg_rr, root);
  ATPGInfo *root_atpg_info = root->atpg_info;

  Stack *stack = create_stack (atpg_rr->mm, DEFAULT_STACK_SIZE);

  Node **ch, *child;
  for (ch = nenofex->changed_subformula.children; (child = *ch); ch++)
    push_stack (atpg_rr->mm, stack, child);

  if (count_stack (stack) < root->num_children)
    {                           /* not all children participate in global-flow/atpg -> collect on stack */
      root->atpg_info->atpg_ch = create_stack (atpg_rr->mm, DEFAULT_STACK_SIZE);
    }

  Node *cur;
  while ((cur = pop_stack (stack)))
    {
      assert (!cur->atpg_info);

      assign_node_atpg_info (atpg_rr, cur);

      /* NOTE: this could/should be done outside while-loop -> wasting check */
      if (root_atpg_info->atpg_ch && cur->parent == root)
        {
          push_stack (atpg_rr->mm, root_atpg_info->atpg_ch, cur->atpg_info->fault_node);
        }

      if (!is_literal_node (cur))
        {
          Node *ch;
          for (ch = cur->child_list.last; ch; ch = ch->level_link.prev)
            push_stack (atpg_rr->mm, stack, ch);
        }
      else
        {
          Var *var = cur->lit->var;
          assert (!var_assigned (var));

          if (!var->atpg_mark)
            {                   /* collect var */
              var->atpg_mark = 1;
              push_stack (atpg_rr->mm, atpg_rr->subformula_vars, var);

              assert (!var->subformula_pos_occs);
              var->subformula_pos_occs = create_stack (atpg_rr->mm, DEFAULT_STACK_SIZE);

              assert (!var->subformula_neg_occs);
              var->subformula_neg_occs = create_stack (atpg_rr->mm, DEFAULT_STACK_SIZE);
            }

          /* need to collect proxy occs, because must check for deleted occs */
          if (cur->lit->negated)
            push_stack (atpg_rr->mm, var->subformula_neg_occs, cur->atpg_info->fault_node);
          else
            push_stack (atpg_rr->mm, var->subformula_pos_occs, cur->atpg_info->fault_node);
        }
    }                           /* end: while stack not empty */

  assert (atpg_rr->cur_atpg_info <= atpg_rr->end_atpg_info);

  if (root_atpg_info->atpg_ch)
    {
      root_atpg_info->watcher_pos = root_atpg_info->atpg_ch->elems;
      root_atpg_info->watcher =
        ((FaultNode *) * root_atpg_info->watcher_pos)->node;
      root_atpg_info->unassigned_ch_cnt =
        count_stack (root_atpg_info->atpg_ch);
    }
  else
    {
      assert (root_atpg_info->watcher == root->child_list.first);
      assert (root_atpg_info->unassigned_ch_cnt == root->num_children);
    }

  delete_stack (atpg_rr->mm, stack);
}


/* 
- visit "changed-subformula" and enqueue nodes
*/
static void
collect_fault_nodes_by_dfs (Nenofex * nenofex)
{
  Node *root = nenofex->changed_subformula.lca;
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;
  Queue *atpg_rr_fault_queue = atpg_rr->fault_queue;

  assert (root->atpg_info);
  assert (root->atpg_info->fault_node);

  Stack *stack = create_stack (atpg_rr->mm, DEFAULT_STACK_SIZE);
  collect_faults_mark (root);
  push_stack (atpg_rr->mm, stack, root);

  Node **ch, *child;
  for (ch = nenofex->changed_subformula.children; (child = *ch); ch++)
    push_stack (atpg_rr->mm, stack, child);

  Node *cur;
  while ((cur = pop_stack (stack)))
    {
      assert (cur->atpg_info);
      assert (cur->atpg_info->fault_node);

      if (is_literal_node (cur))
        enqueue (atpg_rr->mm, atpg_rr_fault_queue, cur->atpg_info->fault_node);
      else
        {
          if (collect_faults_marked (cur))
            {
              collect_faults_unmark (cur);
              enqueue (atpg_rr->mm, atpg_rr_fault_queue, cur->atpg_info->fault_node);
            }
          else                  /* mark and visit children */
            {
              collect_faults_mark (cur);
              push_stack (atpg_rr->mm, stack, cur);

              Node *ch;
              for (ch = cur->child_list.last; ch; ch = ch->level_link.prev)
                push_stack (atpg_rr->mm, stack, ch);
            }
        }
    }                           /* end: while stack not empty */

  delete_stack (atpg_rr->mm, stack);
}


/* 
- visit "changed-subformula" and enqueue nodes
*/
static void
collect_fault_nodes_by_bfs (Nenofex * nenofex)
{
  Node *root = nenofex->changed_subformula.lca;
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;
  Queue *atpg_rr_fault_queue = atpg_rr->fault_queue;

  assert (root->atpg_info);
  assert (root->atpg_info->fault_node);

  Queue *queue = create_queue (atpg_rr->mm, DEFAULT_QUEUE_SIZE);
  enqueue (atpg_rr->mm, atpg_rr_fault_queue, root->atpg_info->fault_node);

  Node **ch, *child;
  for (ch = nenofex->changed_subformula.children; (child = *ch); ch++)
    enqueue (atpg_rr->mm, queue, child);

  Node *cur;
  while ((cur = dequeue (queue)))
    {
      assert (cur->atpg_info);
      assert (cur->atpg_info->fault_node);

      enqueue (atpg_rr->mm, atpg_rr_fault_queue, cur->atpg_info->fault_node);

      if (!is_literal_node (cur))
        {
          Node *ch;
          for (ch = cur->child_list.first; ch; ch = ch->level_link.next)
            enqueue (atpg_rr->mm, queue, ch);
        }

    }                           /* end: while queue not empty */

  delete_queue (atpg_rr->mm, queue);
}


/* 
- visit "changed-subformula" and enqueue nodes
- enqueue literals first then parents in reverse bfs-order
*/
static void
collect_fault_nodes_bottom_up (Nenofex * nenofex)
{
  Node *root = nenofex->changed_subformula.lca;
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;
  Queue *atpg_rr_fault_queue = atpg_rr->fault_queue;

  assert (root->atpg_info);
  assert (root->atpg_info->fault_node);

  Queue *queue = create_queue (atpg_rr->mm, DEFAULT_QUEUE_SIZE);
  Stack *fault_stack = create_stack (atpg_rr->mm, DEFAULT_STACK_SIZE);
  push_stack (atpg_rr->mm, fault_stack, root->atpg_info->fault_node);

  Node **ch, *child;
  for (ch = nenofex->changed_subformula.children; (child = *ch); ch++)
    enqueue (atpg_rr->mm, queue, child);

  assert (count_queue (queue) >= 2);

  Node *cur;
  while ((cur = dequeue (queue)))
    {
      assert (cur->atpg_info);
      assert (cur->atpg_info->fault_node);

      if (!is_literal_node (cur))
        {
          push_stack (atpg_rr->mm, fault_stack, cur->atpg_info->fault_node);

          Node *ch;
          for (ch = cur->child_list.first; ch; ch = ch->level_link.next)
            enqueue (atpg_rr->mm, queue, ch);
        }
      else
        {
          enqueue (atpg_rr->mm, atpg_rr_fault_queue, cur->atpg_info->fault_node);
        }
    }                           /* end: while queue not empty */

  while ((cur = pop_stack (fault_stack)))
    enqueue (atpg_rr->mm, atpg_rr_fault_queue, cur);

  delete_queue (atpg_rr->mm, queue);
  delete_stack (atpg_rr->mm, fault_stack);
}


/*
- collected nodes wil be reset after propagation
*/
void
collect_assigned_node (ATPGRedundancyRemover * atpg_rr, Node * node)
{
  ATPGInfo *atpg_info = node->atpg_info;

  if (!atpg_info->collected)
    {
      atpg_info->collected = 1;
      push_stack (atpg_rr->mm, atpg_rr->touched_nodes, atpg_info);
    }
}


/* 
- delete invalid nodes from watcher list
- called on demand
*/
static void
clean_up_watcher_atpg_child_list (ATPGInfo * atpg_info)
{
  assert (atpg_info->atpg_ch);
  assert (atpg_info->watcher_pos);

  void **elems = atpg_info->atpg_ch->elems;
  void **end = atpg_info->atpg_ch->top;

  /* traverse stack and remove all invalid nodes 
     by copying last node to current position */

  void **cur = elems;
  while (cur < end)
    {
      FaultNode *fault_node = *cur;

      if (fault_node->deleted)
        {
          if (cur == end - 1)
            {
              end--;            /* cut off last element */
            }
          else
            {
              assert (cur >= elems && cur <= end - 2);

              end--;
              *cur = *end;
              continue;
            }
        }                       /* end: if fault node deleted */

      cur++;
    }                           /* end: for all nodes on stack */

  atpg_info->atpg_ch->top = end;

#ifndef NDEBUG
  void **v;
  for (v = atpg_info->atpg_ch->elems; v < atpg_info->atpg_ch->top; v++)
    {
      FaultNode *cur = *v;
      assert (!cur->deleted);
    }
#endif
}


static void init_counter_and_watcher (Node * node);


/* 
- TODO: warning if only 1 unassigned child after initialization -> unclear...
*/
static void
reset_touched_nodes (ATPGRedundancyRemover * atpg_rr)
{
  ATPGInfo *atpg_info;
  Stack *atpg_rr_touched_nodes = atpg_rr->touched_nodes;

  while ((atpg_info = pop_stack (atpg_rr_touched_nodes)))
    {
      assert (atpg_info->collected);

      FaultNode *fault_node = atpg_info->fault_node;

      if (fault_node->deleted)
        continue;

      atpg_info->collected = 0;

      Node *node = fault_node->node;

      if (node_assigned (node))
        {
          atpg_info->assignment = 0;
          atpg_info->justified = 0;

          if (!is_literal_node (node))
            {                   /* init watchers */
              init_counter_and_watcher (node);
            }

        }
      else                      /* unassigned node -> init watchers */
        {
          init_counter_and_watcher (node);
        }                       /* end: node not assigned */

      /* TODO: warnif only 1 unassigned child after initialization -> unclear... */

    }                           /* end: while stack not empty */
}


/* 
- derive var. assignments at fault site -> sensitize the fault
- enqueue any assigned variables for propagation
- BOTTLENECK: skipping of nodes? -> very unlikely since only lit-ch are inspected
*/
static void
fault_sensitization (ATPGRedundancyRemover * atpg_rr,
                     Node * fault_node, ATPGFaultType fault_type)
{
  Queue *atpg_rr_propagation_queue = atpg_rr->propagation_queue;
  Var *var;
  Lit *lit;

  if (fault_type == ATPG_FAULT_TYPE_STUCK_AT_0)
    {
      assert (is_and_node (fault_node) || is_literal_node (fault_node));
      assert (!node_assigned (fault_node));

      if (is_literal_node (fault_node))
        {
          lit = fault_node->lit;
          var = lit->var;
          assert (!var_assigned (var));

          if (lit->negated)
            var_assign_false (var);
          else
            var_assign_true (var);

          enqueue (atpg_rr->mm, atpg_rr_propagation_queue, var);
        }
      else                      /* set children to 1 */
        {
          assert (is_and_node (fault_node));

          Node *ch;
          for (ch = fault_node->child_list.first;
               ch && is_literal_node (ch); ch = ch->level_link.next)
            {
              /* atpg restriction */
              if (!ch->atpg_info)
                continue;
              /* end: atpg restriction */

              assert (!node_assigned (ch));

              lit = ch->lit;
              var = lit->var;
              assert (!var_assigned (var));

              if (lit->negated)
                var_assign_false (var);
              else
                var_assign_true (var);

              enqueue (atpg_rr->mm, atpg_rr_propagation_queue, var);
            }                   /* end: for all literal-children */
        }                       /* end: set children to 1 */
    }
  else                          /* FAULT STUCK-AT-1 */
    {
      assert (fault_type == ATPG_FAULT_TYPE_STUCK_AT_1);
      assert (is_or_node (fault_node) || is_literal_node (fault_node));
      assert (!node_assigned (fault_node));

      if (is_literal_node (fault_node))
        {
          lit = fault_node->lit;
          var = lit->var;
          assert (!var_assigned (var));

          if (lit->negated)
            var_assign_true (var);
          else
            var_assign_false (var);

          enqueue (atpg_rr->mm, atpg_rr_propagation_queue, var);
        }
      else                      /* set children to 0 */
        {
          assert (is_or_node (fault_node));

          Node *ch;
          for (ch = fault_node->child_list.first;
               ch && is_literal_node (ch); ch = ch->level_link.next)
            {
              /* atpg restriction */
              if (!ch->atpg_info)
                continue;
              /* end: atpg restriction */

              assert (!node_assigned (ch));

              lit = ch->lit;
              var = lit->var;
              assert (!var_assigned (var));

              if (lit->negated)
                var_assign_true (var);
              else
                var_assign_false (var);

              enqueue (atpg_rr->mm, atpg_rr_propagation_queue, var);
            }                   /* end: for all literal-children */
        }                       /* end: set children to 0 */
    }                           /* end: stuck-at-1 */
}


#ifndef NDEBUG
/*
- the following functions are used in assertion-checking only
*/
static int
all_children_assigned_value (Node * parent, ATPGAssignment v)
{
  int result = 1;

  switch (v)
    {
    case ATPG_ASSIGNMENT_UNDEFINED:
      {
        Node *ch;
        for (ch = parent->child_list.first; result && ch;
             ch = ch->level_link.next)
          {
            /* atpg restriction */
            if (!ch->atpg_info)
              continue;
            /* end: atpg restriction */

            result = !node_assigned (ch);
          }
      }
      break;

    case ATPG_ASSIGNMENT_FALSE:
      {
        Node *ch;
        for (ch = parent->child_list.first; result && ch;
             ch = ch->level_link.next)
          {
            /* atpg restriction */
            if (!ch->atpg_info)
              continue;
            /* end: atpg restriction */

            result = node_assigned_false (ch);
          }
      }
      break;

    case ATPG_ASSIGNMENT_TRUE:
      {
        Node *ch;
        for (ch = parent->child_list.first; result && ch;
             ch = ch->level_link.next)
          {
            /* atpg restriction */
            if (!ch->atpg_info)
              continue;
            /* end: atpg restriction */

            result = node_assigned_true (ch);
          }
      }
      break;
    }

  return result;
}


static int
count_children_assigned_value (Node * parent, ATPGAssignment v)
{
  int result = 0;

  switch (v)
    {
    case ATPG_ASSIGNMENT_UNDEFINED:
      {
        Node *ch;
        for (ch = parent->child_list.first; ch; ch = ch->level_link.next)
          {
            /* atpg restriction */
            if (!ch->atpg_info)
              continue;
            /* end: atpg restriction */

            if (!node_assigned (ch))
              result++;
          }
      }
      break;

    case ATPG_ASSIGNMENT_FALSE:
      {
        Node *ch;
        for (ch = parent->child_list.first; ch; ch = ch->level_link.next)
          {
            /* atpg restriction */
            if (!ch->atpg_info)
              continue;
            /* end: atpg restriction */

            if (node_assigned_false (ch))
              result++;
          }
      }
      break;

    case ATPG_ASSIGNMENT_TRUE:
      {
        Node *ch;
        for (ch = parent->child_list.first; ch; ch = ch->level_link.next)
          {
            /* atpg restriction */
            if (!ch->atpg_info)
              continue;
            /* end: atpg restriction */

            if (node_assigned_true (ch))
              result++;
          }
      }
      break;
    }

  return result;
}


static Node *
find_child_assigned_value (Node * parent, ATPGAssignment v)
{
  Node *result = 0;

  switch (v)
    {
    case ATPG_ASSIGNMENT_UNDEFINED:
      {
        Node *ch;
        for (ch = parent->child_list.first; !result && ch;
             ch = ch->level_link.next)
          {
            /* atpg restriction */
            if (!ch->atpg_info)
              continue;
            /* end: atpg restriction */

            if (!node_assigned (ch))
              result = ch;
          }
      }
      break;

    case ATPG_ASSIGNMENT_FALSE:
      {
        Node *ch;
        for (ch = parent->child_list.first; !result && ch;
             ch = ch->level_link.next)
          {
            /* atpg restriction */
            if (!ch->atpg_info)
              continue;
            /* end: atpg restriction */

            if (node_assigned_false (ch))
              result = ch;
          }
      }
      break;

    case ATPG_ASSIGNMENT_TRUE:
      {
        Node *ch;
        for (ch = parent->child_list.first; !result && ch;
             ch = ch->level_link.next)
          {
            /* atpg restriction */
            if (!ch->atpg_info)
              continue;
            /* end: atpg restriction */

            if (node_assigned_true (ch))
              result = ch;
          }
      }
      break;
    }

  return result;
}
#endif /* end: ifndef NDEBUG */


/*
- current watcher has been assigned
- special case: searches for unassigned child in watcher list
- for nodes where not all children participate in optimizations
- watcher update by real child list could become bottleneck for such nodes
*/
static void
update_watcher_by_watcher_list (Node * parent)
{
  ATPGInfo *atpg_info = parent->atpg_info;
  Node *watcher = atpg_info->watcher;

  assert (atpg_info->atpg_ch);
  assert (atpg_info->watcher_pos);
  assert (((FaultNode *) * atpg_info->watcher_pos)->node == watcher);

  if (node_assigned (watcher))
    {
      void **cur = atpg_info->watcher_pos + 1;
      void **end = atpg_info->atpg_ch->top;

      assert (cur > atpg_info->watcher_pos && cur <= atpg_info->atpg_ch->top);

      while (cur < end)
        {
          watcher = ((FaultNode *) * cur)->node;

          if (!node_assigned (watcher))
            break;

          cur++;
        }                       /* end: while */

      if (cur == end)
        {                       /* no unassigned child found */
          atpg_info->watcher = 0;
          atpg_info->watcher_pos = end;
        }
      else
        {
          atpg_info->watcher = watcher;
          atpg_info->watcher_pos = cur;
        }
    }
}


/*
- current watcher has been assigned
- average case: searches for unassigned child in child list
*/
static void
update_watcher_by_child_list (Node * parent)
{
  ATPGInfo *atpg_info = parent->atpg_info;
  Node *watcher = atpg_info->watcher;

  assert (!atpg_info->atpg_ch);
  assert (!atpg_info->watcher_pos);

  if (node_assigned (watcher))
    {
      do
        {
          watcher = watcher->level_link.next;
        }
      while (watcher && node_assigned (watcher));

      atpg_info->watcher = watcher;
    }
}


/*
- set up counter and watcher for a node
- clean up of watcher list is done on demand
*/
static void
init_counter_and_watcher (Node * node)
{
  assert (!is_literal_node (node));

  ATPGInfo *atpg_info = node->atpg_info;
  Stack *atpg_info_atpg_ch = atpg_info->atpg_ch;

  if (atpg_info_atpg_ch)
    {
      if (atpg_info->clean_up_watcher_list)
        {
          atpg_info->clean_up_watcher_list = 0;
          clean_up_watcher_atpg_child_list (atpg_info);
        }

      atpg_info->watcher_pos = atpg_info_atpg_ch->elems;
      atpg_info->watcher = ((FaultNode *) * atpg_info->watcher_pos)->node;
      atpg_info->unassigned_ch_cnt = count_stack (atpg_info_atpg_ch);
    }
  else
    {
      assert (!atpg_info->watcher_pos);
      assert (!atpg_info->clean_up_watcher_list);

      atpg_info->watcher = node->child_list.first;
      atpg_info->unassigned_ch_cnt = node->num_children;
    }

}


/* 
- called whenever one of some node's children has been assigned a value
*/
static void
update_counter_and_watcher (Node * parent)
{
  assert (!is_literal_node (parent));

  ATPGInfo *atpg_info = parent->atpg_info;

  assert (!atpg_info->atpg_ch || (atpg_info->unassigned_ch_cnt > 0 &&
                                  atpg_info->unassigned_ch_cnt <=
                                  count_stack (atpg_info->atpg_ch)));
  assert (atpg_info->atpg_ch
          || (atpg_info->unassigned_ch_cnt > 0
              && atpg_info->unassigned_ch_cnt <= parent->num_children));

  atpg_info->unassigned_ch_cnt--;

  if (atpg_info->atpg_ch)
    update_watcher_by_watcher_list (parent);
  else
    update_watcher_by_child_list (parent);
}


/* 
- special case
- POSSIBLE BOTTLENECK?
- NOTE: could maintain a pointer/offset (within ATPGInfo - structure) to location
   in watcher list for each node -> need not search explicitly then
*/
static void
remove_child_from_watcher_list (Node * parent, Node * child)
{
  assert (!is_literal_node (parent));
  assert (child->parent == parent);
  assert (parent->atpg_info->atpg_ch);

  ATPGInfo *atpg_info = parent->atpg_info;
  Stack *atpg_info_atpg_ch = atpg_info->atpg_ch;

  void **cur, **end;
  end = atpg_info_atpg_ch->top;

  for (cur = atpg_info_atpg_ch->elems; cur < end && *cur != (void *) child;
       cur++)
    ;

  assert (cur < end);

  if (cur == end - 1)
    {                           /* cut off at end of collection */
      atpg_info_atpg_ch->top--;
    }
  else
    {                           /* overwrite cur element with last element */
      end--;
      *cur = *end;
      atpg_info_atpg_ch->top = end;
    }
}


#if RECURSIVE_BWD_PROP

/*
- recursive version
*/
static void
backward_propagate_truth (ATPGRedundancyRemover * atpg_rr, Node * node)
{
  assert (!atpg_rr->global_flow_optimizing);

  atpg_rr->stats.bwd_prop_cnt++;

  if ((!atpg_rr->global_flow_optimizing &&
       (atpg_rr->stats.bwd_prop_cnt + atpg_rr->stats.fwd_prop_cnt >
        ATPG_PROPAGATION_LIMIT)) || (atpg_rr->global_flow_optimizing
                                     && (atpg_rr->stats.bwd_prop_cnt +
                                         atpg_rr->stats.fwd_prop_cnt >
                                         GLOBAL_FLOW_PROPAGATION_LIMIT)))
    {
      atpg_rr->prop_cutoff = 1;
      return;
    }

  assert (!atpg_rr->conflict);
  assert (!atpg_rr->prop_cutoff);
  assert (!node_assigned (node));

  Queue *atpg_rr_propagation_queue = atpg_rr->propagation_queue;

  if (is_literal_node (node))
    {
      Lit *lit = node->lit;
      Var *var = lit->var;

      if (!var_assigned (var))  /* assign justifying value and enqueue var. for prop. */
        {
          if (lit->negated)
            var_assign_false (var);
          else
            var_assign_true (var);
          enqueue (atpg_rr->mm, atpg_rr_propagation_queue, var);
        }
      else                      /* check for conflict */
        {
          if ((lit->negated && var_assigned_true (var)) ||
              (!lit->negated && var_assigned_false (var)))
            {
              atpg_rr->conflict = 1;
            }
        }                       /* end: var assigned */
    }
  else if (is_and_node (node))
    {                           /* backward-propagate truth to all unassigned ch. -> assert: no child is 0 */
#if ASSERT_CHILDREN_ASSIGNMENTS
      assert (!count_children_assigned_value (node, ATPG_ASSIGNMENT_FALSE));
#endif

      node_assign_true (node);
      collect_assigned_node (atpg_rr, node);

      Node *ch;
      for (ch = node->child_list.first;
           !atpg_rr->conflict && !atpg_rr->prop_cutoff && ch;
           ch = ch->level_link.next)
        {
          if (!node_assigned (ch))
            backward_propagate_truth (atpg_rr, ch);
          else if (is_or_node (ch))
            {
              assert (node_assigned_true (ch));
#if ASSERT_CHILDREN_ASSIGNMENTS
              assert (count_children_assigned_value
                      (ch, ATPG_ASSIGNMENT_TRUE));
#endif
            }
        }                       /* end: for all children */

      node->atpg_info->justified = 1;
    }
  else                          /* OR */
    {
#if ASSERT_CHILDREN_ASSIGNMENTS
      assert (!count_children_assigned_value (node, ATPG_ASSIGNMENT_TRUE));
#endif

      node_assign_true (node);
      collect_assigned_node (atpg_rr, node);

      if (node->atpg_info->unassigned_ch_cnt == 1)
        {                       /* must justify '1' via single remaining unassigned child */
          Node *implied_node = node->atpg_info->watcher;
          assert (implied_node);
          backward_propagate_truth (atpg_rr, implied_node);

          node->atpg_info->justified = 1;
        }
    }                           /* end: OR */
}

#else

/* 
- non-recursive version using a stack (slower?)
- note: no fwd-propagation can occur until all nodes have been bwd-propagated
*/
static void
backward_propagate_truth (ATPGRedundancyRemover * atpg_rr, Node * node)
{
  assert (!atpg_rr->global_flow_optimizing);
  assert (!atpg_rr->conflict);  /* not sure about this */
  assert (!atpg_rr->prop_cutoff);

  Queue *atpg_rr_propagation_queue = atpg_rr->propagation_queue;

  Stack *bwd_prop_stack = atpg_rr->bwd_prop_stack;
  assert (count_stack (bwd_prop_stack) == 0);

  push_stack (atpg_rr->mm, bwd_prop_stack, node);

  while (!atpg_rr->conflict && (node = pop_stack (bwd_prop_stack)))
    {

      atpg_rr->stats.bwd_prop_cnt++;

      if ((!atpg_rr->global_flow_optimizing &&
           (atpg_rr->stats.bwd_prop_cnt + atpg_rr->stats.fwd_prop_cnt >
            ATPG_PROPAGATION_LIMIT)) || (atpg_rr->global_flow_optimizing
                                         && (atpg_rr->stats.bwd_prop_cnt +
                                             atpg_rr->stats.fwd_prop_cnt >
                                             GLOBAL_FLOW_PROPAGATION_LIMIT)))
        {
          atpg_rr->prop_cutoff = 1;
          break;
        }

      assert (!atpg_rr->conflict);
      assert (!atpg_rr->prop_cutoff);
      assert (!node_assigned (node));

      if (is_literal_node (node))
        {
          Lit *lit = node->lit;
          Var *var = lit->var;

          if (!var_assigned (var))      /* assign justifying value and enqueue var. for prop. */
            {
              if (lit->negated)
                var_assign_false (var);
              else
                var_assign_true (var);

              enqueue (atpg_rr->mm, atpg_rr_propagation_queue, var);
            }
          else                  /* check for conflict */
            {
              if ((lit->negated && var_assigned_true (var)) ||
                  (!lit->negated && var_assigned_false (var)))
                {
                  atpg_rr->conflict = 1;
                }
            }                   /* end: var assigned */
        }
      else if (is_and_node (node))
        {                       /* bwd-propagate truth to all unassigned children -> assert: no child is 0 */
#if ASSERT_CHILDREN_ASSIGNMENTS
          assert (!count_children_assigned_value
                  (node, ATPG_ASSIGNMENT_FALSE));
#endif

          node_assign_true (node);
          collect_assigned_node (atpg_rr, node);

          Node *ch;
          for (ch = node->child_list.last; ch; ch = ch->level_link.prev)
            {
              if (!node_assigned (ch))
                {
                  push_stack (atpg_rr->mm, bwd_prop_stack, ch);
                }
              else if (is_or_node (ch))
                {
                  assert (node_assigned_true (ch));
#if ASSERT_CHILDREN_ASSIGNMENTS
                  assert (count_children_assigned_value
                          (ch, ATPG_ASSIGNMENT_TRUE));
#endif
                }
            }                   /* end: for all children */

          node->atpg_info->justified = 1;
        }
      else                      /* OR */
        {
#if ASSERT_CHILDREN_ASSIGNMENTS
          assert (!count_children_assigned_value
                  (node, ATPG_ASSIGNMENT_TRUE));
#endif

          node_assign_true (node);
          collect_assigned_node (atpg_rr, node);

          if (node->atpg_info->unassigned_ch_cnt == 1)
            {                   /* must justify '1' via single remaining unassigned child */
              Node *implied_node = node->atpg_info->watcher;
              assert (implied_node);

              push_stack (atpg_rr->mm, bwd_prop_stack, implied_node);

              node->atpg_info->justified = 1;
            }
        }                       /* end: OR */
    }                           /* end: while propagation stack not empty */

  reset_stack (atpg_rr->bwd_prop_stack);
}


#endif /* end: non-rec bwd-propagation */


#if RECURSIVE_BWD_PROP

/*
- recursive version
*/
static void
backward_propagate_falsity (ATPGRedundancyRemover * atpg_rr, Node * node)
{
  assert (!atpg_rr->global_flow_optimizing);

  atpg_rr->stats.bwd_prop_cnt++;

  if ((!atpg_rr->global_flow_optimizing &&
       (atpg_rr->stats.bwd_prop_cnt + atpg_rr->stats.fwd_prop_cnt >
        ATPG_PROPAGATION_LIMIT)) || (atpg_rr->global_flow_optimizing
                                     && (atpg_rr->stats.bwd_prop_cnt +
                                         atpg_rr->stats.fwd_prop_cnt >
                                         GLOBAL_FLOW_PROPAGATION_LIMIT)))
    {
      atpg_rr->prop_cutoff = 1;
      return;
    }

  assert (!node_assigned (node));
  assert (!atpg_rr->conflict);
  assert (!atpg_rr->prop_cutoff);

  Queue *atpg_rr_propagation_queue = atpg_rr->propagation_queue;

  if (is_literal_node (node))
    {
      Lit *lit = node->lit;
      Var *var = lit->var;

      if (!var_assigned (var))  /* assign justifying value and enqueue var. for prop. */
        {
          if (lit->negated)
            var_assign_true (var);
          else
            var_assign_false (var);
          enqueue (atpg_rr->mm, atpg_rr_propagation_queue, var);
        }
      else                      /* check for conflict */
        {
          if ((lit->negated && var_assigned_false (var)) ||
              (!lit->negated && var_assigned_true (var)))
            {
              atpg_rr->conflict = 1;
            }
        }                       /* end: var assigned */
    }
  else if (is_and_node (node))
    {
#if ASSERT_CHILDREN_ASSIGNMENTS
      assert (!count_children_assigned_value (node, ATPG_ASSIGNMENT_FALSE));
#endif

      node_assign_false (node);
      collect_assigned_node (atpg_rr, node);

      if (node->atpg_info->unassigned_ch_cnt == 1)
        {                       /* must justify '1' via single remaining unassigned child */
          Node *implied_node = node->atpg_info->watcher;
          assert (implied_node);

          backward_propagate_falsity (atpg_rr, implied_node);

          node->atpg_info->justified = 1;
        }
    }
  else                          /* OR */
    {                           /* bwd-propagate falsity to all unassigned children -> assert: no child is 1 */
#if ASSERT_CHILDREN_ASSIGNMENTS
      assert (!count_children_assigned_value (node, ATPG_ASSIGNMENT_TRUE));
#endif

      node_assign_false (node);
      collect_assigned_node (atpg_rr, node);

      Node *ch;
      for (ch = node->child_list.first;
           !atpg_rr->conflict && !atpg_rr->prop_cutoff && ch;
           ch = ch->level_link.next)
        {
          if (!node_assigned (ch))
            backward_propagate_falsity (atpg_rr, ch);
          else if (is_and_node (ch))
            {
              assert (node_assigned_false (ch));
#if ASSERT_CHILDREN_ASSIGNMENTS
              assert (count_children_assigned_value
                      (ch, ATPG_ASSIGNMENT_FALSE));
#endif
            }
        }                       /* end: for all children */

      node->atpg_info->justified = 1;
    }                           /* end: OR */
}

#else

/* 
- non-recursive version using a stack (slower?)
- note that no fwd- propagation can occur until all nodes have been bwd-propagated
*/
static void
backward_propagate_falsity (ATPGRedundancyRemover * atpg_rr, Node * node)
{
  assert (!atpg_rr->global_flow_optimizing);
  assert (!atpg_rr->conflict);
  assert (!atpg_rr->prop_cutoff);

  Queue *atpg_rr_propagation_queue = atpg_rr->propagation_queue;

  Stack *bwd_prop_stack = atpg_rr->bwd_prop_stack;
  assert (count_stack (bwd_prop_stack) == 0);

  push_stack (atpg_rr->mm, bwd_prop_stack, node);

  while (!atpg_rr->conflict && (node = pop_stack (bwd_prop_stack)))
    {
      atpg_rr->stats.bwd_prop_cnt++;

      if ((!atpg_rr->global_flow_optimizing &&
           (atpg_rr->stats.bwd_prop_cnt + atpg_rr->stats.fwd_prop_cnt >
            ATPG_PROPAGATION_LIMIT)) || (atpg_rr->global_flow_optimizing
                                         && (atpg_rr->stats.bwd_prop_cnt +
                                             atpg_rr->stats.fwd_prop_cnt >
                                             GLOBAL_FLOW_PROPAGATION_LIMIT)))
        {
          atpg_rr->prop_cutoff = 1;
          break;
        }

      assert (!node_assigned (node));
      assert (!atpg_rr->conflict);
      assert (!atpg_rr->prop_cutoff);

      if (is_literal_node (node))
        {
          Lit *lit = node->lit;
          Var *var = lit->var;

          if (!var_assigned (var))      /* assign justifying value and enqueue var. for prop. */
            {
              if (lit->negated)
                var_assign_true (var);
              else
                var_assign_false (var);

              enqueue (atpg_rr->mm, atpg_rr_propagation_queue, var);
            }
          else                  /* check for conflict */
            {
              if ((lit->negated && var_assigned_false (var)) ||
                  (!lit->negated && var_assigned_true (var)))
                {
                  atpg_rr->conflict = 1;
                }
            }                   /* end: var assigned */
        }
      else if (is_and_node (node))
        {
#if ASSERT_CHILDREN_ASSIGNMENTS
          assert (!count_children_assigned_value
                  (node, ATPG_ASSIGNMENT_FALSE));
#endif

          node_assign_false (node);
          collect_assigned_node (atpg_rr, node);

          if (node->atpg_info->unassigned_ch_cnt == 1)
            {                   /* must justify '1' via single remaining unassigned child */
              Node *implied_node = node->atpg_info->watcher;
              assert (implied_node);

              push_stack (atpg_rr->mm, bwd_prop_stack, implied_node);

              node->atpg_info->justified = 1;
            }
        }
      else                      /* OR */
        {                       /* bwd-propagate falsity to all unassigned children -> assert: no child is 1 */
#if ASSERT_CHILDREN_ASSIGNMENTS
          assert (!count_children_assigned_value
                  (node, ATPG_ASSIGNMENT_TRUE));
#endif

          node_assign_false (node);
          collect_assigned_node (atpg_rr, node);

          Node *ch;
          for (ch = node->child_list.last; ch; ch = ch->level_link.prev)
            {
              if (!node_assigned (ch))
                {
                  push_stack (atpg_rr->mm, bwd_prop_stack, ch);
                }
              else if (is_and_node (ch))
                {
                  assert (node_assigned_false (ch));
#if ASSERT_CHILDREN_ASSIGNMENTS
                  assert (count_children_assigned_value
                          (ch, ATPG_ASSIGNMENT_FALSE));
#endif
                }
            }                   /* end: for all children */

          node->atpg_info->justified = 1;
        }                       /* end: OR */
    }                           /* end: while stack not empty */

  reset_stack (bwd_prop_stack);
}

#endif /* end: non-rec bwd-propagation */


static void forward_propagate_falsity (Nenofex * nenofex, Node * node);

/* 
- non-recursive version without using a stack
- unclear: indirect recursion (via fwd_propagate_falsity) can occur at fault path?
*/
static void
forward_propagate_truth (Nenofex * nenofex, Node * node)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;

TAIL_RECURSIVE_CALL:

  atpg_rr->stats.fwd_prop_cnt++;

  if ((!atpg_rr->global_flow_optimizing &&
       (atpg_rr->stats.bwd_prop_cnt + atpg_rr->stats.fwd_prop_cnt >
        ATPG_PROPAGATION_LIMIT)) || (atpg_rr->global_flow_optimizing
                                     && (atpg_rr->stats.bwd_prop_cnt +
                                         atpg_rr->stats.fwd_prop_cnt >
                                         GLOBAL_FLOW_PROPAGATION_LIMIT)))
    {
      atpg_rr->prop_cutoff = 1;
      return;
    }

  assert (!node_assigned (node));
  assert (!atpg_rr->conflict);
  assert (!atpg_rr->prop_cutoff);

  node_assign_true (node);
  node->atpg_info->justified = 1;
  collect_assigned_node (atpg_rr, node);

  Node *parent = node->parent;

  if (node == nenofex->changed_subformula.lca)
    {
      return;
    }
  assert (parent);

  if (ALWAYS_UPDATE_WATCHER || !node_assigned (parent)
      || !parent->atpg_info->justified)
    {                           /* else skip unnecessary calls to update function */
      update_counter_and_watcher (parent);
      collect_assigned_node (atpg_rr, parent);
    }

  Node *parent_parent = parent->parent;

  if (is_and_node (parent))
    {
      if ((node_assigned_false (parent) && !parent->atpg_info->justified)       /* BUG-FIX */
          || (!node_assigned (parent) && !parent->atpg_info->path_mark
              && parent_parent && parent_parent->atpg_info
              && parent_parent->atpg_info->path_mark))
        {                       /* parent is off-path input of an OR  -> must be set to false */
          assert (!atpg_rr->global_flow_optimizing);
#if ASSERT_CHILDREN_ASSIGNMENTS
          assert (!count_children_assigned_value
                  (parent, ATPG_ASSIGNMENT_FALSE));
#endif

          if (parent->atpg_info->unassigned_ch_cnt == 1)
            {
              Node *implied_ch = parent->atpg_info->watcher;
              assert (implied_ch);
              assert (!parent->atpg_info->justified);

              backward_propagate_falsity (atpg_rr, implied_ch);

              if (!node_assigned (parent) && !atpg_rr->conflict)
                {
                  forward_propagate_falsity (nenofex, parent);
                }
            }
        }
      else if (!node_assigned (parent)) /* average case */
        {
          if (parent->atpg_info->unassigned_ch_cnt == 0)
            {
#if ASSERT_CHILDREN_ASSIGNMENTS
              assert (!count_children_assigned_value
                      (parent, ATPG_ASSIGNMENT_UNDEFINED));
              assert (!count_children_assigned_value
                      (parent, ATPG_ASSIGNMENT_FALSE));
#endif
              assert (!parent->atpg_info->justified);

              node = parent;
              goto TAIL_RECURSIVE_CALL;
            }
        }
    }
  else                          /* parent is OR */
    {
      assert (is_or_node (parent));

      if (!node->atpg_info->path_mark && parent->atpg_info->path_mark)
        {                       /* node is off-path input of OR-node */
          assert (!atpg_rr->global_flow_optimizing);
          assert (is_literal_node (node));

          atpg_rr->conflict = 1;
        }
      else if (!node_assigned (parent)) /* average case */
        {
          assert (!node->atpg_info->path_mark
                  || parent->atpg_info->path_mark);
#if ASSERT_CHILDREN_ASSIGNMENTS
          assert (count_children_assigned_value (parent, ATPG_ASSIGNMENT_TRUE)
                  == 1);
#endif
          assert (!parent->atpg_info->justified);

          node = parent;
          goto TAIL_RECURSIVE_CALL;
        }
      else
        {
          assert (node_assigned_true (parent));
          parent->atpg_info->justified = 1;     /* BUG-FIX (watcher) */
        }
    }                           /* end: parent is OR */
}


/* 
- non-recursive version without using a stack
- unclear: indirect recursion (via forward_propagate_truth) might occur at fault path
*/
static void
forward_propagate_falsity (Nenofex * nenofex, Node * node)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;

TAIL_RECURSIVE_CALL:

  atpg_rr->stats.fwd_prop_cnt++;

  if ((!atpg_rr->global_flow_optimizing &&
       (atpg_rr->stats.bwd_prop_cnt + atpg_rr->stats.fwd_prop_cnt >
        ATPG_PROPAGATION_LIMIT)) || (atpg_rr->global_flow_optimizing
                                     && (atpg_rr->stats.bwd_prop_cnt +
                                         atpg_rr->stats.fwd_prop_cnt >
                                         GLOBAL_FLOW_PROPAGATION_LIMIT)))
    {
      atpg_rr->prop_cutoff = 1;
      return;
    }

  assert (!node_assigned (node));
  assert (!atpg_rr->conflict);
  assert (!atpg_rr->prop_cutoff);

  node_assign_false (node);
  node->atpg_info->justified = 1;
  collect_assigned_node (atpg_rr, node);

  Node *parent = node->parent;

  if (node == nenofex->changed_subformula.lca)
    {
      return;
    }
  assert (parent);

  if (ALWAYS_UPDATE_WATCHER || !node_assigned (parent)
      || !parent->atpg_info->justified)
    {                           /* else skip unnecessary calls to update function */
      update_counter_and_watcher (parent);
      collect_assigned_node (atpg_rr, parent);
    }

  Node *parent_parent = parent->parent;

  if (is_or_node (parent))
    {
      if ((node_assigned_true (parent) && !parent->atpg_info->justified)        /* BUG-FIX */
          || (!node_assigned (parent) && !parent->atpg_info->path_mark
              && parent_parent && parent_parent->atpg_info
              && parent_parent->atpg_info->path_mark))
        {                       /* parent is off-path input of an AND  -> must be set to true */
          assert (!atpg_rr->global_flow_optimizing);
#if ASSERT_CHILDREN_ASSIGNMENTS
          assert (!count_children_assigned_value
                  (parent, ATPG_ASSIGNMENT_TRUE));
#endif

          if (parent->atpg_info->unassigned_ch_cnt == 1)
            {
              Node *implied_ch = parent->atpg_info->watcher;
              assert (implied_ch);
              assert (!parent->atpg_info->justified);

              backward_propagate_truth (atpg_rr, implied_ch);

              if (!node_assigned (parent) && !atpg_rr->conflict)
                {
                  forward_propagate_truth (nenofex, parent);
                }
            }
        }
      else if (!node_assigned (parent)) /* average case */
        {
          if (parent->atpg_info->unassigned_ch_cnt == 0)
            {
#if ASSERT_CHILDREN_ASSIGNMENTS
              assert (!count_children_assigned_value
                      (parent, ATPG_ASSIGNMENT_UNDEFINED));
              assert (!count_children_assigned_value
                      (parent, ATPG_ASSIGNMENT_TRUE));
#endif
              assert (!parent->atpg_info->justified);

              node = parent;
              goto TAIL_RECURSIVE_CALL;
            }
        }
    }
  else                          /* parent is AND */
    {
      assert (is_and_node (parent));

      if (!node->atpg_info->path_mark && parent->atpg_info->path_mark)
        {                       /* node is off-path input of AND-node */
          assert (!atpg_rr->global_flow_optimizing);
          assert (is_literal_node (node));

          atpg_rr->conflict = 1;
        }
      else if (!node_assigned (parent)) /* average case */
        {
          assert (!node->atpg_info->path_mark
                  || parent->atpg_info->path_mark);
#if ASSERT_CHILDREN_ASSIGNMENTS
          assert (count_children_assigned_value
                  (parent, ATPG_ASSIGNMENT_FALSE) == 1);
#endif
          assert (!parent->atpg_info->justified);

          node = parent;
          goto TAIL_RECURSIVE_CALL;
        }
      else
        {
          assert (node_assigned_false (parent));
          parent->atpg_info->justified = 1;     /* BUG-FIX (watcher) */
        }
    }                           /* end: parent is AND */
}


#if CLEAN_UP_SUBFORMULA_OCC_LIST

#define clean_up_invalid_occ(cur, end) \
  if ((cur) == (end) - 1) \
  { \
    (end)--; \
    continue; \
  } \
  else \
  { \
    (end)--; \
    *(cur) = *(end); \
    continue; \
  }

#define clean_up_invalid_occ_set_top(stack_top, end) \
  (stack_top) = (end);

#endif /* end: 'clean_up_subformula_occ_list' */

/*
- NOTE: collection might hold invalid nodes -> could be deleted (now: are skipped)
*/
static void
propagate_variable_assigned_true (Nenofex * nenofex, Var * var)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;
  assert (var_assigned_true (var));

  void **v_occ, **v_occ_end;
  Stack *occ_stack = var->subformula_neg_occs;
  v_occ_end = occ_stack->top;
  v_occ = occ_stack->elems;

#if CLEAN_UP_SUBFORMULA_OCC_LIST
  int occ_removed = 0;
#endif

  while (v_occ < v_occ_end)
    {                           /* for all negative occurrences */
      FaultNode *occ = *v_occ;

      if (occ->deleted)
        {
#if !CLEAN_UP_SUBFORMULA_OCC_LIST
          v_occ++;
          continue;
#else
          occ_removed = 1;
          clean_up_invalid_occ (v_occ, v_occ_end);
#endif /* end: clean up occ-list */
        }

      assert (occ->node->atpg_info);
      assert (is_literal_node (occ->node));
      assert (occ->node->lit->negated);
      assert (!node_assigned (occ->node));

      forward_propagate_falsity (nenofex, occ->node);

      if (atpg_rr->conflict || atpg_rr->prop_cutoff)
        {
#if CLEAN_UP_SUBFORMULA_OCC_LIST
          if (occ_removed)
            {
              clean_up_invalid_occ_set_top (occ_stack->top, v_occ_end);
            }
#endif
          return;               /* return early on conflict or cutoff */
        }

      v_occ++;
    }                           /* end: for all neg. occurrences */

#if CLEAN_UP_SUBFORMULA_OCC_LIST
  if (occ_removed)
    {
      occ_removed = 0;
      clean_up_invalid_occ_set_top (occ_stack->top, v_occ_end);
    }
#endif

  occ_stack = var->subformula_pos_occs;
  v_occ_end = occ_stack->top;
  v_occ = occ_stack->elems;

#if 0
  ---------
#if CLEAN_UP_SUBFORMULA_OCC_LIST
    occ_removed = 0;
#endif
  --------
#endif
    while (v_occ < v_occ_end)
    {                           /* for all positive occurrences */
      FaultNode *occ = *v_occ;

      if (occ->deleted)
        {
#if !CLEAN_UP_SUBFORMULA_OCC_LIST
          v_occ++;
          continue;
#else
          occ_removed = 1;
          clean_up_invalid_occ (v_occ, v_occ_end);
#endif /* end: clean up occ-list */
        }

      assert (occ->node->atpg_info);
      assert (is_literal_node (occ->node));
      assert (!occ->node->lit->negated);
      assert (!node_assigned (occ->node));

      forward_propagate_truth (nenofex, occ->node);

      if (atpg_rr->conflict || atpg_rr->prop_cutoff)
        {
#if CLEAN_UP_SUBFORMULA_OCC_LIST
          if (occ_removed)
            {
              clean_up_invalid_occ_set_top (occ_stack->top, v_occ_end);
            }
#endif
          return;               /* return early on conflict */
        }

      v_occ++;
    }                           /* end: for all pos. occurrences */

#if CLEAN_UP_SUBFORMULA_OCC_LIST
  if (occ_removed)
    {
      clean_up_invalid_occ_set_top (occ_stack->top, v_occ_end);
    }
#endif

  assert (!atpg_rr->conflict);
  assert (!atpg_rr->prop_cutoff);
}


/*
- NOTE: collection might hold invalid nodes -> could be deleted (now: are skipped)
*/
static void
propagate_variable_assigned_false (Nenofex * nenofex, Var * var)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;
  assert (var_assigned_false (var));

  void **v_occ, **v_occ_end;
  Stack *occ_stack = var->subformula_neg_occs;
  v_occ_end = occ_stack->top;
  v_occ = occ_stack->elems;

#if CLEAN_UP_SUBFORMULA_OCC_LIST
  int occ_removed = 0;
#endif

  while (v_occ < v_occ_end)
    {                           /* for all negative occurrences */
      FaultNode *occ = *v_occ;

      if (occ->deleted)
        {
#if !CLEAN_UP_SUBFORMULA_OCC_LIST
          v_occ++;
          continue;
#else
          occ_removed = 1;
          clean_up_invalid_occ (v_occ, v_occ_end);
#endif /* end: clean up occ-list */
        }

      assert (occ->node->atpg_info);
      assert (is_literal_node (occ->node));
      assert (occ->node->lit->negated);
      assert (!node_assigned (occ->node));

      forward_propagate_truth (nenofex, occ->node);

      if (atpg_rr->conflict || atpg_rr->prop_cutoff)
        {
#if CLEAN_UP_SUBFORMULA_OCC_LIST
          if (occ_removed)
            {
              clean_up_invalid_occ_set_top (occ_stack->top, v_occ_end);
            }
#endif
          return;               /* return early on conflict or cutoff */
        }

      v_occ++;
    }                           /* end: for all neg. occurrences */

#if CLEAN_UP_SUBFORMULA_OCC_LIST
  if (occ_removed)
    {
      occ_removed = 0;
      clean_up_invalid_occ_set_top (occ_stack->top, v_occ_end);
    }
#endif

  occ_stack = var->subformula_pos_occs;
  v_occ_end = occ_stack->top;
  v_occ = occ_stack->elems;

#if 0
  --------
#if CLEAN_UP_SUBFORMULA_OCC_LIST
    occ_removed = 0;
#endif
  -------
#endif
    while (v_occ < v_occ_end)
    {                           /* for all positive occurrences */
      FaultNode *occ = *v_occ;

      if (occ->deleted)
        {
#if !CLEAN_UP_SUBFORMULA_OCC_LIST
          v_occ++;
          continue;
#else
          occ_removed = 1;
          clean_up_invalid_occ (v_occ, v_occ_end);
#endif /* end: clean up occ-list */
        }

      assert (occ->node->atpg_info);
      assert (is_literal_node (occ->node));
      assert (!occ->node->lit->negated);
      assert (!node_assigned (occ->node));

      forward_propagate_falsity (nenofex, occ->node);

      if (atpg_rr->conflict || atpg_rr->prop_cutoff)
        {
#if CLEAN_UP_SUBFORMULA_OCC_LIST
          if (occ_removed)
            {
              clean_up_invalid_occ_set_top (occ_stack->top, v_occ_end);
            }
#endif
          return;               /* return early on conflict or cutoff */
        }

      v_occ++;
    }                           /* end: for all pos. occurrences */

#if CLEAN_UP_SUBFORMULA_OCC_LIST
  if (occ_removed)
    {
      clean_up_invalid_occ_set_top (occ_stack->top, v_occ_end);
    }
#endif

  assert (!atpg_rr->conflict);
  assert (!atpg_rr->prop_cutoff);
}


static void
propagate_enqueued_variable_assignments (Nenofex * nenofex)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;
  Queue *atpg_rr_propagation_queue = atpg_rr->propagation_queue;
  Stack *atpg_rr_propagated_vars = atpg_rr->propagated_vars;

  assert (!atpg_rr->conflict);
  assert (!atpg_rr->prop_cutoff);

  Var *assigned_var;
  while (!atpg_rr->conflict && !atpg_rr->prop_cutoff &&
         (assigned_var = dequeue (atpg_rr_propagation_queue)))
    {
      assert (var_assigned (assigned_var));

      push_stack (atpg_rr->mm, atpg_rr_propagated_vars, assigned_var);

      if (var_assigned_true (assigned_var))
        propagate_variable_assigned_true (nenofex, assigned_var);
      else
        propagate_variable_assigned_false (nenofex, assigned_var);
    }                           /* end: for all enqueued assignments */
}


/*
- mark nodes on fault-effect path -> will trigger backward-propagations
- called from ATPG-redundancy-removal only
*/
static void
mark_path_nodes (Nenofex * nenofex, Node * fault_node)
{
  assert (!nenofex->atpg_rr->global_flow_optimizing);
  unsigned const int atpg_root_level = nenofex->changed_subformula.lca->level;

  do
    {
      assert (!fault_node->atpg_info->path_mark);
      fault_node->atpg_info->path_mark = 1;

      fault_node = fault_node->parent;
    }
  while (fault_node && fault_node->level >= atpg_root_level);
}


static void
collect_fault_path_node (ATPGRedundancyRemover * atpg_rr,
                         Node * fault_path_node);


/*
- unmark nodes on fault-effect path
- called from ATPG-redundancy-removal only
*/
static void
unmark_path_nodes (Nenofex * nenofex, Node * fault_node,
                   unsigned const int collect_fault_path_nodes)
{
  assert (!nenofex->atpg_rr->global_flow_optimizing);
  unsigned const int atpg_root_level = nenofex->changed_subformula.lca->level;

  do
    {
      assert (fault_node->atpg_info->path_mark);

      /* NOTE: check could be hoisted up */
      if (collect_fault_path_nodes)
        collect_fault_path_node (nenofex->atpg_rr, fault_node);

      fault_node->atpg_info->path_mark = 0;
      fault_node = fault_node->parent;
    }
  while (fault_node && fault_node->level >= atpg_root_level);
}


/*
- called during ATPG-redundancy-removal
- collect necessary var. assignments derived in path inspection (path sensitization)
*/
static void
collect_necessary_off_path_literal_at_or (Nenofex * nenofex, Node * ch)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;
  Queue *atpg_rr_propagation_queue = atpg_rr->propagation_queue;

  assert (is_or_node (ch->parent));
  assert (is_literal_node (ch));

  Lit *lit = ch->lit;
  Var *var = lit->var;

  if ((lit->negated && var_assigned_false (var)) ||
      (!lit->negated && var_assigned_true (var)))
    {                           /* conflict occurred -> abort */
      atpg_rr->conflict = 1;
      return;
    }
  else                          /* assign (possibly already assigned with non-conflicting value) */
    {
      assert (lit->negated || !var_assigned_true (var));
      assert (!lit->negated || !var_assigned_false (var));

      if (!var_assigned (var))
        {                       /* assign and enqueue */
          if (lit->negated)
            var_assign_true (var);
          else
            var_assign_false (var);

          enqueue (atpg_rr->mm, atpg_rr_propagation_queue, var);
        }
    }
}


/*
- called during ATPG-redundancy-removal
- collect necessary var. assignments derived in path inspection (path sensitization)
*/
static void
collect_necessary_off_path_literal_at_and (Nenofex * nenofex, Node * ch)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;
  Queue *atpg_rr_propagation_queue = atpg_rr->propagation_queue;

  assert (is_and_node (ch->parent));
  assert (is_literal_node (ch));

  Lit *lit = ch->lit;
  Var *var = lit->var;

  if ((lit->negated && var_assigned_true (var)) ||
      (!lit->negated && var_assigned_false (var)))
    {                           /* conflict occurred */
      atpg_rr->conflict = 1;
      return;
    }
  else                          /* assign (possibly already be assigned with non-conflicting value) */
    {
      assert (lit->negated || !var_assigned_false (var));
      assert (!lit->negated || !var_assigned_true (var));

      if (!var_assigned (var))
        {                       /* assign and enqueue */
          if (lit->negated)
            var_assign_false (var);
          else
            var_assign_true (var);

          enqueue (atpg_rr->mm, atpg_rr_propagation_queue, var);
        }
    }
}


/*
- called during ATPG-redundancy-removal
- collect necessary var. assignments derived in path inspection (path sensitization)
*/
static void
collect_necessary_off_path_literals (Nenofex * nenofex, Node * fault_node)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;

  if (fault_node == nenofex->changed_subformula.lca)
    return;                     /* nothing to collect */

  Node *cur, *prev;

  cur = fault_node->parent;
  assert (cur);

  /* handle fault_node's parent first -> saves check for fault_node later
     -> MUST exclude fault_node else will get erroneous conflicts */

  if (is_or_node (cur))
    {
      Node *ch;
      for (ch = cur->child_list.first;
           ch && (is_literal_node (ch)); ch = ch->level_link.next)
        {
          if (ch == fault_node || !ch->atpg_info)
            continue;

          collect_necessary_off_path_literal_at_or (nenofex, ch);
        }                       /* end: for all literal-children */
    }
  else                          /* AND */
    {
      assert (is_and_node (cur));

      Node *ch;
      for (ch = cur->child_list.first;
           ch && (is_literal_node (ch)); ch = ch->level_link.next)
        {
          if (ch == fault_node || !ch->atpg_info)
            continue;

          collect_necessary_off_path_literal_at_and (nenofex, ch);
        }                       /* end: for all literal-children */
    }                           /* end: cur is AND */

  unsigned const int top_level = nenofex->changed_subformula.lca->level;
  prev = cur;
  cur = cur->parent;

  /* traverse fault path upwards until root of 'changed-subformula' */

  while (cur && cur->level >= top_level && !atpg_rr->conflict)
    {
      if (is_or_node (cur))
        {
          Node *ch;
          for (ch = cur->child_list.first;
               ch && (is_literal_node (ch)); ch = ch->level_link.next)
            {
              if (!ch->atpg_info)
                continue;

              collect_necessary_off_path_literal_at_or (nenofex, ch);
            }                   /* end: for all literal-children */
        }
      else                      /* AND */
        {
          assert (is_and_node (cur));

          Node *ch;
          for (ch = cur->child_list.first;
               ch && (is_literal_node (ch)); ch = ch->level_link.next)
            {
              if (!ch->atpg_info)
                continue;

              collect_necessary_off_path_literal_at_and (nenofex, ch);
            }                   /* end: for all literal-children */
        }                       /* end: cur is AND */

      prev = cur;
      cur = cur->parent;
    }                           /* end: while top of atpg-relevant graph not reached */
}


/* 
- returns non-zero if fault at 'fault_node' is untestable -> delete subtree at 'fault node' 
*/
static int
test_fault_is_redundant (Nenofex * nenofex, Node * fault_node)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;

  assert (!atpg_rr->conflict);
  assert (!atpg_rr->prop_cutoff);
  assert (count_queue (atpg_rr->propagation_queue) == 0);

  int redundant_fault = 0;

  ATPGFaultType fault_type;
  if (!fault_node->parent)
    {
      assert (!is_literal_node (fault_node));
      fault_type = is_or_node (fault_node) ?
        ATPG_FAULT_TYPE_STUCK_AT_1 : ATPG_FAULT_TYPE_STUCK_AT_0;
    }
  else
    {
      fault_type = is_or_node (fault_node->parent) ?
        ATPG_FAULT_TYPE_STUCK_AT_0 : ATPG_FAULT_TYPE_STUCK_AT_1;
    }

  assert (fault_type != ATPG_FAULT_TYPE_STUCK_AT_0 || is_and_node (fault_node)
          || (is_literal_node (fault_node)
              && is_or_node (fault_node->parent)));
  assert (fault_type != ATPG_FAULT_TYPE_STUCK_AT_1 || is_or_node (fault_node)
          || (is_literal_node (fault_node)
              && is_and_node (fault_node->parent)));

  fault_sensitization (atpg_rr, fault_node, fault_type);
  assert (!atpg_rr->conflict);

  /* NOTE: could merge path marking and off-path-literal collection */
  collect_necessary_off_path_literals (nenofex, fault_node);
  mark_path_nodes (nenofex, fault_node);

  if (atpg_rr->conflict)
    {                           /* best case:  conflict without propagations */
      redundant_fault = 1;
    }
  else if (count_queue (atpg_rr->propagation_queue) != 0)
    {                           /* average case: start propagating enqueued assignments */
      propagate_enqueued_variable_assignments (nenofex);

      if (atpg_rr->conflict)
        {
          assert (!atpg_rr->prop_cutoff);
          redundant_fault = 1;
        }
    }
  else                          /* no variable assignments enqueued -> need not reset any nodes */
    {
      atpg_rr->restricted_clean_up = 1;
    }

  return redundant_fault;
}


/*
- delete redundant node from graph
- must set solver-result if necessary 
*/
static void
delete_redundant_subformula (Nenofex * nenofex, Node * redundant_fault)
{
  assert (nenofex->changed_subformula.lca);

#if 0
  if (redundant_fault == nenofex->changed_subformula.lca)
    {
      fprintf (stderr, "ATPG-ROOT REDUNDANT\n");
    }
#endif

  if (redundant_fault == nenofex->graph_root)
    {                           /* in this case, must set solver result explicitly */
      if (is_or_node (nenofex->graph_root))
        nenofex->result = NENOFEX_RESULT_SAT;   /* has been tested s-a-1 */
      else
        {
          assert (is_and_node (nenofex->graph_root));
          nenofex->result = NENOFEX_RESULT_UNSAT;       /* has been tested s-a-0 */
        }
    }

  remove_and_free_subformula (nenofex, redundant_fault);
}


/*
- collect nodes on fault path
- used for marking variables for updates
*/
static void
collect_fault_path_node (ATPGRedundancyRemover * atpg_rr,
                         Node * fault_path_node)
{
  ATPGInfo *atpg_info = fault_path_node->atpg_info;

  if (!atpg_info->fault_path_node_collected)
    {
      atpg_info->fault_path_node_collected = 1;
      push_stack (atpg_rr->mm, atpg_rr->fault_path_nodes, atpg_info);
    }
}


/* 
- unassign all affected variables, clean up collections
*/
static void
reset_touched_variables (ATPGRedundancyRemover * atpg_rr)
{
  Var *var;
  Queue *atpg_rr_propagation_queue = atpg_rr->propagation_queue;
  Stack *atpg_rr_propagated_vars = atpg_rr->propagated_vars;

  while ((var = dequeue (atpg_rr_propagation_queue)))
    {
      var_unassign (var);
    }                           /* end: while queue not empty */

  while ((var = pop_stack (atpg_rr_propagated_vars)))
    {
      var_unassign (var);
    }                           /* end: while stack not empty */
}


/*
- core function for ATPG-redundancy-removal
- test all nodes on fault-queue for untestable stuck-at-faults
- abort if EITHER saturated OR prop. cutoff occurs OR 'changed-subformula' is deleted 
- invalid nodes are discarged from queue on the fly
- saturated if in one pass over all nodes on queue no more redundancies are found
- TODO: restrict set of nodes to be tested 
*/
static int
test_all_faults (Nenofex * nenofex, ATPGRedundancyRemover * atpg_rr)
{
  FaultNode *fault_node;
  Queue *non_redundant_faults = create_queue (atpg_rr->mm, DEFAULT_QUEUE_SIZE);

  int redundancies_found = 0;

  assert (!atpg_rr->atpg_prop_cutoff);
  assert (!atpg_rr->prop_cutoff);
  assert (!atpg_rr->stats.fwd_prop_cnt);
  assert (!atpg_rr->stats.bwd_prop_cnt);
  assert (!atpg_rr->global_flow_optimizing);
  assert (!atpg_rr->restricted_clean_up);
  assert (count_stack (atpg_rr->bwd_prop_stack) == 0);
  assert (count_stack (atpg_rr->touched_nodes) == 0);
  assert (count_stack (atpg_rr->propagated_vars) == 0);
  assert (!nenofex->atpg_rr_abort);

  atpg_rr->stats.fwd_prop_cnt = atpg_rr->atpg_fwd_prop_cnt;
  atpg_rr->stats.bwd_prop_cnt = atpg_rr->atpg_bwd_prop_cnt;

  int continue_testing = 1;

  while (continue_testing)
    {
      continue_testing = 0;

#if !RESTRICT_ATPG_FAULT_NODE_SET
      assert (atpg_rr->global_atpg_test_node_mark == 0);
#endif

      while (!atpg_rr->prop_cutoff
             && (fault_node = dequeue (atpg_rr->fault_queue)))
        {
          if (fault_node->deleted || fault_node->skip)
            continue;

#if RESTRICT_ATPG_FAULT_NODE_SET
          ATPGInfo *node_atpg_info = fault_node->node->atpg_info;
          /* IMPORTANT NOTE: THIS DOES NOT YET WORK PROPERLY TOGETHER WITH SUCCESSIVE CALLS OF FUNCTION */
          node_atpg_info->queue_mark = !atpg_rr->global_atpg_test_node_mark;    /* mark as being taken from queue */

          if (node_atpg_info->cur_atpg_test_node_mark !=
              atpg_rr->global_atpg_test_node_mark)
            {                   /* anticipate non-redundant faults -> do not test in current iteration */
              if (node_atpg_info->next_atpg_test_node_mark !=
                  atpg_rr->global_atpg_test_node_mark)
                node_atpg_info->cur_atpg_test_node_mark =
                  atpg_rr->global_atpg_test_node_mark;
              else              /* node will be tested in next iteration */
                node_atpg_info->cur_atpg_test_node_mark =
                  node_atpg_info->next_atpg_test_node_mark;

              enqueue (atpg_rr->mm, non_redundant_faults, fault_node);
              continue;
            }
#endif

#if ATPG_SKIP_FAULT_NODES_NO_LIT_CHILDREN
          if (!is_literal_node (fault_node->node) &&
              !is_literal_node (fault_node->node->child_list.first))
            {
              enqueue (atpg_rr->mm, non_redundant_faults, fault_node);
              continue;
            }
#endif

#if ATPG_SKIP_FAULT_NODES_OP_CHILDREN
          if (!is_literal_node (fault_node->node) &&
              !is_literal_node (fault_node->node->child_list.last))
            {
              enqueue (atpg_rr->mm, non_redundant_faults, fault_node);
              continue;
            }
#endif

          assert (fault_node->node->atpg_info->fault_node);
          assert (nenofex->changed_subformula.lca || nenofex->atpg_rr_abort);
          assert (!nenofex->atpg_rr_reset_changed_subformula ||
                  nenofex->changed_subformula.lca);
          assert (!fault_node->node->atpg_info->collected);
          assert (!nenofex->atpg_rr_abort);
          assert (!count_stack (atpg_rr->touched_nodes));
          assert (!count_queue (atpg_rr->propagation_queue));
          assert (count_stack (atpg_rr->propagated_vars) == 0);
          assert (!atpg_rr->restricted_clean_up);

          atpg_rr->stats.fault_cnt++;

          if (test_fault_is_redundant (nenofex, fault_node->node))
            {
#if PRINT_INFO_DETAILS
              fprintf (stderr,
                       "Fault at node->id %d turns out to be redundant\n",
                       fault_node->node->id);
#endif
              continue_testing = 1;
              atpg_rr->stats.red_fault_cnt++;

              assert (!atpg_rr->restricted_clean_up);

              unmark_path_nodes (nenofex, fault_node->node, 1);
              delete_redundant_subformula (nenofex, fault_node->node);

              if (nenofex->atpg_rr_abort)
                {
                  continue_testing = 0;
                  break;
                }

              reset_touched_variables (atpg_rr);
              reset_touched_nodes (atpg_rr);
              atpg_rr->conflict = 0;
            }                   /* end: redundant fault */
          else
            {
#if RESTRICT_ATPG_FAULT_NODE_SET
              node_atpg_info->cur_atpg_test_node_mark =
                node_atpg_info->next_atpg_test_node_mark;
#endif
              enqueue (atpg_rr->mm, non_redundant_faults, fault_node);
              unmark_path_nodes (nenofex, fault_node->node, 0);

              if (!atpg_rr->restricted_clean_up)
                {
                  reset_touched_variables (atpg_rr);
                  reset_touched_nodes (atpg_rr);
                }
              else              /* path nodes already unmarked */
                {
                  atpg_rr->restricted_clean_up = 0;
                }
              assert (!atpg_rr->conflict);
            }
        }                       /* end: while fault-queue not empty */

      if (continue_testing)
        {                       /* swap fault queues and run all tests again */
          Queue *tmp = atpg_rr->fault_queue;
          atpg_rr->fault_queue = non_redundant_faults;
          non_redundant_faults = tmp;

#if RESTRICT_ATPG_FAULT_NODE_SET
          /* flip test mark */
          atpg_rr->global_atpg_test_node_mark =
            !atpg_rr->global_atpg_test_node_mark;
#endif
        }

    }                           /* end: while continue testing */

  /* testing was either aborted or cutoff occurred or ran until saturation */
  assert (atpg_rr->prop_cutoff || nenofex->atpg_rr_abort ||
          count_queue (atpg_rr->fault_queue) == 0);

  /* pass all fault nodes onto following calls of optimization procedures */
  while ((fault_node = dequeue (atpg_rr->fault_queue)))
    {
      enqueue (atpg_rr->mm, non_redundant_faults, fault_node);
    }

  /* swap queues */
  Queue *tmp = atpg_rr->fault_queue;
  atpg_rr->fault_queue = non_redundant_faults;
  non_redundant_faults = tmp;

#if 0
  -------------
/* must flip mark in order to test nodes in following optimization passes? */
#if RESTRICT_ATPG_FAULT_NODE_SET
    atpg_rr->global_atpg_test_node_mark =
    !atpg_rr->global_atpg_test_node_mark;
#endif
  -------------
#endif
    assert (count_queue (non_redundant_faults) == 0);
  delete_queue (atpg_rr->mm, non_redundant_faults);

  if (nenofex->options.show_opt_info_specified)
    {
      fprintf (stderr, "\nATPG Redundancy Removal Statistics: \n");
      fprintf (stderr, "-----------------------------------\n");
      fprintf (stderr, " #Fwd_prop = %d\n",
               nenofex->atpg_rr->stats.fwd_prop_cnt);
      fprintf (stderr, " #Bwd_prop = %d\n",
               nenofex->atpg_rr->stats.bwd_prop_cnt);
      fprintf (stderr, " #tested_faults = %d\n",
               nenofex->atpg_rr->stats.fault_cnt);
      fprintf (stderr, " #red_faults = %d\n",
               nenofex->atpg_rr->stats.red_fault_cnt);
      fprintf (stderr, "\n");
    }

  redundancies_found = nenofex->atpg_rr->stats.red_fault_cnt;
  atpg_rr->atpg_fwd_prop_cnt = atpg_rr->stats.fwd_prop_cnt;
  atpg_rr->atpg_bwd_prop_cnt = atpg_rr->stats.bwd_prop_cnt;

  memset (&(atpg_rr->stats), 0, sizeof (atpg_rr->stats));

  atpg_rr->atpg_prop_cutoff = atpg_rr->prop_cutoff;

  assert (nenofex->atpg_rr_abort
          || count_stack (atpg_rr->touched_nodes) == 0);
  atpg_rr->prop_cutoff = 0;
  atpg_rr->conflict = 0;
  atpg_rr->restricted_clean_up = 0;
  reset_stack (atpg_rr->bwd_prop_stack);
  atpg_rr->prop_cutoff = atpg_rr->restricted_clean_up = 0;

  return redundancies_found;
}


/*
- for debugging purposes
*/
static void
print_atpg_graph (Nenofex * nenofex)
{
  Node *root = nenofex->changed_subformula.lca;

  if (is_literal_node (root))
    return;

  Stack *stack = create_stack (nenofex->mm, 1);
  Node *cur;

  Node **child;
  for (child = nenofex->changed_subformula.top_p - 1;
       child >= nenofex->changed_subformula.children; child--)
    {
      push_stack (nenofex->mm, stack, (void *) *child);
    }

  printf ("%d (%s): ", root->id, is_or_node (root) ? "||" : "&&");

  for (child = nenofex->changed_subformula.children; *child; child++)
    {
      printf ("%d", (*child)->id);
      if (is_literal_node (*child))
        printf ("L");
      if ((*child)->level_link.next)
        printf (", ");
    }
  printf ("\n");

  while ((cur = (Node *) pop_stack (stack)))
    {
      if (!is_literal_node (cur))
        {
          printf ("%d (%s): ", cur->id, is_or_node (cur) ? "||" : "&&");

          Node *child;
          for (child = cur->child_list.last; child;
               child = child->level_link.prev)
            {
              push_stack (nenofex->mm, stack, (void *) child);
            }

          for (child = cur->child_list.first; child;
               child = child->level_link.next)
            {
              printf ("%d", child->id);
              if (is_literal_node (child))
                printf ("L");
              if (child->level_link.next)
                printf (", ");
            }
          printf ("\n");
        }
    }                           /* end: while */
  delete_stack (nenofex->mm, stack);
}


/* ----------- START: GLOBAL FLOW OPTIMIZATION ------------ */


/*
- TODO: POSSIBLE BOTTLENECK
- could instead check if node has a watcher stack?
*/
static long int
all_children_atpg_relevant (Node * node)
{
  long int result = 1;

  Node *ch;
  for (ch = node->child_list.first; result && ch; ch = ch->level_link.next)
    {
      result = (long int) ch->atpg_info;
    }

  return result;
}


/* 
- if any, find implication on fault path which is closest to root 
- restricted to implications of the form 1 -> 1 and 0 -> 0
- search starts at grand_parent of fault node;
*/
static Node *
find_highest_implication_on_path (Nenofex * nenofex, Node * fault_node)
{
  Node *changed_subformula_lca = nenofex->changed_subformula.lca;
  unsigned const int changed_subformula_lca_level =
    changed_subformula_lca->level;

  assert (fault_node);

  if (!node_assigned (fault_node))
    {                           /* unable to assign fault_node; may happen if fault node has op.ch. + lit. ch. */
      assert (!is_literal_node (fault_node));
#if ASSERT_CHILDREN_ASSIGNMENTS
      assert (count_children_assigned_value
              (fault_node, ATPG_ASSIGNMENT_UNDEFINED));
#endif
      return 0;
    }

  /* SUPPOSED BUG FIX: the two checks cover the cases where fault sens. failed
     -> not sure if can ever happen
   */
  if (node_assigned_true (fault_node) && is_or_node (fault_node))
    return 0;

  if (node_assigned_false (fault_node) && is_and_node (fault_node))
    return 0;

  Node *high_impl = 0;
  Node *cur = fault_node->parent;

  assert ((!cur || cur->level != changed_subformula_lca_level) ||
          cur == changed_subformula_lca);

  if (!cur || cur->level <= changed_subformula_lca_level)
    return 0;

  cur = cur->parent;

  assert (cur);
  assert (cur->level >= changed_subformula_lca_level);

  do
    {
      if (node_assigned (cur) == node_assigned (fault_node))
        high_impl = cur;
      cur = cur->parent;
    }
  while (cur && cur->level >= changed_subformula_lca_level);

  assert (!high_impl
          || node_assigned (high_impl) == node_assigned (fault_node));

  /* 
     unclear: ignore 'unreliable' implications
     -> those where implied node's value is not necessarily justified at global view
   */
  if (high_impl)
    {
      if ((is_and_node (high_impl) && node_assigned_true (high_impl) &&
           !all_children_atpg_relevant (high_impl)) ||
          (is_or_node (high_impl) && node_assigned_false (high_impl) &&
           !all_children_atpg_relevant (high_impl)))
        high_impl = 0;
    }

  return high_impl;
}


/* 
- similar to 'test_fault_is_redundant'
*/
static Node *
derive_implications_from_node (Nenofex * nenofex, Node * fault_node,
                               ATPGFaultType fault_type)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;

  assert (!atpg_rr->conflict);
  assert (!atpg_rr->prop_cutoff);
  assert (count_queue (atpg_rr->propagation_queue) == 0);
  assert (fault_type != ATPG_FAULT_TYPE_STUCK_AT_0 ||
          is_and_node (fault_node) || is_literal_node (fault_node));
  assert (fault_type != ATPG_FAULT_TYPE_STUCK_AT_1 ||
          is_or_node (fault_node) || is_literal_node (fault_node));

  Node *relevant_implication = 0;

  fault_sensitization (atpg_rr, fault_node, fault_type);
  assert (!atpg_rr->conflict);

  if (count_queue (atpg_rr->propagation_queue) != 0)
    {                           /* start propagating enqueued assignments */
      propagate_enqueued_variable_assignments (nenofex);

      assert (!atpg_rr->conflict);

      if (!atpg_rr->prop_cutoff)
        relevant_implication =
          find_highest_implication_on_path (nenofex, fault_node);
    }
  else                          /* no assignments enqueued */
    {
      atpg_rr->restricted_clean_up = 1;
    }

  return relevant_implication;
}


/*
- relink implicant and update graph properties
*/
static void
unlink_and_add_implication (Nenofex * nenofex, Node * fault_node,
                            Node * insert_at)
{
  FaultNode *fault_parent = fault_node->parent->atpg_info->fault_node;
  Node *fault_parent_node = fault_parent->node;

  if (fault_parent_node->atpg_info->atpg_ch)
    {
      remove_child_from_watcher_list (fault_parent_node, fault_node);
    }
  unlink_node (nenofex, fault_node);
  update_size_subformula (nenofex, fault_parent_node,
                          -fault_node->size_subformula);
  add_node_to_child_list (nenofex, insert_at, fault_node);
  update_size_subformula (nenofex, fault_node->parent,
                          +fault_node->size_subformula);
  update_level (nenofex, fault_node);

  assert (!fault_parent->deleted);

  if (is_literal_node (fault_node))
    {
      assert (fault_node->occ_link.next || fault_node->occ_link.prev ||
              fault_node == fault_node->lit->occ_list.first);
      simplify_one_level (nenofex, insert_at);
    }

  if (!fault_parent->deleted && fault_parent_node->num_children == 1)
    {
      assert (fault_parent_node->child_list.first ==
              fault_parent_node->child_list.last);

      if (is_literal_node (fault_parent_node->child_list.first))
        update_size_subformula (nenofex, fault_parent_node, -1);
      else
        update_size_subformula (nenofex, fault_parent_node, -2);        /* save another node */

      merge_parent (nenofex, fault_parent_node);
    }

#ifndef NDEBUG
  if (!fault_parent->deleted)
    {
      assert (fault_parent_node->atpg_info->collected);
    }
#endif
}


/* 
- performs transformation of graph as suggested by implication
- supporting implications of types 0->0, 1->1 only
    -> else violating NNF (other two types require  to introduce negation)
- y == 1 -> x == 1: y OR x
- y == 0 -> x == 0: y AND x
- 4 cases for insert-position are possible: 
    -> maintain watcher-lists and 'changed-subformula' accordingly
- nodes are NOT copied, just relinked:  transformations never increase graph size 
    (except if new root needs to be created)
- need not copy nodes because by this method it is guaranteed that original node
    will yield nontestable fault after adding implication
- implications at nodes are tried out according to stuck-at fault model
    (OR/LIT at AND: set to 0; AND/LIT at OR, set to 1)

- STILL TO BE CLARIFIED: if setting lits to other values as described then might 
    have to COPY nodes, not just relink (fault at orig. node not necessarily untestable?)
*/

#define apply_transformation() \
  insert_at_fault_node = insert_at->atpg_info->fault_node; \
  unlink_and_add_implication(nenofex, fault_node, insert_at); \
  if (!insert_at_fault_node->deleted) \
  { /* transformation may cause deletion of insert-node */ \
    if (insert_at->atpg_info->atpg_ch) \
    { \
      push_stack(atpg_rr->mm, insert_at->atpg_info->atpg_ch, fault_node->atpg_info->fault_node); \
    } \
    init_counter_and_watcher(insert_at); \
  }

static void
transform_subformula_by_global_flow_implication (Nenofex * nenofex,
                                                 Node * fault_node,
                                                 Node * highest_implication)
{

  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;
  LCAObject *changed_subformula = &(nenofex->changed_subformula);

  assert (!nenofex->atpg_rr_abort);
  assert (node_assigned (fault_node));
  assert (node_assigned (fault_node) == node_assigned (highest_implication));

  int true_implies_true = node_assigned_true (fault_node);

  assert (!true_implies_true || node_assigned_true (highest_implication));
  assert (true_implies_true || node_assigned_false (highest_implication));

  /* 
     distinguishing 4 global cases for inserting implicant where 
     each case has two subcases depending on node type 
   */

  Node *insert_at;
  FaultNode *insert_at_fault_node;

  if (highest_implication == nenofex->graph_root)
    {                           /* CASE 1 */
      assert (highest_implication == changed_subformula->lca);
      assert (!is_literal_node (nenofex->graph_root));

      if ((true_implies_true && is_and_node (highest_implication)) ||
          (!true_implies_true && is_or_node (highest_implication)))
        {
          /* need to create new graph-root and changed-subformula-root */

#if PRINT_INFO_DETAILS
          fprintf (stderr, "TRANSFORM CASE 1.1\n");
#endif

          collect_fault_path_node (atpg_rr, highest_implication);

          insert_at = is_and_node (highest_implication) ?
            or_node (nenofex) : and_node (nenofex);
          add_node_to_child_list (nenofex, insert_at, nenofex->graph_root);
          update_level (nenofex, nenofex->graph_root);
          update_size_subformula (nenofex, insert_at,
                                  nenofex->graph_root->size_subformula + 1);

          assign_node_atpg_info (atpg_rr, insert_at);
          assert (!insert_at->atpg_info->atpg_ch);
          assert (!insert_at->atpg_info->watcher_pos);
          enqueue (atpg_rr->mm, atpg_rr->fault_queue, insert_at->atpg_info->fault_node);

          nenofex->graph_root = insert_at;

          reset_changed_lca_object (nenofex);
          assert (nenofex->changed_subformula.size_children > 2);

          nenofex->changed_subformula.lca = insert_at;
          assert (insert_at->child_list.first);
          assert (insert_at->child_list.first == insert_at->child_list.last);
          add_changed_lca_child (nenofex, insert_at->child_list.first);
          add_changed_lca_child (nenofex, fault_node);
        }
      else
        {
          assert ((true_implies_true && is_or_node (highest_implication)) ||
                  (!true_implies_true && is_and_node (highest_implication)));

#if PRINT_INFO_DETAILS
          fprintf (stderr, "TRANSFORM CASE 1.2\n");
#endif

          insert_at = highest_implication;
          add_changed_lca_child (nenofex, fault_node);
        }

      apply_transformation ();
    }                           /* end CASE 1: highest_implication == nenofex->graph_root */
  else if (highest_implication == changed_subformula->lca)
    {                           /* CASE 2 */

      if ((true_implies_true && is_and_node (highest_implication)) ||
          (!true_implies_true && is_or_node (highest_implication)))
        {
          insert_at = highest_implication->parent;
          collect_fault_path_node (atpg_rr, highest_implication);

#if PRINT_INFO_DETAILS
          fprintf (stderr, "TRANSFORM CASE 2.1\n");
#endif

          ATPGInfo *insert_at_atpg_info;
          /* BUG-FIX: problem if changed-lca had only 1 child remaining -> will be set back; 
             ->parent will already have atpg_info-pointer */
          if (!(insert_at_atpg_info = insert_at->atpg_info))
            {
              assign_node_atpg_info (atpg_rr, insert_at);

              insert_at_atpg_info = insert_at->atpg_info;
              /* special case: 'insert_at' will get watcher list with exactly two ch. */
              insert_at_atpg_info->atpg_ch =
                create_stack (atpg_rr->mm, DEFAULT_STACK_SIZE);
              insert_at_atpg_info->watcher_pos =
                insert_at_atpg_info->atpg_ch->elems;
              push_stack (atpg_rr->mm, insert_at_atpg_info->atpg_ch,
                          highest_implication->atpg_info->fault_node);
            }
          else                  /* has already pointer to ATPInfo -> must set up watchers */
            {
              assert (count_stack (insert_at_atpg_info->atpg_ch) == 1);
              reset_stack (insert_at_atpg_info->atpg_ch);

              insert_at_atpg_info->watcher_pos =
                insert_at_atpg_info->atpg_ch->elems;
              push_stack (atpg_rr->mm, insert_at_atpg_info->atpg_ch,
                          highest_implication->atpg_info->fault_node);
            }

          /* NOT sure whether this is helpful: enqueue new root ? rather not */
          assert (insert_at_atpg_info->fault_node ==
                  highest_implication->parent->atpg_info->fault_node);
          enqueue (atpg_rr->mm, atpg_rr->fault_queue, insert_at_atpg_info->fault_node);

          Node *tmp = nenofex->changed_subformula.lca;
          reset_changed_lca_object (nenofex);
          nenofex->changed_subformula.lca = insert_at;
          add_changed_lca_child (nenofex, tmp);
          add_changed_lca_child (nenofex, fault_node);
        }
      else
        {
          assert ((true_implies_true && is_or_node (highest_implication)) ||
                  (!true_implies_true && is_and_node (highest_implication)));

#if PRINT_INFO_DETAILS
          fprintf (stderr, "TRANSFORM CASE 2.2\n");
#endif

          insert_at = highest_implication;
          add_changed_lca_child (nenofex, fault_node);
        }

      apply_transformation ();
    }
  else if (highest_implication->parent == changed_subformula->lca)
    {                           /* CASE 3 */

      if ((true_implies_true && is_and_node (highest_implication)) ||
          (!true_implies_true && is_or_node (highest_implication)))
        {
#if PRINT_INFO_DETAILS
          fprintf (stderr, "TRANSFORM CASE 3.1\n");
#endif

          insert_at = highest_implication->parent;

          collect_fault_path_node (atpg_rr, highest_implication);
          add_changed_lca_child (nenofex, fault_node);
        }
      else
        {
          assert ((true_implies_true && is_or_node (highest_implication)) ||
                  (!true_implies_true && is_and_node (highest_implication)));

#if PRINT_INFO_DETAILS
          fprintf (stderr, "TRANSFORM CASE 3.2\n");
#endif

          insert_at = highest_implication;
        }

      apply_transformation ();
    }
  else                          /* end CASE 3 */
    {                           /* CASE 4 */

      if ((true_implies_true && is_and_node (highest_implication)) ||
          (!true_implies_true && is_or_node (highest_implication)))
        {
#if PRINT_INFO_DETAILS
          fprintf (stderr, "TRANSFORM CASE 4.1\n");
#endif

          collect_fault_path_node (atpg_rr, highest_implication);
          insert_at = highest_implication->parent;
        }
      else
        {
          assert ((true_implies_true && is_or_node (highest_implication)) ||
                  (!true_implies_true && is_and_node (highest_implication)));

#if PRINT_INFO_DETAILS
          fprintf (stderr, "TRANSFORM CASE 4.2\n");
#endif

          insert_at = highest_implication;
        }

      apply_transformation ();
    }                           /* end CASE 4 */

  assert (!nenofex->changed_subformula.lca ||
          !is_literal_node (nenofex->changed_subformula.lca));
}


#ifndef NDEBUG
/*
- used during assertion checking only
*/
static void
assert_all_atpg_info_reset (ATPGRedundancyRemover * atpg_rr)
{
  ATPGInfo *atpg_info_p;
  ATPGInfo *end = atpg_rr->end_atpg_info;

  assert (atpg_rr->end_atpg_info ==
          atpg_rr->atpg_info_array +
          atpg_rr->byte_size_atpg_info_array / sizeof (ATPGInfo));

  atpg_info_p = atpg_rr->atpg_info_array;

  for (; atpg_info_p < end; atpg_info_p++)
    {
      FaultNode *fault_node = atpg_info_p->fault_node;

      if (!fault_node || fault_node->deleted)
        continue;

      assert (atpg_info_p->assignment == 0);
      assert (atpg_info_p->justified == 0);
      assert (atpg_info_p->path_mark == 0);

      if (!is_literal_node (fault_node->node))
        {
          assert (!atpg_info_p->atpg_ch ||
                  atpg_info_p->unassigned_ch_cnt ==
                  count_stack (atpg_info_p->atpg_ch));
          assert (atpg_info_p->atpg_ch
                  || atpg_info_p->unassigned_ch_cnt ==
                  fault_node->node->num_children);
          assert (!atpg_info_p->atpg_ch
                  || atpg_info_p->watcher ==
                  ((FaultNode *) * atpg_info_p->watcher_pos)->node);
          assert (!atpg_info_p->atpg_ch
                  || atpg_info_p->watcher_pos == atpg_info_p->atpg_ch->elems);
          assert (atpg_info_p->atpg_ch
                  || atpg_info_p->watcher ==
                  fault_node->node->child_list.first);
          assert (atpg_info_p->atpg_ch || !atpg_info_p->watcher_pos);
        }
    }                           /* end: for */

  void **v_var, **v_end;
  v_end = atpg_rr->subformula_vars->top;
  for (v_var = atpg_rr->subformula_vars->elems; v_var < v_end; v_var++)
    {
      Var *var = *v_var;
      assert (!var_assigned (var));
    }
}
#endif /* end: ifndef NDEBUG */


/*
- called during global flow optimization
- collect nodes on path between implicant and highest implication
- used during variable marking (for cost update)
*/
static void
collect_implication_path_nodes (Nenofex * nenofex,
                                Node * implicant, Node * highest_implication)
{
  Node *cur = implicant->parent;
  assert (cur);
  assert (cur->parent);

  unsigned const int highest_implication_level = highest_implication->level;

  do
    {
      collect_fault_path_node (nenofex->atpg_rr, cur);
      cur = cur->parent;
    }
  while (cur->level > highest_implication_level);

  assert (cur == highest_implication);
}


/*
- traverse implicant and mark variables for cost update
- implicant's subgraph expected to be small
- TODO: restriction of var marking needed
*/
static void
mark_implicant_variables_for_update (Nenofex * nenofex,
                                     Node * implicant,
                                     Node * highest_implication)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;
  Stack *stack = create_stack (atpg_rr->mm, DEFAULT_STACK_SIZE);
  push_stack (atpg_rr->mm, stack, implicant);

  Node *node;
  while ((node = pop_stack (stack)))
    {
      if (is_literal_node (node))
        {
          Var *var = node->lit->var;
          lca_update_mark (var);
          dec_score_update_mark (var);
          inc_score_update_mark (var);
          collect_variable_for_update (nenofex, var);
        }
      else
        {
          Node *ch;
          for (ch = node->child_list.last; ch; ch = ch->level_link.prev)
            push_stack (atpg_rr->mm, stack, ch);
        }
    }                           /* end: while stack not empty */

  delete_stack (atpg_rr->mm, stack);
}


/* 
- similar to function 'test_all_faults'
- assign node a value and derive implications 
- if any, find highest one on path up to root
- see comments at function 'transform_subformula_by_global_flow_implication'
- nodes are tried until abortion, cutoff or saturation (see function 'test_all_faults')
- implications at graph root require to create new node which must get pointer to ATPGInfo 
    -> could run out of these pointers -> abort
- TODO: restrict set of nodes to be tried
*/
static int
optimize_by_global_flow (Nenofex * nenofex, ATPGRedundancyRemover * atpg_rr)
{
  FaultNode *fault_node;
  Queue *tested_nodes = create_queue (atpg_rr->mm, DEFAULT_QUEUE_SIZE);
  Node *highest_implication;
  ATPGFaultType fault_type = 0;
  int implications_found = 0;

  assert (!nenofex->atpg_rr_abort);
  assert (!atpg_rr->global_flow_prop_cutoff);
  assert (!atpg_rr->prop_cutoff);
  assert (!atpg_rr->conflict);
  assert (!atpg_rr->restricted_clean_up);
  assert (!atpg_rr->global_flow_optimizing);
  assert (atpg_rr->stats.fault_cnt == 0);
  assert (!atpg_rr->stats.fwd_prop_cnt);
  assert (!atpg_rr->stats.bwd_prop_cnt);
  assert (count_stack (atpg_rr->bwd_prop_stack) == 0);
  assert (count_stack (atpg_rr->touched_nodes) == 0);
  assert (count_stack (atpg_rr->propagated_vars) == 0);

  atpg_rr->stats.fwd_prop_cnt = atpg_rr->global_flow_fwd_prop_cnt;
  atpg_rr->stats.bwd_prop_cnt = atpg_rr->global_flow_bwd_prop_cnt;
  atpg_rr->global_flow_optimizing = 1;

  int continue_optimizing = 1;

  while (continue_optimizing)
    {
      continue_optimizing = 0;

      while (!atpg_rr->prop_cutoff
             && (fault_node = dequeue (atpg_rr->fault_queue)))
        {
          if (fault_node->deleted)
            continue;

          Node *node = fault_node->node;

#if GLOBAL_FLOW_SKIP_FAULT_NODES_NO_LIT_CHILDREN
          if (!is_literal_node (node)
              && !is_literal_node (node->child_list.first))
            {
              enqueue (atpg_rr->mm, tested_nodes, fault_node);
              continue;
            }
#endif

#if GLOBAL_FLOW_SKIP_FAULT_NODES_OP_CHILDREN
          if (!is_literal_node (node)
              && !is_literal_node (node->child_list.last))
            {
              enqueue (atpg_rr->mm, tested_nodes, fault_node);
              continue;
            }
#endif

          /* 
             NOTE: setting nodes to true (false), -> "test" for s-a-0 (s-a-1)
             -> can call function 'fault_sensitization' both in global flow and redundancy removal
           */

#if GLOBAL_FLOW_LITERALS_BOTH_PHASES
          assert (0);           /* BUGGY */

          if (!is_literal_node (node))
            fault_type = is_and_node (node) ?
              ATPG_FAULT_TYPE_STUCK_AT_0 : ATPG_FAULT_TYPE_STUCK_AT_1;
          else
            fault_type = ATPG_FAULT_TYPE_STUCK_AT_0;
#else
          if (!is_literal_node (node))
            fault_type = is_and_node (node) ?
              ATPG_FAULT_TYPE_STUCK_AT_0 : ATPG_FAULT_TYPE_STUCK_AT_1;
          else if (node->parent)
            fault_type = is_and_node (node->parent) ?
              ATPG_FAULT_TYPE_STUCK_AT_1 : ATPG_FAULT_TYPE_STUCK_AT_0;
          else
            {
              assert (0);
            }
#endif

#if GLOBAL_FLOW_LITERALS_BOTH_PHASES
        LITERAL_NODE_SECOND_PHASE:
#endif

          atpg_rr->stats.fault_cnt++;

          assert (!nenofex->atpg_rr_abort);
          assert (!atpg_rr->conflict);
          assert (!atpg_rr->restricted_clean_up);
          assert (!fault_node->node->atpg_info->collected);
          assert (!count_stack (atpg_rr->touched_nodes));
          assert (!count_queue (atpg_rr->propagation_queue));
          assert (count_stack (atpg_rr->propagated_vars) == 0);

          if ((highest_implication =
               derive_implications_from_node (nenofex, node, fault_type)))
            {
#if PRINT_INFO_DETAILS
              fprintf (stderr, "Fault at node->id %d yields implication\n",
                       node->id);
#endif
              assert (!atpg_rr->conflict);
              assert (!atpg_rr->prop_cutoff);
              assert (!atpg_rr->restricted_clean_up);

              collect_implication_path_nodes (nenofex, fault_node->node,
                                              highest_implication);
              mark_implicant_variables_for_update (nenofex, fault_node->node,
                                                   highest_implication);

              atpg_rr->stats.derived_implications_cnt++;

              continue_optimizing = 1;

              /* always enqueue tested nodes */
              enqueue (atpg_rr->mm, tested_nodes, fault_node);

              assert (!nenofex->atpg_rr_abort);

              /* abort if no more atpg_info pointers are left */
              if (atpg_rr->cur_atpg_info == atpg_rr->end_atpg_info - 1)
                {
#if 0
                  fprintf (stderr,
                           "No ATPGInfo pointers left, aborting global flow optimization\n");
#endif
                  reset_touched_variables (atpg_rr);
                  reset_touched_nodes (atpg_rr);
                  continue_optimizing = 0;
                  break;
                }

              transform_subformula_by_global_flow_implication (nenofex, node,
                                                               highest_implication);

              if (nenofex->atpg_rr_abort)
                {
                  continue_optimizing = 0;
                  break;
                }

              reset_touched_variables (atpg_rr);
              reset_touched_nodes (atpg_rr);
            }                   /* end: implication found  */
          else
            {
              if (!atpg_rr->restricted_clean_up)
                {
                  reset_touched_variables (atpg_rr);
                  reset_touched_nodes (atpg_rr);
                }
              else              /* need not reset anything in this context */
                {
                  atpg_rr->restricted_clean_up = 0;

#ifndef NDEBUG
#if ASSERT_ALL_ATPG_INFO_RESET
                  assert_all_atpg_info_reset (atpg_rr);
#endif
#endif
                }               /* end: restricted clean up */

#if GLOBAL_FLOW_LITERALS_BOTH_PHASES
              if (!atpg_rr->prop_cutoff && is_literal_node (node) &&
                  fault_type == ATPG_FAULT_TYPE_STUCK_AT_0)
                {
                  fault_type = ATPG_FAULT_TYPE_STUCK_AT_1;
                  goto LITERAL_NODE_SECOND_PHASE;
                }
#endif
              enqueue (atpg_rr->mm, tested_nodes, fault_node);
            }                   /* end: no implication found */

        }                       /* end: while fault-queue not empty */

      if (continue_optimizing)
        {                       /* swap fault queues and run all tests again */
          Queue *tmp = atpg_rr->fault_queue;
          atpg_rr->fault_queue = tested_nodes;
          tested_nodes = tmp;
        }

    }                           /* end: while continue optimizing */

  /* testing was either aborted or cutoff occurred or ran until saturation */

  /* pass all fault nodes onto following calls of optimization procedures */
  while ((fault_node = dequeue (atpg_rr->fault_queue)))
    {
      enqueue (atpg_rr->mm, tested_nodes, fault_node);
    }

  /* swap queues */
  Queue *tmp = atpg_rr->fault_queue;
  atpg_rr->fault_queue = tested_nodes;
  tested_nodes = tmp;

  assert (count_queue (tested_nodes) == 0);
  delete_queue (atpg_rr->mm, tested_nodes);

  if (nenofex->options.show_opt_info_specified)
    {
      fprintf (stderr, "\nGlobal Flow Statistics: \n");
      fprintf (stderr, "-----------------------\n");
      fprintf (stderr, " #Fwd_prop = %d\n",
               nenofex->atpg_rr->stats.fwd_prop_cnt);
      fprintf (stderr, " #tested_implications = %d\n",
               nenofex->atpg_rr->stats.fault_cnt);
      fprintf (stderr, " #derived_implications = %d\n",
               nenofex->atpg_rr->stats.derived_implications_cnt);
      fprintf (stderr, "\n");
    }

  implications_found = nenofex->atpg_rr->stats.derived_implications_cnt;
  atpg_rr->global_flow_fwd_prop_cnt = atpg_rr->stats.fwd_prop_cnt;
  atpg_rr->global_flow_bwd_prop_cnt = atpg_rr->stats.bwd_prop_cnt;

  memset (&(atpg_rr->stats), 0, sizeof (atpg_rr->stats));

  atpg_rr->global_flow_prop_cutoff = atpg_rr->prop_cutoff;

  assert (nenofex->atpg_rr->stats.bwd_prop_cnt == 0);
  assert (nenofex->atpg_rr->stats.red_fault_cnt == 0);

  atpg_rr->prop_cutoff = atpg_rr->restricted_clean_up = 0;
  assert (!atpg_rr->conflict);
  atpg_rr->global_flow_optimizing = 0;

  return implications_found;
}

/* ----------- END: GLOBAL FLOW OPTIMIZATION ------------ */


/*
- set propagation limits with respect to subformula size
- favour small formulas
- actually obsolete: optimizing large subformulae infeasible
- subformulae sizes limited by default
*/
static void
set_propagation_limits (unsigned int size)
{
  if (size <= 800)
    {
      ATPG_PROPAGATION_LIMIT = GLOBAL_FLOW_PROPAGATION_LIMIT = 1500000;
    }
  else if (size <= 1000)
    {
      ATPG_PROPAGATION_LIMIT = GLOBAL_FLOW_PROPAGATION_LIMIT = 1200000;
    }
  else if (size <= 1500)
    {
      ATPG_PROPAGATION_LIMIT = GLOBAL_FLOW_PROPAGATION_LIMIT = 800000;
    }
  else if (size <= 2000)
    {
      ATPG_PROPAGATION_LIMIT = GLOBAL_FLOW_PROPAGATION_LIMIT = 700000;
    }
  else if (size <= 3000)
    {
      ATPG_PROPAGATION_LIMIT = GLOBAL_FLOW_PROPAGATION_LIMIT = 600000;
    }
  else if (size <= 4000)
    {
      ATPG_PROPAGATION_LIMIT = GLOBAL_FLOW_PROPAGATION_LIMIT = 500000;
    }
  else if (size <= 6000)
    {
      ATPG_PROPAGATION_LIMIT = GLOBAL_FLOW_PROPAGATION_LIMIT = 300000;
    }
  else if (size <= 8000)
    {
      ATPG_PROPAGATION_LIMIT = GLOBAL_FLOW_PROPAGATION_LIMIT = 200000;
    }
  else if (size <= 10000)
    {
      ATPG_PROPAGATION_LIMIT = GLOBAL_FLOW_PROPAGATION_LIMIT = 100000;
    }
  else if (size <= 12000)
    {
      ATPG_PROPAGATION_LIMIT = GLOBAL_FLOW_PROPAGATION_LIMIT = 50000;
    }
  else
    {
      ATPG_PROPAGATION_LIMIT = GLOBAL_FLOW_PROPAGATION_LIMIT = 10000;
    }
}


/*
- allocate array of ATPGInfo
- allocate some more because global flow might require to create new nodes 
*/
static void
allocate_atpg_info_pointers (Nenofex * nenofex)
{
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;

  unsigned int size = 1;        /* one for lca of 'changed' */

  Node **ch;
  for (ch = nenofex->changed_subformula.children; *ch; ch++)
    size += (*ch)->size_subformula;

  if (nenofex->options.show_opt_info_specified)
    fprintf (stderr,
             "Calling optimization procedures for subgraph-size = %d\n",
             size);

  if (!nenofex->options.propagation_limit_specified)
    set_propagation_limits (size);
  else
    {
      ATPG_PROPAGATION_LIMIT =
        GLOBAL_FLOW_PROPAGATION_LIMIT = nenofex->options.propagation_limit;
    }

  atpg_rr->byte_size_atpg_info_array = (size + size / 2) * sizeof (ATPGInfo);

  assert (!atpg_rr->atpg_info_array);
  assert (atpg_rr->byte_size_atpg_info_array);

  atpg_rr->atpg_info_array =
    (ATPGInfo *) mem_malloc (atpg_rr->mm, atpg_rr->byte_size_atpg_info_array);
  assert (atpg_rr->atpg_info_array);

  atpg_rr->cur_atpg_info = atpg_rr->atpg_info_array;
  memset (atpg_rr->atpg_info_array, 0, atpg_rr->byte_size_atpg_info_array);
  atpg_rr->end_atpg_info =
    atpg_rr->atpg_info_array +
    atpg_rr->byte_size_atpg_info_array / sizeof (ATPGInfo);
}


/* 
- have collected nodes from fault paths of ALL redundant faults and nodes from 
    implication paths during global flow optimization
- must mark variables for update with respect to these paths nodes
- TODO: clarify failed assertions -> there seems to be no problem so far...
*/
static void
mark_affected_variables_by_fault_path_nodes (Nenofex * nenofex)
{
  assert (nenofex->changed_subformula.lca);
  assert (nenofex->graph_root);

  Stack *stack = nenofex->atpg_rr->fault_path_nodes;
  unsigned const int atpg_root_level = nenofex->changed_subformula.lca->level;
  ATPGInfo *atpg_info;

  while ((atpg_info = pop_stack (stack)))
    {
      FaultNode *fault_node = atpg_info->fault_node;

      if (fault_node->deleted)
        continue;

      /* search for lits at path node -> corresponding vars have to be marked for dec-score update */
      Node *node = fault_node->node;
      assert (!is_literal_node (node));

      Node *ch;
      for (ch = node->child_list.first;
           ch && is_literal_node (ch); ch = ch->level_link.next)
        {
          Var *var = ch->lit->var;
          if (var->exp_costs.lca_object.lca)    /* BUG FIX */
            {
              dec_score_update_mark (var);
              collect_variable_for_update (nenofex, var);
            }
        }                       /* end: for all literals */

      /* check whether path node occurs in lca-child-list of some var 
         -> corresponding variables have to be marked for inc-score update */
      Node *parent = node->parent;

      Stack *lca_child_list_occs = node->lca_child_list_occs;

      /* FAILED: possibly because LCAS updated AFTER FLOW/ATPG   
         --assert(!lca_child_list_occs || !count_stack(lca_child_list_occs) || 
         parent->var_lca_list.first); */

      if (lca_child_list_occs && parent && parent->level >= atpg_root_level)
        {
          /* NOT SURE */
          /*
             assert(!count_stack(lca_child_list_occs) || parent->var_lca_list.first);
             assert(!count_stack(lca_child_list_occs) || parent->var_lca_list.last);
           */

          void **v_var, **end;
          end = lca_child_list_occs->top;

          for (v_var = lca_child_list_occs->elems; v_var < end; v_var++)
            {
              Var *var = *v_var;
              inc_score_update_mark (var);
              collect_variable_for_update (nenofex, var);
            }                   /* end: for all variables where parent is lca */

        }                       /* end: node possibly has parent which is LCA of one or more variables */

    }                           /* end: while stack not empty */
}


/*
- called if redundancies have been found
- will traverse path from 'changed-subformula'-root up to graph root
*/
static void
mark_affected_variables_by_subformula_parents (Nenofex * nenofex)
{
  assert (nenofex->changed_subformula.lca);
  assert (nenofex->graph_root);

  mark_affected_scope_variables_for_cost_update (nenofex,
                                                 nenofex->changed_subformula.
                                                 lca);
}


/*
- only if graph has been modified
*/
static void
mark_affected_variables_for_update (Nenofex * nenofex, int redundancies_found)
{
  /* if graph modified -> need to mark affected variables for cost-update */
  if (count_stack (nenofex->atpg_rr->fault_path_nodes) &&
      nenofex->graph_root && nenofex->changed_subformula.lca)
    {
      mark_affected_variables_by_fault_path_nodes (nenofex);

      if (redundancies_found)
        mark_affected_variables_by_subformula_parents (nenofex);
    }
}


/*
- main function
- standard case: first perform global flow optimization, then redundancy removal
- depending whether graph was modified by these functions and 
    whether cutoff/abortion occurred, call functions again
- at end: 'changed-subformula' is reset to null (although this is not a requirement)
*/
int
simplify_by_global_flow_and_atpg_main (Nenofex * nenofex)
{
  LCAObject *changed_subformula = &(nenofex->changed_subformula);
  ATPGRedundancyRemover *atpg_rr = nenofex->atpg_rr;

  assert (!nenofex->options.no_global_flow_specified ||
          !nenofex->options.no_atpg_specified);
  assert (!GLOBAL_FLOW_LITERALS_BOTH_PHASES);

  assert (!is_literal_node (changed_subformula->lca));

  assert (atpg_rr->stats.red_fault_cnt == 0);
  assert (!nenofex->atpg_rr_called);
  assert (!nenofex->atpg_rr_abort);
  assert (!atpg_rr->prop_cutoff);
  assert (!atpg_rr->global_flow_prop_cutoff);
  assert (!atpg_rr->atpg_prop_cutoff);
  assert (!nenofex->atpg_rr_reset_changed_subformula);
  assert (!atpg_rr->global_atpg_test_node_mark);
  assert (!atpg_rr->global_flow_fwd_prop_cnt);
  assert (!atpg_rr->global_flow_bwd_prop_cnt);
  assert (!atpg_rr->atpg_fwd_prop_cnt);
  assert (!atpg_rr->atpg_bwd_prop_cnt);

  nenofex->atpg_rr_called = 1;

  allocate_atpg_info_pointers (nenofex);

  init_subformula_atpg_info (nenofex);

#if 0
  atpg_rr->collect_faults = collect_fault_nodes_in_post_order;
#else
  /* nenofex->atpg_rr->collect_faults = collect_fault_nodes_by_bfs; */
  atpg_rr->collect_faults = collect_fault_nodes_bottom_up;
#endif

  atpg_rr->collect_faults (nenofex);

  int implications_found, implications_found_all,
    redundancies_found, redundancies_found_all, called_again;
  called_again = implications_found = implications_found_all =
    redundancies_found = redundancies_found_all = 0;

GLOBAL_FLOW:

  if (!nenofex->options.no_global_flow_specified)
    {
      if (!atpg_rr->global_flow_prop_cutoff)
        implications_found = optimize_by_global_flow (nenofex, atpg_rr);
    }

  implications_found_all = implications_found_all || implications_found;

  if (nenofex->atpg_rr_abort)
    {
      goto SKIP_ATPG;
    }

  if (!nenofex->options.no_atpg_specified)
    {
      /* flag 'called_again' prevents additional calls of 'test_all_faults' if 
         global flow has not modified the graph before */
      if (!atpg_rr->atpg_prop_cutoff &&
          (!called_again || (called_again && implications_found)))
        redundancies_found = test_all_faults (nenofex, nenofex->atpg_rr);
    }

  redundancies_found_all = redundancies_found_all || redundancies_found;

  /* check whether global flow / redundancy removal should be carried out again */
  if (!nenofex->atpg_rr_abort)
    {
      assert (!nenofex->atpg_rr_reset_changed_subformula);

      called_again = 1;         /* will be ignored if no jump occurs */

      if (implications_found && redundancies_found)
        {
          if (!atpg_rr->global_flow_prop_cutoff)
            {
              implications_found = redundancies_found = 0;
              goto GLOBAL_FLOW;
            }
          else
            {
#if !KEEP_CHANGED_SUBFORMULA
              nenofex->atpg_rr_abort =
                nenofex->atpg_rr_reset_changed_subformula = 1;
#endif
            }
        }
      else if (implications_found && !redundancies_found)
        {
#if !KEEP_CHANGED_SUBFORMULA
          nenofex->atpg_rr_abort = nenofex->atpg_rr_reset_changed_subformula =
            1;
#endif
        }
      else if (!implications_found && redundancies_found)
        {
          if (!atpg_rr->global_flow_prop_cutoff)
            {
              implications_found = redundancies_found = 0;
              goto GLOBAL_FLOW;
            }
#if !KEEP_CHANGED_SUBFORMULA
          else
            nenofex->atpg_rr_abort =
              nenofex->atpg_rr_reset_changed_subformula = 1;
#endif
        }
      else
        {
#if !KEEP_CHANGED_SUBFORMULA
          nenofex->atpg_rr_abort = nenofex->atpg_rr_reset_changed_subformula =
            1;
#endif
        }
    }                           /* end: check for additional calls of optimization procedures */

SKIP_ATPG:

  mark_affected_variables_for_update (nenofex, redundancies_found_all);

  if (nenofex->atpg_rr_reset_changed_subformula)
    {
      assert (nenofex->atpg_rr_abort);
      nenofex->atpg_rr_reset_changed_subformula = 0;
      reset_changed_lca_object (nenofex);
    }
  else
    {
#if 0
      fprintf (stderr, "KEEPING CHANGED-SUBFORMULA\n");
#endif
    }

  reset_atpg_redundancy_remover (atpg_rr);
  nenofex->atpg_rr_called = 0;
  nenofex->atpg_rr_abort = 0;
  nenofex->atpg_rr_reset_changed_subformula = 0;

  return (implications_found_all || redundancies_found_all);
}
