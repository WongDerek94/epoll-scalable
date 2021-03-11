/*----------------------------------------------------------------------------------------------------
 * SOURCE FILE:    server.c
 *
 * PROGRAM:        server
 *
 * FUNCTIONS:      void print_usage(char *arg)
 *                 static void SystemFatal(const char *message)
 *                 void interrupt_handler(int)
 *                 long delay (struct timeval t1, struct timeval t2)
 *                 void do_use_fd(int fd, struct timeval* start, struct timeval* end)
 *
 * DATE:           February 28th, 2021
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * NOTES:
 * This program is a epoll server that services concurrent client connections and requests 
 * -------------------------------------------------------------------------------------------------*/

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <omp.h>
#include <sys/time.h>

// Function prototypes
void print_usage(char *arg);
static void SystemFatal(const char *message);
void interrupt_handler(int);
long delay (struct timeval t1, struct timeval t2);
void do_use_fd(int fd, struct timeval* start, struct timeval* end);

//Globals
#define TRUE 1
#define FALSE 0
#define EPOLL_QUEUE_LEN 256
#define BUFLEN 255

int fd_server;

/*----------------------------------------------------------------------------------------------------
 * FUNCTION:       main
 *
 * DATE:           February 28th, 2021
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      int main(int argc, char *argv[])
 *
 * RETURNS:        int
 *
 * NOTES:
 * Main entrypoint into application
 * -------------------------------------------------------------------------------------------------*/
int main(int argc, char *argv[])
{
	int current_socket_alive = 0;

	int arg, n_core;
	size_t epoll_fd;
	int port;
	struct sockaddr_in addr;
    struct timeval start, end;

	// long options
	static struct option long_options[] = {
		{"port", required_argument, 0, 'p'},
		{"max-core", required_argument, 0, 'm'},
		{0, 0, 0, 0}};

	int long_index = 0, opt = 0;
	while ((opt = getopt_long(argc, argv, "p:m:", long_options, &long_index)) != -1)
	{
		switch (opt)
		{
		case 'p':
			port = atoi(optarg);
			break;
		case 'm':
			n_core = atoi(optarg);
			break;
		default:
			print_usage(optarg);
			exit(EXIT_FAILURE);
		}
	}

	// set up the signal handler to close the server socket when CTRL-c is received
	struct sigaction act;
	act.sa_handler = interrupt_handler;
	act.sa_flags = 0;
	if ((sigemptyset(&act.sa_mask) == -1 || sigaction(SIGINT, &act, NULL) == -1))
	{
		perror("Failed to set SIGINT handler");
		exit(EXIT_FAILURE);
	}
 
	// Create the listening socket
	fd_server = socket(AF_INET, SOCK_STREAM, 0);
	if (fd_server == -1)
		SystemFatal("socket");

	// set SO_REUSEADDR so port can be resused imemediately after exit, i.e., after CTRL-c
	arg = 1;
	if (setsockopt(fd_server, SOL_SOCKET, (SO_REUSEADDR | SO_REUSEPORT | SO_REUSEADDR), &arg, sizeof(arg)) == -1)
		SystemFatal("setsockopt");
 
	// Make the server listening socket non-blocking
	if (fcntl(fd_server, F_SETFL, O_NONBLOCK | fcntl(fd_server, F_GETFL, 0)) == -1)
		SystemFatal("fcntl");

	// Bind to the specified listening port
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);
	if (bind(fd_server, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		SystemFatal("bind");

	// Listen for fd_news; SOMAXCONN is 128 by default
	if (listen(fd_server, SOMAXCONN) == -1)
		SystemFatal("listen");

	// Create the epoll file descriptor
	epoll_fd = epoll_create(EPOLL_QUEUE_LEN);
	if (epoll_fd == -1)
		SystemFatal("epoll_create");

	static struct epoll_event event;
	struct epoll_event events[EPOLL_QUEUE_LEN];
	size_t num_fds, fd_new;
	size_t i;

	struct sockaddr_in remote_addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);

	event.events = EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLET ;
	event.data.fd = fd_server;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_server, &event) == -1)
		SystemFatal("epoll_ctl");

	omp_set_num_threads(n_core);
	int EAGAIN_REACHED;
    
    gettimeofday(&start, NULL); // start delay measure

	while (TRUE)
	{
		num_fds = epoll_wait(epoll_fd, events, EPOLL_QUEUE_LEN, -1);
		if (num_fds < 0)
			SystemFatal("Error in epoll_wait!");
        
        #pragma omp parallel shared(current_socket_alive) private(fd_new, EAGAIN_REACHED)
        {
            #pragma omp for
            for (i = 0; i < num_fds; i++)
            {
                // Client Disconnect
                if (events[i].events & EPOLLRDHUP)
                {
                    #pragma omp critical
                    {
                        current_socket_alive--;
                        printf("Client disconnected.  Clients remaining: %d\n", current_socket_alive);
                    }
                    close(events[i].data.fd);
                    continue;
                }
                
                // Error condition
	    		if (events[i].events & (EPOLLHUP | EPOLLERR)) 
                {
                    fputs("epoll: EPOLLERR", stderr);
                    close(events[i].data.fd);
                    continue;
	    		}

                // Server is receiving a connection request
                if (events[i].data.fd == fd_server)
                {
                    {
                        EAGAIN_REACHED = 0;
                        while (!EAGAIN_REACHED)
                        {
                            fd_new = accept(fd_server, (struct sockaddr *)&remote_addr, &addr_size);
                            if (fd_new == -1)
                            {
                                if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
                                {
                                    EAGAIN_REACHED = 1;
                                    break; // We have processed all incoming connections.
                                }
                                else
                                {
                                    perror("accept");
                                    break;
                                }
                            }

                            // Make the fd_new non-blocking
                            if (fcntl(fd_new, F_SETFL, O_NONBLOCK | fcntl(fd_new, F_GETFL, 0)) == -1)
                                SystemFatal("fcntl");

                            // Add the new socket descriptor to the epoll loop
                            event.data.fd = fd_new;
                            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_new, &event) == -1)
                                SystemFatal("epoll_ctl");

                            // Update active socket count
                            #pragma omp critical
                            {
                                current_socket_alive++;
                            
                                printf("New client request from %s.  Clients connected: %d\n", inet_ntoa(remote_addr.sin_addr), current_socket_alive);
                            }
                        }
                    }
                    continue;
                }
                
                // Socket ready for reading
                int fd = events[i].data.fd;
                do_use_fd(fd, &start, &end);
            }
        }
		
	}

	close(fd_server);
	exit(EXIT_SUCCESS);
}

/*----------------------------------------------------------------------------------------------------
 * FUNCTION:       print_usage
 *
 * DATE:           February 28th, 2021
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void print_usage(char *arg)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Reports usage of application at the command line
 * -------------------------------------------------------------------------------------------------*/
void print_usage(char *arg)
{
	printf("Usage: %s port max_socket_allowed\n", arg);
}

/*----------------------------------------------------------------------------------------------------
 * FUNCTION:       SystemFatal
 *
 * DATE:           February 28th, 2021
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      static void SystemFatal(const char *message)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Prints the error stored in errno and aborts the program
 * -------------------------------------------------------------------------------------------------*/
static void SystemFatal(const char *message)
{
	perror(message);
	exit(EXIT_FAILURE);
}

/*----------------------------------------------------------------------------------------------------
 * FUNCTION:       interrupt_handler
 *
 * DATE:           February 28th, 2021
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void interrupt_handler(int signo)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Signal handler to close the server socket when CTRL-c is received
 * -------------------------------------------------------------------------------------------------*/
void interrupt_handler(int signo)
{
	close(fd_server);
	exit(EXIT_SUCCESS);
}

/*----------------------------------------------------------------------------------------------------
 * FUNCTION:       delay
 *
 * DATE:           February 28th, 2021
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      long delay (struct timeval t1, struct timeval t2)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Compute the delay between tl and t2 in milliseconds 
 * -------------------------------------------------------------------------------------------------*/
long delay (struct timeval t1, struct timeval t2)
{
	long d;

	d = (t2.tv_sec - t1.tv_sec) * 1000;
	d += ((t2.tv_usec - t1.tv_usec + 500) / 1000);
	return(d);
}

/*----------------------------------------------------------------------------------------------------
 * FUNCTION:       do_use_fd
 *
 * DATE:           February 28th, 2021
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      void do_use_fd(int fd, struct timeval* start, struct timeval* end)
 *
 * RETURNS:        void
 *
 * NOTES:
 * Process data from file descriptor to buffer, echo data back, and record client transfer statistics
 * -------------------------------------------------------------------------------------------------*/
void do_use_fd(int fd, struct timeval* start, struct timeval* end)
{
	char buf[BUFLEN];
	int data_len = 0, total_transfer = 0;
	while (1)
	{
		data_len = read(fd, buf, BUFLEN);
		if (data_len == -1)
		{
			if (errno != EAGAIN)
			{
				perror("read");
			}
			break;
		}
		else if (data_len == 0)
		{
			break;
		}
        
        total_transfer += data_len;
        
		send(fd, buf, BUFLEN, 0);
        
        FILE *fp = fopen( "serverData.csv", "a" );
        
        // hostname address, data transfered (bytes), response time (sec)
        struct sockaddr_in addr;
        socklen_t addr_size = sizeof(struct sockaddr_in);
        int res = getpeername(fd, (struct sockaddr *)&addr, &addr_size);
        char clientip[20];
        
        if (res == 0)
        {
           strcpy(clientip, inet_ntoa(addr.sin_addr));
        }
        else perror("getpeername");
        
        gettimeofday (end, NULL); // end delay measure
        
        fprintf(fp, "%s, %d, %f\n"
            , clientip, total_transfer, (float) delay(*start, *end) / (float) 1000);

        fclose(fp);
	}
}