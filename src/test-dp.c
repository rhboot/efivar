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

int
main(int argc __attribute__((__unused__)),
     char *argv[] __attribute__((__unused__)))
{
	int status = 0;

	test(test_make_generic);

	test(test_make_acpi_hid);
	test(test_make_atapi);
	test(test_make_edd10);
	test(test_make_emmc);
	test(test_make_end_entire);
	test(test_make_end_instance);
	test(test_make_file);
	test(test_make_hd);
	test(test_make_hw_vendor);
	test(test_make_ipv4);
	test(test_make_mac_addr);
	test(test_make_media_vendor);
	test(test_make_msg_vendor);
	test(test_make_nvdimm);
	test(test_make_vendor);

	printf("this should print no errors:\n");
	print_efi_error();
	efi_error_pop_all();

	return status;
}

// vim:fenc=utf-8:tw=75:noet
