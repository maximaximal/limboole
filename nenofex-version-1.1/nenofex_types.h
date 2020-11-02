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

#ifndef _NENOFEX_TYPES_H_
#define _NENOFEX_TYPES_H_

#include <picosat.h>
#include "mem.h"
#include "stack.h"
#include "queue.h"
#include "nenofex.h"

typedef struct Node Node;
typedef struct Var Var;
typedef struct Lit Lit;
typedef struct LevelLink LevelLink;
typedef struct OccurrenceLink OccurrenceLink;
typedef struct ChildList ChildList;
typedef struct OccurrenceList OccurrenceList;
typedef struct Scope Scope;
typedef struct LCAObject LCAObject;
typedef struct SameLCALink SameLCALink;
typedef struct VarLCAList VarLCAList;

/* 
- types used in ATPG-redundancy-removal and global flow optimization
*/
typedef struct ATPGRedundancyRemover ATPGRedundancyRemover;
typedef struct ATPGInfo ATPGInfo;
typedef struct FaultNode FaultNode;

/*
- represents LCA of all occurrences of a variable
- 'children' is a collection of exactly those children of lca(var) which 
  contain at least one occurrence of 'var' in their subformula 
*/
struct LCAObject
{
  Node *lca;
  unsigned int num_children;
  unsigned int size_children;
  Node **children;
  Node **top_p;
};

enum NodeType
{
  NODE_TYPE_LITERAL = 1,
  NODE_TYPE_OR = 2,
  NODE_TYPE_AND = 3
};

typedef enum NodeType NodeType;

#define is_literal_node(node) ((node)->type == NODE_TYPE_LITERAL)
#define is_operator_node(node) ((node)->type != NODE_TYPE_LITERAL)
#define is_or_node(node) ((node)->type == NODE_TYPE_OR)
#define is_and_node(node) ((node)->type == NODE_TYPE_AND)

/*
- all children of a node are linked
*/
struct LevelLink
{
  Node *next;
  Node *prev;
};

/*
- all occurrences of a variable are linked
*/
struct OccurrenceLink
{
  Node *next;
  Node *prev;
};

/*
- operator nodes only: anchor for list of children
- INVARIANT: literals are stored first in child-list
*/
struct ChildList
{
  Node *first;
  Node *last;
};

/*
- anchor for occurrence list
*/
struct OccurrenceList
{
  Node *first;
  Node *last;
};

/*
- two Lit-objects (pos./neg.) embedded in each variable
- 'occ_cnt' stores length of occurrence list
*/
struct Lit
{
  Var *var;
  unsigned int negated:1;
  OccurrenceList occ_list;
  unsigned int occ_cnt;
};

enum VarAssignment
{
  VAR_ASSIGNMENT_UNDEFINED = 0,
  VAR_ASSIGNMENT_FALSE = 1,
  VAR_ASSIGNMENT_TRUE = 2
};

typedef enum VarAssignment VarAssignment;

#define var_assigned(var) ((var)->assignment)
#define var_assigned_true(var) ((var)->assignment == VAR_ASSIGNMENT_TRUE)
#define var_assigned_false(var) ((var)->assignment == VAR_ASSIGNMENT_FALSE)

#define var_unassign(var) ((var)->assignment = VAR_ASSIGNMENT_UNDEFINED)
#define var_assign_true(var) ((var)->assignment = VAR_ASSIGNMENT_TRUE)
#define var_assign_false(var) ((var)->assignment = VAR_ASSIGNMENT_FALSE)

/* 
- variables for which the lca is one and the same node 'n' 
  occur in var-lca-list rooted at node 'n'
*/
struct SameLCALink
{
  Var *prev;
  Var *next;
};

/* 
- a node 'n' has a list of variables for which it is the lca (if any)
*/
struct VarLCAList
{
  Var *first;
  Var *last;
};

struct Var
{
  int id;
  Lit lits[2];
  Scope *scope;
  unsigned int eliminated:1;
  unsigned int simp_mark:2;

  unsigned int lca_update_mark:1;
  unsigned int inc_score_update_mark:1;
  unsigned int dec_score_update_mark:1;

  unsigned int atpg_mark:1;

  unsigned int collected_as_unate:1;
  unsigned int collected_for_update:1;
  unsigned int collected_as_depending:1;

  /* parallel collection to 'lca_children: position of 'var' in 'lca_child_list_occs' */
  Stack *pos_in_lca_child_list_occs;

  struct
  {                             /* expansion costs */
    LCAObject lca_object;
    unsigned int inc_score;
    unsigned int dec_score;
    int score;                  /* == inc_score - dec_score */
  } exp_costs;

  VarAssignment assignment;

  /* collecting occurrences of 'var' in a given subformula separately; 
     -> used in global flow / redundancy removal 
   */
  Stack *subformula_pos_occs;
  Stack *subformula_neg_occs;

  SameLCALink same_lca_link;

  int priority_pos;

  Var *copied;                  /* during universal expansions: pointer to copied variable */
};

struct Nenofex
{
  MemManager *mm;
  PicoSAT *picosat;
  /* Keeping track of first two added clauses for special cases in
     graph simplification. */
  Node *first_added_clause;
  Node *second_added_clause;
  unsigned int preamble_set_up:1;
  /* Must not call 'solve' function multiple time. */
  unsigned int solve_called:1;
  unsigned int post_formula_addition_simplified:1;
  unsigned int empty_clause_added:1;
  unsigned int num_orig_vars;
  unsigned int num_orig_clauses;
  unsigned int num_added_clauses;
  unsigned int num_cur_remaining_scope_vars;
  unsigned int next_free_node_id;
  Var **vars;
  Node *graph_root;
  NenofexResult result;
  Stack *scopes;

  /* Graph size at start of solving process. */
  unsigned int init_graph_size;

  Var *cur_expanded_var;
  Node *existential_split_or;   /* for post-expansion flattening only */

  Scope **cur_scope;
  Scope **next_scope;

  /* flag: if 'false' then do NEITHER update scores of universal variables from
     non-innermost scope NOR expand such variables 
   */
  unsigned int consider_univ_exp;

  Stack *unates;

  Stack *vars_marked_for_update;
  Stack *depending_vars;

  unsigned int atpg_rr_called;
  unsigned int atpg_rr_abort;
  unsigned int atpg_rr_reset_changed_subformula;

  unsigned int distributivity_deleting_redundancies;

  ATPGRedundancyRemover *atpg_rr;

  LCAObject changed_subformula;

  int tseitin_next_id;
  int tseitin_first_op_node_id;

  int sat_solver_tautology_mode;

  int is_existential;
  int is_universal;

  int cur_expansions;
  int first_successful_opt;
  int performed_optimizations;
  int successful_optimizations;
  int cnt_post_expansion_flattenings;

  struct
  {
    int total_deleted_nodes;
    int deleted_nodes_by_global_flow_redundancy;

    int num_non_inc_expansions;
    int num_non_inc_expansions_in_scores;

    int num_exp_case_E_OR_ALL;
    int num_exp_case_E_OR_SUBSET;
    int num_exp_case_E_AND_ALL;
    int num_exp_case_E_AND_SUBSET;
    int num_exp_case_A_OR_ALL;
    int num_exp_case_A_OR_SUBSET;
    int num_exp_case_A_AND_ALL;
    int num_exp_case_A_AND_SUBSET;

    int sum_lca_marked;
    int sum_inc_marked;
    int sum_dec_marked;
    int sum_remaining;
    double sum_ratio_lca_marked_in_scope_vars;
    double sum_ratio_inc_marked_in_scope_vars;
    double sum_ratio_dec_marked_in_scope_vars;

    int num_units;
    int num_unates;

    int num_total_created_nodes;
    unsigned int max_tree_size;

    int num_total_lca_parent_visits;
    int num_total_lca_algo_calls;

    unsigned long long sat_solver_decisions;
  } stats;

  struct
  {
    char *input_filename;
    int num_expansions_specified;
    int num_expansions;

    int size_cutoff_relative_specified;
    int size_cutoff_absolute_specified;
    float size_cutoff;

    int cost_cutoff_specified;
    int cost_cutoff;

    int propagation_limit_specified;
    int propagation_limit;

    int opt_subgraph_limit_specified;
    int opt_subgraph_limit;

    unsigned int univ_trigger_abs:1;
    int univ_trigger;
    int univ_trigger_delta;

    int no_optimizations_specified;
    int no_atpg_specified;
    int no_global_flow_specified;

    int post_expansion_flattening_specified;

    int verbose_sat_solving_specified;
    int full_expansion_specified;
    int dump_cnf_specified;
    int no_sat_solving_specified;
    int show_progress_specified;
    int print_short_answer_specified;
    int show_graph_size_specified;
    int show_opt_info;
    int show_opt_info_specified;
    int print_assignment_specified;
    int cnf_generator_tseitin_specified;
    int cnf_generator_tseitin_revised_specified;
    /* Decision limit for SAT solver called in the end. */
    int sat_solver_dec_limit;
    /* Limit (factor) on the absolute size of the graph, which is different
       from 'size_cutoff' or 'cost_cutoff' limits, which allow to bound the
       growth of the graph after expansions. Cut off will occur if the current
       graph size after expansions is larger than 'initial_graph *
       abs_graph_size_cutoff', where 'initial_graph' is the size at the beginning
       of the expansion phase. */
    float abs_graph_size_cutoff;
  } options;

  double start_time;
  double expansion_phase_end_time;
  double end_time;
};


/*
- some fields are used for literal nodes / operator nodes only
*/
struct Node
{
  int id;
  unsigned int level;
  NodeType type;
  Node *parent;
  LevelLink level_link;

  ChildList child_list;         /* for operators only */
  OccurrenceLink occ_link;      /* for literals only */

  Lit *lit;                     /* for literals only */
  unsigned int num_children;    /* actually not needed for literals */

  unsigned int size_subformula; /* node count in subformula rooted at 'node' */
#ifndef NDEBUG
  unsigned int test_size_subformula;    /* size is recalculated during testing */
#endif

  ATPGInfo *atpg_info;

  VarLCAList var_lca_list;      /* CONJECTURE: not for lits if unates fully elim. */

  /* position of a node in lca-child-list of current 'changed-subformula' 
     -> allows fast removal of nodes from this list 
   */
  Node **changed_ch_list_pos;

  /* stores pointers to vars where 'node' occurs in LCA child-list of vars */
  Stack *lca_child_list_occs;
  /* parallel collection to 'occs': position of 'node' in LCA-children of variable */
  Stack *pos_in_lca_children;

  /* multi-purpose marks */
  unsigned int mark1:1;
  unsigned int mark2:1;
/* #ifndef NDEBUG */
  unsigned int mark3:1;
/* #endif */
};

#define is_existential_scope(scope) ((scope)->type == SCOPE_TYPE_EXISTENTIAL)
#define is_universal_scope(scope) ((scope)->type == SCOPE_TYPE_UNIVERSAL)

struct Scope
{
  unsigned int nesting;
  ScopeType type;
  Stack *vars;
  Stack *priority_heap;
  unsigned int is_empty:1;
  int remaining_var_cnt;
};

void remove_and_free_subformula (Nenofex * nenofex, Node * root);

void unlink_node (Nenofex * nenofex, Node * node);

void update_size_subformula (Nenofex * nenofex, Node * root, int delta);

void merge_parent (Nenofex * nenofex, Node * parent);

Node *or_node (Nenofex * nenofex);

Node *and_node (Nenofex * nenofex);

void
add_node_to_child_list (Nenofex * nenofex, Node * parent, Node * new_child);

void update_level (Nenofex * nenofex, Node * root);

void simplify_one_level (Nenofex * nenofex, Node * root);

void
mark_affected_scope_variables_for_cost_update (Nenofex * nenofex,
                                               Node * exp_root);

void reset_changed_lca_object (Nenofex * nenofex);

void add_changed_lca_child (Nenofex * nenofex, Node * node);


#define variable_has_occs(var) ((var)->lits[0].occ_list.first || \
				(var)->lits[1].occ_list.first)

#define lca_update_marked(var) ((var)->lca_update_mark)
#define lca_update_mark(var) ((var)->lca_update_mark = 1)
#define lca_update_unmark(var) ((var)->lca_update_mark = 0)

#define dec_score_update_marked(var) ((var)->dec_score_update_mark)
#define dec_score_update_mark(var) ((var)->dec_score_update_mark = 1)
#define dec_score_update_unmark(var) ((var)->dec_score_update_mark = 0)

#define inc_score_update_marked(var) ((var)->inc_score_update_mark)
#define inc_score_update_mark(var) ((var)->inc_score_update_mark = 1)
#define inc_score_update_unmark(var) ((var)->inc_score_update_mark = 0)

void collect_variable_for_update (Nenofex * nenofex, Var * var);

/* REDUNDANCY REMOVAL / GLOBAL FLOW OPTIMIZATION */

/*
- during redundancy removal: skip non-promising nodes during testing 
- idea: test only those nodes again where the set of variables has changed
   after a modification of the graph
- this could save unnecessary propagations
- NOT YET COMPLETED NOR VERIFIED -> handle with care
*/
#define RESTRICT_ATPG_FAULT_NODE_SET 0  /* disabling recommended */
#define node_taken_from_fault_queue(node) ((node)->atpg_info->queue_mark != \
			     nenofex->atpg_rr->global_atpg_test_node_mark)

int simplify_by_global_flow_and_atpg_main (Nenofex * nenofex);

ATPGRedundancyRemover *create_atpg_redundancy_remover (MemManager *mm);

void free_atpg_redundancy_remover (ATPGRedundancyRemover * atpg_rr);

void mark_fault_node_as_deleted (FaultNode * fault_node);

void collect_assigned_node (ATPGRedundancyRemover * atpg_rr, Node * node);

#define node_assigned(node) ((node)->atpg_info->assignment)
#define node_assigned_true(node) \
  ((node)->atpg_info->assignment == ATPG_ASSIGNMENT_TRUE)
#define node_assigned_false(node) \
  ((node)->atpg_info->assignment == ATPG_ASSIGNMENT_FALSE)

#define node_unassign(node) \
  ((node)->atpg_info->assignment = ATPG_ASSIGNMENT_UNDEFINED)
#define node_assign_true(node) \
  ((node)->atpg_info->assignment = ATPG_ASSIGNMENT_TRUE)
#define node_assign_false(node) \
  ((node)->atpg_info->assignment = ATPG_ASSIGNMENT_FALSE)

enum ATPGFaultType
{
  ATPG_FAULT_TYPE_STUCK_AT_0 = 0,
  ATPG_FAULT_TYPE_STUCK_AT_1 = 1
};

typedef enum ATPGFaultType ATPGFaultType;

/*
- a 'FaultNode' 'encapsulates' a real graph node
- mark 'deleted' will be set whenever a not has been deleted
- redundancy removal / global flow optimization take a 'FaultNode' from 
   a collection and discard it if it has been deleted
*/
struct FaultNode
{
  Node *node;
  unsigned int deleted:1;
  unsigned int skip:1;
};

enum ATPGAssignment
{
  ATPG_ASSIGNMENT_UNDEFINED = 0,
  ATPG_ASSIGNMENT_FALSE = 1,
  ATPG_ASSIGNMENT_TRUE = 2
};

typedef enum ATPGAssignment ATPGAssignment;

struct ATPGInfo
{
  FaultNode *fault_node;
  ATPGAssignment assignment;

  Node *watcher;
  unsigned int unassigned_ch_cnt;
  Stack *atpg_ch;
  void **watcher_pos;
  unsigned int clean_up_watcher_list:1;

  unsigned int justified:1;
  unsigned int path_mark:1;
  unsigned int collected:1;
  unsigned int fault_path_node_collected:1;
  unsigned int cur_atpg_test_node_mark:1;
  unsigned int next_atpg_test_node_mark:1;
  unsigned int queue_mark:1;
};

struct ATPGRedundancyRemover
{
  MemManager *mm;
  Queue *fault_queue;
  Queue *propagation_queue;
  unsigned int conflict;
  unsigned int prop_cutoff;

  unsigned int global_flow_prop_cutoff;
  unsigned int atpg_prop_cutoff;

  unsigned int restricted_clean_up;

  unsigned int global_flow_fwd_prop_cnt;
  unsigned int global_flow_bwd_prop_cnt;

  unsigned int atpg_fwd_prop_cnt;
  unsigned int atpg_bwd_prop_cnt;
  unsigned int atpg_next_global_atpg_test_node_mark:1;

  Stack *touched_nodes;
  Stack *propagated_vars;
  Stack *bwd_prop_stack;
  Stack *fault_path_nodes;
  void (*collect_faults) (Nenofex *);
  struct
  {
    unsigned int fwd_prop_cnt;
    unsigned int bwd_prop_cnt;
    unsigned int fault_cnt;
    unsigned int red_fault_cnt;
    unsigned int derived_implications_cnt;
  } stats;

  ATPGInfo *atpg_info_array;
  size_t byte_size_atpg_info_array;
  ATPGInfo *cur_atpg_info;
  ATPGInfo *end_atpg_info;
  Stack *subformula_vars;

  unsigned int global_flow_optimizing;

  unsigned int global_atpg_test_node_mark:1;
};

#endif /* _NENOFEX_TYPES_H_ */
