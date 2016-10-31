/* Simple FTP Client
 * Authors: Nick Aiello / Rosalyn Tan
 * Description: TODO
 */

#include <ctype.h>
#include <error.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../common/cmd_defs.h"
#include "client_cmds.h"

#define STR_PROMPT "FTP> "
#define CMD_BUFSZ 256

void parse_command(int, char *, size_t);
void prompt(char *, size_t);

int main(int argc, char **argv)
{
	int sockfd;
	char *port, *server;
	char cmd[CMD_BUFSZ];
	struct addrinfo hints, *serv_addr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;      // allow IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM;  // TCP

	if (argc != 3) {
		error(1, 0, "Usage: %s [server] [port]", argv[0]);
	}

	server = argv[1];
	port = argv[2];

	// resolve hostname to IP address
	if (getaddrinfo(server, port, &hints, &serv_addr) != 0) {
		error(2, 0, "Failed to resolve server %s:%s", server, port);
	}

	// open a socket on the local machine
	if ((sockfd = socket(serv_addr->ai_family, serv_addr->ai_socktype, 
					serv_addr->ai_protocol)) == 0) {
		error(3, errno, "Failed to open socket.");
	}

	// use the socket to connect to the server
	if (connect(sockfd, serv_addr->ai_addr, serv_addr->ai_addrlen) != 0) {
		error(4, errno, "Failed to connect to server.");
	}

	while (strncmp(CMDSTR_XIT, cmd, 3)) {
		prompt(cmd, sizeof(cmd));
		parse_command(sockfd, cmd, sizeof(cmd));
	}

	// clean up
	close(sockfd);
	freeaddrinfo(serv_addr);

	return 0;
}

void parse_command(int sockfd, char *cmd, size_t cmdlen) 
{
	char *arg = cmd + 4;
	arg[strlen(arg)-1] = 0;

	// all commands are 3 characters followed by a whitespace char
//	if ((cmd[3] != ' ') && (cmd[3] != '\n')) {
	if(!isspace(cmd[3]) && strlen(cmd) != 3) {
		printf("Unknown command: %s", cmd);
		return;
	}

	// add trailing null and convert to uppercase
	cmd[3] = 0;
	int i;
	for (i = 0; i < strlen(cmd); i++) {
		cmd[i] = toupper(cmd[i]);
	}

	if (!strcmp(cmd, CMDSTR_REQ)) {
		ftpc_request(sockfd, arg, strlen(arg));
	} else if (!strcmp(cmd, CMDSTR_UPL)) {
		ftpc_upload(sockfd, arg, strlen(arg));
	} else if (!strcmp(cmd, CMDSTR_DEL)) {
		ftpc_delete(sockfd, arg, strlen(arg));
	} else if (!strcmp(cmd, CMDSTR_LIS)) {
		ftpc_list(sockfd);
	} else if (!strcmp(cmd, CMDSTR_RMD)) {
		ftpc_rmdir(sockfd, arg, strlen(arg));
	} else if (!strcmp(cmd, CMDSTR_MKD)) {
		ftpc_mkdir(sockfd, arg, strlen(arg));
	} else if (!strcmp(cmd, CMDSTR_CHD)) {
		ftpc_chdir(sockfd, arg, strlen(arg));
	} else if (!strcmp(cmd, CMDSTR_XIT)) {
		printf("Goodbye!\n");
	} else {
		printf("Commands: REQ, UPL, DEL, LIS, RMD, MKD, CHD, XIT\n");
	}
}


void prompt(char *buf, size_t buflen) 
{
	memset(buf, 0, buflen);
	printf(STR_PROMPT);
	fgets(buf, buflen, stdin);
}
