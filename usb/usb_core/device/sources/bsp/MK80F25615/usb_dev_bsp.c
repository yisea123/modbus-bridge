/**HEADER********************************************************************
* 
* Copyright (c) 2013 - 2015 Freescale Semiconductor;
* All Rights Reserved
*
*
*************************************************************************** 
*
* THIS SOFTWARE IS PROVIDED BY FREESCALE "AS IS" AND ANY EXPRESSED OR 
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
* IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
* THE POSSIBILITY OF SUCH DAMAGE.
*
**************************************************************************
*
* Comments:  
*
*END************************************************************************/
#include "adapter.h"
#include "usb.h"
#include "usb_device_config.h"
#if (OS_ADAPTER_ACTIVE_OS != OS_ADAPTER_SDK)
  #error "Only OS_ADAPTER_SDK is supported!"
#endif
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "fsl_device_registers.h"
#include "fsl_clock_manager.h"
#include "fsl_usb_khci_hal.h"

extern uint8_t soc_get_usb_vector_number(uint8_t controller_id);
extern uint32_t soc_get_usb_base_address(uint8_t controller_id);

#define BSP_USB_INT_LEVEL                (4)

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : usb_dev_soc_init
* Returned Value   : USB status
* Comments         :
*    This function performs BSP-specific I/O initialization related to USB
*
*END*----------------------------------------------------------------------*/
usb_status usb_dev_soc_init(uint8_t controller_id) 
{
    usb_status ret       = USBERR_BAD_STATUS;
    uint8_t instance     = controller_id - USB_CONTROLLER_KHCI_0;
    uint32_t base_addres = soc_get_usb_base_address(controller_id);

    if (USB_CONTROLLER_KHCI_0 == controller_id) 
    {
#if USBCFG_DEV_USE_IRC48M
        /* Select PLL output clock as an output of PLLFLLSEL clock multiplexor.  (Register: SIM_SOPT2, bits group PLLFLLSEL) */
        /* Note: This selector also influences clocking of others peripherals such as TPM, SDHC, LPUARTS, I2S/SAI, FlexIO, EWM, etc. */
        CLOCK_SYS_SetPllfllSel(kClockPllFllSelIrc48M);
        /* Select output of USB clock divider as a clock source for USBFS peripheral. (Register: SIM_SOPT2, bit USBSRC) */
        CLOCK_SYS_SetUsbfsSrc(instance, kClockUsbfsSrcPllFllSel);
        /* Set USB clock divider */
        CLOCK_SYS_SetUsbfsDiv(instance, 0U, 0U);
        /* USB Clock Gating */
        CLOCK_SYS_EnableUsbfsClock(instance);
        /* Enable IRC 48MHz clock module */
        usb_hal_khci_ungate_irc48m(base_addres);
        usb_hal_khci_enable_irc48m_recovery_block(base_addres);
#else
        /* Select PLL output clock as an output of PLLFLLSEL clock multiplexor.  (Register: SIM_SOPT2, bits group PLLFLLSEL) */
        /* Note: This selector also influences clocking of others peripherals such as TPM, SDHC, LPUARTS, I2S/SAI, FlexIO, EWM, etc. */
        CLOCK_SYS_SetPllfllSel(kClockPllFllSelPll);
        /* Select output of USB clock divider as a clock source for USBFS peripheral. (Register: SIM_SOPT2, bit USBSRC) */
        CLOCK_SYS_SetUsbfsSrc(instance, kClockUsbfsSrcPllFllSel);

        /* Set USB clock divider */
        switch (CLOCK_SYS_GetUsbfsSrc(instance)) 
        {
        case kClockUsbfsSrcPllFllSel:
            switch (CLOCK_SYS_GetPllFllClockFreq()) 
            {
            case 120000000U:
                CLOCK_SYS_SetUsbfsDiv(instance, 4, 1);
                break;
            case 96000000U:
                CLOCK_SYS_SetUsbfsDiv(instance, 1, 0);
                break;
            case 72000000U:
                CLOCK_SYS_SetUsbfsDiv(instance, 2, 1);
                break;
            case 48000000U:
                CLOCK_SYS_SetUsbfsDiv(instance, 0, 0);
                break;
            default:
                break;
            }
            break;
        case kClockUsbfsSrcExt:
            break;
        default:
            break;
        }
        /* USB Clock Gating */
        CLOCK_SYS_EnableUsbfsClock(instance);
#endif
        /* Confirm the USB source frequency is 48MHz */
        if (48000000U == CLOCK_SYS_GetUsbfsFreq(instance)) 
        {
            /* Disable MPU. All accesses from all bus masters are allowed now. */
            MPU_CESR = (0);

            /* Enable USB regulator for device */
            SIM_HAL_SetUsbVoltRegulatorWriteCmd((SIM_Type*)(SIM_BASE), TRUE);
            SIM_HAL_SetUsbVoltRegulatorCmd((SIM_Type*)(SIM_BASE), TRUE);

            ret = USB_OK;
        }
  }
  return ret;
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : soc_get_usb_dev_int_level
* Returned Value   : usb interrupt level
* Comments         :
*    This function return usb interrupt level
*
*END*----------------------------------------------------------------------*/
uint32_t soc_get_usb_dev_int_level(uint8_t controller_id)
{
    return (uint32_t) BSP_USB_INT_LEVEL;
}
/* EOF */
