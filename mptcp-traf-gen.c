
/* 
	
*/

#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <netdb.h>
#include <netinet/in.h>

#include <arpa/inet.h>
#include <sys/time.h> 
#include <signal.h> 
#include <linux/tcp.h>

#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP 262
#endif
#ifndef SOL_MPTCP
#define SOL_MPTCP 284
#endif

// port 
#define TEST_PORT 15432
#define BUFF_SIZE 256
#define MICRO_SEC 1000000

static void die_perror(const char *msg)
{
	perror(msg);
	exit(1);
}

// catch interruptions 
static volatile int keepRunning = 1;

void intHandler(int dummy) {
    keepRunning = 0;
}

/**
*  buffer = received data ,  
*  return -1 on error, segment delay otherway  
*/
static long get_segment_delay(char* buffer, long receive_t)
{
		
	char* buff_ptr = buffer; 
	while (! (*buff_ptr)) buff_ptr ++ ; 	  // search for timestamp 
	long result = strtol(buff_ptr, NULL, 10); // convert to long  
	if 	( result < 100000000000000) return -1; 					  
	
	// return segment app delay 		
	return receive_t - result; 
}



/**
*  server listen on port "15432"
*  interval : in sec to write result , result if csv format : num seg, incoming time in micro , largest app delay on interval 
*  out_file : name of file to write the results .. not implemented yeat 
*/

static int server(double interval, bool verbose, const char* out_file)
{
	
	struct sockaddr_in serv_addr, cli_addr;
    int sockfd, newsockfd;
	socklen_t clilen; 

	// read buffer 
	int n; 
	char buffer[BUFF_SIZE];
	
	// interval file write on file  
	long write_interval_tv = interval * MICRO_SEC;  // 10 mili 
			
	// create a MPTCP socket
    sockfd =  socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP);
    if (sockfd < 0) 
        die_perror("ERROR opening socket");

    // clear address structure
	bzero((char *) &serv_addr, sizeof(serv_addr));

    /* setup the host_addr structure for use in bind call */
    // server byte order
    serv_addr.sin_family = AF_INET;  

    // automatically be filled with current host's IP address
    serv_addr.sin_addr.s_addr = INADDR_ANY;  

    // convert short integer value for port must be converted into network byte order
    serv_addr.sin_port = htons(TEST_PORT);

     // This bind() call will bind  the socket to the current IP address on port, portno
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
            die_perror("ERROR on binding");

     // This listen() call tells the socket to listen to the incoming connections.
    listen(sockfd,5);

     // The accept() call actually accepts an incoming connection
    clilen = sizeof(cli_addr);
  
	while (true){
		 // The accept() returns a new socket file descriptor for the accepted connection.
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0) 
			  die_perror("ERROR on accept");

		bzero(buffer,BUFF_SIZE);
		long count_seg = 0;	// tottal received segments 
		long max_delay = 0; // max delay on interval 
		
		struct timeval tv;
		gettimeofday(&tv,NULL);
			
		long init_session_tv;	// tv mptcp session start's 
		init_session_tv = tv.tv_sec * MICRO_SEC + tv.tv_usec;
	
		long next_write_tv = init_session_tv; 
		
		int results_fd = 0; 
		if (out_file){
			// Open file to rewrite results, create if not exists 
			results_fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		}
		
		while ( (n = read(newsockfd,buffer,BUFF_SIZE) ) > 0 )  {
			// printf("%ld Redded %d bytes \n", count_seg++, n);
			// get server time when the segment was received  
			struct timeval tv;
			gettimeofday(&tv,NULL);
			long receive_t = tv.tv_sec * MICRO_SEC + tv.tv_usec;
			
			// get segment app delay 
			long seg_delay = get_segment_delay(buffer, receive_t);
			if (seg_delay < 0 ) continue; 
			
			// get max delay on time interval  
			max_delay = (seg_delay > max_delay)? seg_delay : max_delay; 
			
			// write to file if is time to do it 
			if (receive_t > next_write_tv){
				// set next time to write
				next_write_tv = receive_t + write_interval_tv;  
				bzero(buffer,BUFF_SIZE);
				sprintf (buffer, "%ld , %ld , %ld \n",  count_seg++,  receive_t - init_session_tv, max_delay); 
				
				if (verbose) 
					fprintf (stdout,"%s", buffer);
				
				if (results_fd)	
					write (results_fd, buffer, sizeof(buffer));
				
				max_delay = 0; 			
			}
		}
		if (n < 0) die_perror("ERROR reading from socket");
		
		fprintf (stdout,"Total samples received : %ld \n", count_seg); 
		close(newsockfd);
		if (results_fd) close (results_fd);
	}
	
    close(sockfd);
	 
	return 0;
}

/**
*  socket fd, time in sec 
*/

static void gen_traffic (int sockfd, int gen_sec){

	unsigned long seg_num = 0; 			// number segments sended  
	int n; 	
	char buffer[BUFF_SIZE];
	
	struct timeval tv;
	gettimeofday(&tv,NULL);
	// tv.tv_sec // seconds
	// tv.tv_usec // microseconds
	// end.tv_sec * 1000000 + end.tv_usec
	
	// sec to gen traffic
	long end_tv_sec = tv.tv_sec + gen_sec;
		
	while (tv.tv_sec < end_tv_sec){
		bzero(buffer,BUFF_SIZE);
		gettimeofday(&tv,NULL);
		long tt_t = tv.tv_sec * 1000000 + tv.tv_usec; 
		sprintf (buffer, "%ld" , tt_t);
		n = write(sockfd,buffer,BUFF_SIZE);
		if (n < 0) 
			die_perror("ERROR writing to socket");
		seg_num ++ ; 
		
		//usleep(2500);		//sleeps for microsecond
	}
	
	printf ("Total segments sended : %ld \n", seg_num); 
	
}

/**
* Open socket on Client side, connect to remote_addr
*/
static int client(const char* remote_addr, int gen_time)
{
	int sockfd, n;
    struct sockaddr_in serv_addr;
    
	// open MPTCP socket 
	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_MPTCP); // IPPROTO_MPTCP
    if (sockfd < 0) 
        die_perror("ERROR opening socket");
	
	
	// This is to set Nagle buffering off
	int yes = 1;
	int result = setsockopt(sockfd,
                        SOL_SOCKET,	// IPPROTO_TCP , SOL_SOCKET is the socket layer itself. It is used for options that are protocol independent.
                        TCP_NODELAY,
                        (char *) &yes, 
                        sizeof(int));    // 1 - on, 0 - off
	// handle the error
	if (result < 0)
		die_perror ("Error Off NODELAY");


	// set server addr and port 
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(remote_addr);		 
    serv_addr.sin_port = htons(TEST_PORT);
    
	if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        die_perror("ERROR connecting");
	
	// generate traffic for N sec 
	gen_traffic (sockfd, gen_time); 

    close(sockfd);
    return 0;
}


int main(int argc, char *argv[])
{
	
	//signal(SIGINT, intHandler); // interuprion handler 
	
	int c;
	int gen_time = 0 ; 				// no gen traffic by def 
	bool is_client = false;
    char* remote_addr =  NULL; 		// addr to connect to 
	bool verbose = false; 			// print to console on server 
	double interval = 0.1; 			// disolay/write interval 100 milisec
	char* out_file = "tmp-file.csv"; // file to write results 
		
	// parsing arg : https://stackoverflow.com/questions/17877368/getopt-passing-string-parameter-for-argument
	while ((c = getopt(argc, argv, "hsvc:t:i:")) != -1) {
		
		switch (c) {
			case 'h':
				//die_usage(0);
				break;
			case 's':
				is_client = false; 
				break;
			case 'c':
				is_client = true;
				remote_addr = strdup(optarg); 				
				break;
			case 't':
				gen_time = atoi(optarg); 
				break;
			case 'i':
				interval = atof(optarg); 
				break;
			case 'v':
				verbose = true;  
				break;
			default:
				fprintf(stderr, "Usage: %s [-s or -c or -t] \n", argv[0]);
				exit(EXIT_FAILURE);
				break;
		}
	}
	
	if (is_client){
		printf ("Client Mode:\n server addr : %s, gen_time %d sec\n ", remote_addr, gen_time); 
		client(remote_addr,gen_time); 		// client run for N sec
	}
	
	else {
		printf("Server Mode : inetrval %lf, verbose %d \n", interval, verbose);
		server(interval,verbose, out_file); // server run 
	}
	
	if (remote_addr) free(remote_addr); 
	
	return 0;
}
