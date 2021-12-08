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
#define BUF_FOR_SERVER 2048
#define INPUT_BUF 1024