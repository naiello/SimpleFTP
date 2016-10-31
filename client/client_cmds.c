#include <ctype.h>
#include <error.h>
#include <errno.h>
#include <mhash.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../common/cmd_defs.h"
#include "client_cmds.h"
#include "../common/timing.h"

#define RECV_BUF_SZ 4096
#define SEND_BUF_SZ 4096

// Helper function to print a MD5 hash to stdout
static void print_md5(const char *hash)
{
	const unsigned char *uhash = (const unsigned char *)hash;
	int i;
	for (i = 0; i < 16; i++) {
		printf("%02x", uhash[i]);
	}
}

int ftpc_request(int sockfd, const char *arg, size_t arglen)
{
	char recv_buf[RECV_BUF_SZ];
	char file_hash[16];
	char recv_hash[16];
	MHASH hashd;
	int32_t file_size;
	int16_t send_code;
	FILE *outfd;
	size_t recv_counter = 0;
	size_t recv_current;
	unsigned long transfer_time;
	double transfer_secs;

	// send request to server and wait for ACK (short int > 0)
	send_code = htons(CMD_REQ);
	if (send(sockfd, &send_code, sizeof(send_code), 0) < 0) {
		fprintf(stderr, "Failed to send!\n");
		return -1;
	}

	// send the size of the file name followed by the file name
	send_code = htons((int16_t)arglen);
	if ((send(sockfd, &send_code, sizeof(send_code), 0) < sizeof(send_code)) ||
			(send(sockfd, arg, arglen, 0) < arglen)) {

		fprintf(stderr, "Failed to send!\n");
		return -3;
	}

	// receive the file size from the server and ensure file exists
	if (recv(sockfd, &file_size, sizeof(file_size), 0) < sizeof(file_size)) {
		fprintf(stderr, "Corrupt recv!\n");
		return -4;
	}

	file_size = ntohl(file_size);
	if (file_size < 0) {
		fprintf(stderr, "File %s does not exist.", arg);
		return -5;
	}

	// receive md5 hash of the file from server
	if (recv(sockfd, file_hash, sizeof(file_hash), 0) < sizeof(file_hash)) {
		fprintf(stderr, "Corrupt hash.\n");
		return -6;
	}

	// open output file
	if (!(outfd = fopen(arg, "w"))) {
		fprintf(stderr, "Couldn't open %s for writing: %s", 
				arg, strerror(errno));
		return -7;
	}

	// init the hash object
	if ((hashd = mhash_init(MHASH_MD5)) == MHASH_FAILED) {
		fprintf(stderr, "Couldn't initialize hash.\n");
		return -8;
	}

	// read the file in blocks from the server
	tic();
	while (recv_counter < file_size) {
		memset(recv_buf, 0, sizeof(recv_buf));
		if ((recv_current = recv(sockfd, recv_buf, sizeof(recv_buf), 0)) < 0) {
			fprintf(stderr, "Bad read.\n");
			continue;
		}

		fwrite(recv_buf, sizeof(char), recv_current, outfd);
		mhash(hashd, recv_buf, recv_current);
		recv_counter += recv_current;
	}
	transfer_time = toc();
	transfer_secs = transfer_time / 1.0e6;

	// close file and compute the hash
	mhash_deinit(hashd, recv_hash);
	fclose(outfd);

	// compare hashes to ensure correct transfer
	if (strncmp(recv_hash, file_hash, 16)) {
		fprintf(stderr, "Transfer failed (hash mismatch)\n");
		print_md5(recv_hash);
		printf("\n");
		print_md5(file_hash);
		printf("\n");
		unlink(arg);
		return -9;
	}

	printf("Transfer successful.\n");
	printf("Total Bytes: %d Transfer Time: %.2f Rate: %.2f\n", 
			file_size, transfer_secs, file_size / transfer_secs);
	printf("MD5: ");
	print_md5(file_hash);
	printf("\n");
	return 0;
}

int ftpc_upload(int sockfd, const char *arg, size_t arglen)
{
	int16_t op_code = htons(CMD_UPL);
	int16_t fname_sz = htons((int16_t)arglen);
	int16_t ack_code = 0;
	FILE *infd;
	MHASH hashd;
	struct stat file_info;
	char send_buf[SEND_BUF_SZ];
	char hash_buf[16];
	uint32_t file_size;
	size_t send_counter = 0, read_current, send_current;
	unsigned long transfer_time;
	double transfer_secs;

	memset(&file_info, 0, sizeof(struct stat));
	// make sure file exists
	if (stat(arg, &file_info) < 0) {
		fprintf(stderr, "Couldn't stat %s: %s\n", arg, strerror(errno));
		return -4;
	}
	file_size = htonl((uint32_t)file_info.st_size);
	
	// send operation, file name length and file name to server
	if (send(sockfd, &op_code, sizeof(op_code), 0) < 0 ||
			send(sockfd, &fname_sz, sizeof(fname_sz), 0) < 0 ||
			send(sockfd, arg, arglen, 0) < 0) {
		fprintf(stderr, "Failed to init transfer.\n");
		return -1;
	}

	// wait for ACK (int16 > 0)
	if (recv(sockfd, &ack_code, sizeof(ack_code), 0) < 0 ||
			ack_code <= 0) {
		fprintf(stderr, "Server refused transfer.\n");
		return -2;
	}

	// send file size
	if (send(sockfd, &file_size, sizeof(file_size), 0) < 0) {
		fprintf(stderr, "Failed to send file size\n");
		return -2;
	}
	file_size = ntohl(file_size);

	// init mhash
	if ((hashd = mhash_init(MHASH_MD5)) == MHASH_FAILED) {
		fprintf(stderr, "Failed to init mhash.\n");
		return -3;
	}

	// compute md5 hash of the file
	infd = fopen(arg, "r");
	while (send_counter < file_size) {
		memset(send_buf, 0, sizeof(send_buf));
		if ((read_current = fread(send_buf, sizeof(char), 
						sizeof(send_buf), infd)) < 0) {
			fprintf(stderr, "Bad read.\n");
			continue;
		}

		mhash(hashd, send_buf, read_current);
		send_counter += read_current;
	}

	mhash_deinit(hashd, hash_buf);
	if ((send(sockfd, hash_buf, sizeof(hash_buf), 0)) < 0) {
		fprintf(stderr, "Failed to transmit hash.\n");
		return -5;
	}

	// back to the beginning of the file
	rewind(infd);
	send_counter = 0;

	// begin transmitting file
	tic();
	while (send_counter < file_size) {
		memset(send_buf, 0, sizeof(send_buf));
		if ((read_current = fread(send_buf, sizeof(char),
						sizeof(send_buf), infd)) < 0) {
			fprintf(stderr, "Bad read.\n");
			continue;
		}

		if ((send_current = send(sockfd, send_buf, read_current, 0))
				!= read_current) {
			fprintf(stderr, "Bad send.\n");
			continue;
		}

		send_counter += send_current;
	}
	transfer_time = toc();
	transfer_secs = transfer_time / 1.0e6;

	fclose(infd);

	// wait for the server to ack the transfer
	ack_code = 0;
	if (recv(sockfd, &ack_code, sizeof(ack_code), 0) < 0) {
		fprintf(stderr, "Couldn't read ACK\n");
		return -6;
	}

	// make sure the server received the file correctly
	ack_code = ntohs(ack_code);
	if (ack_code <= 0) {
		fprintf(stderr, "Transfer failed (hash mismatch).\n");
		return -7;
	}

	printf("Upload successful!\n");
	printf("Total Bytes: %d Transfer Time: %.2f s Rate: %.2f bytes/s\n", 
			file_size, transfer_secs, file_size / transfer_secs);
	printf("MD5: ");
	print_md5(hash_buf);
	printf("\n");
	return 0;
}

int ftpc_list(int sockfd)
{
	int16_t op_code = htons(CMD_LIS);
	int32_t list_size;
	size_t recv_current, recv_counter = 0;
	char recv_buf[RECV_BUF_SZ];

	// send opcode LIS
	if (send(sockfd, &op_code, sizeof(op_code), 0) < 0) {
		fprintf(stderr, "Failed to send opcode.\n");
		return -1;
	}

	// receive the list size from the server
	if (recv(sockfd, &list_size, sizeof(list_size), 0) < 0) {
		fprintf(stderr, "Failed to recv list size.\n");
		return -2;
	}

	list_size = ntohl(list_size);
	if (list_size < 0) {
		fprintf(stderr, "Server error.\n");
		return -3;
	}

	// receive the transmitted list
	memset(recv_buf, 0, sizeof(recv_buf));
	
	while (recv_counter < list_size) {
		if ((recv_current = recv(sockfd, recv_buf, list_size, 0)) < 0) {
			fprintf(stderr, "Bad recv.\n");
			continue;
		}

		printf("%s", recv_buf);
		recv_counter += recv_current;
	}

	return 0;
}

int ftpc_mkdir(int sockfd, const char *arg, size_t arglen)
{
	int16_t op_code = htons(CMD_MKD);
	int16_t dirname_size = htons((int16_t)arglen);
	int16_t ack_code = 0;

	// send opcode MKD, directory name length, directory name
	if ((send(sockfd, &op_code, sizeof(op_code), 0) < 0) ||
			(send(sockfd, &dirname_size, sizeof(dirname_size), 0) < 0) ||
			(send(sockfd, arg, arglen, 0) < 0)) {
		fprintf(stderr, "Failed to send request.\n");
		return -3;
	}

	// wait for ACK
	if (recv(sockfd, &ack_code, sizeof(ack_code), 0) < 0) {
		fprintf(stderr, "No ACK from server.");
		return -4;
	}
	ack_code = ntohs(ack_code);

	switch (ack_code) {
		case 1:
			printf("Directory created!\n");
			return 0;
		case -1:
			fprintf(stderr, "Directory creation failed.\n");
			return -1;
		case -2:
			fprintf(stderr, "Directory already exists on server.\n");
			return -2;
		default:
			fprintf(stderr, "Invalid ACK.\n");
			return -5;
	}
}

int ftpc_rmdir(int sockfd, const char *arg, size_t arglen)
{
	int16_t op_code = htons(CMD_RMD);
	int16_t dirname_size = htons((int16_t)arglen);
	int16_t ack_code = 0;
	int16_t confirm_code;
	char confirm_inp[5];

	// send opcode MKD, directory name length, directory name
	if ((send(sockfd, &op_code, sizeof(op_code), 0) < 0) ||
			(send(sockfd, &dirname_size, sizeof(dirname_size), 0) < 0) ||
			(send(sockfd, arg, arglen, 0) < 0)) {
		fprintf(stderr, "Failed to send request.\n");
		return -1;
	}

	if (recv(sockfd, &ack_code, sizeof(ack_code), 0) < 0) {
		fprintf(stderr, "Failed to receive ACK.\n");
		return -2;
	}

	if (ack_code == -1) {
		fprintf(stderr, "Directory does not exist on the server.\n");
		return -3;
	} else if (ack_code == -2) {
		fprintf(stderr, "Directory not empty!\n");
		return -3;
	}

	char *iter;
	while (1) {
		printf("Are you sure you want to delete %s? [Yes/No]: ", arg);
		fgets(confirm_inp, sizeof(confirm_inp), stdin);
		
		// convert to upper case
		for (iter = confirm_inp; *iter != 0; iter++) {
			*iter = toupper(*iter);
		}

		if (!strcmp(confirm_inp, "YES\n")) { 
			break;
		} else if (!strcmp(confirm_inp, "NO\n")) {
			fprintf(stderr, "Delete aborted by user.\n");
			// tell server to abort
			confirm_code = htons(-1);
			send(sockfd, &confirm_code, sizeof(confirm_code), 0);
			return -4;
		}
	}

	confirm_code = htons(1);
	if (send(sockfd, &confirm_code, sizeof(confirm_code), 0) < 0) {
		fprintf(stderr, "Failed to send confirm.\n");
		return -5;
	}

	if (recv(sockfd, &ack_code, sizeof(ack_code), 0) < 0) {
		fprintf(stderr, "Failed to receive ACK.\n");
		return -6;
	}

	ack_code = ntohs(ack_code);
	if (ack_code < 0) {
		fprintf(stderr, "Failed to delete directory.\n");
		return -7;
	}

	printf("Directory deleted.\n");
	return 0;
}

int ftpc_chdir(int sockfd, const char *arg, size_t arglen)
{
	int16_t op_code = htons(CMD_CHD);
	int16_t dirname_size = htons((int16_t)arglen);
	int16_t ack_code = 0;

	// send opcode CHD, directory name length, directory name
	if ((send(sockfd, &op_code, sizeof(op_code), 0) < 0) ||
			(send(sockfd, &dirname_size, sizeof(dirname_size), 0) < 0) ||
			(send(sockfd, arg, arglen, 0) < 0)) {
		fprintf(stderr, "Failed to send request.\n");
		return -3;
	}

	// wait for ACK
	if (recv(sockfd, &ack_code, sizeof(ack_code), 0) < 0) {
		fprintf(stderr, "No ACK from server.");
		return -4;
	}
	ack_code = ntohs(ack_code);

	switch (ack_code) {
		case 1:
			printf("Directory changed!\n");
			return 0;
		case -1:
			fprintf(stderr, "Directory change failed.\n");
			return -1;
		case -2:
			fprintf(stderr, "Directory does not exist on server.\n");
			return -2;
		default:
			fprintf(stderr, "Invalid ACK.\n");
			return -5;
	}

	return 0;
}

int ftpc_delete(int sockfd, const char *arg, size_t arglen)
{
	int16_t op_code = htons(CMD_DEL);
	int16_t dirname_size = htons((int16_t)arglen);
	int16_t ack_code = 0;
	int16_t confirm_code;
	char confirm_inp[5];

	// send opcode RMD, directory name length, directory name
	if ((send(sockfd, &op_code, sizeof(op_code), 0) < 0) ||
			(send(sockfd, &dirname_size, sizeof(dirname_size), 0) < 0) ||
			(send(sockfd, arg, arglen, 0) < 0)) {
		fprintf(stderr, "Failed to send request.\n");
		return -1;
	}

	if (recv(sockfd, &ack_code, sizeof(ack_code), 0) < 0) {
		fprintf(stderr, "Failed to receive ACK.\n");
		return -2;
	}

	if (ntohs(ack_code) == -1) {
		fprintf(stderr, "File does not exist on the server.\n");
		return -3;
	}

	char *iter;
	while (1) {
		printf("Are you sure you want to delete %s? [Yes/No]: ", arg);
		fgets(confirm_inp, sizeof(confirm_inp), stdin);
		
		// convert to upper case
		for (iter = confirm_inp; iter < (confirm_inp + 3); iter++) {
			*iter = toupper(*iter);
		}

		if (!strcmp(confirm_inp, "YES\n")) { 
			break;
		} else if (!strcmp(confirm_inp, "NO\n")) {
			fprintf(stderr, "Delete aborted by user.\n");
			// tell server to abort
			confirm_code = htons(-1);
			send(sockfd, &confirm_code, sizeof(confirm_code), 0);
			return -4;
		}
	}

	confirm_code = htons(1);
	if (send(sockfd, &confirm_code, sizeof(confirm_code), 0) < 0) {
		fprintf(stderr, "Failed to send confirm.\n");
		return -5;
	}

	if (recv(sockfd, &ack_code, sizeof(ack_code), 0) < 0) {
		fprintf(stderr, "Failed to receive ACK.\n");
		return -6;
	}

	ack_code = ntohs(ack_code);
	if (ack_code < 0) {
		fprintf(stderr, "Failed to delete file.\n");
		return -7;
	}

	printf("File deleted.\n");
	return 0;
}
