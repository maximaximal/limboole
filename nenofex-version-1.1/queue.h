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

#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "mem.h"

typedef struct Queue Queue;

struct Queue
{
  void **elems;
  void **end;
  void **first;
  void **last;
};


Queue *create_queue (MemManager *, unsigned int);

void delete_queue (MemManager *, Queue *);

void enqueue (MemManager *, Queue *, void *);

void *dequeue (Queue *);

unsigned int size_queue (Queue *);

unsigned int count_queue (Queue *);

void reset_queue (Queue *);

#endif /* _QUEUE_H_ */
