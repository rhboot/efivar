/*
 * libefiboot - library for the manipulation of EFI boot variables
 * Copyright 2012-2018 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 *
 */

#include "fix_coverity.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>

#include "efiboot.h"

int HIDDEN
parse_acpi_hid_uid(struct device *dev, const char *fmt, ...)
{
        int rc;
        char *path = NULL;
        va_list ap;
        char *fbuf = NULL;
        uint16_t tmp16;
        uint32_t acpi_hid = 0;
        uint64_t acpi_uid_int = 0;

        debug("entry");

        va_start(ap, fmt);
        rc = vasprintfa(&path, fmt, ap);
        va_end(ap);
        debug("path:%s rc:%d", path, rc);
        if (rc < 0 || path == NULL)
                return -1;

        rc = read_sysfs_file(&fbuf, "%s/firmware_node/path", path);
        if (rc > 0 && fbuf) {
                size_t l = strlen(fbuf);
                if (l > 1) {
                        fbuf[l-1] = 0;
                        dev->acpi_root.acpi_cid_str = strdup(fbuf);
                        debug("Setting ACPI root path to \"%s\"", fbuf);
                }
        }

        rc = read_sysfs_file(&fbuf, "%s/firmware_node/hid", path);
        if (rc < 0 || fbuf == NULL) {
                efi_error("could not read %s/firmware_node/hid", path);
                return -1;
        }

        rc = strlen(fbuf);
        if (rc < 4) {
hid_err:
                efi_error("could not parse %s/firmware_node/hid", path);
                return -1;
        }
        rc -= 4;

        rc = sscanf((char *)fbuf + rc, "%04hx", &tmp16);
        debug("rc:%d hid:0x%08x\n", rc, tmp16);
        if (rc != 1)
                goto hid_err;

        acpi_hid = EFIDP_EFI_PNP_ID(tmp16);

        /*
         * Apparently basically nothing can look up a PcieRoot() node,
         * because they just check _CID.  So since _CID for the root pretty
         * much always has to be PNP0A03 anyway, just use that no matter
         * what.
         */
        if (acpi_hid == EFIDP_ACPI_PCIE_ROOT_HID)
                acpi_hid = EFIDP_ACPI_PCI_ROOT_HID;
        dev->acpi_root.acpi_hid = acpi_hid;
        debug("acpi root HID:0x%08x", acpi_hid);

        errno = 0;
        fbuf = NULL;
        rc = read_sysfs_file(&fbuf, "%s/firmware_node/uid", path);
        if ((rc < 0 && errno != ENOENT) || (rc > 0 && fbuf == NULL)) {
                efi_error("could not read %s/firmware_node/uid", path);
                return -1;
        }
        if (rc > 0) {
                rc = sscanf((char *)fbuf, "%"PRIu64"\n", &acpi_uid_int);
                if (rc == 1) {
                        dev->acpi_root.acpi_uid = acpi_uid_int;
                } else {
                        /* kernel uses "%s\n" to print it, so there
                         * should always be some value and a newline... */
                        int l = strlen((char *)fbuf);
                        if (l >= 1) {
                                fbuf[l-1] = '\0';
                                dev->acpi_root.acpi_uid_str = strdup(fbuf);
                        }
                }
        }
        debug("acpi root UID:0x%"PRIx64" uidstr:\"%s\"",
              dev->acpi_root.acpi_uid, dev->acpi_root.acpi_uid_str);

        errno = 0;
        return 0;
}
