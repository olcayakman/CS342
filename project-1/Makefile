all: process_sort thread_sort

process_sort: pwc.c
	gcc -g -Wall -o process_sort pwc.c -lrt

thread_sort: twc.c
	gcc -Wall -o thread_sort twc.c -lrt -lpthread

clean:
	rm -fr *~ process_sort thread_sort 