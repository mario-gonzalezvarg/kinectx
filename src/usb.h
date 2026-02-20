#ifndef KN_USB_H
#define KN_USB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Library-wide context for ONE single USB handler connection */
typedef struct kn_usb kn_usb;
typedef struct kn_h kn_h;

/* Device identifer */
typedef struct {
	uint16_t vid, pid;
	uint8_t bus, addr;
} kn_id;

/* Model code notifier */
enum {
	KN_OK = 0,
	KN_EINVAL = -1,
	KN_ENOMEM = -2,
	KN_EIO = -3,
	KN_ETIME = -4,
	KN_ENODEV = -5,
	KN_EACCESS = -7,
	KN_ESTATE = -8
};
const char *kn_error(int code);

// context lifecycle
int kn_usb_new(kn_usb **out);
void kn_usb_del(kn_usb *u);

// device enumeration
int kn_usb_scan(kn_usb *u, uint16_t vid, uint8_t pid, kn_id **out, size_t *out_n);
void kn_usb_ids_del(kn_id *ids);

int kn_usb_open(kn_usb *u, kn_id *id, kn_h **out);
void kn_usb_close(kn_usb *u);

// interface managment
int kn_usb_claim(kn_h *h, int iFace, int detach_kernel);
int kn_usb_release(kn_h *h, int iFace);
int kn_usb_alt(kn_h *h, int iFace, int alt);

// control transfer (i.e. motors/LED)
int kn_usb_ctl(kn_h *h, uint8_t bmReq, uint8_t bReq,
		uint16_t wVal, uint16_t wIdx, 
		void *data, uint16_t len,
		unsigned timeout_ms);

// bulk transfer (i.e. firmware/audio)
int kn_usb_bulk(kn_h *h, uint8_t ep, void *data, int len, unsigned timeout_ms);
int kn_usb_step(kn_usb *u, int timeout_ms);

#ifdef __cplusplus
} // extern "C"
#endif
#endif







