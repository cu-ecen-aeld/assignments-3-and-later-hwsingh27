CROSS_COMPILE =

default: writer

all: writer.o
	$(CROSS_COMPILE)gcc -g -Wall writer.o -0 writer
	
writer.o: writer.c
	$(CROSS_COMPILE)gcc -g -Wall -c -o writer.o writer.c

clean:
	rm -f *.o writer
