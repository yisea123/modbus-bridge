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
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "fsl_device_registers.h"
#include "fsl_clock_manager.h"
#include "fsl_usb_khci_hal.h"

#define BSP_USB_INT_LEVEL                (3)
extern uint8_t soc_get_usb_vector_number(uint8_t controller_id);
extern uint32_t soc_get_usb_base_address(uint8_t controller_id);

usb_status usb_dev_soc_init(uint8_t controller_id)
{
    usb_status ret = USB_OK;
    uint8_t usb_instance = 0;
    uint32_t base_addres = 0;
    
    if (USB_CONTROLLER_KHCI_0 == controller_id)
    {
        usb_instance = controller_id - USB_CONTROLLER_KHCI_0;
        base_addres = soc_get_usb_base_address(controller_id);
        clock_usbfs_src_t src;
#if USBCFG_DEV_USE_IRC48M
        /* IRC48M selected as CLK source */
        CLOCK_SYS_SetUsbfsSrc(usb_instance, kClockUsbfsSrcIrc48M);
        /* USB Clock Gating */
        CLOCK_SYS_EnableUsbfsClock(usb_instance);
        /* Enable IRC 48MHz for USB module */
        usb_hal_khci_ungate_irc48m(base_addres);
        usb_hal_khci_enable_irc48m_recovery_block(base_addres);
#else
        /* USB_CLKIN selected as CLK source */
        CLOCK_SYS_SetUsbfsSrc(usb_instance, kClockUsbfsSrcExt);
        /* USB Clock Gating */
        CLOCK_SYS_EnableUsbfsClock(usb_instance);
#endif
        /* Set USB clock divider */
        src = CLOCK_SYS_GetUsbfsSrc(usb_instance);
        switch(src)
        {
        case kClockUsbfsSrcExt:
            break;
        case kClockUsbfsSrcIrc48M:
            break;
        default:
            ret = USBERR_BAD_STATUS;
            break;
        }
        /* Confirm the USB souce frequency is 48MHz */
        if(48000000U != CLOCK_SYS_GetUsbfsFreq(usb_instance))
        {
            ret = USBERR_BAD_STATUS;
        }
        if (ret != USB_OK)
        {
            return USBERR_BAD_STATUS;
        }
        
        /* Configure enable USB regulator for device */
        SIM_HAL_SetUsbVoltRegulatorWriteCmd((SIM_Type*)(SIM_BASE), TRUE);
        SIM_HAL_SetUsbVoltRegulatorCmd((SIM_Type*)(SIM_BASE), TRUE);

    }
    else
    {
        ret = USBERR_BAD_STATUS;
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
