/***************************************************************************//**
 * @file
 * @brief USB protocol stack library API for EFM32.
 * @brief USB protocol stack library, USB host peripheral interrupt handlers.
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
#include "efm32.h"
#if defined( USB_PRESENT ) && ( USB_COUNT == 1 )
#include "efm32_usb.h"
#if defined( USB_HOST )

#include "efm32_usbtypes.h"
#include "efm32_usbhal.h"
#include "efm32_usbh.h"

/** @cond DO_NOT_INCLUDE_WITH_DOXYGEN */

#define HANDLE_INT( x ) if ( status & x ) { Handle_##x(); status &= ~x; }

#define FIFO_TXSTS_HCNUM_MASK  0x78000000
#define FIFO_TXSTS_HCNUM_SHIFT 27

#if defined( USB_SLAVEMODE )
#define TOGGLE_DATA_PID()                       \
  if ( eptype != HCCHAR_EPTYPE_ISOC )           \
  {                                             \
    if ( hc->ep->toggle == USB_PID_DATA0 )      \
    {                                           \
      hc->ep->toggle = USB_PID_DATA1;           \
    }                                           \
    else if ( hc->ep->toggle == USB_PID_DATA1 ) \
    {                                           \
      hc->ep->toggle = USB_PID_DATA0;           \
    }                                           \
  }
#endif

static void Handle_HcInInt(  uint8_t hcnum );
static void Handle_HcOutInt( uint8_t hcnum );
static void Handle_USB_GINTSTS_DISCONNINT ( void );
static void Handle_USB_GINTSTS_HCHINT     ( void );
static void Handle_USB_GINTSTS_PRTINT     ( void );
#if defined( USB_SLAVEMODE )
static void Handle_USB_GINTSTS_NPTXFEMP   ( void );
static void Handle_USB_GINTSTS_PTXFEMP    ( void );
static void Handle_USB_GINTSTS_RXFLVL     ( void );
#endif

/*
 * USB_IRQHandler() is the first level handler for the USB peripheral interrupt.
 */
void USB_IRQHandler( void )
{
  uint32_t status;

  INT_Disable();

  status = USBHAL_GetCoreInts();
  if ( status == 0 )
  {
    INT_Enable();
    DEBUG_USB_INT_LO_PUTS( "\nSinT" );
    return;
  }

#if defined( USB_SLAVEMODE )
  HANDLE_INT( USB_GINTSTS_RXFLVL     )
  HANDLE_INT( USB_GINTSTS_NPTXFEMP   )
  HANDLE_INT( USB_GINTSTS_PTXFEMP    )
#endif
  HANDLE_INT( USB_GINTSTS_HCHINT     )
  HANDLE_INT( USB_GINTSTS_PRTINT     )
  HANDLE_INT( USB_GINTSTS_DISCONNINT )

  INT_Enable();

  if ( status != 0 )
  {
    DEBUG_USB_INT_LO_PUTS( "\nUinT" );
  }
}

/*
 * Handle host channel IN transfer interrupt.
 */
static void Handle_HcInInt( uint8_t hcnum )
{
  USBH_Hc_TypeDef *hc;
  USB_Status_TypeDef result;
  uint32_t status, hcchar, eptype;
#ifdef DEBUG_USB_INT_HI
  uint32_t status2;
#endif

  hc = &hcs[ hcnum ];
  status = USBHHAL_GetHcInts( hcnum );
  hcchar = USB->HC[ hcnum ].CHAR;
  eptype = hcchar & _USB_HC_CHAR_EPTYPE_MASK;

  DEBUG_USB_INT_HI_PUTCHAR( 'i' );

#if !defined( USB_SLAVEMODE )
  if ( status & USB_HC_INT_CHHLTD )
  {
    USB->HC[ hcnum ].INT = 0xFFFFFFFF;

#ifdef DEBUG_USB_INT_HI
    status2 = status;
#endif
    status &= USB_HC_INT_XFERCOMPL | USB_HC_INT_STALL | USB_HC_INT_XACTERR |
              USB_HC_INT_ACK | USB_HC_INT_NAK | USB_HC_INT_DATATGLERR |
              USB_HC_INT_BBLERR;

    if ( ( status & ( USB_HC_INT_ACK | USB_HC_INT_XFERCOMPL ) ) ==
         ( USB_HC_INT_ACK | USB_HC_INT_XFERCOMPL              )    )
    {
      DEBUG_USB_INT_HI_PUTCHAR( 'c' );

      hc->xferred = hc->hwXferSize -
                    ( ( USB->HC[ hcnum ].TSIZ & _USB_HC_TSIZ_XFERSIZE_MASK ) >>
                      _USB_HC_TSIZ_XFERSIZE_SHIFT );

      hc->remaining -= hc->xferred;
      hc->ep->toggle = ( USB->HC[ hcnum ].TSIZ & _USB_HC_TSIZ_PID_MASK ) ==
                       USB_HC_TSIZ_PID_DATA0 ? USB_PID_DATA0 : USB_PID_DATA1;

      result = USB_STATUS_OK;
    }

    else if ( status & USB_HC_INT_STALL )
    {
      result = USB_STATUS_EP_STALLED;
      DEBUG_USB_INT_LO_PUTS( "StaL" );
    }

    else if ( status & USB_HC_INT_BBLERR )
    {
      result = USB_STATUS_EP_ERROR;
      DEBUG_USB_INT_LO_PUTS( "BabL" );
    }

    else if ( ( ( status &
                  ( USB_HC_INT_DATATGLERR | USB_HC_INT_NAK ) )
                == USB_HC_INT_DATATGLERR                       ) &&
              ( !( hc->status & HCS_TIMEOUT )                  )    )
    {
      /* Toggle error but not nak or timeout */
      result = USB_STATUS_EP_ERROR;
      DEBUG_USB_INT_LO_PUTS( "TglE" );

      hc->errorCnt++;
      if ( hc->errorCnt < 3 )
      {
        USBHHAL_HCStart( hcnum );
        return;
      }
    }

    else if ( ( ( status &
                  ( USB_HC_INT_DATATGLERR | USB_HC_INT_NAK |
                    USB_HC_INT_XACTERR )                       )
                == USB_HC_INT_XACTERR                            ) &&
              ( !( hc->status & HCS_TIMEOUT )                    )    )
    {
      /* Exact error but not toggle err or nak or timeout */
      result = USB_STATUS_EP_ERROR;
      DEBUG_USB_INT_LO_PUTS( "XacT" );

      hc->errorCnt++;
      if ( hc->errorCnt < 3 )
      {
        USBHHAL_HCStart( hcnum );
        return;
      }
    }

    else if ( hc->status & HCS_TIMEOUT )
    {
      DEBUG_USB_INT_HI_PUTCHAR( 't' );
      result = USB_STATUS_TIMEOUT;
    }

    else
    {
#ifdef DEBUG_USB_INT_HI
      if ( !( ( eptype == HCCHAR_EPTYPE_INTR                  ) &&
              ( ( status & USB_HC_INT_NAK ) == USB_HC_INT_NAK )    ) )
        {
          USB_PRINTF( "0x%08X", status2 );
        }
#endif
      return;
    }

    if ( eptype == HCCHAR_EPTYPE_CTRL )
      USBHEP_CtrlEpHandler( hc->ep, result );
    else
      USBHEP_EpHandler( hc->ep, result );
  }
#else /* !defined( USB_SLAVEMODE ) */

  if ( status & USB_HC_INT_XFERCOMPL )
  {
    DEBUG_USB_INT_HI_PUTCHAR( 'c' );
    USB->HC[ hcnum ].INT = USB_HC_INT_XFERCOMPL;
    USBHHAL_HCHalt( hcnum, hcchar, eptype );

    hc->status |= HCS_COMPLETED;

    TOGGLE_DATA_PID()                         /* Update data toggle */
  }

  else if ( status & USB_HC_INT_XACTERR )
  {
    USB->HC[ hcnum ].INT = USB_HC_INT_XACTERR;
    USBHHAL_HCHalt( hcnum, hcchar, eptype );

    hc->status |= HCS_XACT;
    DEBUG_USB_INT_LO_PUTS( "XacT" );
  }

  else if ( status & USB_HC_INT_BBLERR )
  {
    USB->HC[ hcnum ].INT = USB_HC_INT_BBLERR;
    USBHHAL_HCHalt( hcnum, hcchar, eptype );

    hc->status |= HCS_BABBLE;
    DEBUG_USB_INT_LO_PUTS( "BabL" );
  }

  else if ( status & USB_HC_INT_STALL )
  {
    USB->HC[ hcnum ].INT = USB_HC_INT_STALL;
    USBHHAL_HCHalt( hcnum, hcchar, eptype );

    hc->status |= HCS_STALL;
    DEBUG_USB_INT_LO_PUTS( "StaL" );
  }

  else if ( status & USB_HC_INT_NAK )
  {
    DEBUG_USB_INT_HI_PUTCHAR( 'n' );
    USB->HC[ hcnum ].INT = USB_HC_INT_NAK;

    if ( ( eptype != HCCHAR_EPTYPE_INTR ) &&
         ( hc->status == 0              )    )
    {
      /* Re-activate the channel */
      USBHHAL_HCActivate( hcnum, hcchar, false );
    }
    else
    {
      USBHHAL_HCHalt( hcnum, hcchar, eptype );
      hc->status |= HCS_NAK;
    }
  }

  else if ( status & USB_HC_INT_DATATGLERR )
  {
    USB->HC[ hcnum ].INT = USB_HC_INT_DATATGLERR;
    USBHHAL_HCHalt( hcnum, hcchar, eptype );

    hc->status |= HCS_TGLERR;
  }

  else if ( status & USB_HC_INT_CHHLTD )
  {
    DEBUG_USB_INT_HI_PUTCHAR( 'h' );
    USB->HC[ hcnum ].INT = 0xFFFFFFFF;
    USB->HC[ hcnum ].INTMSK &= ~USB_HC_INT_CHHLTD;

    result = USB_STATUS_EP_ERROR;

    if ( hc->status & HCS_STALL )
    {
      result = USB_STATUS_EP_STALLED;
    }

    else if ( ( hc->status &
                ( HCS_NAK | HCS_STALL | HCS_TGLERR |
                  HCS_BABBLE | HCS_TIMEOUT           ) ) == HCS_TGLERR )
    {
      hc->errorCnt++;
      if ( hc->errorCnt < 3 )
      {
        hc->status |= HCS_RETRY;
      }
    }

    else if ( ( hc->status &
              ( HCS_TGLERR | HCS_TIMEOUT |
                HCS_XACT | HCS_BABBLE      ) ) == HCS_XACT )
    {
      hc->errorCnt++;
      if ( hc->errorCnt < 3 )
      {
        hc->status |= HCS_RETRY;
      }
    }

    else if ( ( hc->status & ( HCS_NAK | HCS_TIMEOUT ) ) == HCS_NAK )
    {
      if ( eptype == HCCHAR_EPTYPE_INTR )
        return;

      result = USB_STATUS_EP_NAK;
    }

    else if ( hc->status & ( HCS_TIMEOUT | HCS_NAK ) )
    {
      result = USB_STATUS_TIMEOUT;
    }

    else if ( hc->status == HCS_COMPLETED )
    {
      result = USB_STATUS_OK;
    }

    if ( hc->status & HCS_RETRY )
    {
      USBHHAL_HCStart( hcnum );
    }
    else
    {
      if ( eptype == HCCHAR_EPTYPE_CTRL )
        USBHEP_CtrlEpHandler( hc->ep, result );
      else
        USBHEP_EpHandler( hc->ep, result );
    }
  }
#endif /* !defined( USB_SLAVEMODE ) */
}

/*
 * Handle host channel OUT transfer interrupt.
 */
static void Handle_HcOutInt( uint8_t hcnum )
{
  USBH_Hc_TypeDef *hc;
  USB_Status_TypeDef result;
  uint32_t status, hcchar, eptype;
#ifdef DEBUG_USB_INT_HI
  uint32_t status2;
#endif

  hc = &hcs[ hcnum ];
  status = USBHHAL_GetHcInts( hcnum );
  hcchar = USB->HC[ hcnum ].CHAR;
  eptype = hcchar & _USB_HC_CHAR_EPTYPE_MASK;

  DEBUG_USB_INT_HI_PUTCHAR( 'o' );

#if !defined( USB_SLAVEMODE )
  if ( status & USB_HC_INT_CHHLTD )
  {
    USB->HC[ hcnum ].INT = 0xFFFFFFFF;

#ifdef DEBUG_USB_INT_HI
    status2 = status;
#endif
    status &= USB_HC_INT_XFERCOMPL | USB_HC_INT_STALL | USB_HC_INT_XACTERR |
              USB_HC_INT_ACK | USB_HC_INT_NAK;

    if ( ( status & ( USB_HC_INT_ACK | USB_HC_INT_XFERCOMPL ) ) ==
         ( USB_HC_INT_ACK | USB_HC_INT_XFERCOMPL              )    )
    {
      DEBUG_USB_INT_HI_PUTCHAR( 'c' );

      hc->xferred   = hc->remaining;
      hc->remaining = 0;
      hc->ep->toggle = ( USB->HC[ hcnum ].TSIZ & _USB_HC_TSIZ_PID_MASK ) ==
                       USB_HC_TSIZ_PID_DATA0 ? USB_PID_DATA0 : USB_PID_DATA1;

      result = USB_STATUS_OK;
    }

    else if ( status & USB_HC_INT_STALL )
    {
      result = USB_STATUS_EP_STALLED;
      DEBUG_USB_INT_LO_PUTS( "StaL" );
    }

    else if ( status & USB_HC_INT_XACTERR )
    {
      DEBUG_USB_INT_LO_PUTS( "XacT" );
      if ( status & ( USB_HC_INT_ACK | USB_HC_INT_NAK ) )
      {
        hc->errorCnt = 0;
        USBHHAL_HCStart( hcnum );
        return;
      }
      else
      {
        hc->errorCnt++;
        if ( hc->errorCnt < 3 )
        {
          USBHHAL_HCStart( hcnum );
          return;
        }
      }
      result = USB_STATUS_EP_ERROR;
    }

    else if ( hc->status & HCS_TIMEOUT )
    {
      DEBUG_USB_INT_HI_PUTCHAR( 't' );
      result = USB_STATUS_TIMEOUT;
    }

    else
    {
#ifdef DEBUG_USB_INT_HI
      if ( !( ( eptype == HCCHAR_EPTYPE_INTR                  ) &&
              ( ( status & USB_HC_INT_NAK ) == USB_HC_INT_NAK )    ) )
        {
          USB_PRINTF( "0x%08X", status2 );
        }
#endif
      return;
    }

    if ( eptype == HCCHAR_EPTYPE_CTRL )
      USBHEP_CtrlEpHandler( hc->ep, result );
    else
      USBHEP_EpHandler( hc->ep, result );
  }
#else /* #if !defined( USB_SLAVEMODE ) */

  if ( status & USB_HC_INT_XFERCOMPL )
  {
    DEBUG_USB_INT_HI_PUTCHAR( 'c' );
    USB->HC[ hcnum ].INT = USB_HC_INT_XFERCOMPL;
    USBHHAL_HCHalt( hcnum, hcchar, eptype );

    hc->status |= HCS_COMPLETED;
  }

  else if ( status & USB_HC_INT_STALL )
  {
    USB->HC[ hcnum ].INT = USB_HC_INT_STALL;
    USBHHAL_HCHalt( hcnum, hcchar, eptype );

    hc->status |= HCS_STALL;
    DEBUG_USB_INT_LO_PUTS( "StaL" );
  }

  else if ( status & USB_HC_INT_ACK )
  {
    DEBUG_USB_INT_HI_PUTCHAR( 'a' );
    USB->HC[ hcnum ].INT = USB_HC_INT_ACK;

    if ( hc->pending )
    {
      hc->buf       += hc->pending;
      hc->xferred   += hc->pending;
      hc->remaining -= hc->pending;
      hc->pending = 0;

      TOGGLE_DATA_PID()                              /* Update data toggle    */

      if ( eptype == HCCHAR_EPTYPE_INTR )
      {
        USBHHAL_FillFifo( hcnum, USB->HPTXSTS, hc ); /* There may be more data*/
      }
      else
      {
        USBHHAL_FillFifo( hcnum, USB->GNPTXSTS, hc );/* There may be more data*/
      }
    }
  }

  else if ( status & USB_HC_INT_NAK )
  {
    DEBUG_USB_INT_HI_PUTCHAR( 'n' );
    USB->HC[ hcnum ].INT = USB_HC_INT_NAK;
    USBHHAL_HCHalt( hcnum, hcchar, eptype );

    hc->status |= HCS_NAK;
  }

  else if ( status & USB_HC_INT_XACTERR )
  {
    USB->HC[ hcnum ].INT = USB_HC_INT_XACTERR;
    USBHHAL_HCHalt( hcnum, hcchar, eptype );

    hc->status |= HCS_XACT;
    DEBUG_USB_INT_LO_PUTS( "XacT" );
  }

  else if ( status & USB_HC_INT_CHHLTD )
  {
    DEBUG_USB_INT_HI_PUTCHAR( 'h' );
    USB->HC[ hcnum ].INT = 0xFFFFFFFF;
    USB->HC[ hcnum ].INTMSK &= ~USB_HC_INT_CHHLTD;

    result = USB_STATUS_EP_ERROR;

    if ( hc->status & HCS_STALL )
    {
      result = USB_STATUS_EP_STALLED;
    }

    else if ( hc->status == HCS_XACT )
    {
      hc->errorCnt++;
      if ( hc->errorCnt < 3 )
      {
        hc->status |= HCS_RETRY;
      }
    }

    else if ( ( hc->status & ( HCS_NAK | HCS_TIMEOUT ) ) == HCS_NAK )
    {
      if ( eptype == HCCHAR_EPTYPE_INTR )
        return;

      hc->status |= HCS_RETRY;
    }

    else if ( hc->status & HCS_TIMEOUT )
    {
      result = USB_STATUS_TIMEOUT;
    }

    else if ( hc->status == HCS_COMPLETED )
    {
      result = USB_STATUS_OK;
    }

    if ( hc->status & HCS_RETRY )
    {
      USBHHAL_HCStart( hcnum );
    }
    else
    {
      if ( eptype == HCCHAR_EPTYPE_CTRL )
        USBHEP_CtrlEpHandler( hc->ep, result );
      else
        USBHEP_EpHandler( hc->ep, result );
    }
  }
#endif /* #if !defined( USB_SLAVEMODE ) */
}

/*
 * Handle port disconnect interrupt.
 */
static void Handle_USB_GINTSTS_DISCONNINT( void )
{
  int i;
  uint32_t hcchar;

  USB->GINTSTS = USB_GINTSTS_DISCONNINT;
  USB->HAINTMSK = 0;

  USBH_PortStatus = H_PORT_DISCONNECTED;
  USBTIMER_Stop( HOSTPORT_TIMER_INDEX );
  USBHHAL_PortReset( false );

  for ( i=0; i< NUM_HC_USED + 2; i++ )
  {
    hcchar = USB->HC[ i ].CHAR;                 /* Halt channel             */
    USBHHAL_HCHalt( i, hcchar, hcchar & _USB_HC_CHAR_EPTYPE_MASK );
    USB->HC[ i ].INT = 0xFFFFFFFF;              /* Clear pending interrupts */

    if ( !hcs[ i ].idle )
    {
      USBHEP_TransferDone( hcs[ i ].ep, USB_STATUS_DEVICE_REMOVED );
    }
  }

  DEBUG_USB_INT_LO_PUTS( "\nDisC" );
}

/*
 * Handle host channel interrupt. Call IN and OUT transfer handlers as needed.
 */
static void Handle_USB_GINTSTS_HCHINT( void )
{
  uint8_t hcnum;
  uint32_t hcints, hcmask;

  hcints = USBHHAL_GetHostChannelInts();

  for ( hcnum = 0,               hcmask = 1;
        hcnum < NUM_HC_USED + 2;
        hcnum++,                 hcmask <<= 1 )
  {
    if ( hcints & hcmask )
    {
      if ( USB->HC[ hcnum ].CHAR & USB_HC_CHAR_EPDIR )
      {
        Handle_HcInInt( hcnum );
      }
      else
      {
        Handle_HcOutInt( hcnum );
      }
    }
  }
}

#if defined( USB_SLAVEMODE )
/*
 * Handle non periodic transmit FIFO empty interrupt.
 */
static void Handle_USB_GINTSTS_NPTXFEMP( void )
{
  int i;
  uint8_t hcnum;
  uint32_t fifoStatus;
  USBH_Hc_TypeDef *hc;

  fifoStatus = USB->GNPTXSTS;
  hcnum = ( fifoStatus & FIFO_TXSTS_HCNUM_MASK ) >> FIFO_TXSTS_HCNUM_SHIFT;
  hc = &hcs[ hcnum ];

  USBHHAL_FillFifo( hcnum, fifoStatus, hc );

  DEBUG_USB_INT_LO_PUTS( "NpE" );

  for ( i = 0; i < NUM_HC_USED + 2; i++ )
  {
    if ( hcs[ i ].txNpFempIntOn == true )
      return;                               /* TxFemp int must remain on */
  }

  USB->GINTMSK &= ~USB_GINTSTS_NPTXFEMP;    /* Turn off TxFemp interrupt */
}
#endif

/*
 * Callback function for port interrupt state machine.
 * Called on timeout of the port timer when a port reset is completed.
 */
static void PortResetComplete( void )
{
  if ( USB->HPRT & USB_HPRT_PRTCONNSTS ) /* Is device still connected ? */
  {
    DEBUG_USB_INT_LO_PUTCHAR( '5' );
  }
  else
  {
    USBH_PortStatus = H_PORT_DISCONNECTED;
  }
  USBHHAL_PortReset( false );
}

/*
 * Callback function for port interrupt state machine.
 * Called on timeout of the port timer connection debounce time has expired.
 */
static void PortDebounceComplete( void )
{
  uint32_t hprt;

  hprt = USB->HPRT;                   /* Get port status */

  if ( hprt & USB_HPRT_PRTCONNSTS )   /* Is device still connected ? */
  {
    if ( ( hprt & _USB_HPRT_PRTSPD_MASK ) == HPRT_L_SPEED )
    {
      DEBUG_USB_INT_LO_PUTCHAR( '3' );
      USB->HFIR = 6000;
      /* Set 6 MHz PHY clock */
      USB->HCFG = ( USB->HCFG & ~_USB_HCFG_FSLSPCLKSEL_MASK ) |
                  ( 2 << _USB_HCFG_FSLSPCLKSEL_SHIFT        );
    }
    else if ( ( hprt & _USB_HPRT_PRTSPD_MASK ) == HPRT_F_SPEED )
    {
      DEBUG_USB_INT_LO_PUTCHAR( '4' );
      USB->HFIR = 48000;
      /* Set 48 MHz PHY clock */
      USB->HCFG = ( USB->HCFG & ~_USB_HCFG_FSLSPCLKSEL_MASK ) |
                  ( 1 << _USB_HCFG_FSLSPCLKSEL_SHIFT        );
    }

    USBH_PortStatus = H_PORT_CONNECTED_RESETTING;
    USBTIMER_Start( HOSTPORT_TIMER_INDEX,
                    USBH_AttachTiming[ USBH_AttachRetryCount ].resetTime,
                    PortResetComplete );
    USBHHAL_PortReset( true );
  }
  else
  {
    USBH_PortStatus = H_PORT_DISCONNECTED;
  }
}

/*
 * Handle port interrupt.
 */
static void Handle_USB_GINTSTS_PRTINT( void )
{
  uint32_t hprt;

  hprt = USB->HPRT;                               /* Get port status */

  DEBUG_USB_INT_LO_PUTCHAR( '^' );

  switch ( USBH_PortStatus )
  {
    case H_PORT_DISCONNECTED:
    /***********************/
      if ( ( hprt & USB_HPRT_PRTCONNDET ) &&
           ( hprt & USB_HPRT_PRTCONNSTS )    )    /* Any device connected ? */
      {
        DEBUG_USB_INT_LO_PUTCHAR( '2' );
        USBH_PortStatus = H_PORT_CONNECTED_DEBOUNCING;
        USBTIMER_Start( HOSTPORT_TIMER_INDEX,
                        USBH_AttachTiming[ USBH_AttachRetryCount ].debounceTime,
                        PortDebounceComplete );
      }
      break;

    case H_PORT_CONNECTED_DEBOUNCING:
    /***********************/
      DEBUG_USB_INT_LO_PUTCHAR( 'Y' );
      break;

    case H_PORT_CONNECTED_RESETTING:
    /***********************/
      if ( ( hprt & USB_HPRT_PRTENCHNG  ) &&    /* Port enable changed ?    */
           ( hprt & USB_HPRT_PRTENA     ) &&    /* Port enabled ?           */
           ( hprt & USB_HPRT_PRTCONNSTS )    )  /* Device still connected ? */
      {
        DEBUG_USB_INT_LO_PUTCHAR( '6' );
        USBH_PortStatus = H_PORT_CONNECTED;
      }
      break;

    case H_PORT_CONNECTED:
    /***********************/
      if ( (    hprt & USB_HPRT_PRTENCHNG ) &&
           ( !( hprt & USB_HPRT_PRTENA )  )    )
      {
        DEBUG_USB_INT_LO_PUTCHAR( 'X' );
      }
      break;
  }

  if ( hprt & USB_HPRT_PRTOVRCURRCHNG ) /* Overcurrent change interrupt ? */
  {
    DEBUG_USB_INT_LO_PUTCHAR( '9' );
  }

  hprt &= ~HPRT_WC_MASK;              /* Mask off all write clear bits  */
  hprt |= USB_HPRT_PRTCONNDET | USB_HPRT_PRTENCHNG | USB_HPRT_PRTOVRCURRCHNG;
  USB->HPRT = hprt;                   /* Clear all port interrupt flags */
}

#if defined( USB_SLAVEMODE )
/*
 * Handle periodic transmit FIFO empty interrupt.
 */
static void Handle_USB_GINTSTS_PTXFEMP( void )
{
  int i;
  uint8_t hcnum;
  uint32_t fifoStatus;
  USBH_Hc_TypeDef *hc;

  fifoStatus = USB->HPTXSTS;
  hcnum = ( fifoStatus & FIFO_TXSTS_HCNUM_MASK ) >> FIFO_TXSTS_HCNUM_SHIFT;
  hc = &hcs[ hcnum ];

  USBHHAL_FillFifo( hcnum, fifoStatus, hc );

  DEBUG_USB_INT_LO_PUTS( "PE" );

  for ( i = 0; i < NUM_HC_USED + 2; i++ )
  {
    if ( hcs[ i ].txPFempIntOn == true )
      return;                               /* TxFemp int must remain on */
  }

  USB->GINTMSK &= ~USB_GINTSTS_PTXFEMP ;    /* Turn off TxFemp interrupt */
}
#endif

#if defined( USB_SLAVEMODE )
/*
 * Handle receive FIFO full interrupt.
 */
static void Handle_USB_GINTSTS_RXFLVL( void )
{
  uint8_t hcnum;
  USBH_Hc_TypeDef *hc;
  uint32_t grxstsp, hcchar;
  int bytecnt, pid, packetcnt;

  /* Disable the Rx Status Queue Level interrupt */
  USB->GINTMSK &= ~USB_GINTMSK_RXFLVLMSK;

  grxstsp   = USB->GRXSTSP;
  hcnum     = ( grxstsp & _USB_GRXSTSP_CHEPNUM_MASK ) >> _USB_GRXSTSP_CHEPNUM_SHIFT;
  hc        = &hcs[ hcnum ];
  bytecnt   = ( grxstsp & _USB_GRXSTSP_BCNT_MASK ) >> _USB_GRXSTSP_BCNT_SHIFT;
  pid       = ( grxstsp & _USB_GRXSTSP_DPID_MASK ) >> _USB_GRXSTSP_DPID_SHIFT;
  packetcnt = ( USB->HC[ hcnum ].TSIZ & _USB_HC_TSIZ_PKTCNT_MASK ) >>
              _USB_HC_TSIZ_PKTCNT_SHIFT;
  hcchar    = USB->HC[ hcnum ].CHAR;

  DEBUG_USB_INT_HI_PUTCHAR( 'q' );

  switch ( grxstsp & _USB_GRXSTSP_PKTSTS_MASK )
  {
    case GRXSTSP_PKTSTS_HOST_DATAINRECEIVED:
      /* Read the data into the host buffer. */
      if ( ( bytecnt > 0 ) && ( hc->buf != NULL ) )
      {
        USBHAL_ReadFifo( hc->buf, bytecnt );

        hc->remaining -= bytecnt;
        hc->buf       += bytecnt;
        hc->xferred   += bytecnt;

        if ( packetcnt )
        {
          /* re-activate the channel when more packets are expected */
          hcchar |= USB_HC_CHAR_CHENA;
          hcchar &= ~USB_HC_CHAR_CHDIS;
          USB->HC[ hcnum ].CHAR = hcchar;
        }

        hc->ep->toggle = pid;
      }
      DEBUG_USB_INT_HI_PUTS( "IR" );
      break;

    case GRXSTSP_PKTSTS_HOST_DATAINCOMPLETE:
      if ( packetcnt )
      {
        hc->ep->toggle = pid;
      }
      DEBUG_USB_INT_HI_PUTS( "IC" );
      break;

    case GRXSTSP_PKTSTS_HOST_CHANNELHALTED:
    case GRXSTSP_PKTSTS_HOST_DATATOGGLEERROR:
    default:
      break;
  }

  /* Enable the Rx Status Queue Level interrupt */
  USB->GINTMSK |= USB_GINTMSK_RXFLVLMSK;
}
#endif

/** @endcond */

#endif /* defined( USB_HOST ) */
#endif /* defined( USB_PRESENT ) && ( USB_COUNT == 1 ) */
