#include <string.h>
#include "http-common.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#define PRINT6ADDR(addr) PRINTF(" %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x ", ((uint8_t *)addr)[0], ((uint8_t *)addr)[1], ((uint8_t *)addr)[2], ((uint8_t *)addr)[3], ((uint8_t *)addr)[4], ((uint8_t *)addr)[5], ((uint8_t *)addr)[6], ((uint8_t *)addr)[7], ((uint8_t *)addr)[8], ((uint8_t *)addr)[9], ((uint8_t *)addr)[10], ((uint8_t *)addr)[11], ((uint8_t *)addr)[12], ((uint8_t *)addr)[13], ((uint8_t *)addr)[14], ((uint8_t *)addr)[15])
#define PRINTLLADDR(lladdr) PRINTF(" %02x:%02x:%02x:%02x:%02x:%02x ",(lladdr)->addr[0], (lladdr)->addr[1], (lladdr)->addr[2], (lladdr)->addr[3],(lladdr)->addr[4], (lladdr)->addr[5])
#else
#define PRINTF(...)
#define PRINT6ADDR(addr)
#define PRINTLLADDR(addr)
#endif

/*needed for web services giving all path (http://172.16.79.0/services/light1)
 * instead relative (/services/light1) in HTTP request. Ex: Restlet lib.*/
const char* http_string = "http";

/*HTTP method strings*/
const char* http_get_string = "GET";
const char* http_head_string = "HEAD";
const char* http_post_string = "POST";
const char* http_put_string = "PUT";
const char* http_delete_string = "DELETE";

const char* httpv1_1 = "HTTP/1.1";
const char* line_end = "\r\n";
const char* contiki = "Contiki";
const char* close = "close";

/*header names*/
const char* HTTP_HEADER_NAME_CONTENT_TYPE = "Content-Type";
const char* HTTP_HEADER_NAME_CONTENT_LENGTH = "Content-Length";
const char* HTTP_HEADER_NAME_LOCATION = "Location";
const char* HTTP_HEADER_NAME_CONNECTION = "Connection";
const char* HTTP_HEADER_NAME_SERVER = "Server";
const char* HTTP_HEADER_NAME_HOST = "Host";
const char* HTTP_HEADER_NAME_IF_NONE_MATCH = "If-None-Match";
const char* HTTP_HEADER_NAME_ETAG = "ETag";

const char* header_delimiter = ": ";

const char* http_content_types[] = {
  "text/plain",
  "text/xml",
  "text/csv",
  "text/html",
  "application/xml",
  "application/exi",
  "application/json",
  "application/link-format",
  "application/x-www-form-urlencoded",
};

static char*
http_get_status_string(http_status_code_t status_code)
{
  char* value = NULL;
  switch(status_code) {
    case 200:
      value = "OK";
      break;
    case 201:
      value = "Created";
      break;
    case 202:
      value = "Accepted";
      break;
    case 204:
      value = "No Content";
      break;
    case 304:
      value = "Not Modified";
      break;
    case 400:
      value = "Bad Request" ;
      break;
    case 404:
      value = "Not Found" ;
      break;
    case 405:
      value = "Method Not Allowed" ;
      break;
    case 406:
      value = "Not Acceptable" ;
      break;
    case 414:
      value = "Request-URI Too Long" ;
      break;
    case 415:
      value = "Unsupported Media Type" ;
      break;
    case 500:
      value = "Internal Server Error" ;
      break;
    case 501:
      value = "Not Implemented" ;
      break;
    case 502:
      value = "Bad Gateway" ;
      break;
    case 503:
      value = "Service Unavailable" ;
      break;
    case 504:
      value = "Gateway Timeout" ;
      break;
    default:
      value = "$$BUG$$";
      break;
  }

  return value;
}

/**************************************/
/********* HTTP headers ***************/

static const char* http_filter_header(const char* header_name)
{
  const char* header = NULL;
  /*other headers can be added here if needed*/
  if (strcmp(header_name, HTTP_HEADER_NAME_CONTENT_LENGTH) == 0) {
    header = HTTP_HEADER_NAME_CONTENT_LENGTH;
  } else if (strcmp(header_name, HTTP_HEADER_NAME_CONTENT_TYPE) == 0) {
    header = HTTP_HEADER_NAME_CONTENT_TYPE;
  }

  return header;
}

static http_header_t*
http_allocate_header(mapmem_t* mapmem, uint16_t variable_len)
{
  PRINTF("size of http_header_t %u variable size %u\n", sizeof(http_header_t), variable_len);
  char* buffer = (char*)mapmem_allocate(mapmem, sizeof(http_header_t) + variable_len);
  if (buffer) {
    http_header_t* option = (http_header_t*) buffer;
    option->next = NULL;
    option->name = NULL;
    option->value = buffer + sizeof(http_header_t);
    return option;
  }

  return NULL;
}


const char*
http_get_header(http_header_t* headers, const char* hdr_name)
{
  for (;headers; headers = headers->next) {
    if (strcmp(headers->name, hdr_name) == 0) {
      return headers->value;
    }
  }

  return NULL;
}

/********* HTTP headers ***************/
/**************************************/


/**************************************/
/********** HTTP Request **************/

void
http_init_request(http_request_t* request)
{
  request->request_type = 0;
  request->url = NULL;
  request->url_len = 0;
  request->query = NULL;
  request->query_len = 0;
  request->headers = NULL;
  request->payload = NULL;
  request->payload_len = 0;
}

int
http_set_req_method(http_request_t* request, const char* method)
{
  /*other method types can be added here if needed*/
  if(strncmp(method, http_get_string, 3) == 0) {
    request->request_type = HTTP_METHOD_GET;
  } else if (strncmp(method, http_post_string, 4) == 0) {
    request->request_type = HTTP_METHOD_POST;
  } else if (strncmp(method, http_put_string, 3) == 0) {
    request->request_type = HTTP_METHOD_PUT;
  } else if (strncmp(method, http_delete_string, 3) == 0) {
    request->request_type = HTTP_METHOD_DELETE;
  } else {
    PRINTF("No Method supported : %s\n", method);
    return 0;
  }

  return 1;
}

int
http_set_req_url(http_request_t* request, char* url, mapmem_t* mapmem)
{
  int error = HTTP_NO_ERROR;
  int full_url_path = 0;
  /*even for default index.html there is / Ex: GET / HTTP/1.1*/
  if (url[0] != '/') {
    /*if URL is complete (http://...) rather than relative*/
    if (strncmp(url, http_string, 4) != 0 ) {
      PRINTF("URL not valid : %s \n",url);
      error = HTTP_URL_INVALID;
    } else {
      full_url_path = 1;
    }
  }

  if (error == HTTP_NO_ERROR) {
    char* url_buffer = url;
    if (full_url_path) {
      unsigned char num_of_slash = 0;
      do {
        url_buffer = strchr( ++url_buffer, '/' );

        PRINTF("Buffer : %s %d\n", url_buffer, num_of_slash);

      } while (url_buffer && ++num_of_slash < 3);
    }

    PRINTF("URL found :%s\n", url_buffer);

    /*Get rid of the first slash*/
    if (url_buffer && ++url_buffer) {
      request->url = (char*) mapmem_copy_text(mapmem, url_buffer);
      request->url_len = strlen(url_buffer);


      if ((request->query = strchr(request->url, '?'))) {
        *(request->query++) = 0;
        /*update url_len - decrease the size of query*/
        request->url_len = strlen(request->url);
        request->query_len = strlen(request->query);
      }

      PRINTF("URL %s, url_len %u, query %s, query_len %u\n", request->url, request->url_len, \
    		  	  	  	  	  	  	  	  	  	  	  	     request->query, request->query_len);
      /*URL is not decoded - should be done here*/
    } else {
      error = HTTP_URL_INVALID;
    }
  }

  return error;
}

int
http_set_req_header(http_request_t* request, char* header, mapmem_t* mapmem)
{
  PRINTF("parse_header --->\n");
  const char* header_name = NULL;

  char* delimiter = strchr(header, ':');
  if (delimiter) {
    *delimiter++ = 0; /*after increment delimiter will point space char*/

    header_name = http_filter_header(header);
    if (header_name && delimiter) {
      char* buffer = delimiter;

      if (buffer[0] == ' ') {
        buffer++;
      }

      http_header_t* current_header = NULL;
      http_header_t* head = NULL;

      current_header = http_allocate_header(mapmem, strlen(buffer));

      if (current_header) {
        current_header->name = (char*)header_name;
        strcpy(current_header->value, buffer);
      }

      head = request->headers;
      request->headers = current_header;
      if (head) {
        current_header->next = head;
      }

      return 1;
    }
  }

  return 0;
}

/* http query, comment it here temporarily */
//int
//http_get_query_variable(http_request_t* request, const char *name, char* output, uint16_t output_size)
//{
//  if (request->query) {
//    return get_variable(name, request->query, request->query_len, output, output_size, 0);
//  }
//
//  return 0;
//}
//
//int
//http_get_post_variable(http_request_t* request, const char *name, char* output, uint16_t output_size)
//{
//  if (request->payload) {
//    return get_variable(name, request->payload, request->payload_len, output, output_size, 1);
//  }
//
//  return 0;
//}

/********** HTTP Request **************/
/**************************************/

/**************************************/
/********** HTTP Response *************/

void
http_init_response(http_response_t* response)
{
  response->status_code = 0;
  response->status_string = NULL;
  response->headers = NULL;
  response->payload = NULL;
  response->payload_len = 0;
}

int
http_set_res_status(http_response_t* response, http_status_code_t status)
{
  response->status_code = status;
  response->status_string = http_get_status_string(status);
  return 1;
}

int
http_set_res_header(http_response_t* response, const char* name, const char* value, int copy, mapmem_t* mapmem)
{
  PRINTF("http_set_res_header (copy:%d) %s:%s\n", copy, name, value);
  uint16_t size = 0;
  http_header_t* current_header = NULL;
  http_header_t* head = NULL;

  if (copy) {
    size += strlen(value) + 1;
  }

  current_header = http_allocate_header(mapmem, size);

  if (current_header) {
    current_header->name = (char*)name;
    if (copy) {
      strcpy(current_header->value, value);
    } else {
      current_header->value = (char*)value;
    }

    head = response->headers;
    response->headers = current_header;
    if (head) {
      current_header->next = head;
    }

    return 1;
  }

  return 0;
}

int
http_set_res_payload(http_response_t* response, uint8_t* payload, uint16_t size, int copy, mapmem_t* mapmem)
{
  if (copy) {
    response->payload = mapmem_copy_data(mapmem, payload, size);
    if (response->payload) {
      response->payload_len = size;
      return 1;
    }
  } else {
	response->payload = payload;
	response->payload_len = size;
	return 1;
  }

  return 0;
}

/********** HTTP Response *************/
/**************************************/
