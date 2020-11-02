#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../nenofex.h"

int main (int argc, char ** argv)
{
  Nenofex *nenofex = nenofex_create ();
  void **lits = calloc (100, sizeof (void *));

  /*
    p cnf 2 3 
    a 1 0
    e 2 0
    1 -2 0
    0
    -1 2 0
  */

  nenofex_set_up_preamble (nenofex, 2, 3);
  lits[0] = (void *) 1;
  nenofex_add_orig_scope (nenofex, lits, 1, SCOPE_TYPE_UNIVERSAL);
  lits[0] = (void *) 2;
  nenofex_add_orig_scope (nenofex, lits, 1, SCOPE_TYPE_EXISTENTIAL);

  lits[0] = (void *) 1;
  lits[1] = (void *) -2;
  nenofex_add_orig_clause (nenofex, lits, 2);

  /* empty clause */
  nenofex_add_orig_clause (nenofex, lits, 0);

  lits[0] = (void *) -1;
  lits[1] = (void *) 2;
  nenofex_add_orig_clause (nenofex, lits, 2);

  NenofexResult res = nenofex_solve (nenofex);
  assert (res == NENOFEX_RESULT_UNSAT);

  nenofex_delete (nenofex);
  free (lits);
  return 0;
}
