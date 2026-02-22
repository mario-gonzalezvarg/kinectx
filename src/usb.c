//usb.c
#include "usb.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <libusb-1.0/libusb.h>

// module state
struct device_host {
    libusb_context *usb;
};

// resource handler
struct device_link {
    device_host *host;
    libusb_device_handle *usb;
    uint32_t claimed;
    uint32_t detached;
};

static int map_libusb(int rc) {
  if (rc >= 0) return rc;
  switch (rc) {
    case LIBUSB_ERROR_INVALID_PARAM: return DEVICE_EINVAL;
    case LIBUSB_ERROR_NO_MEM:        return DEVICE_ENOMEM;
    case LIBUSB_ERROR_NO_DEVICE:     return DEVICE_ENODEV;
    case LIBUSB_ERROR_TIMEOUT:       return DEVICE_ETIME;
    case LIBUSB_ERROR_BUSY:          return DEVICE_EBUSY;
    case LIBUSB_ERROR_ACCESS:        return DEVICE_EACCESS;
    default:                         return DEVICE_EIO;
  }
}

const char *device_err_str(int code) {
  switch (code) {
    case DEVICE_OK:      return "ok";
    case DEVICE_EINVAL:  return "invalid argument";
    case DEVICE_ENOMEM:  return "out of memory";
    case DEVICE_EIO:     return "I/O error";
    case DEVICE_ETIME:   return "timeout";
    case DEVICE_ENODEV:  return "no such device";
    case DEVICE_EBUSY:   return "busy";
    case DEVICE_EACCESS: return "access denied";
    case DEVICE_ESTATE:  return "invalid state";
    default:             return "unknown error";
  }
}