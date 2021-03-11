/*----------------------------------------------------------------------------------------------------
 * SOURCE FILE:    client.c
 *
 * PROGRAM:        client
 *
 * FUNCTIONS:      void print_usage(char *arg)
 *                 void interrupt_handler(int signo)
 *                 int mil_sleep(long msec)
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
 * This program is a simple client that will connect to a server to send an echo request 
 * -------------------------------------------------------------------------------------------------*/

#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <omp.h>
#include <time.h>

// Function Prototypes
void print_usage(char *arg);
void interrupt_handler(int signo);
int mil_sleep(long msec);

// Globals
#define SERVER_TCP_PORT 7000 // Default port
#define BUFLEN 255           // Buffer length

int fd_client;

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
 * INTERFACE:      int main(int argc, char **argv)
 *
 * RETURNS:        int
 *
 * NOTES:
 * Main entrypoint into application
 * -------------------------------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
	int i;
    int send_count, port;
	struct hostent *hp;
	struct sockaddr_in server;
	char *host, *msg, sbuf[BUFLEN], rbuf[BUFLEN];

	long delay_msec = 0;
	struct timespec start, end;
	unsigned long elapsed = 0;

	// set up the signal handler to close the server socket when CTRL-c is received
	struct sigaction act;
	act.sa_handler = interrupt_handler;
	act.sa_flags = 0;
	if ((sigemptyset(&act.sa_mask) == -1 || sigaction(SIGINT, &act, NULL) == -1))
	{
		perror("Failed to set SIGINT handler");
		exit(EXIT_FAILURE);
	}

	// long options
	static struct option long_options[] = {
		{"host", required_argument, 0, 'h'},
		{"port", required_argument, 0, 'p'},
		{"message", required_argument, 0, 'm'},
		{"count", required_argument, 0, 'c'},
		{"delay", required_argument, 0, 'd'},
		{0, 0, 0, 0}};

	int long_index = 0, opt = 0;
	while ((opt = getopt_long(argc, argv, "h:p:m:c:d:", long_options, &long_index)) != -1)
	{
		switch (opt)
		{
		case 'h':
			host = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'm':
			msg = optarg;
			strcpy(sbuf, msg);
			break;
		case 'c':
			send_count = atoi(optarg);
			break;
		case 'd':
			delay_msec = atol(optarg);
			break;
		default:
			print_usage(optarg);
			exit(EXIT_FAILURE);
		}
	}

	// Create the socket list
	if ((fd_client = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Cannot create socket");
		exit(1);
	}

	// Initialize server info
	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	if ((hp = gethostbyname(host)) == NULL)
	{
		fprintf(stderr, "Unknown server address\n");
		exit(1);
	}
	bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);

	// Connecting to the server
	if (connect(fd_client, (struct sockaddr *)&server, sizeof(server)) == -1)
	{
		fprintf(stderr, "Can't connect to server\n");
		perror("connect");
		exit(1);
	}

	for (i = 0; i < send_count; i++)
	{
		send(fd_client, sbuf, BUFLEN, 0);
		//printf("transmit: %s\n", sbuf);
		clock_gettime(CLOCK_REALTIME, &start);
		recv(fd_client, rbuf, BUFLEN, 0);
		clock_gettime(CLOCK_REALTIME, &end);
		elapsed += (end.tv_nsec - start.tv_nsec) + (end.tv_sec - start.tv_sec) * 1000000000;
		//printf("receive : %s\n", rbuf);
        mil_sleep(delay_msec);
	}
    
    FILE *fp = fopen( "clientData.csv", "a" );
    // request count, total data transfered (bytes), total elapsed time (sec), average response time (sec)
    fprintf(fp, "%d, %ld, %f, %f\n"
        , send_count, send_count * strlen(msg), elapsed / (float) 1000000000, elapsed / (float) send_count / (float)1000000000);

    fclose(fp);
    
	close(fd_client);
	fflush(stdout);
	return (0);
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
	printf("Usage: %s host [port]\n", arg);
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
 * Signal handler to close the client socket when CTRL-c is received
 * -------------------------------------------------------------------------------------------------*/
void interrupt_handler(int signo)
{
    close(fd_client);
	fprintf(stderr, "Interrupt\n");
}

/*----------------------------------------------------------------------------------------------------
 * FUNCTION:       mil_sleep
 *
 * DATE:           February 28th, 2021
 *
 * REVISIONS:      N/A
 *
 * DESIGNER:       Derek Wong
 *
 * PROGRAMMER:     Derek Wong
 *
 * INTERFACE:      int mil_sleep(long msec)
 *
 * RETURNS:        int
 *
 * NOTES:
 * Nanosleep wrapper function that sleeps in milliseconds
 * -------------------------------------------------------------------------------------------------*/
int mil_sleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}