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

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include "mem.h"
#include "nenofex.h"

#define VERSION						\
  "Nenofex 1.1\n"					\
  "Copyright 2008, 2012, 2017 Florian Lonsing.\n"	\
"This is free software; see COPYING for copying conditions.\n"		\
"There is NO WARRANTY, to the extent permitted by law.\n"


#define USAGE \
"usage: nenofex [<option> ...] [ <in-file> ]\n"\
"\n"\
"where <in-file> is a file in (Q)DIMACS format and <option> is any of the following:\n\n\n"\
"Printing Information:\n"\
"---------------------\n\n"\
"  -h | -help			print usage information\n"\
"  --version                     print version\n"\
"  -v				verbose output (default: only QDIMACS output)\n"\
"  --show-progress		print short summary after each expansion step\n"\
"  --show-graph-size		print graph size after each expansion step\n\n\n"\
"SAT Solving:\n"\
"------------\n\n"\
"  --no-sat-solving		never call internal SAT-solver even if formula is\n"\
"				  purely existential/universal\n"\
"  --verbose-sat-solving 	enable verbosity mode during SAT-solving\n"\
"  --dump-cnf			print generated CNF (if any) to 'stdout'\n"\
"	       			  (may be combined with '--no-sat-solving')\n"\
"  --sat-solver-dec-limit=<val> non-zero positive SAT solver decision limit (default: no limit)\n"\
"  --cnf-generator=<cnf-gen>	set NNF-to-CNF generator where <cnf-gen> is either \n"\
"				  'tseitin' or 'tseitin_revised' (default)\n\n\n"\
"Expansion:\n"\
"----------\n\n"\
"  --full-expansion		do not stop expanding variables even if formula is\n"\
"				  purely existential/universal\n"\
"  -n=<val>			expand at most <val> variables where \n"\
"				  <val> is a positive integer\n"\
"  --size-cutoff=<val>		stop expanding if graph size after an expansion\n"\
"				  step has grown faster than specified where\n" \
"				  <val> is either\n"\
"				    - a floating point value between -1.0 and 1.0\n"\
"		       		      and cutoff occurs if 'new_size > old_size * (1 + <val>)'\n"\
"				  or\n"\
"				    - an integer and cutoff occurs if 'new_size > (old_size + <val>)'\n"\
"  --cost-cutoff=<val>		stop expanding if predicted minimal expansion\n"\
"				  costs exceed <val> where val is an integer\n\n"\
"  --abs-graph-size-cutoff=<val> stop expanding if overall graph size exceeds initial\n"\
"                                  graph size by factor <val>\n"\
"  --univ-trigger=<val>	       	enable non-inner universal expansion if tree has grown\n"\
"				  faster than <val> (default: 10) nodes in last exist. expansion\n"\
"  --univ-delta=<val>	       	increase universal trigger by <val> after \n"\
"				  universal expansion (default: 10)\n"\
"  --post-expansion-flattening	affects the following situation only: \n"\
"				  existential variable 'x' has AND-LCA and either\n"\
"				  has exactly one positive occurrence and <n> negative \n"\
"				  ones or vice versa, or variable 'x' has exactly two\n"\
"				  positive and two negative occurrences -> flatten subgraph\n"\
"				  rooted at 'split-OR' by multiplying out clauses\n\n\n" \
"Optimizations:\n"\
"--------------\n\n"\
"  --show-opt-info		print short info after calls of optimizations\n"\
"  --opt-subgraph-limit=<val>	impose size limit <val> (default: 500) on\n"\
"				  subgraph where optimizations are carried out\n"\
"  --no-optimizations		do not optimize by global flow and redundancy removal\n"\
"  --no-atpg			do not optimize by ATPG redundancy removal\n"\
"				  (overruled by '--no-optimizations')\n"\
"  --no-global-flow		do not optimize by global flow\n"\
"				  (overruled by '--no-optimizations')\n"\
"  --propagation-limit=<val>	set hard propagation limit in optimizations (see below)"\
"\n\n\n"\
"REMARKS:\n\n"\
"  - For calling the solver on a CNF, you should specify '--full-expansion'\n\n"\
"  - If '-n=<val>' is specified the solver will - if possible - forward a CNF\n"\
"      to the internal SAT solver unless '--no-sat-solving' is specified\n\n"\
"  - Options '--size-cutoff=<val>', '--cost-cutoff=<val>' and '-n <val>' may be combined\n\n"\
"  - Option '--propagation-limit=<val>' will set a limit for global flow optimization\n"\
"      and redundancy removal separately, i.e. both optimizations may perform <val>\n"\
"      propagations. If this option is omitted (default) then a built-in limit will\n"\
"      be set depending on the size of the formula subject to optimization\n\n"


static char *input_filename = 0;

static int
is_unsigned_string (char *str)
{
  int result;
  char *p = str;

  result = p && *p;
  while (p && *p && (result = isdigit (*p++)))
    ;

  return result;
}

static void
sigalarm_handler (int sig)
{
  fprintf (stderr, "\n\n Time limit exceeded!\n\n");
  signal (sig, SIG_DFL);
  raise (sig);
}

static int
parse_cmd_line_options (Nenofex *nenofex, int argc, char **argv)
{
  int done = 0;
  int opt_cnt;
  for (opt_cnt = 1; opt_cnt < argc; opt_cnt++)
    {
      char *opt_str = argv[opt_cnt];

      /* Parse options of this program. */
      if (!strcmp (opt_str, "-h") || !strcmp (opt_str, "--help"))
        {
          done = 1;
          fprintf (stdout, USAGE);
        }
      else if (!strcmp (opt_str, "--version"))
        {
          done = 1;
          fprintf (stdout, VERSION);
        }
      /* Parse Nenofex options. */
      else if (!strncmp (opt_str, "-", 1) || !strncmp (opt_str, "--", 2))
        {
          nenofex_configure (nenofex, opt_str);
        }
      else if (is_unsigned_string (opt_str))
	{
	  unsigned int max_time = atoi (opt_str);
	  if (max_time == 0)
	    {
	      fprintf (stderr, "Expecting value > 0 for max-time limit\n\n");
	      exit (1);
	    }
	  alarm (max_time);
          signal (SIGALRM, sigalarm_handler);
	  fprintf (stderr, "Time limit set to %u seconds\n", max_time);
	}
      else if (!input_filename)
	{
	  input_filename = opt_str;
	}
      else
        {
          fprintf (stderr, "Unknown option: %s\n", opt_str);
          exit (1);
        }
    }
  return done;
}

int
main (int argc, char **argv)
{
  int done = 0;
  int result = NENOFEX_RESULT_UNKNOWN;

  Nenofex *nenofex = nenofex_create ();

  done = parse_cmd_line_options (nenofex, argc, argv);

  if (done)
    goto FREE_GRAPH;

  DIR *dir = 0;
  FILE *input_file = 0;

  if (!input_filename)
    input_file = stdin;
  else
    {
      if ((dir = opendir (input_filename)) != NULL)
        {
          fprintf (stderr, "'%s' is a directory!\n\n",
                   input_filename);
          closedir (dir);
          exit (1);
        }
      input_file = fopen (input_filename, "r");
    }

  if (!done && input_file)
    result = nenofex_parse (nenofex, input_file);
  else if (!done)
    {
      fprintf (stderr, "Could not open file '%s'!\n\n",
               input_filename);
      exit (1);
    }

  if (input_filename)
    fclose (input_file);

#ifndef NDEBUG
#if ASSERT_GRAPH_AFTER_PARSING
  assert_all_child_occ_lists_integrity (nenofex);
  assert_all_occ_lists_integrity (nenofex);
  assert_all_subformula_sizes (nenofex);
  assert_all_one_level_simplified (nenofex);
#endif
#endif

  if (result == NENOFEX_RESULT_UNKNOWN)
    {
      result = nenofex_solve (nenofex);
    }

  fprintf (stderr, "%s\n",
           result == NENOFEX_RESULT_SAT ?
           "TRUE" : (result ==
                     NENOFEX_RESULT_UNSAT ? "FALSE" : "UNKNOWN"));

FREE_GRAPH:
  nenofex_delete (nenofex);

  return result;
}
