#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/select.h>

#define USERNAME_BUF 64
#define	PASSWORD_BUF 64
#define MAX_ACCOUNT_NUM 10
#define MAX_GAME_NUM 16
#define BUF_FOR_CLIENT 1024
#define STATE_BUF 2048

//.c檔內的write()看情況決定是否傳\0,有時需讓整個buf是一個string
