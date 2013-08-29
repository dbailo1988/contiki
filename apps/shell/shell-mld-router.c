/**
 * \file
 *         Shell library for routers to test MLD
 *
 * \author Daniel Bailo <dbailo1988@gmail.com>
 *
 */
#include "contiki.h"
#include "shell.h"
#include "uip.h"
#include "net/multicast6/mld6.h"
#include "net/uip-udp-packet.h"
#include <string.h>
#include <stdio.h>

#define MAX_BUF_SIZE 64
#define MIN_BUF_TEST 10



PROCESS(shell_create_group_process, "create_group");
SHELL_COMMAND(create_group_command,
            "create_group",
            "create_group: creates a multicast group",
            &shell_create_group_process);

PROCESS(shell_print_router_table_process, "print_router_table");
SHELL_COMMAND(print_router_table_command,
              "print_router_table",
              "print_table: print mld router table",
              &shell_print_router_table_process);

//-------------------------------------------------------------------
static void
set_s_addr(uint16_t s, uip_ipaddr_t *send_addr)
{
    uip_ip6addr(send_addr, 0xaaaa, 0x0, 0x0, 0x0, 0xc30c, 0x0, 0x0, s);
}

static void
set_g_addr(uint16_t g, uip_ipaddr_t *grp_addr)
{
    uip_ip6addr(grp_addr, 0xff31, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, g);
}

PROCESS_THREAD(shell_create_group_process, ev, data)
{
    static uip_ipaddr_t group_addr;
    static uip_ipaddr_t source_addr;
    const char *nextptr;
    uint16_t g,s;

    PROCESS_BEGIN();
    shell_output_str(&create_group_command, "Creating mcast group", "");
    g = shell_strtolong(data, &nextptr);
    s = shell_strtolong(nextptr, &nextptr);
    printf("g= %d , s= %d \n", g,s);
    set_g_addr(g, &group_addr);
    set_s_addr(s, &source_addr);
    if (mcast_group_new(&group_addr, &source_addr) == NULL)
       printf("No more groups could be created \n");

    PROCESS_END();
}

PROCESS_THREAD(shell_print_router_table_process, ev, data)
{
    PROCESS_BEGIN();
    print_mld_router_table();
    PROCESS_END();
}

//-------------------------------------------------------------------

void
shell_mld_router_init(void)
{
    shell_register_command(&print_router_table_command);
    shell_register_command(&create_group_command);
}
