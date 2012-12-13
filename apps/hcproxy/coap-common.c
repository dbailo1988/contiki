#include <string.h>
#include "coap-common.h"
#include "contiki-net.h" // for uip_htons in coap_serialize_packet

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

static coap_header_option_t*
allocate_header_option(uint16_t variable_len, mapmem_t* mapmem)
{
  PRINTF("sizeof coap_header_option_t %u variable size %u\n", sizeof(coap_header_option_t), variable_len);
  uint8_t* buffer = mapmem_allocate(mapmem, sizeof(coap_header_option_t) + variable_len);
  if (buffer){
    coap_header_option_t* option = (coap_header_option_t*) buffer;
    option->next = NULL;
    option->len = 0;
    option->value = buffer + sizeof(coap_header_option_t);
    return option;
  }

  return NULL;
}

void coap_init_packet(coap_packet_t* packet)
{
  packet->ver = 1;
  packet->type = 0;
  packet->option_count = 0;
  packet->code = 0;
  packet->tid = 0;
  packet->options = NULL;
  packet->url = NULL;
  packet->url_len = 0;
  packet->query = NULL;
  packet->query_len = 0;
  packet->payload = NULL;
  packet->payload_len = 0;
}

int
coap_set_option(coap_packet_t* packet, coap_option_type option_type, uint16_t len, uint8_t* value, mapmem_t* mapmem)
{
  PRINTF("coap_set_option len %u\n", len);
  coap_header_option_t* option = allocate_header_option(len, mapmem);
  if (option){
    option->next = NULL;
    option->len = len;
    option->option = option_type;
    memcpy(option->value, value, len);
    coap_header_option_t* option_current = packet->options;
    coap_header_option_t* prev = NULL;
    while (option_current){
      if (option_current->option > option->option){
        break;
      }
      prev = option_current;
      option_current = option_current->next;
    }

    if (!prev){
      if (option_current){
        option->next = option_current;
      }
      packet->options = option;
    } else{
      option->next = option_current;
      prev->next = option;
    }

    packet->option_count++;

    if (option_type == Option_Type_Uri_Path) {
      packet->url = (char*)option->value;
      packet->url_len = option->len;
    }

//    PRINTF("option->len %u option->option %u option->value %x next %x\n", option->len, option->option, (unsigned int) option->value, (unsigned int)option->next);
//
//    int i = 0;
//    for ( ; i < option->len ; i++ ){
//      PRINTF(" (%u)", option->value[i]);
//    }
//    PRINTF("\n");

    return 1;
  }

  return 0;
}

int
coap_set_method(coap_packet_t* packet, coap_method_t method)
{
  packet->code = (uint8_t)method;

  return 1;
}

void
coap_parse_message(coap_packet_t* packet, uint8_t* buf, uint16_t size, mapmem_t* mapmem)
{
  int processed = 0;
  int i = 0;
  PRINTF("parse_message size %d-->\n",size);

  coap_init_packet(packet);

  packet->ver = (buf[0] & COAP_HEADER_VERSION_MASK) >> COAP_HEADER_VERSION_POSITION;
  packet->type = (buf[0] & COAP_HEADER_TYPE_MASK) >> COAP_HEADER_TYPE_POSITION;
  packet->option_count = buf[0] & COAP_HEADER_OPTION_COUNT_MASK;
  packet->code = buf[1];
  packet->tid = (buf[2] << 8) + buf[3];

  processed += 4;

  if (packet->option_count) {
    int option_index = 0;
    uint8_t option_delta;
    uint16_t option_len;
    uint8_t* option_buf = buf + processed;
    packet->options = (coap_header_option_t*)mapmem_allocate(mapmem, sizeof(coap_header_option_t) * packet->option_count);

    if (packet->options) {
      coap_header_option_t* current_option = packet->options;
      coap_header_option_t* prev_option = NULL;
      while(option_index < packet->option_count) {
        /*FIXME : put boundary controls*/
        option_delta = (option_buf[i] & COAP_HEADER_OPTION_DELTA_MASK) >> COAP_HEADER_OPTION_DELTA_POSITION;
        option_len = (option_buf[i] & COAP_HEADER_OPTION_SHORT_LENGTH_MASK);
        i++;
        if (option_len == 0xf) {
          option_len += option_buf[i];
          i++;
        }

        current_option->option = option_delta;
        current_option->len = option_len;
        current_option->value = option_buf + i;
        if (option_index) {
          prev_option->next = current_option;
          /*This field defines the difference between the option Type of
           * this option and the previous option (or zero for the first option)*/
          current_option->option += prev_option->option;
        }

        if (current_option->option == Option_Type_Uri_Path) {
          packet->url = (char*)current_option->value;
          packet->url_len = current_option->len;
        } else if (current_option->option == Option_Type_Uri_Query){
          packet->query = (char*)current_option->value;
          packet->query_len = current_option->len;
        }

        PRINTF("OPTION %d %u %s \n", current_option->option, current_option->len, current_option->value);

        i += option_len;
        option_index++;
        prev_option = current_option++;
      }
      current_option->next = NULL;
    } else {
      PRINTF("MEMORY ERROR\n"); /*add control here*/
      return;
    }
  }
  processed += i;

  /**/
  if (processed < size) {
    packet->payload = &buf[processed];
    packet->payload_len = size - processed;
  }

  /*url is not decoded - is necessary?*/

  /*If query is not already provided via Uri_Query option then check URL*/
  if (packet->url && !packet->query) {
    if ((packet->query = strchr(packet->url, '?'))) {
      uint16_t total_url_len = packet->url_len;
      /*set query len and update url len so that it does not include query part now*/
      packet->url_len = packet->query - packet->url;
      packet->query++;
      packet->query_len = packet->url + total_url_len - packet->query;

      PRINTF("url %s, url_len %u, query %s, query_len %u\n", packet->url, packet->url_len, packet->query, packet->query_len);
    }
  }

  PRINTF("PACKET ver:%d type:%d oc:%d \ncode:%d tid:%u url:%s len:%u payload:%s pay_len %u\n", (int)packet->ver, (int)packet->type, (int)packet->option_count, (int)packet->code, packet->tid, packet->url, packet->url_len, packet->payload, packet->payload_len);
}

int coap_serialize_packet(coap_packet_t* packet, uint8_t* buffer)
{
  int index = 0;
  coap_header_option_t* option = NULL;
  uint16_t option_delta = 0;

  buffer[0] = (packet->ver) << COAP_HEADER_VERSION_POSITION;
  buffer[0] |= (packet->type) << COAP_HEADER_TYPE_POSITION;
  buffer[0] |= packet->option_count;
  buffer[1] = packet->code;
  uint16_t temp = uip_htons(packet->tid);
  memcpy(
    (void*)&buffer[2],
    (void*)(&temp),
    sizeof(packet->tid));

  index += 4;

  PRINTF("serialize option_count %u\n", packet->option_count);

  /*Options should be sorted beforehand*/
  for (option = packet->options ; option ; option = option->next){
    uint16_t delta = option->option - option_delta;
    if ( !delta ){
      PRINTF("WARNING: Delta==Zero\n");
    }
    buffer[index] = (delta) << COAP_HEADER_OPTION_DELTA_POSITION;

    PRINTF("option %u len %u option diff %u option_value addr %x option addr %x next option addr %x", option->option, option->len, option->option - option_delta, (unsigned int) option->value, (unsigned int)option, (unsigned int)option->next);

    int i = 0;
    for ( ; i < option->len ; i++ ){
      PRINTF(" (%u)", option->value[i]);
    }
    PRINTF("\n");

    if (option->len < 0xF){
      buffer[index] |= option->len;
      index++;
    } else{
      buffer[index] |= (0xF); //1111
      buffer[index + 1] = option->len - (0xF);
      index += 2;
    }

    memcpy((char*)&buffer[index], option->value, option->len);
    index += option->len;
    option_delta = option->option;
  }

  if(packet->payload){
    memcpy(&buffer[index], packet->payload, packet->payload_len);
    index += packet->payload_len;
  }

  return index;
}
