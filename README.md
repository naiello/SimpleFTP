# SimpleFTP
Simple FTP client written for Computer Networks (Due October 31, 2016)

** INCLUDED FILES **

client/
	client_cmds.c
	client_cmds.h
	ftpclient.c
	myftp
	test.txt
common/
	cmd_defs.h
	timing.c
	timing.h
Makefile
README.md
server/
	ftpserver.c
	myftpd

** USAGE **

To start the server:
./myftpd [port]

To start the client:
./myftp [server] [port]

Example client commands:
FTP> UPL test.txt
FTP> REQ test2.txt
FTP> CHD somedir
FTP> RMD olddir
FTP> MKD newdir
FTP> LIS
FTP> XIT
