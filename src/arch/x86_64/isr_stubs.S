.macro push_all_registers
	push %rax
	push %rbx
	push %rcx
	push %rdx
	push %rsi
	push %rdi
	push %rbp
	push %r8
	push %r9
	push %r10
	push %r11
	push %r12
	push %r13
	push %r14
	push %r15
.endm

.macro pop_all_registers
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rbp
	pop %rdi
	pop %rsi
	pop %rdx
	pop %rcx
	pop %rbx
	pop %rax
.endm

.macro interrupt_no_err intr
	.align 16
	pushq $0
	pushq $\intr
	jmp __common_interrupt
.endm

.macro interrupt_err intr
	.align 16
	pushq $\intr
	jmp __common_interrupt
.endm

.macro isr_insert intr
	.section .text
	interrupt_no_err \intr
.endm

.section .text

.extern interrupt_handler
__common_interrupt:
	cld
	push_all_registers
	
	movq %rsp, %rdi
	call interrupt_handler
	movq %rax, %rsp

	pop_all_registers
	addq $16, %rsp

	iretq

.global isr_stubs
isr_stubs:
interrupt_no_err	0
interrupt_no_err	1
interrupt_no_err	2
interrupt_no_err	3
interrupt_no_err	4
interrupt_no_err	5
interrupt_no_err	6
interrupt_no_err	7
interrupt_err		8
interrupt_no_err	9
interrupt_err		10
interrupt_err		11
interrupt_err		12
interrupt_err		13
interrupt_err		14
interrupt_no_err	15
interrupt_no_err	16
interrupt_err		17
interrupt_no_err	18
interrupt_no_err	19
interrupt_no_err	20
interrupt_no_err	21
interrupt_no_err 	22
interrupt_no_err	23
interrupt_no_err	24
interrupt_no_err	25
interrupt_no_err	26
interrupt_no_err	27
interrupt_no_err	28
interrupt_no_err	29
interrupt_err		30
interrupt_no_err	31

.altmacro
.set i, 32
.rept 224
	isr_insert %i
	.set i, i + 1
.endr
