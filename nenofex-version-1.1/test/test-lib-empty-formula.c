#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../nenofex.h"

int main (int argc, char ** argv)
{
  Nenofex *nenofex = nenofex_create ();
  void **lits = calloc (100, sizeof (void *));

  nenofex_set_up_preamble (nenofex, 0, 0);
  NenofexResult res = nenofex_solve (nenofex);
  assert (res == NENOFEX_RESULT_SAT);

  nenofex_delete (nenofex);
  free (lits);
  return 0;
}
