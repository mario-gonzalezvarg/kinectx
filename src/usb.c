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

static int map_libusb(const int rc) {
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

const char *device_err_str(const int code) {
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

int device_host_create(device_host **out) {
  if (!out) return DEVICE_EINVAL;

  device_host *host = (device_host *)calloc(1, sizeof(*host));
  if (!host) return DEVICE_ENOMEM;

  const int rc = libusb_init_context(&host->usb, NULL, 0);
  if (rc < 0) {free(host); return map_libusb(rc); }

  *out = host;
  return DEVICE_OK;
}

void device_host_destroy(device_host *host) {
  if (!host) return;
  if (host->usb) libusb_exit(host->usb);
  free(host);
}

int device_host_scan(device_host *host, uint16_t vid, uint16_t pid, device_id **out_ids, size_t *out_n) {
  if (!host || !out_ids || !out_n) return DEVICE_EINVAL;

  libusb_device **list = NULL;
  const ssize_t ndev = libusb_get_device_list(host->usb, &list);
  if (ndev < 0) return map_libusb((int) ndev);

  // allocate memory for devices scanned
  size_t cap = 32, n = 0;
  device_id *ids = (device_id *)calloc(cap, sizeof(*ids));
  if (!ids) {
    libusb_free_device_list(list, 1);
    return DEVICE_ENOMEM;
  }

  // populate info of devices to a data type
  for (ssize_t i = 0; i < ndev; i++) {
    libusb_device *dev = list[i];
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) != 0) continue;

    if (vid && desc.idVendor != vid) continue;
    if (pid && desc.idProduct != pid) continue;

  }
}