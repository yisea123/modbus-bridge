/*HEADER**********************************************************************
*
* Copyright 2014 Freescale Semiconductor, Inc.
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
*   This file contains the main C language entrypoint, starting up the MQX
*
*
*END************************************************************************/

#include "mqx.h"
#include "bsp.h"

extern TASK_TEMPLATE_STRUCT MQX_template_list[];
extern void * kernel_data_prv;

/*!
 * \cond DOXYGEN_IGNORE
 */

/* Modifier const is removed in IAR tool to enable adaptive heap placement for IAR linker.
** Search for linker last block (and change of START_OF_HEAP value) must be done in run time.
** It enable to set heap as big as possible in remaining memory.
** See _mqx and mqx_init calls.*/
#ifndef __ICCARM__ /* IAR */
const
#endif  /* __ICCARM__ */
_WEAK_SYMBOL(MQX_INITIALIZATION_STRUCT MQX_init_struct) =
/*! \endcond */
{
    /* PROCESSOR_NUMBER                */  BSP_DEFAULT_PROCESSOR_NUMBER,
    /* [KPSDK-2559] Workaround to make TAD happy. With new version of TAD START_OF_KERNEL_MEMORY and END_OF_KERNEL_MEMORY can be removed */
    /* START_OF_KERNEL_MEMORY          */  &kernel_data_prv,
    /* END_OF_KERNEL_MEMORY            */  BSP_DEFAULT_END_OF_HEAP,
    /* INTERRUPT_STACK_SIZE            */  BSP_DEFAULT_INTERRUPT_STACK_SIZE,
#if MQX_CUSTOM_MAIN
    /* TASK_TEMPLATE_LIST              */  NULL,
#else
    /* TASK_TEMPLATE_LIST              */  MQX_template_list,
#endif /* MQX_CUSTOM_MAIN */
    /* MQX_HARDWARE_INTERRUPT_LEVEL_MAX*/  BSP_DEFAULT_MQX_HARDWARE_INTERRUPT_LEVEL_MAX,
    /* MAX_MSGPOOLS                    */  BSP_DEFAULT_MAX_MSGPOOLS,
    /* MAX_MSGQS                       */  BSP_DEFAULT_MAX_MSGQS,
    /* IO_CHANNEL                      */  BSP_DEFAULT_IO_CHANNEL,
    /* IO_OPEN_MODE                    */  BSP_DEFAULT_IO_OPEN_MODE,
#if defined(__ICCARM__) /* IAR */
    /* START_OF_HEAP                   */  NULL, /* The value cannot be computed by preprocessor. */
#else
    /* START_OF_HEAP                   */  BSP_DEFAULT_START_OF_HEAP,
#endif  /* defined(__ICCARM__) */
    /* END_OF_HEAP                     */  BSP_DEFAULT_END_OF_HEAP,
};

/**FUNCTION********************************************************************
*
* Function Name    : main
* Returned Value   : return "status"
* Comments         : Starts MQX
*
*END**************************************************************************/

#if !MQX_CUSTOM_MAIN
int main
    (
        void
    )
{
#if defined(__ICCARM__) /* IAR */
/* Search for last block placed by IAR linker to set heap as big as possible.
** See mqx_main.c for details.*/
    if (NULL == MQX_init_struct.START_OF_HEAP)
    {
        MQX_init_struct.START_OF_HEAP = (__RW_END > __ZI_END) ? __RW_END : __ZI_END;
    }
#endif  /* defined(__ICCARM__) */
    /* Start MQX */
    _mqx( (MQX_INITIALIZATION_STRUCT_PTR) &MQX_init_struct );
    return 0;
}
#endif /* MQX_CUSTOM_MAIN */
