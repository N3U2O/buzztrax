	.data
.LC0:	.string	"Called unk_%s\n"
	.align 4
.globl unk_exp1
unk_exp1:
	pushq %ebp
	movl %esp,%ebp
	subl $4,%esp
	movl $1,-4(%ebp)
	movl -4(%ebp),%eax
	movl %eax,%ecx
	movl %ecx,%edx
	sall $4,%edx
	subl %eax,%edx
	leal 0(,%edx,2),%eax
	movl %eax,%edx
	addl $export_names,%edx
	pushq %edx
	pushq $.LC0
	call printf
	addl $8,%esp
	xorl %eax,%eax
	leave
	ret
.globl exp_EH_prolog
exp_EH_prolog:
	pushq $0xff
	pushq %eax
	pushq %fs:0
	movl  %esp, %fs:0
	movl  12(%esp), %eax
	movl  %ebp, 12(%esp)
	leal  12(%esp), %ebp
	pushq %eax
	ret

.section .note.GNU-stack,"",@progbits
