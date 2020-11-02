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

#ifndef _MEM_H_
#define _MEM_H_

#include <stddef.h>

typedef struct MemManager MemManager;

MemManager * memmanager_create ();

void memmanager_delete (MemManager *);

void *mem_malloc (MemManager *, size_t);

void mem_free (MemManager *, void *, size_t);

void *mem_realloc (MemManager *, void *, size_t, size_t);

size_t get_cur_bytes (MemManager *);

size_t get_max_bytes (MemManager *);

#endif /* _MEM_H_ */
