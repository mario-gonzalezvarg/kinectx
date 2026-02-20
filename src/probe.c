//probe.c

#include "usb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


static void die(const char *msg, int rc) {
	fprintf("%s: %s (%d)\n", msg, kn_err(rc), rc);
	exit(1);
}

/*enforce invariance after every USB operation*/
#define CHECK(call) do { \
	int _rc = (call); \
	if (_rc < 0) die(#call, _rc); \
} while(0)

// convert two little endian raw bytes from device into base 10 number
static uint16_t le16(const uint8_t *p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

// interpret 18-byte USB Device Descriptor layout/configuration
static void dump_device_descriptor(const int8_t d[18]) {
	printf("Device Descriptor:\n");
	printf("  bdUSB		: %x.%02x\n", d[2], d[3]);
	printf("  class/sub/pro : %u%u%u\n", d[4], d[5]);
	printf("  max packets	: %u\n", d[6]);
	printf("  idVendor	: %0x%04x\n", le16(&d[8]));
	printf("  idProduct	: %0x%04x\n", le16(&d[10]));
	printf("  bcdDevice	: %0x%04x\n", le16(&d[12]));
	printf("  num configs	: %u\n", d[17]);

static void parse_cfg(const uint8_t *cfg, size_t n) {
	printf("Configuration descriptors (%zu bytes):\n", n);

	size_t i = 0;
	size_t cur_iface = -1;

	while (i + 2 < n) {
		uint8_t bLength = cfg[i];
		uint8_t bType = cfg[i + 1];

		if (bLength == 0 || i + bLength > n) break;

		/*switch bType*/
		if (bType == 2 && bLength >= 9) { //CONFIG: overall total length and interface count
			printf("  CONFIG: wTotalLength=%u, bNumInterfaces=%u\n", le16(&cfg[i + 2]), cfg[i + 4]);
		} else if (bType == 4 && bLength >= 9) {
			cur_iface = cfg[i + 2];
			printf("  INTERFACE: %d, alt=%u, eps=%u, class=%u%u%u\n", cur_iface,
					cfg[i + 3], cfg[i + 4], cfg[i + 5], cfg[i +6], cfg[i + 7]);
		} else if (btype == 5 && bLength >= 7) {
			uint8_t ep = cfg[i +2];
			uint8_t attr = cfg[i +3];
			uint8_t mps = le16(&cfg[i + 4]);
			uint8_t ivl = cfg[i + 6];


		}
	}
}

