all: myftp

myftp: client/ftpclient.o client/client_cmds.o
	gcc -Wall -lmhash client/ftpclient.o client/client_cmds.o -o myftp

client/ftpclient.o: client/ftpclient.c
	gcc -Wall -c client/ftpclient.c -o client/ftpclient.o

client/client_cmds.o: client/client_cmds.c
	gcc -Wall -c client/client_cmds.c -o client/client_cmds.o

.PHONY: clean
clean:
	rm -f client/*.o server/*.o myftp
