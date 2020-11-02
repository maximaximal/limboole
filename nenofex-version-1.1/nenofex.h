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

#ifndef _NENOFEX_H_
#define _NENOFEX_H_

typedef struct Nenofex Nenofex;

enum NenofexResult
{
  NENOFEX_RESULT_UNKNOWN = 0,
  NENOFEX_RESULT_SAT = 10,
  NENOFEX_RESULT_UNSAT = 20
};

enum ScopeType
{
  SCOPE_TYPE_EXISTENTIAL = 1,
  SCOPE_TYPE_UNIVERSAL = 2
};

typedef enum NenofexResult NenofexResult;
typedef enum ScopeType ScopeType;

/****************************************************************************
IMPORTANT NOTE: please see also the example program
'test/test-formula-by-lib-and-parser.c' which demonstrate the use of the API.
****************************************************************************/

/* Create solver object and return pointer to it. */
Nenofex * nenofex_create ();

/* Delete solver object and release all memory. */
void nenofex_delete (Nenofex *);

/* Import formula given by file. */
NenofexResult nenofex_parse (Nenofex *, FILE *);

/* Declare number of variables and clauses that will be added. This function
   must be called before any call of 'nenofex_add_orig_scope' and
   'nenofex_add_orig_clause'. */
void nenofex_set_up_preamble (Nenofex *, unsigned int, unsigned int);

/* Add scope (block) of variables, starting with leftmost scope of the
   quantifier prefix. */
void nenofex_add_orig_scope (Nenofex *, void **, unsigned int, ScopeType);

/* Add clause. */
void nenofex_add_orig_clause (Nenofex *, void **, unsigned int);

/* Configure solver object by command line parameters. See also the output of
   './nenofex -h'. */
void nenofex_configure (Nenofex *, char *);

/* Solve added formula. 
   IMPORTANT NOTE: this function can be called at most once, i.e., the library
   cannot be used in an incremental way. In particular, it is not possible to
   modify the formula after a call of 'nenofex_solve' and then call
   'nenofex_solve' again. */
NenofexResult nenofex_solve (Nenofex *);

#endif /* _NENOFEX_H_ */
