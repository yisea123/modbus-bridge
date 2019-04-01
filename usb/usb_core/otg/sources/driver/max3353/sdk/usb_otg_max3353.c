/**HEADER********************************************************************
 * 
 * Copyright (c) 2010, 2013 - 2015 Freescale Semiconductor;
 * All Rights Reserved
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
 * $FileName: usb_otg_max3353.c$
 * $Version : 
 * $Date    : 
 *
 * Comments : This file contains the implementation of the OTG functions using the MAX3353 circuit
 *
 *         
 *****************************************************************************/
#include "usb.h"
#include "usb_otg_private.h"
#include "fsl_i2c_master_driver.h"
#include "fsl_i2c_shared_function.h"
#include "adapter.h"
#include "pin_mux.h"
#include "usb_otg.h"
#include "usb_otg_max3353.h"

/*******************************************************************************
 * Global Variables
 ******************************************************************************/
/* Max 3353 ID */
uint32_t g_u32ID = 0;
/* i2c master */
i2c_master_state_t g_master;
/* i2c slave */
i2c_device_t g_slave =
{
    MAX3353_SLAVE_ADDR,
    MAX3353_SLAVE_BAUDRATE
};
extern usb_otg_khci_call_struct_t * g_otg_khci_call_ptr;
/* Private functions prototypes *********************************************/
static uint8_t _usb_otg_max3353_CheckifPresent(uint8_t i2c_channel);
static uint8_t _usb_otg_max3353_Init(uint8_t i2c_channel);
static bool _usb_otg_max3353_WriteReg(uint8_t i2c_channel, uint8_t regAdd, uint8_t regValue);
static bool _usb_otg_max3353_ReadReg(uint8_t i2c_channel, uint8_t regAdd, uint8_t* p_regValue);
static void _usb_otg_max3353_enable_disable(uint8_t i2c_channel, bool enable);
static uint8_t _usb_otg_max3353_get_status(uint8_t i2c_channel);
static uint8_t _usb_otg_max3353_get_interrupts(uint8_t i2c_channel);
static void _usb_otg_max3353_set_VBUS(uint8_t i2c_channel, bool enable);
static void _usb_otg_max3353_set_pdowns(uint8_t i2c_channel, uint8_t bitfield);
extern void* bsp_usb_otg_get_peripheral_init_param(uint8_t peripheral_id);
/* Private functions definitions *********************************************/
/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _usb_otg_max3353_i2c_isr
 * Returned Value   :
 * Comments         : _usb_otg_max3353_i2c_isr
 *    
 *
 *END*----------------------------------------------------------------------*/
static void _usb_otg_max3353_i2c_isr(void)
{
    usb_otg_max3353_init_struct_t* peripheral_init_param_ptr = (usb_otg_max3353_init_struct_t*) bsp_usb_otg_get_peripheral_init_param((uint8_t) USB_OTG_PERIPHERAL_MAX3353);
    I2C_DRV_IRQHandler(peripheral_init_param_ptr->i2c_channel);
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _usb_otg_max3353_CheckifPresent
 * Returned Value   :
 * Comments         : Check if MAX3353 is present
 *    
 *
 *END*----------------------------------------------------------------------*/
static uint8_t _usb_otg_max3353_CheckifPresent
(
    uint8_t i2c_channel
    )
{

    uint8_t u8Counter;
    uint8_t ch_read;
    for (u8Counter = 0; u8Counter < 4; u8Counter++)
    {
        g_u32ID = g_u32ID << 8;
        _usb_otg_max3353_ReadReg(i2c_channel, u8Counter, &ch_read);
        g_u32ID |= ch_read;
    }
    if (g_u32ID != MAX3353_MID)
    {
        return (MAX3353_NOT_PRESENT);
    }

    g_u32ID = 0;
    for (u8Counter = 4; u8Counter < 8; u8Counter++)
    {
        g_u32ID = g_u32ID << 8;
        _usb_otg_max3353_ReadReg(i2c_channel, u8Counter, &ch_read);
        g_u32ID |= ch_read;
    }
    if (g_u32ID != MAX3353_PID)
    {
        return (MAX3353_NOT_PRESENT);
    }
    return (MAX3353_OK);
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _usb_otg_max3353_Init
 * Returned Value   :
 * Comments         : Initialize max3353
 *    
 *
 *END*----------------------------------------------------------------------*/
static uint8_t _usb_otg_max3353_Init
(
    uint8_t i2c_channel
    )
{
    if (_usb_otg_max3353_CheckifPresent(i2c_channel))
    {
        return (MAX3353_NOT_PRESENT);
    }

    /* Enable Charge pump for VBUs detection */
    _usb_otg_max3353_WriteReg(i2c_channel, MAX3353_REG_CTRL_2, 0x80);

    /* Set Rising edge for VBUS detection */
    _usb_otg_max3353_WriteReg(i2c_channel, MAX3353_REG_INT_EDGE, VBUS_VALID_ED_MASK);

    /* Activate ID (GND and float) & SESSION Interrupts */
    _usb_otg_max3353_WriteReg(i2c_channel, MAX3353_REG_INT_MASK, ID_FLOAT_EN_MASK | ID_GND_EN_MASK\
 | VBUS_VALID_EN_MASK | SESSION_SESSEND_EN_MASK);

    return (MAX3353_OK);
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _usb_otg_max3353_WriteReg
 * Returned Value   :
 * Comments         : Write data to max3353 register
 *    
 *
 *END*----------------------------------------------------------------------*/
static bool _usb_otg_max3353_WriteReg
(
    uint8_t i2c_channel,
    uint8_t regAdd,
    uint8_t regValue
    )
{
    i2c_status_t error;
    uint8_t buff[2];
    buff[0] = regAdd;
    buff[1] = regValue;
    /* write register address and register value */
    error = I2C_DRV_MasterSendDataBlocking(i2c_channel, &g_slave, NULL, 0, buff, 2, 200);

    if (error != USB_OK)
    {
        return FALSE;
    }
    return TRUE;
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _usb_otg_max3353_ReadReg
 * Returned Value   :
 * Comments         : Read data from max3353 register
 *    
 *
 *END*----------------------------------------------------------------------*/
static bool _usb_otg_max3353_ReadReg
(
    uint8_t i2c_channel,
    uint8_t regAdd,
    uint8_t* p_regValue
    )
{
    i2c_status_t error;
    /* write register address */
    error = I2C_DRV_MasterSendDataBlocking(i2c_channel, &g_slave, NULL, 0, &regAdd, 1, 200);
    if (error != USB_OK)
    {
        return FALSE;
    }
    /* read register value */
    error = I2C_DRV_MasterReceiveDataBlocking(i2c_channel, &g_slave, NULL, 0, p_regValue, 1, 200);
    if (error != USB_OK)
    {
        return FALSE;
    }
    return TRUE;
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _usb_otg_max3353_enable_disable
 * Returned Value   :
 * Comments         : Enable/disable MAX3353
 *    
 *
 *END*----------------------------------------------------------------------*/
static void _usb_otg_max3353_enable_disable
(
    uint8_t i2c_channel,
    bool enable
    )
{
    uint8_t max3353_data;

    _usb_otg_max3353_Init(i2c_channel);
    if (enable)
    {
        max3353_data = 0x02;
        while (_usb_otg_max3353_WriteReg(i2c_channel, MAX3353_REG_CTRL_1, max3353_data) == FALSE)
        {
        }
        max3353_data = 0x1F; /* Enable interrupts */
        while (_usb_otg_max3353_WriteReg(i2c_channel, MAX3353_REG_INT_MASK, max3353_data) == FALSE)
        {
        }
        max3353_data = 0x00; /* Enable module. */
    }
    else
    {
        max3353_data = 1; /* Activate shutdown */
    }

    /* Enable/Disable module */
    while (_usb_otg_max3353_WriteReg(i2c_channel, MAX3353_REG_CTRL_2, max3353_data) == FALSE)
    {
    }

    /* Read the latch to clear any pending interrupts */
    while (_usb_otg_max3353_get_interrupts(i2c_channel))
    {
    }
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _usb_otg_max3353_get_status
 * Returned Value   : unsigned char, meaning the status of MAX3353
 * Comments         : Get MAX3353 status
 *    
 *
 *END*----------------------------------------------------------------------*/
static uint8_t _usb_otg_max3353_get_status
(
    uint8_t i2c_channel
    )
{
    uint8_t status;
    uint8_t edge;
    uint8_t new_edge;

    while (_usb_otg_max3353_ReadReg(i2c_channel, MAX3353_REG_STATUS, &status) == FALSE)
    {
    }

    /* Handle here the edge detection in SessionValid and VBus valid */
    /* Read the current edge */
    while (_usb_otg_max3353_ReadReg(i2c_channel, MAX3353_REG_INT_EDGE, &edge) == FALSE)
    {
    }

    new_edge = (uint8_t)((~(status)) & 0x03u);

    if (new_edge != edge)
    {
        /* Write the new edges */
        while (_usb_otg_max3353_WriteReg(i2c_channel, MAX3353_REG_INT_EDGE, new_edge) == FALSE)
        {
        }
    }

    return status;
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _usb_otg_max3353_get_interrupts
 * Returned Value   : unsigned char meaning interrupts
 * Comments         : Read interrupts from MAX3353
 *    
 *
 *END*----------------------------------------------------------------------*/
static uint8_t _usb_otg_max3353_get_interrupts
(
    uint8_t i2c_channel
    )
{
    uint8_t data;

    while (_usb_otg_max3353_ReadReg(i2c_channel, MAX3353_REG_INT_LATCH, &data) == FALSE)
    {
    }
    return data;
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _usb_otg_max3353_set_VBUS
 * Returned Value   :
 * Comments         : Set VBUS for MAX3353
 *    
 *
 *END*----------------------------------------------------------------------*/
static void _usb_otg_max3353_set_VBUS
(
    uint8_t i2c_channel,
    bool enable
    )
{
    uint8_t max3353_data;

    if (enable)
    {
        max3353_data = OTG_CTRL_2_VBUS_DRV;/* connect VBUS to the charge pump */
    }
    else
    {
        max3353_data = OTG_CTRL_2_VBUS_DISCHG;/* disconnect the charge pump and  activate the 5k pull down resistor */
    }

    /* Enable/Disable module */
    while (_usb_otg_max3353_WriteReg(i2c_channel, MAX3353_REG_CTRL_2, max3353_data) == FALSE)
    {
    }

}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _usb_otg_max3353_set_pdowns
 * Returned Value   :
 * Comments         : Set pull-downs for MAX3353
 *    
 *
 *END*----------------------------------------------------------------------*/
static void _usb_otg_max3353_set_pdowns
(
    uint8_t i2c_channel,
    uint8_t bitfield
    )
{
    uint8_t max3353_data;

    max3353_data = OTG_CTRL_1_IRQ_PUSH_PULL;
    if (bitfield & OTG_CTRL_PDOWN_DP)
    {
        max3353_data |= OTG_CTRL_1_PDOWN_DP;
    }

    if (bitfield & OTG_CTRL_PDOWN_DM)
    {
        max3353_data |= OTG_CTRL_1_PDOWN_DM;
    }

    while (_usb_otg_max3353_WriteReg(i2c_channel, MAX3353_REG_CTRL_1, max3353_data) == FALSE)
    {
    }
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : _usb_otg_max3353_pin_isr
 * Returned Value   :
 * Comments         : 
 *    
 *
 *END*----------------------------------------------------------------------*/
static void _usb_otg_max3353_pin_isr
(
    void
    )
{
    usb_otg_state_struct_t*        usb_otg_struct_ptr = ((usb_otg_max3353_call_struct_t *) g_otg_khci_call_ptr)->otg_handle_ptr;
    usb_otg_max3353_init_struct_t* peripheral_init_param_ptr = (usb_otg_max3353_init_struct_t*) bsp_usb_otg_get_peripheral_init_param((uint8_t) USB_OTG_PERIPHERAL_MAX3353);
    if (NULL != peripheral_init_param_ptr->interrupt_pin_flag_clear)
    {
        peripheral_init_param_ptr->interrupt_pin_flag_clear();
    }
    OS_Event_set(usb_otg_struct_ptr->otg_isr_event, USB_OTG_MAX3353_ISR_EVENT);
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : usb_otg_max3353_preinit
 * Returned Value   :
 * Comments         : 
 *    
 *
 *END*----------------------------------------------------------------------*/
usb_status usb_otg_max3353_preinit
(
    void* uplayer_handle,
    void** handle
    )
{
    usb_otg_max3353_call_struct_t * max3353_call_struct_ptr;

    max3353_call_struct_ptr = (usb_otg_max3353_call_struct_t*) OS_Mem_alloc_zero(sizeof(usb_otg_max3353_call_struct_t));
    if (max3353_call_struct_ptr == NULL)
    {
        *handle = NULL;
        return USBERR_ALLOC;
    }
    max3353_call_struct_ptr->otg_handle_ptr = (usb_otg_state_struct_t *) uplayer_handle;
    *handle = (void*) max3353_call_struct_ptr;

    return USB_OK;
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : usb_otg_max3353_init
 * Returned Value   :
 * Comments         : 
 *    
 *
 *END*----------------------------------------------------------------------*/
usb_status usb_otg_max3353_init
(
    void* handle
    )
{
    usb_otg_max3353_call_struct_t * otg_max3353_call_ptr = (usb_otg_max3353_call_struct_t*) handle;
    usb_otg_state_struct_t * usb_otg_struct_ptr = ((usb_otg_max3353_call_struct_t *) otg_max3353_call_ptr)->otg_handle_ptr;
    uint8_t i2c_channel = 0;

    otg_max3353_call_ptr->init_param_ptr = (usb_otg_max3353_init_struct_t*) bsp_usb_otg_get_peripheral_init_param((uint8_t) USB_OTG_PERIPHERAL_MAX3353);
    if ((NULL == otg_max3353_call_ptr->init_param_ptr->interrupt_pin_init) || (NULL == otg_max3353_call_ptr->init_param_ptr->interrupt_pin_flag_clear))
    {
        return USBERR_INIT_FAILED;
    }
    i2c_channel = otg_max3353_call_ptr->init_param_ptr->i2c_channel;
    /* install interrupt for i2c0 */
    OS_install_isr((IRQn_Type) otg_max3353_call_ptr->init_param_ptr->i2c_vector, (void (*)(void))_usb_otg_max3353_i2c_isr, NULL);
    /* init the i2c master */
    I2C_DRV_MasterInit(i2c_channel, &g_master);
    OS_install_isr((IRQn_Type) otg_max3353_call_ptr->init_param_ptr->int_vector, (void (*)(void))_usb_otg_max3353_pin_isr, NULL);
    OS_intr_init((IRQn_Type) otg_max3353_call_ptr->init_param_ptr->int_vector, otg_max3353_call_ptr->init_param_ptr->priority, 0, TRUE);
#if defined (FSL_RTOS_FREE_RTOS)
    NVIC_SetPriority((IRQn_Type)otg_max3353_call_ptr->init_param_ptr->i2c_vector,3);
#endif
    /* set interrupt pin for max 3353 out put interrupt */
    //bsp_usb_otg_max3353_set_pin_int(FALSE, TRUE);
    otg_max3353_call_ptr->init_param_ptr->interrupt_pin_init(FALSE, TRUE);
    /* set event to read the status */
    OS_Event_set(usb_otg_struct_ptr->otg_isr_event, USB_OTG_MAX3353_ISR_EVENT);
    OS_Unlock(); /* need to use i2c interrupt */
    _usb_otg_max3353_enable_disable(i2c_channel, TRUE);
    OS_Lock();
    return USB_OK;
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : usb_otg_max3353_shut_down
 * Returned Value   :
 * Comments         : 
 *    
 *
 *END*----------------------------------------------------------------------*/
usb_status usb_otg_max3353_shut_down
(
    void* handle
    )
{
    usb_otg_max3353_call_struct_t * otg_max3353_call_ptr = (usb_otg_max3353_call_struct_t*) handle;
    usb_otg_state_struct_t * usb_otg_struct_ptr = ((usb_otg_max3353_call_struct_t *) otg_max3353_call_ptr)->otg_handle_ptr;
    uint8_t channel = otg_max3353_call_ptr->init_param_ptr->i2c_channel;

    /* configure GPIO for I2C function */
    if (NULL != otg_max3353_call_ptr->init_param_ptr->interrupt_pin_init)
    {
        otg_max3353_call_ptr->init_param_ptr->interrupt_pin_init(FALSE, FALSE);
    }
    OS_Event_clear(usb_otg_struct_ptr->otg_isr_event, USB_OTG_MAX3353_ISR_EVENT);
    _usb_otg_max3353_enable_disable(channel, FALSE);
    return USB_OK;
}

/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : usb_otg_max3353_get_status
 * Returned Value   :
 * Comments         : 
 *    
 *
 *END*----------------------------------------------------------------------*/
usb_status usb_otg_max3353_get_status
(
    void* handle
    )
{
    usb_otg_max3353_call_struct_t * otg_max3353_call_ptr = (usb_otg_max3353_call_struct_t*) handle;
    uint8_t status;
    uint8_t channel = otg_max3353_call_ptr->init_param_ptr->i2c_channel;
    usb_otg_state_struct_t * usb_otg_struct_ptr = otg_max3353_call_ptr->otg_handle_ptr;
    usb_otg_status_t * otg_status_ptr = &usb_otg_struct_ptr->otg_status;

    while (_usb_otg_max3353_get_interrupts(channel))
    {
    }

    status = _usb_otg_max3353_get_status(channel);
    /* check the status indications */
    /* ID status update */
    otg_status_ptr->id = (uint8_t)((status & OTG_STAT_ID_FLOAT) ? TRUE : FALSE);
    /* V_BUS_VALID status update */
    otg_status_ptr->vbus_valid = (uint8_t)((status & OTG_STAT_VBUS_VALID) ? TRUE : FALSE);
    /* SESS_VALID status update */
    otg_status_ptr->sess_valid = (uint8_t)((status & OTG_STAT_SESS_VALID) ? TRUE : FALSE);
    /* SESS_END status update */
    otg_status_ptr->sess_end = (uint8_t)((status & OTG_STAT_SESS_END) ? TRUE : FALSE);

    return USB_OK;
}
/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : usb_otg_max3353_set_vbus
 * Returned Value   :
 * Comments         : 
 *    
 *
 *END*----------------------------------------------------------------------*/
usb_status usb_otg_max3353_set_vbus
(
    void* handle,
    bool enable
    )
{
    usb_otg_max3353_call_struct_t * otg_max3353_call_ptr = (usb_otg_max3353_call_struct_t*) handle;
    uint8_t channel = otg_max3353_call_ptr->init_param_ptr->i2c_channel;
    _usb_otg_max3353_set_VBUS(channel, enable);

    return USB_OK;
}
/*FUNCTION*-------------------------------------------------------------------
 *
 * Function Name    : usb_otg_max3353_set_pull_downs
 * Returned Value   :
 * Comments         : 
 *    
 *
 *END*----------------------------------------------------------------------*/
usb_status usb_otg_max3353_set_pull_downs
(
    void* handle,
    uint8_t bitfield
    )
{
    usb_otg_max3353_call_struct_t * otg_max3353_call_ptr = (usb_otg_max3353_call_struct_t*) handle;
    uint8_t channel = otg_max3353_call_ptr->init_param_ptr->i2c_channel;
    _usb_otg_max3353_set_pdowns(channel, bitfield);
    return USB_OK;
}
/* EOF */
