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
#include <pthread.h>
#include <sys/queue.h>
#include <sys/time.h>

#define PORT "9000"

#define LISTEN_BACKLOG (5)
#define FILE_PERMISSIONS (0644)
#define BUFFER_SIZE (1024)
	

#define USE_AESD_CHAR_DEVICE	(1)
#if (USE_AESD_CHAR_DEVICE == 1)
	#define STORAGE_PATH  	"/dev/aesdchar"
#else
	#define STORAGE_PATH 	"/var/tmp/aesdsocketdata"
#endif

typedef struct
{
	bool thread_complete;
	pthread_t thread_id;
	int clifd;
	pthread_mutex_t *mutex;
}thread_parameters;

struct slist_data_s
{
	thread_parameters	thread_params;
	SLIST_ENTRY(slist_data_s) entries;
};

typedef struct slist_data_s slist_data_t;
pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
SLIST_HEAD(slisthead, slist_data_s) head;


int fd = 0;	
int sockfd = 0;	
int clientfd = 0;		
int status = 0;	
int packets = 0;		
char byte = 0;		
bool isdaemon = false;	
slist_data_t *datap = NULL;	

void handle_socket(void);
void* thread_handler(void* thread_params);
#if (USE_AESD_CHAR_DEVICE == 0)

static void sigalrm_handler();
#endif

static void signal_handler(int signal_number);

int main(int argc, char *argv[])
{
	openlog("aesdsocket",LOG_PID,LOG_USER);

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

	#if (USE_AESD_CHAR_DEVICE == 0)
	if(signal(SIGALRM, sigalrm_handler)==SIG_ERR)
	{
		syslog(LOG_ERR, "Failed to configure SIGALRM handler\n");
		exit(EXIT_FAILURE);
	}
	#endif

	pthread_mutex_init(&mutex_lock, NULL);

	if((argc >= 2) && (!strcmp("-d",(char*)argv[1])))
	{
		isdaemon = true;
	}

	handle_socket();

	closelog();

	return -1;
}

void handle_socket()
{
	struct addrinfo hints;
	struct addrinfo *results;
	struct sockaddr client_addr;
	socklen_t client_addr_size;
	char ip_address[INET_ADDRSTRLEN];
	struct sockaddr_in *address;
	struct itimerval timer;
	SLIST_INIT(&head);
	
	memset(&hints,0,sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_STREAM;
	status = getaddrinfo(NULL,PORT,&hints,&results);
	if(status != 0)
	{
		syslog(LOG_ERR,"getaddrinfo function failed\n");
		exit(EXIT_FAILURE);
	}
	
	sockfd = socket(PF_INET6, SOCK_STREAM, 0);
	if(sockfd == -1)
	{
		syslog(LOG_ERR,"socket function failed\n");
		freeaddrinfo(results);
		exit(EXIT_FAILURE);
	}
	
	status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	if(status == -1)
	{
		syslog(LOG_ERR,"setsockopt function failed\n");
		freeaddrinfo(results);
		exit(EXIT_FAILURE);
	}

	status = bind(sockfd, results->ai_addr, results->ai_addrlen);
	if(status == -1)
	{
		syslog(LOG_ERR,"bind function failed\n");
		freeaddrinfo(results);
		exit(EXIT_FAILURE);
	}
	
	if (isdaemon)
	{
		status = daemon(0,0);
		if(status == -1)
		{
			syslog(LOG_ERR,"daemon function failed\n");
			exit(EXIT_FAILURE);
		}
		syslog(LOG_INFO,"Entering daemon mode\n");
	}

	fd = creat(STORAGE_PATH, FILE_PERMISSIONS);
	if(fd == -1)
	{
		syslog(LOG_ERR,"create function failed\n");
		exit(EXIT_FAILURE);
	}
	close(fd);
	freeaddrinfo(results);

	timer.it_interval.tv_sec = 10;
	timer.it_interval.tv_usec = 0;
	timer.it_value.tv_sec = 10; 
	timer.it_value.tv_usec = 0;
	status = setitimer(ITIMER_REAL, &timer, NULL);
	if(status == -1)
	{
		syslog(LOG_ERR,"setitimer function failed\n");
	}

	while(true)
	{
		status = listen(sockfd, LISTEN_BACKLOG);
		if(status == -1)
		{
			syslog(LOG_ERR,"listen function failed\n");
			freeaddrinfo(results);
			exit(EXIT_FAILURE);
		}

		client_addr_size = sizeof(struct sockaddr);
		clientfd = accept(sockfd,(struct sockaddr *)&client_addr, &client_addr_size);
		if(clientfd == -1)
		{
			syslog(LOG_ERR,"accept function failed\n");
			freeaddrinfo(results);
			exit(EXIT_FAILURE);
		}
		address = (struct sockaddr_in *)&client_addr;
		inet_ntop(AF_INET, &(address->sin_addr),ip_address,INET_ADDRSTRLEN);
		syslog(LOG_INFO,"Accepting connection from %s",ip_address);

		datap = (slist_data_t *) malloc(sizeof(slist_data_t));
		SLIST_INSERT_HEAD(&head,datap,entries);

		datap->thread_params.clifd = clientfd;
		datap->thread_params.thread_complete = false;
		datap->thread_params.mutex = &mutex_lock;

		status = pthread_create(&(datap->thread_params.thread_id), 			
							NULL,			
							thread_handler,			
							&datap->thread_params
							);

		if (status != 0)
		{
			syslog(LOG_ERR,"pthread_create function failed\n");
			exit(EXIT_FAILURE);
		}

		syslog(LOG_INFO,"Thread created\n");

		SLIST_FOREACH(datap,&head,entries)
		{
			pthread_join(datap->thread_params.thread_id,NULL);
			if(datap->thread_params.thread_complete == true)
			{
				SLIST_REMOVE(&head, datap, slist_data_s, entries);
				free(datap);
				break;
			}
		}
		syslog(LOG_INFO,"Thread exited\n");

		syslog(LOG_DEBUG,"Closed connection from %s",ip_address);
	}
	close(clientfd);
	close(sockfd);
}

void* thread_handler(void* thread_params)
{
	bool packet_rcvd = false;
	int i;
	char read_data[BUFFER_SIZE] = {0};
	char *buffer = NULL;
	thread_parameters * parameters = (thread_parameters *) thread_params;
	
	buffer = (char *) malloc(sizeof(char)*BUFFER_SIZE);
	if(buffer == NULL)
	{
		syslog(LOG_ERR,"malloc() failed");
		exit(EXIT_FAILURE);
	}

	memset(buffer,0,BUFFER_SIZE);
	while(!packet_rcvd)
	{
		status = recv(parameters->clifd, read_data, BUFFER_SIZE ,0); 
		if(status == -1)
		{
			syslog(LOG_ERR,"recv() failed");
			exit(EXIT_FAILURE);
		}
		for(i = 0;i < BUFFER_SIZE;i++)
		{
			if(read_data[i] == '\n')
			{
				packet_rcvd = true;
				i++;
				break;
			}
		}
		packets += i;
		buffer = (char *) realloc(buffer,(ssize_t)(packets+1));
		if(buffer == NULL)
		{
			syslog(LOG_ERR,"realloc() failed");
			exit(EXIT_FAILURE);
		}
		strncat(buffer,read_data,i);
		memset(read_data,0,BUFFER_SIZE);
	}

	status = pthread_mutex_lock(parameters->mutex);
	if(status != 0)
	{
		syslog(LOG_ERR,"pthread_mutex_lock() failed with error code: %d\n",status);
		exit(EXIT_FAILURE);
	}
	fd = open(STORAGE_PATH,O_APPEND | O_WRONLY);
	if(fd == -1)
	{
		syslog(LOG_ERR,"open() failed (append and write only)\n");
		exit(EXIT_FAILURE);
	}

	int status = write(fd,buffer,strlen(buffer));
	if(status == -1)
	{
		syslog(LOG_ERR,"write() failed\n");
		exit(EXIT_FAILURE);
	}
	else if(status != strlen(buffer))
	{
		syslog(LOG_ERR,"File partially written\n");
		exit(EXIT_FAILURE);
	}
	close(fd);

	memset(read_data,0,BUFFER_SIZE);
	fd = open(STORAGE_PATH,O_RDONLY);
	if(fd == -1)
	{
		syslog(LOG_ERR,"open() failed (read only)\n");
		exit(EXIT_FAILURE);
	}

	for(i=0;i<packets;i++)
	{
		status = read(fd, &byte, 1);
		if(status == -1)
		{
			syslog(LOG_ERR,"read() failed!\n");
			exit(EXIT_FAILURE);
		}

		status = send(parameters->clifd,&byte,1,0);
		if(status == -1)
		{
			syslog(LOG_ERR,"send() failed!\n");
			exit(EXIT_FAILURE);
		}
	}

	status = pthread_mutex_unlock(parameters->mutex);
	if(status != 0)
	{
		syslog(LOG_ERR,"pthread_mutex_unlock() failed with error code: %d\n",status);
		exit(EXIT_FAILURE);
	}
	
	parameters->thread_complete = true;

	close(fd);
	close(parameters->clifd);
	free(buffer);

	return parameters;
}

static void signal_handler(int signal_number)
{
	switch(signal_number)
	{
		case SIGINT:
			syslog(LOG_DEBUG,"SIGINT Caught! Exiting ... \n");
			while(SLIST_FIRST(&head) != NULL)
			{
				SLIST_FOREACH(datap,&head,entries)
				{
					close(datap->thread_params.clifd);
					pthread_join(datap->thread_params.thread_id,NULL);
					SLIST_REMOVE(&head, datap, slist_data_s, entries);
					free(datap);
					break;
				}
			}

			pthread_mutex_destroy(&mutex_lock);
			unlink(STORAGE_PATH);
			close(clientfd);
			close(sockfd);
			break;

		case SIGTERM:
			syslog(LOG_INFO,"SIGTERM Caught! Exiting ... \n");

			while(SLIST_FIRST(&head) != NULL)
			{
				SLIST_FOREACH(datap,&head,entries)
				{
					close(datap->thread_params.clifd);
					pthread_join(datap->thread_params.thread_id,NULL);
					SLIST_REMOVE(&head, datap, slist_data_s, entries);
					free(datap);
					break;
				}
			}
			pthread_mutex_destroy(&mutex_lock);
			unlink(STORAGE_PATH);
			close(clientfd);
			close(sockfd);
			break;
		default:
			exit(EXIT_FAILURE);
			break;
	}
	exit(EXIT_SUCCESS);
}


#if (USE_AESD_CHAR_DEVICE==0)
static void sigalrm_handler()
{
	char timestr[200];
	time_t t;
	struct tm *ptr;
	int timer_length = 0;

	t = time(NULL);
	ptr = localtime(&t);
	if(ptr == NULL)
	{
		syslog(LOG_ERR,"localtime() failed\n");
		exit(EXIT_FAILURE);
	}

	timer_length = strftime(timestr,sizeof(timestr),"timestamp:%d.%b.%y - %k:%M:%S\n",ptr);
	if(timer_length == 0)
	{
		syslog(LOG_ERR,"strftime function failed\n");
		exit(EXIT_FAILURE);
	}

	fd = open(STORAGE_PATH,O_APPEND | O_WRONLY);
	if(fd == -1)
	{
		syslog(LOG_ERR,"open function failed (append and write only)\n");
		exit(EXIT_FAILURE);
	}

	status = pthread_mutex_lock(&mutex_lock);
	if(status != 0)
	{
		syslog(LOG_ERR,"pthread_mutex_lock function failed with error code %d\n", status);
		exit(EXIT_FAILURE);
	}

	status = write(fd,timestr,timer_length);
	if(status == -1)
	{
		syslog(LOG_ERR,"write() failed\n");
		exit(EXIT_FAILURE);
	}
	else if(status != timer_length)
	{
		syslog(LOG_ERR,"file partially written\n");
		exit(EXIT_FAILURE);
	}
	packets += timer_length;

	status = pthread_mutex_unlock(&mutex_lock);
	if(status != 0)
	{
		syslog(LOG_ERR,"pthread_mutex_unlock function failed with error code %d\n", status);
		exit(EXIT_FAILURE);
	}
	close(fd);
}
#endif
