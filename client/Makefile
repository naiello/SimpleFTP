all: myftp

myftp: ftpclient.o client_cmds.o ../common/timing.o
	gcc -Wall -lmhash ftpclient.o client_cmds.o ../common/timing.o -o myftp

ftpclient.o: ftpclient.c client_cmds.h ../common/cmd_defs.h
	gcc -Wall -c ftpclient.c -o ftpclient.o

client_cmds.o: client_cmds.c ../common/timing.h client_cmds.h ../common/cmd_defs.h
	gcc -Wall -c client_cmds.c -o client_cmds.o

../common/timing.o: ../common/timing.c
	gcc -Wall -c ../common/timing.c -o ../common/timing.o

.PHONY: clean
clean:
	rm -f *.o myftp
