#include "contiki.h"
#include "shell.h"
#include "uip.h"
#include "net/multicast6/mld6.h"
#include "net/uip-udp-packet.h"

#define MAX_BUF_SIZE 64
#define MIN_BUF_TEST 10

PROCESS(shell_print_router_table_process, "print_router_table");
SHELL_COMMAND(print_router_table_command,
              "print_router_table",
              "print_table: print mld router table",
              &shell_print_router_table_process);

//-------------------------------------------------------------------

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
}
