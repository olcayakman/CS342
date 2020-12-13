all: libsmemlib.a app create_memory destroy_memory

libsmemlib.a: smemlib.c
	gcc -Wall -c smemlib.c -lpthread -lrt
	ar -cvq libsmemlib.a smemlib.o
	ranlib libsmemlib.a

app: app.c
	gcc -Wall -o app app.c -L. -lsmemlib -lpthread -lrt

create_memory: create_memory.c
	gcc -Wall -o create_memory create_memory.c -L. -lsmemlib -lpthread -lrt

destroy_memory: destroy_memory.c
	gcc -Wall -o destroy_memory destroy_memory.c -L. -lsmemlib -lpthread -lrt

clean: 
	rm -fr *.o *.a *~ a.out app smemlib.o smemlib.a libsmemlib.a create_memory destroy_memory 