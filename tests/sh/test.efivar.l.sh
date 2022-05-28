#!/bin/sh -eu

"$1" -L | grep -q '^{91376aff-cba6-42be-949d-06fde81128e8}	{grub}	efi_guid_grub	GRUB$'
