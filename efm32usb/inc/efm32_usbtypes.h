/***************************************************************************//**
 * @file
 * @brief USB protocol stack library, internal type definitions.
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
#ifndef __EFM32_USBTYPES_H
#define __EFM32_USBTYPES_H

#include "efm32.h"
#if defined( USB_PRESENT ) && ( USB_COUNT == 1 )
#include "efm32_usb.h"
#if defined( USB_DEVICE ) || defined( USB_HOST )

#ifdef __cplusplus
extern "C" {
#endif

/** @cond DO_NOT_INCLUDE_WITH_DOXYGEN */

/* Limits imposed by the USB peripheral */
#define NP_RX_QUE_DEPTH       8
#define HP_RX_QUE_DEPTH       8
#define MAX_XFER_LEN          524287L         /* 2^19 - 1 bytes             */
#define MAX_PACKETS_PR_XFER   1023            /* 2^10 - 1 packets           */
#define MAX_NUM_TX_FIFOS      6               /* In addition to EP0 Tx FIFO */
#define MAX_NUM_IN_EPS        6               /* In addition to EP0         */
#define MAX_NUM_OUT_EPS       6               /* In addition to EP0         */

/* Limit imposed by the USB standard */
#define MAX_USB_EP_NUM      15

/* Developer mode debugging macro's */

#if defined( DEBUG_USB_INT_LO )
  #define DEBUG_USB_INT_LO_PUTS( s )    USB_PUTS( s )
  #define DEBUG_USB_INT_LO_PUTCHAR( c ) USB_PUTCHAR( c )
#else
  #define DEBUG_USB_INT_LO_PUTS( s )
  #define DEBUG_USB_INT_LO_PUTCHAR( c )
#endif /* defined( DEBUG_USB_INT_LO ) */

#if defined( DEBUG_USB_INT_HI )
  #define DEBUG_USB_INT_HI_PUTS( s )    USB_PUTS( s )
  #define DEBUG_USB_INT_HI_PUTCHAR( c ) USB_PUTCHAR( c )
#else
  #define DEBUG_USB_INT_HI_PUTS( s )
  #define DEBUG_USB_INT_HI_PUTCHAR( c )
#endif /* defined( DEBUG_USB_INT_HI ) */

#if defined( USB_HOST )
  #if defined( NUM_APP_TIMERS )
    #define HOSTPORT_TIMER_INDEX  (NUM_APP_TIMERS)
  #else
    #define HOSTPORT_TIMER_INDEX  (0)
  #endif
  #define HOSTCH_TIMER_INDEX      (HOSTPORT_TIMER_INDEX + 1 )
#endif

#if defined( USB_HOST )
#define HCS_NAK       0x01
#define HCS_STALL     0x02
#define HCS_XACT      0x04
#define HCS_TGLERR    0x08
#define HCS_BABBLE    0x10
#define HCS_TIMEOUT   0x20
#define HCS_COMPLETED 0x40
#define HCS_RETRY     0x80
#endif

#if defined( USB_DEVICE )
typedef enum
{
  D_EP_IDLE          = 0,
  D_EP_TRANSMITTING  = 1,
  D_EP_RECEIVING     = 2,
  D_EP_STATUS        = 3
} USBD_EpState_TypeDef;

typedef struct
{
  bool                        in;
  uint8_t                     zlp;
  uint8_t                     num;
  uint8_t                     addr;
  uint8_t                     type;
  uint8_t                     txFifoNum;
  uint8_t                     *buf;
  uint16_t                    packetSize;
  uint16_t                    mask;
  uint32_t                    remaining;
  uint32_t                    xferred;
#if !defined( USB_SLAVEMODE )
  uint32_t                    hwXferSize;
#endif
  uint32_t                    fifoSize;
  USBD_EpState_TypeDef        state;
  USB_XferCompleteCb_TypeDef  xferCompleteCb;
} USBD_Ep_TypeDef;

typedef struct
{
  USB_Setup_TypeDef                     *setup;
#if !defined( USB_SLAVEMODE )
  USB_Setup_TypeDef                     setupPkt[3];
#else
  USB_Setup_TypeDef                     setupPkt[1];
#endif
  uint8_t                               configurationValue; /* Must be DWORD aligned */
  bool                                  remoteWakeupEnabled;
  uint8_t                               numberOfStrings;
  USBD_State_TypeDef                    state;
  USBD_State_TypeDef                    savedState;
  USBD_State_TypeDef                    lastState;
  const USB_DeviceDescriptor_TypeDef    *deviceDescriptor;
  const USB_ConfigurationDescriptor_TypeDef *configDescriptor;
  const void * const                    *stringDescriptors;
  const USBD_Callbacks_TypeDef          *callbacks;
  USBD_Ep_TypeDef                       ep[ NUM_EP_USED + 1 ];
  uint8_t                               inEpAddr2EpIndex[  MAX_USB_EP_NUM + 1 ];
  uint8_t                               outEpAddr2EpIndex[ MAX_USB_EP_NUM + 1 ];
} USBD_Device_TypeDef;
#endif /* defined( USB_DEVICE ) */

#if defined( USB_HOST )
typedef enum
{
  H_PORT_DISCONNECTED         = 0,
  H_PORT_CONNECTED_DEBOUNCING = 1,
  H_PORT_CONNECTED_RESETTING  = 2,
  H_PORT_CONNECTED            = 3
} USBH_PortState_TypeDef;

typedef struct
{
  int   debounceTime;
  int   resetTime;
} USBH_AttachTiming_TypeDef;

typedef struct
{
  uint8_t                 *buf;
  int                     errorCnt;
  uint32_t                remaining;
  uint32_t                xferred;
#if defined( USB_SLAVEMODE )
  uint32_t                pending;
#else
  uint32_t                hwXferSize;
#endif
  uint8_t                 status;
  bool                    idle;
#if defined( USB_SLAVEMODE )
  bool                    txNpFempIntOn;
  bool                    txPFempIntOn;
#endif
  USBH_Ep_TypeDef         *ep;
} USBH_Hc_TypeDef;
#endif /* defined( USB_HOST ) */

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* defined( USB_DEVICE ) || defined( USB_HOST ) */
#endif /* defined( USB_PRESENT ) && ( USB_COUNT == 1 ) */
#endif /* __EFM32_USBTYPES_H */
