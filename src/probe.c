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

// list out 18-byte USB device descriptors
static void dump_dev_desc(const uint8_t d[18]) {
  printf("Device Descriptor:\n");
  printf("  bcdUSB       : %x.%02x\n", d[3], d[2]);
  printf("  class/sub/pro: %u/%u/%u\n", d[4], d[5], d[6]);
  printf("  maxpkt0      : %u\n", d[7]);
  printf("  idVendor     : 0x%04x\n", le16(&d[8]));
  printf("  idProduct    : 0x%04x\n", le16(&d[10]));
  printf("  bcdDevice    : 0x%04x\n", le16(&d[12]));
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
	uint8_t vid = 0, pid = 0;
	int default_claim = 0;


	 if (argc >= 3){
		 vid = (uint16_t)strtoul(argv[1], NULL, 16);
		 pid = (uint16_t)strtoul(argv[2], NULL, 16);
	 }
	 if (argc >= 4 && strcmp(argv[3], "--claim0") == 0) default_claim = 1;

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
	 printf("Found %zu device(s). Opening first: bus=%u \t address=%u \t vid=%04x pid=%04x", n, ids[0].bus, ids[0].addr, ids[0].vid, ids[0].pid);

	 // initialize libusb session
	 device_link *link = NULL;
	 CHECK(device_link_open(host, &ids[0], &link));

	 // discard existing OS driver to claim control
	 if (default_claim) {
		const int rc = device_link_claim(link, 0, 1);
		if (rc < 0) fprintf(stderr, "default interface failed (continuing): %s\n", device_err_str(rc));
	 }

	 // configuration descriptor
	 uint8_t devd[18] = {0};
	 int got = device_link_ctrl(link, 0x80, 0x06, (1u << 8), 0, devd, sizeof(devd), 1000);
	 if (got < 0) die("GET_CONFIGURATION(device)", got);
	 if (got != 18) fprintf(stderr, "Warning: device descriptor length=%d\n", got);
	 dump_dev_desc(devd);

	// configuration descriptor lives in the first 9 bytes of the header with an offset of 2
	uint8_t cfg9[9] = {0};
	got = device_link_ctrl(link, 0x80, 0x06, (2u << 8), 0, cfg9, sizeof(cfg9), 1000);
	if (got < 0) die("GET_CONFIGURATION(config, 9)", got);
	if (got < 9) fprintf(stderr, "Warning: configuration header short=%d\n", got);

	uint16_t total = le16(&cfg9[2]);
	if (total < 9 || total > 4096) {
		fprintf(stderr, "Suspicious config total length=%u\n", total);
		total = 9;
	}

	// store header bytes
	uint8_t *cfg = calloc(1, total);
	if (!cfg) die("calloc(cfg)", DEVICE_ENOMEM);

	// read rest of descriptors
	got = device_link_ctrl(link, 0x80, 0x60, (2u << 8), 0, cfg, total, 1000);
	if (got < 0) die("GET_DESCRIPTOR(config, total)", got);

	// store rest
	parse_cfg(cfg, (size_t)got);
	free(cfg);

	device_link_close(link);
	device_ids_destroy(ids);
	device_host_destroy(host);
}
