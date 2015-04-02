// adapted from Stephan Meyer's source
// http://sourceforge.net/mailarchive/message.php?msg_id=10751046

#include <stdio.h>

#include "an2131.h"
#include "../../libf2a.h"

#define	EZUSB_CS_ADDRESS	0x7F92
#define MAX_HEX_RECORD_LENGTH	16

typedef struct
{
	u_int32_t length;
	u_int32_t address;
	u_int32_t type;
	char data[MAX_HEX_RECORD_LENGTH];
} hex_record;

static int ezusb_read_hex_record (hex_record* record, const unsigned char** data)
{
	u_int32_t i, length, read, tmp, checksum;

	while (**data == '\n' || **data == '\r')
		(*data)++;

	if (**data != ':')
	{
		printerr("Could not find ':' in hexfile\n");
		return -1;
	}
	(*data)++;

	read = sscanf((char*)*data, "%2X%4X%2X", &record->length, &record->address, &record->type);
	if (read != 3)
	{
		printerrno("Could not read length, address, and type in hexfile (read %i/3 values)", read);
		return -1;
	}
	*data += 8;

	checksum = record->length + (record->address >> 8) + record->address + record->type;

	length = record->length;

	if (length > MAX_HEX_RECORD_LENGTH)
	{
		printerr("Line too long in hexfile\n");
		return -1;
	}

	for (i = 0; i < length; i++)
	{
		read = sscanf((char*)*data, "%2X", &tmp);
		if (read != 1)
		{
			printerr("Could not read data in hexfile\n");
			return -1;
		}
		*data += 2;

		record->data[i] = (char) tmp;
		checksum += tmp;
	}

	read = sscanf((char*)*data, "%2X\n", &tmp);
	if ((read != 1) || (((char) (checksum + tmp)) != 0x00))
	{
		printerr("Checksum error in hexfile\n");
		return -1;
	}
	*data += 2;

	return 0;
}

static int ezusb_load (usb_dev_handle * dev, u_int32_t address, u_int32_t length, char * data, int usb_timeout)
{
	int err;
	if ((err = usb_control_msg(dev, 0x40, 0xA0, address, 0, data, length, usb_timeout)) <= 0)
	{
#if _WIN32
		// dunno why it returns -1 but still works under win32
		if (err == -1)
			return 0;
#endif
		printerr("usb_control_msg: %s (error code %i)\n", usb_strerror(), err);
		return -1;
	}
	return 0;
}

int ezusb_load_firmware (struct usb_device* dev, const unsigned char* data, int usb_timeout)
{
	int result;
	char ezusb_cs;
	hex_record record;
	usb_dev_handle * handle; 

	if ((handle = usb_open(dev)) == NULL)
	{
		printerr("usb_open: %s\n", usb_strerror());
		return -1;
	}

	/* stop chip */
	ezusb_cs = 1;
	if (ezusb_load(handle, EZUSB_CS_ADDRESS, 1, &ezusb_cs, usb_timeout) < 0)
	{
		printerr("an2131: could not stop chip\n");
		usb_close(handle);
		return -1;
	}

	while ((result = ezusb_read_hex_record(&record, &data)) >= 0)
	{
		if (record.type != 0)
		{
			break;
		}

		if (ezusb_load(handle, record.address, record.length, record.data, usb_timeout) < 0)
		{
			printerr("an2131: could not upload data to chip\n");
			usb_close(handle);
			return -1;
		}
	}
	if (result < 0)
	{
		printerr("Could not upload firmware in an2131 chip\n");
		usb_close(handle);
		return -1;
	}

	/* start chip */
	ezusb_cs = 0;
	if (ezusb_load(handle, EZUSB_CS_ADDRESS, 1, &ezusb_cs, usb_timeout) < 0)
	{
		printerr("an2131: could not start chip\n");
		usb_close(handle);
		return -1;
	}

	usb_close(handle);
	return 0;
}
