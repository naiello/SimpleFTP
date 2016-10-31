all: myftpd

myftpd: ftpserver.o
	gcc -Wall -lmhash ftpserver.o ../common/timing.o -o myftpd

ftpserver.o: ftpserver.c ../common/timing.h
	gcc -Wall -c ftpserver.c -o ftpserver.o

../common/timing.o: ../common/timing.c
	gcc -Wall -c ../common/timing.c -o ../common/timing.o

.PHONY: clean
clean:
	rm -f *.o myftpd
