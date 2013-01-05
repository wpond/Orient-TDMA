/**************************************************************************//**
 * @file
 * @brief USB protocol stack library, USB device endpoint handlers.
 * @author Energy Micro AS
 * @version 2.3.2
 ******************************************************************************
 * @section License
 * <b>(C) Copyright 2011 Energy Micro AS, http://www.energymicro.com</b>
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
#if defined( USB_PRESENT ) && ( USB_COUNT == 1 )
#include "efm32_usb.h"
#if defined( USB_DEVICE )

#include "efm32_usbtypes.h"
#include "efm32_usbhal.h"
#include "efm32_usbd.h"

/** @cond DO_NOT_INCLUDE_WITH_DOXYGEN */

/*
 * USBDEP_Ep0Handler() is called each time a packet has been transmitted
 * or recieved on the default endpoint.
 * A state machine navigate us through the phases of a control transfer
 * according to "chapter 9" in the USB spec.
 */
void USBDEP_Ep0Handler( USBD_Device_TypeDef *device )
{
  int status;
  USBD_Ep_TypeDef *ep;
  static bool statusIn;
  static uint32_t xferred;
  static USB_XferCompleteCb_TypeDef callback;

  ep = &device->ep[ 0 ];
  switch ( ep->state )
  {
    case D_EP_IDLE:
      ep->remaining = 0;
      ep->zlp = 0;
      callback = NULL;
      statusIn = false;

      status = USBDCH9_SetupCmd( device );

      if ( status == USB_STATUS_REQ_ERR )
      {
        ep->in = true;
        USBDHAL_StallEp( ep );              /* Stall Ep0 IN                 */
        USBDHAL_ReenableEp0Setup( device ); /* Prepare for next SETUP packet*/
        ep->in = false;                     /* OUT for next SETUP           */
      }
      else /* ( Status == USB_STATUS_OK ) */
      {
        if ( (ep->state == D_EP_RECEIVING) || (ep->state == D_EP_TRANSMITTING) )
        {
          callback = ep->xferCompleteCb;
        }

        if ( ep->state != D_EP_RECEIVING )
        {
          if ( ep->remaining )
          {
            /* Data will be sent to host, check if a ZLP must be appended */
            if ( ( ep->remaining < device->setup->wLength ) &&
                 ( ep->remaining % ep->packetSize == 0    )    )
            {
              ep->zlp = 1;
            }
          }
          else
          {
            /* Prepare for next SETUP packet*/
            USBDHAL_ReenableEp0Setup( device );

            /* No data stage, a ZLP may have been sent. If not, send one */

            xferred = 0;
            if ( ep->zlp == 0 )
            {
              USBD_Write( 0, NULL, 0, NULL );             /* ACK to host */
              ep->state = D_EP_STATUS;
            }
            else
            {
              ep->state = D_EP_IDLE;
              ep->in = false;                      /* OUT for next SETUP */
            }
          }
        }
      }
      break;

    case D_EP_RECEIVING:
      if ( ep->remaining )
      {
        /* There is more data to receive */
        USBD_ReArmEp0( ep );
      }
      else
      {
        status = USB_STATUS_OK;
        if ( callback != NULL )
        {
          status = callback( USB_STATUS_OK, ep->xferred, 0 );
          callback = NULL;
        }

        if ( status != USB_STATUS_OK )
        {
          ep->in = true;
          USBDHAL_StallEp( ep );              /* Stall Ep0 IN                */
          USBDHAL_ReenableEp0Setup( device ); /* Prepare for next SETUP pkt. */
          ep->state = D_EP_IDLE;
        }
        else /* Everything OK, send a ZLP (ACK) to host */
        {
          USBDHAL_ReenableEp0Setup( device );/* Prepare for next SETUP packet*/

          ep->state = D_EP_IDLE;              /* USBD_Write() sets state back*/
                                              /* to EP_TRANSMITTING          */
          USBD_Write( 0, NULL, 0, NULL );
          ep->state = D_EP_STATUS;
        }
      }
      break;

    case D_EP_TRANSMITTING:
      if ( ep->remaining )
      {
        /* There is more data to transmit */
        USBD_ReArmEp0( ep );
      }
      else
      {
        /* All data transferred, is a ZLP packet needed ? */
        if ( ep->zlp == 1 )
        {
          xferred   = ep->xferred;
          ep->state = D_EP_IDLE;          /* USBD_Write() sets state back */
                                          /* to EP_TRANSMITTING           */
          USBD_Write( 0, NULL, 0, NULL ); /* Send ZLP                     */
          ep->zlp = 2;
        }
        else
        {
          if ( ep->zlp == 0 )
          {
            xferred = ep->xferred;
          }

          ep->state = D_EP_IDLE;
          USBD_Read( 0, NULL, 0, NULL );  /* Get ZLP packet (ACK) from host */
          statusIn = true;
          ep->state = D_EP_STATUS;
        }
      }
      break;

    case D_EP_STATUS:
      if ( statusIn )
      {
        USBDHAL_ReenableEp0Setup( device );
      }

      if ( callback != NULL )
      {
        callback( USB_STATUS_OK, xferred, 0 );
      }

      ep->state = D_EP_IDLE;
      ep->in = false;                     /* OUT for next SETUP */
      break;

    default:
      EFM_ASSERT( false );
      break;
  }
}

/*
 * USBDEP_EpHandler() is called each time a packet has been transmitted
 * or recieved on an endpoint other than the default endpoint.
 */
void USBDEP_EpHandler( uint8_t epAddr )
{
  USB_XferCompleteCb_TypeDef callback;
  USBD_Ep_TypeDef *ep = USBD_GetEpFromAddr( epAddr );

  if ( ( ep->state == D_EP_TRANSMITTING ) || ( ep->state == D_EP_RECEIVING ) )
  {
    ep->state = D_EP_IDLE;
    if ( ep->xferCompleteCb )
    {
      callback = ep->xferCompleteCb;
      ep->xferCompleteCb = NULL;
      callback( USB_STATUS_OK, ep->xferred, ep->remaining );
    }
  }
  else
  {
    EFM_ASSERT( false );
  }
}

/** @endcond */

#endif /* defined( USB_DEVICE ) */
#endif /* defined( USB_PRESENT ) && ( USB_COUNT == 1 ) */
