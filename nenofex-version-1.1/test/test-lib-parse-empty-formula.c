#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "../nenofex.h"

int main (int argc, char ** argv)
{
  Nenofex *nenofex = nenofex_create ();

  FILE *input_file = fopen ("./test-lib-parse-empty-formula.qdimacs", "r");

  nenofex_parse (nenofex, input_file);
  NenofexResult res = nenofex_solve (nenofex);
  assert (res == NENOFEX_RESULT_SAT);

  fclose (input_file);
  nenofex_delete (nenofex);
  return 0;
}
