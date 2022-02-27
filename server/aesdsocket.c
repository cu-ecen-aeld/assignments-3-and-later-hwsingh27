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


#define PATH "/var/tmp/aesdsocketdata"
#define PORT_NUM "9000"

#define BUFFER_SIZE (1024)
 	
#define ERROR	(-1)
#define SUCCESS (0)

typedef struct
{
	int cl_accept_fd;
	bool thread_comp_flag;
	pthread_t thread_id;
	pthread_mutex_t *mutex;
}thread_parameters;

struct slist_data_s
{
	thread_parameters thread_params;
	SLIST_ENTRY(slist_data_s) entries;
};

typedef struct slist_data_s slist_data_t;
pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
slist_data_t *datap = NULL;
		
int socket_fd = 0;		
int client_fd = 0;
char byte = 0;			
int packets_len = 0;	
int status_flag = 0;		
char *buffer = NULL;
int fd = 0;
	
bool isdaemon = false;	

struct addrinfo *new_addr_info;			

static void signal_handler(int signal_number);

static void gracefully_exit();

void* thread_handler(void* thread_params);

static void timer_handler(int signal_number)
{
	int status_flag;
	char timestr[256];
	time_t t;
	struct tm *time_ptr;
	int timer_length = 0;
	ssize_t num_write;
	
	t = time(NULL);
	time_ptr = localtime(&t);
	if (time_ptr == NULL)
	{
		syslog(LOG_ERR, "localtime function failed\n");
		gracefully_exit();
		exit(EXIT_FAILURE);
	}
	syslog(LOG_INFO, "localtime function successful.\n");
	
	timer_length = strftime(timestr, sizeof(timestr), "timestamp: %m/%d/%Y - %k:%M:%S\n", time_ptr);
	if (timer_length == 0)
	{
		syslog(LOG_ERR, "strftime function failed.\n");
		gracefully_exit();
		exit(EXIT_FAILURE);
	}
	syslog(LOG_INFO, "strftime function successful.\n");
	printf("%s\n",timestr);
	
	fd = open(PATH, O_APPEND | O_WRONLY);
	if(fd == ERROR)
	{
		syslog(LOG_ERR, "File open error for appending.\n");
		gracefully_exit();
		exit(EXIT_FAILURE);
	}

	status_flag = pthread_mutex_lock(&mutex_lock);
	if (status_flag != SUCCESS)
	{
		syslog(LOG_ERR, "pthread_mutex_lock() failed with error number %d\n", status_flag);
		gracefully_exit();
		exit(EXIT_FAILURE);
	}

	num_write = write(fd, timestr, timer_length);
	if (num_write == ERROR)
	{
		syslog(LOG_ERR, "write function failed.\n");
		gracefully_exit();
		exit(EXIT_FAILURE);
	}
	else if (num_write != timer_length)
	{
		syslog(LOG_ERR, "File written partially.\n");
		gracefully_exit();
		exit(EXIT_FAILURE);
	}

	packets_len += timer_length;

	status_flag = pthread_mutex_unlock(&mutex_lock);
	if (status_flag != SUCCESS)
	{
		syslog(LOG_ERR, "pthread_mutex_unlock() failed with error number %d\n", status_flag);
		gracefully_exit();
		exit(EXIT_FAILURE);
	}
	close(fd);
}

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
	if(signal(SIGALRM, timer_handler)==SIG_ERR)
	{
		syslog(LOG_ERR, "Failed to configure SIGALRM handler\n");
		exit(EXIT_FAILURE);
	}

	pthread_mutex_init(&mutex_lock, NULL);
	
	//handle all socket events
	struct addrinfo addr_hints;
	struct sockaddr_in clientaddr;
	socklen_t sockaddrsize = sizeof(struct sockaddr_in); 
    	SLIST_HEAD(slisthead, slist_data_s) head;
	SLIST_INIT(&head);
	char ipv_4[INET_ADDRSTRLEN];
	struct itimerval interval_timer;
	
	//clear the structure
	memset(&addr_hints, 0, sizeof(addr_hints));
	addr_hints.ai_family = AF_INET6;
	addr_hints.ai_socktype = SOCK_STREAM;
	addr_hints.ai_flags = AI_PASSIVE;
	status_flag = getaddrinfo(NULL, PORT_NUM, &addr_hints, &new_addr_info);
	if(status_flag != SUCCESS)
	{
		syslog(LOG_ERR, "Error in getaddrinfo with error code: %d\n",status_flag);	
		return ERROR;
	}
	syslog(LOG_INFO, "getaddrinfo successful\n");

	//creat a socket
	socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
	if (socket_fd == ERROR)
	{
		syslog(LOG_ERR, "Socket creation failed.\n");	
		return ERROR;
	}
	syslog(LOG_INFO, "Socket creation successful.\n");	
	

	status_flag = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	if (status_flag == ERROR)
	{
		syslog(LOG_ERR, "setsockopt function failed.\n");	
		return ERROR;
	}
	syslog(LOG_INFO, "setsockopt function succeeded!\n");
	
	//bind
	status_flag = bind(socket_fd, new_addr_info->ai_addr, new_addr_info->ai_addrlen);
	if (status_flag == ERROR)
	{
		syslog(LOG_ERR, "Binding unsuccessful\n");
		syslog(LOG_ERR, "errno: %d\n", errno);
		return ERROR;
	}
	syslog(LOG_INFO, "Blinding successful\n");
	
	//check if daemon is required, then run as daemon
	if (isdaemon)
	{	
		status_flag = daemon(0, 0);
		if (status_flag == ERROR)
		{
			syslog(LOG_ERR, "Failed to run as Daemon\n");
			return ERROR;
		}
	}
	
	//create a file
	int fd = creat(PATH, 0644);
	if (fd == ERROR)
	{
		syslog(LOG_ERR, "File creation failed.");
		return ERROR;
	}
	close(fd);
	syslog(LOG_INFO, "File creation successful\n");	
	
	interval_timer.it_interval.tv_sec = 10; 
	interval_timer.it_interval.tv_usec = 0;
	interval_timer.it_value.tv_sec = 10; 
	interval_timer.it_value.tv_usec = 0;
	
	status_flag = setitimer(ITIMER_REAL, &interval_timer, NULL);
	if (status_flag == ERROR)
	{
		syslog(LOG_ERR, "setitimer function failed");
		return ERROR;
	}
	
	while (true)
	{
		//listen
		status_flag = listen(socket_fd, 5);
		if(status_flag == ERROR)
		{
			syslog(LOG_ERR, "Error in Listening\n");
			return ERROR;
		}
		syslog(LOG_INFO, "Listening successful!\n");
		
		//accept
		client_fd = accept(socket_fd, (struct sockaddr *)&clientaddr, &sockaddrsize); 
		if (client_fd == ERROR)
		{
			syslog(LOG_ERR, "Error in accepting\n");
			return ERROR;
		}
		
		inet_ntop(AF_INET, &(clientaddr.sin_addr),ipv_4,INET_ADDRSTRLEN);
		
		syslog(LOG_DEBUG,"Accepting connection from %s",ipv_4);
		printf("Accepting connection from %s\n",ipv_4);
		
		datap = (slist_data_t *) malloc(sizeof(slist_data_t));
		SLIST_INSERT_HEAD(&head,datap,entries);

		
		datap->thread_params.cl_accept_fd = client_fd;
		datap->thread_params.thread_comp_flag = false;
		datap->thread_params.mutex = &mutex_lock;
		pthread_create(&(datap->thread_params.thread_id), 	
						NULL,			
						thread_handler,	
						&datap->thread_params	
				   	  );

		printf("All created threads are waiting to exit\n");

		SLIST_FOREACH(datap,&head,entries)
		{
			pthread_join(datap->thread_params.thread_id,NULL);
			datap = SLIST_FIRST(&head);
			SLIST_REMOVE_HEAD(&head, entries);
			free(datap);
		}
		printf("All thread exited.\n");
		syslog(LOG_DEBUG,"Closed connection from %s",ipv_4);
		printf("Closed connection from %s\n",ipv_4);
	}
	close(client_fd);
	close(socket_fd);

	gracefully_exit();
	return ERROR;
}

void* thread_handler(void* thread_params)
{
	bool packet_rcvd = false;
	int val;		
	status_flag = 0;
	char read_data[BUFFER_SIZE];
 	memset(read_data,0,BUFFER_SIZE);
 	thread_parameters *parameters = (thread_parameters *) thread_params;
 	ssize_t num_read=0;			
    	ssize_t num_write=0;			

 	buffer = (char *) malloc((sizeof(char)*BUFFER_SIZE));
	if(buffer == NULL)
	{
		syslog(LOG_ERR, "Malloc unsucessful.\n");
		gracefully_exit();
		exit(EXIT_FAILURE);
	}
	syslog(LOG_INFO, "Malloc successful.\n");

	memset(buffer,0,BUFFER_SIZE);
	
	while(!packet_rcvd)
	{
		num_read = recv(client_fd, read_data, BUFFER_SIZE, 0);		
		if (num_read == ERROR)
		{
			syslog(LOG_ERR, "Receiving failed\n");
			gracefully_exit();
			exit(EXIT_FAILURE);
		}
		else if (num_read == SUCCESS)
		{
			syslog(LOG_INFO, "recv() succeeded.\n");
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
		packets_len = packets_len + val;			
		buffer = (char *) realloc(buffer,(size_t)(packets_len+1));
		if(buffer == NULL)
		{
			syslog(LOG_ERR, "Realloc unsuccessful.\n");
			gracefully_exit();
			exit(EXIT_FAILURE);
		}
		syslog(LOG_INFO, "realloc() successful.\n");
		strncat(buffer,read_data,val);
		memset(read_data,0,BUFFER_SIZE);
	}

	status_flag = pthread_mutex_lock(parameters->mutex);
	if (status_flag != SUCCESS)
	{
		syslog(LOG_ERR, "pthread_mutex_lock() failed with error number %d\n", status_flag);
		gracefully_exit();
		exit(EXIT_FAILURE);
	}

	
	fd = open(PATH, O_APPEND | O_WRONLY);		
	if(fd == ERROR)
	{
		syslog(LOG_ERR, "File opening failed.\n");
		gracefully_exit();
		exit(EXIT_FAILURE);
	}
	syslog(LOG_INFO, "File opening successful.\n");
	
	num_write = write(fd, buffer, strlen(buffer));	
	if (num_write == ERROR)
	{
		syslog(LOG_ERR, "writing failed\n");
		gracefully_exit();
		exit(EXIT_FAILURE);
	}
	else if (num_write != strlen(buffer))
	{
		syslog(LOG_ERR, "file partially written\n");
		gracefully_exit();
		exit(EXIT_FAILURE);	
	}
	syslog(LOG_INFO, "writing successful\n");
	close (fd);
	
	memset(read_data,0,BUFFER_SIZE);		
	fd = open(PATH,O_RDONLY);		
	if(fd == ERROR)				
	{						
		syslog(LOG_ERR, "readonly File open failed.\n");
		gracefully_exit();
		exit(EXIT_FAILURE);
	}
	syslog(LOG_INFO, "readonly File open successful.\n");
	
	status_flag = 0;
	for(val=0; val<packets_len; val++)
	{
		status_flag = read(fd, &byte, 1);		
		if (status_flag == ERROR)
		{
			syslog(LOG_ERR, "reading failed\n");
			gracefully_exit();
			exit(EXIT_FAILURE);
		}
		
			status_flag = 0; 
		status_flag = send(parameters->cl_accept_fd, &byte, 1, 0);	
		if (status_flag == ERROR)
		{
			syslog(LOG_ERR, "sending failed\n");
			gracefully_exit();
			exit(EXIT_FAILURE);
		}	
	}

	status_flag = pthread_mutex_unlock (parameters->mutex);
	if (status_flag != SUCCESS)
	{
		syslog(LOG_ERR, "pthread_mutex_unlock() failed with error number %d\n", status_flag);
		gracefully_exit();
		exit(EXIT_FAILURE);
	}

	close(fd);			
	free(buffer);		
	close(parameters->cl_accept_fd);

	return parameters;
}

static void signal_handler(int signal_number)
{
	if (signal_number == SIGINT)
	{
		syslog(LOG_DEBUG, "SIGINT Error\n");
		gracefully_exit();
	}
	else if (signal_number == SIGTERM)
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
	close (socket_fd);
	close (client_fd); 
	freeaddrinfo(new_addr_info);
	unlink(PATH);
	syslog(LOG_INFO, "Cleared\n");
	closelog();
}

