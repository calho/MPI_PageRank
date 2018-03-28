compile: clean datatrim main basic serial serialtester

datatrim: datatrim.c
	gcc datatrim.c -o datatrim

main: main.c Lab4_IO.c
	mpicc -w -g -Wall -o main main.c Lab4_IO.c -lm

basic: basic.c Lab4_IO.c
	mpicc -w -g -Wall -o basic basic.c Lab4_IO.c -lm

serial: serial.c Lab4_IO.c
	mpicc -g -Wall -o serial serial.c Lab4_IO.c -lm

serialtester: serialtester.c Lab4_IO.c
	mpicc -g -Wall -o serialtester serialtester.c Lab4_IO.c -lm

clean:
	rm -rf tmp datatrim.o datatrim main.o main basic basic.o

