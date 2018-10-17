/*------------------------------------------------------------------------*\

Translate a CNF formula in DIMACS format into the boole format expected
by limboole.  The parser is not really solid.  For the reverse direction
use

  sed -e '/&/d' -e 's,!,-,g' -e 's,[v(],,g' -e 's,), 0,g' -e 's, | , ,g'

In particular

  ./dimacs2boole | \
  sed -e '/&/d' -e 's,!,-,g' -e 's,[v(],,g' -e 's,), 0,g' -e 's, | , ,g'

should be the identity.

\*------------------------------------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

/*------------------------------------------------------------------------*/

static void
die (const char *fmt, ...)
{
  va_list ap;

  fprintf (stderr, "*** dimacs2boole: ");
  va_start (ap, fmt);
  vfprintf (stderr, fmt, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}

/*------------------------------------------------------------------------*/

static int
next (FILE * file, int *lineno_ptr)
{
  int res;

  res = fgetc (file);
  if (res == '\n')
    *lineno_ptr += 1;

  return res;
}

/*------------------------------------------------------------------------*/

int
main (int argc, char **argv)
{
  int count_literals_per_clause;
  char *name = "<stdin>";
  int count_clauses;
  int close_file;
  FILE *file;
  int lineno;
  int sign;
  int idx;
  int ch;
  int i;

  file = stdin;
  close_file = 0;

  for (i = 1; i < argc; i++)
    {
      if (!strcmp (argv[i], "-h"))
	{
	  printf ("usage: dimacs2boole [-h][<file-name>]\n");
	  exit (0);
	}
      else if (close_file)
	die ("can not open two files");
      else if (!(file = fopen ((name = argv[i]), "r")))
	die ("can not read '%s'", argv[i]);
      else
	close_file = 1;
    }

  count_literals_per_clause = 0;
  count_clauses = 0;
  lineno = 1;

  for (;;)
    {
      do
	{
	  ch = next (file, &lineno);
	}
      while (isspace (ch));

      if (ch == EOF)
	break;

      if (ch == 'c' || ch == 'p')
	{
	  do
	    {
	      ch = next (file, &lineno);
	    }
	  while (ch != '\n' && ch != EOF);
	}
      else
	{
	  if (ch == '-')
	    {
	      sign = -1;
	      ch = next (file, &lineno);
	    }
	  else
	    sign = 1;

	  if (!isdigit (ch))
	    die ("syntax error:\n%s:%d: expected digit but found '%c'", name,
		 lineno, ch);

	  idx = 0;

	  do
	    {
	      idx = 10 * idx + (ch - '0');
	      ch = next (file, &lineno);
	    }
	  while (isdigit (ch));

	  if (idx)
	    {
	      if (count_literals_per_clause)
		printf (" | ");
	      else
		{
		  if (count_clauses)
		    printf ("&\n");
		  printf ("(");
		}

	      if (sign == -1)
		printf ("!");
	      printf ("v%d", idx);
	      count_literals_per_clause++;
	    }
	  else if (count_literals_per_clause)
	    {
	      printf (")\n");
	      count_literals_per_clause = 0;
	      count_clauses++;
	    }
	  else
	    die ("syntax error:\n%s:%d: can not handle empty clauses", name,
		 lineno);
	}
    }

  if (count_literals_per_clause)
    die ("syntax error:\n%s:%d: closing 0 missing", name, lineno);

  if (!count_clauses)
    die ("syntax error:\n%s:%d: no clauses found", name, lineno);

  if (close_file)
    fclose (file);

  return 0;
}
