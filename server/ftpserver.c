/* Simple FTP Server
 * Authors: Rosalyn Tan / Nick Aiello
 * Description: TODO
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define SERVER_PORT 41001 // or 41002
#define MAX_PENDING 5
#define MAX_LINE 256 // may need to change

int main(int argc, char **argv) {
	struct sockaddr_in sin;
	char buf[MAX_LINE];
	int len;
	int s, new_s;

	// build address data structure
	bzero((char*)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(SERVER_PORT);

	// create socket
	if((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Create socket failed");
		exit(1);
	}

	// set socket option
	if((setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(int))) < 0) {
		perror("Set socket option failed");
		exit(1);
	}

	// bind
	if((bind(s, (struct sockaddr*)&sin, sizeof(sin))) < 0) {
		perror("Bind failed");
		exit(1);
	}

	// listen
	if((listen(s, MAX_PENDING)) < 0) {
		perror("Listen failed");
		exit(1);
	}

	// wait for connections
	while(1) {
		if((new_s = accept(s, (struct sockaddr*)&sin, &len)) < 0) {
			perror("Connection accept failed");
			exit(1);
		}
		while(1) {
			if((len=recv(new_s, buf, sizeof(buf), 0)) == -1) {
				perror("Server receive failed");
				exit(1);
			}
			if(len == 0) {
				break;
			}
		}
		close(new_s);
	}
}
