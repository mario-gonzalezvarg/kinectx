// usb.h - USB device host interface
#pragma once

#ifndef USB_H
#define USB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* constrain data fields to be accessed only through pointers */
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
  DEVICE_EINVAL = -1,
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
void device_host_destroy(device_host *host);

/* enumerate devices connected to USB */
int device_host_scan(device_host *host, uint16_t vid, uint16_t pid,
                     device_id **out_ids, size_t *out_n);
void device_ids_destroy(device_id *ids);

/* open/close link handle */
int device_link_open(device_host *host, device_id *id,
                     device_link **out_link);
void device_link_close(device_link *link);

/* interface management */
int device_link_claim(device_link *link, int iface, int detach_kernel);
int device_link_release(device_link *link, int iface);
int device_link_set_alt(device_link *link, int iface, int alt);

/* transfers: return >=0 bytes transferred; <0 is DEVICE_* error */
int device_link_ctrl(device_link *link,
                     uint8_t bmReq, uint8_t bReq,
                     uint16_t wVal, uint16_t wIdx,
                     void *data, uint16_t len,
                     unsigned timeout_ms);


int device_link_bulk(device_link *link, uint8_t ep, void *data, int len, unsigned timeout_ms);

/* event pump */
int device_host_poll(device_host *host, int timeout_ms);

#ifdef __cplusplus
} // extern "C"
#endif
#endif
