#ifndef COAP_COMMON_H_
#define COAP_COMMON_H_

#include "mapmem.h"

#define COAP_MAX_PAYLOAD_LEN   100

/*COAP method types*/
typedef enum {
  COAP_METHOD_GET = 1,
  COAP_METHOD_POST,
  COAP_METHOD_PUT,
  COAP_METHOD_DELETE,
  COAP_METHOD_UNKNOWN
} coap_method_t;

typedef enum {
  MESSAGE_TYPE_CON,
  MESSAGE_TYPE_NON,
  MESSAGE_TYPE_ACK,
  MESSAGE_TYPE_RST
} coap_message_type;

typedef enum {
  COAP_OK_200 = 80,
  COAP_CREATED_201 = 81,
  COAP_NOT_MODIFIED_304 = 124,
  COAP_BAD_REQUEST_400 = 160,
  COAP_NOT_FOUND_404 = 164,
  COAP_METHOD_NOT_ALLOWED_405 = 165,
  COAP_UNSUPPORTED_MADIA_TYPE_415 = 175,
  COAP_INTERNAL_SERVER_ERROR_500 = 200,
  COAP_BAD_GATEWAY_502 = 202,
  COAP_GATEWAY_TIMEOUT_504 = 204
} coap_status_code_t;

typedef enum {
  Option_Type_Content_Type = 1,
  Option_Type_Max_Age = 2,
  Option_Type_Etag = 4,
  Option_Type_Uri_Authority = 5,
  Option_Type_Location = 6,
  Option_Type_Uri_Path = 9,
  Option_Type_Subscription_Lifetime = 10,
  Option_Type_Token = 11,
  Option_Type_Block = 13,
  Option_Type_Uri_Query = 15
} coap_option_type;

typedef enum {
  COAP_TEXT_PLAIN = 0,
  COAP_TEXT_XML = 1,
  COAP_TEXT_CSV = 2,
  COAP_TEXT_HTML = 3,
  COAP_IMAGE_GIF = 21,
  COAP_IMAGE_JPEG = 22,
  COAP_IMAGE_PNG = 23,
  COAP_IMAGE_TIFF = 24,
  COAP_AUDIO_RAW = 25,
  COAP_VIDEO_RAW = 26,
  COAP_APPLICATION_LINK_FORMAT = 40,
  COAP_APPLICATION_XML = 41,
  COAP_APPLICATION_OCTET_STREAM = 42,
  COAP_APPLICATION_RDF_XML = 43,
  COAP_APPLICATION_SOAP_XML = 44,
  COAP_APPLICATION_ATOM_XML = 45,
  COAP_APPLICATION_XMPP_XML = 46,
  COAP_APPLICATION_EXI = 47,
  COAP_APPLICATION_X_BXML = 48,
  COAP_APPLICATION_FASTINFOSET = 49,
  COAP_APPLICATION_SOAP_FASTINFOSET = 50,
  COAP_APPLICATION_JSON = 51
} coap_content_type_t;

#define COAP_HEADER_VERSION_MASK 0xC0
#define COAP_HEADER_TYPE_MASK 0x30
#define COAP_HEADER_OPTION_COUNT_MASK 0x0F
#define COAP_HEADER_OPTION_DELTA_MASK 0xF0
#define COAP_HEADER_OPTION_SHORT_LENGTH_MASK 0x0F

#define COAP_HEADER_VERSION_POSITION 6
#define COAP_HEADER_TYPE_POSITION 4
#define COAP_HEADER_OPTION_DELTA_POSITION 4

#define COAP_REQUEST_BUFFER_SIZE 200

#define COAP_DEFAULT_CONTENT_TYPE 0
#define COAP_DEFAULT_MAX_AGE 60
#define COAP_DEFAULT_URI_AUTHORITY ""
#define COAP_DEFAULT_URI_PATH ""

//keep open requests and their xactid

struct coap_header_option_t
{
  struct coap_header_option_t* next;
  uint16_t option;
  uint16_t len;
  uint8_t* value;
};
typedef struct coap_header_option_t coap_header_option_t;

struct coap_block_option_t {
  uint32_t number;
  uint8_t more;
  uint8_t size;
};
typedef struct coap_block_option_t coap_block_option_t;

typedef struct
{
  uint8_t ver; //2-bits currently set to 1.
  uint8_t type; //2-bits Confirmable (0), Non-Confirmable (1), Acknowledgment (2) or Reset (3)
  uint8_t option_count; //4-bits
  uint8_t code; //8-bits Method or response code
  uint16_t tid; //16-bit unsigned integer
  coap_header_option_t* options;
  char* url; //put it just as a shortcut or else need to parse options every time to access it.
  uint16_t url_len;
  char* query;
  uint16_t query_len;
  uint16_t payload_len;
  uint8_t* payload;
} coap_packet_t;

/*error definitions*/
typedef enum
{
  COAP_NO_ERROR,
  /*Memory errors*/
  COAP_MEMORY_ALLOC_ERR,
  COAP_MEMORY_BOUNDARY_EXCEEDED,
  COAP_UDP_CONN_ERR
} coap_error_t;

void coap_init_packet(coap_packet_t* packet);
int coap_set_option(coap_packet_t* packet, coap_option_type option_type, uint16_t len, uint8_t* value, mapmem_t* mapmem);
int coap_set_method(coap_packet_t* packet, coap_method_t method);
void coap_parse_message(coap_packet_t* packet, uint8_t* buf, uint16_t size, mapmem_t* mapmem);
int coap_serialize_packet(coap_packet_t* request, uint8_t* buffer);


#endif /* COAP_COMMON_H_ */
