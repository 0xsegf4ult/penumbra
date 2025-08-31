ENTRY(init)

PHDRS
{
	null		PT_NULL;
	limine_requests	PT_LOAD;
	text		PT_LOAD;
	rodata		PT_LOAD;
	data		PT_LOAD;
	dynamic		PT_DYNAMIC;

}

SECTIONS
{
	. = 0xffffffff80000000;
	virt_kernel_start = .;

	.limine_requests : 
	{
		KEEP(*(.limine_requests_start))
		KEEP(*(.limine_requests))
		KEEP(*(.limine_requests_end))
	} : limine_requests

	. = ALIGN(CONSTANT(MAXPAGESIZE));
	
	.text :
	{
		_text_start = .;
		*(.text .text.*)
		_text_end = .;
	} : text

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	.rodata :
	{
		start_ctors = .;
		*(.ctors)
		*(.init_array)
		end_ctors = .;

		_rodata_start = .;
		*(.rodata .rodata.*)
		_rodata_end = .;
	} : rodata

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	.data  :
	{
		_data_start = .;
		*(.data .data.*)
		_data_end = .;
	} : data

	.dynamic :
	{
		*(.dynamic .dynamic.*)
	} : data : dynamic

	.bss :
	{
		_bss_start = .;
		*(.bss .bss.*)
		*(COMMON)
		_bss_end = .;
	} : data

	virt_kernel_end = .;

	/DISCARD/ :
	{
		*(.eh_frame*)
		*(.note .note.*)
	}
}
