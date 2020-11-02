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

#ifndef _STACK_H_
#define _STACK_H_

#include "mem.h"

typedef struct Stack Stack;

struct Stack
{
  void **elems;
  void **top;
  void **end;
};

Stack *create_stack (MemManager *, unsigned int size);

void delete_stack (MemManager *, Stack * stack);

void push_stack (MemManager *, Stack * stack, void *elem);

void *pop_stack (Stack * stack);

unsigned int count_stack (Stack * stack);

unsigned int size_stack (Stack * stack);

void reset_stack (Stack * stack);

#endif /* _STACK_H_ */
