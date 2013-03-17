/**************************************************************************//**
 * @file main.c
 * @brief USB CDC Serial Port adapter example project.
 * @author Energy Micro AS
 * @version 1.2.2
 ******************************************************************************
 * @section License
 * <b>(C) Copyright 2010 Energy Micro AS, http://www.energymicro.com</b>
 ******************************************************************************
 *
 * This source code is the property of Energy Micro AS. The source and compiled
 * code may only be used on Energy Micro "EFM32" microcontrollers.
 *
 * This copyright notice may not be removed from the source code nor changed.
 *
 * DISCLAIMER OF WARRANTY/LIMITATION OF REMEDIES: Energy Micro AS has no
 * obligation to support this Software. Energy Micro AS is providing the
 * Software "AS IS", with no express or implied warranties of any kind,
 * including, but not limited to, any implied warranties of merchantability
 * or fitness for any particular purpose or warranties against infringement
 * of any proprietary rights of a third party.
 *
 * Energy Micro AS will not be liable for any consequential, incidental, or
 * special damages, or any other relief, or for any claim by any third party,
 * arising from your use of this Software.
 *
 *****************************************************************************/
#include "efm32.h"
#include "efm32_cmu.h"
#include "efm32_dma.h"
#include "efm32_gpio.h"
#include "efm32_int.h"
#include "efm32_rtc.h"
#include "efm32_usb.h"
#include "efm32_usart.h"

#include "queue.h"
#include "usb.h"
#include "led.h"

/**************************************************************************//**
 *
 * This example shows how a CDC based USB to Serial port adapter can be
 * implemented.
 *
 * Use the file EFM32-Cdc.inf to install a USB serial port device driver
 * on the host PC.
 *
 * This implementation uses DMA to transfer data between UART1 and memory
 * buffers.
 *
 *****************************************************************************/

/*** Typedef's and defines. ***/

/* Define USB endpoint addresses */
#define EP_DATA_OUT       0x01  /* Endpoint for USB data reception.       */
#define EP_DATA_IN        0x81  /* Endpoint for USB data transmission.    */
#define EP_NOTIFY         0x82  /* The notification endpoint (not used).  */

#define BULK_EP_SIZE     USB_MAX_EP_SIZE  /* This is the max. ep size.    */
#define USB_RX_BUF_SIZ   BULK_EP_SIZE /* Packet size when receiving on USB*/
#define USB_TX_BUF_SIZ   127    /* Packet size when transmitting on USB.  */

/* Calculate a timeout in ms corresponding to 5 char times on current     */
/* baudrate. Minimum timeout is set to 10 ms.                             */
#define RX_TIMEOUT    EFM32_MAX(10U, 50000 / (cdcLineCoding.dwDTERate))

/* The serial port LINE CODING data structure, used to carry information  */
/* about serial port baudrate, parity etc. between host and device.       */
EFM32_PACK_START(1)
typedef struct
{
  uint32_t dwDTERate;               /** Baudrate                            */
  uint8_t  bCharFormat;             /** Stop bits, 0=1 1=1.5 2=2            */
  uint8_t  bParityType;             /** 0=None 1=Odd 2=Even 3=Mark 4=Space  */
  uint8_t  bDataBits;               /** 5, 6, 7, 8 or 16                    */
  uint8_t  dummy;                   /** To ensure size is a multiple of 4 bytes.*/
} __attribute__ ((packed)) cdcLineCoding_TypeDef;
EFM32_PACK_END()


/*** Function prototypes. ***/

//static void DmaSetup(void);
static int  LineCodingReceived(USB_Status_TypeDef status,
                               uint32_t xferred,
                               uint32_t remaining);
static void StateChange(USBD_State_TypeDef oldState,
                        USBD_State_TypeDef newState);
static int  SetupCmd(const USB_Setup_TypeDef *setup);
static void StateChange(USBD_State_TypeDef oldState,
                        USBD_State_TypeDef newState);

/*** Include device descriptor definitions. ***/

#include "descriptors.h"


/*** Variables ***/

/*
 * The LineCoding variable must be 4-byte aligned as it is used as USB
 * transmit and receive buffer
 */
EFM32_ALIGN(4)
EFM32_PACK_START(1)
static cdcLineCoding_TypeDef __attribute__ ((aligned(4))) cdcLineCoding =
{
  10000000, 0, 0, 8, 0
};
EFM32_PACK_END()

STATIC_UBUF(usbRxBuffer0, USB_RX_BUF_SIZ);    /* USB receive buffers.   */
STATIC_UBUF(usbRxBuffer1, USB_RX_BUF_SIZ);

const uint8_t  *usbRxBuffer[  2 ] = { usbRxBuffer0, usbRxBuffer1 };


int            usbRxIndex, usbBytesReceived;

static bool           usbRxActive, dmaTxActive;
static volatile bool           usbOnline, usbActive;

#define SEND_MEM_SIZE 19200
#define MIN_SEND_SIZE 0

static uint8_t usbSendMem[2][SEND_MEM_SIZE],
	usbSending = 1;
static uint16_t usbSendFill[2] = { 0, 0 };

queue_t usbQueue;
uint8_t usbRecvMem[USB_RECV_SIZE * 32],
	usbRecvPacket[32],
	usbRecvPacketPos = 0;

/**************************************************************************//**
 * @brief main - the entrypoint after reset.
 *****************************************************************************/
void USB_Init(void)
{
	
	QUEUE_Init(&usbQueue,usbRecvMem,32,USB_RECV_SIZE);
	
	//CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFXO);

	USB->CTRL |= USB_CTRL_VREGOSEN;

	usbOnline = false;
	USBD_Init(&initstruct);

	/*
	* When using a debugger it is practical to uncomment the following three
	* lines to force host to re-enumerate the device.
	*/
	USBD_Disconnect();
	USBTIMER_DelayMs(1000);
	USBD_Connect();
	
	while(!usbOnline);

	//TRACE("USB OK\n");
    
}

/**************************************************************************//**
 * @brief Callback function called whenever a new packet with data is received
 *        on USB.
 *
 * @param[in] status    Transfer status code.
 * @param[in] xferred   Number of bytes transferred.
 * @param[in] remaining Number of bytes not transferred.
 *
 * @return USB_STATUS_OK.
 *****************************************************************************/
static int UsbDataReceived(USB_Status_TypeDef status,
                           uint32_t xferred,
                           uint32_t remaining)
{
  (void) remaining;            /* Unused parameter */

  if ((status == USB_STATUS_OK) && (xferred > 0))
  {
    for (int i = 0; i < xferred; i++) {
        // handle msg
    	usbRecvPacket[usbRecvPacketPos++] = usbRxBuffer[ usbRxIndex ][i];
    	if (usbRecvPacketPos == 32)
    	{
			QUEUE_Queue(&usbQueue,(void*)usbRecvPacket);
			usbRecvPacketPos = 0;
    	}
    }
    
    usbRxIndex ^= 1;
    
      /* Start a new USB receive transfer. */
    USBD_Read(EP_DATA_OUT, (void*) usbRxBuffer[ usbRxIndex ],
                USB_RX_BUF_SIZ, UsbDataReceived);
  }
  return USB_STATUS_OK;
}

bool USB_Recv(uint8_t packet[32])
{
	return QUEUE_Dequeue(&usbQueue,packet);
}

static int USB_TransmitComplete(USB_Status_TypeDef status,
                              uint32_t xferred,
                              uint32_t remaining);

static int USB_TransmitComplete(USB_Status_TypeDef status,
                              uint32_t xferred,
                              uint32_t remaining)
{
  (void) xferred;              /* Unused parameter */
  (void) remaining;            /* Unused parameter */
	
	if (status == USB_STATUS_OK)
	{
		
		uint8_t _usbWriting;
		if (usbSending == 0)
		{
			_usbWriting = 1;
		}
		else
		{
			_usbWriting = 0;
		}
		
		if (usbSendFill[_usbWriting] > MIN_SEND_SIZE)
		{
			USBD_Write(EP_DATA_IN, usbSendMem[_usbWriting], usbSendFill[_usbWriting], USB_TransmitComplete);
			usbSending = _usbWriting;
			usbSendFill[_usbWriting] = 0;
		}
		else
		{
			usbActive = false;
		}
		
	}
	
	return USB_STATUS_OK;
}

bool USB_Transmit(uint8_t *data, int len)
{
	
    if (!usbOnline)// || len > USB_SEND_QUEUE_SIZE)
    {
        return false;
	}
	
	INT_Disable();
	uint8_t _usbWriting;
	if (usbSending == 0)
	{
		_usbWriting = 1;
	}
	else
	{
		_usbWriting = 0;
	}
	
	if (SEND_MEM_SIZE - usbSendFill[_usbWriting] < len)
	{
		if (!usbActive)
		{
			usbActive = true;
			USBD_Write(EP_DATA_IN, usbSendMem[_usbWriting], usbSendFill[_usbWriting], USB_TransmitComplete);
			usbSending = _usbWriting;
		}
		else
		{
			INT_Enable();
			LED_On(RED);
			return false;
		}
	}
	
	memcpy(&usbSendMem[_usbWriting][usbSendFill[_usbWriting]],data,len);
	usbSendFill[_usbWriting] += len;
	
	if (!usbActive && usbSendFill[_usbWriting] > MIN_SEND_SIZE)
	{
		usbActive = true;
		USBD_Write(EP_DATA_IN, usbSendMem[_usbWriting], usbSendFill[_usbWriting], USB_TransmitComplete);
		usbSending = _usbWriting;
		usbSendFill[_usbWriting] = 0;
	}
	INT_Enable();
	
	return true;
	
}

/**************************************************************************//**
 * @brief
 *   Callback function called each time the USB device state is changed.
 *   Starts CDC operation when device has been configured by USB host.
 *
 * @param[in] oldState The device state the device has just left.
 * @param[in] newState The new device state.
 *****************************************************************************/
static void StateChange(USBD_State_TypeDef oldState,
                        USBD_State_TypeDef newState)
{
  if (newState == USBD_STATE_CONFIGURED)
  {
    /* We have been configured, start CDC functionality ! */

    if (oldState == USBD_STATE_SUSPENDED)   /* Resume ?   */
    {
    }

    /* Start receiving data from USB host. */
    usbRxIndex  = 0;
    usbRxActive = true;
    dmaTxActive = false;
    USBD_Read(EP_DATA_OUT, (void*) usbRxBuffer[ usbRxIndex ],
              USB_RX_BUF_SIZ, UsbDataReceived);
    
    usbOnline = true;
  } 

  else if ((oldState == USBD_STATE_CONFIGURED) &&
           (newState != USBD_STATE_SUSPENDED))
  {
    /* We have been de-configured, stop CDC functionality */
    USBTIMER_Stop(0);
    DMA->CHENC = 3;     /* Stop DMA channels 0 and 1. */
    usbOnline = false;
  }

  else if (newState == USBD_STATE_SUSPENDED)
  {
    /* We have been suspended, stop CDC functionality */
    /* Reduce current consumption to below 2.5 mA.    */
    USBTIMER_Stop(0);
    DMA->CHENC = 3;     /* Stop DMA channels 0 and 1. */
    usbOnline = false;
  }
}

/**************************************************************************//**
 * @brief
 *   Handle USB setup commands. Implements CDC class specific commands.
 *
 * @param[in] setup Pointer to the setup packet received.
 *
 * @return USB_STATUS_OK if command accepted.
 *         USB_STATUS_REQ_UNHANDLED when command is unknown, the USB device
 *         stack will handle the request.
 *****************************************************************************/
static int SetupCmd(const USB_Setup_TypeDef *setup)
{
  int retVal = USB_STATUS_REQ_UNHANDLED;

  if ((setup->Type == USB_SETUP_TYPE_CLASS) &&
      (setup->Recipient == USB_SETUP_RECIPIENT_INTERFACE))
  {
    switch (setup->bRequest)
    {
    case USB_CDC_GETLINECODING:
      /********************/
      if ((setup->wValue == 0) &&
          (setup->wIndex == 0) &&               /* Interface no.            */
          (setup->wLength == 7) &&              /* Length of cdcLineCoding  */
          (setup->Direction == USB_SETUP_DIR_IN))
      {
        /* Send current settings to USB host. */
        USBD_Write(0, (void*) &cdcLineCoding, 7, NULL);
        retVal = USB_STATUS_OK;
      }
      break;

    case USB_CDC_SETLINECODING:
      /********************/
      if ((setup->wValue == 0) &&
          (setup->wIndex == 0) &&               /* Interface no.            */
          (setup->wLength == 7) &&              /* Length of cdcLineCoding  */
          (setup->Direction != USB_SETUP_DIR_IN))
      {
        /* Get new settings from USB host. */
        USBD_Read(0, (void*) &cdcLineCoding, 7, LineCodingReceived);
        retVal = USB_STATUS_OK;
      }
      break;

    case USB_CDC_SETCTRLLINESTATE:
      /********************/
      if ((setup->wIndex == 0) &&               /* Interface no.  */
          (setup->wLength == 0))                /* No data        */
      {
        /* Do nothing ( Non compliant behaviour !! ) */
        retVal = USB_STATUS_OK;
      }
      break;
    }
  }

  return retVal;
}

/**************************************************************************//**
 * @brief
 *   Callback function called when the data stage of a CDC_SET_LINECODING
 *   setup command has completed.
 *
 * @param[in] status    Transfer status code.
 * @param[in] xferred   Number of bytes transferred.
 * @param[in] remaining Number of bytes not transferred.
 *
 * @return USB_STATUS_OK if data accepted.
 *         USB_STATUS_REQ_ERR if data calls for modes we can not support.
 *****************************************************************************/
static int LineCodingReceived(USB_Status_TypeDef status,
                              uint32_t xferred,
                              uint32_t remaining)
{
  uint32_t frame = 0;
  (void) remaining;

  /* We have received new serial port communication settings from USB host */
  if ((status == USB_STATUS_OK) && (xferred == 7))
  {
    /* Check bDataBits, valid values are: 5, 6, 7, 8 or 16 bits */
    if (cdcLineCoding.bDataBits == 5)
      frame |= UART_FRAME_DATABITS_FIVE;

    else if (cdcLineCoding.bDataBits == 6)
      frame |= UART_FRAME_DATABITS_SIX;

    else if (cdcLineCoding.bDataBits == 7)
      frame |= UART_FRAME_DATABITS_SEVEN;

    else if (cdcLineCoding.bDataBits == 8)
      frame |= UART_FRAME_DATABITS_EIGHT;

    else if (cdcLineCoding.bDataBits == 16)
      frame |= UART_FRAME_DATABITS_SIXTEEN;

    else
      return USB_STATUS_REQ_ERR;

    /* Check bParityType, valid values are: 0=None 1=Odd 2=Even 3=Mark 4=Space  */
    if (cdcLineCoding.bParityType == 0)
      frame |= UART_FRAME_PARITY_NONE;

    else if (cdcLineCoding.bParityType == 1)
      frame |= UART_FRAME_PARITY_ODD;

    else if (cdcLineCoding.bParityType == 2)
      frame |= UART_FRAME_PARITY_EVEN;

    else if (cdcLineCoding.bParityType == 3)
      return USB_STATUS_REQ_ERR;

    else if (cdcLineCoding.bParityType == 4)
      return USB_STATUS_REQ_ERR;

    else
      return USB_STATUS_REQ_ERR;

    /* Check bCharFormat, valid values are: 0=1 1=1.5 2=2 stop bits */
    if (cdcLineCoding.bCharFormat == 0)
      frame |= UART_FRAME_STOPBITS_ONE;

    else if (cdcLineCoding.bCharFormat == 1)
      frame |= UART_FRAME_STOPBITS_ONEANDAHALF;

    else if (cdcLineCoding.bCharFormat == 2)
      frame |= UART_FRAME_STOPBITS_TWO;

    else
      return USB_STATUS_REQ_ERR;

//    /* Program new UART baudrate etc. */
//    UART1->FRAME = frame;
//    USART_BaudrateAsyncSet(UART1, 0, cdcLineCoding.dwDTERate, usartOVS16);

    return USB_STATUS_OK;
  }
  return USB_STATUS_REQ_ERR;
}

