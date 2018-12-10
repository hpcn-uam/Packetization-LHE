// @author: Angel Lopez

#ifndef _RSTP_SERVER_C_
#define _RSTP_SERVER_C_

#ifdef  __cplusplus
extern "C" {
#endif

#include "rtsp_lhe.h"

#define SERV_PORT 5554 //default rtsp port is 554 -> see note below 
/*
from bind() man:
	The port numbers below 1024 are called privileged ports (or sometimes: 
	reserved ports).  Only a privileged process (on Linux: a process that has the
   CAP_NET_BIND_SERVICE capability in the user namespace governing its
   network namespace) may bind(2) to these sockets.

that's why sudo is needed (if running on default rtsp port)
*/
#define DIRECTORY "/home/hpcn/Packetization-LHE/data/mockvid_povcar_LHE_360p_30fps_128_8_yuv420p_5s/"

static volatile bool stop = false;

static int cseq = 0;
static struct sockaddr_in rtsp_serv_addr, rtsp_cli_addr;
static int rtp_socket;
static int sockfd;
static int newsockfd = 0;
static int state;
static char session[10]; //warning!
static int ssrc = 0;
//RTP ports
static uint16_t rtp_client_port_lo = 0; 
static uint16_t rtp_client_port_hi = 0;
static uint16_t rtp_server_port_lo = 0;
static uint16_t rtp_server_port_hi = 0;
/*	Port 0 trick:
		"On both Windows and Linux, if you bind a socket to port 0,
	 	the kernel will assign it a free port number somewhere above 1024."
	--> default ports = 0
*/
static uint64_t packets_sent = 0;
static size_t initial_buffer_size = MAX_DGRAM_SIZE;

static pthread_t tid;

static void error(const char *msg){
	perror(msg);
    close(newsockfd);
	close(sockfd);
	close(rtp_socket);
	if(state == SERVER_STATE_PLAYING){
		stop_sender();
	}

    exit(1);
}

static void sigint_handler(int signo){
	(void)signo; // ignore warning

	close(newsockfd);
	close(sockfd);
	close(rtp_socket);
	if(state == SERVER_STATE_PLAYING){
		stop_sender();
	}

	fprintf(stderr, "[RTSP] Received SIGINT!\n"); 
	stop = true;
}

void * start_sender(void *arg){
	srand(time(NULL));
	uint16_t initial_seq_num = rand();
	uint32_t initial_ts = 0;

	//*(uint64_t*)arg = LHE_send_dir(DIRECTORY, rtp_socket, initial_buffer_size, initial_seq_num, initial_ts, ssrc);
	*(uint64_t*)arg = LHE_send_mem(rtp_socket, initial_buffer_size, initial_seq_num, initial_ts, ssrc);

	return NULL;
}

void stop_sender(){
	int err = 1;

	printf("\tStopping sender...");
	err = pthread_kill(tid, SIGINT);
	close(rtp_socket);
	sleep(1);
	if(0 == err){
		pthread_join(tid, NULL);
	}
	printf("\n\tSender stopped!\n");
}


void rtsp_reply_header(char head[LENGTH], enum RTSP_status_code error_number){
	char reason_phrase[50];																													

	switch(error_number) {
		case RTSP_STATUS_OK:
			sprintf(reason_phrase, "OK");
			break;
		case RTSP_STATUS_METHOD:
			sprintf(reason_phrase, "Method Not Allowed");
			break;
		case RTSP_STATUS_BANDWIDTH:
			sprintf(reason_phrase, "Not Enough Bandwidth");
			break;
		case RTSP_STATUS_SESSION:
			sprintf(reason_phrase, "Session Not Found");
			break;
		case RTSP_STATUS_STATE:
			sprintf(reason_phrase, "Method Not Valid in This State");
			break;
		case RTSP_STATUS_AGGREGATE:
			sprintf(reason_phrase, "Aggregate operation not allowed");
			break;
		case RTSP_STATUS_ONLY_AGGREGATE:
			sprintf(reason_phrase, "Only aggregate operation allowed");
			break;
		case RTSP_STATUS_TRANSPORT:
			sprintf(reason_phrase, "Unsupported transport");
			break;
		case RTSP_STATUS_INTERNAL:
			sprintf(reason_phrase, "Internal Server Error");
			break;
		case RTSP_STATUS_NOT_IMPLEMENTED:
			sprintf(reason_phrase, "Not Implemented");
			break;
		case RTSP_STATUS_SERVICE:
			sprintf(reason_phrase, "Service Unavailable");
			break;
		case RTSP_STATUS_VERSION:
			sprintf(reason_phrase, "RTSP Version not supported");
			break;
		default:
			sprintf(reason_phrase, "Unknown Error");
			break;
	}

	sprintf(head, "RTSP/1.0 %d %s\r\n",error_number, reason_phrase);

	return;
}

int rtsp_send_reply(char options[LENGTH], char body[2*LENGTH], enum RTSP_status_code error_number){
	int n;
	char reply[3*LENGTH];
	char head[LENGTH];

	bzero(reply, 3*LENGTH);
	bzero(head, LENGTH);

	rtsp_reply_header(head, error_number);

	if(body != NULL){
		sprintf(reply, "%s%sCSeq: %d\r\n\r\n%s", head, options, cseq, body);
	}else if(options != NULL){
		sprintf(reply, "%s%sCSeq: %d\r\n\r\n", head, options, cseq);
	}else{
		sprintf(reply, "%sCSeq: %d\r\n\r\n", head, cseq);
	}

	//printf("\n[REPLY]\n%s\n", reply); //debug

 	n=write(newsockfd, reply, strlen(reply));
    if (n < 0){
       error("ERROR writing to socket\n");
       return 1;
    }
    return 0;
}

void rtsp_method_describe(){
	char options[LENGTH];
	bzero(options,LENGTH);
	char body[2*LENGTH];
	bzero(body,2*LENGTH);

	char * sdp = "v=0\r\no=LHE_Server 123 0 IN IP4 127.0.0.1\r\ns=LHE\r\nt=0 0\r\nm=video 6660 RTP/AVP 124\r\na=rtpmap:124 H264/3000\r\na=control:rtsp://127.0.0.1:8884/coche1/trackID=12\r\n";

	sprintf(options, "Server: RTSP-RTP-LHE\r\nContent-Type: application/sdp\r\nContent-Base: rtsp://150.244.58.163:%d/coche1\r\nContent-Length:%zu\r\nCache-Control: no-cache\r\n", SERV_PORT, strlen(sdp));
	sprintf(body, "%s", sdp);

	
	rtsp_send_reply(options, body, RTSP_STATUS_OK); //if

	return;
}

void rtsp_method_options(){
	char options[LENGTH];
	bzero(options,LENGTH);

	sprintf(options, "Public: OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN\r\n");
	
	rtsp_send_reply(options, NULL, RTSP_STATUS_OK); //if

	return;
}

void rtsp_method_setup(char buffer[MAX_DATA]){
	char options[LENGTH];
	bzero(options,LENGTH);

	char buffer_aux[MAX_DATA];
	char * search;
    char * p;

	if(state == SERVER_STATE_INIT){
		//Get client port
		bzero(buffer_aux, MAX_DATA);
	    strcpy(buffer_aux, buffer);
	    search = strstr(buffer_aux, "client_port=");
	    if(search != NULL){
	        p = strtok (search, "=");
	        p = strtok (NULL, "-");
	        rtp_client_port_lo = atoi(p);
	        p = strtok (NULL, ";");
	        rtp_client_port_hi = atoi(p);
	    }

		//get UDP socket for RTP-LHE sender
		rtp_socket = get_udp_sender_socket(inet_ntoa(rtsp_cli_addr.sin_addr), rtp_client_port_lo, &rtp_server_port_lo);
		rtp_server_port_hi = rtp_server_port_lo + 1;

		printf("LHE RTP sender to be set @ localhost : %d\n", rtp_server_port_lo);//debug

	    //reply
		sprintf(options, "Transport: RTP/AVP;client_port=%d-%d;server_port=%d-%d;ssrc=%d\nSession: %s\r\n", rtp_client_port_lo, rtp_client_port_hi, rtp_server_port_lo, rtp_server_port_hi, ssrc, session);
		rtsp_send_reply(options, NULL, RTSP_STATUS_OK); //if

		state = SERVER_STATE_READY;
		printf("[RTSP STATE]: READY\n");//debug
	}else{
		rtsp_send_reply(NULL, NULL, RTSP_STATUS_NOT_IMPLEMENTED);
	}

	return;
}

void rtsp_method_play(char buffer[MAX_DATA])
{
	char options[LENGTH];
	bzero(options,LENGTH);

    char buffer_aux[MAX_DATA];
    char * search;
    char * p;

    char client_session[10]; //warning!

	
	if(state == SERVER_STATE_READY){

		bzero(buffer_aux, MAX_DATA);
	    strcpy(buffer_aux, buffer);
	    search = strstr(buffer_aux, "Session: ");
	    if(search != NULL){
	    	p = strtok (search, ": ");
		    p = strtok (NULL, "\r\n;");    			    	
	        strcpy(client_session, p);
	    }
	    /*
	    if(strcmp(client_session, session) != 0) {
	    	printf("wrong session: '%s' ('%s')\n", client_session, session);
			rtsp_send_reply(NULL, NULL, RTSP_STATUS_SESSION);
			return;
		}
*/
		//start sender
		printf("\tStarting sender...\n");//debug
		printf("\t\tSending to receiver @ %s : %d\n", inet_ntoa(rtsp_cli_addr.sin_addr), rtp_client_port_lo);//debug
		printf("\t\tSender @ localhost : %d\n", rtp_server_port_lo);//debug

		//start sender
		if(0 != pthread_create(&tid, NULL, &start_sender, &packets_sent)){
			error("Could not start sender!");
		}

		//reply
		sprintf(options, "Session: %s\r\n", session);
		rtsp_send_reply(options, NULL, RTSP_STATUS_OK); //if

		state = SERVER_STATE_PLAYING;
		printf("[RTSP STATE]: PLAYING\n");//debug

	}else{
		rtsp_send_reply(NULL, NULL, RTSP_STATUS_NOT_IMPLEMENTED);
	}

	return;
}

void rtsp_method_pause(){
	printf("\tWARNING: client queried PAUSE (client ignored server's OPTIONS)\n");//debug

	printf("\tTearing down...\n");//debug
	
	if(state == SERVER_STATE_PLAYING){
		//stop sender
		stop_sender();	
	}

	rtsp_send_reply(NULL, NULL, RTSP_STATUS_METHOD);

	close(newsockfd);
	
	state = SERVER_STATE_INIT;
	printf("[RTSP STATE]: INIT\n");//debug

	while (!stop){
    	if (stop) {
			break;
		}

        rtsp_wait_client();
    }
	
	return;
}	

void rtsp_method_teardown(){
	char options[LENGTH];
	bzero(options,LENGTH);

	printf("\tTearing down...\n");//debug
	
	if(state == SERVER_STATE_PLAYING){
		//stop sender
		stop_sender();	
	}

	//reply	
	rtsp_send_reply(options, NULL, RTSP_STATUS_OK); //if
	
	close(newsockfd);
	
	state = SERVER_STATE_INIT;
	printf("[RTSP STATE]: INIT\n");//debug

	while (!stop){
    	if (stop) {
			break;
		}

        rtsp_wait_client();
    }

	
	return;
}

void process_request(int newsockfd){
	int n;

	char protocol[LENGTH];
    char method[LENGTH];
    char url[LENGTH];

    char read_buffer[MAX_DATA];
    char buffer[MAX_DATA];
    char buffer_aux[MAX_DATA];

    int pread = 0;

    char * search;
    char * p;

    bzero(read_buffer,MAX_DATA);
	bzero(buffer,MAX_DATA);
	bzero(buffer_aux,MAX_DATA);
	
	do{
    	n = read(newsockfd,read_buffer,MAX_DATA-1);  
   		if (n <= 0){
   			printf("ERROR reading from socket\n");
   			close(newsockfd);
   			rtsp_wait_client();
   		}
    	if(n > 0){
    		memcpy(buffer + pread, read_buffer, n);
   			pread += n;	
   			strcpy(buffer_aux, buffer);
    	}
	}while(strstr(buffer_aux, "\r\n\r\n") == NULL); //read until \r\n\r\n

    //printf("\n[REQUEST]\n%s\n", buffer); //debug

    sscanf(buffer,"%s %s %s",method, url, protocol);
    
    bzero(buffer_aux, MAX_DATA);
    strcpy(buffer_aux, buffer);
    search = strstr(buffer_aux, "CSeq: ");
    if(search != NULL){
        p = strtok (search, ": ");
        p = strtok (NULL, "\r\n");
        cseq = atoi(p);
    }

	if(strcmp(protocol, "RTSP/1.0") != 0) {
		rtsp_send_reply(NULL, NULL, RTSP_STATUS_VERSION);
		return;
	}
	
	// handle methods
	if (!strcmp(method, "DESCRIBE"))
		rtsp_method_describe();
	else if (!strcmp(method, "OPTIONS"))
		rtsp_method_options();
	else if (!strcmp(method, "SETUP"))
		rtsp_method_setup(buffer);
	else if (!strcmp(method, "PLAY"))
		rtsp_method_play(buffer);
	else if (!strcmp(method, "PAUSE"))
		rtsp_method_pause();	
	else if (!strcmp(method, "TEARDOWN"))
		rtsp_method_teardown();
	else
		rtsp_send_reply(NULL, NULL, RTSP_STATUS_METHOD);	

    return;
}

void rtsp_wait_client(){
	int n;

	if (listen(sockfd, MAX_CONNECTION) == -1){
        error("ERROR: Listen.\n");
        close(sockfd);
    }

    n = sizeof(struct sockaddr_in);
    if ((newsockfd=accept(sockfd,(struct sockaddr*)&rtsp_cli_addr,(socklen_t *) &n)) == -1){
        error("ERROR: Accept.\n");
    }

    while (!stop){
    	if (stop) {
			break;
		}
        process_request(newsockfd);
    }
}

int rtsp_server_start(uint16_t server_port){
	srand(time(NULL));

	/* Set handler for SIGINT */
	struct sigaction act;

	act.sa_handler = sigint_handler;
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	rtp_server_port_lo = server_port;
	strcpy(session, "1234LHE"); //TO DO (random string)
	ssrc = rand();

	printf("\nLHE RTSP SERVER\n\n");//debug
	printf("Waiting for client...\n");//debug

	/* Create TCP socket*/
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
 	if (sockfd < 0)
        error("ERROR opening socket\n");

    bzero((char *) &rtsp_serv_addr, sizeof(rtsp_serv_addr));
    rtsp_serv_addr.sin_family = AF_INET; 
    rtsp_serv_addr.sin_addr.s_addr = INADDR_ANY;
    rtsp_serv_addr.sin_port = htons(SERV_PORT);
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&rtsp_serv_addr,sizeof(struct sockaddr_in))==-1){
        error("ERROR: Fallo al reutilizar el puerto.\n");
    }
    if (bind(sockfd, (struct sockaddr *) &rtsp_serv_addr,sizeof(rtsp_serv_addr)) < 0){
        error("ERROR on binding");
    }

    
    state = SERVER_STATE_INIT;
    printf("[RTSP STATE]: INIT\n");//debug
    
    while (!stop){
    	if (stop) {
			break;
		}
        rtsp_wait_client();
    }

	close(sockfd);
	close(newsockfd);
	close(rtp_socket);

    return 0;
}
	

#ifdef  __cplusplus
}
#endif

#endif /* _RSTP_SERVER_C_ */
