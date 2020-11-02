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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"

Queue *
create_queue (MemManager *mm, unsigned int size)
{
  size_t bytes = sizeof (Queue);
  Queue *result = (Queue *) mem_malloc (mm, bytes);
  assert (result);

  size = size ? size : 1;
  bytes = size * sizeof (void *);
  result->elems = (void **) mem_malloc (mm, bytes);
  assert (result->elems);
  memset (result->elems, 0, bytes);

  result->end = result->elems + size;
  result->first = result->last = result->elems;

  return result;
}


void
delete_queue (MemManager *mm, Queue * queue)
{
  mem_free (mm, queue->elems, size_queue (queue) * sizeof (void *));
  mem_free (mm, queue, sizeof (Queue));
}


unsigned int
size_queue (Queue * queue)
{
  return queue->end - queue->elems;
}


unsigned int
count_queue (Queue * queue)
{
  if (queue->first <= queue->last)
    return queue->last - queue->first;
  else
    return queue->end - queue->first + queue->last - queue->elems;
}


static void
enlarge_queue (MemManager *mm, Queue * queue)
{
  assert (queue->last == queue->first || queue->first == queue->elems);

  unsigned int new_size = size_queue (queue) * 2;
  assert (new_size);
  size_t bytes = new_size * sizeof (void *);
  void **new_elems = (void **) mem_malloc (mm, bytes);
  assert (new_elems);
  void **new_last = new_elems;

  do
    {
      *new_last++ = *queue->first++;
      if (queue->first == queue->end && queue->last != queue->end)
        queue->first = queue->elems;
    }
  while (queue->first != queue->last);
  mem_free (mm, queue->elems, size_queue (queue) * sizeof (void *));

  queue->elems = queue->first = new_elems;
  queue->end = queue->elems + new_size;
  queue->last = new_last;

  assert (count_queue (queue) != 0 || queue->first == queue->last);
  assert (count_queue (queue) == 0 || queue->first != queue->last);
  assert (queue->first < queue->end && queue->first >= queue->elems);
  assert (queue->last < queue->end && queue->last >= queue->elems);
}


void
enqueue (MemManager *mm, Queue * queue, void *elem)
{
  assert (count_queue (queue) != 0 || queue->first == queue->last);
  assert (count_queue (queue) == 0 || queue->first != queue->last);
  assert (queue->first < queue->end && queue->first >= queue->elems);
  assert (queue->last < queue->end && queue->last >= queue->elems);

  *queue->last++ = elem;

  if (queue->last == queue->first)
    enlarge_queue (mm, queue);
  else if (queue->last == queue->end)
    {
      if (queue->first == queue->elems)
        enlarge_queue (mm, queue);
      else
        queue->last = queue->elems;
    }

  assert (count_queue (queue) != 0 || queue->first == queue->last);
  assert (count_queue (queue) == 0 || queue->first != queue->last);
  assert (queue->first < queue->end && queue->first >= queue->elems);
  assert (queue->last < queue->end && queue->last >= queue->elems);
}


void *
dequeue (Queue * queue)
{
  assert (count_queue (queue) != 0 || queue->first == queue->last);
  assert (count_queue (queue) == 0 || queue->first != queue->last);
  assert (queue->first < queue->end && queue->first >= queue->elems);
  assert (queue->last < queue->end && queue->last >= queue->elems);

  if (queue->first == queue->last)
    return 0;
  else
    {
      void *result = *queue->first++;

      if (queue->first == queue->end)
        queue->first = queue->elems;

      return result;
    }

  assert (count_queue (queue) != 0 || queue->first == queue->last);
  assert (count_queue (queue) == 0 || queue->first != queue->last);
  assert (queue->first < queue->end && queue->first >= queue->elems);
  assert (queue->last < queue->end && queue->last >= queue->elems);
}


void
reset_queue (Queue * queue)
{
  queue->first = queue->last = queue->elems;
}
