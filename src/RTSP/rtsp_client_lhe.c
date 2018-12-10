// @author: Angel Lopez

#ifndef _RSTP_CLIENT_C_
#define _RSTP_CLIENT_C_

#ifdef  __cplusplus
extern "C" {
#endif

#include "rtsp_lhe.h"

#define SERV_PORT 5554 //default rtsp port is 554 -> see note below 
#define CLI_PORT 5555 //for runnning client and server in the same machine (debug)
/*
from bind() man:
	The port numbers below 1024 are called privileged ports (or sometimes: 
	reserved ports).  Only a privileged process (on Linux: a process that has the
   CAP_NET_BIND_SERVICE capability in the user namespace governing its
   network namespace) may bind(2) to these sockets.

that's why sudo is needed (if running on default rtsp port)
*/

static char * server_ip; 
static int cseq = 0;
static struct sockaddr_in rtsp_serv_addr,rtsp_cli_addr;
static int rtp_socket;
static int sockfd;
static int state;
static char session[10]; //warning!
static int ssrc = 0;
static uint16_t rtp_client_port_lo = 0;
static uint16_t rtp_client_port_hi = 0;
static uint16_t rtp_server_port_lo = 0;
static uint16_t rtp_server_port_hi = 0;
/*	Port 0 trick:
		"On both Windows and Linux, if you bind a socket to port 0,
	 	the kernel will assign it a free port number somewhere above 1024."
	--> default ports = 0
*/
static uint64_t packets_received;
static size_t initial_buffer_size = MAX_DGRAM_SIZE;

static pthread_t tid;

static void error(const char *msg){
	rtsp_request_teardown();
    perror(msg);
	close(sockfd);
	if(state == CLIENT_STATE_PLAYING){
		stop_receiver();
	}

    exit(1);
}

static void sigint_handler(int signo){
	(void)signo; // ignore warning

	close(sockfd);
	if(state == CLIENT_STATE_PLAYING){
		stop_receiver();
	}

	fprintf(stderr, "[RTSP] Received SIGINT!\n"); 
}

void * start_receiver(void *arg){
	*(uint64_t*)arg = LHE_receive_pipe(rtp_socket, initial_buffer_size, ssrc);
	return NULL;
}

void stop_receiver(){
	int err = 1;

	fprintf(stderr,"\tStopping receiver...\n");
	err = pthread_kill(tid, SIGINT);
	sleep(1);
	if(0 == err){
		pthread_join(tid, NULL);
	}
	fprintf(stderr,"\n\tReceiver stopped!\n");
}


bool valid_ip(char* ip_address)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ip_address, &(sa.sin_addr));
    return result != 0;
}

void process_reply(){
	int n;

	char protocol[LENGTH];
    enum RTSP_status_code error_number;
    char reason_phrase[50];

    int server_cseq = 0;

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
    	n = read(sockfd,read_buffer,MAX_DATA-1);  
   		if (n <= 0){
   			fprintf(stderr,"ERROR reading from socket\n");
   			state == CLIENT_STATE_ERROR;
			return;
   		}
    	if(n > 0){
    		memcpy(buffer + pread, read_buffer, n);
   			pread += n;	
   			strcpy(buffer_aux, buffer);
    	}
	}while(strstr(buffer_aux, "\r\n\r\n") == NULL); //read until \r\n\r\n

    //printf("\n[REPLY]\n%s\n", buffer); //debug

    sscanf(buffer,"%s %d %s",protocol, (int*)&error_number, reason_phrase);
    
    bzero(buffer_aux, MAX_DATA);
    strcpy(buffer_aux, buffer);
    search = strstr(buffer_aux, "CSeq: ");
    if(search != NULL){
        p = strtok (search, ": ");
        p = strtok (NULL, "\r\n");
        server_cseq = atoi(p);
    }

    if(server_cseq != cseq){
    	state = CLIENT_STATE_ERROR;
    	perror("Wrong CSeq:");
    	return;
    }

	if(strcmp(protocol, "RTSP/1.0") != 0) {
		state = CLIENT_STATE_ERROR;
		perror("Wrong protocol:");
		return;
	}
	
	if(error_number == RTSP_STATUS_OK){
		switch(state){
			case CLIENT_STATE_INIT: //options ok
				state = CLIENT_STATE_NEGOTIATION;
				fprintf(stderr,"[RTSP STATE]: CLIENT_STATE_NEGOTIATION\n");//debug
				break;
			case CLIENT_STATE_NEGOTIATION://describe ok
				state = CLIENT_STATE_SETUP;
				fprintf(stderr,"[RTSP STATE]: CLIENT_STATE_SETUP\n");//debug
				break;
			case CLIENT_STATE_SETUP://setup ok

				//PARSE SETUP REPLY:
				//Get client port
				bzero(buffer_aux, MAX_DATA);
    			strcpy(buffer_aux, buffer);
    			search = strstr(buffer_aux, "client_port="); //CHECK!
    			if(search != NULL){
    			    p = strtok (search, "=");
	        		p = strtok (NULL, "-");
	        		if(rtp_client_port_lo != atoi(p)){
	        			state = CLIENT_STATE_ERROR;
	        			break;
	        		}
	        		p = strtok (NULL, ";");
	        		if(rtp_client_port_hi != atoi(p)){
	        			state = CLIENT_STATE_ERROR;
	        			break;
	        		}
    			}
    			//Get server port
    			bzero(buffer_aux, MAX_DATA);
    			strcpy(buffer_aux, buffer);
    			search = strstr(buffer_aux, "server_port=");
    			if(search != NULL){
    			    p = strtok (search, "=");
	        		p = strtok (NULL, "-");
	        		rtp_server_port_lo = atoi(p);	    
	        		p = strtok (NULL, ";");
	        		rtp_server_port_hi = atoi(p);	    
    			}
    			//Get SSRC
    			bzero(buffer_aux, MAX_DATA);
    			strcpy(buffer_aux, buffer);
    			search = strstr(buffer_aux, "ssrc=");
    			if(search != NULL){
    			    p = strtok (search, "=");
    			    p = strtok (NULL, "\r\n");
    			    ssrc = atoi(p);  
    			}
    			//Get Session
    			bzero(buffer_aux, MAX_DATA);
    			strcpy(buffer_aux, buffer);
    			search = strstr(buffer_aux, "Session: ");
    			if(search != NULL){
    			    p = strtok (search, ": ");
    			    p = strtok (NULL, "\r\n;");
    			    strcpy(session, p);
    			}

				state = CLIENT_STATE_SERVER_READY;
				break;
			case CLIENT_STATE_CLIENT_READY://play ok
				state = CLIENT_STATE_PLAYING;
				fprintf(stderr,"[RTSP STATE]: CLIENT_STATE_PLAYING\n");//debug
				break;
			case CLIENT_STATE_TEARDOWN://teardown ok
				state = CLIENT_STATE_CLOSING;
				fprintf(stderr,"[RTSP STATE]: CLIENT_STATE_CLOSING\n");//debug
				break;
			default:
				state = CLIENT_STATE_ERROR;//¿?
				fprintf(stderr,"[RTSP STATE]: CLIENT_STATE_ERROR\n");//debug
				perror("Something happened:");
				break;
		}
	}else{
		state = CLIENT_STATE_ERROR;
		perror("SERVER ERROR:");
		return;
	}

    return;
}

int rtsp_get_reply(){

    process_reply(sockfd);
    cseq++;

    return 0;
}

void rtsp_request_header(char head[LENGTH], enum RTSP_method method){
	char method_name[10];

	switch(method) {
		case DESCRIBE:
			sprintf(method_name, "DESCRIBE");
			break;
		case OPTIONS:
			sprintf(method_name, "OPTIONS");
			break;
		case SETUP:
			sprintf(method_name, "SETUP");
			break;
		case PLAY:
			sprintf(method_name, "PLAY");
			break;
		case TEARDOWN:
			sprintf(method_name, "TEARDOWN");
			break;
		case UNKNOWN:
			error("Unknown method");
			break;
	}

	sprintf(head, "%s rtsp://%s:%d/coche1 RTSP/1.0\r\n", method_name, server_ip, SERV_PORT);

	return;
}

int rtsp_send_request(char body[LENGTH], enum RTSP_method method){
	int n;
	char request[2*LENGTH];
	char head[LENGTH];

	bzero(request, 2*LENGTH);
	bzero(head, LENGTH);

	rtsp_request_header(head, method);

	if(body != NULL){
		sprintf(request, "%s%sCSeq: %d\r\n\r\n", head, body, cseq);
	}else{
		sprintf(request, "%sCSeq: %d\r\n\r\n", head, cseq);
	}

	//printf("\n[REQUEST]\n%s\n", request); //debug

 	n=write(sockfd, request, strlen(request));
    if (n < 0){
       error("ERROR writing to socket\n");
       return 1;
    }

    rtsp_get_reply();

    return 0;
}

void rtsp_request_options(){
	//printf("Requesting [OPTIONS]...\n");//debug

	rtsp_send_request(NULL, OPTIONS);

	return;
};

void rtsp_request_describe(){
	//printf("Requesting [DESCRIBE]...\n");//debug

	rtsp_send_request(NULL, DESCRIBE);

	return;
};

void rtsp_request_setup(){
	//printf("Requesting [SETUP]...\n");//debug

	char body[LENGTH];
	bzero(body,LENGTH);

	//get UDP socket for RTP-LHE receiver
	rtp_socket = get_udp_receiver_socket(inet_ntoa(rtsp_serv_addr.sin_addr), &rtp_client_port_lo);
	rtp_client_port_hi = rtp_client_port_lo + 1;

	fprintf(stderr,"LHE RTP receiver to be set @ localhost : %d\n", rtp_client_port_lo);//debug

	sprintf(body, "Transport: RTP/AVP;client_port=%d-%d\r\n", rtp_client_port_lo, rtp_client_port_hi);

	rtsp_send_request(body, SETUP);

	return;
};

void rtsp_request_play(){
	//printf("Requesting [PLAY]...\n");//debug

	char body[LENGTH];
	bzero(body,LENGTH);

	sprintf(body, "Session: %s\r\n", session);

	rtsp_send_request(body, PLAY);

	return;
};

void open_player(){
	fprintf(stderr,"\tOpening LHE receiver on port...\n");//debug
	fprintf(stderr,"\t\tReceiving from sender @ %s : %d\n", inet_ntoa(rtsp_serv_addr.sin_addr), rtp_server_port_lo);//debug
	fprintf(stderr,"\t\tReceiver @ localhost : %d\n", rtp_client_port_lo);//debug


	//start receiver
	if(0 != pthread_create(&tid, NULL, &start_receiver, &packets_received)){
			error("Could not start receiver!");
	}

	state = CLIENT_STATE_CLIENT_READY;
	fprintf(stderr,"[RTSP STATE]: CLIENT_STATE_CLIENT_READY\n");//debug

	return;
};

void rtsp_request_teardown(){
	//printf("Requesting [TEARDOWN]...\n");//debug

	char body[LENGTH];
	bzero(body,LENGTH);

	sprintf(body, "Session: %s\r\n", session);

	rtsp_send_request(body, TEARDOWN);

	return;
};

int close_player(){
	stop_receiver();

	return 0;
};

int rtsp_client_start(uint16_t client_port, char* ip){
	char command[20];
	int retval;

	/* Set handler for SIGINT */
	struct sigaction act;

	act.sa_handler = sigint_handler;
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);
	
	rtp_client_port_lo = client_port;
	server_ip = ip;
	cseq = 1;

	if(!valid_ip(server_ip)){
		error("ERROR: IP adress not valid.");
	}

	fprintf(stderr,"\nLHE RTSP CLIENT\n\n");//debug
	fprintf(stderr,"Calling RTSP server @ %s ...\n", server_ip);//debug

	/* Create TCP socket*/
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
 	if (sockfd < 0){
        error("ERROR opening socket\n");
 	}

	//client
    bzero((char *) &rtsp_cli_addr, sizeof(rtsp_cli_addr));
    rtsp_cli_addr.sin_family = AF_INET; 
    rtsp_cli_addr.sin_addr.s_addr = INADDR_ANY;
    rtsp_cli_addr.sin_port = htons(CLI_PORT);
    if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&rtsp_cli_addr,sizeof(struct sockaddr_in))==-1){
        perror("ERROR: Fallo al reutilizar el puerto.");
    }
    if (bind(sockfd, (struct sockaddr *) &rtsp_cli_addr,sizeof(rtsp_cli_addr)) < 0){
        perror("ERROR on binding");
    }

    //server
    bzero((char *) &rtsp_serv_addr, sizeof(rtsp_serv_addr));
    rtsp_serv_addr.sin_family = AF_INET; 
    rtsp_serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    rtsp_serv_addr.sin_port = htons(SERV_PORT);

    if(connect(sockfd, (struct sockaddr *)&rtsp_serv_addr, sizeof(struct sockaddr)) == -1){
    	error("ERROR: Couldn't connect to server.");
    }

    state = CLIENT_STATE_INIT;
    printf("[RTSP STATE]: INIT\n");//debug


	while(state == CLIENT_STATE_INIT){
		rtsp_request_options();
		if(state == CLIENT_STATE_ERROR){
			error("ERROR: Requesting OPTIONS.");
			state = CLIENT_STATE_CLOSING;
		}
	}	
	while(state == CLIENT_STATE_NEGOTIATION){
		rtsp_request_describe();
		if(state == CLIENT_STATE_ERROR){
			error("ERROR: Requesting DESCRIBE.");
			state = CLIENT_STATE_CLOSING;
		}
	}
	while(state == CLIENT_STATE_SETUP){
		rtsp_request_setup();
		if(state == CLIENT_STATE_ERROR){
			error("ERROR: Requesting SETUP.");
			state = CLIENT_STATE_CLOSING;
		}
	}
	while(state == CLIENT_STATE_SERVER_READY){
		open_player();
		if(state == CLIENT_STATE_ERROR){
			error("ERROR: Opening player.");
			state = CLIENT_STATE_CLOSING;
		}
	}
	while(state == CLIENT_STATE_CLIENT_READY){
		rtsp_request_play();
		if(state == CLIENT_STATE_ERROR){
			error("ERROR: Requesting PLAY.");
			state = CLIENT_STATE_CLOSING;
		}
	}
	
	do{
		fprintf(stderr,"\n\nFor stopping video enter S:\t");
		scanf("%s", command);
	}while(strcmp(command, "S"));
	fprintf(stderr,"\n\n");

	if(state == CLIENT_STATE_PLAYING){
		state = CLIENT_STATE_TEARDOWN;		
		fprintf(stderr,"[RTSP STATE]: CLIENT_STATE_TEARDOWN\n");//debug
	}
	
	
	while(state == CLIENT_STATE_TEARDOWN){
		rtsp_request_teardown();
		if(state == CLIENT_STATE_ERROR){
			error("ERROR: Requesting TEARDOWN.");
			state = CLIENT_STATE_CLOSING;
		}
	}
	if(state == CLIENT_STATE_CLOSING){
		while(close_player());
		close(sockfd);
	}

	return 0;
}

#ifdef  __cplusplus
}
#endif

#endif /* _RSTP_CLIENT_C_ */
