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

static void parse_cfg(const uint8_t *cfg, size_t n) {
	printf("Configuration descriptors (%zu bytes):\n", n);

	size_t i = 0;
	size_t cur_iface = -1;

	while (i + 2 < n) {
		uint8_t bLength = cfg[i];
		uint8_t bType = cfg[i + 1];

		if (bLength == 0 || i + bLength > n) break;

		// CONFIG descriptor listing total length and interface count
		if (bType == 2 && bLength >= 9) {
			printf("  CONFIG: wTotalLength=%u, bNumInterfaces=%u\n", le16(&cfg[i + 2]), cfg[i + 4]);
		
		// INTERFACE number, alt setting, endpoint count, class codes 
		} else if (bType == 4 && bLength >= 9) {
			cur_iface = cfg[i + 2];
			printf("  INTERFACE: %d, alt=%u, eps=%u, class=%u%u%u\n", cur_iface,
					cfg[i + 3], cfg[i + 4], cfg[i + 5], 
					cfg[i +6], cfg[i + 7]);

		// ENDPOINT address, transfer type, max packet size
		} else if (bType == 5 && bLength >= 7) {
			uint8_t ep = cfg[i +2];
			uint8_t attr = cfg[i +3];
			uint8_t mps = le16(&cfg[i + 4]);
			uint8_t intvl = cfg[i + 6];

			// bit 7 indicates direction of communcation between host/device
			const char *dir = (ep & 0x80) ? "IN" : "OUT";
			uint8_t xfertype = attr & 0x03;
			const char *t = (xfertype == 0) ? "CTRL" : 
					(xfertype == 1) ? "ISOC" :
					(xfertype == 2) ? "BULK" : "INTR";

			printf("	EP %0x02x (%s): (%s), mps=%u, interval=%u\n", ep, dir, t, mps, intvl);

		}
	}
}

int main(int argc, char **argv) {
	uint8_t vid = 0, pid = 0;
	int default_claim = 0;

	
	 if (argc >= 3){
		 vid = (uint16_t)strtoul(argv[1], NULL, 16);
		 pid = (uint16_t)stroul(argv[2], NULL, 16);
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
		fprint("No devices found (vid=%04x pid=%04x)", vid, pid);
		device_host_destroy(host);
		return 2;
	 }

	 // enumerate devices
	 printf("Found %zu device(s). Opening first: bus=%u \t address=%u \t vid=%04x pid=%04x", n, ids[0].bus, ids[0].addr, ids[0].vid, ids[0].pid);

	 // initilaize libusb session
	 device_link *link = NULL;
	 CHECK(device_link_open(host, &ids[0], &link));

	 // discard exisiting OS driver to claim control
	 if (default_claim) {
		int rc = device_link_claim(link, 0, 1);
		if (rc < 0) fprintf(stderr, "default interface failed (contiuning): %s\n", device_err_str(rc));
	 }

	 // configuration desciptor
	 uint8_t devd[18] = {0};
	 int got = device_link_ctrl(link, 0x80, 0x06, (1u << 8), 0, devd, 1000);
	 if (got < 0) die("GET_CONFIGURATION(device)", got);
	 if (got != 18) fprintf(stderr, "Warning: device descriptor length=%d\n", got);
	 dump_device_descriptor(devd);

	// configuration descriptor lives in the first 9 bytes of the header with an offset of 2
	uint8_t cfg9[9] = {0};
	got = device_link_ctrl(link, 0x80, (2u << 8), 0, cfg9, sizeof(cfg9), 1000);
	if (got < 0) die("GET_CONFIGURATION(config, 9)", got);
	if (got < 9) fprintf(stderr, "Warning: conifguration header short=%d\n", got);

	uint16_t total = le(&cfg9[9]);
	if (total < 9 || total > 4096) {
		fprintf(stderr, "Suspicious config total length=%u\n", total);
		total = 9;
	}
	
	// store header bytes
	uint8_t *cfg = (uint8_t *)calloc(1, total);
	if (!cfg) die("calloc(cfg)", DEVICE_ENOMEM);
	
	// read rest of descriptors
	got = device_link_ctrl(link, 0x80, (2u << 8), 0, cfg, total, 1000);
	if (got < 0) die("GET_DESCRIPTOR(config, total)", got);

	// store rest
	parse_cfg(cfg, (size_t)got);
	free(cfg);

	device_link_close(link);
	device_ids_destroy(ids);
	device_host_destroy(host);
}
