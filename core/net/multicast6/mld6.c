
#include "net/uip.h"
#include "contiki-net.h"
#include "net/uip-ds6.h"
#include "net/uip-icmp6.h"
#include "lib/random.h"
#include "mld6.h"
#include "sys/clock.h"
#include "sys/ctimer.h"
#include <string.h>

#define DEBUG DEBUG_NONE

#include "net/uip-debug.h"

#define UIP_IP_BUF       ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])
#define UIP_ICMP_BUF     ((struct uip_icmp_hdr *)&uip_buf[uip_l2_l3_hdr_len])
#define UIP_ICMP_PAYLOAD ((unsigned char *)&uip_buf[uip_l2_l3_icmp_hdr_len])


static mcast_group_t mcast_group_table[MAX_NUM_OF_MCAST6_GROUPS];

static mld6_listen_group_t listening_table[MAX_NUM_OF_MCAST_LISTENING];

unsigned short rseed;


/*
 * Calculate a pseudo random number between 0 and 65535.
 *
 * \return A pseudo-random number between 0 and 65535.
 */

unsigned short
random_time (uint16_t MRD)
{
        unsigned short result;

        random_init(clock_time());
        return (random_rand() % (MRD));

}

/**
 *       send a general query in MLD to all link local nodes
 */

void
mld_gen_query()
{
	uip_ipaddr_t dest_addr;

	uip_create_linklocal_allnodes_mcast(&dest_addr);

	PRINTF("MLD: GENERAL QUERY to ");
	PRINT6ADDR(dest_addr);

	uip_icmp6_send(dest_addr, ICMP6_MLD_QUERY, 0, 0);
}

/**
 *       send a multicast address specific query
 *       in MLD to all link local nodes asking for group_addr
 *       and source_addr
 */

void
mld_mas_query(uip_ipaddr_t *group_addr, uip_ipaddr_t *source_addr)
{

	uip_ipaddr_t dest_addr;
	int pos;
	unsigned char *buffer;


	uip_create_linklocal_allnodes_mcast(&dest_addr);

	PRINTF("MLD: MCAST SPECIFIC QUERY to ");
	PRINT6ADDR(dest_addr);
	PRINTF(" asking for mcast group ");
	PRINT6ADDR(group_addr);

	buffer = UIP_ICMP_PAYLOAD;
	pos = 0;
	memcpy(buffer, group_addr, sizeof(uip_ipaddr_t));
	pos += sizeof(uip_ipaddr_t);
	if (source_addr != NULL)
	{
	    memcpy(buffer + pos, source_addr, sizeof(uip_ipaddr_t));
            pos += sizeof(uip_ipaddr_t);
	}

	uip_icmp6_send(dest_addr, ICMP6_MLD_QUERY, 0, pos);

}

/**
 *       send a listening request to the multicast group group_addr
 *       and source source_addr (if 0, all sources of the mcast group)
 */


void
mld_report(uip_ipaddr_t *group_addr, uip_ipaddr_t *source_addr)
{
	uip_ipaddr_t dest_addr;
	int pos;
	unsigned char *buffer;


	uip_create_linklocal_allnodes_mcast(&dest_addr);

	PRINTF("MLD: MCAST REQUEST TO ");
	PRINT6ADDR(dest_addr);
	PRINTF(" asking for mcast group ");
	PRINT6ADDR(group_addr);

	buffer = UIP_ICMP_PAYLOAD;
	pos = 0;
	memcpy(buffer, group_addr, sizeof(uip_ipaddr_t));
	pos += sizeof(uip_ipaddr_t);
	if (source_addr != NULL)
        {
                PRINTF(" to source ");
                PRINT6ADDR(source_addr);
                memcpy(buffer + pos, source_addr, sizeof(uip_ipaddr_t));
                pos += sizeof(uip_ipaddr_t);
        }


	uip_icmp6_send(dest_addr, ICMP6_MLD_REPORT, 0, pos);

}

/**
 *       send a done message to the multicast group group_addr
 *       and source source_addr (if 0, all sources of the mcast group)
 */

void
mld_done(uip_ipaddr_t *group_addr, uip_ipaddr_t *source_addr)
{
	uip_ipaddr_t dest_addr;
	int pos;
	unsigned char *buffer;


	uip_create_linklocal_allrouters_mcast(&dest_addr);

	PRINTF("MLD: MCAST DONE TO ");
	PRINT6ADDR(dest_addr);
	PRINTF(" for mcast group ");
	PRINT6ADDR(group_addr);

	buffer = UIP_ICMP_PAYLOAD;
	pos = 0;
	memcpy(buffer, group_addr, sizeof(uip_ipaddr_t));
	pos += sizeof(uip_ipaddr_t);
	if (source_addr!= NULL)
        {
                PRINTF(" to source ");
                PRINT6ADDR(source_addr);
                memcpy(buffer + pos, source_addr, sizeof(uip_ipaddr_t));
                pos += sizeof(uip_ipaddr_t);
        }

	uip_icmp6_send(dest_addr, ICMP6_MLD_DONE, 0, pos);

}


/**
 *      Function to introduce a new mcast_group_t in the table
 *      If there are no available entries return NULL
 */

mcast_group_t *
mcast_group_new(uip_ipaddr_t *group_addr)
{
   mcast_group_t *aux;

    for(int i=0; i < MAX_NUM_OF_MCAST6_GROUPS; i++)
    {
        aux= &mcast_group_table[i];
        if(aux->used == 0)
        {
            memset(aux, 0, sizeof(mcast_group_t));
            aux->used = 1;
            aux->mcast_group = group_addr;
            aux->state = NO_LISTENERS;
            return aux;
        }
    }
    return NULL;
}

/**
 *      Function to find a multicast group in a multicast table in
 *      a router
 */
mcast_group_t *
mcast_group_find (uip_ipaddr_t *group_addr)
{
    mcast_group_t *aux;

    for(int i=0; i< MAX_NUM_OF_MCAST6_GROUPS; i++)
    {
    	aux= &mcast_group_table[i];
            if(aux->used)
            {
                if(uip_ipaddr_cmp(&aux->mcast_group, group_addr))
                {
                    return aux;
                }
            }
    }
    return NULL;
}

/*
 *   Functions to work with listeners_group in nodes
 */

mld6_listen_group_t *
new_listener(uip_ipaddr_t *group_addr, uip_ipaddr_t *source)
{
	 mld6_listen_group_t *aux;

	    for(int i=0; i < MAX_NUM_OF_MCAST_LISTENING; i++)
	    {
	    		aux= &listening_table[i];
	            if(aux->used == 0)
	            {
	                memset(aux, 0, sizeof(mcast_group_t));
	                aux->used = 1;
	                aux->mld_flag = 0; /* We are the last node*/
	                aux->mcast_group = group_addr;
	                aux->source = source;
	                ctimer_set(&aux->delay_timer, QUERY_RESPONSE_INTERVAL, mld_report , &aux->mcast_group, &aux->source);
	                return aux;
	            }
	    }
	    return NULL;
}

mld6_listen_group_t *
find_listener (uip_ipaddr_t *group_addr, uip_ipaddr_t *source)
{
	mld6_listen_group_t *aux;

	    for(int i=0; i< MAX_NUM_OF_MCAST_LISTENING; i++) {
	    	aux= &listening_table[i];
                if(aux->used)
                {
                    if(uip_ipaddr_cmp(&aux->mcast_group, group_addr))
                    {
                        return aux;
                    }
                }
	    }
	    return NULL;
}

/*
 * Handler for node
 */

void
mld_node_handler(void)
{
    PRINTF("MLD receive in node\n");
    switch(UIP_ICMP_BUF->type) {
    case ICMP6_MLD_QUERY:
        mld_query_node_in();
        break;
    case ICMP6_MLD_REPORT:
        mld_report_node_in();
        break;
    default:
        PRINTF("Node discard ICMPv6 message\n");
        break;
    }

    uip_len = 0;
}

/*
 * Handler for node
 */

void
mld_router_handler(void)
{
    PRINTF("MLD receive in node\n");
    switch(UIP_ICMP_BUF->type) {
    case ICMP6_MLD_REPORT:
        mld_report_router_in();
        break;
    case ICMP6_MLD_DONE:
        mld_done_router_in();
        break;
    default:
        PRINTF("Node discard ICMPv6 message\n");
        break;
    }

    uip_len = 0;
}


void
mld_query_node_in(){  // Receive query

    uip_ipaddr_t group_addr;
    mld6_listen_group_t *aux;
    unsigned char *buffer;
    uint8_t buffer_length;
    int pos;

    buffer = UIP_ICMP_PAYLOAD;
    buffer_length = uip_len - uip_l3_icmp_hdr_len;

    if (buffer_length > 0)
    { /* Mcast address specific query*/
    	pos = 0;
    	memcpy(&group_addr, buffer, sizeof(uip_ipaddr_t));
    	pos += sizeof(uip_ipaddr_t);

        PRINTF("Received MLD query from \n");
        PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
        PRINTF(" G = ");
        PRINT6ADDR(&group_addr);
        PRINTF("\n");

        aux = find_listener(&group_addr);

        if (aux==NULL){
                 PRINTF("Node is no listening requested group\n");
        } else {
                if (&aux->state == IDLE_LISTENER){
                         aux->state = DELAYING_LISTENER;
                         ctimer_set(&aux->delay_timer, QUERY_RESPONSE_INTERVAL, mld_report , &aux->mcast_group, &aux->source);
                } else{
                        if (&aux->state == DELAYING_LISTENER){
                            //TODO if (MRD < current time)
                            ctimer_set(&aux->delay_timer, QUERY_RESPONSE_INTERVAL, mld_report , &aux->mcast_group, &aux->source);
                        }
                }
        }
    } else { //General query
    	for (i=0; i< MAX_NUM_OF_MCAST_LISTENING; i++){
    		aux = &listening_table[i];
    		if (aux->used == 1){
    		        aux->state = DELAYING_LISTENER;
    			ctimer_set(&aux->delay_timer, random_time (QUERY_RESPONSE_INTERVAL), mld_report , &aux->mcast_group, &aux->source);
    		}
    	}
    }
}

void mld_report_node_in(){
	uip_ipaddr_t group_addr, source;
	mld6_listen_group_t *aux;
	unsigned char *buffer;
	uint8_t buffer_length;
	int pos;

	buffer = UIP_ICMP_PAYLOAD;
	buffer_length = uip_len - uip_l3_icmp_hdr_len;

	pos = 0;
	memcpy(&group_addr, buffer, sizeof(uip_ipaddr_t));
	pos += sizeof(uip_ipaddr_t);
	memcpy(&source, buffer + pos, sizeof(uip_ipaddr_t));
	pos += sizeof(uip_ipaddr_t);

	PRINTF("Received MLD report from ");
	PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
	PRINTF("in node \n");
	PRINTF(" G = ");
	PRINT6ADDR(&group_addr);
	PRINTF(" Source = ");
	PRINT6ADDR(&source);
	PRINTF("\n");

	aux = find_listener(&group_addr);
	if (aux->state == DELAYING_LISTENER){
		if (aux->used){
			ctimer_stop(aux->delay_timer);
			aux->mld_flag = 0;  //Clear flag we were not the last node
			aux->state = IDLE_LISTENER;
		}
	}
}

void mld_report_router_in(){
	uip_ipaddr_t group_addr, source;
	mcast_group_t *aux;
	unsigned char *buffer;
	uint8_t buffer_length;
	int pos;

	buffer = UIP_ICMP_PAYLOAD;
	buffer_length = uip_len - uip_l3_icmp_hdr_len;

	pos = 0;
	memcpy(&group_addr, buffer, sizeof(uip_ipaddr_t));
	pos += sizeof(uip_ipaddr_t);
	memcpy(&source, buffer + pos, sizeof(uip_ipaddr_t));
	pos += sizeof(uip_ipaddr_t);

	PRINTF("Received MLD report from ");
	PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
	PRINTF(" in router \n");
	PRINTF(" G = ");
	PRINT6ADDR(&group_addr);
	PRINTF(" Source = ");
	PRINT6ADDR(&source);
	PRINTF("\n");

	aux= mcast_group_find(&group_addr);
	if (aux == NULL) { // Requested Group is not in the list of mcast group
		PRINTF("Report discard, no mcast group in router\n");
	} else {
		  switch(aux->state) {
		  	 case NO_LISTENERS:
		         // notify routing
		  	 aux->state = LISTENERS;
		  	 ctimer_set(&aux->timer, QUERY_RESPONSE_INTERVAL, mld_query , &aux->mcast_group); // query with source or without, where it would take it
		         break;
		     case LISTENERS:
		         ctimer_set(&aux->timer, QUERY_RESPONSE_INTERVAL, mld_query , &aux->mcast_group);
		         break;
		     case CHECKING_LISTENERS:
		         aux->state = LISTENERS;
		         ctimer_set(&aux->timer, QUERY_RESPONSE_INTERVAL, mld_query , &aux->mcast_group);
		         ctimer_stop(&aux->rtx_timer);
		    	 break;
		     default:
		         PRINTF("Error in router's state\n");
		         break;
		  }
	}

}
void mld_done_router_in()
{
        uip_ipaddr_t group_addr, source;
        mcast_group_t *aux;
        unsigned char *buffer;
        uint8_t buffer_length;
        int pos;

        buffer = UIP_ICMP_PAYLOAD;
        buffer_length = uip_len - uip_l3_icmp_hdr_len;

        pos = 0;
        memcpy(&group_addr, buffer, sizeof(uip_ipaddr_t));
        pos += sizeof(uip_ipaddr_t);
        memcpy(&source, buffer + pos, sizeof(uip_ipaddr_t));
        pos += sizeof(uip_ipaddr_t);

        PRINTF("Received MLD done from ");
        PRINT6ADDR(&UIP_IP_BUF->srcipaddr);
        PRINTF(" in router \n");
        PRINTF(" G = ");
        PRINT6ADDR(&group_addr);
        PRINTF(" Source = ");
        PRINT6ADDR(&source);
        PRINTF("\n");

        aux= mcast_group_find(&group_addr);
        if (aux == NULL) { // Requested Group is not in the list of mcast group
                PRINTF("Report discard, no mcast group in router\n");
        } else {
                  switch(aux->state) {
                     case LISTENERS:
                         // start timer + start rtx timer + send masquery
                         break;
                     default:
                         PRINTF("Error in router's state receiving mld_done\n");
                         break;
                  }
        }







}
