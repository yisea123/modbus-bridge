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
 * $FileName: usb_otg_bsp.c$
 * $Version : 
 * $Date    : 
 *
 * Comments:
 *
 *         
 *****************************************************************************/
#include "usb_otg_config.h"
#include "adapter.h"
#include "usb.h"
#include "usb_otg_main.h"
#include "usb_otg.h"
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "fsl_device_registers.h"
#include "fsl_clock_manager.h"
#include "board.h"
#include "fsl_usb_khci_hal.h"

extern uint8_t soc_get_usb_vector_number(uint8_t controller_id);
extern uint32_t soc_get_usb_base_address(uint8_t controller_id);

#include "usb_otg.h"
#define BSP_USB_INT_LEVEL                (4)

static usb_khci_otg_int_struct_t g_khci0_otg_init_param =
{
    NULL,
    0,
    BSP_USB_INT_LEVEL,
};

static usb_otg_max3353_init_struct_t g_otg_max3353_init_param;

void* bsp_usb_otg_get_init_param
(
    uint8_t controller_id
)
{
    if (USB_CONTROLLER_KHCI_0 == controller_id)
    {
        return (void*) (&g_khci0_otg_init_param);
    }
    else
    {
        return NULL;
    }
}

void* bsp_usb_otg_get_peripheral_init_param
(
    uint8_t peripheral_id
)
{
    if (peripheral_id == USB_OTG_PERIPHERAL_MAX3353)
    {
        return (void*) (&g_otg_max3353_init_param);
    }
    else
    {
        return NULL;
    }
}

/*FUNCTION*-------------------------------------------------------------------
*
* Function Name    : usb_otg_set_peripheral_init_param
* Returned Value   :
* Comments         : Set USB OTG perepheral param
*                  
*END*----------------------------------------------------------------------*/
usb_status usb_otg_set_peripheral_init_param
(
    uint8_t peripheral_id,
    usb_otg_peripheral_union_t *peripheral_param
)
{
    if (peripheral_param == NULL)
    {
        return USBERR_INVALID_PARAM;
    }
    
    if (USB_OTG_PERIPHERAL_MAX3353 == peripheral_id)
    {
        g_otg_max3353_init_param.i2c_address = peripheral_param->max3353_init.i2c_address;

        g_otg_max3353_init_param.i2c_channel = peripheral_param->max3353_init.i2c_channel;
        g_otg_max3353_init_param.i2c_vector = peripheral_param->max3353_init.i2c_vector;
        g_otg_max3353_init_param.int_pin = peripheral_param->max3353_init.int_pin;
        g_otg_max3353_init_param.int_port = peripheral_param->max3353_init.int_port;
        g_otg_max3353_init_param.int_vector = peripheral_param->max3353_init.int_vector;
        g_otg_max3353_init_param.priority = peripheral_param->max3353_init.priority;

        g_otg_max3353_init_param.interrupt_pin_init = peripheral_param->max3353_init.interrupt_pin_init;
        g_otg_max3353_init_param.interrupt_pin_flag_clear = peripheral_param->max3353_init.interrupt_pin_flag_clear;

        return USB_OK;
    }
    else
    {
        return USBERR_INVALID_PARAM;
    }
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : usb_otg_soc_clock_config
 * Returned Value   : none
 * Comments         :
 *    This function sets the selection of the high frequency clock for various USB clock options
 *
 *END*----------------------------------------------------------------------*/
static usb_status usb_otg_soc_clock_config(int32_t controller_id, usb_otg_clock_sel_t setting)
{
    usb_status ret = USB_OK;
    uint8_t usb_instance;

    if (USB_CONTROLLER_KHCI_0 == controller_id)
    {
        usb_instance = controller_id - USB_CONTROLLER_KHCI_0;
        uint32_t freq;
        clock_usbfs_src_t src;

        CLOCK_SYS_DisableUsbfsClock(usb_instance);
        switch(setting)
        {
        case USB_OTG_EXTERNAL_CLKIN:
            ret = USBERR_BAD_STATUS;
            break;
        case USB_OTG_IRC_CLK48M:
            ret = USBERR_BAD_STATUS;
            break;
        case USB_OTG_PLLFLL_CLK:
            /* PLL/FLL selected as CLK source */
            CLOCK_SYS_SetPllfllSel (kClockPllFllSelPll);
            CLOCK_SYS_SetUsbfsSrc(usb_instance, kClockUsbfsSrcPllFllSel);
            /* USB clock divider */
            src = CLOCK_SYS_GetUsbfsSrc(usb_instance);
            ret = USB_OK;
            switch(src)
            {
            case kClockUsbfsSrcExt:
                ret = USBERR_BAD_STATUS;
                break;
            case kClockUsbfsSrcPllFllSel:
                freq = CLOCK_SYS_GetPllFllClockFreq();
                switch(freq)
                {
                case 120000000U:
                    CLOCK_SYS_SetUsbfsDiv(usb_instance, 4, 1);
                    break;
                case 96000000U:
                    CLOCK_SYS_SetUsbfsDiv(usb_instance, 1, 0);
                    break;
                case 72000000U:
                    CLOCK_SYS_SetUsbfsDiv(usb_instance, 2, 1);
                    break;
                case 48000000U:
                    CLOCK_SYS_SetUsbfsDiv(usb_instance, 0, 0);
                    break;
                default:
                    ret = USBERR_BAD_STATUS;
                    break;
                }
                break;
            default:
                ret = USBERR_BAD_STATUS;
                break;
            }

            /* Confirm the USB souce frequency is 48MHz */
            if (48000000U != CLOCK_SYS_GetUsbfsFreq(usb_instance))
            {
                ret = USBERR_BAD_STATUS;
            }
            /* USB Clock Gating */
            CLOCK_SYS_EnableUsbfsClock(usb_instance);
            break;
        default:
            ret = USBERR_BAD_STATUS;
            break;
        }

    }
    else
    {
        ret = USBERR_BAD_STATUS;
    }

    return ret;
}

usb_status usb_otg_soc_init(uint8_t controller_id)
{
    usb_status ret = USB_OK;
    uint32_t base_addres = 0;

    if (USB_CONTROLLER_KHCI_0 == controller_id)
    {
        g_khci0_otg_init_param.base_ptr = (void*)soc_get_usb_base_address(controller_id);
        g_khci0_otg_init_param.vector = soc_get_usb_vector_number(controller_id);
        
        base_addres = soc_get_usb_base_address(controller_id);
        ret = usb_otg_soc_clock_config(controller_id, USB_OTG_PLLFLL_CLK);
        if (ret != USB_OK)
        {
            return ret;
        }
        /* Configure enable USB regulator for device */
        SIM_HAL_SetUsbVoltRegulatorWriteCmd((SIM_Type*) (SIM_BASE), TRUE);
        SIM_HAL_SetUsbVoltRegulatorCmd((SIM_Type*) (SIM_BASE), TRUE);
        /* reset USB CTRL register */
        usb_hal_khci_reset_control_register(base_addres);

        /* MPU is disabled. All accesses from all bus masters are allowed */
        MPU_CESR = 0;

        /* setup interrupt */
        OS_intr_init((IRQn_Type) soc_get_usb_vector_number(controller_id), g_khci0_otg_init_param.priority, 0, TRUE);

   
    }
    else
    {
        ret = USBERR_BAD_STATUS;
    }

    return ret;
}

usb_status usb_otg_host_soc_init(uint8_t controller_id)
{
    usb_status ret = USB_OK;

    ret = usb_otg_soc_clock_config(controller_id, USB_OTG_PLLFLL_CLK);

    return ret;
}

usb_status usb_otg_dev_soc_init(uint8_t controller_id)
{
    usb_status ret = USB_OK;

    ret = usb_otg_soc_clock_config(controller_id, USB_OTG_PLLFLL_CLK);

    return ret;
}

/* EOF */
