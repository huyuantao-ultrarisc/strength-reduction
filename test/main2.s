	.text
	.file	"main.c"
	.globl	main                            # -- Begin function main
	.p2align	4, 0x90
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movl	$0, -12(%rbp)
	movl	$0, -8(%rbp)
	movl	$1, -4(%rbp)
	cmpl	$99, -4(%rbp)
	jg	.LBB0_3
	.p2align	4, 0x90
.LBB0_2:                                # %for.body
                                        # =>This Inner Loop Header: Depth=1
	movl	-4(%rbp), %eax
	leal	(%rax,%rax,2), %eax
	addl	$-2, %eax
	addl	%eax, -8(%rbp)
	movl	%eax, -16(%rbp)
	addl	$2, -4(%rbp)
	cmpl	$99, -4(%rbp)
	jle	.LBB0_2
.LBB0_3:                                # %for.end
	movl	-8(%rbp), %esi
	movl	$.L.str, %edi
	xorl	%eax, %eax
	callq	printf@PLT
	movl	-12(%rbp), %eax
	addq	$16, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end0:
	.size	main, .Lfunc_end0-main
	.cfi_endproc
                                        # -- End function
	.type	.L.str,@object                  # @.str
	.section	.rodata.str1.1,"aMS",@progbits,1
.L.str:
	.asciz	"%d"
	.size	.L.str, 3

	.ident	"Ubuntu clang version 14.0.0-1ubuntu1"
	.section	".note.GNU-stack","",@progbits