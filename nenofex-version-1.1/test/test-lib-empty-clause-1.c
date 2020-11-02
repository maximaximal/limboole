#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../nenofex.h"

int main (int argc, char ** argv)
{
  Nenofex *nenofex = nenofex_create ();
  void **lits = calloc (100, sizeof (void *));

  nenofex_set_up_preamble (nenofex, 2, 3);
  lits[0] = (void *) 1;
  nenofex_add_orig_clause (nenofex, lits, 1);
  lits[0] = (void *) 0;
  nenofex_add_orig_clause (nenofex, lits, 0);
  lits[0] = (void *) 2;
  nenofex_add_orig_clause (nenofex, lits, 1);
  NenofexResult res = nenofex_solve (nenofex);
  assert (res == NENOFEX_RESULT_UNSAT);

  nenofex_delete (nenofex);
  free (lits);
  return 0;
}
