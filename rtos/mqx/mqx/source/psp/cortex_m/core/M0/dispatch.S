
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
* See license agreement file for full license terms including other restrictions.
*****************************************************************************
*
* Comments:
*
*   This assembler file contains functions for task scheduling
*
*
*END************************************************************************/

#include "asm_mac.h"

#include "mqx_cnfg.h"
#include "types.inc"
#include "psp_prv.inc"

#define __ASM__
#include "mqx_prv.h"
#undef __ASM__

ASM_EQUATE(SVC_MQX_FN, 0xaa)

 ASM_EXTERN(_mqx_kernel_data)
 ASM_EXTERN(_klog_isr_start_internal)
 ASM_EXTERN(_klog_isr_end_internal)

 ASM_CODE_SECTION(.text)
 SET_FUNCTION_ALIGNMENT
 ASM_COMP_SPECIFIC_DIRECTIVES

 ASM_PUBLIC(_sched_start_internal)
 ASM_PUBLIC(_sched_run_internal)
 ASM_PUBLIC(_sched_check_scheduler_internal)
 ASM_PUBLIC(_sched_execute_scheduler_internal)
 ASM_PUBLIC(PendSV_Handler)

 ASM_PUBLIC(_task_block)
 ASM_PUBLIC(_int_kernel_isr)

 ASM_PUBLIC(_mem_test_and_set)
 ASM_PUBLIC(__get_PSP)
 ASM_PUBLIC(__get_MSP)
 ASM_PUBLIC(__set_MSP)
 ASM_PUBLIC(__get_PSR)
 ASM_PUBLIC(__set_MSP)

#if MQXCFG_EXCEPTION_HANDLING
 ASM_PUBLIC(_psp_exception_return)
#endif /* MQXCFG_EXCEPTION_HANDLING */

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _sched_start_internal
 * Returned Value   : none
 * Comments         : start MQX scheduler
 *
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC_BEGIN(_sched_start_internal)
 ASM_PUBLIC_FUNC(_sched_start_internal)
ASM_LABEL(_sched_start_internal) /* R0 contains stack limit pointer, R1 contains stack size. */
                cpsid.n i    /* dissable interupts */
                //mrs r3, MSP  /* read cstack address loaded during startup */
                //msr MSP, r3  /* update PSP to new base address for _task_fill_stack_internal return address */
                GET_KERNEL_DATA r1 //TODO: give only limit as parameter size is not neccessary
                ldr r1, [r1, #KD_INTERRUPT_STACK_PTR]
#if MQX_MONITOR_STACK
//stack fill start - init part
                ldr r2, =0x7374616B /* Load pattern "kats". */
                subs r1, r1, #4     /* Prevent rewrite of data pointed by MSP*/
ASM_LABEL(_fill_stack)
                str r2, [r0]        /* Store pattern to address */
                adds r0, r0, #4     /* Increment pointer to next address. */
                cmp r1, r0          /* Test if MSP was reached. */
                bne _fill_stack
#endif /* MQX_MONITOR_STACK */
                /* Switch to proccess stack (PSP) */
                mrs r0, CONTROL
                movs r1, #0x2
                orrs r0, r0, r1
                msr CONTROL, r0
                isb
                /* set PSP = MSP */
                mrs r0, MSP
                msr PSP, r0
                cpsie.n i   /* enable interruprs */

                GET_KERNEL_DATA r0
                b sched_internal
 ASM_PUBLIC_END(_sched_start_internal)

//-------------------------------------------------------------------------------

 ASM_PUBLIC_BEGIN(_sched_run_internal)
 ASM_PUBLIC_FUNC(_sched_run_internal)
ASM_LABEL(_sched_run_internal)
                GET_KERNEL_DATA r0
                b sched_internal
 ASM_PUBLIC_END(_sched_run_internal)

//-------------------------------------------------------------------------------

 ASM_PUBLIC_BEGIN(_sched_check_scheduler_internal)
 ASM_PUBLIC_FUNC(_sched_check_scheduler_internal)
ASM_LABEL(_sched_check_scheduler_internal)
                GET_KERNEL_DATA r0

                ldrh r1, [r0, #KD_IN_ISR]
                cmp r1, #0
                bne _sched_check_scheduler_internal_end

                ldr r1, [r0, #KD_CURRENT_READY_Q]
                ldr r2, [r0, #KD_ACTIVE_PTR]
                ldr r3, [r2, #TD_MY_QUEUE]
                cmp r1, r3

                // current task is not the active task
                bne _sched_execute_scheduler_internal

ASM_LABEL(_sched_check_scheduler_internal_end)
                bx lr
 ASM_PUBLIC_END(_sched_check_scheduler_internal)

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _sched_execute_scheduler_internal
 * Returned Value   : none
 * Comments         :
 *
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC_BEGIN(_sched_execute_scheduler_internal)
 ASM_PUBLIC_FUNC(_sched_execute_scheduler_internal)
ASM_LABEL(_sched_execute_scheduler_internal)

                // store active task registers
                STORE_ALL_REGISTERS
                // disable interrupts
                cpsid i

                GET_KERNEL_DATA r0
                ldr r3, [r0, #KD_ACTIVE_PTR]        // get active task descriptor
                str r1, [r3, #TD_STACK_PTR]         // store task SP to task descriptor td

#if MQX_KERNEL_LOGGING
                KLOG r0, ASM_PREFIX(_klog_execute_scheduler_internal)  // kernel log this function
#endif

ASM_LABEL(sched_internal)
                // disable interrupts
                cpsid i
                ldr r1, [r0, #KD_CURRENT_READY_Q]   // get current ready q
ASM_LABEL(find_noempty_que)
                ldr r2, [r1, #0]                    // address of first td
                cmp r2, r1                          // ready_q structure itself?
                bne switch_task
                ldr r1, [r1, #RQ_NEXT_Q]            // try next queue
                movs r1, r1
                bne find_noempty_que

                // r1 is 0 -> empty
ASM_LABEL(no_one_to_run)
                // wait for interrupt
                cpsie i
                wfi
                cpsid i

                // TODO check r0, must be kernel data
                ldr r1, [r0, #KD_READY_Q_LIST]      // get first queue from ready list
                b find_noempty_que

ASM_LABEL(switch_task)
                // update kernel structures
                str r1, [r0, #KD_CURRENT_READY_Q]   // store addr for active que
                str r2, [r0, #KD_ACTIVE_PTR]        // active task descriptor

                ldrh r3, [r2, #TD_TASK_SR]
                strh r3, [r0, #KD_ACTIVE_SR]        // restore priority mask for enabled interrupt for active task

#if MQX_KERNEL_LOGGING
                KLOG r0, ASM_PREFIX(_klog_context_switch_internal)  // do kernel logging
#endif

                // activate task, restore registers stacked in the following order (from higher addresses to lower addresses):
                // xPSR, PC, LR, R12, R3, R2, R1, R0, R11, R10, R9, R8, R7, R6, R5, R4, PRIMASK
                ldr r0, [r2, #TD_STACK_PTR]         // get task SP

                //restore R4-R7, PRIMASK goes to R3
                ldm r0!, {r3-r7}
                //restore R8-R9
                ldm r0!, {r1-r2}
                mov r8, r1
                mov r9, r2
                //restore R10-R11
                ldm r0!, {r1-r2}
                mov r10, r1
                mov r11, r2

                //the rest of stack frame will be loaded automatically by HW when returning from the ISR or PendSV
                msr PSP, r0

                //check if we are returning from the ISR or not
                mrs r1, CONTROL
                cmp r1, #0
                bne return_via_pendsv

                //restore PRIMASK from r3
                msr PRIMASK, r3

                //load lr previously pushed to the main stack
                pop {pc}

ASM_LABEL(return_via_pendsv)
                // load SCB address to r0
                ldr r0, =0xE000ED00

                //set PendSV flag
                ldr r1, =0x10000000                 /* PENDSVSET */
                str r1, [r0, #4]                    /* 0xE000ED04 */

                //enable interrupts to execute PendSV handler
                cpsie i

                //this point normally shall not be reached - PendSV executes immediatelly and never returns back here
                b .

 ASM_PUBLIC_END(_sched_execute_scheduler_internal)


/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : PendSV_Handler
 * Returned Value   : none
 * Comments         : PendSV handler - task switch functionality
 *
 *END*----------------------------------------------------------------------*/

/* Pending Service Call */
 ASM_PUBLIC_BEGIN(PendSV_Handler)
 ASM_PUBLIC_FUNC(PendSV_Handler)
ASM_LABEL(PendSV_Handler)
                //discard the stack frame stored by hw when entering PendSV handler
                mrs r0, psp
                adds r0, r0, #32
                msr psp, r0

                //restore PRIMASK from r3
                msr PRIMASK, r3

                //return using use stack frame of the task being resumed
                bx lr
 ASM_PUBLIC_END(PendSV_Handler)


/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _task_block
 * Returned Value   : none
 * Comments         : task block function - block actual task - switch to another
 *
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC_BEGIN(_task_block)
 ASM_PUBLIC_FUNC(_task_block)
ASM_LABEL(_task_block)

                // store active task registers
                STORE_ALL_REGISTERS
                //disable interrupts
                cpsid i

                GET_KERNEL_DATA r0
                ldr r3, [r0, #KD_ACTIVE_PTR]        // get active task descriptor
                str r1, [r3, #TD_STACK_PTR]         // store task SP to task descriptor td


                // set task block bit
                ldr r2, [r3, #TD_STATE]
                movs r1, #1                        // TODO change from number to define
                orrs r2, r2, r1
                str r2, [r3, #TD_STATE]

#if MQX_KERNEL_LOGGING
                KLOG r0, ASM_PREFIX(_klog_block_internal)        // kernel log this function
#endif // MQX_KERNEL_LOGGING
                // remove active task from ready queue
                ldr r2, [r3, #TD_TD_PREV]           // get ptr to ready_q structure
                ldr r1, [r3, #TD_TD_NEXT]
                str r1, [r2, #RQ_HEAD_READY_Q]
                str r2, [r1, #TD_TD_PREV]

                b sched_internal   // Search for the next task in the ready queue.


 ASM_PUBLIC_END(_task_block)


/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _int_kernel_isr
 * Returned Value   : none
 * Comments         : MQX kernel interrupt handler
 *
 *END*----------------------------------------------------------------------*/

// kernel interrupt handler
 ASM_PUBLIC_BEGIN(_int_kernel_isr)
 ASM_PUBLIC_FUNC(_int_kernel_isr)
ASM_LABEL(_int_kernel_isr)
                mrs  r0, PRIMASK
                cpsid i
                push {lr}
                push {r0}

                GET_KERNEL_DATA r3              // get the kernel data address

                // increment in interrupt counter
                ldrh r1, [r3, #KD_IN_ISR]
                adds r1, r1, #1
                strh r1, [r3, #KD_IN_ISR]

                // create interrupt content
                ldr r0, =0                      // error code (set 0)
                push {r0}                       // store in stack
                mrs r1, IPSR                    // exception number
                ldr r0, [r3, #KD_INTERRUPT_CONTEXT_PTR] // previous interrupt content
                push {r0-r1}                    // store in stack

                mrs r0, MSP                     // get address of interrupt content
                str r0, [r3, #KD_INTERRUPT_CONTEXT_PTR] // store address of actual interrupt content in kernel data

                mov r0, r1

#if MQX_KERNEL_LOGGING
                ldr r1, [r3, #KD_LOG_CONTROL]
                cmp r1, #0x00000001
                beq _isr_no_logging

                // prepare parameters for klog function
                push {r0-r3}
                bl ASM_PREFIX(_klog_isr_start_internal)
                pop {r0-r3}
ASM_LABEL(_isr_no_logging)
#endif // MQX_KERNEL_LOGGING
                // get C function address from MQX interrupt table
                // r0 - interrupt number

                // check if isr is in table range
                ldr r2, [r3, #KD_LAST_USER_ISR_VECTOR]

                // cbz r2, _isr_run_default       // isr not used (cbz not working in CW)
                cmp r2, #0
                bne _isr_skip_run_default1       // isr not used (this is CW workaround)
                b _isr_run_default
ASM_LABEL(_isr_skip_run_default1)

                cmp r0, r2
                bhi _isr_run_default

                ldr r2, [r3, #KD_FIRST_USER_ISR_VECTOR]
                subs r1, r0, r2                         // r1 = actual exception number in table
                blt _isr_run_default

#if MQXCFG_SPARSE_ISR_TABLE

ASM_LABEL(_int_kernel_isr_vect_ok)
                ldr r2, [r3, #KD_INTERRUPT_TABLE_PTR]   // get the interrupt table pointer
                lsrs r1, r1, #MQX_SPARSE_ISR_SHIFT
                lsls r1, r1, #2

                ldr r1, [r2, r1]                        // get address of first isr in linked list

                // cbz r1, _isr_run_default              // isr not used (cbz not working in CW)
                cmp r1, #0
                bne _isr_skip_run_default2              // isr not used (this is CW workaround)
                b _isr_run_default
ASM_LABEL(_isr_skip_run_default2)

                // r1 - address of first isr in linked list
ASM_LABEL(_isr_search)
                ldr r2, [r1, #HASH_ISR_NUM]             // get isr num
                cmp r2, #0
                beq _isr_search_fail

                cmp r2, r0                              // compare isr number in record with actual isr number
                beq _isr_search_suc

                ldr r1, [r1, #HASH_ISR_NEXT]            // next vector in list
                tst r1, r1
                bne _isr_search

ASM_LABEL(_isr_search_fail)
                b _isr_run_default

ASM_LABEL(_isr_search_suc)
                ldr r0, [r1, #HASH_ISR_DATA]            // move notifier data into r0 = first parameter in C func
                ldr r2, [r1, #HASH_ISR_ADDR]            // move interrupt function address to r2

#else /* MQXCFG_SPARSE_ISR_TABLE */

ASM_LABEL(_int_kernel_isr_vect_ok)
                // calculate offset in table
                movs r2, #IT_STRUCT_SIZE
                muls r1, r2, r1

                ldr r2, [r3, #KD_INTERRUPT_TABLE_PTR]       // pointer to interrupt table begin
                add r1, r1, r2                              // get address of entry in table
                ldr r2, [r1, #0]
                ldr r0, [r1, #IT_APP_ISR_DATA]              // move notifier data into r0 = first parameter in C func
#endif /* MQXCFG_SPARSE_ISR_TABLE */

ASM_LABEL(_isr_execute)
                // r0 = first parameter in C func
                // r2 contain interrupt function address

                cpsie i
                //MSR PRIMASK, R11
                push {r3}

                blx r2

ASM_LABEL(_int_kernel_isr_epilog)

                pop {r3}
                //MRS  R11, PRIMASK
                cpsid i

ASM_LABEL(_int_kernel_isr_return_internal)

#if MQX_KERNEL_LOGGING
                ldr r1, [r3, #KD_LOG_CONTROL]
                cmp r1, #0x00000001
                beq _isr_return_no_logging

                mrs r0, IPSR                    // exception number

                // prepare parameters for klog function
                push {r0-r3}
                bl ASM_PREFIX(_klog_isr_end_internal)
                pop {r0-r3}
ASM_LABEL(_isr_return_no_logging)
#endif /* MQX_KERNEL_LOGGING */

                // remove interrupt content
                pop {r0-r1}
                str r0, [r3, #KD_INTERRUPT_CONTEXT_PTR] // update pointer to interrupt content

                pop {r0}                                // error code

                // decrement interrupt counter
                ldrh r1, [r3, #KD_IN_ISR]
                subs r1, r1, #1
                strh r1, [r3, #KD_IN_ISR]

                cmp r1, #0
                bne _isr_return_end        // waiting another isr, do not reschedule

                // check for reschedule
                // check preemtion
                ldr r2, [r3, #KD_ACTIVE_PTR]    // TD pointer
                ldr r0, [r2, #TD_FLAGS]
                ldr r1, =TASK_PREEMPTION_DISABLED
                tst r0, r1
                bne _isr_return_end

                // if a different TD at head of current readyq, then we need to run the scheduler
                // check for reschedule
                ldr r1, [r3, #KD_CURRENT_READY_Q]
                ldr r1, [r1]
                cmp r1, r2

                beq _isr_return_end

                // store rest of stack frame that is not stacked automatically by the HW when entering an exception
                // i.e. R11, R10, R9, R8, R7, R6, R5, R4, PRIMASK
                mrs r1, PSP
                subs r1, r1, #36
                //get PRIMASK from the main stack and store it in the process stack along with other registers
                pop {r3}
                stm r1!, {r3-r7}
                mov r2, r8
                mov r3, r9
                mov r4, r10
                mov r5, r11
                stm r1!, {r2-r5}
                subs r1, r1, #36

                //store new PSP value into TD
                GET_KERNEL_DATA r0
                ldr r3, [r0, #KD_ACTIVE_PTR]        // get active task descriptor
                str r1, [r3, #TD_STACK_PTR]         // store task SP to task descriptor td

                b sched_internal

ASM_LABEL(_isr_return_end)
                pop {r0}
                msr PRIMASK, r0
                pop {pc}

ASM_LABEL(_isr_run_default)
                // r0 - interrupt number

                ldr r2, [r3, #KD_DEFAULT_ISR]

                b _isr_execute
 ASM_PUBLIC_END(_int_kernel_isr)

#if MQXCFG_EXCEPTION_HANDLING
/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _psp_exception_return
 * Returned Value   : none
 * Comments         : This function returns us from an isr exception
 *
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC_BEGIN(_psp_exception_return)
 ASM_PUBLIC_FUNC(_psp_exception_return)
ASM_LABEL(_psp_exception_return)
                cpsid i
                push {r0, r1, r2, r4-r7}    // store parameter and registers which are used for copy and cpu do not store if

                GET_KERNEL_DATA r3

                // copy actual int context + 4WORDS -> prev int context - (1 + 8)WORDS
                ldr r2, [r3, #KD_INTERRUPT_CONTEXT_PTR]    // get actual int context
                adds r2, r2, #16               // calculate source address

                ldr r1, [r2]
                mov lr, r1                     // lr - return from exception... (0xfffffff1)
                adds r2, r2, #4                // increment r2 by 4

                // modify return address (PC) in exception stack frame (kernel isr epilog)
                ldr r0, =_int_kernel_isr_epilog
                movs r1, #1
                bics r0, r0, r1
                str r0, [r2, #24]

                ldmia r2, {r0-r7}              // read exception stack frame
                mov r8, r7

                pop {r7}                       // get prev int context address (function parameter - r0)

                /* destination stack address (4B + 32B - exception stack frame) */
                subs r7, r7, #36    /* #36 */
                stmia r7!, {r0-r6}  /* substitution of stmdb in M0+ */
                
                mov r0, r7          /* store r7 reg */
                mov r7, r8
                /* clear 9 bit of XPSR to avoid HW aligner, register r7 */
                movs r1, #1
                lsls r1, r1, #9		/* shift to 9th bit */
                bics r7, r7, r1		/* clear 9th bit */
                str r7, [r0]
                subs r0, r0, #28    /* modify stack offset */
                /* restore modified registers which are not restored by cpu */
                pop {r1, r2, r4-r7}

                msr MSP, r0                    // update MSP (main stack pointer)
                cpsie i

                bx lr
 ASM_PUBLIC_END(_psp_exception_return)
#endif /* MQXCFG_EXCEPTION_HANDLING */

/*******************************************************************************
 * test support
 ******************************************************************************/

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _mem_test_and_set
 * Returned Value   :  previous value of location
 * Comments         :
 *   This function tests a byte location, and if AND 0x80 is 0 (7bit is set !!!), sets it to 0x80.
 *   It returns the previous value of the byte (0 or 0x80).
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC_BEGIN(_mem_test_and_set)
 ASM_PUBLIC_FUNC(_mem_test_and_set)
ASM_LABEL(_mem_test_and_set)
                push {r1, r2, lr}

                ldrb r1, [r0]
                ldr r2, =0x80
                tst r1, r2
                bne _mem_test_and_set_0x80
                orrs r1, r1, r2
                strb r1, [r0]
                eors r0, r0
                b _mem_test_and_set_pop
ASM_LABEL(_mem_test_and_set_0x80)
                movs r0, #0x80

ASM_LABEL(_mem_test_and_set_pop)
                pop {r1, r2, pc}
 ASM_PUBLIC_END(_mem_test_and_set)

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : __get_PSP
 * Returned Value   : PSP value
 * Comments         : This function returns PSP (Process Stack Pointer) register value
 *
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC_BEGIN(__get_PSP)
 ASM_PUBLIC_FUNC(__get_PSP)
ASM_LABEL(__get_PSP)
                mrs r0, PSP
                bx lr
 ASM_PUBLIC_END(__get_PSP)

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : __get_MSP
 * Returned Value   : MSP value
 * Comments         : This function returns MSP (Main Stack Pointer) register value
 *
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC_BEGIN(__get_MSP)
 ASM_PUBLIC_FUNC(__get_MSP)
ASM_LABEL(__get_MSP)
                mrs r0, MSP
                bx lr
 ASM_PUBLIC_END(__get_MSP)

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : __set_MSP
 * Returned Value   : none
 * Comments         : This function sets MSP (Main Stack Pointer) register value
 *
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC_BEGIN(__set_MSP)
 ASM_PUBLIC_FUNC(__set_MSP)
ASM_LABEL(__set_MSP)
                msr MSP, r0
                bx lr
 ASM_PUBLIC_END(__set_MSP)

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : __get_PSR
 * Returned Value   : PSR value
 * Comments         : This function returns PSR (procesor status register) value
 *
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC_BEGIN(__get_PSR)
 ASM_PUBLIC_FUNC(__get_PSR)
ASM_LABEL(__get_PSR)
                 mrs r0, PSR
                 bx lr
 ASM_PUBLIC_END(__get_PSR)

/* IAR defines following functions internally */
#ifndef __IAR_SYSTEMS_ASM__

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : __enable_interrupt
 * Returned Value   : none
 * Comments         : This function enables interrupts
 *
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC_BEGIN(__enable_interrupt)
 ASM_PUBLIC_FUNC(__enable_interrupt)
ASM_LABEL(__enable_interrupt)
                 cpsie i
                 bx lr
 ASM_PUBLIC_END(__enable_interrupt)


/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : __disable_interrupt
 * Returned Value   : none
 * Comments         : This function disables interrupts
 *
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC_BEGIN(__disable_interrupt)
 ASM_PUBLIC_FUNC(__disable_interrupt)
ASM_LABEL(__disable_interrupt)
                 cpsid i
                 bx lr
 ASM_PUBLIC_END(__disable_interrupt)

 ASM_PUBLIC(__enable_interrupt)
 ASM_PUBLIC(__disable_interrupt)

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : __get_CONTROL
 * Returned Value   : CONTROL register value
 * Comments         : This function returns CONTROL register value
 *
 *END*----------------------------------------------------------------------*/

 ASM_PUBLIC(__get_CONTROL)
 ASM_PUBLIC_BEGIN(__get_CONTROL)
 ASM_PUBLIC_FUNC(__get_CONTROL)
ASM_LABEL(__get_CONTROL)
                mrs r0, CONTROL
                bx lr
 ASM_PUBLIC_END(__get_CONTROL)

#endif  /*__IAR_SYSTEMS_ASM__*/

 ASM_ALIGN(4)
 ASM_END


