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

#include "stack.h"
#include <assert.h>
#include <stdlib.h>


Stack *
create_stack (MemManager *mm, unsigned int size)
{
  Stack *stack = (Stack *) mem_malloc (mm, sizeof (Stack));
  assert (stack);
  size = size ? size : 1;
  stack->elems = (void **) mem_malloc (mm, size * sizeof (void *));
  assert (stack->elems);
  stack->top = stack->elems;
  stack->end = stack->elems + size;
  return stack;
}


void
delete_stack (MemManager *mm, Stack * stack)
{
  mem_free (mm, stack->elems, size_stack (stack) * sizeof (void *));
  mem_free (mm, stack, sizeof (Stack));
}


unsigned int
count_stack (Stack * stack)
{
  return stack->top - stack->elems;
}


unsigned int
size_stack (Stack * stack)
{
  return stack->end - stack->elems;
}


static void
enlarge_stack (MemManager *mm, Stack * stack)
{
  assert (count_stack (stack) == size_stack (stack));
  assert (size_stack (stack));
  assert (stack->top == stack->end);
  assert (stack->top == stack->elems + count_stack (stack));

  unsigned int new_size = size_stack (stack) * 2;
  unsigned int old_count = count_stack (stack);
  stack->elems =
    mem_realloc (mm, stack->elems, size_stack (stack) * sizeof (void *),
                 new_size * sizeof (void *));
  stack->top = stack->elems + old_count;
  stack->end = stack->elems + new_size;
}


void
push_stack (MemManager *mm, Stack * stack, void *elem)
{
  assert (stack->top < stack->end);

  *stack->top = elem;
  stack->top++;
  if (stack->top == stack->end)
    enlarge_stack (mm, stack);
}


void *
pop_stack (Stack * stack)
{
  assert (stack->top >= stack->elems);
  assert (stack->top <= stack->end);

  if (stack->top == stack->elems)
    return 0;
  else
    {
      stack->top--;
      return *stack->top;
    }
}


void
reset_stack (Stack * stack)
{
  stack->top = stack->elems;
}
