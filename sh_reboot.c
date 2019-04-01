/*HEADER**********************************************************************
 *
 * Copyright 2008 Freescale Semiconductor, Inc.
 * Copyright 2004-2008 Embedded Access Inc.
 * Copyright 1989-2008 ARC International
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
 *   This file contains the exit command.
 *
 *
 *END************************************************************************/

#include <stdio.h>
#include <mqx.h>
#include <mqx_inc.h> /* internal kernel structures */
#include "shell.h"
#include "sh_prv.h"


/*FUNCTION*-------------------------------------------------------------
 *
 *  Function Name :  Shell_exit
 *  Returned Value:  none
 *  Comments  :  SHELL utility to exit a task
 *  Usage:
 *
 *END*-----------------------------------------------------------------*/

int32_t  Shell_reboot(int32_t argc, char *argv[])
{
    bool              print_usage, shorthelp = FALSE;
    int32_t               return_code = SHELL_EXIT_SUCCESS;
    SHELL_CONTEXT_PTR    shell_ptr = Shell_get_context( argv );
    uint32_t ms_delay;

    print_usage = Shell_check_help_request(argc, argv, &shorthelp );

    if (!print_usage)
    {
        ms_delay = 0;
        if (argc == 2) {
            ms_delay = atoi(argv[1]) * 1000;
        }
        fprintf(shell_ptr->STDOUT, "The system will restart in %s seconds...\n", ms_delay ? argv[1] : "0");
        _time_delay(ms_delay);
        NVIC_SystemReset();
    }

    if (print_usage)  {
        if (shorthelp)  {
            fprintf(shell_ptr->STDOUT, "%s <time>\n", argv[0]);
        } else  {
            fprintf(shell_ptr->STDOUT, "Usage: %s <time>\n", argv[0]);
            fprintf(shell_ptr->STDOUT, "   <time> = Time to wait before restarting (seconds)\n");
        }
    }
    return return_code;
}


