//usb.c
#include "usb.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

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

static void read_ascii_str(libusb_device_handle *usb, const uint8_t idx, char *out, size_t out_sz) {
  assert(out && out_sz > 0);
  out[0] = '\0';
  if (!usb || idx == 0) return;

  int rc = libusb_get_string_descriptor_ascii(usb, idx, (unsigned char*) out, (int)out_sz);
  if (rc < 0) {out[0] = '\0'; return; }
  if ((ssize_t)rc >= out_sz) out[out_sz - 1] = '\0';
  else out[rc] = '\0';
}

static void fill_mfg(libusb_device_handle *usb, device_id *id) {
  if (!usb || !id) return;

  struct libusb_device_descriptor desc;
  int rc = libusb_get_device_descriptor(libusb_get_device(usb), &desc);
  if (rc < 0) return;

  read_ascii_str(usb, desc.iManufacturer, id->mfg, sizeof(id->mfg));
}

int device_host_create(device_host **out) {
  if (!out) return DEVICE_EINVAL;

  device_host *host = calloc(1, sizeof(*host));
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
  device_id *ids = calloc(cap, sizeof(*ids));
  if (!ids) { libusb_free_device_list(list, 1); return DEVICE_ENOMEM; }

  // populate info of devices to a libusb device data structure
  for (ssize_t i = 0; i < ndev; i++) {
    libusb_device *dev = list[i];
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(dev, &desc) != 0) continue;

    // filter
    if (vid && desc.idVendor != vid) continue;
    if (pid && desc.idProduct != pid) continue;

    // increase allocated memory using amortize resizing
    if (n == cap) {
      const size_t old = cap;
      cap *= 2;

      device_id *tmp = realloc(ids, cap * sizeof(*ids));
      if (!tmp) {
        free(ids);
        libusb_free_device_list(list, 1);
        return DEVICE_ENOMEM;
      }
      ids = tmp;
      memset(ids + old, 0, (cap - old) * sizeof(*ids));
    }

    ids[n].vid = desc.idVendor;
    ids[n].pid = desc.idProduct;
    ids[n].bus = libusb_get_bus_number(dev);
    ids[n].addr = libusb_get_device_address(dev);
    n++;
  }
  libusb_free_device_list(list, 1);

  if (n == 0) { free(ids); ids = NULL; }
  *out_ids = ids;
  *out_n = n;
  return DEVICE_OK;
}

void device_ids_destroy(device_id *ids) {
  free(ids);
}

static int open_by_id(const device_host *host, device_id *id, libusb_device_handle **out_usb) {

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

    fill_mfg(usb, id);
    *out_usb = usb;
    rc = DEVICE_OK;
    break;
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
  device_link *link = calloc(1, sizeof(*link));
  if (!link) {libusb_close(usb); return DEVICE_ENOMEM;}

  link->host = host;
  link->usb = usb;
  link->claimed = 0;
  link->detached = 0;

  *out_link = link;
  return DEVICE_OK;
}

static void restore_interfaces(device_link *link) {
  if (!link->usb) return;

  for (int iface = 0; iface < 32; iface++) {
    const uint32_t bit = (1u << iface);

    if (link->claimed & bit) {
      libusb_release_interface(link->usb, iface);
      link->claimed &= ~bit;
    }

    if (link->detached & bit) {
      libusb_attach_kernel_driver(link->usb, iface);
      link->detached &= ~bit;
    }
  }
}

void device_link_close(device_link *link) {
  if (!link) return;
  restore_interfaces(link);
  libusb_close(link->usb);
  free(link);
}

int device_link_claim(device_link *link, const int iface, const int detach_kernel) {
  if (!link || !link->usb || iface < 0 || iface > 32) return DEVICE_EINVAL;

  const uint32_t bit = (1u << iface);
  if (link->claimed & bit) return DEVICE_OK;

  if (detach_kernel) {
    const int active = libusb_kernel_driver_active(link->usb, iface);
    if (active == 1) {
      const int rc = libusb_detach_kernel_driver(link->usb, iface);
      if (rc < 0 && rc != LIBUSB_ERROR_NOT_SUPPORTED) return map_libusb(rc);
      if (rc >= 0) link->detached |= bit;
    }
  }

  const int rc = libusb_claim_interface(link->usb, iface);
  if (rc < 0) return map_libusb(rc);

  link->claimed |= bit;
  return DEVICE_OK;
}

int device_link_release(device_link *link, const int iface) {
  if (!link || !link->usb || iface < 0 || iface >= 32) return DEVICE_EINVAL;

  const uint32_t bit = (1u << iface);

  if (link->claimed & bit) {
    const int rc = libusb_release_interface(link->usb, iface);
    if (rc < 0) return map_libusb(rc);
    link->claimed &= ~bit;
  }

  if (link->detached & bit) {
    const int rc = libusb_attach_kernel_driver(link->usb, iface);
    if (rc < 0 && rc != LIBUSB_ERROR_NOT_SUPPORTED) return map_libusb(rc);
    link->detached &= ~bit;
  }

  return DEVICE_OK;
}

int device_link_set_alt(device_link *link, const int iface, const int alt) {
  if (!link || !link->usb) return DEVICE_EINVAL;
  const int rc = libusb_set_interface_alt_setting(link->usb, iface, alt);
  if (rc < 0) return map_libusb(rc);
  return DEVICE_OK;
}

int device_link_ctrl(device_link *link, uint8_t bmReq, uint8_t bReq, uint16_t wVal, uint16_t wIdx, void *data, uint16_t len, unsigned timeout_ms) {
  if (!link || !link->usb) return DEVICE_EINVAL;

  // perform a USB control transfer
  const int rc = libusb_control_transfer(link->usb, bmReq, bReq, wVal, wIdx, (unsigned char *) data, len, timeout_ms);
  return map_libusb(rc);
}

int device_link_bulk(device_link *link, uint8_t ep, void *data, const int len, const unsigned timeout_ms) {
  if (!link || !link->usb || !data || len < 0) return DEVICE_EINVAL;
  int done = 0;
  const int rc = libusb_bulk_transfer(link->usb, ep, (unsigned char *) data, len, &done, timeout_ms);
  if (rc < 0) return map_libusb(rc);
  return done;
}

int device_host_poll(device_host *host, const int timeout_ms) {
  if (!host || !host->usb) return DEVICE_EINVAL;

  if (timeout_ms < 0) {
    const int rc = libusb_handle_events(host->usb);
    return map_libusb(rc);
  }

  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  const int rc = libusb_handle_events_timeout(host->usb, &tv);
  return map_libusb(rc);
}
