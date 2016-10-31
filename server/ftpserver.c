/* Simple FTP Server
 * Authors: Rosalyn Tan / Nick Aiello
 * NetIds: rtan / naiello
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
#include "../common/timing.h"

#define SERVER_PORT 41001 // or 41002
#define MAX_LINE 4096 // may need to change
#define MAX_PENDING 5

void cmd_req(int);
void cmd_upl(int);
void cmd_del(int);
void cmd_lis(int);

int main(int argc, char **argv) {
	struct sockaddr_in sin;
	unsigned int len;
	int s, new_s;
	int opt = 0;
	int16_t code;

	// build address data structure
	bzero((char*)&sin, sizeof(sin));
	sin.sin_family = AF_UNSPEC;
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
			continue;
		}
		while(1) {
			if((len=recv(new_s, &code, sizeof(code), 0)) == -1) {
				perror("Server receive failed");
				continue;
			}
			// client closed connection
			if(len == 0) {
				break;
			}
			if(code == htons(CMD_REQ)) {
				cmd_req(new_s);
			} else if(code == htons(CMD_UPL)) {
				cmd_upl(new_s);
			} else if(code == htons(CMD_LIS)) {
				cmd_lis(new_s);
			} else if(code == htons(CMD_MKD)) {
				// MKD code
			} else if(code == htons(CMD_RMD)) {
				// RMD code
			} else if(code == htons(CMD_CHD)) {
				// CHD code
			} else if(code == htons(CMD_DEL)) {
				cmd_del(new_s);
			} else if(code == htons(CMD_XIT)) {
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
	char file_name[MAX_LINE];
	struct stat file_stat;
	int32_t file_size;
	MHASH hashd;
	FILE *fp;
	char hash_buf[16];
	char file_buf[MAX_LINE];
	int count;
	int len;

	// zero buffers
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
		file_size = htonl(file_size);
		if(write(s, &file_size, sizeof(int32_t)) == -1) {
			perror("File size send error");
			return;
		}
	} else {
		// send -1
		file_size = htonl(-1);
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
	while((count = fread(file_buf, 1, MAX_LINE, fp)) == MAX_LINE) {
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
	
	rewind(fp);

	// send file to client
	while((count = fread(file_buf, 1, MAX_LINE, fp)) == MAX_LINE) {
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
	fclose(fp);
}

void cmd_upl(int s) {
	int16_t ack;
	int16_t file_len;
	char file_name[MAX_LINE];
	char file_buf[MAX_LINE];
	int32_t file_size;
	int32_t counter = 0;
	int32_t file_read;
	FILE* fp;
	char file_hash[16];
	char recv_hash[16];
	MHASH hashd;
	unsigned long trans_time;
	
	//  zero buffers
	bzero((void*)file_name, sizeof(file_name));
	bzero((void*)&file_buf, sizeof(file_buf));
	bzero((void*)file_hash, sizeof(file_hash));
	bzero((void*)recv_hash, sizeof(recv_hash));

	// receive length of file name
	if(read(s, &file_len, sizeof(file_len)) == -1) {
		perror("Receive file length error");
		return;
	}
	file_len = ntohs(file_len);

	// receive file name
	if(read(s, file_name, file_len) == -1) {
		perror("Receive file name error");
		return;
	}

	fp = fopen(file_name, "w+");

	// send ACK
	ack = htons(1);
	if(write(s, &ack, sizeof(ack)) == -1) {
		perror("Send ack failed");
		return;
	} 
	
	// receive file size
	if(read(s, &file_size, sizeof(file_size)) == -1) {
		perror("Receive file size error");
		return;
	}
	file_size = ntohl(file_size);

	// init hash
	if((hashd = mhash_init(MHASH_MD5)) == MHASH_FAILED) {
		perror("Init hash failed");
		return;
	}

	tic();
	// receive file
	while(counter < file_size) {
		if((file_read = read(s, file_buf, MAX_LINE)) == -1) {
			perror("Receive file error");
			return;
		}
		mhash(hashd, file_buf, file_read);
		fwrite(file_buf, 1, file_read, fp); // error handling
		counter += file_read;
	}
	trans_time = toc();
	trans_time = trans_time / 1000000;

	// receive MD5 hash
	if(read(s, file_hash, sizeof(file_hash)) == -1) {
		perror("Receive hash error");
		return;
	}	

	mhash_deinit(hashd, recv_hash);
	fclose(fp);
	
	// unsuccessful transfer--mismatched hashes
	if(strcmp(file_hash, recv_hash)) {
		perror("Mismatched hashes");
		// TODO: delete output file
		return;
	// successful transfer
	} else {
		if(write(s, &trans_time, sizeof(trans_time)) == -1) {
			perror("Send throughput error");
		}
		return;
	}
}

void cmd_del(int s) {
	int16_t fname_len;
	char file_name[MAX_LINE];
	int ack;
	struct stat file_stat;
	int fexists = 1;
	char confirm[MAX_LINE];

	// zero buffers	
	bzero((void*)file_name, sizeof(file_name));
	bzero((void*)&file_stat, sizeof(file_stat));

	if(read(s, &fname_len, sizeof(fname_len)) == -1) {
		perror("Receive file name length error");
		return;
	}
	if(read(s, &file_name, sizeof(file_name)) == -1) {
		perror("Receive file name error");
		return;
	}

	// check if file exists
	if(stat(file_name, &file_stat) == 0) {
		// send 1 to positive confirm
		ack = htons(1);
	} else {
		// send -1 to negative confirm
		ack = htons(-1);
		fexists = 0;
	}
	if(write(s, &ack, sizeof(ack)) == -1) {
		perror("Send confirmation error");
		return;
	}
	if(fexists == 0) {
		return;
	}
	
	// receive confirm from client
	if(read(s, confirm, MAX_LINE) == -1) {
		perror("Receive confirm code error");
		return;
	}
	if(!strcmp(confirm, "NO")) {
		return;
	} else if(!strcmp(confirm, "YES")) {
		// delete file and send confirmation
		if(remove(file_name) == -1) {
			perror("Delete file failed");
			return;
		}
		if(write(s, &ack, sizeof(ack)) == -1) {
			perror("Send confirmation error");
			return;
		}
	} else {
		perror("Invalid confirmation code");
		return;
	}
}

void cmd_lis(int s) {

}
