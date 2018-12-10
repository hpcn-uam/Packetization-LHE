// @author: Angel Lopez

#ifndef _RSTP_LHE_H_
#define _RSTP_LHE_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>
#include <time.h>

#include <pthread.h>

#include "utils.h"
#include "LHE_pack.h"

#define MAX_DATA 10000
#define LENGTH 200
#define MAX_CONNECTION 1

enum RTSP_client_state {
	CLIENT_STATE_INIT = 0,
	CLIENT_STATE_NEGOTIATION,
	CLIENT_STATE_SETUP,
	CLIENT_STATE_SERVER_READY,
	CLIENT_STATE_CLIENT_READY,
	CLIENT_STATE_PLAYING,
	CLIENT_STATE_TEARDOWN,
	CLIENT_STATE_CLOSING,
	CLIENT_STATE_ERROR
};

enum RTSP_server_state {
	SERVER_STATE_INIT = 0,
	SERVER_STATE_READY,
	SERVER_STATE_PLAYING
};

/* RTSP handling */
enum RTSP_status_code {
	RTSP_STATUS_OK              =200, /**< OK */
	RTSP_STATUS_METHOD          =405, /**< Method Not Allowed */
	RTSP_STATUS_BANDWIDTH       =453, /**< Not Enough Bandwidth */
	RTSP_STATUS_SESSION         =454, /**< Session Not Found */
	RTSP_STATUS_STATE           =455, /**< Method Not Valid in This State */
	RTSP_STATUS_AGGREGATE       =459, /**< Aggregate operation not allowed */
	RTSP_STATUS_ONLY_AGGREGATE  =460, /**< Only aggregate operation allowed */
	RTSP_STATUS_TRANSPORT       =461, /**< Unsupported transport */
	RTSP_STATUS_INTERNAL        =500, /**< Internal Server Error */
	RTSP_STATUS_NOT_IMPLEMENTED	=501, /**< Not Implemented */
	RTSP_STATUS_SERVICE         =503, /**< Service Unavailable */
	RTSP_STATUS_VERSION         =505, /**< RTSP Version not supported */
};

enum RTSP_method {
    DESCRIBE,
    OPTIONS,
    SETUP,
    PLAY,
    TEARDOWN,
    UNKNOWN = -1,
};

int rtsp_server_start(uint16_t server_port);
int rtsp_client_start(uint16_t client_port, char* ip);
bool valid_ip(char* ip_address);
int close_player();
int rtsp_get_reply();
int rtsp_send_reply(char options[LENGTH], char body[LENGTH], enum RTSP_status_code error_number);
int rtsp_send_request(char body[LENGTH], enum RTSP_method method);
void open_player();
void process_reply();
void process_request(int newsockfd);
void rtsp_method_describe();
void rtsp_method_options();
void rtsp_method_play(char buffer[MAX_DATA]);
void rtsp_method_setup(char buffer[MAX_DATA]);
void rtsp_method_teardown();
void rtsp_reply_header(char head[LENGTH], enum RTSP_status_code error_number);
void rtsp_request_describe();
void rtsp_request_header(char head[LENGTH], enum RTSP_method method);
void rtsp_request_options();
void rtsp_request_play();
void rtsp_request_setup();
void rtsp_request_teardown();
void rtsp_wait_client();
void * start_receiver(void *arg);
void * start_sender(void *arg);
void stop_receiver();
void stop_sender();



#ifdef  __cplusplus
}
#endif

#endif /* _RSTP_LHE_H_ */
