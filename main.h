/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * [File Name]     main.h
 * [Platform]      MK64FN1M0VLL12
 * [Project]       RTU
 * [Version]       1.00
 * [Author]        Juan Francisco Carol
 * [Date]          10/28/2016
 * [Language]      'C'
 * [History]       1.00 - Original Release
 *
 */

//-----------------------------------------------------------------------
// KSDK Includes
//-----------------------------------------------------------------------
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "fsl_clock_manager.h"
#include "fsl_interrupt_manager.h"
#include "fsl_power_manager.h"
#include "fsl_os_abstraction.h"
#include "fsl_adc16_driver.h"
#include "fsl_cmp_driver.h"
#include "fsl_cmt_driver.h"
#include "fsl_crc_driver.h"
#include "fsl_dac_driver.h"
#include "fsl_dspi_edma_master_driver.h"
#include "fsl_dspi_edma_slave_driver.h"
#include "fsl_dspi_master_driver.h"
#include "fsl_dspi_slave_driver.h"
#include "fsl_edma_driver.h"
#include "fsl_enet_driver.h"
#include "fsl_ewm_driver.h"
#include "fsl_flexbus_driver.h"
#include "fsl_flexcan_driver.h"
#include "fsl_ftm_driver.h"
#include "fsl_gpio_driver.h"
#include "fsl_i2c_master_driver.h"
#include "fsl_i2c_slave_driver.h"
#include "fsl_lptmr_driver.h"
#include "fsl_mpu_driver.h"
#include "fsl_pdb_driver.h"
#include "fsl_pit_driver.h"
#include "fsl_rnga_driver.h"
#include "fsl_rtc_driver.h"
#include "fsl_sai_driver.h"
#include "fsl_sdhc_driver.h"
#include "fsl_smartcard_driver.h"
#if defined(FSL_FEATURE_SOC_EMVSIM_COUNT)
#if (FSL_FEATURE_SOC_EMVSIM_COUNT >= 1)
#include "fsl_smartcard_emvsim_driver.h"
#endif
#endif
#if defined(FSL_FEATURE_UART_HAS_SMART_CARD_SUPPORT)
#if (FSL_FEATURE_UART_HAS_SMART_CARD_SUPPORT == 1)
#include "fsl_smartcard_uart_driver.h"
#endif
#endif
#include "fsl_uart_driver.h"
#include "fsl_uart_edma_driver.h"
#include "fsl_vref_driver.h"
#include "fsl_wdog_driver.h"
#if USING_DIRECT_INTERFACE
#include "fsl_smartcard_direct_driver.h"
#endif
#if USING_NCN8025_INTERFACE
#include "fsl_smartcard_ncn8025_driver.h"
#endif

typedef enum {
    MAIN_IDX = 1,
    MODBUS_SLAVE_IDX,
    MODBUS_MASTER_IDX,
    MFS_FILESYSTEM_IDX
} task_index_t;

////////////////////////////////////////////////////////////////////////////////
// EOF
////////////////////////////////////////////////////////////////////////////////
