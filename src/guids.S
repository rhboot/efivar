	.globl efi_well_known_guids
	.data
	.balign 16
	.type	efi_well_known_guids, %object
	.size	efi_well_known_guids, efi_well_known_guids_end - efi_well_known_guids
efi_well_known_guids:
.incbin "guids.bin"
	.globl efi_well_known_guids_end
	.data
	.balign 16
	.type	efi_well_known_guids_end, %object
	.size	efi_well_known_guids_end, 1
efi_well_known_guids_end:
	.byte 0
	.globl efi_well_known_names
	.data
	.balign 16
	.type	efi_well_known_names, %object
	.size	efi_well_known_names, efi_well_known_names_end - efi_well_known_names
efi_well_known_names:
.incbin "names.bin"
	.globl efi_well_known_names_end
	.data
	.balign 16
	.type	efi_well_known_names_end, %object
	.size	efi_well_known_names_end, 1
efi_well_known_names_end:
	.byte 0

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
