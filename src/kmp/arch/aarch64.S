// aarch64.S : microtasking routine __kmp_invoke_microtask for ARM aarch64.
//
// Ported from LLVM's OpenMP runtime (openmp/runtime/src/z_Linux_asm.S, the
// KMP_ARCH_AARCH64 path) and trimmed to Linux/ELF aarch64 without OMPT support
// (xkomp does not use OMPT, so the exit_frame parameter and the x19/x20 save it
// requires are dropped). Unlike the generic C backend this has no argument-count
// cap and needs no libffi.
//
// This file is selected by SUPPORT_MICROTASK_ARM in the top-level CMakeLists.txt,
// which also defines KMP_ARCH_AARCH64=1 (mirrors how SUPPORT_MICROTASK_INTEL
// selects z_Linux_asm.S with KMP_ARCH_X86_64=1).

#if KMP_ARCH_AARCH64

// -----------------------------------------------------------------------
// macros (Linux/ELF aarch64)
// -----------------------------------------------------------------------

# define KMP_PREFIX_UNDERSCORE(x) x    // no extra underscore for Linux* OS symbols
# define KMP_LABEL(x) .L_##x           // local label, hidden from gdb backtraces

.macro ALIGN size
	.align 1<<(\size)
.endm

.macro DEBUG_INFO proc
	.cfi_endproc
	ALIGN 2
	.type  \proc,@function
	.size  \proc,.-\proc
.endm

.macro PROC proc
	ALIGN 2
	.globl KMP_PREFIX_UNDERSCORE(\proc)
KMP_PREFIX_UNDERSCORE(\proc):
	.cfi_startproc
.endm

// BTI and PAC gnu property note (emitted only when the toolchain requests the
// features; otherwise PACBTI_C / PACBTI_RET expand to nothing).
# define NT_GNU_PROPERTY_TYPE_0 5
# define GNU_PROPERTY_AARCH64_FEATURE_1_AND 0xc0000000
# define GNU_PROPERTY_AARCH64_FEATURE_1_BTI 1
# define GNU_PROPERTY_AARCH64_FEATURE_1_PAC 2

# define GNU_PROPERTY(type, value)                                             \
  .pushsection .note.gnu.property, "a";                                        \
  .p2align 3;                                                                  \
  .word 4;                                                                     \
  .word 16;                                                                    \
  .word NT_GNU_PROPERTY_TYPE_0;                                                \
  .asciz "GNU";                                                                \
  .word type;                                                                  \
  .word 4;                                                                     \
  .word value;                                                                 \
  .word 0;                                                                     \
  .popsection

# if defined(__ARM_FEATURE_BTI_DEFAULT)
#  define BTI_FLAG GNU_PROPERTY_AARCH64_FEATURE_1_BTI
# else
#  define BTI_FLAG 0
# endif
# if __ARM_FEATURE_PAC_DEFAULT & 3
#  define PAC_FLAG GNU_PROPERTY_AARCH64_FEATURE_1_PAC
# else
#  define PAC_FLAG 0
# endif

# if (BTI_FLAG | PAC_FLAG) != 0
#  if PAC_FLAG != 0
#   define PACBTI_C hint #25
#   define PACBTI_RET hint #29
#  else
#   define PACBTI_C hint #34
#   define PACBTI_RET
#  endif
#  define GNU_PROPERTY_BTI_PAC \
     GNU_PROPERTY(GNU_PROPERTY_AARCH64_FEATURE_1_AND, BTI_FLAG | PAC_FLAG)
# else
#  define PACBTI_C
#  define PACBTI_RET
#  define GNU_PROPERTY_BTI_PAC
# endif

//------------------------------------------------------------------------
// int
// __kmp_invoke_microtask( void (*pkfn) (int *gtid, int *tid, ...),
//                         int gtid, int tid,
//                         int argc, void *p_argv[] ) {
//   (*pkfn)( & gtid, & tid, argv[0], ... );
//   return 1;
// }
//
// parameters:
//	x0:	pkfn
//	w1:	gtid
//	w2:	tid
//	w3:	argc
//	x4:	p_argv
//
// locals:
//	__gtid:	gtid parm spilled on stack so we can pass &gtid to pkfn
//	__tid:	tid  parm spilled on stack so we can pass &tid  to pkfn
//
// reg temps:
//	 x8:	holds pkfn address
//	 w9:	remaining pkfn parm count / stack-slot count
//	x10:	traverses p_argv array
//	x11:	stack write pointer for the 7th+ parms
//	x12:	temporary for stack parameters
//
// return:	w0	(always 1/TRUE)
//------------------------------------------------------------------------

__gtid = 4
__tid = 8

// -- Begin __kmp_invoke_microtask
	.text
	PROC __kmp_invoke_microtask
	PACBTI_C

	stp	x29, x30, [sp, #-16]!	// save frame pointer and link register
	mov	x29, sp

	// reserve a 16-byte-aligned area for parms 7..argc: 16 * (1 + argc/2)
	orr	w9, wzr, #1
	add	w9, w9, w3, lsr #1
	sub	sp, sp, w9, uxtw #4
	mov	x11, sp

	mov	x8, x0			// pkfn
	str	w1, [x29, #-__gtid]	// spill gtid
	str	w2, [x29, #-__tid]	// spill tid
	mov	w9, w3			// argc -> counter
	mov	x10, x4			// p_argv

	sub	x0, x29, #__gtid	// 1st parm: &gtid
	sub	x1, x29, #__tid		// 2nd parm: &tid

	// first six p_argv[] entries go in x2..x7
	cbz	w9, KMP_LABEL(kmp_1)
	ldr	x2, [x10]

	sub	w9, w9, #1
	cbz	w9, KMP_LABEL(kmp_1)
	ldr	x3, [x10, #8]!

	sub	w9, w9, #1
	cbz	w9, KMP_LABEL(kmp_1)
	ldr	x4, [x10, #8]!

	sub	w9, w9, #1
	cbz	w9, KMP_LABEL(kmp_1)
	ldr	x5, [x10, #8]!

	sub	w9, w9, #1
	cbz	w9, KMP_LABEL(kmp_1)
	ldr	x6, [x10, #8]!

	sub	w9, w9, #1
	cbz	w9, KMP_LABEL(kmp_1)
	ldr	x7, [x10, #8]!

	// remaining p_argv[] entries are passed on the stack
KMP_LABEL(kmp_0):
	sub	w9, w9, #1
	cbz	w9, KMP_LABEL(kmp_1)
	ldr	x12, [x10, #8]!
	str	x12, [x11], #8
	b	KMP_LABEL(kmp_0)
KMP_LABEL(kmp_1):
	blr	x8			// (*pkfn)( &gtid, &tid, argv[0], ... )
	orr	w0, wzr, #1		// return 1
	mov	sp, x29
	ldp	x29, x30, [sp], #16
	PACBTI_RET
	ret

	DEBUG_INFO __kmp_invoke_microtask
// -- End  __kmp_invoke_microtask

#endif /* KMP_ARCH_AARCH64 */

#if defined(__linux__)
.section .note.GNU-stack,"",%progbits
#endif

#if KMP_ARCH_AARCH64 && defined(__linux__)
GNU_PROPERTY_BTI_PAC
#endif
