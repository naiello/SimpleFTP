all: client/myftp server/myftpd

client/myftp: client/ftpclient.o client/client_cmds.o common/timing.o
	gcc -Wall -lmhash client/ftpclient.o client/client_cmds.o common/timing.o -o myftp

server/myftpd: server/ftpserver.o
	gcc -Wall -lmhash server/ftpserver.o common/timing.o -o myftpd

client/ftpclient.o: client/ftpclient.c client/client_cmds.h common/cmd_defs.h
	gcc -Wall -c client/ftpclient.c -o client/ftpclient.o

client/client_cmds.o: client/client_cmds.c common/timing.h client/client_cmds.h common/cmd_defs.h
	gcc -Wall -c client/client_cmds.c -o client/client_cmds.o

server/ftpserver.o: server/ftpserver.c common/timing.h
	gcc -Wall -c server/ftpserver.c -o server/ftpserver.o

common/timing.o: common/timing.c
	gcc -Wall -c common/timing.c -o common/timing.o

.PHONY: clean
clean:
	rm -f client/*.o server/*.o common/*.o myftp myftpd
