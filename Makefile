all : FIFO.out RR.out VRR.out

FIFO.out:FIFO.o
	gcc -o FIFO.out FIFO.o
RR.out:RR.o
	gcc -o RR.out RR.o
VRR.out:VRR.o
	gcc -o VRR.out VRR.o


FIFO.o:FIFO.c
	gcc -c -o FIFO.o FIFO.c
RR.o:RR.c
	gcc -c -o RR.o RR.c
VRR.o:VRR.c
	gcc -c -o VRR.o VRR.c

clean :
	rm *.o