CC=gcc
libusmem.so: usmem.o
	$(CC) $^ -shared -o $@
usmem.o:usmem.c
	$(CC) -fPIC $^ -c -o $@
clean:
	-rm *.o libusmem.so
