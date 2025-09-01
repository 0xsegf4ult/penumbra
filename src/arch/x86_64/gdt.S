.section .text
.global reload_segments
.type reload_segments, @function

reload_segments:
	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss
	push $0x8
	lea trampoline(%rip), %rax
	push %rax
	lretq
trampoline:
	retq
