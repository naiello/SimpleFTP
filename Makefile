all: myftp

myftp: client/ftpclient.o client/client_cmds.o client/timing.o
	gcc -Wall -lmhash client/ftpclient.o client/client_cmds.o client/timing.o -o myftp

client/ftpclient.o: client/ftpclient.c client/client_cmds.h common/cmd_defs.h
	gcc -Wall -c client/ftpclient.c -o client/ftpclient.o

client/client_cmds.o: client/client_cmds.c client/timing.h client/client_cmds.h common/cmd_defs.h
	gcc -Wall -c client/client_cmds.c -o client/client_cmds.o

client/timing.o: client/timing.c
	gcc -Wall -c client/timing.c -o client/timing.o

.PHONY: clean
clean:
	rm -f client/*.o server/*.o myftp
