#ifndef CLIENT_CMDS_H_
#define CLIENT_CMDS_H_

int ftpc_request(int, const char *, size_t);
int ftpc_upload(int, const char *, size_t);
int ftpc_delete(int, const char *, size_t);
int ftpc_list(int);
int ftpc_mkdir(int, const char *, size_t);
int ftpc_chdir(int, const char *, size_t);
int ftpc_rmdir(int, const char *, size_t);

#endif
