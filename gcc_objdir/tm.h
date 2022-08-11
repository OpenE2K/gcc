#ifndef GCC_TM_H
#define GCC_TM_H

/* ************************************************************************** */

/* ������������-��������� �����. */

#if defined(__sparc__)

#if defined(__sparcv9) || defined(__arch64__)
#define UNITS_PER_WORD          8
#else /* defined(__sparcv9) || defined(__arch64__) */
#define UNITS_PER_WORD          4
#endif /* defined(__sparcv9) || defined(__arch64__) */

#define MIN_UNITS_PER_WORD      UNITS_PER_WORD

/* FIXME �����������, ����� ��� �����, �, ��������, ���������� ��� e2k. */
#define WIDEST_HARDWARE_FP_SIZE 64

/* ������������ ��� ����������� DWARF_FRAME_REGISTERS. */
#define FIRST_PSEUDO_REGISTER 103

/* ������������ ��� ����������� __LIBGCC_STACK_GROWS_DOWNWARD__. */
#define STACK_GROWS_DOWNWARD 1

/* ������������ ��� ����������� __LIBGCC_EH_RETURN_STACKADJ_RTX__. */
#define EH_RETURN_STACKADJ_RTX /* */

#define TEXT_SECTION_ASM_OP     "\t.text"
#define INIT_SECTION_ASM_OP     "\t.section \".init\""
#define FINI_SECTION_ASM_OP     "\t.section \".fini\""

/* �����ģ� � ���������� �� ��������� �� crtstuff.c, �� ��� ������������ ����
 * ���������� ���� (�� �������� �������� ������� � �������). */
#define CRT_CALL_STATIC_FUNCTION(SECTION_OP, FUNC)      \
asm(SECTION_OP "\n"                                     \
    "\tcall " #FUNC "\n"                                  \
    "\t nop");

#elif defined(__e2k__)

/* ������������ � libgcc2.c, libgcc2.h � ��� ����������� MIN_UNITS_PER_WORD. */
#define UNITS_PER_WORD 8

/* ������������ � libgcc2.c, libgcc2.h. */
#define MIN_UNITS_PER_WORD      UNITS_PER_WORD

/* ������������ ��� ����������� DWARF_FRAME_REGISTERS. */
/* FIXME �����������, ����� �� ��� ��� e2k. */
#define FIRST_PSEUDO_REGISTER 48

#define TEXT_SECTION_ASM_OP     "\t.text"
#define INIT_SECTION_ASM_OP     "\t.section \".init\", \"ax\", @progbits"
#define FINI_SECTION_ASM_OP     "\t.section \".fini\", \"ax\", @progbits"

/* �����, ��� wbs = 0x6. ���� ��������, �� ����� ����� ���������� crti.c
 * (������� �ģ� � ������� glibc). � ��� ������� �������� �������������. */
#define CRT_CALL_STATIC_FUNCTION(SECTION_OP, FUNC)      \
asm(SECTION_OP "\n"                                     \
    "\t{ nop 4; disp %ctpr1, " #FUNC " }\n"             \
    "\tcall %ctpr1, wbs = 0x6\n");

#endif /* defined(__sparc__) || defined(__e2k__) */

/* ************************************************************************** */

/* ����� ����� ��� �������������� ��������. */

/* ������������ � crtstuff.c, libgcc2.c. */
#define OBJECT_FORMAT_ELF

/* ������������ � crtstuff.c. ��� ���������� �� ����� transactional memory,
 * ������� 0. */
#define USE_TM_CLONE_REGISTRY 0

/* �������� � auto-host.h ����� ��������� ��������� .init_array, .fini_array. */
#ifdef HAVE_INITFINI_ARRAY_SUPPORT

/* ������������ � crtstuff.c. */
#define USE_INITFINI_ARRAY

/* ���� ���� ��������� .init_array, .fini_array, ��� ������� �����
 * �������������. */
#undef INIT_SECTION_ASM_OP
#undef FINI_SECTION_ASM_OP

/* ������������ ��� ����������� __LIBGCC_INIT_ARRAY_SECTION_ASM_OP__. */
#define INIT_ARRAY_SECTION_ASM_OP

/* ������������ � crtstuff.c */
#define FINI_ARRAY_SECTION_ASM_OP

#endif /* HAVE_INITFINI_ARRAY_SUPPORT */

#define BITS_PER_UNIT 8

/* ������ � ����� ������ ������������ � gthr.h, gthr-posix.h. */
#define SUPPORTS_WEAK 1

/* ������������ � crtstuff.c. */
#define TARGET_ATTRIBUTE_WEAK __attribute__ ((weak))

/* ������������ ��� ����������� __LIBGCC_EH_FRAME_SECTION_NAME__. */
#define EH_FRAME_SECTION_NAME ".eh_frame"

/* ������������ ��� ����������� __LIBGCC_EH_TABLES_CAN_BE_READ_ONLY__. */
#define EH_TABLES_CAN_BE_READ_ONLY 1

/* ������������ ��� ����������� __LIBGCC_DWARF_FRAME_REGISTERS__. */
#define DWARF_FRAME_REGISTERS FIRST_PSEUDO_REGISTER

/* ������������ � unwind-dw2.c. */
#define DWARF_REG_TO_UNWIND_COLUMN(REGNO) (REGNO)

/* ������������ ��� ����������� __LIBGCC_VTABLE_USES_DESCRIPTORS__. */
#define TARGET_VTABLE_USES_DESCRIPTORS 0

/* ������������ � libgcov.h, libgcov-driver-system.c. */
#define TARGET_POSIX_IO

/* ������������ � libgcov.h. */
#define LONG_LONG_TYPE_SIZE 64

/* ************************************************************************** */

/* ����������� ��������, ������� ��-�������� ������ ���������� ����������
 * �� ����� -fbuilding-libgcc. �� ���� ����� �� ���������� ����� �� ���;
 * ��������� ������� ������� �����. */

/* ������������ � crtstuff.c. */
#define __LIBGCC_EH_TABLES_CAN_BE_READ_ONLY__ EH_TABLES_CAN_BE_READ_ONLY

/* ������������ � crtstuff.c. */
#ifdef EH_FRAME_SECTION_NAME
#define __LIBGCC_EH_FRAME_SECTION_NAME__ EH_FRAME_SECTION_NAME
#endif /* EH_FRAME_SECTION_NAME */

/* ������������ � crtstuff.c. */
#ifdef CTORS_SECTION_ASM_OP
#define __LIBGCC_CTORS_SECTION_ASM_OP__ CTORS_SECTION_ASM_OP
#endif /* CTORS_SECTION_ASM_OP */

/* ������������ � crtstuff.c. */
#ifdef DTORS_SECTION_ASM_OP
#define __LIBGCC_DTORS_SECTION_ASM_OP__ DTORS_SECTION_ASM_OP
#endif /* DTORS_SECTION_ASM_OP */

/* ������������ � crtstuff.c. */
#ifdef TEXT_SECTION_ASM_OP
#define __LIBGCC_TEXT_SECTION_ASM_OP__ TEXT_SECTION_ASM_OP
#endif /* TEXT_SECTION_ASM_OP */

/* ������������ � crtstuff.c, libgcc2.c. */
#ifdef INIT_SECTION_ASM_OP
#define __LIBGCC_INIT_SECTION_ASM_OP__ INIT_SECTION_ASM_OP
#endif /* INIT_SECTION_ASM_OP */

/* ������������ � crtstuff.c, libgcc2.c. */
#ifdef INIT_ARRAY_SECTION_ASM_OP
#define __LIBGCC_INIT_ARRAY_SECTION_ASM_OP__
#endif /* INIT_ARRAY_SECTION_ASM_OP */

/* ������������ � unwind-dw2.c. */
#if STACK_GROWS_DOWNWARD
#define __LIBGCC_STACK_GROWS_DOWNWARD__
#endif /* STACK_GROWS_DOWNWARD */

/* ������������ � unwind-dw2.c. */
#define __LIBGCC_DWARF_FRAME_REGISTERS__ DWARF_FRAME_REGISTERS

/* ������������ � unwind-dw2.c */
#ifdef EH_RETURN_STACKADJ_RTX
#define __LIBGCC_EH_RETURN_STACKADJ_RTX__
#endif /* EH_RETURN_STACKADJ_RTX */

/* ������������ � libgcov-profiler.c. */
#define __LIBGCC_VTABLE_USES_DESCRIPTORS__ TARGET_VTABLE_USES_DESCRIPTORS

#endif /* GCC_TM_H */
