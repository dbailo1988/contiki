/**
 * \file
 *         Router example for testing MLD
 *
 * \author Daniel Bailo <dbailo1988@gmail.com>
 *
 */#include <stdio.h>
#include "contiki.h"
#include "shell.h"
#include "serial-shell.h"
#include "net/uip.h"
#include "net/uip-ds6.h"
#include "net/uip-debug.h"
#include "net/rpl/rpl.h"
#include "dev/serial-line.h"
#include "shell-mld-router.h"
#include "net/multicast6/mld6.h"
#include "sys/ctimer.h"
#include "sys/clock.h"
#if CONTIKI_TARGET_Z1
#include "dev/uart0.h"
#else
#include "dev/uart1.h"
#endif

//#define DEBUG 1

#define UIP_IP_BUF ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

static struct ctimer timer_querier;


PROCESS(router_shell_process, "Basic Shell For Multicast Listener Discovery router");
AUTOSTART_PROCESSES(&router_shell_process);

static uip_ipaddr_t *
set_global_address(void)
{
    static uip_ipaddr_t ipaddr;
    int i;
    uint8_t state;

    uip_ip6addr(&ipaddr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

    printf("IPV6 addresses: \n");
    for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
            (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
            uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
            printf("\n");
        }
    }

  return &ipaddr;
}


static void
querier_expired ()
{
    mld_gen_query();
    ctimer_set(&timer_querier, QUERY_INTERVAL * CLOCK_SECOND, querier_expired , NULL);
}

PROCESS_THREAD(router_shell_process, ev, data)
{


    PROCESS_BEGIN();
#if CONTIKI_TARGET_Z1
    uart0_set_input(serial_line_input_byte);
#else
    uart1_set_input(serial_line_input_byte);
#endif
    serial_line_init();
    serial_shell_init();

    shell_mld_router_init();

    set_global_address();
    ctimer_set(&timer_querier, QUERY_INTERVAL * CLOCK_SECOND, querier_expired , NULL);
    PROCESS_END();
}

