/*HEADER**********************************************************************
*
* Copyright 2010-2013 Freescale Semiconductor, Inc.
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

*
*END************************************************************************/

#include "asm_mac.h"
#include "mqx_cnfg.h"
#include "types.inc"
#include "psp_prv.inc"

    ASM_COMP_SPECIFIC_DIRECTIVES
    ASM_CODE_SECTION(.text)
    SET_FUNCTION_ALIGNMENT

    ASM_PUBLIC(Reset_Handler)
    ASM_PUBLIC(DefaultISR)
    ASM_PUBLIC(SysTick_Handler)
    ASM_PUBLIC(HardFault_Handler)

    ASM_EXTERN(_int_kernel_isr)

    ASM_PUBLIC_BEGIN(DefaultISR)
    ASM_PUBLIC_FUNC(DefaultISR)
ASM_LABEL(DefaultISR)
          ldr r0, =_int_kernel_isr
          bx r0
    ASM_PUBLIC_END(DefaultISR)

    ASM_PUBLIC_BEGIN(SysTick_Handler)
    ASM_PUBLIC_FUNC(SysTick_Handler)
ASM_LABEL(SysTick_Handler)
          ldr r0, =_int_kernel_isr
          bx r0
    ASM_PUBLIC_END(SysTick_Handler)

    ASM_PUBLIC_BEGIN(HardFault_Handler)
    ASM_PUBLIC_FUNC(HardFault_Handler)
ASM_LABEL(HardFault_Handler)
          ldr r0, =_int_kernel_isr
          bx r0
    ASM_PUBLIC_END(HardFault_Handler)

/*FUNCTION*-------------------------------------------------------------------

 Function Name    : Reset_Handler
 Returned Value   :
 Comments         : startup sequence

 END*-----------------------------------------------------------------------*/


ASM_EQUATE(NVIC_ICER, 0xE000E180)
ASM_EQUATE(NVIC_ICPR, 0xE000E280)

    ASM_EXTERN(__iar_program_start)
    ASM_EXTERN(SystemInit)
    ASM_EXTERN(init_data_bss)

    ASM_PUBLIC_BEGIN(Reset_Handler)
    ASM_PUBLIC_FUNC(Reset_Handler)
ASM_LABEL(Reset_Handler)
        /* Disable interrupts and clear pending flags */
        ldr r0, =0xFFFFFFFF
        /* NVIC_ICER - Interrupt Clear-Enable Register */
        ldr r1, =NVIC_ICER
        str r0, [r1]
        /* NVIC_ICPR - Interrupt Clear-Pending Register */
        ldr r1, =NVIC_ICPR
        str r0, [r1]

        ASM_EXTERN(SystemInit)
        bl ASM_PREFIX(SystemInit)
        ASM_EXTERN(init_data_bss)
        bl ASM_PREFIX(init_data_bss)
        /* Perform toolchain startup routines */
        ASM_EXTERN(toolchain_startup)
        bl ASM_PREFIX(toolchain_startup)

    ASM_PUBLIC_END(Reset_Handler)

        ASM_ALIGN(4)
        ASM_END
