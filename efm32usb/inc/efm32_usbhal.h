/***************************************************************************//**
 * @file
 * @brief USB protocol stack library, low level USB peripheral access.
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
#ifndef __EFM32_USBHAL_H
#define __EFM32_USBHAL_H

#include "efm32.h"
#if defined( USB_PRESENT ) && ( USB_COUNT == 1 )
#include "efm32_usb.h"
#if defined( USB_DEVICE ) || defined( USB_HOST )

#ifdef __cplusplus
extern "C" {
#endif

/** @cond DO_NOT_INCLUDE_WITH_DOXYGEN */

#define USB_PID_DATA0         0
#define USB_PID_DATA2         1
#define USB_PID_DATA1         2
#define USB_PID_SETUP         3

#define HPRT_F_SPEED          ( 1 << _USB_HPRT_PRTSPD_SHIFT )
#define HPRT_L_SPEED          ( 2 << _USB_HPRT_PRTSPD_SHIFT )
#define HCFG_PHYCLK_48MHZ     1
#define HCFG_PHYCLK_6MHZ      2

#define DOEP0_XFERSIZE_PKTCNT_MASK ( _USB_DOEP0TSIZ_XFERSIZE_MASK | \
                                     _USB_DOEP0TSIZ_PKTCNT_MASK   )
#define DOEP_XFERSIZE_PKTCNT_MASK  ( _USB_DOEP_TSIZ_XFERSIZE_MASK | \
                                     _USB_DOEP_TSIZ_PKTCNT_MASK   )

#define DIEP0_XFERSIZE_PKTCNT_MASK ( _USB_DIEP0TSIZ_XFERSIZE_MASK | \
                                     _USB_DIEP0TSIZ_PKTCNT_MASK   )
#define DIEP_XFERSIZE_PKTCNT_MASK  ( _USB_DIEP_TSIZ_XFERSIZE_MASK | \
                                     _USB_DIEP_TSIZ_PKTCNT_MASK   | \
                                     _USB_DIEP_TSIZ_MC_MASK       )

#define DIEPCTL_EPTYPE_CONTROL (0 << _USB_DIEP_CTL_EPTYPE_SHIFT )
#define DIEPCTL_EPTYPE_ISOC    (1 << _USB_DIEP_CTL_EPTYPE_SHIFT )
#define DIEPCTL_EPTYPE_BULK    (2 << _USB_DIEP_CTL_EPTYPE_SHIFT )
#define DIEPCTL_EPTYPE_INTR    (3 << _USB_DIEP_CTL_EPTYPE_SHIFT )

#define HCCHAR_EPTYPE_CTRL     (0 << _USB_HC_CHAR_EPTYPE_SHIFT )
#define HCCHAR_EPTYPE_ISOC     (1 << _USB_HC_CHAR_EPTYPE_SHIFT )
#define HCCHAR_EPTYPE_BULK     (2 << _USB_HC_CHAR_EPTYPE_SHIFT )
#define HCCHAR_EPTYPE_INTR     (3 << _USB_HC_CHAR_EPTYPE_SHIFT )

#define GRXSTSP_PKTSTS_DEVICE_GOTNAK          ( 1 << _USB_GRXSTSP_PKTSTS_SHIFT )
#define GRXSTSP_PKTSTS_DEVICE_DATAOUTRECEIVED ( 2 << _USB_GRXSTSP_PKTSTS_SHIFT )
#define GRXSTSP_PKTSTS_DEVICE_DATAOUTCOMPLETE ( 3 << _USB_GRXSTSP_PKTSTS_SHIFT )
#define GRXSTSP_PKTSTS_DEVICE_SETUPCOMPLETE   ( 4 << _USB_GRXSTSP_PKTSTS_SHIFT )
#define GRXSTSP_PKTSTS_DEVICE_SETUPRECEIVED   ( 6 << _USB_GRXSTSP_PKTSTS_SHIFT )

#define GRXSTSP_PKTSTS_HOST_DATAINRECEIVED  ( 2 << _USB_GRXSTSP_PKTSTS_SHIFT )
#define GRXSTSP_PKTSTS_HOST_DATAINCOMPLETE  ( 3 << _USB_GRXSTSP_PKTSTS_SHIFT )
#define GRXSTSP_PKTSTS_HOST_DATATOGGLEERROR ( 5 << _USB_GRXSTSP_PKTSTS_SHIFT )
#define GRXSTSP_PKTSTS_HOST_CHANNELHALTED   ( 7 << _USB_GRXSTSP_PKTSTS_SHIFT )

#define DCTL_WO_BITMASK \
          ( _USB_DCTL_CGOUTNAK_MASK  | _USB_DCTL_SGOUTNAK_MASK   | \
            _USB_DCTL_CGNPINNAK_MASK | _USB_DCTL_SGNPINNAK_MASK  )
#define GUSBCFG_WO_BITMASK ( USB_GUSBCFG_CORRUPTTXPKT )
#define DEPCTL_WO_BITMASK \
          ( USB_DIEP_CTL_CNAK       | USB_DIEP_CTL_SNAK        | \
            USB_DIEP_CTL_SETD0PIDEF | USB_DIEP_CTL_SETD1PIDOF  )

#define HPRT_WC_MASK ( USB_HPRT_PRTCONNDET | USB_HPRT_PRTENA | \
                       USB_HPRT_PRTENCHNG  | USB_HPRT_PRTOVRCURRCHNG )

typedef __IO uint32_t USB_FIFO_TypeDef[ 0x1000 / sizeof( uint32_t ) ];
typedef __IO uint32_t USB_DIEPTXF_TypeDef;

#define USB_DINEPS       ((USB_DIEP_TypeDef    *) &USB->DIEP0CTL )
#define USB_DOUTEPS      ((USB_DOEP_TypeDef    *) &USB->DOEP0CTL )
#define USB_FIFOS        ((USB_FIFO_TypeDef    *) &USB->FIFO0D   )
#define USB_DIEPTXFS     ((USB_DIEPTXF_TypeDef *) &USB->DIEPTXF1 )

void USBHAL_CoreReset( void );

#if defined( USB_DEVICE )
void USBDHAL_AbortAllTransfers( USB_Status_TypeDef reason );
USB_Status_TypeDef USBDHAL_CoreInit( const uint32_t totalRxFifoSize,
                                     const uint32_t totalTxFifoSize );
void USBDHAL_Connect( void );
void USBDHAL_Disconnect( void );
void USBDHAL_AbortAllEps( void );
void USBDHAL_AbortEpIn( USBD_Ep_TypeDef *ep );
void USBDHAL_AbortEpOut( USBD_Ep_TypeDef *ep );

static __INLINE USB_Status_TypeDef USBDHAL_GetStallStatusEp(
                                   USBD_Ep_TypeDef *ep, uint16_t *halt );
static __INLINE uint32_t USBDHAL_GetInEpInts( USBD_Ep_TypeDef *ep );
static __INLINE uint32_t USBDHAL_GetOutEpInts( USBD_Ep_TypeDef *ep );
static __INLINE void USBDHAL_SetEPDISNAK( USBD_Ep_TypeDef *ep );
#endif /* defined( USB_DEVICE ) */

#if defined( USB_HOST )
USB_Status_TypeDef USBHHAL_CoreInit( const uint32_t rxFifoSize,
                                     const uint32_t nptxFifoSize,
                                     const uint32_t ptxFifoSize );
void USBHHAL_HCHalt(  int hcnum, uint32_t hcchar, uint32_t eptype );
void USBHHAL_HCInit(  int hcnum );
void USBHHAL_HCStart( int hcnum );
#endif /* defined( USB_HOST ) */

static __INLINE void USBHAL_DisableGlobalInt( void )
{
  USB->GAHBCFG &= ~USB_GAHBCFG_GLBLINTRMSK;
}

static __INLINE void USBHAL_EnableGlobalInt( void )
{
  USB->GAHBCFG |= USB_GAHBCFG_GLBLINTRMSK;
}

static __INLINE void USBHAL_FlushRxFifo( void )
{
  USB->GRSTCTL = USB_GRSTCTL_RXFFLSH;
  while ( USB->GRSTCTL & USB_GRSTCTL_RXFFLSH ) {}
}

static __INLINE void USBHAL_FlushTxFifo( uint8_t fifoNum )
{
  USB->GRSTCTL = USB_GRSTCTL_TXFFLSH | ( fifoNum << _USB_GRSTCTL_TXFNUM_SHIFT );
  while ( USB->GRSTCTL & USB_GRSTCTL_TXFFLSH ) {}
}

static __INLINE uint32_t USBHAL_GetCoreInts( void )
{
  uint32_t retVal;

  retVal  = USB->GINTSTS;
  retVal &= USB->GINTMSK;

  return retVal;
}

static __INLINE void USBHAL_ReadFifo( uint8_t *dest, uint16_t byteCount )
{
#if 0
  int words, i;
#else
  int words, loops;
#endif

  uint32_t *data;
  __IO uint32_t *fifo;

  words = (byteCount + 3) / 4;
  data  = (uint32_t*)dest;
  fifo  = USB->FIFO0D;

#if 0
  for ( i=0; i<words; i++ )
  {
    *data++ = *fifo;
  }
#else
  /* Duff's Device ! */
  loops = ( words + 7 ) / 8;
  switch( words % 8)
  {
    case 0: do
            {
              *data++ = *fifo;
    case 7:   *data++ = *fifo;
    case 6:   *data++ = *fifo;
    case 5:   *data++ = *fifo;
    case 4:   *data++ = *fifo;
    case 3:   *data++ = *fifo;
    case 2:   *data++ = *fifo;
    case 1:   *data++ = *fifo;
            } while ( --loops );
  }
#endif
}

static __INLINE void USBHAL_FlushFifo( uint16_t byteCount )
{
  int words, loops;
  __IO uint32_t *fifo;

  words = (byteCount + 3) / 4;
  fifo  = USB->FIFO0D;

  /* Duff's Device ! */
  loops = ( words + 7 ) / 8;
  switch( words % 8)
  {
    case 0: do
            {
              *fifo;
    case 7:   *fifo;
    case 6:   *fifo;
    case 5:   *fifo;
    case 4:   *fifo;
    case 3:   *fifo;
    case 2:   *fifo;
    case 1:   *fifo;
            } while ( --loops );
  }
}

static __INLINE void USBHAL_WriteFifo( int epnum, uint8_t *src,
                                        uint16_t byteCount )
{
#if 0
  int words, i;
#else
  int words, loops;
#endif

  uint32_t *data;
  __IO uint32_t *fifo;

  words = (byteCount + 3) / 4;
  data  = (uint32_t*)src;
  fifo  = USB_FIFOS[ epnum ];

#if 0
  for ( i=0; i<words; i++ )
  {
    *fifo = *data++;
  }
#else
  /* Duff's Device ! */
  loops = ( words + 7 ) / 8;
  switch( words % 8)
  {
    case 0: do
            {
              *fifo = *data++;
    case 7:   *fifo = *data++;
    case 6:   *fifo = *data++;
    case 5:   *fifo = *data++;
    case 4:   *fifo = *data++;
    case 3:   *fifo = *data++;
    case 2:   *fifo = *data++;
    case 1:   *fifo = *data++;
            } while ( --loops );
  }
#endif
}

#if defined( USB_DEVICE )
static __INLINE void USBDHAL_ActivateEp( USBD_Ep_TypeDef *ep, bool forceIdle )
{
#define DIEP_MPS_EPTYPE_TXFNUM_MASK ( _USB_DIEP_CTL_MPS_MASK    | \
                                      _USB_DIEP_CTL_EPTYPE_MASK | \
                                      _USB_DIEP_CTL_TXFNUM_MASK )
#define DOEP_MPS_EPTYPE_MASK        ( _USB_DOEP_CTL_MPS_MASK    | \
                                      _USB_DOEP_CTL_EPTYPE_MASK )
  uint32_t daintmask, depctl;

  if ( forceIdle )
    ep->state = D_EP_IDLE;

  if ( ep->in )
  {
    daintmask = ep->mask;
    depctl = USB_DINEPS[ ep->num ].CTL & ~DEPCTL_WO_BITMASK;

    if ( !( depctl & USB_DIEP_CTL_USBACTEP ) )
    {
      depctl = ( depctl                                     &
                 ~( DIEP_MPS_EPTYPE_TXFNUM_MASK          |
                    USB_DIEP_CTL_STALL                     )  ) |
               ( ep->packetSize << _USB_DIEP_CTL_MPS_SHIFT    ) |
               ( ep->type       << _USB_DIEP_CTL_EPTYPE_SHIFT ) |
               ( ep->txFifoNum  << _USB_DIEP_CTL_TXFNUM_SHIFT ) |
               USB_DIEP_CTL_SETD0PIDEF                          |
               USB_DIEP_CTL_USBACTEP                            |
               USB_DIEP_CTL_SNAK;
    }
    else
    {
      depctl |= USB_DIEP_CTL_SETD0PIDEF;
    }
    USB_DINEPS[ ep->num ].CTL = depctl;
  }
  else
  {
    daintmask = ep->mask << _USB_DAINTMSK_OUTEPMSK0_SHIFT;
    depctl = USB_DOUTEPS[ ep->num ].CTL & ~DEPCTL_WO_BITMASK;

    if ( !( depctl & USB_DOEP_CTL_USBACTEP ) )
    {
      depctl = ( depctl                                     &
                 ~( DOEP_MPS_EPTYPE_MASK                 |
                    USB_DOEP_CTL_STALL                     )  ) |
               ( ep->packetSize << _USB_DOEP_CTL_MPS_SHIFT    ) |
               ( ep->type       << _USB_DOEP_CTL_EPTYPE_SHIFT ) |
               USB_DOEP_CTL_SETD0PIDEF                          |
               USB_DOEP_CTL_USBACTEP                            |
               USB_DOEP_CTL_SNAK;
    }
    else
    {
      depctl |= USB_DOEP_CTL_SETD0PIDEF;
    }
    USB_DOUTEPS[ ep->num ].CTL = depctl;
  }

  /* Enable interrupt for this EP */
  USB->DAINTMSK |= daintmask;

#undef DIEP_MPS_EPTYPE_TXFNUM_MASK
#undef DOEP_MPS_EPTYPE_MASK
}

static __INLINE void USBDHAL_ClearRemoteWakeup( void )
{
  USB->DCTL &= ~( DCTL_WO_BITMASK | USB_DCTL_RMTWKUPSIG );
}

static __INLINE void USBDHAL_DeactivateEp( USBD_Ep_TypeDef *ep )
{
  uint32_t daintmask;

  if ( ep->in )
  {
    USB_DINEPS[ ep->num ].CTL = 0;
    daintmask = ep->mask;
  }
  else
  {
    USB_DOUTEPS[ ep->num ].CTL = 0;
    daintmask = ep->mask << _USB_DAINTMSK_OUTEPMSK0_SHIFT;
  }

  /* Disable interrupt for this EP */
  USB->DAINTMSK &= ~daintmask;
}

static __INLINE void USBDHAL_EnableInts( USBD_Device_TypeDef *dev )
{
  uint32_t mask;

  /* Disable all interrupts. */
  USB->GINTMSK = 0;

  /* Clear pending interrupts */
  USB->GINTSTS = 0xFFFFFFFF;

  mask = USB_GINTMSK_USBSUSPMSK     |
#if defined( USB_SLAVEMODE )
         USB_GINTMSK_RXFLVLMSK      |
#endif
         USB_GINTMSK_USBRSTMSK      |
         USB_GINTMSK_ENUMDONEMSK    |
         USB_GINTMSK_IEPINTMSK      |
         USB_GINTMSK_OEPINTMSK      |
         USB_GINTMSK_WKUPINTMSK     |
         USB_GINTMSK_SESSREQINTMSK;

  if ( dev->callbacks->sofInt )
  {
    mask |= USB_GINTMSK_SOFMSK;
  }

  USB->GINTMSK = mask;
}

static __INLINE void USBDHAL_EnableUsbResetInt( void )
{
  /* Disable all interrupts. */
  USB->GINTMSK = 0;

  /* Clear pending interrupts */
  USB->GINTSTS = 0xFFFFFFFF;

  USB->GINTMSK = USB_GINTMSK_USBRSTMSK | USB_GINTMSK_SESSREQINTMSK;
}

static __INLINE void USBDHAL_Ep0Activate( void )
{
  USB->DCTL = ( USB->DCTL & ~DCTL_WO_BITMASK ) | USB_DCTL_CGNPINNAK;

#if !defined( USB_SLAVEMODE )
  USB->DOEP0CTL = ( USB->DOEP0CTL & ~DEPCTL_WO_BITMASK ) |
                  USB_DOEP_CTL_CNAK | USB_DOEP_CTL_EPENA;
#endif
}

static __INLINE bool USBDHAL_EpIsStalled( USBD_Ep_TypeDef *ep )
{
  bool retVal = false;
  uint16_t stallStatus;

  if ( USBDHAL_GetStallStatusEp( ep, &stallStatus ) == USB_STATUS_OK )
  {
    retVal = stallStatus & 1 ? true : false;
  }
  return retVal;
}

#if defined( USB_SLAVEMODE )
static __INLINE void USBDHAL_FillFifo( USBD_Ep_TypeDef *ep )
{
  uint32_t len, words, fifoSpace;

  for( ;; )
  {
    len = EFM32_MIN( ep->remaining, ep->packetSize );
    words = (len + 3) / 4;
    fifoSpace = USB_DINEPS[ ep->num ].TXFSTS & _USB_DIEP_TXFSTS_SPCAVAIL_MASK;

    if  ( ( fifoSpace >= words ) && len )
    {
      USBHAL_WriteFifo( ep->num, ep->buf, len );
      ep->xferred   += len;
      ep->remaining -= len;
      ep->buf       += len;
      if ( ep->num == 0 )
      {
        break;
      }
    }
    else
    {
      break;
    }
  }

  if ( len == 0 )
  {
    /* Disable Tx FIFO empty interrupt */
    USB->DIEPEMPMSK &= ~ep->mask;
  }
}
#endif

static __INLINE uint32_t USBDHAL_GetAllInEpInts( void )
{
  uint32_t retVal;

  retVal  = USB->DAINT;
  retVal &= USB->DAINTMSK;
  return retVal & 0xFFFF;
}

static __INLINE uint32_t USBDHAL_GetAllOutEpInts( void )
{
  uint32_t retVal;

  retVal  = USB->DAINT;
  retVal &= USB->DAINTMSK;
  return retVal >> 16;
}

static __INLINE uint32_t USBDHAL_GetInEpInts( USBD_Ep_TypeDef *ep )
{
  uint32_t retVal, msk;
#if defined( USB_SLAVEMODE )
  uint32_t emp;
#endif

  msk  = USB->DIEPMSK;
#if defined( USB_SLAVEMODE )
  emp  = USB->DIEPEMPMSK;
  msk |= ((emp >> ep->num) & 0x1) << 7;
#endif
  retVal = USB_DINEPS[ ep->num ].INT;

  return retVal & msk;
}

static __INLINE uint32_t USBDHAL_GetOutEpInts( USBD_Ep_TypeDef *ep )
{
  uint32_t retVal;

  retVal  = USB_DOUTEPS[ ep->num ].INT;
  retVal &= USB->DOEPMSK;
  return retVal;
}

static __INLINE USB_Status_TypeDef USBDHAL_GetStallStatusEp(
                                   USBD_Ep_TypeDef *ep, uint16_t *halt )
{
  uint32_t depctl, eptype;
  USB_Status_TypeDef retVal = USB_STATUS_REQ_ERR;

  if ( ep->in == true )
  {
    depctl = USB_DINEPS[ ep->num ].CTL;
    eptype = depctl & _USB_DIEP_CTL_EPTYPE_MASK;

    if (( eptype == DIEPCTL_EPTYPE_INTR ) || ( eptype == DIEPCTL_EPTYPE_BULK ))
    {
      *halt = depctl & USB_DIEP_CTL_STALL ? 1 : 0;
      retVal = USB_STATUS_OK;
    }
  }
  else
  {
    depctl = USB_DOUTEPS[ ep->num ].CTL;
    eptype = depctl & _USB_DOEP_CTL_EPTYPE_MASK;

    if (( eptype == DIEPCTL_EPTYPE_INTR ) || ( eptype == DIEPCTL_EPTYPE_BULK ))
    {
      *halt = depctl & USB_DOEP_CTL_STALL ? 1 : 0;
      retVal = USB_STATUS_OK;
    }
  }

  return retVal;
}

static __INLINE void USBDHAL_ReenableEp0Setup( USBD_Device_TypeDef *dev )

{
#if !defined( USB_SLAVEMODE )
  USB->DOEP0DMAADDR = (uint32_t)dev->setupPkt;
#else
  (void)dev;
#endif

  USB->DOEP0CTL = ( USB->DOEP0CTL & ~DEPCTL_WO_BITMASK ) | USB_DOEP_CTL_EPENA;
}

static __INLINE void USBDHAL_SetAddr( uint8_t addr )
{
  USB->DCFG = ( USB->DCFG                    &
                ~_USB_DCFG_DEVADDR_MASK        ) |
              (addr << _USB_DCFG_DEVADDR_SHIFT );
}

#if !defined( USB_SLAVEMODE )
static __INLINE void USBDHAL_SetEp0InDmaPtr( uint8_t* addr )
{
  USB->DIEP0DMAADDR = (uint32_t)addr;
}

static __INLINE void USBDHAL_SetEp0OutDmaPtr( uint8_t* addr )
{
  USB->DOEP0DMAADDR = (uint32_t)addr;
}
#endif

static __INLINE void USBDHAL_SetEPDISNAK( USBD_Ep_TypeDef *ep )
{
  if ( ep->in )
  {
    USB_DINEPS[ ep->num ].CTL = ( USB_DINEPS[ ep->num ].CTL   &
                                  ~DEPCTL_WO_BITMASK            ) |
                                USB_DIEP_CTL_SNAK                 |
                                USB_DIEP_CTL_EPDIS;
  }
  else
  {
    USB_DOUTEPS[ ep->num ].CTL = ( USB_DOUTEPS[ ep->num ].CTL   &
                                   ~DEPCTL_WO_BITMASK             ) |
                                 USB_DOEP_CTL_EPENA;

    USB_DOUTEPS[ ep->num ].CTL = ( USB_DOUTEPS[ ep->num ].CTL   &
                                   ~DEPCTL_WO_BITMASK             ) |
                                 USB_DOEP_CTL_SNAK                  |
                                 USB_DOEP_CTL_EPDIS;
  }
}

static __INLINE void USBDHAL_SetRemoteWakeup( void )
{
  USB->DCTL = ( USB->DCTL & ~DCTL_WO_BITMASK ) | USB_DCTL_RMTWKUPSIG;
}

static __INLINE USB_Status_TypeDef USBDHAL_StallEp( USBD_Ep_TypeDef *ep )
{
  uint32_t depctl, eptype;
  USB_Status_TypeDef retVal = USB_STATUS_REQ_ERR;

  if ( ep->in == true )
  {
    depctl = USB_DINEPS[ ep->num ].CTL & ~DEPCTL_WO_BITMASK;
    eptype = depctl & _USB_DIEP_CTL_EPTYPE_MASK;

    if ( eptype != DIEPCTL_EPTYPE_ISOC )
    {
      if ( depctl & USB_DIEP_CTL_EPENA )
      {
        depctl |= USB_DIEP_CTL_EPDIS;
      }
      USB_DINEPS[ ep->num ].CTL = depctl | USB_DIEP_CTL_STALL;
      retVal = USB_STATUS_OK;
    }
  }
  else
  {
    depctl = USB_DOUTEPS[ ep->num ].CTL & ~DEPCTL_WO_BITMASK;
    eptype = depctl & _USB_DOEP_CTL_EPTYPE_MASK;

    if (( eptype == DIEPCTL_EPTYPE_INTR ) || ( eptype == DIEPCTL_EPTYPE_BULK ))
    {
      USB_DOUTEPS[ ep->num ].CTL = depctl | USB_DOEP_CTL_STALL;
      retVal = USB_STATUS_OK;
    }
  }

  return retVal;
}

static __INLINE void USBDHAL_StartEp0In( uint32_t len )
{
  USB->DIEP0TSIZ = ( len << _USB_DIEP0TSIZ_XFERSIZE_SHIFT   ) |
                   ( 1   << _USB_DIEP0TSIZ_PKTCNT_SHIFT     );

  USB->DIEP0CTL = ( USB->DIEP0CTL & ~DEPCTL_WO_BITMASK ) |
                  USB_DIEP_CTL_CNAK | USB_DIEP_CTL_EPENA;

#if defined( USB_SLAVEMODE )
  if ( len )        /* Enable Tx FIFO Empty Interrupt */
  {
    USB->DIEPEMPMSK |= 1;
  }
#endif
}

static __INLINE void USBDHAL_StartEp0Out( uint32_t len )
{
  USB->DOEP0TSIZ = ( len << _USB_DOEP0TSIZ_XFERSIZE_SHIFT   ) |
                   ( 1   << _USB_DOEP0TSIZ_PKTCNT_SHIFT     );

  USB->DOEP0CTL = ( USB->DOEP0CTL & ~DEPCTL_WO_BITMASK ) |
                  USB_DOEP_CTL_CNAK | USB_DOEP_CTL_EPENA;
}

static __INLINE void USBDHAL_StartEp0Setup( USBD_Device_TypeDef *dev )
{
  dev->ep[ 0 ].in = false;

  USB->DOEP0TSIZ = 3 << _USB_DOEP0TSIZ_SUPCNT_SHIFT;

#if !defined( USB_SLAVEMODE )
  dev->setup = dev->setupPkt;
  USB->DOEP0DMAADDR = (uint32_t)dev->setup;

  USB->DOEP0CTL = ( USB->DOEP0CTL & ~DEPCTL_WO_BITMASK ) |
                  USB_DOEP_CTL_CNAK | USB_DOEP_CTL_EPENA;
#endif
}

static __INLINE void USBDHAL_StartEpIn( USBD_Ep_TypeDef *ep )
{
  uint32_t pktcnt, xfersize;

  if ( ep->remaining == 0 )     /* ZLP ? */
  {
    pktcnt = 1;
    xfersize = 0;
  }
  else
  {
    pktcnt = ( ep->remaining - 1 + ep->packetSize ) / ep->packetSize;
    xfersize = ep->remaining;
  }

  USB_DINEPS[ ep->num ].TSIZ =
                ( USB_DINEPS[ ep->num ].TSIZ                &
                  ~DIEP_XFERSIZE_PKTCNT_MASK                  ) |
                ( xfersize << _USB_DIEP_TSIZ_XFERSIZE_SHIFT   ) |
                ( pktcnt   << _USB_DIEP_TSIZ_PKTCNT_SHIFT     );

#if defined( USB_SLAVEMODE )
  if ( ep->remaining )              /* Enable Tx FIFO Empty Interrupt */
  {
    USB->DIEPEMPMSK |= ep->mask;
  }
#else
  USB_DINEPS[ ep->num ].DMAADDR = (uint32_t)ep->buf;
#endif

  USB_DINEPS[ ep->num ].CTL =
                  ( USB_DINEPS[ ep->num ].CTL & ~DEPCTL_WO_BITMASK ) |
                  USB_DIEP_CTL_CNAK                                  |
                  USB_DIEP_CTL_EPENA;
}

static __INLINE void USBDHAL_StartEpOut( USBD_Ep_TypeDef *ep )
{
  uint32_t pktcnt, xfersize;

  if ( ep->remaining == 0 )     /* ZLP ? */
  {
    pktcnt = 1;
    xfersize = ep->packetSize;
  }
  else
  {
    pktcnt = ( ep->remaining - 1 + ep->packetSize ) / ep->packetSize;
    xfersize = pktcnt * ep->packetSize;
  }

  USB_DOUTEPS[ ep->num ].TSIZ =
                  ( USB_DOUTEPS[ ep->num ].TSIZ               &
                    ~DOEP_XFERSIZE_PKTCNT_MASK                  ) |
                  ( xfersize << _USB_DOEP_TSIZ_XFERSIZE_SHIFT   ) |
                  ( pktcnt   << _USB_DOEP_TSIZ_PKTCNT_SHIFT     );

#if !defined( USB_SLAVEMODE )
  ep->hwXferSize = xfersize;
  USB_DOUTEPS[ ep->num ].DMAADDR = (uint32_t)ep->buf;
#endif

  USB_DOUTEPS[ ep->num ].CTL =
                          ( USB_DOUTEPS[ ep->num ].CTL  &
                            ~DEPCTL_WO_BITMASK             ) |
                          USB_DOEP_CTL_CNAK                  |
                          USB_DOEP_CTL_EPENA;
}

static __INLINE USB_Status_TypeDef USBDHAL_UnStallEp( USBD_Ep_TypeDef *ep )
{
  uint32_t depctl, eptype;
  USB_Status_TypeDef retVal = USB_STATUS_REQ_ERR;

  if ( ep->in == true )
  {
    depctl = USB_DINEPS[ ep->num ].CTL & ~DEPCTL_WO_BITMASK;
    eptype = depctl & _USB_DIEP_CTL_EPTYPE_MASK;

    if (( eptype == DIEPCTL_EPTYPE_INTR ) || ( eptype == DIEPCTL_EPTYPE_BULK ))
    {
      depctl |=  USB_DIEP_CTL_SETD0PIDEF;
      depctl &= ~USB_DIEP_CTL_STALL;
      USB_DINEPS[ ep->num ].CTL = depctl;
      retVal = USB_STATUS_OK;
    }
  }
  else
  {
    depctl = USB_DOUTEPS[ ep->num ].CTL & ~DEPCTL_WO_BITMASK;
    eptype = depctl & _USB_DOEP_CTL_EPTYPE_MASK;

    if (( eptype == DIEPCTL_EPTYPE_INTR ) || ( eptype == DIEPCTL_EPTYPE_BULK ))
    {
      depctl |=  USB_DOEP_CTL_SETD0PIDEF;
      depctl &= ~USB_DOEP_CTL_STALL;
      USB_DOUTEPS[ ep->num ].CTL = depctl;
      retVal = USB_STATUS_OK;
    }
  }

  return retVal;
}
#endif /* defined( USB_DEVICE ) */

#if defined( USB_HOST )
static __INLINE void USBHHAL_HCActivate( int hcnum, uint32_t hcchar, bool intep )
{
  uint32_t oddframe;

  if ( intep )
  {
    oddframe = USB->HFNUM & 1;

    USB->HC[ hcnum ].CHAR =
                    ( hcchar &
                      ~( USB_HC_CHAR_CHDIS | _USB_HC_CHAR_ODDFRM_MASK ) ) |

                    /* Schedule INT transfers to start in next frame. */
                    ( oddframe & 1 ? 0 : USB_HC_CHAR_ODDFRM             ) |

                    USB_HC_CHAR_CHENA;
  }
  else
  {
    USB->HC[ hcnum ].CHAR = ( hcchar & ~USB_HC_CHAR_CHDIS ) |
                              USB_HC_CHAR_CHENA;
  }
}

static __INLINE bool USBHHAL_InitializedAndPowered( void )
{
  if ( ( USB->ROUTE & USB_ROUTE_PHYPEN ) &&
       ( USB->HPRT  & USB_HPRT_PRTPWR  )    )
    return true;
  return false;
}

static __INLINE void USBHHAL_EnableInts( void )
{
  /* Disable all interrupts. */
  USB->GINTMSK = 0;

  /* Clear pending OTG interrupts */
  USB->GOTGINT = 0xFFFFFFFF;

  /* Clear pending interrupts */
  USB->GINTSTS = 0xFFFFFFFF;

  USB->GINTMSK = USB_GINTMSK_PRTINTMSK     |
#if defined( USB_SLAVEMODE )
                 USB_GINTMSK_RXFLVLMSK     |
#endif
                 USB_GINTMSK_HCHINTMSK     |
                 USB_GINTMSK_DISCONNINTMSK;
}

#if defined( USB_SLAVEMODE )
static __INLINE void USBHHAL_FillFifo( uint8_t hcnum,
                                       uint32_t fifoStatus,
                                       USBH_Hc_TypeDef *hc )
{
  uint16_t words, len;

  if ( hc->remaining > 0 )
  {
    len   = EFM32_MIN( hc->remaining, hc->ep->packetSize );
    words = ( len + 3 ) / 4;

    if ( ( fifoStatus   & _USB_GNPTXSTS_NPTXQSPCAVAIL_MASK            ) &&
         ( ( fifoStatus & _USB_GNPTXSTS_NPTXFSPCAVAIL_MASK ) >= words )    )
    {
      hc->pending = len;
      USBHAL_WriteFifo( hcnum, hc->buf, len );
      if ( hc->ep->type == USB_EPTYPE_INTR )
      {
        hc->txPFempIntOn = false;
      }
      else
      {
        hc->txNpFempIntOn = false;
      }
    }
    else
    {
      /* Write to FIFO on "empty" int. */
      if ( hc->ep->type == USB_EPTYPE_INTR )
      {
        USB->GINTMSK |= USB_GINTSTS_PTXFEMP;
        hc->txPFempIntOn = true;
      }
      else
      {
        USB->GINTMSK |= USB_GINTSTS_NPTXFEMP; /* Write FIFO on "empty" int. */
        hc->txNpFempIntOn = true;
      }
    }
  }
}
#endif

static __INLINE uint16_t USBHHAL_GetFrameNum( void )
{
  return USB->HFNUM;
}

static __INLINE uint32_t USBHHAL_GetHcChar( uint8_t hcnum )
{
  return USB->HC[ hcnum ].CHAR;
}

static __INLINE uint32_t USBHHAL_GetHcInts( uint8_t hcnum )
{
  uint32_t retVal;

  retVal  = USB->HC[ hcnum ].INT;
#if defined( USB_SLAVEMODE )
  retVal &= USB->HC[ hcnum ].INTMSK;
#endif
  return retVal;
}

static __INLINE uint32_t USBHHAL_GetHostChannelInts( void )
{
  return USB->HAINT;
}

static __INLINE uint8_t USBHHAL_GetPortSpeed( void )
{
  return ( USB->HPRT & _USB_HPRT_PRTSPD_MASK ) >> _USB_HPRT_PRTSPD_SHIFT;
}

static __INLINE void USBHHAL_PortReset( bool on )
{
  if ( on )
  {
    DEBUG_USB_INT_LO_PUTCHAR( '+' );
    USB->HPRT = ( USB->HPRT & ~HPRT_WC_MASK ) | USB_HPRT_PRTRST;
  }
  else
  {
    DEBUG_USB_INT_LO_PUTCHAR( '-' );
    USB->HPRT &= ~( HPRT_WC_MASK | USB_HPRT_PRTRST );
  }
}

static __INLINE void USBHHAL_PortResume( bool on )
{
  if ( on )
  {
    USB->HPRT = ( USB->HPRT & ~( HPRT_WC_MASK | USB_HPRT_PRTSUSP ) ) |
                USB_HPRT_PRTRES;
  }
  else
  {
    USB->HPRT &= ~( HPRT_WC_MASK | USB_HPRT_PRTSUSP | USB_HPRT_PRTRES );
  }
}

static __INLINE void USBHHAL_PortSuspend( void )
{
  USB->HPRT = ( USB->HPRT & ~HPRT_WC_MASK ) | USB_HPRT_PRTSUSP;
}

static __INLINE void USBHHAL_VbusOn( bool on )
{
  if ( on )
  {
    USB->HPRT = ( USB->HPRT & ~HPRT_WC_MASK ) | USB_HPRT_PRTPWR;
    DEBUG_USB_INT_LO_PUTCHAR( '/' );
  }
  else
  {
    USB->HPRT &= ~( HPRT_WC_MASK | USB_HPRT_PRTPWR );
    DEBUG_USB_INT_LO_PUTCHAR( '\\' );
  }
}
#endif /* defined( USB_HOST ) */

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* defined( USB_DEVICE ) || defined( USB_HOST ) */
#endif /* defined( USB_PRESENT ) && ( USB_COUNT == 1 ) */
#endif /* __EFM32_USBHAL_H */
