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
#ifndef __EFM32_USBH_H
#define __EFM32_USBH_H

#include "efm32.h"
#if defined( USB_PRESENT ) && ( USB_COUNT == 1 )
#include "efm32_usb.h"
#if defined( USB_HOST )

#ifdef __cplusplus
extern "C" {
#endif

/** @cond DO_NOT_INCLUDE_WITH_DOXYGEN */

extern USBH_Hc_TypeDef                  hcs[];
extern int                              USBH_AttachRetryCount;
extern const USBH_AttachTiming_TypeDef  USBH_AttachTiming[];
extern USBH_Init_TypeDef                USBH_InitData;
extern volatile USBH_PortState_TypeDef  USBH_PortStatus;

USB_Status_TypeDef USBH_CtlSendSetup(   USBH_Ep_TypeDef *ep );
USB_Status_TypeDef USBH_CtlSendData(    USBH_Ep_TypeDef *ep, uint16_t length );
USB_Status_TypeDef USBH_CtlReceiveData( USBH_Ep_TypeDef *ep, uint16_t length );

void USBHEP_EpHandler(     USBH_Ep_TypeDef *ep, USB_Status_TypeDef result );
void USBHEP_CtrlEpHandler( USBH_Ep_TypeDef *ep, USB_Status_TypeDef result );
void USBHEP_TransferDone(  USBH_Ep_TypeDef *ep, USB_Status_TypeDef result );

static __INLINE uint16_t USBH_GetFrameNum( void )
{
  return USBHHAL_GetFrameNum();
}

static __INLINE bool USBH_FrameNumIsEven( void )
{
  return ( USBHHAL_GetFrameNum() & 1 ) == 0;
}

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* defined( USB_HOST ) */
#endif /* defined( USB_PRESENT ) && ( USB_COUNT == 1 ) */
#endif /* __EFM32_USBH_H      */
