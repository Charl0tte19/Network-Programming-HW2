#include"client.h"

//這是用來定義用戶狀態
enum _state{
	ONLINE,
	SEARCHING,
	REQUESTING,
	REQUESTED,
	PLAYING,
	NEXT_OR_NOT,
	NEXT_TRUE
};

int main(){
	char username[USERNAME_BUF];
	char password[PASSWORD_BUF];
	
	//printf("Configuring local address...\n");
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));	//將hints初始化為0
	hints.ai_family = AF_INET;			//AF_INET表示我們用IPv4 address
	hints.ai_socktype = SOCK_STREAM;	//SOCK_STREAM表示我們用TCP
    
	struct addrinfo *bind_address;
	getaddrinfo("127.0.0.1", "8765", &hints, &bind_address);
	
	//printf("Creating socket...\n");												
	int sockfd;
    sockfd = socket(bind_address->ai_family,						//bind_address的內容是getaddrinfo()根據我們提供的資訊，自行設定的。
            bind_address->ai_socktype, bind_address->ai_protocol); 	//另外，雖然這邊是用bind_address的內部資料，但其實它和bind_address本身沒什麼關係
																	//它就是要取那個資料而已，直接填AF_INET也是一樣的
	if (sockfd<0) {				
        fprintf(stderr, "socket() failed. (%d)\n", errno);			
        exit(1);
    }
	
	//printf("Connecting...\n");
    if (connect(sockfd, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", errno);
        exit(1);
    }
    freeaddrinfo(bind_address);
	
	int maxfd = sockfd;
	
	//讓client輸入帳號密碼
	printf("Username: ");
	fgets(username, USERNAME_BUF, stdin);
	username[strlen(username) - 1] = '\0';
	printf("Password: ");
	fgets(password, PASSWORD_BUF, stdin);
	password[strlen(password) - 1] = '\0';	
	
	char line[INPUT_BUF];
	memset(line,0,sizeof(line));
	
	//登入測試
	sprintf(line, "LOGIN %s %s\n", username, password);
	write(sockfd, line, strlen(line)+1);
	
	char buf[BUF_FOR_SERVER];		// read buffer from server
	memset(buf,0,sizeof(buf));
    read(sockfd, buf, BUF_FOR_SERVER);
	if(strncmp(buf, "LOGIN FAIL",10) == 0){
		fprintf(stderr, "login failed!\n");
		close(sockfd);
		exit(1);
	}
	
	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(sockfd, &fdset);
	FD_SET(0, &fdset);	//0是stdin
	
	//指令們
	char prompt[7][128] = {
		"\033[33m<(p)lay\t(l)ist\t(q)uit> \033[0m",													//for online
		"\033[33m<Search user account> \033[0m",													//for searching
		"\033[33m<Waiting for accept... (c)ancel> \033[0m",											//for requesting
		"\033[33m<Accept? (y)es\t(n)o> \033[0m",													//for requested
		"\033[33m<choose position (1~9)\t(p)rint table\tSend message (@[text])\t(l)eave game>\n\033[0m",  //for playing
		"\033[33m<Do you want to start a new game? (y/n)> \033[0m",									//for next_or_not
		"\033[33m<Waiting for your opponent... (c)ancel> \033[0m"									//for next_true
	};

	int curPrompt = ONLINE;
	printf("%s", prompt[curPrompt]);
    fflush(stdout);

	while(1){	
		fd_set readset = fdset;
		struct timeval tv = {0, 0};		// let select() return immediately
		if(select(maxfd + 1, &readset, NULL, NULL, &tv) == -1){
			fprintf(stderr, "select() failed\n");
			break;
		}

		if(FD_ISSET(sockfd, &readset)){		// if get something from server
			int len = read(sockfd, buf, BUF_FOR_SERVER);
			//server斷線
			if(len<=0)
				break;
			
			//印出server傳來的訊息
            printf("%s",buf);
            //write(1, buf, len);

			if(curPrompt == SEARCHING){
				if(strncmp(buf, "Request failed", 14) == 0)
					curPrompt = ONLINE;
				else
					curPrompt = REQUESTING;
			}
			else if(strncmp(buf, "\nInvitation from", 16) == 0)
				curPrompt = REQUESTED;
			else if(strncmp(buf, "\nRequest canceled...\n", 21) == 0)
				curPrompt = ONLINE;
			else if(strncmp(buf, "\nInvitation been rejected...\n", 29) == 0){
				curPrompt = ONLINE;
			}
			else if(strncmp(buf, "Game", 4) == 0 || strncmp(buf, "\nGame", 5)==0)
				curPrompt = PLAYING;
			else if(strstr(buf,"@")==NULL && (strstr(buf, "--- You") != NULL || strstr(buf, "--- Tie") != NULL)){
                curPrompt = NEXT_OR_NOT;
            }
			//對手掉線
			else if(strncmp(buf, "\nAlert:",7) == 0)
				curPrompt = ONLINE;
			else if(strncmp(buf, "...\n",4)==0){
                curPrompt = NEXT_TRUE;
            }
			else if(curPrompt == NEXT_TRUE && strstr(buf, "\nNew Game")!=NULL)
				curPrompt = PLAYING;
			else if(curPrompt == NEXT_OR_NOT && strstr(buf, "New Game")!=NULL)
				curPrompt = PLAYING;
			else if(strncmp(buf,"\nYour opponent",14)==0)
				curPrompt = ONLINE;

			printf("%s", prompt[curPrompt]);		// prompt again
            fflush(stdout);
			
		}
		else if(FD_ISSET(0, &readset)){			// if get something from user input
            fgets(line, INPUT_BUF, stdin);

			write(sockfd, line, strlen(line)+1);	// write user input to server

			if(curPrompt == ONLINE && strncmp(line, "p\n", 2) == 0 || strncmp(line, "play\n", 5) == 0){
				curPrompt = SEARCHING;
				printf("%s", prompt[curPrompt]);
			}
			else if(curPrompt == ONLINE && strncmp(line, "q\n", 2) == 0 || strncmp(line, "quit\n", 5) == 0){
				printf("logout\n");
				break;
			}
			else if((curPrompt == REQUESTING || curPrompt == NEXT_TRUE) && (strncmp(line, "c\n", 2) == 0 || strncmp(line, "cancel\n", 7) == 0)){
				printf("Request canceled\n");
				curPrompt = ONLINE;
				printf("%s", prompt[curPrompt]);		
			}
			else if(curPrompt == REQUESTED && (strncmp(line, "n\n", 2) == 0 || strncmp(line, "no\n", 3) == 0))
				curPrompt = ONLINE;
			else if(curPrompt == NEXT_OR_NOT && strncmp(line, "n\n", 2) == 0){
                curPrompt = ONLINE;
                printf("%s", prompt[curPrompt]);
            }
			else if(curPrompt == PLAYING && strncmp(line, "l\n", 6) == 0 ){
				curPrompt = ONLINE;
				printf("%s", prompt[curPrompt]);
			}
			
            fflush(stdout);
		}
		
	}

	close(sockfd);

	return 0;
	
}
