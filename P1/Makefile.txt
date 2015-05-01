shared.out: shared.c 
	g++ -ansi -Wall -o shared.out shared.c -lrt

clean:
	rm -f shared.out