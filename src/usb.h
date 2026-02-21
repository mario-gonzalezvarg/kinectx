// usb.h - USB device host interface

#ifndef USB.H
#define USB.H

#ifdef __cplusplus
extern "C" {
#endif

/* constrain data fields to be accesed only through pointers */
typedef struct device_host device_host;
typedef struct device_link device_link;

/* device locator */
typedef struct {
	uint16_t vid, pid;
	uint8_t bus, addr;
} device_id;

/* decoding layer for operation success/failure */
enum {
	DEVICE_OK = 0,
	DEVICE_INVAL = -1,
	DEVICE_ENOMEM = -2,
	DEVICE_EIO = -3,
	DEVICE_ETIME = -4,
	DEVICE_ENODEV = -5,
	DEVICE_EBUSY = -6,
	DEVICE_EACCESS = -7,
	DEVICE_ESTATE = -8
};

const char *device_err_str(int code);

/* host session lifecycle */
int device_host_create(device_host **out);
void device_host_create(device_host *host);

/* enumeration */



#ifdef __cplusplus
}
#endif
#endif


