#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int limboole_extended(int argc, char **argv, int op, char *input,
                      unsigned int input_length);
int limboole (int, char **);

int
main (int argc, char **argv)
{
#ifndef NDEBUG
  if(argc >= 2) {
    if(strcmp(argv[1], "WEB") == 0) {
      if(argc == 2) {
        printf("WEB Mode! Input mode 0: Validity Check, 1 SAT Check, 2 QBF Validity Check, 3 QBF SAT Check\n");
        return -1;
      }
      int extendedop = atoi(argv[2]);
      argc -= 2;
      argv[2] = argv[1];
      argv += 2;
      return limboole_extended(argc, argv, extendedop, NULL, 0);
    }
  }
#endif
  
  return limboole (argc, argv);
}
