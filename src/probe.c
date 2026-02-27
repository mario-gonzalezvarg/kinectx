//probe.c

#include "usb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>


static void die(const char *msg, int rc) {
	fprintf(stderr, "%s: %s (%d)\n", msg, device_err_str(rc), rc);
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


static void utf16le_to_ascii(const uint8_t *in, size_t in_bytes,
							 char *out, size_t out_sz) {
	out[0] = '\0';
	if (!in || in_bytes < 2) return;

	size_t j = 0;
	for (size_t i = 0; i + 1 < in_bytes && j + 1 < out_sz; i += 2) {
		uint16_t ch = (uint16_t)in[i] | ((uint16_t)in[i + 1] << 8);
		if (ch == 0) break;
		out[j++] = (ch < 0x80) ? (char)ch : '?'; // simple ASCII fallback
	}
	out[j] = '\0';
}

static int get_string_ascii(device_link *link, uint16_t langid, uint8_t idx,
							char *out, size_t out_sz) {
	out[0] = '\0';
	if (idx == 0) return DEVICE_OK; // no string available

	uint8_t buf[128] = {0};
	const int got = device_link_ctrl(
		link,
		0x80,                 // IN | STANDARD | DEVICE
		0x06,                 // GET_DESCRIPTOR
		(uint16_t)((3u << 8) | idx), // STRING desc (type=3), index=idx
		langid,               // wIndex = language ID
		buf, (uint16_t)sizeof(buf),
		1000);

	if (got < 0) return got;
	if (got < 2 || buf[1] != 3) return DEVICE_EIO;

	// buf[0] is bLength; payload is UTF-16LE starting at buf+2
	const size_t payload = (size_t)got;
	if (payload < 2) return DEVICE_EIO;

	utf16le_to_ascii(buf + 2, payload - 2, out, out_sz);
	return DEVICE_OK;
}

// change signature to accept manufacturer string
static void dump_dev_desc(const uint8_t d[18], const char *mfg, const char *prod) {
	printf("Device Descriptor:\n");
	printf("  bcdUSB       : %x.%02x\n", d[3], d[2]);
	printf("  class/sub/pro: %u/%u/%u\n", d[4], d[5], d[6]);
	printf("  Max Packet   : %u\n", d[7]);
	printf("  idVendor     : 0x%04x\n", le16(&d[8]));
	printf("  idProduct    : 0x%04x\n", le16(&d[10]));
	printf("  bcdDevice    : 0x%04x\n", le16(&d[12]));
	printf("  Manufacturer : %s\n", mfg);
	printf("  Product      : %s\n", prod);
	printf("  num configs  : %u\n", d[17]);
}

static void parse_cfg(const uint8_t *cfg, const size_t n) {

	if (!cfg) return;
	printf("Configuration descriptors (%zu bytes)\n", n);

	size_t i = 0;
	while (i + 2 <= n) {
		const uint8_t len = cfg[i + 1];
		const uint8_t type = cfg[i + 2];
		if (len == 0 || i + len > n) break;

		// CONFIG descriptor
		if (type == 2 && len >= 9) {
			printf("Configuration: wTotalLength = %u, numInterfaces = %u", le16(&cfg[i + 2]), cfg[i + 4]);

			// INTERFACE
		} else if (type == 4 && len >= 9) {
			printf("INTERFACE %u: alt=%u, eps=%u, class=%u/%u/%u",
				cfg[i + 2], cfg[i + 3], cfg[i +4],
				cfg[i + 5], cfg[i +6], cfg[i + 7]);

			// ENDPOINTS
		} else if (type == 5 && len >= 7) {
			uint8_t ep = cfg[i + 2];
			const uint8_t attr = cfg[i + 3];
			uint8_t mps = le16(&cfg[i + 4]);
			uint8_t ivl = cfg[i + 6];

			const char *dir = (attr & 0x80) ? "IN" : "OUT";
			uint8_t xfer = attr & 0x30;
			const char *t = (xfer == 0) ? "CTRL" :
							(xfer == 1) ? "ISOC" :
							(xfer == 3) ? "BULK" : "INTR";

			printf("    EP 0x%02x (%s): %s, mps=%u, interval=%u\n",
			ep, dir, t, mps, ivl);
		}
		i += len;
	}


}

int main(int argc, char **argv) {
	const uint16_t vid = 0x045e, pid = 0x02b0;

	 // create device
	 device_host *host = NULL;
	 CHECK(device_host_create(&host));

	 // scan for devices
	 device_id *ids = NULL;
	 size_t n = 0;
	 CHECK(device_host_scan(host, vid, pid, &ids, &n));

	 // free handler from memory if no devices were found and terminate program
	 if (n == 0) {
		printf("No devices found (vid=%04x pid=%04x)", vid, pid);
		device_host_destroy(host);
		return 2;
	 }

	 // enumerate devices
	 printf("Found %zu device(s). Opening...\n"
		 "Bus %03u Device=%03u: ID %04x:%04x\n", n, ids[0].bus, ids[0].addr, ids[0].vid, ids[0].pid);

	 // initialize libusb session
	 device_link *link = NULL;
	 CHECK(device_link_open(host, &ids[0], &link));

	 // configuration descriptor
	 uint8_t devd[18] = {0};
	 int got = device_link_ctrl(link, 0x80, 0x06, (1u << 8), 0, devd, sizeof(devd), 1000);
	 if (got < 0) die("GET_CONFIGURATION(device)", got);
	 if (got != 18) fprintf(stderr, "Warning: device descriptor length=%d\n", got);

	char mfg[128] = {0};
	char prd[128] = {0};

	(void)get_string_ascii(link, 0x0409, devd[14], mfg, sizeof mfg);
	get_string_ascii(link, 0x0409, devd[15], prd, sizeof prd);

	dump_dev_desc(devd, mfg, prd);

	device_link_close(link);
	device_ids_destroy(ids);
	device_host_destroy(host);
}
