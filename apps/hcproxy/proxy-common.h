#ifndef PROXYCOMMON_H_
#define PROXYCOMMON_H_

#include "contiki.h"
#include "contiki-net.h"
#include "mapmem.h"
#include "http-common.h"
#include "coap-common.h"


#define PROXY_MAX_MAPPINGS 5 /* maximum number of TCP/UDP connections is 10, defined in uipopt.h */
#define PROXY_HTTP_SERV_DEF_PORT 8080
#define PROXY_COAP_SERV_DEF_PORT 61616

/*State of proxy*/
typedef enum {
	STATE_HTTP_IN,
	STATE_HTTP_OUT,
	STATE_COAP_IN,
	STATE_COAP_OUT,
	STATE_NONE,
} proxy_state_t;

/*This structure contains information about the Http-Coap mapping.*/
typedef struct {
  struct psock sin, sout; /*Protosockets for incoming and outgoing communication*/
  struct pt outputpt;
  struct uip_conn * http_conn;
  struct uip_udp_conn * coap_conn;
  mapmem_t* mapmem;
  char inputbuf[INCOMING_DATA_BUFF_SIZE]; /*to put incoming data in*/
  proxy_state_t state;
  http_request_t http_request;
  http_response_t http_response;
  coap_packet_t coap_request;
  coap_packet_t coap_response;
} proxy_mapping_state_t;

typedef enum {
  PROXY_NO_ERROR,

  /*Memory errors*/
  PROXY_MEMORY_ALLOC_ERR,
  PROXY_MEMORY_BOUNDARY_EXCEEDED,

  /*specific errors*/
  PROXY_URL_INVALID,
} proxy_error_t;

#endif /*PROXYCOMMON_H_*/
