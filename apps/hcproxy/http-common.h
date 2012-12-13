#ifndef HTTPCOMMON_H_
#define HTTPCOMMON_H_

/*includes*/
#include "mapmem.h"

/*definitions of the line ending characters*/
#define LINE_FEED_CHAR '\n'
#define CARRIAGE_RETURN_CHAR '\r'

/*needed for web services giving all path (http://172.16.79.0/services/light1)
 * instead relative (/services/light1) in HTTP request. Ex: Restlet lib. does it*/
extern const char* http_string;

/*HTTP method strings*/
extern const char* http_get_string;
extern const char* http_head_string;
extern const char* http_post_string;
extern const char* http_put_string;
extern const char* http_delete_string;

extern const char* httpv1_1;
extern const char* line_end;
extern const char* contiki;
extern const char* close;

/*HTTP header names*/
extern const char* HTTP_HEADER_NAME_CONTENT_TYPE;
extern const char* HTTP_HEADER_NAME_CONTENT_LENGTH;
extern const char* HTTP_HEADER_NAME_LOCATION;
extern const char* HTTP_HEADER_NAME_CONNECTION;
extern const char* HTTP_HEADER_NAME_SERVER;
extern const char* HTTP_HEADER_NAME_HOST;
extern const char* HTTP_HEADER_NAME_IF_NONE_MATCH;
extern const char* HTTP_HEADER_NAME_ETAG;

extern const char* header_delimiter;

extern const char* http_content_types[];

/*Configuration parameters*/
#define HTTP_PORT 8080
#define HTTP_DATA_BUFF_SIZE 600
#define INCOMING_DATA_BUFF_SIZE 102 /*100+2, 100 = max url len, 2 = space char+'\0'*/


/*HTTP method types*/
typedef enum {
  HTTP_METHOD_GET = (1 << 0),
  HTTP_METHOD_POST = (1 << 1),
  HTTP_METHOD_PUT = (1 << 2),
  HTTP_METHOD_DELETE = (1 << 3)
} http_method_t;

typedef enum {
  HTTP_OK_200 = 200,
  HTTP_CREATED_201 = 201,
  HTTP_ACCEPTED_201 = 202,
  HTTP_NO_CONTENT_204 = 204,
  HTTP_NOT_MODIFIED_304 = 304,
  HTTP_BAD_REQUEST_400 = 400,
  HTTP_NOT_FOUND_404 = 404,
  HTTP_METHOD_NOT_ALLOWED_405 = 405,
  HTTP_NOT_ACCEPTABLE_406 = 406,
  HTTP_REQUEST_URI_TOO_LONG_414 = 414,
  HTTP_UNSUPPORTED_MADIA_TYPE_415 = 415,
  HTTP_INTERNAL_SERVER_ERROR_500 = 500,
  HTTP_NOT_IMPLEMENTED = 501,
  HTTP_BAD_GATEWAY_502 = 502,
  HTTP_SERVICE_UNAVAILABLE_503 = 503,
  HTTP_GATEWAY_TIMEOUT_504 = 504
} http_status_code_t;

typedef enum {
  HTTP_TEXT_PLAIN,
  HTTP_TEXT_XML,
  HTTP_TEXT_CSV,
  HTTP_TEXT_HTML,
  HTTP_APPLICATION_XML,
  HTTP_APPLICATION_EXI,
  HTTP_APPLICATION_JSON,
  HTTP_APPLICATION_LINK_FORMAT,
  HTTP_APPLICATION_WWW_FORM,
  HTTP_UNKNOWN_CONTENT_TYPE
} http_content_type_t;

/*Header type*/
struct http_header_t {
  struct http_header_t* next;
  char* name;
  char* value;
};
typedef struct http_header_t http_header_t;

/*This structure contains information about the HTTP request.*/
struct http_request_t {
  char* url;
  uint16_t url_len;
  http_method_t request_type; /* GET, POST, etc */
  char* query;
  uint16_t query_len;
  http_header_t* headers;
  uint16_t payload_len;
  uint8_t* payload;
};
typedef struct http_request_t http_request_t;

/*This structure contains information about the HTTP response.*/
struct http_response_t {
  http_status_code_t status_code;
  char* status_string;
  http_header_t* headers;
  uint16_t payload_len;
  uint8_t* payload;
};
typedef struct http_response_t http_response_t;


/*error definitions*/
typedef enum {
  HTTP_NO_ERROR,

  /*Memory errors*/
  HTTP_MEMORY_ALLOC_ERR,
  HTTP_MEMORY_BOUNDARY_EXCEEDED,

  /*specific errors*/
  HTTP_XML_NOT_VALID,
  HTTP_SOAP_MESSAGE_NOT_VALID,
  HTTP_URL_TOO_LONG,
  HTTP_URL_INVALID
} http_error_t;

/** HTTP HEADER **/
const char* http_get_header(http_header_t* headers, const char* hdr_name);
/** HTTP REQUEST **/
void http_init_request(http_request_t* request);
int http_set_req_method(http_request_t* request, const char* method);
int http_set_req_url(http_request_t* request, char* url, mapmem_t* mapmem);
int http_set_req_header(http_request_t* request, char* header, mapmem_t* mapmem);
//int http_get_query_variable(http_request_t* request, const char *name, char* output, uint16_t output_size);
//int http_get_post_variable(http_request_t* request, const char *name, char* output, uint16_t output_size);


/** HTTP RESPONSE **/
void http_init_response(http_response_t* response);
int http_set_res_status(http_response_t* response, http_status_code_t status);
int http_set_res_header(http_response_t* response, const char* name, const char* value, int copy, mapmem_t* mapmem);
int http_set_res_payload(http_response_t* response, uint8_t* payload, uint16_t size, int copy, mapmem_t* mapmem);

#endif /*HTTPCOMMON_H_*/
