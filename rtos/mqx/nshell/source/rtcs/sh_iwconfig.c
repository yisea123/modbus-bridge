/*HEADER**********************************************************************
*
* Copyright 2009 Freescale Semiconductor, Inc.
*
* This software is owned or controlled by Freescale Semiconductor.
* Use of this software is governed by the Freescale MQX RTOS License
* distributed with this Material.
* See the MQX_RTOS_LICENSE file distributed for more details.
*
* Brief License Summary:
* This software is provided in source form for you to use free of charge,
* but it is not open source software. You are allowed to use this software
* but you cannot redistribute it or derivative works of it in source form.
* The software may be used only in connection with a product containing
* a Freescale microprocessor, microcontroller, or digital signal processor.
* See license agreement file for full license terms including other
* restrictions.
*****************************************************************************
*
* Comments:
*
*   Example using RTCS Library.
*
*
*END************************************************************************/


#include <mqx.h>
#include "shell.h"
#include "sh_prv.h"

#if SHELLCFG_USES_RTCS
#include <bsp.h>
#include <rtcs.h>
#include <ipcfg.h>
#include <iwcfg.h>
#include <ctype.h>
#include "sh_rtcs.h"
#include "sh_enet.h"
#include "string.h"

#ifdef BSP_ENET_DEVICE_COUNT
#if  (BSP_ENET_DEVICE_COUNT > 0)

/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : Shell_iwconfig_set_ssid
* Returned Value : ERROR code on error else success
* Comments       :
*
*END------------------------------------------------------------------*/
static int32_t Shell_iwconfig_set_ssid(FILE *fout, uint32_t enet_device,char *ssid)
{
    uint32_t error;

    error = iwcfg_set_essid (enet_device,ssid);
    if (error != 0)
    {
        fprintf(fout, "Error during setting of ssid %s error=%08x!\n",ssid, error);
        return SHELL_EXIT_ERROR;
    }

    fprintf(fout, "SSID %s set successful.\n",ssid);
    return SHELL_EXIT_SUCCESS;
}
/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : Shell_iwconfig_set_mode
* Returned Value : ERROR code on error else success
* Comments       :
*
*END------------------------------------------------------------------*/
static int32_t Shell_iwconfig_set_mode(FILE *fout, uint32_t enet_device,char *mode)
{
    uint32_t error;

    error = iwcfg_set_mode (enet_device,mode);
    if (error != 0)
    {
        fprintf(fout, "Error during setting of mode %08x!\n", error);
        return SHELL_EXIT_ERROR;
    }

    fprintf(fout, "mode %s set successful.\n",mode);
    return SHELL_EXIT_SUCCESS;
}

/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : Shell_iwconfig_set_mode
* Returned Value : ERROR code on error else success
* Comments       :
*
*END------------------------------------------------------------------*/
static int32_t Shell_iwconfig_commit(FILE *fout, uint32_t enet_device)
{
    uint32_t error;

    error = iwcfg_commit (enet_device);
    if (error != 0)
    {
        fprintf(fout, "Error commiting \n");
        return SHELL_EXIT_ERROR;
    }

    fprintf(fout, "commit successful.\n");
    return SHELL_EXIT_SUCCESS;
}

/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : Shell_iwconfig_set_mode
* Returned Value : ERROR code on error else success
* Comments       :
*
*END------------------------------------------------------------------*/
static int32_t Shell_iwconfig_set_wep_key(FILE *fout, uint32_t enet_device,int32_t index,int32_t argc,char *key[])
{
    uint32_t error;
    uint32_t i = 0;
    char *wep_key = NULL;
    uint32_t key_index = 0;
    uint32_t key_len=0;

    for (i=index+1;i<argc;i++)
    {
        if (strcmp(key[i],"none") == 0)
        {
            wep_key = NULL;
            key_len = 0;
            break;
        }
        else if (strncmp(key[i],"[",1) == 0)
        {
          char *ptr = key[i];
          if (!isdigit(*(++ptr)))
          {
            fprintf(fout, "%s key index passed is incorrect\n",key[i]);
            return SHELL_EXIT_ERROR;
          }
          key_index = key_index*10 + (*ptr - '0');
          continue;

        }
        else if (strncmp(key[i],"s:",2) == 0)
        {
            char *data = key[i];
            wep_key = data+2;
            key_len = strlen(data)-2;
            continue;
        }
        else if (strcmp(key[i],"restricted") == 0)
        {
           continue;
        }
        else if (strcmp(key[i],"open") == 0)
        {
            continue;
        }
        else
        {
            //Key is in hexadecimal format.
            fprintf(fout, "WEP Key in hexadecimal format not supported. Pass it as string using s:[password]\n");
            return SHELL_EXIT_ERROR;
        }

    }
    error = iwcfg_set_wep_key (enet_device,wep_key,key_len,key_index);
    if (error != 0)
    {
        fprintf(fout, "Error during setting of key %08x!\n", error);
        return SHELL_EXIT_ERROR;
    }

    fprintf(fout, "key set successful.\n");
    return SHELL_EXIT_SUCCESS;
}

/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : Shell_iwconfig_set_sec_type
* Returned Value : ERROR code on error else success
* Comments       :
*
*END------------------------------------------------------------------*/
static int32_t Shell_iwconfig_set_sec_type(FILE *fout, uint32_t enet_device,char *sec_type)
{
    uint32_t error;

    error = iwcfg_set_sec_type (enet_device,sec_type);
    if (error != 0)
    {
        fprintf(fout, "Error during setting of sectype %08x!\n", error);
        return SHELL_EXIT_ERROR;
    }

    fprintf(fout, "sectype %s set successful.\n",sec_type);
    return SHELL_EXIT_SUCCESS;
}

/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : Shell_iwconfig_set_passphrase
* Returned Value : ERROR code on error else success
* Comments       :
*
*END------------------------------------------------------------------*/
static int32_t Shell_iwconfig_set_passphrase(FILE *fout, uint32_t enet_device,char *passphrase)
{
    uint32_t error;
    char *p_phrase;


    if (passphrase[0] == '"')
    {
       uint32_t tmp_len = 0;
       p_phrase = passphrase+1;
       tmp_len = strlen(p_phrase);
       p_phrase[tmp_len] = '\0';
    }
    else
    {
        p_phrase = passphrase;
    }

    error = iwcfg_set_passphrase (enet_device,p_phrase);
    if (error != 0)
    {
        fprintf(fout, "Error during setting of passphrase %08x!\n", error);
        return SHELL_EXIT_ERROR;
    }

    fprintf(fout, "passphrase %s set successful.\n",p_phrase);
    return SHELL_EXIT_SUCCESS;
}
/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : Shell_iwconfig_set_power
* Returned Value : ERROR code on error else success
* Comments       :
*
*END------------------------------------------------------------------*/
static int32_t Shell_iwconfig_set_power (FILE *fout, uint32_t enet_device,int32_t index,int32_t argc,char *argv[])
{
    uint32_t error;
    uint32_t    i = index+1;
    uint32_t flags = 0;
    uint32_t pow_val=0;

    if(i<argc)
    {
        if (strcmp(argv[i],"off") == 0)
        {
            flags = TRUE;
          //  break;
        }
        else if (strcmp(argv[i],"on") == 0)
        {
           flags = FALSE;
         //  break;
        }
        else if (strcmp(argv[i],"period") == 0)
        {
            char *data = argv[i+1];
            uint32_t str_len = strlen(data);
            uint32_t sec_flag;
            if (data[str_len-1] == 's')
            {
              sec_flag = 1;
            }
            else if (data[str_len-1] == 'm')
            {
                sec_flag = 0;
            }
            else
            {
                fprintf(fout, "period supported only in seconds\n");
                return SHELL_EXIT_ERROR;
            }

            data[--str_len] = '\0';

            while (*data != '\0')
            {
               if (!isdigit(*data))
               {
                   fprintf(fout, "%s  period value passed is incorrect\n",argv[i+1]);
                   return SHELL_EXIT_ERROR;
               }

               pow_val = pow_val*10 + (*data - '0');
               data++;
            }
            i++;
            flags = FALSE;
            if (sec_flag)
               pow_val = pow_val*1000;

           // break;
        }
        else
        {
            //Key is in hexadecimal format.
            fprintf(fout, "parameter not supported with power command\n");
            return SHELL_EXIT_ERROR;
        }

    }
    error = iwcfg_set_power (enet_device,pow_val,flags);
    if (error != 0)
    {
        fprintf(fout, "Error during setting of key %08x!\n", error);
        return SHELL_EXIT_ERROR;
    }

    fprintf(fout, "power %s set successful.\n",argv[index+1]);
    return SHELL_EXIT_SUCCESS;
}

/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : Shell_iwconfig_set_power
* Returned Value : ERROR code on error else success
* Comments       :
*
*END------------------------------------------------------------------*/
static int32_t Shell_iwconfig_set_scan (FILE *fout, uint32_t enet_device,int32_t index,int32_t argc,char *argv[])
{
    uint32_t error;
    char *ssid = NULL;
    if (argc > 2)
    {
        ssid = argv[index+1];
    }

    error = iwcfg_set_scan (enet_device, ssid);
    if (error != 0)
    {
        fprintf(fout, "Error during scan %08x!\n", error);
        return SHELL_EXIT_ERROR;
    }

    if (ssid)
      fprintf(fout, "scan %s done.\n",ssid);
    else
      fprintf(fout, "scan done.\n");

    return SHELL_EXIT_SUCCESS;
}

/*FUNCTION*-----------------------------------------------------------------
*
* Function Name  : Shell_iwconfig
* Returned Value : ERROR code on error else success
* Comments       :
*
*END------------------------------------------------------------------*/

int32_t Shell_iwconfig(int32_t argc, char *argv[] )
{ /* Body */
    bool                 print_usage, shorthelp = FALSE;
    int32_t                  return_code = SHELL_EXIT_SUCCESS;
    uint32_t                 enet_device = BSP_DEFAULT_ENET_DEVICE, index = 1;
    SHELL_CONTEXT_PTR shell_ptr = Shell_get_context(argv);

    print_usage = Shell_check_help_request (argc, argv, &shorthelp);

    if (!print_usage)
    {
        if (argc > index)
        {
            if (Shell_parse_number (argv[index], &enet_device))
            {
                index++;
            }
        }

        if (enet_device >= BSP_ENET_DEVICE_COUNT)
        {
            fprintf(shell_ptr->STDOUT, "Wrong number of ethernet device (%d)!\n", enet_device);
            return_code = SHELL_EXIT_ERROR;
        }
        else
        {
            if (argc > index)
            {
                if (strcmp (argv[index], "ssid") == 0)
                {
                    return_code = Shell_iwconfig_set_ssid(shell_ptr->STDOUT, enet_device,argv[index+1]);
                }
                else if (strcmp (argv[index], "mode") == 0)
                {
                    return_code = Shell_iwconfig_set_mode(shell_ptr->STDOUT, enet_device,argv[index+1]);
                }
                else if (strcmp (argv[index], "commit") == 0)
                {
                    return_code = Shell_iwconfig_commit(shell_ptr->STDOUT, enet_device);
                }
                else if (strcmp (argv[index], "key") == 0)
                {
                    return_code = Shell_iwconfig_set_wep_key(shell_ptr->STDOUT, enet_device,index,argc,argv);
                }
                else if (strcmp (argv[index], "sectype") == 0)
                {
                    return_code = Shell_iwconfig_set_sec_type(shell_ptr->STDOUT, enet_device,argv[index+1]);
                }
                else if (strcmp (argv[index], "passphrase") == 0)
                {
                    return_code = Shell_iwconfig_set_passphrase(shell_ptr->STDOUT, enet_device,argv[index+1]);
                }
                else if (strcmp (argv[index], "power") == 0)
                {
                    return_code = Shell_iwconfig_set_power(shell_ptr->STDOUT, enet_device,index,argc,argv);
                }
                else if (strcmp (argv[index], "scan") == 0)
                {
                    return_code = Shell_iwconfig_set_scan(shell_ptr->STDOUT, enet_device,index,argc,argv);
                }

                else if (strcmp (argv[index], "help") == 0)
                {
                    return_code = SHELL_EXIT_ERROR;
                }
                else
                {
                    fprintf(shell_ptr->STDOUT, "Unknown iwconfig command!\n");
                    return_code = SHELL_EXIT_ERROR;
                }
            }
            else if (argc == 1)
            {
                char data[32];
                uint32_t error;
                error = iwcfg_get_essid (enet_device,(char *)data);
                if (error != 0)
                   return_code = SHELL_EXIT_ERROR;

                fprintf(shell_ptr->STDOUT, "ssid=%s\n",data);
                iwcfg_get_mode (enet_device,(char *)data);
                fprintf(shell_ptr->STDOUT, "mode=%s\n",data);
                iwcfg_get_sectype (enet_device,(char *)data);
                fprintf(shell_ptr->STDOUT, "security type=%s\n",data);
                if (strcmp(data,"wep") == 0)
                {
                    uint32_t def_key_index;
                    iwcfg_get_wep_key(enet_device,(char *)data,&def_key_index);
                    fprintf(shell_ptr->STDOUT, "wep key=%s\n",data);
                    fprintf(shell_ptr->STDOUT, "default key index=%d\n",def_key_index);
                }
                if (strcmp(data,"wpa") == 0)
                {
                    unsigned char p_phrase[65];
                    iwcfg_get_passphrase(enet_device,(char *)p_phrase);
                    fprintf(shell_ptr->STDOUT, "   passphrase=%s\n",p_phrase);
                }
                if (strcmp(data,"wpa2") == 0)
                {
                    unsigned char p_phrase[65];
                    iwcfg_get_passphrase(enet_device,(char *)p_phrase);
                    fprintf(shell_ptr->STDOUT, "   passphrase=%s\n",p_phrase);
                }
            }
        }
    }
    else
    {
        return_code = SHELL_EXIT_ERROR;
    }

    if ((print_usage) || (return_code != SHELL_EXIT_SUCCESS))
    {
        if (shorthelp)
        {
            fprintf(shell_ptr->STDOUT, "%s [<device>] [<command>]\n", argv[0]);
        }
        else
        {
            fprintf(shell_ptr->STDOUT, "Use iwconfig commands if Wifi device is initialized\n");
            fprintf(shell_ptr->STDOUT, "Usage: %s [<command>]\n", argv[0]);
            fprintf(shell_ptr->STDOUT, "  Commands:\n");
            fprintf(shell_ptr->STDOUT, "    ssid   [<network id>]   = Set SSID on device to associate with AP\n");
            fprintf(shell_ptr->STDOUT, "    mode   [<network mode>] = Set network mode on device\n");
            fprintf(shell_ptr->STDOUT, "    commit                  = Forces the card to apply all pending changes.\n");
            fprintf(shell_ptr->STDOUT, "    key [<WEP Key>],[<key index>]  = Sets WEP Keys and enables WEP security\n");
            fprintf(shell_ptr->STDOUT, "    passphrase [<passphrase string>] = Enables WPA2 security and sets the passphrase\n");
            fprintf(shell_ptr->STDOUT, "    sectype [<security type>]= Sets the Security for WiFi Device    \n");
            fprintf(shell_ptr->STDOUT, "    power   [<on>] [<off>] [period] [<value>] = Enables or disables the power mode and sets the sleep duration.\n");
            fprintf(shell_ptr->STDOUT, "    scan                   = Scan for Access Points and display there infrmation\n");
            fprintf(shell_ptr->STDOUT, "    help                   = Display commands list and parameters.\n");
            fprintf(shell_ptr->STDOUT, "  Parameters:\n");
            fprintf(shell_ptr->STDOUT, "    <network id>   = name of the wireless network\n");
            fprintf(shell_ptr->STDOUT, "    <network mode> = type of Wireless network. Possible values: managed or adhoc\n");
            fprintf(shell_ptr->STDOUT, "    <WEP Key>      = WEP Security Key. If set to none, WEP security is disabled.\n");
            fprintf(shell_ptr->STDOUT, "    <key index>    = Four keys can be set as WEP keys.Set index as [1],[2],[3],[4]\n");
            fprintf(shell_ptr->STDOUT, "                   = To change which key is the currently active key, just enter [index]\n");
            fprintf(shell_ptr->STDOUT, "                   = (without entering any key value).\n");
            fprintf(shell_ptr->STDOUT, "    <security type>= wep,wpa,wpa2 or none\n");
            fprintf(shell_ptr->STDOUT, "    <passphrase string> = pass phrase string\n");
            fprintf(shell_ptr->STDOUT, "    <on>                = enables power mode\n");
            fprintf(shell_ptr->STDOUT, "    <off>               = disables power mode\n");
            fprintf(shell_ptr->STDOUT, "    <value>             = sleep duration\n");
        }
    }

    return return_code;
} /* Endbody */

#else /* (BSP_ENET_DEVICE_COUNT > 0) */
int32_t Shell_iwconfig(int32_t argc, char *argv[] )
{
   SHELL_CONTEXT_PTR shell_ptr = Shell_get_context(argv);
   fprintf(shell_ptr->STDOUT, "Cannot use this command, no enet device driver available in this BSP.");
   return SHELL_EXIT_ERROR;
}
#endif /* (BSP_ENET_DEVICE_COUNT > 0) */
#else  /* BSP_ENET_DEVICE_COUNT */
int32_t Shell_iwconfig(int32_t argc, char *argv[] )
{
   SHELL_CONTEXT_PTR shell_ptr = Shell_get_context(argv);
   fprintf(shell_ptr->STDOUT, "Cannot use this command, no enet device driver available in this BSP.");
   return SHELL_EXIT_ERROR;
}
#endif /* BSP_ENET_DEVICE_COUNT */
#endif /* SHELLCFG_USES_RTCS */
/* EOF */
