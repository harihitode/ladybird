ENTRY(_zerokara_hajimeru)

MEMORY {
	bram (rw)  : ORIGIN = 0x80000000, LENGTH = 8k
	iram (rwx) : ORIGIN = 0x90000000, LENGTH = 72k
	uart (rw)  : ORIGIN = 0x10000000, LENGTH = 4k
}

SECTIONS {
	.text : { *(.text) __errno = .;} > iram
	.data : { *(.data) *(.rodata*) } > iram
	.bss  : { *(.bss) _end = .; } > bram
	.uart : { _uart = .; } > uart
	}
