CFLAGS=-g3 

all:
	gcc $(CFLAGS) -o test-lib-empty-clause test-lib-empty-clause.c -L.. -lnenofex
	gcc $(CFLAGS) -o test-lib-parse-empty-clause test-lib-parse-empty-clause.c -L.. -lnenofex
#
	gcc $(CFLAGS) -o test-lib-empty-clause-1 test-lib-empty-clause-1.c -L.. -lnenofex
	gcc $(CFLAGS) -o test-lib-parse-empty-clause-1 test-lib-parse-empty-clause-1.c -L.. -lnenofex
#
	gcc $(CFLAGS) -o test-lib-empty-clause-2 test-lib-empty-clause-2.c -L.. -lnenofex
	gcc $(CFLAGS) -o test-lib-parse-empty-clause-2 test-lib-parse-empty-clause-2.c -L.. -lnenofex
#
	gcc $(CFLAGS) -o test-lib-empty-formula test-lib-empty-formula.c -L.. -lnenofex
	gcc $(CFLAGS) -o test-lib-parse-empty-formula test-lib-parse-empty-formula.c -L.. -lnenofex
#
	gcc $(CFLAGS) -o test-lib-trivial-formula test-lib-trivial-formula.c -L.. -lnenofex
	gcc $(CFLAGS) -o test-lib-parse-trivial-formula test-lib-parse-trivial-formula.c -L.. -lnenofex
#
	gcc $(CFLAGS) -o test-lib-easy-sat test-lib-easy-sat.c -L.. -lnenofex
	gcc $(CFLAGS) -o test-lib-parse-easy-sat test-lib-parse-easy-sat.c -L.. -lnenofex
#
	gcc $(CFLAGS) -o test-lib-easy-unsat test-lib-easy-unsat.c -L.. -lnenofex
	gcc $(CFLAGS) -o test-lib-parse-easy-unsat test-lib-parse-easy-unsat.c -L.. -lnenofex
#
	gcc $(CFLAGS) -o test-formula-by-lib-and-parser test-formula-by-lib-and-parser.c -L.. -lnenofex
clean:
	rm -f ./test-lib-empty-formula ./test-lib-empty-clause ./test-lib-empty-clause-1 ./test-lib-empty-clause-2 ./test-lib-parse-empty-clause ./test-lib-parse-empty-clause-1 ./test-lib-parse-empty-clause-2 ./test-lib-parse-empty-formula ./test-formula-by-lib-and-parser ./test-lib-parse-easy-sat ./test-lib-parse-trivial-formula ./test-lib-trivial-formula ./test-lib-easy-unsat ./test-lib-parse-easy-unsat ./test-lib-easy-sat *~
