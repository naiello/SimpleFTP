/* Simple FTP Server
 * Authors: Rosalyn Tan / Nick Aiello
 * Description: TODO
 */

#include <mhash.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../common/cmd_defs.h"

#define SERVER_PORT 41001 // or 41002
#define BUF_SIZE 256 // may need to change
#define MAX_PENDING 5

void cmd_req(int);

int main(int argc, char **argv) {
	struct sockaddr_in sin;
	char buf[BUF_SIZE];
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
	if((setsockopt(s, SOL_SOCKET, SO_REUSEADDR, 0, sizeof(int))) < 0) {
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
			continue;
		}
		while(1) {
			bzero((void*)&buf, sizeof(buf));
			if((len=recv(new_s, buf, sizeof(buf), 0)) == -1) {
				perror("Server receive failed");
				continue;
			}
			// client closed connection
			if(len == 0) {
				break;
			}
			if(!strcmp(buf, CMDSTR_REQ)) {
				cmd_req(new_s);
			} else if(!strcmp(buf, CMDSTR_UPL)) {
				// UPL code
			} else if(!strcmp(buf, CMDSTR_LIS)) {
				// LIS code
			} else if(!strcmp(buf, CMDSTR_MKD)) {
				// MKD code
			} else if(!strcmp(buf, CMDSTR_RMD)) {
				// RMD code
			} else if(!strcmp(buf, CMDSTR_CHD)) {
				// CHD code
			} else if(!strcmp(buf, CMDSTR_DEL)) {
				// DEL code
			} else if(!strcmp(buf, CMDSTR_XIT)) {
				break;
			} else {
				continue;
			}
		}
		close(new_s);
	}
}

void cmd_req(int s) {
	int16_t file_len;
	char file_name[BUF_SIZE];
	struct stat file_stat;
	int32_t file_size;
	MHASH hashd;
	FILE *fp;
	char hash_buf[16];
	char file_buf[BUF_SIZE];
	int count;
	int len;

	// need to zero buffers
	bzero((void*)file_name, sizeof(file_name));
	bzero((void*)&file_stat, sizeof(file_stat));
	bzero((void*)hash_buf, sizeof(hash_buf));
	bzero((void*)file_buf, sizeof(file_buf));

	// receive file length and file name
	if((len = read(s, &file_len, sizeof(file_len))) == -1) {
		perror("File length receive error");
		return;
	// client closed connection--handle in some other way?
	} else if(len == 0) {
		return;
	}
	file_len = ntohs(file_len);
	
	if((len = read(s, &file_name, file_len)) == -1) {
		perror("File name receive error");
		return;
	// client closed connection
	} else if(len == 0) {
		return;
	}

	// check if file exists in local directory, send size or -1
	if(stat(file_name, &file_stat) == 0) {
		// send size
		file_size = (int32_t)file_stat.st_size;
		file_size = htons(file_size);
		if(write(s, &file_size, sizeof(int32_t)) == -1) {
			perror("File size send error");
			return;
		}
	} else {
		// send -1
		file_size = htons(-1);
		if(write(s, &file_size, sizeof(int32_t)) == -1) {
			perror("File does not exist error");
			return;
		}
	}

	// init hash
	if((hashd = mhash_init(MHASH_MD5)) == MHASH_FAILED) {
		perror("Init mhash failed");
		return;
	}

	// compute hash
	fp = fopen(file_name, "r"); // need error handling?
	while(count = fread(file_buf, 1, BUF_SIZE, fp) == BUF_SIZE) {
		mhash(hashd, file_buf, count);
	}
	// hash last chunk of file
	mhash(hashd, file_buf, count);
	mhash_deinit(hashd, hash_buf);

	// send hash
	if(write(s, hash_buf, sizeof(hash_buf)) == -1) {
		perror("File hash send error");
		return;
	}
	
	// send file to client
	while(count = fread(file_buf, 1, BUF_SIZE, fp) == BUF_SIZE) {
		if(write(s, file_buf, count) == -1) {
			perror("File send error");
			return;
		}
		bzero((void*)file_buf, sizeof(file_buf));
	}
	if(write(s, file_buf, count) == -1) {
		perror("File send error");
		return;
	}
}
