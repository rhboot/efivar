// SPDX-License-Identifier: LGPL-2.1-or-later
/*
 * test-make-dp.c - test making device paths
 * Copyright Peter Jones <pjones@redhat.com>
 */

#include "efivar/efivar.h" // IWYU pragma: keep

#include "test.h"

#include "gpt.h"

#include <stdlib.h>

static void
print_one_efi_error(char * const filename,
		    char * const function,
		    int line,
		    char * const message,
		    int error)
{
	printf("%s %s():%d error:%d %s\n",
	       filename, function, line, error, message);
}

static void
print_efi_error(void)
{
	int rc = 1;
	for (unsigned int n = 0; rc == 1 && n < 255; n++) {
		char * filename = NULL;
		char * function = NULL;
		char * message = NULL;
		int line = 0;
		int error = 0;

		rc = efi_error_get(n, &filename, &function, &line, &message, &error);
		if (rc > 0)
			print_one_efi_error(filename, function, line, message, error);
	}
}

static void
efi_error_pop_all(void)
{
	int rc = 1;
	do {
		char * filename = NULL;
		char * function = NULL;
		char * message = NULL;
		int line = 0;
		int error = 0;

		efi_error_pop();

		rc = efi_error_get(0, &filename, &function, &line, &message, &error);
	} while (rc > 0);
}

#define TEST_PREAMBLE \
	ssize_t sz = 0;						\
	ssize_t dnsz = 0;					\
	size_t insz = 0;					\
	uint8_t *buf = NULL, *bufp = NULL;			\
	const size_t bufsz = 4096;				\
	int rc;							\
	char *file, *func, *msg;				\
	int line, error;					\
	int ret = -1;						\
	const size_t outbufsz = 4096;				\
	ssize_t outsize = 0;					\
	unsigned char *outbuf = NULL, *outbufp = NULL;		\
								\
	efi_error_pop_all();					\
								\
	buf = aligned_alloc(bufsz, bufsz);			\
	assert_nonzero_goto(buf, out, "got %p: %m\n");		\
	memset(buf, 0, bufsz);					\
								\
	outbuf = aligned_alloc(outbufsz, outbufsz);		\
	assert_nonzero_goto(outbuf, out, "got %p: %m\n");	\
	memset(outbuf, 0, outbufsz);				\
	outbufp = outbuf;					\
	assert_nonzero_goto(outbufp, out, "got %p\n");

#define TEST_POSTAMBLE						\
	ret = 0;						\
out:								\
	if (buf)						\
		free(buf);					\
	if (outbuf)						\
		free(outbuf);					\
	efi_error_pop_all();					\
	return ret;

#define TEST_RETURNS(szexpr, makefn, args...)							\
	dnsz = szexpr;										\
												\
	insz = 0;										\
	bufp = NULL;										\
	sz = makefn(bufp, insz, ## args);							\
	assert_equal_goto(sz, dnsz, out, "got %zd expected %zd\n");				\
												\
	insz = dnsz - 1;									\
	bufp = &buf[bufsz - insz];								\
	sz = makefn(bufp, insz, ## args);							\
	assert_negative_goto(sz, out, "\n");							\
	efi_error_pop_all();									\
												\
	insz = dnsz;										\
	bufp = &buf[bufsz - insz];								\
	sz = makefn(bufp, insz, ## args);							\
	assert_equal_goto(sz, dnsz, out, "got %zd expected %zd\n");				\
	efi_error_pop_all();									\
												\
	insz = dnsz + 1;									\
	bufp = &buf[bufsz - insz];								\
	sz = makefn(bufp, insz, ## args);							\
	assert_equal_goto(sz, dnsz, out, "got %zd expected %zd\n");				\
	efi_error_pop_all();

#define TEST_INVALID_ARG(makefn, args...)							\
	insz = dnsz;										\
	bufp = &buf[bufsz - insz];								\
	sz = makefn(bufp, insz, ## args);							\
	assert_negative_goto(sz, out, "\n");							\
	efi_error_pop_all();

#define __TEST_APPEND_END(ctr, makefn, args...)							\
	insz = dnsz;										\
	bufp = &buf[bufsz - insz - sizeof(efidp_header)];					\
	sz = makefn(bufp, insz, ## args);							\
	assert_equal_goto(sz, dnsz, out, "got %zd expected %zd\n");				\
	efi_error_pop_all();									\
												\
	const_efidp __CONCAT(next, ctr);							\
	rc = efidp_next_node((efidp)bufp, &__CONCAT(next, ctr));				\
	assert_positive_goto(rc, out, "\n");							\
												\
	sz = efidp_make_end_entire((uint8_t *)__CONCAT(next, ctr), 4);				\
	assert_equal_goto(sz, sizeof(efidp_header), out, "got sz %zd expected %zu\n");

#define _TEST_APPEND_END(ctr, makefn, args...)							\
	__TEST_APPEND_END(ctr, makefn, ## args)

#define TEST_APPEND_END(makefn, args...)							\
	_TEST_APPEND_END(__COUNTER__, makefn, ## args)

#define __TEST_FORMAT(ctr, _dp, expected_str_result)						\
	char __CONCAT(expected_format, ctr) [] = expected_str_result;				\
												\
	outsize = sizeof(__CONCAT(expected_format, ctr));					\
	outbufp = &outbuf[outbufsz - outsize];							\
	sz = efidp_format_device_path((uint8_t *)outbufp, outsize, _dp, insz);			\
	assert_equal_goto(sz, outsize, out, "got %zd expected %zd: \"%s\"\n", outbufp);		\
												\
	sz = strlen((char *)outbufp);								\
	assert_equal_goto(sz, outsize-1, out, "got %zd expected %zd: \"%s\"\n", outbufp);	\
												\
	rc = strcmp((char *)outbufp, __CONCAT(expected_format, ctr));				\
	assert_zero_goto(rc, out, "got %d comparing \"%s\" to \"%s\"\n",			\
			 (char *)outbufp, __CONCAT(expected_format, ctr));			\
												\
	rc = efi_error_get(0, &file, &func, &line, &msg, &error);				\
	if (rc != 0)										\
		print_efi_error();								\
	assert_zero_goto(rc, out, " got %d\n");

#define _TEST_FORMAT(ctr, _dp, expected_str_result)						\
	__TEST_FORMAT(ctr, _dp, expected_str_result)

#define TEST_FORMAT(_dp, expected_str_result) \
	_TEST_FORMAT(__COUNTER__, _dp, expected_str_result)

#pragma GCC diagnostic ignored "-Wunused-variable"

static int
test_make_generic(void)
{
	TEST_PREAMBLE;

	size_t node_size;

	node_size = sizeof(efidp_header);
	bufp = &buf[bufsz - node_size];
	sz = efidp_make_generic(bufp, bufsz, 0, 1, node_size);
	assert_equal_goto(sz, 4, out, "got %zd expected %d\n");

	node_size = sizeof(efidp_header) + 1;
	bufp = &buf[bufsz - node_size];
	sz = efidp_make_generic(bufp, bufsz, 0, 1, node_size);
	assert_equal_goto(sz, 5, out, "got %zd expected %d\n");

	node_size = sizeof(efidp_header) + 4;
	bufp = &buf[bufsz - node_size];
	sz = efidp_make_generic(bufp, bufsz, 0, 1, node_size);
	assert_equal_goto(sz, 8, out, "got %zd expected %d\n");

	node_size = sizeof(efidp_header) + 5;
	bufp = &buf[bufsz - node_size];
	sz = efidp_make_generic(bufp, bufsz, 0, 1, node_size);
	assert_equal_goto(sz, 9, out, "got %zd expected %d\n");

	rc = efi_error_get(0, &file, &func, &line, &msg, &error);
	if (rc != 0)
		print_efi_error();
	assert_zero_goto(rc, out, "got %d\n");

	TEST_POSTAMBLE;
}

#pragma GCC diagnostic error "-Wunused-variable"

static int
test_make_acpi_hid()
{
	TEST_PREAMBLE;

	TEST_RETURNS(sizeof(efidp_acpi_hid), efidp_make_acpi_hid, 1, 1);

	TEST_APPEND_END(efidp_make_acpi_hid, 1, 1);
	TEST_FORMAT((efidp)bufp, "Acpi(0x1,0x1)");

	TEST_APPEND_END(efidp_make_acpi_hid, EFIDP_ACPI_PCI_ROOT_HID, 0x5a5a5a5a);
	TEST_FORMAT((efidp)bufp, "PciRoot(0x5a5a5a5a)");

	TEST_APPEND_END(efidp_make_acpi_hid, EFIDP_ACPI_CONTAINER_0A05_HID, 0x5a5a5a5a);
	TEST_FORMAT((efidp)bufp, "AcpiContainer()");

	TEST_APPEND_END(efidp_make_acpi_hid, EFIDP_ACPI_CONTAINER_0A06_HID, 0x5a5a5a5a);
	TEST_FORMAT((efidp)bufp, "AcpiContainer()");

	TEST_APPEND_END(efidp_make_acpi_hid, EFIDP_ACPI_PCIE_ROOT_HID, 0x5a5a5a5a);
	TEST_FORMAT((efidp)bufp, "PcieRoot(0x5a5a5a5a)");

	TEST_APPEND_END(efidp_make_acpi_hid, EFIDP_ACPI_EC_HID, 0x5a5a5a5a);
	TEST_FORMAT((efidp)bufp, "EmbeddedController()");

	TEST_APPEND_END(efidp_make_acpi_hid, EFIDP_ACPI_FLOPPY_HID, 0x5a5a5a5a);
	TEST_FORMAT((efidp)bufp, "Floppy(0x5a5a5a5a)");

	TEST_APPEND_END(efidp_make_acpi_hid, EFIDP_ACPI_KEYBOARD_HID, 0x5a5a5a5a);
	TEST_FORMAT((efidp)bufp, "Keyboard(0x5a5a5a5a)");

	TEST_APPEND_END(efidp_make_acpi_hid, EFIDP_ACPI_SERIAL_HID, 0x5a5a5a5a);
	TEST_FORMAT((efidp)bufp, "Serial(0x5a5a5a5a)");

	TEST_APPEND_END(efidp_make_acpi_hid, EFIDP_ACPI_PARALLEL_HID, 0x5a5a5a5a);
	TEST_FORMAT((efidp)bufp, "Parallel(0x5a5a5a5a)");

	/* XXX: add an nvdimm test */

	TEST_APPEND_END(efidp_make_acpi_hid, 0xa5a5a5a5, 0x5a5a5a5a);
	TEST_FORMAT((efidp)bufp, "Acpi(0xa5a5a5a5,0x5a5a5a5a)");

	TEST_POSTAMBLE;
}

static int
test_make_atapi()
{
	TEST_PREAMBLE;

	TEST_RETURNS(sizeof(efidp_atapi), efidp_make_atapi, 1, 2, 3);

	TEST_APPEND_END(efidp_make_atapi, 1, 2, 3);
	TEST_FORMAT((efidp)bufp, "Ata(1,2,3)");

	TEST_POSTAMBLE;
}

static int
test_make_bios_boot()
{
	TEST_PREAMBLE;

	char *descs[] = {
		"",
		"A:",
		"C:",
		"D:",
		"E:",
		"F:",
		"network",
		"???"
	};

	TEST_RETURNS(sizeof(efidp_bios_boot) + strlen(descs[1]) + 1, efidp_make_bios_boot,
		     EFIDP_BIOS_BOOT_DEVICE_TYPE_FLOPPY, 0, (uint8_t *)descs[1]);

	TEST_APPEND_END(efidp_make_bios_boot, EFIDP_BIOS_BOOT_DEVICE_TYPE_FLOPPY, 0, (uint8_t *)descs[1]);
	TEST_FORMAT((efidp)bufp, "BBS(Floppy,A:,0x0)");

	TEST_RETURNS(sizeof(efidp_bios_boot) + strlen(descs[2]) + 1, efidp_make_bios_boot,
		     EFIDP_BIOS_BOOT_DEVICE_TYPE_HD, 0, (uint8_t *)descs[2]);

	TEST_APPEND_END(efidp_make_bios_boot, EFIDP_BIOS_BOOT_DEVICE_TYPE_HD, 0, (uint8_t *)descs[2]);
	TEST_FORMAT((efidp)bufp, "BBS(HD,C:,0x0)");

	TEST_RETURNS(sizeof(efidp_bios_boot) + strlen(descs[3]) + 1, efidp_make_bios_boot,
		     EFIDP_BIOS_BOOT_DEVICE_TYPE_CDROM, 0, (uint8_t *)descs[3]);

	TEST_APPEND_END(efidp_make_bios_boot, EFIDP_BIOS_BOOT_DEVICE_TYPE_CDROM, 0, (uint8_t *)descs[3]);
	TEST_FORMAT((efidp)bufp, "BBS(CDROM,D:,0x0)");

	TEST_RETURNS(sizeof(efidp_bios_boot) + strlen(descs[4]) + 1, efidp_make_bios_boot,
		     EFIDP_BIOS_BOOT_DEVICE_TYPE_PCMCIA, 0, (uint8_t *)descs[4]);

	TEST_APPEND_END(efidp_make_bios_boot, EFIDP_BIOS_BOOT_DEVICE_TYPE_PCMCIA, 0, (uint8_t *)descs[4]);
	TEST_FORMAT((efidp)bufp, "BBS(PCMCIA,E:,0x0)");

	TEST_RETURNS(sizeof(efidp_bios_boot) + strlen(descs[5]) + 1, efidp_make_bios_boot,
		     EFIDP_BIOS_BOOT_DEVICE_TYPE_USB, 0, (uint8_t *)descs[5]);

	TEST_APPEND_END(efidp_make_bios_boot, EFIDP_BIOS_BOOT_DEVICE_TYPE_USB, 0, (uint8_t *)descs[5]);
	TEST_FORMAT((efidp)bufp, "BBS(USB,F:,0x0)");

	TEST_RETURNS(sizeof(efidp_bios_boot) + strlen(descs[6]) + 1, efidp_make_bios_boot,
		     EFIDP_BIOS_BOOT_DEVICE_TYPE_EMBEDDED_NET, 0, (uint8_t *)descs[6]);

	TEST_APPEND_END(efidp_make_bios_boot, EFIDP_BIOS_BOOT_DEVICE_TYPE_EMBEDDED_NET, 0, (uint8_t *)descs[6]);
	TEST_FORMAT((efidp)bufp, "BBS(Network,network,0x0)");

	TEST_RETURNS(sizeof(efidp_bios_boot) + strlen(descs[7]) + 1, efidp_make_bios_boot,
		     EFIDP_BIOS_BOOT_DEVICE_TYPE_UNKNOWN, 0, (uint8_t *)descs[7]);

	TEST_APPEND_END(efidp_make_bios_boot, EFIDP_BIOS_BOOT_DEVICE_TYPE_UNKNOWN, 0, (uint8_t *)descs[7]);
	TEST_FORMAT((efidp)bufp, "BBS(255,???,0x0)");

	TEST_POSTAMBLE;
}

static int
test_make_edd10()
{
	TEST_PREAMBLE;

	TEST_RETURNS(sizeof(efidp_edd10), efidp_make_edd10, 1);

	TEST_APPEND_END(efidp_make_edd10, 1);
	TEST_FORMAT((efidp)bufp, "EDD10(0x1)");

	TEST_POSTAMBLE;
}

static int
test_make_emmc()
{
	TEST_PREAMBLE;

	TEST_RETURNS(sizeof(efidp_emmc), efidp_make_emmc, 1);

	TEST_APPEND_END(efidp_make_emmc, 1);
	TEST_FORMAT((efidp)bufp, "eMMC(1)");

	TEST_POSTAMBLE;
}

/*
 * This one is special: 1) we can't use the normal "append an end marker"
 * technique, 2) it tests the efi_get_error() path as well.
 */
static int
test_make_end_entire(void)
{
	TEST_PREAMBLE;

	dnsz = sizeof(efidp_header);
	bufp = NULL;
	sz = efidp_make_end_instance(bufp, 0);
	assert_equal_goto(sz, dnsz, out, "got %zd expected %zd\n");

	sz = efidp_make_end_instance(bufp, 1);
	assert_negative_goto(sz, out, "\n");

	sz = efidp_make_end_instance(bufp, 2);
	assert_negative_goto(sz, out, "\n");

	printf("this should print at least two errors:\n");
	print_efi_error();
	efi_error_pop_all();

	TEST_RETURNS(sizeof(efidp_header), efidp_make_end_entire);

	insz = dnsz;
	bufp = &buf[bufsz - insz];
	sz = efidp_make_end_entire(bufp, insz);
	assert_equal_goto(sz, dnsz, out, "got %zd expected %zd\n");
	efi_error_pop_all();

	char expected_format[] = "";

	outsize = sizeof(expected_format);
	outbufp = &outbuf[outbufsz - outsize];
	sz = efidp_format_device_path((uint8_t *)outbufp, outsize, (const_efidp)bufp, insz);
	assert_equal_goto(sz, outsize, out, "got %zd expected %zd\n");

	sz = strlen((char *)outbufp);
	assert_equal_goto(sz, outsize - 1, out, "got %zd expected %zd\n");

	rc = strcmp((char *)outbufp, expected_format);
	assert_zero_goto(rc, out, "got %d\n");

	rc = efi_error_get(0, &file, &func, &line, &msg, &error);
	if (rc != 0)
		print_efi_error();
	assert_zero_goto(rc, out, " got %d\n");

	TEST_POSTAMBLE;
}

static int
test_make_end_instance(void)
{
	TEST_PREAMBLE;

	TEST_RETURNS(sizeof(efidp_header), efidp_make_end_instance);

	TEST_APPEND_END(efidp_make_end_instance);
	TEST_FORMAT((efidp)bufp, ",");

	TEST_POSTAMBLE;
}

static int
test_make_file(void)
{
	TEST_PREAMBLE;

	char teststr[] = "ééé";

	/*
	 * Is this actually reliably the right size with other strings?  Who knows...
	 */
	TEST_RETURNS(sizeof(efidp_file) + sizeof(teststr) + 1, efidp_make_file, teststr);

	TEST_APPEND_END(efidp_make_file, teststr);
	TEST_FORMAT((efidp)bufp, "ééé");

	TEST_POSTAMBLE;
}

int
test_make_hd(void)
{
	TEST_PREAMBLE;

	partition_signature_t signature = { 0, };

	TEST_RETURNS(sizeof(efidp_hd), efidp_make_hd, 0, 1024, 4096, (uint8_t *)&signature, 0, EFIDP_HD_SIGNATURE_NONE);

	TEST_APPEND_END(efidp_make_hd, 0, 1024, 4096, (uint8_t *)&signature, 0, EFIDP_HD_SIGNATURE_NONE);
	TEST_FORMAT((efidp)bufp, "HD(0,0,00000000000000000000000000000000,0x400,0x1000)");

	TEST_POSTAMBLE;
}

static int
test_make_hw_vendor(void)
{
	TEST_PREAMBLE;

	char teststr[] = "éééa";
	efi_guid_t vendor_guid = efi_guid_redhat_2;

	TEST_RETURNS(sizeof(efidp_hw_vendor) + sizeof(teststr), efidp_make_hw_vendor,
		     vendor_guid, teststr, sizeof(teststr));

	TEST_APPEND_END(efidp_make_hw_vendor, vendor_guid, teststr, sizeof(teststr));
	TEST_FORMAT((efidp)bufp, "VenHw(0223eddb-9079-4388-af77-2d65b1c35d3b,c3a9c3a9c3a96100)");

	TEST_POSTAMBLE;
}

static int
test_make_ipv4()
{
	TEST_PREAMBLE;

	uint32_t local = 0x0a000001;
	uint32_t remote = 0x0a000101;
	uint32_t gateway = 0x0a000002;
	uint32_t netmask = 0xff000000;
	uint32_t local_port = 1025;
	uint32_t remote_port = 443;
	uint16_t protocol = 0;
	int is_static = 0;

	/*
	 * In a few examples, the spec says this string:
	 *
	 * IPv4(192.168.0.100,TCP,Static,192.168.0.1)
	 *
	 * should represent:
	 *
	 * 03				dp.header.type = messaging
	 * 0c				dp.header.subtype = ipv4
	 * 1b 00			dp.header.length = 27 (0x001b)
	 * c0 a8 00 01			dp.ipv4.local = 192.168.0.1
	 * c0 a8 00 64			dp.ipv4.remote = 192.168.0.100
	 * 00 00			dp.ipv4.local_port = 0
	 * 0c bc			dp.ipv4.rempote_port = 3260
	 * 00 06			dp.ipv4.protocol = 6 (tcp)
	 * 01				dp.ipv4.static = true
	 * 00 00 00 00			dp.ipv4.gateway = 0.0.0.0
	 * 00 00 00 00			dp.ipv4.subnet_mask = 0.0.0.0
	 *
	 * Which I think is a bit junk, as surely nonzero ports were meant
	 * to be included.  Elsewhere it says:
	 *
	 *   IPv4 Address	Series of four integer values (each between 0-255),
	 *			separated by a ‘.’  Optionally, followed by a ‘:’
	 *			and an integer value between 0-65555. If the ‘:’
	 *			is not present, then the port value is zero.
	 *
	 * and:
	 *
	 *   IPv4(RemoteIp, Protocol, Type, LocalIp, GatewayIPAddress, SubnetMask)
	 *   IPv4(RemoteIp) (Display Only)
	 *
	 * So at the least that should have been:
	 *
	 * IPv4(192.168.0.100:3260,TCP,Static,192.168.0.1)
	 *
	 */

	TEST_RETURNS(sizeof(efidp_ipv4_addr), efidp_make_ipv4,
		     local, remote, gateway, netmask, local_port, remote_port, protocol, is_static);

	TEST_APPEND_END(efidp_make_ipv4,
		     local, remote, gateway, netmask, local_port, remote_port, protocol, is_static);
	TEST_FORMAT((efidp)bufp, "IPv4(10.0.1.1:443,0,DHCP,10.0.0.1:1025,10.0.0.2,255.0.0.0)");

	is_static = 1;
	TEST_APPEND_END(efidp_make_ipv4,
		     local, remote, gateway, netmask, local_port, remote_port, protocol, is_static);
	TEST_FORMAT((efidp)bufp, "IPv4(10.0.1.1:443,0,Static,10.0.0.1:1025,10.0.0.2,255.0.0.0)");

	protocol = 6;
	TEST_APPEND_END(efidp_make_ipv4,
		     local, remote, gateway, netmask, local_port, remote_port, protocol, is_static);
	TEST_FORMAT((efidp)bufp, "IPv4(10.0.1.1:443,TCP,Static,10.0.0.1:1025,10.0.0.2,255.0.0.0)");

	protocol = 17;
	TEST_APPEND_END(efidp_make_ipv4,
		     local, remote, gateway, netmask, local_port, remote_port, protocol, is_static);
	TEST_FORMAT((efidp)bufp, "IPv4(10.0.1.1:443,UDP,Static,10.0.0.1:1025,10.0.0.2,255.0.0.0)");

	protocol = 12345;
	TEST_APPEND_END(efidp_make_ipv4,
		     local, remote, gateway, netmask, local_port, remote_port, protocol, is_static);
	TEST_FORMAT((efidp)bufp, "IPv4(10.0.1.1:443,12345,Static,10.0.0.1:1025,10.0.0.2,255.0.0.0)");

	TEST_POSTAMBLE;
}

/*
 * In a few examples, the spec says this string:
 *
 * IPv6(2001:4898:000A:1005:95A6:EE6C:BED3:4859,TCPDHCP,2001:4898:000A:1005:0215:17FF:FE21:5593)
 *
 * should represent:
 *
 * 03				dp.header.type = messaging
 * 0c				dp.header.subtype = ipv6
 * 3c 00			dp.header.length = 60 (0x003c)
 * 20 01 48 98 00 0a 10 05	dp.ipv6.local =
 * 02 15 17 ff fe 21 55 93	  2001:4898:000a:1005:0215:17ff:fe21:5593
 * 20 01 48 98 00 0a 10 05	dp.ipv6.remote =
 * 95 a6 ee 6c be d3 48 59	  2001:4989:000a:1005:95a6:ee6c:bed3:4859
 * 00 00			dp.ipv6.local_port = 0
 * 0c bc			dp.ipv6.remote_port = 3260 (0x0cbc)
 * 00 06			dp.ipv6.protocol = 6 (tcp)
 * 01				dp.ipv6.origin = 1 (dhcp)
 * 00				dp.ipv6.prefix_length = 0
 * 00 00 00 00 00 00 00 00	dp.ipv6.gateway = [0::0]
 * 00 00 00 00 00 00 00 00
 *
 * Ignoring the missing comma (typo), this looks wrong with respect to
 * other parts, which say:
 *
 *   IPv6 Address		IPv6 Address is expressed in the format
 *				[address]:port. The ‘address’ is expressed
 *				in the way defined in RFC4291 Section 2.2.
 *				The ‘:port’ after the [address] is
 *				optional. If present, the ‘port’ is an
 *				integer value between 0-65535 to represent
 *				the port number, or else, port number is
 *				zero.
 *
 * and
 *
 *   IPv6(RemoteIp, Protocol, IPAddressOrigin, LocalIp, GatewayIPAddress, SubnetMask)
 *   IPv6(RemoteIp) (Display Only)
 *
 * I think "SubnetMask" should have been PrefixLength.  The spec also says
 * GatewayAddress and PrefixLength are optional and can be ommitted, though
 * I assume the latter depends on the former.
 *
 * So that data should be expressed as:
 *
 * IPv6([2001:4898:000A:1005:95A6:EE6C:BED3:4859],TCP,DHCP,
 *      [2001:4898:000A:1005:0215:17FF:FE21:5593]:3260)
 *
 * or
 *
 * IPv6([2001:4898:000A:1005:95A6:EE6C:BED3:4859],TCP,DHCP,
 *      [2001:4898:000A:1005:0215:17FF:FE21:5593]:3260,[0::0],0)
 */
static int
test_make_ipv6()
{
	TEST_PREAMBLE;

	uint8_t		local[16] = { 0xfd, 0x00, 0x16, 0x00,
				      0x00, 0xff, 0x00, 0x00,
				      0x00, 0x01, 0xff, 0x00,
				      0x00, 0x00, 0x00, 0x01 };
	uint8_t		remote[16] = { 0xfd, 0x24, 0x16, 0xe2,
				       0x03, 0x1c, 0x26, 0x01,
				       0x00, 0x00, 0x00, 0x00,
				       0x00, 0x00, 0x00, 0x02 };
	uint16_t	local_port = 1025;
	uint16_t	remote_port = 443;
	uint16_t	protocol = 5;
	uint8_t		origin = EFIDP_IPv6_ORIGIN_MANUAL;
	uint8_t		prefix_length = 128;
	uint8_t		gateway[16] = { 0xfd, 0x24, 0x16, 0xe2,
					0x03, 0x1c, 0x26, 0x01,
					0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0xff};

	TEST_RETURNS(sizeof(efidp_ipv6_addr), efidp_make_ipv6,
		     local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);

	TEST_APPEND_END(efidp_make_ipv6, local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);
	TEST_FORMAT((efidp)bufp, "IPv6([fd24:16e2:31c:2601::2]:443,"
				 "5,Static,"
				 "[fd00:1600:ff:0:1:ff00:0:1]:1025,"
				 "[fd24:16e2:31c:2601::ff],128)");

	memset(gateway, 0, sizeof(gateway));
	TEST_APPEND_END(efidp_make_ipv6, local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);
	TEST_FORMAT((efidp)bufp, "IPv6([fd24:16e2:31c:2601::2]:443,"
				 "5,Static,"
				 "[fd00:1600:ff:0:1:ff00:0:1]:1025,"
				 "[0::0],128)");

	prefix_length = 0;
	TEST_APPEND_END(efidp_make_ipv6, local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);
	TEST_FORMAT((efidp)bufp, "IPv6([fd24:16e2:31c:2601::2]:443,"
				 "5,Static,"
				 "[fd00:1600:ff:0:1:ff00:0:1]:1025)");

	protocol = 6;
	TEST_APPEND_END(efidp_make_ipv6, local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);
	TEST_FORMAT((efidp)bufp, "IPv6([fd24:16e2:31c:2601::2]:443,"
				 "TCP,Static,"
				 "[fd00:1600:ff:0:1:ff00:0:1]:1025)");

	protocol = 17;
	TEST_APPEND_END(efidp_make_ipv6, local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);
	TEST_FORMAT((efidp)bufp, "IPv6([fd24:16e2:31c:2601::2]:443,"
				 "UDP,Static,"
				 "[fd00:1600:ff:0:1:ff00:0:1]:1025)");

	protocol = 12345;
	TEST_APPEND_END(efidp_make_ipv6, local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);
	TEST_FORMAT((efidp)bufp, "IPv6([fd24:16e2:31c:2601::2]:443,"
				 "12345,Static,"
				 "[fd00:1600:ff:0:1:ff00:0:1]:1025)");

	origin = EFIDP_IPv6_ORIGIN_AUTOCONF;
	TEST_APPEND_END(efidp_make_ipv6, local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);
	TEST_FORMAT((efidp)bufp, "IPv6([fd24:16e2:31c:2601::2]:443,"
				 "12345,StatelessAutoConfigure,"
				 "[fd00:1600:ff:0:1:ff00:0:1]:1025)");

	origin = EFIDP_IPv6_ORIGIN_STATEFUL;
	TEST_APPEND_END(efidp_make_ipv6, local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);
	TEST_FORMAT((efidp)bufp, "IPv6([fd24:16e2:31c:2601::2]:443,"
				 "12345,StatefulAutoConfigure,"
				 "[fd00:1600:ff:0:1:ff00:0:1]:1025)");

	origin = 3;
	TEST_APPEND_END(efidp_make_ipv6, local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);
	TEST_FORMAT((efidp)bufp, "IPv6([fd24:16e2:31c:2601::2]:443,"
				 "12345,"
				 "[fd00:1600:ff:0:1:ff00:0:1]:1025)");

	local_port = 0;
	TEST_APPEND_END(efidp_make_ipv6, local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);
	TEST_FORMAT((efidp)bufp, "IPv6([fd24:16e2:31c:2601::2]:443,"
				 "12345,"
				 "[fd00:1600:ff:0:1:ff00:0:1])");

	remote_port = 0;
	TEST_APPEND_END(efidp_make_ipv6, local, remote, local_port, remote_port, protocol, origin, prefix_length, gateway);
	TEST_FORMAT((efidp)bufp, "IPv6([fd24:16e2:31c:2601::2],"
				 "12345,"
				 "[fd00:1600:ff:0:1:ff00:0:1])");

	TEST_POSTAMBLE;
}

static int
test_make_mac_addr()
{
	TEST_PREAMBLE;

	uint8_t mac_addr[] = { 0xc0, 0xff, 0xee, 0xc0, 0xff, 0xee };

	TEST_RETURNS(sizeof(efidp_mac_addr), efidp_make_mac_addr, 1, mac_addr, sizeof(mac_addr));

	TEST_APPEND_END(efidp_make_mac_addr, 1, mac_addr, sizeof(mac_addr));
	TEST_FORMAT((efidp)bufp, "MAC(c0ffeec0ffee,1)");

	TEST_POSTAMBLE;
}

static int
test_make_media_vendor(void)
{
	TEST_PREAMBLE;

	char teststr[] = "éééabc";
	efi_guid_t vendor_guid = efi_guid_redhat_2;

	TEST_RETURNS(sizeof(efidp_media_vendor) + sizeof(teststr), efidp_make_media_vendor,
		     vendor_guid, teststr, sizeof(teststr));

	TEST_APPEND_END(efidp_make_media_vendor, vendor_guid, teststr, sizeof(teststr));
	TEST_FORMAT((efidp)bufp, "VenMedia(0223eddb-9079-4388-af77-2d65b1c35d3b,c3a9c3a9c3a961626300)");

	TEST_POSTAMBLE;
}

static int
test_make_msg_vendor(void)
{
	TEST_PREAMBLE;

	char teststr[] = "éééab";
	efi_guid_t vendor_guid = efi_guid_redhat_2;

	TEST_RETURNS(sizeof(efidp_msg_vendor) + sizeof(teststr), efidp_make_msg_vendor,
		     vendor_guid, teststr, sizeof(teststr));

	TEST_APPEND_END(efidp_make_msg_vendor, vendor_guid, teststr, sizeof(teststr));
	TEST_FORMAT((efidp)bufp, "VenMsg(0223eddb-9079-4388-af77-2d65b1c35d3b,c3a9c3a9c3a9616200)");

	TEST_POSTAMBLE;
}

static int
test_make_nvdimm()
{
	TEST_PREAMBLE;

	efi_guid_t guid = {0x89d4a929, 0x322b, 0x4c65, 0x982b, {0x54, 0xce, 0x9a, 0xae, 0x7e, 0x4f }};

	TEST_RETURNS(sizeof(efidp_nvdimm), efidp_make_nvdimm, &guid);

	TEST_APPEND_END(efidp_make_nvdimm, &guid);
	TEST_FORMAT((efidp)bufp, "NVDIMM(89d4a929-322b-4c65-2b98-54ce9aae7e4f)");

	TEST_POSTAMBLE;
}

int
test_make_nvme(void)
{
	TEST_PREAMBLE;

	uint8_t eui[] = {0x9d, 0x9e, 0x7f, 0x00, 0x3a, 0x48, 0x52, 0x56};

	TEST_RETURNS(sizeof(efidp_nvme), efidp_make_nvme, 0, eui);

	TEST_APPEND_END(efidp_make_nvme, 0, eui);
	TEST_FORMAT((efidp)bufp, "NVMe(0x0,9D-9E-7F-00-3A-48-52-56)");

	TEST_APPEND_END(efidp_make_nvme, 1, eui);
	TEST_FORMAT((efidp)bufp, "NVMe(0x1,9D-9E-7F-00-3A-48-52-56)");

	TEST_POSTAMBLE;
}

static int
test_make_pci()
{
	TEST_PREAMBLE;

	TEST_RETURNS(sizeof(efidp_pci), efidp_make_pci, 1, 1);

	TEST_APPEND_END(efidp_make_pci, 1, 1);
	TEST_FORMAT((efidp)bufp, "Pci(0x1,0x1)");

	TEST_POSTAMBLE;
}

int
test_make_sas(void)
{
	TEST_PREAMBLE;

	uint64_t sas_addr = 0xa871311123554d57;

	TEST_RETURNS(sizeof(efidp_sas), efidp_make_sas, sas_addr);

	TEST_APPEND_END(efidp_make_sas, sas_addr);
	TEST_FORMAT((efidp)bufp, "SAS(a871311123554d57,0,0,NoTopology)");

	TEST_POSTAMBLE;
}

static int
test_make_sata()
{
	TEST_PREAMBLE;

	TEST_RETURNS(sizeof(efidp_sata), efidp_make_sata, 1, 1, 1);

	TEST_APPEND_END(efidp_make_sata, 1, 1, 1);
	TEST_FORMAT((efidp)bufp, "Sata(1,1,1)");

	TEST_POSTAMBLE;
}

static int
test_make_scsi()
{
	TEST_PREAMBLE;

	TEST_RETURNS(sizeof(efidp_scsi), efidp_make_scsi, 1, 1);

	TEST_APPEND_END(efidp_make_scsi, 1, 1);
	TEST_FORMAT((efidp)bufp, "SCSI(1,1)");

	TEST_POSTAMBLE;
}

static int
test_make_uart()
{
	TEST_PREAMBLE;

	TEST_RETURNS(sizeof(efidp_uart), efidp_make_uart,
		     9600, 8, EFIDP_UART_PARITY_NONE, EFIDP_UART_STOP_BITS_ONE);

	TEST_APPEND_END(efidp_make_uart,
			9600, 8, EFIDP_UART_PARITY_NONE, EFIDP_UART_STOP_BITS_ONE);
	TEST_FORMAT((efidp)bufp, "Uart(9600,8,N,1)");

	TEST_INVALID_ARG(efidp_make_uart, 9600, 8, EFIDP_UART_PARITY_SPACE+1, EFIDP_UART_STOP_BITS_ONE);

	TEST_INVALID_ARG(efidp_make_uart, 9600, 8, EFIDP_UART_PARITY_SPACE, EFIDP_UART_STOP_BITS_TWO+1);

	TEST_POSTAMBLE;
}

static int
test_make_uart_flow_control()
{
	TEST_PREAMBLE;

	TEST_RETURNS(sizeof(efidp_uart_flow_control), efidp_make_uart_flow_control, 0);

	TEST_RETURNS(sizeof(efidp_uart_flow_control), efidp_make_uart_flow_control,
		     EFIDP_UART_FLOW_CONTROL_HARDWARE);

	TEST_RETURNS(sizeof(efidp_uart_flow_control), efidp_make_uart_flow_control,
		     EFIDP_UART_FLOW_CONTROL_XONXOFF);

	TEST_INVALID_ARG(efidp_make_uart_flow_control, EFIDP_UART_FLOW_CONTROL_HARDWARE|EFIDP_UART_FLOW_CONTROL_XONXOFF);

	TEST_INVALID_ARG(efidp_make_uart_flow_control, (EFIDP_UART_FLOW_CONTROL_HARDWARE|EFIDP_UART_FLOW_CONTROL_XONXOFF)<<1);

	TEST_APPEND_END(efidp_make_uart_flow_control, EFIDP_UART_FLOW_CONTROL_NONE);
	TEST_FORMAT((efidp)bufp, "UartFlowControl(None)");

	TEST_APPEND_END(efidp_make_uart_flow_control, EFIDP_UART_FLOW_CONTROL_HARDWARE);
	TEST_FORMAT((efidp)bufp, "UartFlowControl(Hardware)");

	TEST_APPEND_END(efidp_make_uart_flow_control, EFIDP_UART_FLOW_CONTROL_XONXOFF);
	TEST_FORMAT((efidp)bufp, "UartFlowControl(XonXoff)");

	TEST_POSTAMBLE;
}

static int
test_make_vendor(void)
{
	TEST_PREAMBLE;

	char teststr[] = "ééé";
	efi_guid_t vendor_guid = efi_guid_redhat_2;

	TEST_RETURNS(sizeof(efidp_hw_vendor) + sizeof(teststr), efidp_make_vendor, 0, 1, vendor_guid, teststr, sizeof(teststr));

	TEST_APPEND_END(efidp_make_vendor, 0, 1, vendor_guid, teststr, sizeof(teststr));
	TEST_FORMAT((efidp)bufp, "Path(0,1,dbed230279908843af772d65b1c35d3bc3a9c3a9c3a900)");

	TEST_POSTAMBLE;
}

#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

#define EFI_TEST_HD_SIGNATURE { 0xf7, 0x37, 0xbd, 0xc7, 0xbc, 0x0b, 0x4a, 0x26, 0xb8, 0xc7, 0xba, 0xac, 0x14, 0x31, 0xb6, 0xc0 }
#define EFI_FEDORA_SHIM_EFI_UTF8 "\\EFI\\FEDORA\\SHIM.EFI"
#define __MAKE_UNICODE(x) __CONCAT(u,x)
#define EFI_FEDORA_SHIM_EFI_UCS2 __MAKE_UNICODE(EFI_FEDORA_SHIM_EFI_UTF8)
#define ONE_MEG (1024 * 1024)
#define ONE_GIG (ONE_MEG * 1024)

static int
test_dp_functions(void)
{
	TEST_PREAMBLE;

	int16_t type;
	efidp node, dp;
	const_efidp next;
	void *nodep;

	efi_char16_t file_name[] = EFI_FEDORA_SHIM_EFI_UCS2;

	struct sample_s {
		efidp_pci pci;
		efidp_hd hd;
		struct {
			efidp_header header;
			efi_char16_t name[sizeof(file_name)/2];
		} file;
		efidp_header end;
	};

	uint8_t hd_signature[] = EFI_TEST_HD_SIGNATURE;

	const struct sample_s sample = {
		.pci = {
			.header = {
				.type = EFIDP_HARDWARE_TYPE,
				.subtype = EFIDP_HW_PCI,
				.length = sizeof(efidp_pci),
			},
			.function = 0xa5,
			.device = 0x5a,
		},
		.hd = {
			.header = {
				.type = EFIDP_MEDIA_TYPE,
				.subtype = EFIDP_MEDIA_HD,
				.length = sizeof(efidp_hd),
			},
			.partition_number = 0,
			.start = ONE_MEG,
			.size = ONE_GIG,
			.signature = EFI_TEST_HD_SIGNATURE,
			.format = EFIDP_HD_FORMAT_GPT,
			.signature_type = EFIDP_HD_SIGNATURE_GUID,
		},
		.file = {
			.header = {
				.type = EFIDP_MEDIA_TYPE,
				.subtype = EFIDP_MEDIA_FILE,
				.length = sizeof(efidp_header) + sizeof(file_name),
			},
			.name = EFI_FEDORA_SHIM_EFI_UCS2,
		},
		.end = {
			.type = EFIDP_END_TYPE,
			.subtype = EFIDP_END_ENTIRE,
			.length = sizeof(efidp_header)
		},
	};

	insz = sizeof(sample);
	bufp = &buf[bufsz - insz];
	node = dp = (efidp)bufp;
	nodep = bufp;
	dnsz = sizeof(sample.pci);

	sz = efidp_make_generic(nodep, dnsz, EFIDP_HARDWARE_TYPE, EFIDP_HW_PCI, sizeof(efidp_pci));
	assert_equal_goto(sz, sizeof(sample.pci), out, "got %zd expected %zd\n");

	rc = efidp_set_node_data(node, (void *)&sample.pci.function, 2 * sizeof(sample.pci.function));
	if (rc < 0)
		print_efi_error();
	assert_zero_goto(rc, out, "got %d\n");

#ifdef DEBUG
	printf("sample = {\n"
	       "  .header = {\n"
	       "    .type = %hu,\n"
	       "    .subtype = %hu,\n"
	       "    .length = %hhu\n"
	       "  }\n"
	       "  .pci = {\n"
	       "    .device = 0x%hx,\n"
	       "    .function = 0x%hx\n"
	       "  }\n"
	       "};\n",
	       sample.pci.header.type,
	       sample.pci.header.subtype,
	       (unsigned char)sample.pci.header.length,
	       sample.pci.device,
	       sample.pci.function);

	printf("node = {\n"
	       "  .header = {\n"
	       "    .type = %hu,\n"
	       "    .subtype = %hu,\n"
	       "    .length = %hhu\n"
	       "  }\n"
	       "  .pci = {\n"
	       "    .device = 0x%hx,\n"
	       "    .function = 0x%hx\n"
	       "  }\n"
	       "};\n",
	       node->pci.header.type,
	       node->pci.header.subtype,
	       (unsigned char)node->pci.header.length,
	       node->pci.device,
	       node->pci.function);
#endif

	type = efidp_type((efidp)&sample.pci);
	assert_equal_goto((uint32_t)type, EFIDP_HARDWARE_TYPE, out, "got %"PRIu32" expected %d");

	type = efidp_subtype((efidp)&sample.pci);
	assert_equal_goto((uint32_t)type, EFIDP_HW_PCI, out, "got %"PRIu32" expected %d");

	rc = memcmp(node, &sample.pci, efidp_node_size(node));
	assert_zero_goto(rc, out, "got %d\n");

	rc = efidp_next_node((efidp)node, &next);
	assert_positive_goto(rc, out, "\n");

	node = (efidp)next;
	nodep = (void *)node;
	insz = sizeof(sample.hd);
	sz = efidp_make_hd(nodep, insz, 0, ONE_MEG, ONE_GIG,
			   hd_signature, EFIDP_HD_FORMAT_GPT,
			   EFIDP_HD_SIGNATURE_GUID);
	assert_equal_goto(sz, 42, out, "got %zd expected %d\n");

	rc = memcmp(node, &sample.hd, efidp_node_size(node));
#ifdef DEBUG
	printf("sample = {\n"
	       "  .header = {\n"
	       "    .type = %hu,\n"
	       "    .subtype = %hu,\n"
	       "    .length = %hhu\n"
	       "  },\n"
	       "  .hd = {\n"
	       "    .partition_number = 0x%hx,\n"
	       "    .start = 0x%" PRIx64 ",\n"
	       "    .size = 0x%" PRIx64 ",\n"
	       "    .signature = { %hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx },\n"
	       "    .format = 0x%hhx,\n"
	       "    .signature_type = %hhx\n"
	       "  }\n"
	       "};\n",
	       sample.hd.header.type, sample.hd.header.subtype,
	       (unsigned char)sample.hd.header.length, sample.hd.partition_number,
	       sample.hd.start, sample.hd.size, sample.hd.signature[0],
	       sample.hd.signature[1], sample.hd.signature[2],
	       sample.hd.signature[3], sample.hd.signature[4],
	       sample.hd.signature[5], sample.hd.signature[6],
	       sample.hd.signature[7], sample.hd.signature[8],
	       sample.hd.signature[9], sample.hd.signature[10],
	       sample.hd.signature[11], sample.hd.signature[12],
	       sample.hd.signature[13], sample.hd.signature[14],
	       sample.hd.signature[15], sample.hd.format,
	       sample.hd.signature_type);
	printf("node = {\n"
	       "  .header = {\n"
	       "    .type = %hu,\n"
	       "    .subtype = %hu,\n"
	       "    .length = %hhu\n"
	       "  },\n"
	       "  .hd = {\n"
	       "    .partition_number = 0x%hx,\n"
	       "    .start = 0x%" PRIx64 ",\n"
	       "    .size = 0x%" PRIx64 ",\n"
	       "    .signature = { %hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx,%hx },\n"
	       "    .format = 0x%hhx,\n"
	       "    .signature_type = %hhx\n"
	       "  }\n"
	       "};\n",
	       node->hd.header.type, node->hd.header.subtype,
	       (unsigned char)node->hd.header.length, node->hd.partition_number,
	       node->hd.start, node->hd.size, node->hd.signature[0],
	       node->hd.signature[1], node->hd.signature[2],
	       node->hd.signature[3], node->hd.signature[4],
	       node->hd.signature[5], node->hd.signature[6],
	       node->hd.signature[7], node->hd.signature[8],
	       node->hd.signature[9], node->hd.signature[10],
	       node->hd.signature[11], node->hd.signature[12],
	       node->hd.signature[13], node->hd.signature[14],
	       node->hd.signature[15], node->hd.format,
	       node->hd.signature_type);
#endif
	assert_zero_goto(rc, out, "got %d\n");

	rc = efidp_next_node((efidp)node, &next);
	assert_positive_goto(rc, out, "\n");

	node = (efidp)next;
	nodep = (void *)node;
	insz = sizeof(sample.file);

	sz = efidp_make_file(nodep, insz, EFI_FEDORA_SHIM_EFI_UTF8);
	assert_equal_goto(sz, 46, out, "got %zd expected %d\n");

#ifdef DEBUG
	printf("sample.file = {\n"
	       "  .header = {%hu, %hu, %hhu},\n"
	       "  .name = \"",
	       sample.file.header.type,
	       sample.file.header.subtype,
	       (unsigned char)sample.file.header.length);
	for (unsigned int i = 0; sample.file.name[i] != 0; i++)
		printf("%c", (char)sample.file.name[i]);
	printf("\"\n};\n");

	printf("node->file = {\n"
	       "  .header = {%hu, %hu, %hhu},\n"
	       "  .name = \"",
	       node->file.header.type,
	       node->file.header.subtype,
	       (unsigned char)node->file.header.length);
	for (unsigned int i = 0; node->file.name[i] != 0; i++)
		printf("%c", (char)node->file.name[i]);
	printf("\"\n};\n");
#endif

	rc = memcmp(node, &sample.file, efidp_node_size(node));
	assert_zero_goto(rc, out, "got %d\n");

	rc = efidp_next_node((efidp)node, &next);
	assert_positive_goto(rc, out, "\n");

	node = (efidp)next;
	nodep = (void *)node;
	insz = sizeof(sample.hd);
	sz = efidp_make_end_entire(nodep, sizeof(efidp_header));
	assert_equal_goto(sz, sizeof(sample.end), out, "got %zd expected %zd\n");

	rc = memcmp(node, &sample.end, efidp_node_size(node));
	assert_zero_goto(rc, out, "got %d\n");

	rc = memcmp(dp, &sample, efidp_size(dp));
	assert_zero_goto(rc, out, "got %d\n");

	insz = sizeof(sample);
	TEST_FORMAT((efidp)bufp,
		    "Pci(0x5a,0xa5)/HD(0,GPT,c7bd37f7-0bbc-264a-b8c7-baac1431b6c0,0x100000,0x40000000)/\\EFI\\FEDORA\\SHIM.EFI");

	struct {
		efidp_header end_instance;
		efidp_header end_entire;
	} ends;

	efidp_make_end_instance((uint8_t *)&ends.end_instance, 4);
	efidp_make_end_entire((uint8_t *)&ends.end_entire, 4);

#if 0
		= calloc(1, sizeof(sample) * 2 + sizeof(efidp_header));
	assert_not_equal_goto(double_path, NULL, out, "got %p expected !%p\n");
#endif

	efidp tmp0 = NULL;
	rc = efidp_append_path((const_efidp)&ends.end_entire, (const_efidp)&sample, &tmp0);
	assert_zero_goto(rc, out, "got %d\n");

	insz = efidp_size(tmp0);
	TEST_FORMAT(tmp0,
		    "Pci(0x5a,0xa5)/HD(0,GPT,c7bd37f7-0bbc-264a-b8c7-baac1431b6c0,0x100000,0x40000000)/\\EFI\\FEDORA\\SHIM.EFI");

	efidp tmp1 = NULL;
	rc = efidp_append_node(tmp0, (const efidp)&ends.end_instance, &tmp1);
	assert_zero_goto(rc, out, "got %d\n");

	insz = efidp_size(tmp1);
	dp = tmp1;
	TEST_FORMAT(tmp1,
		    "Pci(0x5a,0xa5)/HD(0,GPT,c7bd37f7-0bbc-264a-b8c7-baac1431b6c0,0x100000,0x40000000)/\\EFI\\FEDORA\\SHIM.EFI,");

	efidp double_path = NULL;
	rc = efidp_append_path(tmp1, (const_efidp)&sample, &double_path);
	assert_zero_goto(rc, out, "got %d\n");

	insz = efidp_size(double_path);
	TEST_FORMAT(double_path,
		    "Pci(0x5a,0xa5)/HD(0,GPT,c7bd37f7-0bbc-264a-b8c7-baac1431b6c0,0x100000,0x40000000)/\\EFI\\FEDORA\\SHIM.EFI,"
		    "Pci(0x5a,0xa5)/HD(0,GPT,c7bd37f7-0bbc-264a-b8c7-baac1431b6c0,0x100000,0x40000000)/\\EFI\\FEDORA\\SHIM.EFI");

	efidp new = NULL;
	rc = efidp_duplicate_path(double_path, &new);
	TEST_FORMAT(new,
		    "Pci(0x5a,0xa5)/HD(0,GPT,c7bd37f7-0bbc-264a-b8c7-baac1431b6c0,0x100000,0x40000000)/\\EFI\\FEDORA\\SHIM.EFI,"
		    "Pci(0x5a,0xa5)/HD(0,GPT,c7bd37f7-0bbc-264a-b8c7-baac1431b6c0,0x100000,0x40000000)/\\EFI\\FEDORA\\SHIM.EFI");

	free(tmp0);
	free(tmp1);
	free(double_path);
	free(new);

	efidp_header hdr = { 0, 0, 2 };
	char tmpstr[] = "abcdefg";
	rc = efidp_set_node_data((efidp)&hdr, tmpstr, sizeof(tmpstr));
	assert_negative_goto(rc, out, "%d\n", __LINE__);
	efi_error_pop_all();

	sz = efidp_parse_device_node(NULL, NULL, 0);
	assert_negative_goto(sz, out, "\n");
	assert_equal_goto(errno, ENOSYS, out, "got %d expected %d\n");
	efi_error_pop_all();

	sz = efidp_parse_device_path(NULL, NULL, 0);
	assert_negative_goto(sz, out, "\n");
	assert_equal_goto(errno, ENOSYS, out, "got %d expected %d\n");
	efi_error_pop_all();

	rc = efidp_append_path((const_efidp)&sample, NULL, &new);
	assert_zero_goto(rc, out, "got %d %d\n", __LINE__);
	TEST_FORMAT(new, "Pci(0x5a,0xa5)/HD(0,GPT,c7bd37f7-0bbc-264a-b8c7-baac1431b6c0,0x100000,0x40000000)/\\EFI\\FEDORA\\SHIM.EFI");
	free(new);

	rc = efidp_append_path(NULL, (const_efidp)&sample, &new);
	assert_zero_goto(rc, out, "got %d %d\n", __LINE__);
	TEST_FORMAT(new, "Pci(0x5a,0xa5)/HD(0,GPT,c7bd37f7-0bbc-264a-b8c7-baac1431b6c0,0x100000,0x40000000)/\\EFI\\FEDORA\\SHIM.EFI");
	free(new);

	rc = efidp_append_path(NULL, NULL, &new);
	assert_zero_goto(rc, out, "got %d %d\n", __LINE__);
	TEST_FORMAT(new, "");
	free(new);

	TEST_POSTAMBLE
}

int
main(int argc __attribute__((__unused__)),
     char *argv[] __attribute__((__unused__)))
{
	int status = 0;

	test(test_make_generic);

	test(test_make_acpi_hid);
	test(test_make_atapi);
	test(test_make_bios_boot);
	test(test_make_edd10);
	test(test_make_emmc);
	test(test_make_end_entire);
	test(test_make_end_instance);
	test(test_make_file);
	test(test_make_hd);
	test(test_make_hw_vendor);
	test(test_make_ipv4);
	test(test_make_ipv6);
	test(test_make_mac_addr);
	test(test_make_media_vendor);
	test(test_make_msg_vendor);
	test(test_make_nvdimm);
	test(test_make_nvme);
	test(test_make_pci);
	test(test_make_sas);
	test(test_make_sata);
	test(test_make_scsi);
	test(test_make_uart);
	test(test_make_uart_flow_control);
	test(test_make_vendor);

	test(test_dp_functions);

	printf("this should print no errors:\n");
	print_efi_error();
	efi_error_pop_all();

	return status;
}

// vim:fenc=utf-8:tw=75:noet
