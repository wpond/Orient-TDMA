#include <stdbool.h>
#include <stdio.h>

#include <efm32.h>

#include "usb.h"

#include "usb/usbd.h"

#include "usb/macros.h"
#include "usb/usb-util.h"

/* DMA for the EFM32's USB core core is performed in word-sized chunks, and the 
   source/destination regions in main SRAM must all have addresses on word 
   boundaries. The USB_BUFFER macro ensures correct alignment of the memory
   regions used for USB DMA. */

USB_BUFFER static const struct usb_device_desc device_desc = {
    .bLength                    = sizeof(struct usb_device_desc),
    .bDescriptorType            = USB_DEVICE_DESC,

    /* Advertise USB 1.1, otherwise a bunch of extra descriptors will be
       requested during enumeration (e.g. device qualifier). EFM32 is USB 2.0
       compliant (though it isn't capable of High Speed signalling), but
       unless you really need 1024-byte isochronous packets (the USB 1.1 limit
       is an awkward 1023 bytes), then there is no benefit to using it. */
    .bcdUSB                     = 0x0101,

    /* Present as a USB composite device. This allows us to export each
       standardised (e.g. HID, mass storage, audio) or proprietary
       functionality on its own interface. */
    .bDeviceClass               = 0,
    .bDeviceSubClass            = 0,
    .bDeviceProtocol            = 0,
    .bMaxPacketSize0            = USB_PACKET_SIZE,

    /* Vendor IDs are centrally allocated by the USB Implementors Forum,
       and cost a few thousand USD per year. No site-local vendor IDs are
       officially defined by the USB-IF, so we have to make up something that
       is unlikely to collide with a real VID in the future. */
    .idVendor                   = 0xFFFF,
    .idProduct                  = 0x0001,
    .bcdDevice                  = 0x0000,

    /* Indices for strings describing this device. These strings will be
       displayed in the Windows device manager or in the Linux syslog. 

       String descriptors are optional; if you set all of these to zero, then
       you can dispense with the string descriptor handling code altogether.
       In this instance, your device will presumably be identified as just a
       "USB Composite Device". */
    .iManufacturer              = 1,
    .iProduct                   = 2,
    .iSerialNumber              = 3,
    .bNumConfigurations         = 1
};

/* Configuration descriptors must be followed by a contiguous blob of every
   descriptor pertaining to that configuration (interfaces, endpoints, HID
   descriptors, etc). The size of this entire wad is returned in the
   wTotalLength field of the config descriptor itself. */
USB_BUFFER static const struct {
    struct usb_config_desc config;
    struct usb_interface_desc iface0;
    struct usb_endpoint_desc ep1;
} config_desc = {
    .config = {
        .bLength                = sizeof(struct usb_config_desc),
        .bDescriptorType        = USB_CONFIG_DESC,
        .wTotalLength           = sizeof(struct usb_config_desc) 
                                    + 1 * sizeof(struct usb_interface_desc) 
                                    + 1 * sizeof(struct usb_endpoint_desc),
        .bNumInterfaces         = 1,
        .bConfigurationValue    = 1,
        .iConfiguration         = 0,
        .bmAttributes           = USB_CONF_RESERVED | USB_CONF_SELF_POWERED,
        .bMaxPower              = 50
    },

    .iface0 = {
        .bLength                = sizeof(struct usb_interface_desc),
        .bDescriptorType        = USB_INTERFACE_DESC,
        .bInterfaceNumber       = 0,
        .bAlternateSetting      = 0,

        /* Interface descriptor must be immediately followed by this many
           endpoint descriptors. Note that Endpoint 0 is implicitly associated
           with every interface, and it is not counted here. */
        .bNumEndpoints          = 1,

        /* We provide a vendor-specific interface here. The OS will not attempt
           to bind any generic USB device drivers to this interface, leaving
           it available for binding from user-space using libusb. 

           It is a good idea to bundle all of your private control transfers
           into an interface, instead of making them global to the device. 
           The device itself should be nothing more than a container for
           various standardardised and proprietary functionalities exposed
           on separate interfaces. */
        .bInterfaceClass        = 0xFF,
        .bInterfaceSubClass     = 0,
        .bInterfaceProtocol     = 0,

        .iInterface             = 0
    },

    .ep1 = {
        .bLength                = sizeof(struct usb_endpoint_desc),
        .bDescriptorType        = USB_ENDPOINT_DESC,
        .bEndpointAddress       = USB_XFER_IN | 1,
        .bmAttributes           = USB_BULK_ENDPOINT,
        .wMaxPacketSize         = USB_PACKET_SIZE,

        .bInterval              = 16
    }
};

/* Devices that provide string descriptors must also provide a special string
   descriptor with an ID of 0. Instead of a UTF-16LE string, this descriptor
   contains a list of languages into which this device's strings have been
   internationalised. Requests for string descriptors other than zero will
   specify the desired language ID in the wIndex field of the requesting SETUP 
   packet. For our purposes, a single language ID corresponding to US English 
   will be perfectly adequate. 

   The full list of language IDs is available at:

   http://www.usb.org/developers/docs/USB_LANGIDs.pdf

   I'm sure the fact that USB LANGIDs are completely identical to 
   Microsoft Windows NT LANGIDs is a complete co-incidence. */

USB_BUFFER static const struct usb_string_desc str_0 = {
    .bLength                    = 4,
    .bDescriptorType            = USB_STRING_DESC,
    .utf16                      = { 0x0409 }
};

DEFINE_USB_STRING(str_manf, "SpeckNet");
DEFINE_USB_STRING(str_prod, "Dummy device");
DEFINE_USB_STRING_BUFFER(str_serial, 16);

/* Internal table of strings used by handle_setup_get_descriptor() below.
   Various descriptors will refer to human-readable strings using a non-zero
   ID; for instance, the Device Descriptor contains two bytes called iProduct
   and iManufacturer. The host will then perform GET_DESCRIPTOR Control IN
   transfers referencing each string ID in sequence, together with one of the
   LANGIDs advertised in string descriptor 0 (the request for string
   descriptor 0 itself carries a LANGID of zero). */

static const struct usb_string_desc *strings[] = {
    &str_0,
    &str_manf,
    &str_prod,
    &str_serial.desc
};

/* After a successful enumeration, endpoint 1 on this device will yield the
   following string on every read. N.B. The standard lorem ipsum has been
   padded with an ellipsis because this makes the length a multiple of 64. */
USB_BUFFER static const char hello_world[] = 
    "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate "
    "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
    "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
    "mollit anim id est laborum...";

/* The contents of the current control transfer's SETUP packet are stored
   in this global. */
static struct usb_setup_packet setup;

static void handle_in(const struct usb_event *ev);
static void handle_out(const struct usb_event *ev);
static void handle_setup(const struct usb_event *ev);
static bool handle_setup_get_descriptor(const struct usb_event *ev);
static bool handle_setup_set_configuration(const struct usb_event *ev);
static bool handle_setup_std_in(const struct usb_event *ev);
static bool handle_setup_std_out(const struct usb_event *ev);
static bool handle_setup_ven_in(const struct usb_event *ev);

void USB_IRQHandler(void)
{
    struct usb_event ev;

    while (usb_handle_irq(&ev)) {
        switch (ev.type) {
            case USB_ATTACH:
                /* The USB core requires a 48 MHz crystal in order to operate
                   correctly. The USB driver doesn't switch this on
                   automatically, because having your high frequency peripheral
                   clock jump from (by default) 7MHz to 48MHz is the sort of
                   event that you'd want to be very explictly aware of.

                   More sophisticated applications should take care that there
                   aren't any external bus transfers in progress when this
                   happens, or the bus speed will jump and cause bus errors. */
                usb_power_on();

                break;

            case USB_DETACH:
                usb_power_off();
                //cmu_hfxo_off();

                break;

            case USB_RESET:
                /* Prepare to receive first SETUP packet. Endpoints are
                   deconfigured automatically for you on reset. 

                   The second parameter indicates whether EP0 is in a valid
                   state or not. Passing false will cause the device to
                   respond to all non-SETUP packets with a STALL packet, which
                   immediately indicates an error condition to the host. */
                usb_prepare_setup(&setup, true);

                break;

            case USB_SETUP:
                handle_setup(&ev);

                break;

            case USB_IN:
                handle_in(&ev);

                break;

            case USB_OUT:
                handle_out(&ev);

                break;
        }
    }
}

static void handle_in(const struct usb_event *ev)
{
    switch (ev->ep) {
        case 0:
            if (setup.bmRequestType & USB_XFER_IN) {
                /* Data phase of Control IN complete, prepare status OUT */
                usb_prepare_out(0, NULL, 0);
            } else {
                /* Status phase of Control OUT complete, await next SETUP */
                usb_prepare_setup(&setup, true);
            }

            break;

        case 1:
            if (ev->nbytes > 0 && ev->nbytes % USB_PACKET_SIZE == 0) {
                /* A USB transfer is over when a packet shorter than the
                   maximum packet size is sent. If we transferred a payload
                   that fits into an integral number of maximum length packets,
                   then we need to explicitly terminate the transfer with a
                   packet of length zero. The ev->nbytes > 0 check is there so
                   that we don't get stuck in an infinite loop of sending
                   ZLPs that will be interpreted as blank payloads.

                   This is completely unrelated to control message Status IN
                   phases, even if it looks similar!

                   Note that the ellipsis at the end of the lorem ipsum above
                   has been deliberately inserted to pad out the payload size
                   to a multiple of 64 (USB_PACKET_SIZE), in order to
                   illustrate this particular case. */
                usb_prepare_in(1, NULL, 0);
            } else {
                /* The transfer is complete, arm the endpoint to transmit
                   another copy of the sample text. */
                usb_prepare_in(1, hello_world, sizeof(hello_world) - 1);
            }

            break;
    }
}

static void handle_out(const struct usb_event *ev)
{
    /* Same idea as case 0 in handle_in() */
    if (setup.bmRequestType & USB_XFER_IN) {
        usb_prepare_setup(&setup, true);
    } else {
        usb_prepare_in(0, NULL, 0);
    }
}

static void handle_setup(const struct usb_event *ev)
{
    bool ok;

    switch (setup.bmRequestType) {
        case USB_XFER(IN, STANDARD, DEVICE):
            ok = handle_setup_std_in(ev);

            break;

        case USB_XFER(OUT, STANDARD, DEVICE):
            ok = handle_setup_std_out(ev);

            break;

        case USB_XFER(IN, VENDOR, INTERFACE):
            ok = handle_setup_ven_in(ev);

            break;

        default:
            ok = false;

            break;
    }

    if (!ok) {
        /* Invalid control transfer, respond to the data/status phase with a
           STALL packet. The host will need to send a SETUP packet in order to
           clear the STALL. */
        usb_prepare_setup(&setup, false);
    }
}

static bool handle_setup_std_in(const struct usb_event *ev)
{
    switch (setup.bRequest) {
        case USB_GET_DESCRIPTOR:
            handle_setup_get_descriptor(ev);

            return true;

        default:
            return false;
    }
}

static bool handle_setup_std_out(const struct usb_event *ev)
{
    switch (setup.bRequest) {
        case USB_SET_ADDRESS:
            usb_prepare_in(0, NULL, 0);

            return true;

        case USB_SET_CONFIGURATION:
            return handle_setup_set_configuration(ev);

        default:
            return false;
    }
}

static bool handle_setup_get_descriptor(const struct usb_event *ev)
{
    uint8_t type;
    uint8_t id;

    type = setup.wValue >> 8;
    id   = setup.wValue >> 0;

    switch (type) {
        case USB_DEVICE_DESC:
            if (id != 0) {
                return false;
            }

            usb_prepare_control_in(&setup, &device_desc, sizeof(device_desc));

            return true;

        case USB_CONFIG_DESC:
            if (id != 0) {
                return false;
            }

            usb_prepare_control_in(&setup, &config_desc, sizeof(config_desc));

            return true;

        case USB_STRING_DESC:
            if (id > lengthof(strings)) {
                return false;
            }

            /* Ignore LANGID in setup.wIndex, since we only advertise one
               LANGID in string descriptor 0. I'm sure most USB devices out
               there do the same. */

            usb_prepare_control_in(&setup, strings[id], strings[id]->bLength);

            return true;

        default:
            return false;
    }
}

static bool handle_setup_set_configuration(const struct usb_event *ev)
{
    switch (setup.wValue) {
        case 0:
            /* Deconfigure device, disable nonzero endpoints */
            usb_change_configuration(USB_PACKET_SIZE);

            break;

        case 1:
            /* Configure device, enable nonzero endpoints. Also prepare the
               first IN payload (for this application is is a simple hello
               world message that can be read out using a bulk IN transfer).

               handle_in() will just repeat the same value available every time
               the host performs a bulk IN. Real applications will of course 
               want to present some meaningful stream of data instead. */
            usb_change_configuration(USB_PACKET_SIZE);
            usb_configure_in_endpoint(1, USB_BULK_ENDPOINT, USB_PACKET_SIZE);
            usb_prepare_in(1, hello_world, sizeof(hello_world) - 1);

            break;

        default:
            /* Invalid configuration requested */
            return false;
    }

    usb_prepare_in(0, NULL, 0);

    return true;
}

static bool handle_setup_ven_in(const struct usb_event *ev)
{
    /* Obviously these two if statements would normally be switch blocks */

    if (setup.wIndex != 0) {
        /* Unknown interface */
        return false;
    }

    if (setup.bRequest != 0) {
        /* Unknown request */
        return false;
    }

    usb_prepare_control_in(&setup, hello_world, sizeof(hello_world) - 1);

    return true;
}

void USB_Init()
{
	usb_populate_serial_number(&str_serial.desc, 
            (void *) &DEVINFO->UNIQUEL, 8);

    usb_init();
}
