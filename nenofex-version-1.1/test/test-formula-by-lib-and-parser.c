#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../nenofex.h"
#include "../stack.h"

#define STACK_SIZE 1024

static void 
import_file_by_lib (Nenofex *nenofex, FILE *input_file)
{
  unsigned int clause_cnt = 0;
  int closed = 1;
  int preamble_found = 0;
  //  NenofexResult result = NENOFEX_RESULT_UNKNOWN;
  ScopeType parsed_scope_type = 0;
  const unsigned int stack_size = STACK_SIZE;
  void *stack[STACK_SIZE] = {0};
  void **stack_p = stack;
  void ** const stack_e = stack + STACK_SIZE;

  char c;
  while ((c = fgetc (input_file)) != EOF)
    {
      if (c == 'c')
        {
          while ((c = fgetc (input_file)) != EOF && c != '\n')
            ;
        }
      else if (c == 'p')
        {
          if (preamble_found)
            {
              fprintf (stderr, "Preamble already occurred!\n\n");
              exit (1);
            }

          if ((c = fgetc (input_file)) != ' ' ||
              (c = fgetc (input_file)) != 'c' ||
              (c = fgetc (input_file)) != 'n' ||
              (c = fgetc (input_file)) != 'f' ||
              (c = fgetc (input_file)) != ' ')
            {
              fprintf (stderr, "Malformed preamble!\n\n");
              exit (1);
            }

          char num_vars_str[128] = { '\0' };
          char num_clauses_str[128] = { '\0' };

          fscanf (input_file, "%s", num_vars_str);
          fscanf (input_file, "%s", num_clauses_str);

#if 0
          if (!is_unsigned_string (num_vars_str)
              || !is_unsigned_string (num_clauses_str))
            {
              fprintf (stderr, "Malformed preamble!\n\n");
              exit (1);
            }
#endif

          unsigned int num_vars, num_clauses;
          num_vars = atoi (num_vars_str);
          num_clauses = atoi (num_clauses_str);

          nenofex_set_up_preamble (nenofex, num_vars, num_clauses);

          preamble_found = 1;
        }
      else if (c == '-' || isdigit (c))
        {
          if (!preamble_found)
            {
              fprintf (stderr, "Preamble missing!\n\n");
              exit (1);
            }


          closed = 0;

          ungetc (c, input_file);

          long int val;
          fscanf (input_file, "%ld", &val);

          if (val == 0)
            {
              if (!parsed_scope_type)   /* parsing a clause */
                {
                  clause_cnt++;

                  nenofex_add_orig_clause (nenofex, stack, stack_p - stack);

                  /*clause_closed = 1; */
                }
              else              /* parsing a scope */
                {
                  nenofex_add_orig_scope (nenofex, stack, stack_p - stack, parsed_scope_type);
                  parsed_scope_type = 0;
                }

              closed = 1;
              stack_p = stack;
            }
          else
            {
              if (stack_p == stack_e)
                {
                  fprintf (stderr, "Clause exceeds static length limit of %u\n", STACK_SIZE);
                  exit (1);
                }
              *stack_p++ = (void *) val;
            }
        }
      else if (c == 'a')
        {
          parsed_scope_type = SCOPE_TYPE_UNIVERSAL;
          if (!closed)
            {
              fprintf (stderr, "Scope not closed!\n");
              exit (1);
            }
        }
      else if (c == 'e')
        {
          parsed_scope_type = SCOPE_TYPE_EXISTENTIAL;
          if (!closed)
            {
              fprintf (stderr, "Scope not closed!\n");
              exit (1);
            }
        }
      else if (!isspace (c))
        {
          fprintf (stderr, "Parsing: invalid character %c\n", c);
          exit (1);
        }
    }
}

int main (int argc, char ** argv)
{
  Nenofex *nenofex1 = nenofex_create ();
  Nenofex *nenofex2 = nenofex_create ();

  char *input_filename = argv[1]; 
  FILE *input_file = fopen (input_filename, "r");
  if (!input_file)
    {
      fprintf (stderr, "Cannot open input file: %s\n", input_filename);
      exit (1);
    }

  nenofex_parse (nenofex1, input_file);
  NenofexResult res1 = nenofex_solve (nenofex1);
  fprintf (stderr, "res1: %u\n", res1);

  /* Must reset file position indicator. */
  rewind (input_file);
  import_file_by_lib (nenofex2, input_file);
  NenofexResult res2 = nenofex_solve (nenofex2);
  fprintf (stderr, "res2: %u\n", res2);

  assert (res1 == res2);

  fclose (input_file);
  nenofex_delete (nenofex1);
  nenofex_delete (nenofex2);
  return 0;
}
