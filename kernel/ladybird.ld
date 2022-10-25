ENTRY(_zerokara_hajimeru)

MEMORY {
	ram (rw)  : ORIGIN = 0x80000000, LENGTH = 2048m
	uart (rw)  : ORIGIN = 0x10000000, LENGTH = 4k
}

SECTIONS {
	.text : { *(.text) __errno = .;} > ram
	.data : { *(.data) *(.rodata*) } > ram
	.bss  : { *(.bss) _end = .; } > ram
	.uart : { _uart = .; } > uart
	}
