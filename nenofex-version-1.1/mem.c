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
#include "mem.h"

struct MemManager
{
  size_t cur_bytes;
  size_t max_bytes;
};

static void
mem_check (MemManager *mm)
{
  if (mm->cur_bytes)
    {
      fprintf (stderr, "ERROR - mem: cur_bytes = %ld, but expected 0!\n",
               (long unsigned int) mm->cur_bytes);
      abort ();
    }
}

/* --------- START: API FUNCTIONS --------- */

MemManager * 
memmanager_create ()
{
  MemManager *mm = (MemManager *) malloc (sizeof (MemManager));
  memset (mm, 0, sizeof (MemManager));
  return mm;
}

void 
memmanager_delete (MemManager *mm)
{
  /* Leak check. */
  mem_check (mm);
  free (mm);
}

void *
mem_malloc (MemManager *mm, size_t bytes)
{
  void *result;

  result = malloc (bytes);

  if (!result)
    {
      fprintf (stderr, "ERROR - mem: malloc failed!\n");
      abort ();
    }
  mm->cur_bytes += bytes;

  if (mm->cur_bytes > mm->max_bytes)
    mm->max_bytes = mm->cur_bytes;

  return result;
}


void *
mem_realloc (MemManager *mm, void *ptr, size_t old_bytes, size_t new_bytes)
{
  void *result;

  if (!ptr)
    {
      assert (!old_bytes);
      return mem_malloc (mm, new_bytes);
    }

  if (!new_bytes)
    {
      mem_free (mm, ptr, old_bytes);
      return 0;
    }

  assert (mm->cur_bytes >= old_bytes);
  mm->cur_bytes -= old_bytes;
  mm->cur_bytes += new_bytes;
  result = realloc (ptr, new_bytes);

  if (!result)
    {
      fprintf (stderr, "ERROR - mem: realloc failed!\n");
      abort ();
    }

  if (mm->cur_bytes > mm->max_bytes)
    mm->max_bytes = mm->cur_bytes;

  return result;
}


void
mem_free (MemManager *mm, void *ptr, size_t bytes)
{
  if (!ptr)
    {
      fprintf (stderr, "ERROR - mem: free at null pointer!\n");
      abort ();
    }

  assert (mm->cur_bytes >= bytes);
  free (ptr);
  mm->cur_bytes -= bytes;
}


size_t
get_cur_bytes (MemManager *mm)
{
  return mm->cur_bytes;
}


size_t
get_max_bytes (MemManager *mm)
{
  return mm->max_bytes;
}

/* --------- END: API FUNCTIONS --------- */

