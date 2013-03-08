#ifndef __MLD6_H__
#define __MLD6_H__

#include "net/uip.h"
#include "sys/clock.h"

#ifndef MAX_NUM_OF_MCAST6_GROUPS
#define MAX_NUM_OF_MCAST6_GROUPS 8
#endif

#ifndef MAX_NUM_OF_MCAST_LISTENING
#define MAX_NUM_OF_MCAST_LISTENING 8
#endif

/*MLD constants*/

#define ROBUSTNESS_VARIABLE 2
#define QUERY_INTERVAL 125 /*seconds within periodical general queries*/
#define QUERY_RESPONSE_INTERVAL 10
#define MCAST_LISTENER_INTERVAL (ROBUSTNESS_VARIABLE x QUERY_INTERVAL)+ QUERY_RESPONSE_INTERVAL
/* time to decide there is no more listeners for an address*/
#define OTHER_QUERIER_INTERVAL	(ROBUSTNESS_VARIABLE x QUERY_INTERVAL)+ (QUERY_RESPONSE_INTERVAL/2)
/*time to decide there is no querier routers*/
#define START_QI (QUERY_INTERVAL/4)
#define START_COUNT ROBUSTNESS_VARIABLE
#define LAST_LISTENER_QI 1
#define COUNT_LAST_LISTENER ROBUSTNESS_VARIABLE
#define UNSOLICITED REPORT INTERVAL 10

/* Node States*/

#define DELAYING_LISTENER 0
#define IDLE_LISTENER 1

/* Router States*/

#define NO_LISTENERS 0
#define LISTENERS 1
#define CHECKING_LISTENERS 2

/* Struct for nodes*/

struct mld6_listen_group {
	int used;
	uint8_t mld_flag; /* Whether there is more listeners than you*/
	uint8_t state;
	uip_ipaddr_t mcast_group;
	uip_ipaddr_t source; /* Source-filthering*/
	struct ctimer *delay_timer; /* [0, Max.Respons.Delay] */
};

typedef struct mld6_listen_group mld6_listen_group_t;

//List or array of mld6_groups, as structs as number of mcast groups listening

struct mcast_group {
	int used;
	uip_ipaddr_t mcast_group;
	uint8_t state;
	struct ctimer *timer;
	struct ctimer *rtx_timer;
};

typedef struct mcast_group mcast_group_t;

/*
    Functions to send MLD packets
*/

void mld_gen_query();
void mld_mas_query(uip_ipaddr_t *group_addr, uip_ipaddr_t *source_addr);
void mld_report(uip_ipaddr_t *group_addr, uip_ipaddr_t *source_addr);
void mld_done(uip_ipaddr_t *group_addr, uip_ipaddr_t *source_addr);

/*
 *   Functions to work with Multicast groups in routers
 */

mcast_group_t *mcast_group_new(uip_ipaddr_t *group_addr);
mcast_group_t *mcast_group_find (uip_ipaddr_t *group_addr);

/*
 *   Functions to work with listeners_group in nodes
 */

mld6_listen_group_t *new_listener(uip_ipaddr_t *group_addr, uip_ipaddr_t *source);
mld6_listen_group_t *find_listener (uip_ipaddr_t *group_addr, uip_ipaddr_t *source);

/*
 * 	Handlers for MLD messages
 */

void mld_router_handler();
void mld_node_handler();
void mld_query_node_in();
void mld_report_node_in();
void mld_report_router_in();
void mld_done_router_in();

#endif /* __MLD6_H__ */
