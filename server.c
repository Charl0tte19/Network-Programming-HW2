#include"server.h"

//這是用來定義用戶狀態
enum _state{
	OFFLINE,
	ONLINE,
	SEARCHING,
	REQUESTING,
	REQUESTED,
	PLAYING,
	NEXT_OR_NOT,
	NEXT_TRUE
};

//用戶狀態，用來在查詢時輸出
char STATUS_STRING[6][64] = {
	"Offline",
	"\033[1;32mOnline\033[0m",			//綠
	"\033[1;36mSearching\033[0m",		//青
	"\033[1;33mRequesting\033[0m",		//黃
	"\033[1;33mRequested\033[0m",		//黃	
	"\033[1;31mPlaying\033[0m"			//紅
};

//用戶資訊
typedef struct _AccountInfo{
	char username[USERNAME_BUF];	//用戶名
	char password[PASSWORD_BUF];	//密碼
	int fd;							//自己的fd
	int tfd;						//對手的fd
	int status;						//狀態
	int gid;						// game id
}AccountInfo;

//一場遊戲中的資訊
typedef struct _Game{
	char table[9];			//棋盤
	int playerfd[2];		//2位玩家
	int turn;				//0 or 1  (誰的回合)
	int on;					//是否有人在玩
}Game;

//讀入已存的用戶資料
void load_account(AccountInfo* a_info, int* a_Count){
	char line[USERNAME_BUF+PASSWORD_BUF];
	FILE* fp = fopen("account.txt", "r");						//存著帳戶密碼
	
    if(fp==NULL){
        printf("account.txt not found.\n");
        exit(1);
    }

    int count = 0;
	while(fgets(line, USERNAME_BUF+PASSWORD_BUF, fp) != NULL){	//讀account.txt的內容
		char* tmp = strstr(line, "\t");							//帳號和密碼是用\t隔開
		strncpy(a_info[count].username, line, tmp - &line[0]);	//取帳號名，[]優先級高於&，所以這是取到\t就停(不包含\t)
		tmp++;												
		char* tmp2 = strstr(tmp, "\n");							//取到\n就停(不包含\n)
		strncpy(a_info[count].password, tmp, tmp2 - tmp);		//取密碼

		count++;
	}
	*a_Count = count;
}

//從fd找用戶id
int fd_to_account_id(int fd, AccountInfo* a_info, int a_Count){
	int i;
	for(i = 0; i < a_Count; i++){
		if(a_info[i].fd == fd)
			return i;
	}
	return -1;
}

//從用戶名找用戶id
int username_to_account_id(char* username, AccountInfo* a_info, int a_Count){
	int i;
	for(i = 0; i < a_Count; i++){
		if(strcmp(username, a_info[i].username) == 0)
			return i;
	}
	return -1;
}

void draw_table(Game* games, int gid, char* tmp_table){
	sprintf(tmp_table, "-------\n|%c|%c|%c|\n-------\n|%c|%c|%c|\n-------\n|%c|%c|%c|\n-------\n", games[gid].table[0],
			games[gid].table[1], games[gid].table[2], games[gid].table[3], games[gid].table[4], games[gid].table[5], 
			games[gid].table[6], games[gid].table[7], games[gid].table[8]);
}

//處理各種client傳來的訊息
void handle(int fd, char* buffer, int len, AccountInfo* a_info, int a_Count, fd_set* fdset, Game* games, int* gameId){
	
	int cur_account_id = fd_to_account_id(fd, a_info, a_Count);
	
	if(strncmp(buffer, "LOGIN ", 6) == 0){	// handle login(this command is generate by client program, not user typing)
		/* parse from buffer */
		char username[USERNAME_BUF];
		char password[PASSWORD_BUF];
		memset(username, 0, USERNAME_BUF);
		memset(password, 0, PASSWORD_BUF);

		char* tmp = strstr(&buffer[6], " ");				//Client端會把用戶名和密碼一起傳來，中間用空格分開
		strncpy(username, &buffer[6], tmp - &buffer[6]);	
		username[tmp - &buffer[6]] = '\0';					
		tmp++;
		strncpy(password, tmp, &buffer[len] - tmp - 2);	//-2是忽略掉\n\0
		password[&buffer[len] - tmp - 2] = '\0';
		int i;
		
		// 確認這個帳號存不存在，密碼對不對，是否已是登入狀態 
		for(i = 0; i < a_Count; i++){
			if(strcmp(a_info[i].username, username) == 0 && strcmp(a_info[i].password, password) == 0 && a_info[i].status == OFFLINE){		
					write(fd, "LOGIN SUCCESS\n", 14);	//要傳回給client端
					a_info[i].status = ONLINE;
					a_info[i].fd = fd;					//client socket的fd
					printf("server: fd %d login as %s\n", fd, a_info[i].username);
					break;
			}
		}
		// 找不到該帳號
		if(i == a_Count)
			write(fd, "LOGIN FAIL", 10);	
	}
	//處理登入之外的訊息
	else{
		//該帳號為online狀態
		if(a_info[cur_account_id].status == ONLINE){
			//登出
			if(strncmp(buffer, "q\n", 2) == 0 || strncmp(buffer, "quit\n", 5) == 0){	
				a_info[cur_account_id].status = OFFLINE;
				printf("server: %s logout\n", a_info[cur_account_id].username);
				FD_CLR(fd, fdset);	//將fd從fdset中刪除
				close(fd);
			}
			//列出用戶
			else if(strncmp(buffer, "l\n", 2) == 0 || strncmp(buffer, "list\n", 5) == 0 ){
				char state_buffer[STATE_BUF];
				int i;
                strcpy(state_buffer, "Account\tStatus\n---------------\n");

				for(i = 0; i < a_Count; i++){
					if(a_info[i].status==OFFLINE)
						continue;
					else if(a_info[i].status<5){
						strcat(state_buffer,a_info[i].username);
						strcat(state_buffer,"\t");
						strcat(state_buffer,STATUS_STRING[a_info[i].status]);
						strcat(state_buffer,"\n");
					}
                    else{
						strcat(state_buffer,a_info[i].username);
						strcat(state_buffer,"\t");
						strcat(state_buffer,STATUS_STRING[PLAYING]);
						strcat(state_buffer,"\n");
                    }
				}	
				write(fd, state_buffer, strlen(state_buffer)+1);
			}
			//開始選擇對手
			else if(strncmp(buffer, "p\n", 2) == 0 || strncmp(buffer, "play\n", 5) == 0){
				a_info[cur_account_id].status = SEARCHING;
			}
			else
				write(fd, "Can't parse command.\n", 22);
			
		}
		//該帳號為SEARCHING狀態
		else if(a_info[cur_account_id].status == SEARCHING){		//因為client輸入的是用戶名
			char* tmp = strstr(buffer, "\n");		//這裡的buffer應該要是一個用戶名
			*tmp = '\0';
			int rid = username_to_account_id(buffer, a_info, a_Count);		// the requested account id
			
			if(rid != -1 && fd != a_info[rid].fd){
				char tmp[128];
				if(a_info[rid].status == ONLINE){
					sprintf(tmp, "Inviting %s to a game\n", buffer);
					write(fd, tmp, strlen(tmp)+1);					//傳給發出邀請的client
					a_info[cur_account_id].status = REQUESTING;
					a_info[cur_account_id].tfd = a_info[rid].fd; 	//記住該client正在邀請誰
					
					//設定被邀請者的狀態
					a_info[rid].status = REQUESTED;		
					a_info[rid].tfd = fd;		
					//因為要在對方client的新一行顯示
					sprintf(tmp, "\nInvitation from %s...\n", a_info[cur_account_id].username);
					write(a_info[rid].fd, tmp, strlen(tmp)+1);	//傳給要被邀請的client
				}
				else if(a_info[rid].status == SEARCHING){
					sprintf(tmp, "Request failed: %s is searching for opponent now.\n", buffer);
					write(fd, tmp, strlen(tmp)+1);
					a_info[cur_account_id].status = ONLINE;
				}
				else if(a_info[rid].status == REQUESTING){
					sprintf(tmp, "Request failed: %s is making a requesting to others now.\n", buffer);
					write(fd, tmp, strlen(tmp)+1);
					a_info[cur_account_id].status = ONLINE;
				}
				else if(a_info[rid].status == REQUESTED){
					sprintf(tmp, "Request failed: %s is being requested by others now.\n", buffer);
					write(fd, tmp, strlen(tmp)+1);
					a_info[cur_account_id].status = ONLINE;				
				}
				else if(a_info[rid].status == PLAYING){
					sprintf(tmp, "Request failed: %s is playing a game now.\n", buffer);
					write(fd, tmp, strlen(tmp)+1);
					a_info[cur_account_id].status = ONLINE;					
				}
                else if(a_info[rid].status == OFFLINE){
				    write(fd, "Request failed\n", 16);
				    a_info[cur_account_id].status = ONLINE;
                }
			}
			else{
				write(fd, "Request failed\n", 16);
				a_info[cur_account_id].status = ONLINE;
			}
		}
		//該帳號為REQUESTING狀態
		else if(a_info[cur_account_id].status == REQUESTING){
			int tfd = a_info[cur_account_id].tfd;
			int rid = fd_to_account_id(tfd, a_info, a_Count);
			
			//若對方一直不回應，我方也能拒絕邀請，而對方會看到request cancel
			if(strncmp(buffer, "c\n", 2) == 0 || strncmp(buffer, "cancel\n", 7) == 0){								
				a_info[cur_account_id].status = ONLINE;
				a_info[rid].status = ONLINE;
				write(tfd, "\nRequest canceled...\n", 22); 
			}
			else
				write(fd, "Can't parse command.\n", 22);
		}
		//該帳號為REQUESTED狀態
		else if(a_info[cur_account_id].status == REQUESTED){
			int tfd = a_info[cur_account_id].tfd;
			int rid = fd_to_account_id(tfd, a_info, a_Count);
			if(strncmp(buffer, "y\n", 2) == 0 || strncmp(buffer, "yes\n", 4) == 0){
				a_info[cur_account_id].status = PLAYING;
				a_info[rid].status = PLAYING;	//自己和對手都會變成PLAYING狀態

				*gameId = (*gameId+1)%MAX_GAME_NUM;		//新增一場遊戲
				while(games[*gameId].on==1){
					*gameId = (*gameId+1)%MAX_GAME_NUM;
				}
				games[*gameId].on = 1;
				
				a_info[cur_account_id].gid = *gameId;	//指定gameID給雙方
				a_info[rid].gid = *gameId;
				games[*gameId].playerfd[0] = fd;		//指定玩家，0是叉(被邀請方)，1是圈
				games[*gameId].playerfd[1] = tfd;
				
				int i;
				for(i = 0; i < 9; i++)
					games[*gameId].table[i] = '0' + 1 + i;		//填棋盤各格的位置編號
				games[*gameId].turn = 0;

				printf("server: fd %d and %d join game %d\n", fd, tfd, *gameId);

				char tmp[256];
				char tmp_table[128];
				draw_table(games,*gameId,tmp_table);
				//接受邀請方是先手
				sprintf(tmp, "Game #%d Start!\nYou are %c player.\n%sIt's your turn.\n", *gameId, 'x', tmp_table);
				write(fd, tmp, strlen(tmp)+1);
				//邀請方是後手
				sprintf(tmp, "\nGame #%d Start!\nYou are %c player.\n%sWaiting for %s's action.\n", *gameId, 'o', tmp_table, a_info[cur_account_id].username);
				write(tfd, tmp, strlen(tmp)+1);
			}
			//如果拒絕邀請
			else if(strncmp(buffer, "n\n", 2) == 0 || strncmp(buffer, "no\n", 3) == 0 ){
				a_info[cur_account_id].status = ONLINE;
				write(fd, "Reject invitation\n", 19);

				a_info[rid].status = ONLINE;
				write(tfd, "\nInvitation been rejected...\n", 30);
			}
			else
				write(fd, "Can't parse command.\n", 22);
		}
		//該帳號為PLAYING狀態
		else if(a_info[cur_account_id].status == PLAYING){
			int tfd = a_info[cur_account_id].tfd;
			int rid = fd_to_account_id(tfd, a_info, a_Count);
			int gid = a_info[cur_account_id].gid;
			int i;
			char *tmp = strstr(buffer,"\n");
			char command[256];
			strncpy(command, buffer, tmp - &buffer[0]);
			command[tmp - &buffer[0]] = '\0';
					
			if(i=atoi(command)){
				i -= 1;
				if(i>-1 && i<9){
					//是該client的回合
					if(fd == games[gid].playerfd[games[gid].turn]){
						if(games[gid].table[i] == '0' + i + 1){						// 該位置是否被填過
							games[gid].table[i] = games[gid].turn ? 'o' : 'x';		// insert to table
							games[gid].turn = !games[gid].turn;						// change turn							
						
							//傳送填好的棋盤
							char tmp_table[128];
							char tmp[256];
							draw_table(games,gid,tmp_table);
							sprintf(tmp, "You are %c player.\n%sWaiting for %s's action.\n", games[gid].playerfd[0] == fd ? 'x' : 'o', tmp_table, a_info[rid].username);
							//不要連\0一起傳，後面還有字串要傳
							write(fd, tmp, strlen(tmp));
							sprintf(tmp, "\nYou are %c player.\n%sIt's your turn.\n", games[gid].playerfd[0] == fd ? 'o' : 'x', tmp_table); //你是0號玩家，則對手就是圈
							write(tfd, tmp, strlen(tmp));
								
							//檢查遊戲是否結束
							char winner = ' ';		// ' ' 表示還無勝者, 't' 表示 tie(平局)
							if(games[gid].table[0] == games[gid].table[1] && games[gid].table[1] == games[gid].table[2])
								winner = games[gid].table[0];
							else if(games[gid].table[3] == games[gid].table[4] && games[gid].table[4] == games[gid].table[5])
								winner = games[gid].table[3];
							else if(games[gid].table[6] == games[gid].table[7] && games[gid].table[7] == games[gid].table[8])
								winner = games[gid].table[6];
							else if(games[gid].table[0] == games[gid].table[3] && games[gid].table[3] == games[gid].table[6])
								winner = games[gid].table[0];
							else if(games[gid].table[1] == games[gid].table[4] && games[gid].table[4] == games[gid].table[7])
								winner = games[gid].table[1];
							else if(games[gid].table[2] == games[gid].table[5] && games[gid].table[5] == games[gid].table[8])
								winner = games[gid].table[2];
							else if(games[gid].table[0] == games[gid].table[4] && games[gid].table[4] == games[gid].table[8])
								winner = games[gid].table[0];
							else if(games[gid].table[2] == games[gid].table[4] && games[gid].table[4] == games[gid].table[6])
								winner = games[gid].table[2];
							else if(games[gid].table[0] != '1' && games[gid].table[1] != '2' && games[gid].table[2] != '3' && games[gid].table[3] != '4' && 
									games[gid].table[4] != '5' && games[gid].table[5] != '6' && games[gid].table[6] != '7' && games[gid].table[7] != '8' && 
									games[gid].table[8] != '9')
								winner = 't';
							else
								winner = ' ';
								
							if(winner == 'o'){
                                //這裡寫入的內容會接在前面的write內容後,不是分開傳(所以不能用printf(),因為中間有\0,會印不完整)
                                write(games[gid].playerfd[1], "\033[31m\n--- You win ---\n\033[0m", strlen("\033[31m\n--- You win ---\n\033[0m")+1);
								write(games[gid].playerfd[0], "\033[32m\n--- You lose ---\n\033[0m", strlen("\033[32m\n--- You lose ---\n\033[0m")+1);
							}
							else if(winner == 'x'){
								write(games[gid].playerfd[0], "\033[31m\n--- You win ---\n\033[0m", strlen("\033[31m\n--- You win ---\n\033[0m")+1);
								write(games[gid].playerfd[1], "\033[32m\n--- You lose ---\n\033[0m", strlen("\033[32m\n--- You lose ---\n\033[0m")+1);
                            }
							else if(winner == 't'){
								write(games[gid].playerfd[0], "\033[36m\n--- Tie ---\n\033[0m", strlen("\033[33m\n--- Tie ---\n\033[0m")+1);
								write(games[gid].playerfd[1], "\033[36m\n--- Tie ---\n\033[0m", strlen("\033[33m\n--- Tie ---\n\033[0m")+1);
							}
								
							if(winner != ' '){
								games[gid].on = 0;
								a_info[cur_account_id].status = NEXT_OR_NOT;
								a_info[rid].status = NEXT_OR_NOT;
							}
                            else{
                                write(fd, "\0", 1);
                                write(tfd, "\0", 1);
                            }
						}
						else
							write(fd, "Can't choose this position!\n", 29);
					}
					//非該client的回合
					else
						write(fd, "It's not your turn!\n", 21);
				}
				else
					write(fd, "Can't parse command.\n", 22);
			}
			//對話
			else if(strncmp(command,"@",1)==0){
				char dialog[256];
                strcpy(dialog,&command[1]);
				char message[360];
				sprintf(message,"\033[1;36mSend message...\n\033[0m");
				write(fd, message, strlen(message)+1);
				sprintf(message,"\033[1;36m\n[%s]: %s\n\033[0m", a_info[cur_account_id].username, dialog);
				write(tfd, message, strlen(message)+1);
			}
            else if(strncmp(buffer,"p\n", 2)==0){
				char tmp_table[128];
				char tmp[256];
				draw_table(games,gid,tmp_table);
			    if(fd == games[gid].playerfd[games[gid].turn])
				    sprintf(tmp, "You are %c player.\n%sIt's your turn.\n", games[gid].playerfd[0] == fd ? 'x' : 'o', tmp_table, a_info[rid].username);
                else
				    sprintf(tmp, "You are %c player.\n%sWaiting for %s's action.\n", games[gid].playerfd[0] == fd ? 'x' : 'o', tmp_table, a_info[rid].username);
				write(fd, tmp, strlen(tmp)+1);
            }
			else if(strncmp(buffer,"l\n", 6)==0){
				a_info[cur_account_id].status = ONLINE;
				a_info[rid].status = ONLINE;
				write(tfd, "\nYour opponent leaves the game.\n", 33);				
			}
			else
				write(fd, "Can't parse command.\n", 22);
		}
		//該帳號為NEXT_OR_NOT的狀態
		else if(a_info[cur_account_id].status == NEXT_OR_NOT){
			int tfd = a_info[cur_account_id].tfd;
			int rid = fd_to_account_id(tfd, a_info, a_Count);	
            
            if(strncmp(buffer, "y\n", 2) == 0){
				if(a_info[rid].status == NEXT_OR_NOT){
					write(fd, "...\n", 5); 
					a_info[cur_account_id].status = NEXT_TRUE;
                    
				}
				else if(a_info[rid].status == NEXT_TRUE){
					write(fd, "\033[1;33mNew Game!\n\033[0m", strlen("\033[1;33mNew Game!\n\033[0m"));
					write(tfd, "\033[1;33m\nNew Game!\n\033[0m", strlen("\033[1;33m\nNew Game!\n\033[0m"));
					a_info[cur_account_id].status = PLAYING;
					a_info[rid].status = PLAYING;
				
					*gameId = (*gameId+1)%MAX_GAME_NUM;		//新增一場遊戲				
					while(games[*gameId].on==1){
						*gameId = (*gameId+1)%MAX_GAME_NUM;
					}
					games[*gameId].on = 1;
					
					a_info[cur_account_id].gid = *gameId;	//指定gameID給雙方
					a_info[rid].gid = *gameId;
					games[*gameId].playerfd[0] = fd;		//指定玩家，0是叉(被邀請方)，1是圈
					games[*gameId].playerfd[1] = tfd;
				
					int i;
					for(i = 0; i < 9; i++)
						games[*gameId].table[i] = '0' + 1 + i;		//填棋盤各格的位置編號
					games[*gameId].turn = 0;

					printf("server: fd %d and %d start a new game %d\n", fd, tfd, *gameId);

					char tmp[256];
					char tmp_table[128];
					draw_table(games,*gameId,tmp_table);
					sprintf(tmp, "Game #%d Start!\nYou are %c player.\n%sIt's your turn.\n", *gameId, 'x', tmp_table);
					write(fd, tmp, strlen(tmp)+1);
					sprintf(tmp, "Game #%d Start!\nYou are %c player.\n%sWaiting for %s's action.\n", *gameId, 'o', tmp_table, a_info[cur_account_id].username);
					write(tfd, tmp, strlen(tmp)+1);
				}
			}
			else if(strncmp(buffer, "n\n", 2) == 0){
				a_info[cur_account_id].status = ONLINE;
				a_info[rid].status = ONLINE;
				write(tfd, "\nYour opponent leaves the game.\n", 33);
			}
			else
				write(fd, "Can't parse command.\n", 22);
		}
		//該帳號為NEXT_TRUE的狀態
		else if(a_info[cur_account_id].status == NEXT_TRUE){
			int tfd = a_info[cur_account_id].tfd;
			int rid = fd_to_account_id(tfd, a_info, a_Count);	
			if(strncmp(buffer, "c\n", 2) == 0 || strncmp(buffer, "cancel\n", 7) == 0){								
				a_info[cur_account_id].status = ONLINE;
				a_info[rid].status = ONLINE;
				write(tfd, "\nRequest canceled...\n", 22); 
			}
			else
				write(fd, "Can't parse command.\n", 22);
		}

	}	
}	
			
			
int main(){
	
	printf("Configuring local address...\n");
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));	//將hints初始化為0
	hints.ai_family = AF_INET;			//AF_INET表示我們用IPv4 address
	hints.ai_socktype = SOCK_STREAM;	//SOCK_STREAM表示我們用TCP
    hints.ai_flags = AI_PASSIVE;		//設定這個會告訴getaddrinfo，我們要bind wildcard address
	
	struct addrinfo *bind_address;
	
	getaddrinfo(NULL, "8765", &hints, &bind_address);	//我們用getaddrinfo來設定addrinfo。第一參數可填網址(域名)或IP，在這裡我們沒有要指定網卡，所以填0(NULL)
														//第二參數是我們要從哪個port監聽連線，可填協定(如http)或port號，hints用來指定一些基本項目(見上)
														//bind_address會指向設定好的結果，總之這個addrinfo是提供給bind()的資訊
		
	
	printf("Creating socket...\n");												
	int socket_listen;
    socket_listen = socket(bind_address->ai_family,					//bind_address的內容是getaddrinfo()根據我們提供的資訊，自行設定的。
            bind_address->ai_socktype, bind_address->ai_protocol); 	//另外，雖然這邊是用bind_address的內部資料，但其實它和bind_address本身沒什麼關係
																	//它就是要取那個資料而已，直接填AF_INET也是一樣的
	if (socket_listen<0) {				
        fprintf(stderr, "socket() failed. (%d)\n", errno);			
        exit(1);
    }

    printf("Binding socket to local address...\n");
	//bind成功回傳0，否則回傳非0
    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {		
        fprintf(stderr, "bind() failed. (%d)\n", errno);
        exit(1);
    }
	
	freeaddrinfo(bind_address);		//綁好後就可以將addrinfo釋放記憶體了
	
	printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {		//這裡的10是指最多能queue住10個connections，再多的就拒絕掉
        fprintf(stderr, "listen() failed. (%d)\n", errno);
        exit(1);
    }

	fd_set fdset,readset;				//存著listener和clients的fd set和存readable fd的readset
	int maxfd = socket_listen;		
	
	//用戶陣列
	AccountInfo a_info[MAX_ACCOUNT_NUM];
	//將陣列的內容(包含struct)都初始化為0
	memset(a_info, 0, sizeof(a_info));
	
	//用戶數
	int a_Count;
	
	//從account.txt載入用戶
	load_account(a_info, &a_Count);

	//最多同時進行16局比賽
	Game games[MAX_GAME_NUM];
	int gameId = -1;

	FD_ZERO(&fdset);					//初始化fdset為0
	FD_SET(socket_listen, &fdset);		//將listener加到fdset
	char buffer[BUF_FOR_CLIENT];		//buffer read from client	

	while(1){
		
		//structure可用=做淺複製
		readset = fdset;		// file destriptor that can read
		
		//看select()有無出錯，沒出錯的話，readable的fd會存在readset中
		if(select(maxfd + 1, &readset, NULL, NULL, NULL) == -1){
			fprintf(stderr, "select() failed\n");
			break;
		}
		
		int i = 0;
		for(i = 0; i <= maxfd; i++){
			if(FD_ISSET(i, &readset)){	 		// fd i在readset的話
				if(i == socket_listen){			// new connection happend
					int socket_client = accept(socket_listen, NULL, NULL);		//NULL，client的addr資訊不存
								
					if (socket_client<0) {
						fprintf(stderr, "accept() failed. (%d)\n", errno);
						exit(1);
					}
					printf("server: fd %d connect\n", socket_client);
					FD_SET(socket_client, &fdset);		//socket_client
					if(socket_client > maxfd)				//更新maxfd
						maxfd = socket_client;
							
				}
				//i是client
				else{	
					int len = read(i, buffer, BUF_FOR_CLIENT);	//把i的內容讀到buffer
					if(len > 0){
						printf("fd %d: ", i);	//server印出client傳了什麼
						printf("%s",buffer);
						handle(i, buffer, len, a_info, a_Count, &fdset, games, &gameId);	//處理這個message
					}
					// len=0表示client意外斷線，比如client端ctrl+C
					else{	
						FD_CLR(i, &fdset);	//把i從fdset刪除
						close(i);
						printf("server: fd %d disconnect\n", i);
						
						int cur_account_id = fd_to_account_id(i, a_info, a_Count);
						//要重設定該client和其對手(有的話)的狀態
						if(cur_account_id != -1){		
							if(a_info[cur_account_id].status == REQUESTING || a_info[cur_account_id].status == REQUESTED || a_info[cur_account_id].status == PLAYING ||
							   a_info[cur_account_id].status == NEXT_OR_NOT || a_info[cur_account_id].status == NEXT_TRUE){
								int tfd = a_info[cur_account_id].tfd;
								a_info[fd_to_account_id(tfd, a_info, a_Count)].status = ONLINE;		//因為其中一方掉線了，也把對方變回online
								char tmp[32];
								sprintf(tmp, "\nAlert: %s logout.\n", a_info[cur_account_id].username);	//並傳送對方登出的訊息
								write(tfd, tmp, strlen(tmp)+1);
							}
							a_info[cur_account_id].status = OFFLINE;
							printf("server: %s auto logout.\n", a_info[cur_account_id].username);
						}
					}
				}	
			}			
		
		}
		
	}

}
