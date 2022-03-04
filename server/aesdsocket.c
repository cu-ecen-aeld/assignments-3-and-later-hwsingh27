#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>


#define PATH "/var/tmp/aesdsocketdata"
#define PORT_NUM "9000"
#define SOCKET_FAMILY 	AF_INET6
#define SOCKET_TYPE	SOCK_STREAM
#define SOCKET_FLAGS	AI_PASSIVE

#define BUFFER_SIZE (1024)

int socket_fd  = 0;		
int client_fd = 0;		
bool isdaemon = false;			
char *buffer = NULL;		
char byte = 0;		
int packets_len = 0;
int status_flag = 0;		

struct addrinfo *new_addr_info;				

static void signal_handler(int sig_num);

static void gracefully_exit();

int main(int argc, char* argv[])
{
	openlog("aesdsocket", 0, LOG_USER);
	if (argc >= 2)
    	{	
    		if (strcmp("-d", argv[1]) == 0)
    		{
    			isdaemon = true;
    		}
    	}

	//configure signals
	if(signal(SIGINT, signal_handler)==SIG_ERR)
	{
		syslog(LOG_ERR, "Failed to configure SIGINT handler\n");
		exit(EXIT_FAILURE);
	}
	if(signal(SIGTERM, signal_handler)==SIG_ERR)
	{
		syslog(LOG_ERR, "Failed to configure SIGTERM handler\n");
		exit(EXIT_FAILURE);
	}

	//handle all socket events
	struct addrinfo addr_hints;
	struct sockaddr_in clientaddr;
	socklen_t sockaddrsize = sizeof(struct sockaddr_in); 
    	ssize_t num_read=0;	//number of bytes read
    	ssize_t num_write=0;	//number of bytes written

	char read_data[BUFFER_SIZE];
    	memset(read_data,0,BUFFER_SIZE);

    	//clear the structure
	memset(&addr_hints, 0, sizeof(addr_hints));
	addr_hints.ai_family = SOCKET_FAMILY;
	addr_hints.ai_socktype = SOCKET_TYPE;
	addr_hints.ai_flags = SOCKET_FLAGS;

	status_flag = getaddrinfo(NULL, PORT_NUM, &addr_hints, &new_addr_info);
	if(status_flag != 0)
	{
		syslog(LOG_ERR, "Error in getaddrinfo with error code: %d\n", status_flag);	
		return -1;
	}
	syslog(LOG_INFO, "getaddrinfo succeeded!\n");

	//create a socket
	socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (socket_fd == -1)
	{
		syslog(LOG_ERR, "Socket creation failed.\n");
		return -1;
	}
	syslog(LOG_INFO, "socket creation successful!\n");	

	//bind
	status_flag = bind(socket_fd, new_addr_info->ai_addr, new_addr_info->ai_addrlen);
	if (status_flag == -1)
	{
		syslog(LOG_ERR, "Binding unsuccessful\n");
		return -1;
	}
	syslog(LOG_INFO, "Binding successful\n");


	//check if daemon is required, then run as daemon
	if (isdaemon)
	{	
		status_flag = daemon(0,0);
		if (status_flag == -1)
		{
			syslog(LOG_ERR, "Failed to run as Daemon\n");
			return -1;
		}
	}

	//create a file
	int fd = creat(PATH, 0644);
	if (fd == -1)
	{
		syslog(LOG_ERR, "File cannot be created");
		return -1;
	}
	close(fd);
	syslog(LOG_INFO, "File created!\n");	

	while (true)
	{
        	buffer = (char *) malloc((sizeof(char)*BUFFER_SIZE));
		if(buffer == NULL)
		{
			syslog(LOG_ERR, "Malloc failed!\n");
			return -1;
		}
		syslog(LOG_INFO, "malloc succeded!\n");
		
		memset(buffer,0,BUFFER_SIZE);

		//listen
		status_flag = listen(socket_fd, 5);
		if(status_flag == -1)
		{
			syslog(LOG_ERR, "Error Listening\n");
			return -1;
		}
		syslog(LOG_INFO, "listen() succeeded!\n");

		//accept
		client_fd = accept (socket_fd, (struct sockaddr *)&clientaddr, &sockaddrsize);
		if (client_fd == -1)
		{
			syslog(LOG_ERR, "Error accepting\n");
			return -1;
		}
		//print the ip address of the connection
		syslog(LOG_INFO, "accept succeeded! Accepted connection from: %s\n", inet_ntoa(clientaddr.sin_addr));

		bool packet_rcvd = false;
		int val;		
		status_flag = 0;

		while(!packet_rcvd)
		{
			num_read = recv(client_fd, read_data, BUFFER_SIZE, 0);		
			if (num_read == -1)
			{
				syslog(LOG_ERR, "Receiving failed\n");
				return -1;
			}
			else if (num_read == 0)
			{
				syslog(LOG_INFO, "recv succeeded\n");
			}

			for(val = 0;val < BUFFER_SIZE;val++)
			{
				if(read_data[val] == '\n')
				{
					packet_rcvd = true;		
					val++;			
					break;
				}
			}
			packets_len += val;			
			buffer = (char *) realloc(buffer,(size_t)(packets_len+1));
			if(buffer == NULL)
			{
				syslog(LOG_ERR, "Realloc failed!\n");
				return -1;
			}
			syslog(LOG_INFO, "realloc successful\n");
			strncat(buffer,read_data,val);
			memset(read_data,0,BUFFER_SIZE);
		}

		fd = open(PATH, O_APPEND | O_WRONLY);		
		if(fd == -1)
		{
			syslog(LOG_ERR, "File opening failed\n");
			return -1;
		}
		syslog(LOG_INFO, "open successful\n");



		num_write = write(fd, buffer, strlen(buffer));	
		if (num_write == -1)
		{
			syslog(LOG_ERR, "writing failed\n");
			return -1;
		}
		else if (num_write != strlen(buffer))
		{
			syslog(LOG_ERR, "file partially written\n");
			return -1;	
		}
		syslog(LOG_INFO, "write successful\n");
		close (fd);


		memset(read_data,0,BUFFER_SIZE);		

		fd = open(PATH,O_RDONLY);			
		if(fd == -1)								
		{						
			syslog(LOG_ERR, "File open failed\n");
			return -1;
		}
		syslog(LOG_INFO, "open successful\n");


		status_flag = 0;
		for(val=0; val<packets_len; val++)
		{
			status_flag = read(fd, &byte, 1);		
			if (status_flag == -1)
			{
				syslog(LOG_ERR, "reading failed\n");
				return -1;
			}

			status_flag = 0; 
			status_flag = send(client_fd, &byte, 1, 0);	
			if (status_flag == -1)
			{
				syslog(LOG_ERR, "sending failed\n");
				return -1;
			}
		}
		close(fd);		
		free(buffer);		
		syslog(LOG_INFO,"Closed connection with %s\n", inet_ntoa(clientaddr.sin_addr));
	}
	gracefully_exit();
	return -1;
}


static void signal_handler(int sig_num)
{
	if (sig_num == SIGINT)
	{
		syslog(LOG_DEBUG, "SIGINT Error\n");
		gracefully_exit();
	}
	else if (sig_num == SIGTERM)
	{
		syslog(LOG_DEBUG, "SIGTERM Error\n");
		gracefully_exit();
	}
	else
	{
		syslog(LOG_ERR, "Other Signal Error\n");
		gracefully_exit();
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

static void gracefully_exit()
{
	free(buffer);
	close (socket_fd);
	close (client_fd); 
	freeaddrinfo(new_addr_info);
	unlink(PATH);
	syslog(LOG_INFO, "Cleared\n");
	closelog();
}
