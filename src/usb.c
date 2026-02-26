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

  // populate info of devices to a libusb device data structure
  for (ssize_t i = 0; i < ndev; i++) {
    libusb_device *dev = list[i];
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) != 0) continue;

    // filter
    if (vid && desc.idVendor != vid) continue;
    if (pid && desc.idProduct != pid) continue;

    // increase allocated memory using amortize resizing
    if (cap == n) {
      cap *= 2;
      device_id *tmp = (device_id *)realloc(ids, cap * sizeof(*ids));
      if (!tmp) {
        free(ids);
        libusb_free_device_list(list, 1);
        return DEVICE_ENOMEM;
      }
      ids = tmp;
      memset(ids + n, 0, (cap - n) * sizeof(*ids));
    }

    ids[0].vid = desc.idVendor;
    ids[0].pid = desc.idProduct;
    ids[0].bus = libusb_get_bus_number(dev);
    ids[0].addr = libusb_get_device_address(dev);
    n++;
  }
  libusb_free_device_list(list, 1);

  if (n == 0) {free(ids); ids = NULL;}
  *out_ids = ids;
  *out_n = n;
  return DEVICE_OK;
}

void device_ids_destroy(device_id *ids) {
  free(ids);
}

static int open_by_id(const device_host *host, const device_id *id, libusb_device_handle **out_usb) {

  // obtain USB devices
  libusb_device **list = NULL;
  ssize_t ndev = libusb_get_device_list(host->usb, &list);
  if (ndev < 0) return map_libusb((int) ndev);

  int rc = DEVICE_ENODEV;

  // obtain device descriptors for each USB connection
  for (ssize_t i = 0; i < ndev; i++) {
    libusb_device *dev = list[i];
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) != 0) continue;

    // verify ids
    if (desc.idVendor != id->vid) continue;
    if (desc.idProduct != id->pid) continue;
    if (libusb_get_bus_number(dev) != id->bus) continue;
    if (libusb_get_device_address(dev) != id->addr) continue;

    // open device to obtain handle
    libusb_device_handle *usb = NULL;
    int orc = libusb_open(dev, &usb);
    if (orc < 0) {rc = map_libusb(orc); break;}

    *out_usb = usb;
    rc = DEVICE_OK;
  }
  libusb_free_device_list(list, 1);
  return rc;
}

int device_link_open(device_host *host, device_id *id, device_link **out_link) {
  if (!host || !id || !out_link) return DEVICE_EINVAL;

  // open device using id value
  libusb_device_handle *usb = NULL;
  const int rc = open_by_id(host, id, &usb);
  if (rc != DEVICE_OK) return rc;

  // allocate memory space for resource handler
  device_link *link = (device_link *)calloc(1, sizeof(*link));
  if (!link) {libusb_close(usb); return DEVICE_ENOMEM;}

  link->host = host;
  link->usb = usb;
  link->claimed = 0;
  link->detached = 0;

  *out_link = link;
  return DEVICE_OK;
}

