#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"

//--------------------------------------------------------------------+
// Device Descriptors - Core USB device properties
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
		{
				.bLength = sizeof(tusb_desc_device_t),
				.bDescriptorType = TUSB_DESC_DEVICE,
				.bcdUSB = USB_BCD,
				.bDeviceClass = 0x00,
				.bDeviceSubClass = 0x00,
				.bDeviceProtocol = 0x00,
				.bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

				.idVendor = USB_VID,
				.idProduct = USB_PID,
				.bcdDevice = 0x0100,

				.iManufacturer = 0x01,
				.iProduct = 0x02,
				.iSerialNumber = 0x03,

				.bNumConfigurations = 0x01};

// Return device descriptor when requested by host
uint8_t const *tud_descriptor_device_cb(void)
{
	return (uint8_t const *)&desc_device;
}

//--------------------------------------------------------------------+
// HID Report Descriptors - Define HID device types
//--------------------------------------------------------------------+

// Keyboard HID report descriptor
uint8_t const desc_hid_keyboard[] =
		{
				TUD_HID_REPORT_DESC_KEYBOARD()};

// Mouse HID report descriptor
uint8_t const desc_hid_mouse[] =
		{
				TUD_HID_REPORT_DESC_MOUSE()};

// Gamepad HID report descriptor
uint8_t const desc_hid_gamepad[] =
		{
				TUD_HID_REPORT_DESC_GAMEPAD()};

// RAWHID descriptor
uint8_t const desc_hid_rawhid[] =
		{
				TUD_HID_REPORT_DESC_GENERIC_INOUT(CFG_TUD_HID_EP_BUFSIZE)};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t itf)
{
	if (itf == 0)
	{
		return desc_hid_keyboard;
	}
	else if (itf == 1)
	{
		return desc_hid_mouse;
	}
	else if (itf == 2)
	{
		return desc_hid_gamepad;
	}
	else if (itf == 3)
	{
		return desc_hid_rawhid;
	}

	return NULL;
}
//--------------------------------------------------------------------+
// Configuration Descriptor - Interface/Endpoint setup
//--------------------------------------------------------------------+

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_INOUT_DESC_LEN)

uint8_t const desc_configuration[] =
		{
				// Config number, interface count, string index, total length, attribute, power in mA
				TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

				// Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
				TUD_HID_DESCRIPTOR(INTERFACE_KEYBOARD, 4, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_keyboard), EPNUM_KEYBOARD, CFG_TUD_HID_EP_BUFSIZE, USB_POLLING_INTERVAL),
				TUD_HID_DESCRIPTOR(INTERFACE_MOUSE, 5, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_mouse), EPNUM_MOUSE, CFG_TUD_HID_EP_BUFSIZE, USB_POLLING_INTERVAL),
				TUD_HID_DESCRIPTOR(INTERFACE_GAMEPAD, 6, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_gamepad), EPNUM_GAMEPAD, CFG_TUD_HID_EP_BUFSIZE, USB_POLLING_INTERVAL),
				TUD_HID_INOUT_DESCRIPTOR(INTERFACE_RAWHID, 7, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_rawhid), EPNUM_RAWHID, 0x80 | EPNUM_RAWHID, CFG_TUD_HID_EP_BUFSIZE, USB_POLLING_INTERVAL)};

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
	(void)index; // for multiple configurations
	return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// String descriptors (index 1: Manufacturer, 2: Product, 6: PHAC, 7: Interface)
char const *string_desc_arr[] =
		{
				(const char[]){0x09, 0x04}, // 0: Supported language is English (0x0409)
				"PHDesign",									// 1: Manufacturer
				"PHDesign PHAC_V1",					// 2: Product
				NULL,												// 3: Serial Number (unused)
				NULL,												// 4: Keyboard Interface (unused)
				NULL,												// 5: Mouse Interface (unused)
				"PHAC Controller",					// 6: Gamepad Interface It's needed to show the ture name
				"PHDesign PHAC Interface"}; // 7: RAWHID Interface For Button Mapping

static uint16_t _desc_str[32 + 1];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
	(void)langid;
	size_t chr_count;

	switch (index)
	{
	case STRID_LANGID:
		memcpy(&_desc_str[1], string_desc_arr[0], 2);
		chr_count = 1;
		break;

	case STRID_SERIAL:
		chr_count = board_usb_get_serial(_desc_str + 1, 32);
		break;

	default:

		if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
			return NULL;

		const char *str = string_desc_arr[index];

		// Cap at max char
		chr_count = strlen(str);
		size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type
		if (chr_count > max_count)
			chr_count = max_count;

		// Convert ASCII string into UTF-16
		for (size_t i = 0; i < chr_count; i++)
		{
			_desc_str[1 + i] = str[i];
		}
		break;
	}

	// first byte is length (including header), second byte is string type
	_desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

	return _desc_str;
}
