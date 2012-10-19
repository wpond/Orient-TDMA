/***************************************************************************//**
 * @file
 * @brief USB protocol stack library API for EFM32.
 * @author Energy Micro AS
 * @version 2.3.2
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2011 Energy Micro AS, http://www.energymicro.com</b>
 *******************************************************************************
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
 ******************************************************************************/
#ifndef __EFM32_USBD_H
#define __EFM32_USBD_H

#include "efm32.h"
#if defined( USB_PRESENT ) && ( USB_COUNT == 1 )
#include "efm32_usb.h"
#if defined( USB_DEVICE )

#ifdef __cplusplus
extern "C" {
#endif

/** @cond DO_NOT_INCLUDE_WITH_DOXYGEN */

#if defined( DEBUG_USB_API )
#define DEBUG_TRACE_ABORT( x )                                  \
{                                                               \
  if ( x == USB_STATUS_EP_STALLED )                             \
     DEBUG_USB_API_PUTS( "\nEP cb(), EP stalled" );             \
  else if ( x == USB_STATUS_EP_ABORTED )                        \
     DEBUG_USB_API_PUTS( "\nEP cb(), EP aborted" );             \
  else if ( x == USB_STATUS_DEVICE_UNCONFIGURED )               \
     DEBUG_USB_API_PUTS( "\nEP cb(), device unconfigured" );    \
  else if ( x == USB_STATUS_DEVICE_SUSPENDED )                  \
     DEBUG_USB_API_PUTS( "\nEP cb(), device suspended" );       \
  else /* ( x == USB_STATUS_DEVICE_RESET ) */                   \
     DEBUG_USB_API_PUTS( "\nEP cb(), device reset" );           \
}
#else
#define DEBUG_TRACE_ABORT( x )
#endif

extern USBD_Device_TypeDef *dev;

static __INLINE void USBD_ArmEp0( USBD_Ep_TypeDef *ep );
static __INLINE void USBD_ArmEpN( USBD_Ep_TypeDef *ep );
static __INLINE void USBD_AbortEp( USBD_Ep_TypeDef *ep );

void USBD_SetUsbState( USBD_State_TypeDef newState );

int  USBDCH9_SetupCmd( USBD_Device_TypeDef *device );

void USBDEP_Ep0Handler( USBD_Device_TypeDef *device );
void USBDEP_EpHandler( uint8_t epAddr );

static __INLINE void USBD_ActivateAllEps( bool forceIdle )
{
  int i;

  for ( i = 1; i <= NUM_EP_USED; i++ )
  {
    USBDHAL_ActivateEp( &dev->ep[ i ], forceIdle );
  }
}

static __INLINE void USBD_ArmEp( USBD_Ep_TypeDef *ep )
{
  if ( ep->num == 0 )
  {
    USBD_ArmEp0( ep );
  }
  else
  {
    USBD_ArmEpN( ep );
  }
}

static __INLINE void USBD_ArmEp0( USBD_Ep_TypeDef *ep )
{
  if ( ep->in )
  {
    if ( ep->remaining == 0 )       /* Zero Length Packet? */
    {
      ep->zlp = 1;
    }

#if !defined( USB_SLAVEMODE )
    USBDHAL_SetEp0InDmaPtr( ep->buf );
#endif

    USBDHAL_StartEp0In( EFM32_MIN( ep->remaining, ep->packetSize ) );
  }
  else
  {

#if !defined( USB_SLAVEMODE )
    USBDHAL_SetEp0OutDmaPtr( ep->buf );
#endif

    USBDHAL_StartEp0Out( ep->packetSize );
  }
}

static __INLINE void USBD_ArmEpN( USBD_Ep_TypeDef *ep )
{
  if ( ep->in )
  {
    USBDHAL_StartEpIn( ep );
  }
  else
  {
    USBDHAL_StartEpOut( ep );
  }
}

static __INLINE void USBD_DeactivateAllEps( USB_Status_TypeDef reason )
{
  int i;
  USBD_Ep_TypeDef *ep;

  for ( i = 1; i <= NUM_EP_USED; i++ )
  {
    ep = &dev->ep[ i ];

    if ( ep->state == D_EP_IDLE )
    {
      USBDHAL_DeactivateEp( ep );
    }
  }

  USBDHAL_AbortAllTransfers( reason );
}

static __INLINE USBD_Ep_TypeDef *USBD_GetEpFromAddr( uint8_t epAddr )
{
  int epIndex;
  USBD_Ep_TypeDef *ep = NULL;

  if ( epAddr & USB_SETUP_DIR_MASK )
  {
    epIndex = dev->inEpAddr2EpIndex[ epAddr & USB_EPNUM_MASK ];
  }
  else
  {
    epIndex = dev->outEpAddr2EpIndex[ epAddr & USB_EPNUM_MASK ];
  }

  if ( epIndex )
  {
    ep = &dev->ep[ epIndex ];
  }
  else if ( ( epAddr & USB_EPNUM_MASK ) == 0 )
  {
    ep = &dev->ep[ 0 ];
  }

  return ep;
}

static __INLINE void USBD_ReArmEp0( USBD_Ep_TypeDef *ep )
{
  if ( ep->in )
  {
    USBDHAL_StartEp0In( EFM32_MIN( ep->remaining, ep->packetSize ) );
  }
  else
  {
    USBDHAL_StartEp0Out( ep->packetSize );
  }
}

static __INLINE void USBD_AbortEp( USBD_Ep_TypeDef *ep )
{
  if ( ep->state == D_EP_IDLE )
  {
    return;
  }

  if ( ep->in )
  {
    USBDHAL_AbortEpIn( ep );
  }
  else
  {
    USBDHAL_AbortEpOut( ep );
  }
}

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* defined( USB_DEVICE ) */
#endif /* defined( USB_PRESENT ) && ( USB_COUNT == 1 ) */
#endif /* __EFM32_USBD_H */
