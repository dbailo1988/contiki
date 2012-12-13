#include <stdio.h>
#include <string.h>
#include "proxy-common.h"

#define DEBUG 1
#if DEBUG
#define PRINTF(...)  printf(__VA_ARGS__)
#define PRINT6ADDR(addr)  PRINTF(" %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr)  PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

static mapmem_t mapmems[PROXY_MAX_MAPPINGS];
// used by coap request sending
static int xact_id = 0;


static mapmem_t* find_unused_mapmem()
{
  int i;
  for (i = 0; i < PROXY_MAX_MAPPINGS; i++) {
    if (mapmems[i].mem == NULL) {
      return &mapmems[i];
    }
  }

  return NULL;
}

static void
mapping_state_init(proxy_mapping_state_t* mapping_state, mapmem_t* mapmem)
{
  mapping_state->state = STATE_HTTP_IN;
  mapping_state->mapmem = mapmem;

  http_init_request(&mapping_state->http_request);
  http_init_response(&mapping_state->http_response);
  
  coap_init_packet(&mapping_state->coap_request);
  coap_init_packet(&mapping_state->coap_response);
}

static void
mapping_state_set_http_conn(proxy_mapping_state_t* mapping_state, struct uip_conn * http_conn)
{
  if (http_conn) mapping_state->http_conn = http_conn;
}

static void
mapping_state_set_coap_conn(proxy_mapping_state_t* mapping_state, struct uip_udp_conn * coap_conn)
{
  if (coap_conn) mapping_state->coap_conn = coap_conn;
}

static
uint16_t atouint16(const char* str, int base)
{
  uint16_t re = 0;
  char ch = *str;
  while(ch) {
	if (ch >= '0' && ch <= '9') {
	  re = re * base + (ch - '0');
	} else if (ch >= 'a' && ch <= 'f') {
	  re = re * base + (ch - 'a') + 10;
	} else {
	  return 0;
	}
	++str;
	ch = *str;
  }
  return re;
}

/* put xxxx:0:0:0:0:0:0:0 into ipaddr */
static int
text_to_ipv6(char* text, uip_ipaddr_t* ipaddr)
{
  uint16_t addr[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  int i = 0;
  char* start = text;
  char* tmp;

  tmp = strchr(start, ':');
  while (tmp) {
    *tmp = 0;
    if (strlen(start) > 4) {
      *tmp = ':';
      return 0;
    }
    addr[i++] = atouint16(start, 16);
    *tmp = ':';

    start = ++tmp;
    tmp = strchr(start, ':');
  }

  addr[i++] = atouint16(start, 16);

  if (i != 8) return 0;

  uip_ip6addr(ipaddr, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);

  return 1;
}

static coap_method_t
httpmtd_mapto_coapmtd (http_method_t httpmtd)
{
  if (httpmtd == HTTP_METHOD_GET)
    return COAP_METHOD_GET;

  if (httpmtd == HTTP_METHOD_POST)
	return COAP_METHOD_POST;

  if (httpmtd == HTTP_METHOD_PUT)
	return COAP_METHOD_PUT;

  if (httpmtd == HTTP_METHOD_DELETE)
	return COAP_METHOD_DELETE;

  return COAP_METHOD_UNKNOWN;
}


static
PT_THREAD(http_handle_request(proxy_mapping_state_t* mapping_state))
{
  static http_error_t error;
  const char* content_len;

  PSOCK_BEGIN(&(mapping_state->sin));

  content_len = NULL;

  error = HTTP_NO_ERROR; /*always reinit static variables due to protothreads*/

  // PRINTF("Request--->\n");

  //read method
  PSOCK_READTO(&(mapping_state->sin), ' ');

  if (!http_set_req_method(&mapping_state->http_request, mapping_state->inputbuf)) {
    /*method not handled*/
    http_set_res_status(&mapping_state->http_response, HTTP_SERVICE_UNAVAILABLE_503);
    mapping_state->state = STATE_HTTP_OUT;
  } else {
    /*read until the end of url*/
    PSOCK_READTO(&(mapping_state->sin), ' ');

    /*-1 is needed since it also includes space char*/
    if (mapping_state->inputbuf[PSOCK_DATALEN(&(mapping_state->sin)) - 1] != ' ' ) {
      error = HTTP_URL_TOO_LONG;
    }

    mapping_state->inputbuf[PSOCK_DATALEN(&(mapping_state->sin)) - 1] = 0;

    // PRINTF("Read URL:%s\n", mapping_state->inputbuf);

    if (error == HTTP_NO_ERROR) {
      error = http_set_req_url(&mapping_state->http_request, mapping_state->inputbuf, mapping_state->mapmem);
    }

    if (error != HTTP_NO_ERROR) {
      if (error == HTTP_URL_TOO_LONG) {
        http_set_res_status(&mapping_state->http_response, HTTP_REQUEST_URI_TOO_LONG_414);
      } else {
        http_set_res_status(&mapping_state->http_response, HTTP_BAD_REQUEST_400);
      }

      mapping_state->state = STATE_HTTP_OUT;
    } else {
      /*read until the end of HTTP version - not used yet*/
      PSOCK_READTO(&(mapping_state->sin), LINE_FEED_CHAR);

      // PRINTF("After URL:%s\n", mapping_state->inputbuf);

      /*PSOCK_READTO takes just a single delimiter so I read till the end of line
      but now it may not fit in the buffer. If PSOCK_READTO would take two delimiters,
      we would have read until : and <CR> so it would not be blocked.*/

      /*Read the headers and store the necessary ones*/
      do {
        /*read the next line*/
        PSOCK_READTO(&(mapping_state->sin), LINE_FEED_CHAR);
        mapping_state->inputbuf[ PSOCK_DATALEN(&(mapping_state->sin)) - 1] = 0;

        /*if headers finished then stop the infinite loop*/
        if (mapping_state->inputbuf[0] == CARRIAGE_RETURN_CHAR || mapping_state->inputbuf[0] == 0) {
          // PRINTF("Finished Headers!\n\n");
          break;
        }

        http_set_req_header(&mapping_state->http_request, mapping_state->inputbuf, mapping_state->mapmem);
      }
      while(1);

      content_len = http_get_header(mapping_state->http_request.headers, HTTP_HEADER_NAME_CONTENT_LENGTH);
      if (content_len) {
        mapping_state->http_request.payload_len = atouint16(content_len, 10);

        // PRINTF("Post Data Size string: %s int: %d\n", content_len, mapping_state->http_request.payload_len);
      }

      if (mapping_state->http_request.payload_len) {
        static uint16_t read_bytes = 0;
        /*init the static variable again*/
        read_bytes = 0;

        mapping_state->http_request.payload = mapmem_allocate(mapping_state->mapmem, mapping_state->http_request.payload_len + 1);

        if (mapping_state->http_request.payload) {
          do {
            PSOCK_READBUF(&(mapping_state->sin));
            /*null terminate the buffer in case it is a string.*/
            mapping_state->inputbuf[PSOCK_DATALEN(&(mapping_state->sin))] = 0;

            memcpy(mapping_state->http_request.payload + read_bytes, mapping_state->inputbuf, PSOCK_DATALEN(&(mapping_state->sin)));

            read_bytes += PSOCK_DATALEN(&(mapping_state->sin));

          } while (read_bytes < mapping_state->http_request.payload_len);

          mapping_state->http_request.payload[read_bytes++] = 0;

          // PRINTF("PostData => %s \n", mapping_state->http_request.payload);
        } else {
          error = HTTP_MEMORY_ALLOC_ERR;
        }
      }

      if (error == HTTP_NO_ERROR) {
        mapping_state->state = STATE_COAP_OUT;
      } else {
        // PRINTF("Error:%d\n",error);
        http_set_res_status(&mapping_state->http_response, HTTP_INTERNAL_SERVER_ERROR_500);
        mapping_state->state = STATE_HTTP_OUT;
      }
    }
  }

  PSOCK_END(&(mapping_state->sin));
}

static coap_error_t
coap_send_request(proxy_mapping_state_t* mapping_state, uip_ipaddr_t* serv_ip, uint16_t serv_port)
{
  coap_error_t error = COAP_NO_ERROR;

  struct uip_udp_conn *client_conn = udp_new(serv_ip, UIP_HTONS(serv_port), mapping_state);
  if (client_conn) {
    //udp_bind(client_conn, UIP_HTONS(61617));
	mapping_state_set_coap_conn(mapping_state, client_conn);
	PRINTF("udp connection created!\n");
	mapping_state->coap_request.tid = xact_id++;
	mapping_state->coap_request.type = MESSAGE_TYPE_CON;

	uint8_t buf[COAP_MAX_PAYLOAD_LEN];
	int data_size = coap_serialize_packet(&mapping_state->coap_request, buf);

	if (data_size < COAP_MAX_PAYLOAD_LEN){
	  PRINTF("Client sending request to:[");
	  PRINT6ADDR(&client_conn->ripaddr);
	  PRINTF("]:%u/%s\n", (uint16_t)serv_port, mapping_state->coap_request.url);
	  uip_udp_packet_send(client_conn, buf, data_size);
	  PRINTF("packet sent!\n");
	} else {
	  error = COAP_MEMORY_ALLOC_ERR;
	  // PRINTF("packet is too long!\n");
	}
  } else {
	error = COAP_UDP_CONN_ERR;
	// PRINTF("udp connection can not be established!\n");
  }

  return error;
}

/* the URL format should be coap/host(ip)_port/resource */
static void
httpreq_mapto_coapreq(proxy_mapping_state_t* mapping_state)
{
  proxy_error_t error = PROXY_NO_ERROR;
  uint16_t coap_req_port = 0;
  uip_ipaddr_t coap_req_ipaddr;

  // start with URL: coap/host(ip)_port/resource
  char* url = mapping_state->http_request.url;
  if (strncmp(url, "coap/", 5) != 0) {
    error = PROXY_URL_INVALID;
  }

  char* start, *end, *middle;
  if (error == PROXY_NO_ERROR) {
	  // move to URL: host(ip)_port/resource
	  start = &url[5];
	  end = strchr(start, '/');
	  if (end == NULL || end[1] == 0) {
	    /* invalid url like: coap/host(ip)_port
	     * 				  or coap/host(ip_port)/ */
	    error = PROXY_URL_INVALID;
	  }

	  if (error == PROXY_NO_ERROR) {
		  // parse host(ip)_port
		  *end = 0;
		  middle = strchr(start, '_');
		  if (middle) {
			*middle = 0;
			if (text_to_ipv6(start, &coap_req_ipaddr)) {
		      coap_req_port = atouint16(&middle[1], 10);
			} else {
		      error = PROXY_URL_INVALID;
			}
			*middle = '_';
		  } else {
		    if (text_to_ipv6(start, &coap_req_ipaddr)) {
		      coap_req_port = PROXY_COAP_SERV_DEF_PORT;
		    } else {
		      error = PROXY_URL_INVALID;
		    }
		  }
		  *end = '/';
	  }
  }

  if (error != PROXY_NO_ERROR) {
	// PRINTF("proxy url invalid!\n");
    http_set_res_status(&mapping_state->http_response, HTTP_BAD_REQUEST_400);
	mapping_state->state = STATE_HTTP_OUT;

	return ;
  }

  // coap URL starts from end[1]
  coap_set_method(&mapping_state->coap_request, httpmtd_mapto_coapmtd(mapping_state->http_request.request_type));
  coap_set_option(&mapping_state->coap_request, Option_Type_Uri_Path, strlen(&end[1]), (uint8_t*) &end[1], mapping_state->mapmem);

  if (coap_send_request(mapping_state, &coap_req_ipaddr, coap_req_port) != COAP_NO_ERROR) {
    // PRINTF("coap handle request failed!\n");
	http_set_res_status(&mapping_state->http_response, HTTP_INTERNAL_SERVER_ERROR_500);
    mapping_state->state = STATE_HTTP_OUT;
  } else {
	mapping_state->state = STATE_COAP_IN;
  }
}

//TODO: refine the parameters to PSOCK, http_response_t, mapmem_t
static
PT_THREAD(send_data(proxy_mapping_state_t* mapping_state))
{
  uint16_t index;
  http_response_t* response;
  http_header_t* header;
  char* buffer;

  PSOCK_BEGIN(&(mapping_state->sout));

  // PRINTF("send_data -> \n");

  index = 0;
  response = &mapping_state->http_response;
  PRINTF("payload: %d\n", response->payload_len);
  header = response->headers;
  buffer = (char*)mapmem_allocate(mapping_state->mapmem, 100);


  /*what is the best solution here to send the data. Right now, if buffer is not allocated, no data is sent!*/
  if (buffer) {
    index += sprintf(buffer + index, "%s %d %s%s", httpv1_1, response->status_code, response->status_string, line_end);
    for (;header;header = header->next) {
      // PRINTF("header %u \n", (uint16_t)header);
      index += sprintf(buffer + index, "%s%s%s%s", header->name, header_delimiter, header->value, line_end);
    }
    index += sprintf(buffer + index, "%s", line_end);

    memcpy(buffer + index, response->payload, response->payload_len);
    index += response->payload_len;
    buffer[index] = 0;

    PRINTF("Sending Data(size %d) %s \n", index, buffer);
    PSOCK_SEND(&(mapping_state->sout), (uint8_t*)buffer, index);
//    char* tmp = "HTTP/1.1 200 OK";
//    PSOCK_SEND(&(mapping_state->sout), (uint8_t*)tmp, strlen(tmp));
  } else {
    PRINTF("buffer error when sending data!\n");
  }

  PSOCK_END(&(mapping_state->sout));
}

static
PT_THREAD(http_handle_response(proxy_mapping_state_t* mapping_state))
{
  PT_BEGIN(&(mapping_state->outputpt));

  PRINTF("handle_response ->\n");

  http_set_res_header(&mapping_state->http_response, HTTP_HEADER_NAME_CONNECTION, close, 0, mapping_state->mapmem);
  http_set_res_header(&mapping_state->http_response, HTTP_HEADER_NAME_SERVER, contiki, 0, mapping_state->mapmem);


  PT_WAIT_THREAD(&(mapping_state->outputpt), send_data(mapping_state));

  PRINTF("<-- handle_response\n\n\n");

  PSOCK_CLOSE(&(mapping_state->sout));
  mapping_state->state = STATE_HTTP_IN;

  PT_END(&(mapping_state->outputpt));
}

//TODO: it should be improved to handle multiple packets
static
void coapres_mapto_httpres(proxy_mapping_state_t* mapping_state)
{
  if(uip_newdata() && uip_udp_conn == mapping_state->coap_conn) {
	PRINTF("Incoming packet size: %u \n", (uint16_t)uip_datalen());

	uip_udp_remove(uip_udp_conn);
    coap_parse_message(&mapping_state->coap_response, uip_appdata, uip_datalen(), mapping_state->mapmem);
    http_set_res_payload(&mapping_state->http_response, mapping_state->coap_response.payload, mapping_state->coap_response.payload_len, 0, mapping_state->mapmem);
    http_set_res_status(&mapping_state->http_response, HTTP_OK_200);
    tcpip_poll_tcp(mapping_state->http_conn);
    mapping_state->state = STATE_HTTP_OUT;
  }
}

static void
handle_mapping(proxy_mapping_state_t* mapping_state)
{
  if (mapping_state->state == STATE_HTTP_IN) {
	http_handle_request(mapping_state);
  }

  if (mapping_state->state == STATE_COAP_OUT) {
	httpreq_mapto_coapreq(mapping_state);
  }

  if (mapping_state->state == STATE_COAP_IN) {
    coapres_mapto_httpres(mapping_state);
  }

  if (mapping_state->state == STATE_HTTP_OUT) {
    http_handle_response(mapping_state);
  }
}


PROCESS_NAME(hcproxy);

PROCESS(hcproxy, "HCProxy Process");

PROCESS_THREAD(hcproxy, ev, data)
{
  static proxy_mapping_state_t *mapping_state;

  PROCESS_BEGIN();

  tcp_listen(uip_htons(PROXY_HTTP_SERV_DEF_PORT));

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == tcpip_event);

    mapping_state = (proxy_mapping_state_t *)data;


    if (uip_conn) {
    	PRINTF("receiving TCP data from:[");
        PRINT6ADDR(&uip_conn->ripaddr);
        PRINTF("]:%u \n", UIP_HTONS(uip_conn->rport));
    } else if (uip_udp_conn) {
    	PRINTF("receiving UDP data from:[");
    	PRINT6ADDR(&uip_udp_conn->ripaddr);
    	PRINTF("]:%u \n", UIP_HTONS(uip_udp_conn->rport));
    }

    if(uip_connected()) {
      PRINTF("New Connection!\n");
      
      mapmem_t* mapmem = find_unused_mapmem();

      if (mapmem && mapmem_init(mapmem, HTTP_DATA_BUFF_SIZE)) {
    	mapping_state = (proxy_mapping_state_t*)mapmem_allocate(mapmem, sizeof(proxy_mapping_state_t));
    	if (mapping_state) {
    	  mapping_state_init(mapping_state, mapmem);
    	  mapping_state_set_http_conn(mapping_state, uip_conn);
    	  tcp_markconn(uip_conn, mapping_state);

    	  PSOCK_INIT(&(mapping_state->sin), (uint8_t*)mapping_state->inputbuf, sizeof(mapping_state->inputbuf) - 1);
    	  PSOCK_INIT(&(mapping_state->sout), (uint8_t*)mapping_state->inputbuf, sizeof(mapping_state->inputbuf) - 1);

    	  handle_mapping(mapping_state);
    	}
      } else {
    	  // PRINTF("No enough memory. Aborting!");
    	  uip_abort();
      }
    } else if (uip_aborted() || uip_closed() || uip_timedout()) {
      if (mapping_state) {
    	PRINTF("clean mapping_state!\n");
    	mapmem_delete(mapping_state->mapmem);

        /*Following 2 lines are needed since this part of code is somehow executed twice so it tries to free the same region twice.
        Potential bug in uip*/
    	mapping_state = NULL;
        tcp_markconn(uip_conn, NULL);
      }
    } else {
      if (uip_newdata()) {
    	  PRINTF("new data!\n");
    	  PRINTF("state: %d\n", mapping_state->state);
      }
      handle_mapping(mapping_state);
    }
  }

  PROCESS_END();
}
