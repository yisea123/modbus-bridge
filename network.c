#include <rtcs.h>
#include <telnetsrv.h>
#include <mfs.h>
#if !PLATFORM_SDK_ENABLED
#include <enet.h>
#endif
#include <timer.h>
#include <shell.h>

#ifndef ENET_IPADDR_DEFAULT
#define ENET_IPADDR_DEFAULT  IPADDR(192,168,1,28)
#endif

#ifndef ENET_IPMASK_DEFAULT
#define ENET_IPMASK_DEFAULT  IPADDR(255,255,255,0)
#endif

#ifndef ENET_IPGATEWAY_DEFAULT
#define ENET_IPGATEWAY_DEFAULT  IPADDR(0,0,0,0)
#endif

#ifndef ENET_MAC
#define ENET_MAC  {0x00,0xA7,0xC5,0xF1,0x11,0x90}
#endif

extern int32_t Shell_reboot(int32_t argc, char *argv[] );

const SHELL_COMMAND_STRUCT Telnetsrv_shell_commands[] = {
#if SHELLCFG_USES_MFS
        {"cd", Shell_cd},
        {"copy", Shell_copy},
        {"create", Shell_create},
        {"del", Shell_del},
        {"disect", Shell_disect},
        {"dir", Shell_dir},
        {"df", Shell_df},
        {"format", Shell_format},
        {"mkdir", Shell_mkdir},
        {"pwd", Shell_pwd},
        {"read", Shell_read},
        {"ren", Shell_rename},
        {"rmdir", Shell_rmdir},
        {"sh", Shell_sh},
        {"type", Shell_type},
        {"write", Shell_write},
#endif
#if SHELLCFG_USES_RTCS
#if RTCSCFG_ENABLE_ICMP
        {"ping", Shell_ping},
#endif
        {"ftpsrv", Shell_ftpsrv},
#endif
        {"reboot", Shell_reboot},
        {"exit", Shell_exit},
        {"help", Shell_help},
        {"?", Shell_command_list},
        {NULL, NULL}
};

static uint32_t telnetsrv_handle;

int8_t network_telnetsrv_start(void)
{
    TELNETSRV_PARAM_STRUCT params = {0};

    params.shell_commands = (void *)Telnetsrv_shell_commands;
    params.shell = (TELNET_SHELL_FUNCTION)Shell;

    telnetsrv_handle = TELNETSRV_init(&params);
    return ((telnetsrv_handle) ? 0 : -1);
}

int8_t network_telnetsrv_stop(void)
{
    uint32_t rc = RTCS_OK;

    if (telnetsrv_handle)
    {
        rc = TELNETSRV_release(telnetsrv_handle);
        telnetsrv_handle = 0;
    }

    return ((rc == RTCS_OK) ? 0 : -1);
}

int8_t network_init(void)
{
    uint32_t error;
    IPCFG_IP_ADDRESS_DATA ip_data;
    _enet_address macaddress = ENET_MAC;
#if RTCSCFG_ENABLE_IP6
    uint32_t n = 0;
    uint32_t i = 0;
    IPCFG6_GET_ADDR_DATA data[RTCSCFG_IP6_IF_ADDRESSES_MAX];
    char prn_addr6[sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"];
#endif

    /* Initialize RTCS */
    _RTCSPCB_init = 4;
    _RTCSPCB_grow = 2;
    _RTCSPCB_max = 20;
    _RTCS_msgpool_init = 4;
    _RTCS_msgpool_grow = 2;
    _RTCS_msgpool_max  = 20;
    _RTCS_socket_part_init = 4;
    _RTCS_socket_part_grow = 2;
    _RTCS_socket_part_max  = 20;

    error = RTCS_create();
    if (error != RTCS_OK) {
        return -1;
    }

    /* Initialize ethernet */
    error = ipcfg_init_device(BSP_DEFAULT_ENET_DEVICE, macaddress);
    if (error != IPCFG_OK) {
        return -1;
    }

#if DEMOCFG_USE_WIFI
    iwcfg_set_essid(BSP_DEFAULT_ENET_DEVICE, DEMOCFG_SSID);
    if ((strcmp(DEMOCFG_SECURITY,"wpa") == 0)||strcmp(DEMOCFG_SECURITY,"wpa2") == 0)
    {
        iwcfg_set_passphrase (BSP_DEFAULT_ENET_DEVICE,DEMOCFG_PASSPHRASE);
    }
    if (strcmp(DEMOCFG_SECURITY,"wep") == 0)
    {
        iwcfg_set_wep_key (BSP_DEFAULT_ENET_DEVICE,DEMOCFG_WEP_KEY,strlen(DEMOCFG_WEP_KEY),DEMOCFG_WEP_KEY_INDEX);
    }
    iwcfg_set_sec_type(BSP_DEFAULT_ENET_DEVICE, DEMOCFG_SECURITY);
    iwcfg_set_mode(BSP_DEFAULT_ENET_DEVICE, DEMOCFG_NW_MODE);
    iwcfg_commit(BSP_DEFAULT_ENET_DEVICE);
#endif

#if RTCSCFG_ENABLE_IP4
    bool dhcp = false;
    if (!dhcp) /* Static IP */
    {
        ip_data.ip = ENET_IPADDR_DEFAULT;
        ip_data.mask = ENET_IPMASK_DEFAULT;
        ip_data.gateway = ENET_IPGATEWAY_DEFAULT;

        /* Bind static IP address */
        error = ipcfg_bind_staticip(BSP_DEFAULT_ENET_DEVICE, &ip_data);
        if (error != IPCFG_OK) {
            return -1;
        }
    }
    else /* DHCP enabled */
    {
        error = ipcfg_bind_dhcp_wait(BSP_DEFAULT_ENET_DEVICE, false, NULL);
        if (error != IPCFG_OK) {
            return -1;
        }

        bool success;
        success = ipcfg_get_ip(BSP_DEFAULT_ENET_DEVICE, &ip_data);
        if (!success) {
            return -1;
        }

        // LOG IP data
    }
#endif

#if RTCSCFG_ENABLE_IP6
    while(!ipcfg6_get_addr(BSP_DEFAULT_ENET_DEVICE, n, &data[n]))
    {
        n++;
    }
    /* Prepare IPv6 socket for incoming connections */
    sockets.sock6 = socket(PF_INET6, SOCK_STREAM, 0);
    if (sockets.sock6 == RTCS_SOCKET_ERROR)
    {
        fputs("Error: Unable to create socket for IPv6 connections.", stderr);
    }
    else
    {
        uint32_t option;

        option = SERIAL_SOCKET_BUFFER_SIZE;
        (void) setsockopt(sockets.sock4, SOL_TCP, OPT_TBSIZE, &option, sizeof(option));
        (void) setsockopt(sockets.sock4, SOL_TCP, OPT_RBSIZE, &option, sizeof(option));
        ((sockaddr_in6*) &addr)->sin6_family = AF_INET6;
        ((sockaddr_in6*) &addr)->sin6_port = DEMO_PORT;
        ((sockaddr_in6*) &addr)->sin6_addr = in6addr_any; /* Any address */
        ((sockaddr_in6*) &addr)->sin6_scope_id= 0; /* Any scope */
        retval = bind(sockets.sock6, &addr, sizeof(addr));
        if (retval != RTCS_OK)
        {
            fprintf(stderr, "Error 0x%X: Unable to bind IPv6 socket.\n", retval);
        }
        else
        {
            retval = listen(sockets.sock6, 0);
            if (retval != RTCS_OK)
            {
                fprintf(stderr, "Error 0x%X: Unable to put IPv6 socket in listening state.\n", retval);
            }
        }
    }
#endif
    return 0;
}

