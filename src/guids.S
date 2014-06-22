	.globl well_known_guids
	.data
	.balign 16
	.type	well_known_guids, %object
	.size	well_known_guids, well_known_guids_end - well_known_guids
well_known_guids:
.incbin "guids.bin"
	.globl well_known_guids_end
	.data
	.balign 16
	.type	well_known_guids_end, %object
	.size	well_known_guids_end, 1
well_known_guids_end:
	.byte 0
	.globl well_known_names
	.data
	.balign 16
	.type	well_known_names, %object
	.size	well_known_names, well_known_names_end - well_known_names
well_known_names:
.incbin "names.bin"
	.globl well_known_names_end
	.data
	.balign 16
	.type	well_known_names_end, %object
	.size	well_known_names_end, 1
well_known_names_end:
	.byte 0

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
