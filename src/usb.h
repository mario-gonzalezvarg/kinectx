#ifndef KN_USB_H
#define KN_USB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*library-wide context for ONE single USB connection*/
typedef struct kn_usb kn_usb;
typedef struct kn_h kn_h;

/*device identifer*/
typedef struct {
	uint16_t vid, pid;
	uint8_t bus, addr;
} kn_id;

/*error code notifier*/
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

/*--------- Exposed Operations --------*/

/*create or destroy USB communcation*/
int kn_usb_new(kn_usb **out);
void kn_usb_del(kn_usb *u);
