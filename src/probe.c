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

#define CHECK(call) do {
	int _rc = (call);
	if (_rc < 0) die(#call, _rc);
} while(0)

static uint16_t le16(const uint8_t *p) {
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

