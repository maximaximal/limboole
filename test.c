#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

/*------------------------------------------------------------------------*/

int limboole (int, char **);

/*------------------------------------------------------------------------*/

typedef struct TestSuite TestSuite;

struct TestSuite
{
  const char *pattern;
  unsigned count;
  unsigned failed;
  unsigned ok;
  int keep;
};

/*------------------------------------------------------------------------*/

static int
cmp_files (const char *a, const char *b)
{
  FILE *f;
  FILE *g;
  int res;
  int ch;

  res = 1;

  if (!(f = fopen (a, "r")))
    res = 0;
  if (!(g = fopen (b, "r")))
    res = 0;

  if (res)
    {
      do
	{
	  ch = fgetc (f);
	  res = (fgetc (g) == ch);
	}
      while (ch != EOF && res);
    }

  if (f)
    fclose (f);
  if (g)
    fclose (g);

  return res;
}

/*------------------------------------------------------------------------*/

static int
match (const char *str, const char *pattern)
{
  const char *p, *q;

  for (p = str, q = pattern; *q && *q == *p; p++, q++)
    ;

  return !*q;
}

/*------------------------------------------------------------------------*/

#define USAGE \
"usage: testlimboole [-h|-k|--version] [ <pattern> ]\n"

#define ID \
"$Id: test.c,v 1.10 2005-11-28 23:12:10 biere Exp $\n"

/*------------------------------------------------------------------------*/

static void run_all (TestSuite *);

/*------------------------------------------------------------------------*/

int
main (int argc, char **argv)
{
  TestSuite ts;
  int error;
  int done;
  int i;

  memset (&ts, 0, sizeof (ts));
  error = 0;
  done = 0;

  for (i = 1; !done && !error && i < argc; i++)
    {
      if (!strcmp (argv[i], "-h"))
	{
	  printf (USAGE);
	  done = 1;
	}
      else if (!strcmp (argv[i], "-k"))
	{
	  ts.keep = 1;
	}
      else if (!strcmp (argv[i], "--version"))
	{
	  printf (ID);
	  done = 1;
	}
      else if (ts.pattern)
	{
	  fprintf (stderr, "*** multiple patterns specified (try '-h')\n");
	  error = 1;
	}
      else
	ts.pattern = argv[i];
    }

  if (error)
    return 1;

  if (done)
    return 0;

  run_all (&ts);
  printf ("%u ok, %u failed (out of %u)\n", ts.ok, ts.failed, ts.count);

  return 0;
}

/*------------------------------------------------------------------------*/

static void
overwrite (int ch)
{
  int i;

  for (i = 0; i < 31; i++)
    fputc (ch, stdout);
}

/*------------------------------------------------------------------------*/

static int
erase (TestSuite * ts)
{
  char *str;

  if (ts->keep)
    return 0;

  if (!isatty (1))
    return 0;

  str = getenv ("TERM");

  if (!str)
    return 0;

  if (!strcmp (str, "dumb"))
    return 0;

  if (!strcmp (str, "emacs"))
    return 0;

  overwrite ('');
  overwrite (' ');
  overwrite ('');

  return 1;
}

/*------------------------------------------------------------------------*/

static void
run (TestSuite * ts, int expected_res, int argc, ...)
{
  char *out_name;
  char *log_name;
  char **my_argv;
  int real_res;
  char *name;
  int my_argc;
  va_list ap;
  int matched;
  int res;
  int len;
  int i;

  assert (argc > 0);

  va_start (ap, argc);
  name = va_arg (ap, char *);

  matched = (!ts->pattern || match (name, ts->pattern));

  if (matched)
    {
      printf ("%-20s ...", name);
      fflush (stdout);

      len = strlen (name);
      out_name = (char *) malloc (len + 9);
      sprintf (out_name, "log/%s.out", name);

      log_name = (char *) malloc (len + 9);
      sprintf (log_name, "log/%s.log", name);

      my_argc = argc + 4;
      my_argv = (char **) malloc (my_argc * sizeof (char *));
      my_argv[0] = name;
      my_argv[1] = "-o";
      my_argv[2] = log_name;
      my_argv[3] = "-l";
      my_argv[4] = log_name;

      for (i = 1; i < argc; i++)
	my_argv[i + 4] = va_arg (ap, char *);

      real_res = limboole (my_argc, my_argv);
      res = (real_res == expected_res);
      if (res)
	res = cmp_files (out_name, log_name);

      free (my_argv);

      ts->count++;
      if (res)
	{
	  printf (" ok    ");
	  if (!erase (ts))
	    fputc ('\n', stdout);
	  ts->ok++;
	}
      else
	{
	  printf (" failed\n");
	  ts->failed++;
	}

      free (out_name);
      free (log_name);
    }

  va_end (ap);
}

/*------------------------------------------------------------------------*/

static void
run_all (TestSuite * ts)
{
  run (ts, 1, 2, "nologfilename", "-l");
  run (ts, 1, 2, "nooutfilename", "-o");
  run (ts, 1, 3, "outfilenotwritable", "-o",
       "/a-non-existing-dir/a-non-writable-file");
  run (ts, 1, 3, "logfilenotwritable", "-l",
       "/a-non-existing-dir/a-non-writable-file");
  run (ts, 1, 3, "twooutfiles", "-o", "/dev/null");
  run (ts, 1, 3, "twologfiles", "-l", "/dev/null");
  run (ts, 1, 2, "invalidoption", "-invalid-option");
  run (ts, 1, 2, "infilenotreadable", "/a-non-existing-file");
  run (ts, 1, 2, "missingmpara", "-m");
  run (ts, 1, 3, "twofiles", "log/twofiles.in", "/dev/null");
  run (ts, 0, 3, "var0", "-p", "log/var0.in");
  run (ts, 1, 3, "var1", "-p", "log/var1.in");
  run (ts, 1, 2, "devnull", "/dev/null");
  run (ts, 1, 2, "missingpara", "log/missingpara.in");
  run (ts, 0, 3, "basic0", "-p", "log/basic0.in");
  run (ts, 0, 3, "iff0", "-p", "log/iff0.in");
  run (ts, 0, 3, "iff1", "-p", "log/iff1.in");
  run (ts, 0, 3, "implies0", "-p", "log/implies0.in");
  run (ts, 0, 3, "implies1", "-p", "log/implies1.in");
  run (ts, 0, 3, "not0", "-p", "log/not0.in");
  run (ts, 0, 3, "not1", "-p", "log/not1.in");
  run (ts, 0, 3, "or0", "-p", "log/or0.in");
  run (ts, 0, 3, "or1", "-p", "log/or1.in");
  run (ts, 0, 3, "and0", "-p", "log/and0.in");
  run (ts, 0, 3, "and1", "-p", "log/and1.in");
  run (ts, 0, 3, "comment0", "-p", "log/comment0.in");
  run (ts, 0, 3, "ppandor", "-p", "log/ppandor.in");
  run (ts, 0, 3, "pp0", "-p", "log/pp0.in");
  run (ts, 0, 3, "pp1", "-p", "log/pp1.in");
  run (ts, 1, 2, "twovar", "log/twovar.in");
  run (ts, 1, 2, "iff2", "log/iff2.in");
  run (ts, 1, 2, "implies2", "log/implies2.in");
  run (ts, 1, 2, "or2", "log/or2.in");
  run (ts, 1, 2, "and2", "log/and2.in");
  run (ts, 1, 2, "not2", "log/not2.in");
  run (ts, 0, 3, "not3", "-p", "log/not3.in");
  run (ts, 1, 2, "invalidchar", "log/invalidchar.in");
  run (ts, 1, 2, "aandcommaa", "log/aandcommaa.in");
  run (ts, 1, 2, "ltnodash", "log/ltnodash.in");
  run (ts, 0, 2, "ltdashnogt", "log/ltdashnogt.in");
  run (ts, 1, 2, "dashnogt", "log/dashnogt.in");
  run (ts, 0, 3, "varorvar", "-p", "log/varorvar.in");
  run (ts, 1, 2, "varlp", "log/varlp.in");
  run (ts, 1, 2, "varrp", "log/varrp.in");
  run (ts, 1, 2, "varnot", "log/varnot.in");
  run (ts, 1, 2, "varandand", "log/varandand.in");
  run (ts, 1, 2, "variffor", "log/variffor.in");
  run (ts, 1, 2, "varorimplies", "log/varorimplies.in");
  run (ts, 1, 2, "varnotiff", "log/varnotiff.in");
  run (ts, 1, 2, "firstcharinvalid", "log/firstcharinvalid.in");
  run (ts, 0, 3, "dumpvar", "-d", "log/dumpvar.in");
  run (ts, 0, 3, "dumpnotvar", "-d", "log/dumpnotvar.in");
  run (ts, 0, 3, "dumpvarornotvar", "-d", "log/dumpvarornotvar.in");
  run (ts, 0, 3, "dumpvariffvar", "-d", "log/dumpvariffvar.in");
  run (ts, 0, 3, "dumpvarimpliesnotvar", "-d", "log/dumpvarimpliesnotvar.in");
  run (ts, 0, 3, "dumpnotvarandnotvar", "-d", "log/dumpnotvarandnotvar.in");
  run (ts, 0, 3, "sat0", "-s", "log/sat0.in");
  run (ts, 0, 3, "sat1", "-s", "log/sat1.in");
  run (ts, 0, 2, "valid0", "log/valid0.in");
  run (ts, 0, 2, "valid1", "log/valid1.in");
  run (ts, 0, 2, "valid2", "log/valid2.in");
  run (ts, 1, 4, "valid3", "-m", "0", "log/valid3.in");
  run (ts, 0, 2, "valid4", "log/valid4.in");
  run (ts, 0, 2, "valid5", "log/valid5.in");
  run (ts, 0, 3, "sat2", "-s", "log/sat2.in");
  run (ts, 0, 3, "prime9", "-s", "log/prime9.in");
  run (ts, 0, 2, "count2live", "log/count2live.in");
  run (ts, 0, 2, "count2stall", "log/count2stall.in");
}
