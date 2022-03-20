#define maxstations 10
#define hello 0
#define asksong 1
#define upsong 2
#define expectingwelcome 3
#define invalid 4
#define expectingannounce 5
#define idle 6
#define newstation 7
#define expectingpermit 8
#define welcome 9
#define up 10
#define multiplewelcome 11
#define straypermit 12
#define strayannounce 13
#define generic 14
#define upload 15
#define quitting -1


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <ctype.h>

int port;
int uport;
int globalstate;
char ip[15];
char mip[15];
int station;
int numofstations;
int songsize;
char songname[100];



void *udpart (void* arg)
{
	char buffer[1024], calc[100], sip[15];
	int sock[maxstations], status, socklen, current_station,i, iplast, rsize;
	unsigned char ttl = (unsigned char) 255;
	struct sockaddr_in saddr;
	struct ip_mreq imreq;
	FILE *spipe;
	
	for (i=0; i<maxstations; i++)
		sock[i] = 0;
	
	current_station = 0;
	
	for (i=0; mip[i] != '.'; i++);
	i++;
	for (i; mip[i] != '.'; i++);
	i++;
	for (i; mip[i] != '.'; i++);
	iplast = atoi(mip+i+1);
	strncpy(sip,mip, i+1);
	
	sprintf(sip+i+1, "%d", iplast+current_station);
	
	spipe = popen("play -t mp3 -> /dev/null 2>&1", "w");
	if( spipe == NULL)
	{
		printf("error openning a pipe\n");
		globalstate = quitting;
	}
	
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	memset(&imreq, 0, sizeof(struct ip_mreq));
	
	sock[0] = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		printf("error opening a UDP socket\n");
		globalstate = quitting;
	}
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(uport); // listen on port 4096
	saddr.sin_addr.s_addr = htonl(INADDR_ANY); // bind socket to any interface
	status = bind(sock[0], (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
	setsockopt(sock[0], IPPROTO_IP, IP_MULTICAST_TTL, &ttl, 8);
	if (status <0)
	{
		printf("error binding the socket\n");
		globalstate = quitting;
	}
	imreq.imr_multiaddr.s_addr = inet_addr(sip);
	imreq.imr_interface.s_addr = INADDR_ANY; // use DEFAULT interface
	status = setsockopt(sock[0], IPPROTO_IP, IP_ADD_MEMBERSHIP,(const void *)&imreq, sizeof(struct ip_mreq));
	socklen = sizeof(struct sockaddr_in);
	
	while(globalstate != quitting)
	{
		if(current_station != station)
		{
			current_station = station;
			status = setsockopt(sock[0], IPPROTO_IP, IP_DROP_MEMBERSHIP,(const void *)&imreq, sizeof(struct ip_mreq));
			sip[i+1] = '\0';
			sip[i+2] = '\0';
			sip[i+3] = '\0';
			sprintf(sip+i+1, "%d", iplast+current_station);
			imreq.imr_multiaddr.s_addr = inet_addr(sip);
			/*sock[current_station] = socket(AF_INET, SOCK_DGRAM, 0);
			status = bind(sock[current_station], (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
			setsockopt(sock[current_station], IPPROTO_IP, IP_MULTICAST_TTL, &ttl, 8);
			if (status <0)
			{
				printf("error binding the socket\n");
				globalstate = quitting;
				continue;
			}*/
			//imreq.imr_multiaddr.s_addr = inet_addr(sip);
			//imreq.imr_interface.s_addr = INADDR_ANY; // use DEFAULT interface
			status = setsockopt(sock[0], IPPROTO_IP, IP_ADD_MEMBERSHIP,(const void *)&imreq, sizeof(struct ip_mreq));
		}
		
		rsize = recvfrom(sock[0], buffer, 1024, 0,(struct sockaddr *)&saddr, &socklen);
		fwrite (buffer , sizeof(char), rsize, spipe); //write to the pipe
	}
	
	for( i=0; i<maxstations; i++)
		if( sock[i] != 0)
			close(sock[i]);

	return NULL;
}


void *tcpart (void* arg)
{
	int clientSocket, sflag, amount_of_KB, i=0, state = hello, sel, current_station=0, iflag, size, perci;
	double perc, tsize=0;
	unsigned char buffer[1024], calc[100];
	struct sockaddr_in serverAddr;
	uint8_t b8;
	uint16_t b16;
	uint32_t b32;
	fd_set readset;
	struct timeval timeout;
	FILE *fp;
	
	
	socklen_t addr_size;
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = inet_addr(ip);
	memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero); 
	addr_size = sizeof serverAddr;
	if (connect(clientSocket, (struct sockaddr *) &serverAddr, addr_size) == -1)
	{
		printf("error connecting to the server\n");
		globalstate = quitting;
		return 1;
	}
	
	
	while(globalstate != quitting)
	{
		if( current_station != station)
		{
			current_station = station;
			state = asksong;
		}
		if (globalstate == up)
			state = upsong;
		switch (state)
		{
			case hello:
				printf("in the hello\n");
				buffer[0] = 0;
				buffer[1] = 0;
				buffer[2] = 0;
				send(clientSocket, buffer, 3,0);
				state = expectingwelcome;
			break;
			
			case expectingwelcome:
			printf("in the welcome\n");
				timeout.tv_sec = 0;
				timeout.tv_usec = 100000;// try 500000 if there are troubles
				FD_ZERO(&readset);
				FD_SET(clientSocket, &readset);
				sel = select(clientSocket+1, &readset, NULL, NULL, &timeout);
				if (sel == -1 ) 
				{
					printf("error using select\n");
					globalstate = quitting;
					continue;
				}
				else if(sel && FD_ISSET(clientSocket, &readset))
				{
					size = read(clientSocket, buffer, 1024);
					if (size < 9)
					{
						state = invalid;
						iflag = expectingwelcome;
						break;
					}
					b8 = buffer[0];
					if(b8!= 0)
					{
						printf("was expecting hello and got message type %d\n", b8);
						globalstate = quitting;
						continue;
					}
					b16 = ((int)buffer[1])<<8 +buffer[2];
					numofstations = b16;
					b8 = buffer[6];
					i = sprintf(mip, "%d", b8);
					mip[i] = '.';
					i++;
					b8 = buffer[5];
					i += sprintf(mip+i, "%d", b8); 
					mip[i] = '.';
					i++;
					b8 = buffer[4];
					i += sprintf(mip+i, "%d", b8); 
					mip[i] = '.';
					i++;
					b8 = buffer[3];
					i += sprintf(mip+i, "%d", b8); 
					b16 = (((int)buffer[7])<<8) +buffer[8];
					uport = b16;
					state = asksong;
					globalstate = idle;
				}
				else
				{
				//	printf("got invalid in expecting, sel is %d\n", sel);
					printf("timeout while waiting for welcome\n");
					globalstate = quitting;
					continue;
				}
			break;
			
			case asksong :
				b8 = 1;
				b16 = (uint16_t) current_station;
				buffer[0] = b8;
				buffer[1] = ((b16 >> 8) & 0xFF);
				buffer[2] = (b16 & 0xFF);
				send(clientSocket, buffer, 3,0);
				state = expectingannounce;
			break;
			
			case expectingannounce:
				timeout.tv_sec = 0;
				timeout.tv_usec = 100000; // try 500000 if there are troubles
				FD_ZERO(&readset);
				FD_SET(clientSocket, &readset);
				sel = select(clientSocket+1, &readset, NULL, NULL, &timeout);
				if (sel == -1 ) 
				{
					printf("error using select\n");
					globalstate = quitting;
					continue;
				}
				else if(sel && FD_ISSET(clientSocket, &readset))
				{
					read(clientSocket, buffer, 1024);
					b8 = buffer[0];
					if( b8 != 1) // not an announce message
					{
						state = invalid;
						iflag = expectingannounce;
						break;
					}
					b8 = buffer[1]; // announce message
					strncpy(calc, buffer+2, b8);
					calc[b8+1] = '\0'; // calc gets song name
					printf("on station %d the song playing is %s\n", current_station, calc);
					state = idle;
				}
				else
				{
					printf("timeout while waiting for annouce\n");
					globalstate = quitting;
					continue;
				}
			break;
			
			case idle:
				memset (buffer, '\0', 1024);
				timeout.tv_sec = 0;
				timeout.tv_usec = 10000; // try 500000 if there are troubles
				FD_ZERO(&readset);
				FD_SET(clientSocket, &readset);
				sel = select(clientSocket+1, &readset, NULL, NULL, &timeout);
				if (sel == -1 ) 
				{
					printf("error using select\n");
					globalstate = quitting;
					continue;
				}
				else if(sel && FD_ISSET(clientSocket, &readset))
				{
					read(clientSocket, buffer, 1024);
					b8 = buffer[0]; // checking the message type
					if (b8 == 0)
					{
						iflag = multiplewelcome;
						state = invalid;
					}
					else if (b8 == 1)
					{
						iflag = strayannounce;
						state = invalid;
					}
					else if (b8 == 2)
					{
						iflag = straypermit;
						state = invalid;
					}
					else if(b8 == 3)
					{
						b8 = buffer[1];
						strncpy(calc, buffer+2, b8);
						printf("we got an invalid message from the server saying %s\n", calc);
						globalstate = quitting;
						continue;
					}
					else if(b8 == 4)
					{
						state = newstation;
					}
					else
					{
						iflag = generic;
						state = invalid;
					}
				}
						
				
			break;
			
			case upsong: 
				b8 = 2;
				buffer[0] = b8; //message type
				b32 = (uint32_t)songsize;
				buffer[1] = (unsigned char) ((b32 >> 24) & 0xFF);
				buffer[2] = (unsigned char) ((b32 >> 16) & 0xFF);
				buffer[3] = (unsigned char) ((b32 >> 8) & 0xFF);
				buffer[4] = (unsigned char) (b32 & 0xFF);
				b8 = strlen(songname); //song name length
				buffer[5] = b8;
				strncpy(buffer+6, songname, b8);
				send(clientSocket, buffer, 6+b8,0);
				state = expectingpermit;
				globalstate = upload;
			break;
			
			case expectingpermit:
				timeout.tv_sec = 0;
				timeout.tv_usec = 100000; // try 500000 if there are troubles
				FD_ZERO(&readset);
				FD_SET(clientSocket, &readset);
				sel = select(clientSocket+1, &readset, NULL, NULL, &timeout);
				if (sel == -1 ) 
				{
					printf("error using select\n");
					globalstate = quitting;
					continue;
				}
				else if(sel && FD_ISSET(clientSocket, &readset))
				{
					size = read(clientSocket, buffer, 1024);
					b8 = buffer[0]; //message type
					if(b8 != 2)
					{
						iflag = expectingpermit;
						state = invalid;
						break;
					}
					b8 = buffer[1];
					if(b8 == 0) // no permission to upload
					{
						printf("no permission to upload a song, please try again later\n");
						globalstate = idle;
						state = idle;
						break;
					}
					else // permission to upload granted
					{
						fp = fopen(songname, "rb");
						while (((int)tsize) != songsize)
						{
							size = fread(buffer,1, 1024, fp);
							tsize += send(clientSocket, buffer, size,0);
							if(perc+0.1 < (tsize / songsize))
							{
								perci = ((int)((perc+0.1)*100));
								printf("the upload is %d%% done\n",perci);
								perc += 0.1;
							}
							usleep(8000);
						}
						printf("file was successfully uploaded\n");
						globalstate = idle;
						state = idle;
						fclose(fp);
						fp = NULL;
					}
				}
				else
				{
					printf("timeout while waiting for permitsong\n");
					globalstate = quitting;
					continue;
				}
			break;
			
			case newstation:
				b16 = (((uint16_t)buffer[1])<<8) +buffer[2];
				printf("!!! a new station has been announced!!! station %d is now available\n", b16-1);
				numofstations++;
				state = idle;
			break;
			
			case invalid:
				switch (iflag)
				{
					case expectingwelcome:
						printf("error! while expecting welcome we received a message type %d\n", buffer[0]);
					break;
					
					case expectingannounce:
						printf("error! while expecting annouce we received a message type %d\n", buffer[0]);
					break;
					
					case multiplewelcome:
						printf("error! got a second welcome\n");
					break;
					
					case strayannounce:
						printf("error! got an announce message but didn't send asksong\n");
					break;
					
					case straypermit:
						printf("error! got a permit message but didn't send an upsong\n");
					break;
					
					case expectingpermit:
						printf("error! while expectinf permit we received a message type %d\n", buffer[0]);
					break;
					
					default:
					printf("error! got an unknown message type, %d", buffer[0]);
				}
				globalstate = quitting;
				continue;
			break;
		}
	}
	
	close(clientSocket);
	if (fp)
		fclose(fp);
	return NULL;
}


int main (int argc, char *argv[])
{
	char choice;
	FILE *fp;
	char buffer[100];
	globalstate = welcome;
	if(argc != 3)
	{
		printf("invalid amount of arguments\n");
		return 1;
	}
	strncpy(ip, argv[1], 15);
	port = atoi(argv[2]);
	pthread_t tcp, udp;
	
	if (pthread_create(&tcp, NULL, tcpart, NULL) != 0)
	{
		printf ("failed to open thread\n");
		return 1;
	}
	while(globalstate != idle);
	
	if (pthread_create(&udp, NULL, udpart, NULL) != 0)
		{
			printf ("failed to open thread\n");
			return 1;
		}
	while(globalstate != quitting)
	{	
		if (globalstate == idle)
		{
			printf("welcome to the radio station client, please enter the number of station you would like to listen");
			printf(", there are currently %d stations to choose from",numofstations+1);
			printf(", please enter a number from 0 to %d to choose.\nif you want to quit, type q\n", numofstations);
			do
			{
				scanf("%c", &choice);
			}
			while(choice == '\n');
	
			if (choice == 'q')
				globalstate = quitting;
			else if (choice>= '0' && choice <= '9')
			{
				if (numofstations < (choice -'0'))
					printf("please enter a number BETWEEN 0 and %d\n", numofstations);
				else
					station = choice -'0';
			}
			else if(choice == 's')
			{
				printf("please enter the name of the song you would like to upload to the server\n");
				scanf("%100s", buffer);
				fp = fopen(buffer, "r");
				if (fp != NULL)
				{
					fseek(fp, 0, SEEK_END);
					songsize = ftell(fp);
					fclose(fp);
					strncpy (songname, buffer,100);
					globalstate = up;
					while(globalstate != idle);
				}
				else
					printf("could not find a file named %s, please try again\n", buffer);
				
				memset(buffer, '\0', 100);
			}
			else
				printf("unknown input, please try again\n");
		}
		else if (globalstate != quitting)
			globalstate = idle;
	}
	
	printf("bye bye\n");
	sleep(1);
	
	return 0;
}
