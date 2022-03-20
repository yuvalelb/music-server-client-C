
#define maxstations 10
// states for the client
#define invalid 0
#define expectinghello 1
#define gothello 2
#define idle 3
#define announce 4
#define upsong 5

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

int defaultport;
char mip[15];
int mport;
int tcpport;
int activestations[maxstations];
char *songnames[maxstations];
int permitf =1;
int num_of_stations =0;


/*typedef struct {
	int snum;
	char* songname;
} sargs;*/
	

void whatsmyip(char* ip)
{
 int fd;
 struct ifreq ifr;

 fd = socket(AF_INET, SOCK_DGRAM, 0);

 /* I want to get an IPv4 IP address */
 ifr.ifr_addr.sa_family = AF_INET;

 /* I want IP address attached to "eth0" */
 strncpy(ifr.ifr_name, "eth0", IFNAMSIZ-1);

 ioctl(fd, SIOCGIFADDR, &ifr);

 close(fd);

 /* display result */
 strncpy(ip, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),15);

}

void *station (void* arg)
{
	char* songname;
	FILE *fp;
	struct timeval delay;
	int *num = (int *)arg;
	int sock, port, status, socklen;
	unsigned char ttl =255;
	char buffer[1024];
	struct sockaddr_in saddr;
	struct in_addr iaddr;
	char* ip;
	

	ip = malloc (15);
	strncpy(ip, mip, 15);
	port = mport + *num;
	songname = songnames[*num];
	if (!(fp = fopen(songname, "r")))
		perror("error opening a song in a thread\n"),exit(1);
	
	//delay.tv_sec = 0;
	//delay.tv_usec = 62500;
	
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	memset(&iaddr, 0, sizeof(struct in_addr));
	sock = socket(AF_INET,SOCK_DGRAM,0);
	if(sock <0)
		perror("error creating socket"),exit(1);
	
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(0);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	iaddr.s_addr = INADDR_ANY;
	setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, &iaddr, sizeof(struct in_addr));
	setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof (unsigned char));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = inet_addr(ip);
	saddr.sin_port = htons(port);
	status = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,ip, 15);
	
	fread(buffer,1,1024,fp);
	socklen = sizeof(struct sockaddr_in);
	
	while (status != -1 && activestations[*num] == 1)
	{
		sendto(sock, buffer,strlen(buffer), 0, (struct sockaddr_in *)&saddr, socklen);  // might have a bug (sockaddr_in and such)
		fread(buffer,1,1024,fp);
		usleep(62500);
		if (feof(fp))
			rewind(fp);
	}
	free(ip);
	fclose(fp);
	exit(0);
}

void *client (void* arg)
{
	int state = 1, sel, size, i, iflag, stn=0, anf=0;
	uint8_t b8;
	uint16_t b16;
	uint32_t b32;
	fd_set readset;
	int *socket;
	struct timeval timeout;
	unsigned char buffer[100], calc[100];
	FILE *fp;
	socket = (int*) arg;
	timeout.tv_sec = 0;
	while (state != -1)
	{
		switch (state)
		{
			case expectinghello :
				
				timeout.tv_usec = 100000; // try 500000 if there are troubles
				FD_ZERO(&readset);
				FD_SET(*socket, &readset);
				sel = select(*socket+1, &readset, NULL, NULL, &timeout);
					if (sel == -1 ) 
					{
						perror("error using select");
						state = -1;
					}
					else if(sel && FD_ISSET(*socket, &readset))
					{
						size = recv(*socket, buffer, 1024,0);
						if (size < 24)
						{
							state = invalid;
							iflag = expectinghello;
						}
						strncpy(calc, buffer, 8);
						b8 = atoi(calc);
						strncpy(calc, buffer+8, 16);
						b16 = ntohs(atoi(calc));
						if ( !(b8 == 0 && b16 == 0))
						{
							state = invalid;
							iflag = expectinghello;
						}
							
						state = gothello;
						break;
					}
					else
					{
						state = invalid;
						iflag = expectinghello;
					}
			break;
				
			case gothello :
				
				b8 = 0;
				buffer[0] = (unsigned char) b8;
				for(b16=0; activestations[b16] != 0; b16++);
				buffer[1] = (unsigned char) ((b16 >> 8) & 0xFF);
				buffer[2] = (unsigned char) (b16 & 0xFF);
				b32 = 0;
				for (i=0; i<strlen(mip); i++)
					if (i%4 != 0)
						b32 = b32 *10 + ((uint32_t)(mip[i] - '0'));
				buffer[3] = (unsigned char) ((b32 >>24) & 0xFF);
				buffer[4] = (unsigned char) ((b32 >>16) & 0xFF);
				buffer[5] = (unsigned char) ((b32 >>8) & 0xFF);
				buffer[6] = (unsigned char) ((b32 & 0xFF));
				b16 = mport;
				buffer[7] = (unsigned char) ((b16 >> 8) & 0xFF);
				buffer[8] = (unsigned char) (b16 & 0xFF);
				send(*socket, buffer, 9,0);
				
				
				state = idle;
				
			break;
			
			case idle:
			
				do
				{
					FD_ZERO(&readset);
					FD_SET(*socket, &readset);
					timeout.tv_usec = 100000;
					sel = select(*socket+1, &readset, NULL, NULL, &timeout);
					
					if (sel == -1)
					{
						perror("error using select");
						state = -1;
					}
					else if(sel && FD_ISSET(*socket, &readset))
					{
						size = recv(*socket, buffer, 1024, 0);
						if (size == 0)
						{
							close(*socket);
							state = -1;
						}
						b8 = buffer[0];
						if(i == 1)
							state = announce;
						else if(i==2)
							state = upsong;
						else
						{
							iflag = idle;
							state = invalid;
						}
					}
					else if (activestations[stn] != 0 && anf == 1)
						state = announce;
				}
				while (state == idle);
			
			break;
			
			case announce:
			
				anf = 1;
				while(activestations[stn] != 0)
				{
					b8 = 1;
					buffer[0] = (unsigned char) b8;
					b8 = (uint8_t) strlen(songnames[stn]);
					buffer[1] = (unsigned char) b8;
					strncpy(buffer+2, songnames[stn], (int)b8);
					send(*socket, buffer, 2+(int)b8,0);
					
					stn++;
				}
				
				state = idle;
			
			break;
			
			case upsong:
				
				
				b8 = 2;
				buffer[0] = (unsigned char)  b8;
				memset(calc, 0, 5);
				strncpy(calc, buffer+1, 4);
				b32 = atoi(calc);
				if((permitf == 1) && (b32 > 2000) && (b32 < 10*1024*1024))
				{
					permitf = 0;
					b8 = 1;
				}
				else
					b8 = 0;
				
				buffer[1] = b8;
				
				send(*socket, buffer, 2,0);
				
				
				if(b8)
				{
					memset(calc, 0, 5);
					strncpy(songnames[num_of_stations+1] , buffer+5, 100);
					fp = fopen(songnames[num_of_stations+1], "w");
					
					do
					{
						FD_ZERO(&readset);
						FD_SET(*socket, &readset);
						timeout.tv_usec = 0;
						timeout.tv_sec = 3;
						
						sel = select(*socket+1, &readset, NULL, NULL, &timeout);
						
						if (sel == -1)
						{
							perror("error using select");
							state = -1;
						}
						else if(sel && FD_ISSET(*socket, &readset))
						{
							size = recv(*socket, buffer, 1024, 0);
							if (size == -1)
							{
								printf("connection has been terminated\n");
								state = -1;
							}
							else
							{
								strncpy(calc, buffer, size);
								calc[size+1] = '\0';
								fputs (calc, fp);
							}
						}
						else 
						{
							printf("connection has been timed out\n");
							fclose(fp);
							permitf = 1;
							iflag = upsong;
							state = invalid;
							break;
						}
						b32 -= (uint32_t)size;
					}
					while(b32 > 0);
					
					state = announce;
					activestations[num_of_stations +1] = num_of_stations+1;
					num_of_stations++;
					permitf = 1;
					timeout.tv_sec = 0;
					
					
				}
			
			break;
			
			case invalid:
				
				switch (iflag)
				{
					
					case upsong:
						stpcpy(calc, "a timeout has occured during the upload of a song\0");	
					break;
					
					case idle:
						strcpy(calc, "an unknown message type has been received: ");
						size = sprintf(calc+43, "%d", (int)b8) + 43;
						calc[size+1] = '0';
					break;
					
					case expectinghello:
						strcpy(calc, "did not get a hello\0");
					break;
					
					default:
						strcpy(calc, "a weird error has occured\0");
					break;
				}
				
				b8 = 3;
				buffer[0] = (unsigned char) b8;
				b8 = (uint8_t) strlen(calc);
				buffer[1] = (unsigned char) b8;
				strncpy(buffer, calc, (int) b8);
				send(*socket, buffer, 2+(int)b8,0);
				state = -1;
			default:
			state = -1;
			break;
		}
	}
	close(*socket);
	exit(0);
}
	
	
				
				
			
	
	



int main(int argc, char* argv[])	
{
	int welcomeSocket, newSocket, sflag, amount_of_KB, i=0,j=0, port, sock_num=0, sel, stn=0;
	int sock[100];
	char* ip;
	char buffer[1024];
	char lastoct[3];
	struct sockaddr_in serverAddr;
	struct sockaddr_storage serverStorage;
	struct timeval delay;
	fd_set readset;
	socklen_t addr_size;
	pthread_t stations[maxstations];
	pthread_t clients[100];
	
	if (argc<4)
	{perror("not enough arguments\n"); return 1;}
	
	tcpport = atoi(argv[1]);
	strcpy(mip, argv[2]);
	mport= atoi(argv[3]);
	
	for (i =0; i<15; i++)
		mip[i] = '\0';
	
	for (i=0; i<maxstations; i++)
	{
		activestations[i] = 0;
		songnames[i] = malloc(100);
	}
	
	i=4;
	while (argv[i] && (i-4)<maxstations)
	{
		strncpy(songnames[i-4], argv[i], 100);
		activestations[i-4] = i-4;
		if (pthread_create(&stations[i-4], NULL, station, (void*)&(activestations[i-4])) != 0)
		{perror ("failed to open thread"); return 1;}
		i++;
		stn++;
	}
	
	ip = (char*) malloc(15);
	whatsmyip(ip);
	port = defaultport;
	
	
	
	
	welcomeSocket = socket(AF_INET,SOCK_DGRAM,0);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr - inet_addr(ip);
	memset(serverAddr.sin_zero,'\0', sizeof serverAddr.sin_zero);
	
	bind(welcomeSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));
	if(listen(welcomeSocket,5) == 0)
		printf("listening\n");
	else
		printf("not listening error\n");
	addr_size = sizeof serverStorage;
	
	while(1)
	{
		delay.tv_sec = 1;
		delay.tv_usec = 0;
		FD_ZERO(&readset);
		FD_SET(welcomeSocket, &readset);
		sel = select(welcomeSocket+1, &readset, NULL, NULL, &delay);
		if(sel == -1) 
		{ 
			printf("error using select\n"); 
			return 1;
		}
		else if(sel)
		{
			sock[sock_num] = accept(welcomeSocket, (struct sockaddr *) &serverStorage, &addr_size);
			sock_num++;
			if (pthread_create(&clients[sock_num], NULL, client, (void*)&sock[sock_num]) != 0)
				printf("error i still think \n");
		}
		while(stn < num_of_stations)
		{
			if (pthread_create(&stations[stn], NULL, station, (void*)&(activestations[stn])) != 0)
			{perror ("failed to open thread"); return 1;}
			stn++;
		}
	}
	
	return 0;
			
}