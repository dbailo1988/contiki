#include "contiki.h"
#include "shell.h"
#include "uip.h"
#include "net/multicast6/mld6.h"
#include "net/uip-udp-packet.h"
//#include <string.h>
//#include <stdio.h>

//---------------------------------------------------

PROCESS(shell_send_report_process, "send_report");
SHELL_COMMAND(send_report_command,
            "send_report",
            "send_report: sends a report message",
            &shell_send_report_process);

PROCESS(shell_send_done_process, "send_data");
SHELL_COMMAND(send_done_command,
            "send_done",
            "send_done: sends done message",
            &shell_send_done_process);

//---------------------------------------------------
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

PROCESS_THREAD(shell_send_report_process, ev, data)
{
    static uip_ipaddr_t group_addr;
    static uip_ipaddr_t send_addr;
    const char *nextptr;
    uint16_t s,g;

    PROCESS_BEGIN();
    shell_output_str(&send_report_command, "Sending query request", "");

    s = shell_strtolong(data, &nextptr);
    g = shell_strtolong(nextptr, &nextptr);
    set_g_addr(g, &group_addr);
    set_s_addr(s, &send_addr);
    mld_report(&group_addr, &send_addr);

    PROCESS_END();
}

PROCESS_THREAD(shell_send_done_process, ev, data)
{
    static uip_ipaddr_t group_addr;
    static uip_ipaddr_t send_addr;
    const char *nextptr;
    uint16_t s,g;

    PROCESS_BEGIN();
    shell_output_str(&send_done_command, "Sending done request", "");

    s = shell_strtolong(data, &nextptr);
    g = shell_strtolong(nextptr, &nextptr);
    set_g_addr(g, &group_addr);
    set_s_addr(s, &send_addr);
    mld_done(&group_addr, &send_addr);

    PROCESS_END();
}


//----------------------------------------------------
void
shell_mld_init(void)
{
    shell_register_command(&send_report_command);
    shell_register_command(&send_done_command);
}
