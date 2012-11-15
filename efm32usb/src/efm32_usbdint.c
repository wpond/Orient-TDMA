/**************************************************************************//**
 * @file
 * @brief USB protocol stack library, USB device peripheral interrupt handlers.
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

#define HANDLE_INT( x ) if ( status & x ) { Handle_##x(); status &= ~x; }

static void Handle_USB_GINTSTS_ENUMDONE  ( void );
static void Handle_USB_GINTSTS_IEPINT    ( void );
static void Handle_USB_GINTSTS_OEPINT    ( void );
#if defined( USB_SLAVEMODE )
static void Handle_USB_GINTSTS_RXFLVL    ( void );
#endif
static void Handle_USB_GINTSTS_SOF       ( void );
static void Handle_USB_GINTSTS_USBRST    ( void );
static void Handle_USB_GINTSTS_SESSREQINT( void );
static void Handle_USB_GINTSTS_USBSUSP   ( void );
static void Handle_USB_GINTSTS_WKUPINT   ( void );

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

  HANDLE_INT( USB_GINTSTS_SESSREQINT )
  HANDLE_INT( USB_GINTSTS_WKUPINT    )
  HANDLE_INT( USB_GINTSTS_USBSUSP    )
  HANDLE_INT( USB_GINTSTS_SOF        )
#if defined( USB_SLAVEMODE )
  HANDLE_INT( USB_GINTSTS_RXFLVL     )
#endif
  HANDLE_INT( USB_GINTSTS_ENUMDONE   )
  HANDLE_INT( USB_GINTSTS_USBRST     )
  HANDLE_INT( USB_GINTSTS_IEPINT     )
  HANDLE_INT( USB_GINTSTS_OEPINT     )

  INT_Enable();

  if ( status != 0 )
  {
    DEBUG_USB_INT_LO_PUTS( "\nUinT" );
  }
}

/*
 * Handle port enumeration interrupt. This has nothing to do with normal
 * device enumeration.
 */
static void Handle_USB_GINTSTS_ENUMDONE( void )
{
  USBDHAL_Ep0Activate();
  dev->ep[ 0 ].state = D_EP_IDLE;
  USBDHAL_EnableInts( dev );
  DEBUG_USB_INT_LO_PUTS( "EnumD" );
}

/*
 * Handle IN endpoint transfer interrupt.
 */
static void Handle_USB_GINTSTS_IEPINT( void )
{
  int epnum;
  uint16_t epint;
  uint16_t epmask;
  uint32_t status;
  USBD_Ep_TypeDef *ep;

  DEBUG_USB_INT_HI_PUTCHAR( 'i' );

  epint = USBDHAL_GetAllInEpInts();
  for ( epnum = 0,                epmask = 1;
        epnum <= MAX_NUM_IN_EPS;
        epnum++,                  epmask <<= 1 )
  {
    if ( epint & epmask )
    {
      ep = USBD_GetEpFromAddr( USB_SETUP_DIR_MASK | epnum );
      status = USBDHAL_GetInEpInts( ep );

      if ( status & USB_DIEP_INT_XFERCOMPL )
      {
#if defined( USB_SLAVEMODE )
        /* Disable Tx FIFO empty interrupt */
        USB->DIEPEMPMSK &= ~epmask;
        USB_DINEPS[ epnum ].INT = USB_DIEP_INT_TXFEMP;
        status &= ~USB_DIEP_INT_TXFEMP;
#endif
        USB_DINEPS[ epnum ].INT = USB_DIEP_INT_XFERCOMPL;

        DEBUG_USB_INT_HI_PUTCHAR( 'c' );

        if ( epnum == 0 )
        {
#if !defined( USB_SLAVEMODE )
          if ( ep->remaining > ep->packetSize )
          {
            ep->remaining -= ep->packetSize;
            ep->xferred += ep->packetSize;
          }
          else
          {
            ep->xferred += ep->remaining;
            ep->remaining = 0;
          }
#endif
          USBDEP_Ep0Handler( dev );
        }
        else
        {
#if !defined( USB_SLAVEMODE )
          ep->xferred = ep->remaining -
                        ( ( USB_DINEPS[ epnum ].TSIZ      &
                            _USB_DIEP_TSIZ_XFERSIZE_MASK    ) >>
                          _USB_DIEP_TSIZ_XFERSIZE_SHIFT          );
          ep->remaining -= ep->xferred;
#endif
          USBDEP_EpHandler( ep->addr );
        }
      }

#if defined( USB_SLAVEMODE )
      if ( status & USB_DIEP_INT_TXFEMP )
      {
        USB_DINEPS[ epnum ].INT = USB_DIEP_INT_TXFEMP;

        if ( ep->state != D_EP_IDLE )
        {
          DEBUG_USB_INT_HI_PUTCHAR( 'f' );
          USBDHAL_FillFifo( ep );
        }
      }
#endif
    }
  }
}

/*
 * Handle OUT endpoint transfer interrupt.
 */
static void Handle_USB_GINTSTS_OEPINT( void )
{
  int epnum;
  uint16_t epint;
  uint16_t epmask;
  uint32_t status;
  USBD_Ep_TypeDef *ep;

  DEBUG_USB_INT_HI_PUTCHAR( 'o' );
  epint = USBDHAL_GetAllOutEpInts();
  for ( epnum = 0,                epmask = 1;
        epnum <= MAX_NUM_OUT_EPS;
        epnum++,                  epmask <<= 1 )
  {
    if ( epint & epmask )
    {
      ep = USBD_GetEpFromAddr( epnum );
      status = USBDHAL_GetOutEpInts( ep );

      if ( status & USB_DOEP_INT_XFERCOMPL )
      {
        USB_DOUTEPS[ epnum ].INT = USB_DOEP_INT_XFERCOMPL;

        DEBUG_USB_INT_HI_PUTCHAR( 'c' );

        if ( epnum == 0 )
        {
#if !defined( USB_SLAVEMODE )
          if ( ep->remaining > ep->packetSize )
          {
            ep->remaining -= ep->packetSize;
            ep->xferred += ep->packetSize;
          }
          else
          {
            ep->xferred += ep->remaining;
            ep->remaining = 0;
          }
#endif
          USBDEP_Ep0Handler( dev );
        }
        else
        {
#if !defined( USB_SLAVEMODE )
          ep->xferred = ep->hwXferSize -
              ( ( USB_DOUTEPS[ epnum ].TSIZ & _USB_DOEP_TSIZ_XFERSIZE_MASK ) >>
                _USB_DOEP_TSIZ_XFERSIZE_SHIFT );
          ep->remaining -= ep->xferred;
#endif
          USBDEP_EpHandler( ep->addr );
        }
      }

      /* Setup Phase Done */
      if ( status & USB_DOEP_INT_SETUP )
      {
        DEBUG_USB_INT_LO_PUTS( "\nSP" );

#if !defined( USB_SLAVEMODE )
        if ( USB->DOEP0INT & USB_DOEP_INT_BACK2BACKSETUP )
        {                           /* Back to back setup packets received */
          USB->DOEP0INT = USB_DOEP_INT_BACK2BACKSETUP;
          DEBUG_USB_INT_LO_PUTS( "B2B" );

          dev->setup = (USB_Setup_TypeDef*)
                       ( USB->DOEP0DMAADDR - USB_SETUP_PKT_SIZE );
        }
        else
        {
          /* Read SETUP packet counter from hw. */
          int supCnt = ( USB->DOEP0TSIZ & _USB_DOEP0TSIZ_SUPCNT_MASK )
                       >> _USB_DOEP0TSIZ_SUPCNT_SHIFT;

          if ( supCnt == 3 )
            supCnt = 2;

          dev->setup = &dev->setupPkt[ 2 - supCnt ];
        }
#endif
        USB->DOEP0TSIZ |= 3 << _USB_DOEP0TSIZ_SUPCNT_SHIFT;
        USB->DOEP0DMAADDR = (uint32_t)dev->setupPkt;
        USB->DOEP0INT = USB_DOEP_INT_SETUP;
        USBDEP_Ep0Handler( dev );   /* Call the SETUP process for the EP0  */
      }
    }
  }
}

#if defined( USB_SLAVEMODE )
/*
 * Handle receive FIFO full interrupt.
 */
static void Handle_USB_GINTSTS_RXFLVL( void )
{
  USBD_Ep_TypeDef *ep;
  uint32_t status, byteCount, count, residue;

  DEBUG_USB_INT_HI_PUTCHAR( 'q' );

  status = USB->GRXSTSP;                  /* Get status from top of FIFO */

  ep = USBD_GetEpFromAddr( status & _USB_GRXSTSP_CHEPNUM_MASK );

  switch ( status & _USB_GRXSTSP_PKTSTS_MASK )
  {
    case GRXSTSP_PKTSTS_DEVICE_DATAOUTRECEIVED:
      byteCount = (status & _USB_GRXSTSP_BCNT_MASK) >> _USB_GRXSTSP_BCNT_SHIFT;
      if ( byteCount )
      {
        DEBUG_USB_INT_HI_PUTCHAR( 'e' );
        if ( ep->state != D_EP_IDLE )
        {
          /* Check for possible buffer overflow */
          if ( byteCount > ep->remaining )
          {
            residue = byteCount - ep->remaining;
          }
          else
          {
            residue = 0;
          }

          count = EFM32_MIN( byteCount, ep->remaining );
          USBHAL_ReadFifo( ep->buf, count );
          ep->xferred   += count;
          ep->remaining -= count;
          ep->buf       += count;

          if ( residue )
          {
            USBHAL_FlushFifo( residue );
          }
        }
      }
      break;

    case GRXSTSP_PKTSTS_DEVICE_SETUPRECEIVED:
      USBHAL_ReadFifo( (uint8_t*)dev->setup, USB_SETUP_PKT_SIZE );
      break;
  }
}
#endif

/*
 * Handle Start Of Frame (SOF) interrupt.
 */
static void Handle_USB_GINTSTS_SOF( void )
{
  USB->GINTSTS = USB_GINTSTS_SOF;

  if ( dev->callbacks->sofInt )
  {
    dev->callbacks->sofInt(
      ( USB->DSTS & _USB_DSTS_SOFFN_MASK ) >> _USB_DSTS_SOFFN_SHIFT );
  }
}

/*
 * Handle USB port reset interrupt.
 */
static void Handle_USB_GINTSTS_USBRST( void )
{
  int i;

  DEBUG_USB_INT_LO_PUTS( "ReseT" );

  /* Check if reset in suspend mode with stopped USB PHY clock. */
  if ( USB->PCGCCTL & ( USB_PCGCCTL_GATEHCLK | USB_PCGCCTL_STOPPCLK ) )
  {
    USB->PCGCCTL &= ~USB_PCGCCTL_GATEHCLK;
    USB->PCGCCTL &= ~USB_PCGCCTL_STOPPCLK;
  }

  /* Clear Remote Wakeup Signalling */
  USB->DCTL &= ~( DCTL_WO_BITMASK | USB_DCTL_RMTWKUPSIG );
  USBHAL_FlushTxFifo( 0 );

  /* Clear pending interrupts */
  for ( i = 0; i <= MAX_NUM_IN_EPS; i++ )
  {
    USB_DINEPS[ i ].INT = 0xFFFFFFFF;
  }

  for ( i = 0; i <= MAX_NUM_OUT_EPS; i++ )
  {
    USB_DOUTEPS[ i ].INT = 0xFFFFFFFF;
  }

  USB->DAINTMSK = USB_DAINTMSK_INEPMSK0 | USB_DAINTMSK_OUTEPMSK0;
  USB->DOEPMSK  = USB_DOEPMSK_SETUPMSK  | USB_DOEPMSK_XFERCOMPLMSK;
  USB->DIEPMSK  = USB_DIEPMSK_XFERCOMPLMSK;

  /* Reset Device Address */
  USB->DCFG &= ~_USB_DCFG_DEVADDR_MASK;

  /* Setup EP0 to receive SETUP packets */
  USBDHAL_StartEp0Setup( dev );
  USBDHAL_EnableInts( dev );

  if ( dev->callbacks->usbReset )
  {
    dev->callbacks->usbReset();
  }

  USBD_SetUsbState( USBD_STATE_DEFAULT );
  USBDHAL_AbortAllTransfers( USB_STATUS_DEVICE_RESET );
}

/*
 * Handle USB port session request interrupt.
 */
static void Handle_USB_GINTSTS_SESSREQINT( void )
{
  USB->GINTSTS = USB_GINTSTS_SESSREQINT;

  /* Check if USB PHY clock has been stopped. */
  if ( USB->PCGCCTL & ( USB_PCGCCTL_GATEHCLK | USB_PCGCCTL_STOPPCLK ) )
  {
    USB->PCGCCTL &= ~USB_PCGCCTL_GATEHCLK;
    USB->PCGCCTL &= ~USB_PCGCCTL_STOPPCLK;
  }
  DEBUG_USB_INT_LO_PUTS( "\nSreQ" );
}

/*
 * Handle USB port suspend interrupt.
 */
static void Handle_USB_GINTSTS_USBSUSP( void )
{
  USB->GINTSTS = USB_GINTSTS_USBSUSP;

  USBDHAL_AbortAllTransfers( USB_STATUS_DEVICE_SUSPENDED );
  USBD_SetUsbState( USBD_STATE_SUSPENDED );

  DEBUG_USB_INT_LO_PUTS( "\nSusP" );
}

/*
 * Handle USB port wakeup interrupt.
 */
static void Handle_USB_GINTSTS_WKUPINT( void )
{
  USB->GINTSTS = USB_GINTSTS_WKUPINT;

  USBD_SetUsbState( dev->savedState );

  DEBUG_USB_INT_LO_PUTS( "WkuP\n" );
}

/** @endcond */

#endif /* defined( USB_DEVICE ) */
#endif /* defined( USB_PRESENT ) && ( USB_COUNT == 1 ) */
