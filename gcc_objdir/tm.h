#ifndef GCC_TM_H
#define GCC_TM_H

/* ************************************************************************** */

/* Архитектурно-зависимая часть. */

#if defined(__sparc__)

#if defined(__sparcv9) || defined(__arch64__)
#define UNITS_PER_WORD          8
#else /* defined(__sparcv9) || defined(__arch64__) */
#define UNITS_PER_WORD          4
#endif /* defined(__sparcv9) || defined(__arch64__) */

#define MIN_UNITS_PER_WORD      UNITS_PER_WORD

/* FIXME Разобраться, зачем оно нужно, и, возможно, определить для e2k. */
#define WIDEST_HARDWARE_FP_SIZE 64

/* Используется при определении DWARF_FRAME_REGISTERS. */
#define FIRST_PSEUDO_REGISTER 103

/* Используется при определении __LIBGCC_STACK_GROWS_DOWNWARD__. */
#define STACK_GROWS_DOWNWARD 1

/* Используется при определении __LIBGCC_EH_RETURN_STACKADJ_RTX__. */
#define EH_RETURN_STACKADJ_RTX /* */

#define TEXT_SECTION_ASM_OP     "\t.text"
#define INIT_SECTION_ASM_OP     "\t.section \".init\""
#define FINI_SECTION_ASM_OP     "\t.section \".fini\""

/* Подойдёт и реализация по умолчанию из crtstuff.c, но так ассемблерный файл
 * получается чище (не строятся мусорные прологи и эпилоги). */
#define CRT_CALL_STATIC_FUNCTION(SECTION_OP, FUNC)      \
asm(SECTION_OP "\n"                                     \
    "\tcall " #FUNC "\n"                                  \
    "\t nop");

#elif defined(__e2k__)

/* Используется в libgcc2.c, libgcc2.h и при определении MIN_UNITS_PER_WORD. */
#define UNITS_PER_WORD 8

/* Используется в libgcc2.c, libgcc2.h. */
#define MIN_UNITS_PER_WORD      UNITS_PER_WORD

/* Используется при определении DWARF_FRAME_REGISTERS. */
/* FIXME Разобраться, нужно ли оно для e2k. */
#define FIRST_PSEUDO_REGISTER 48

#define TEXT_SECTION_ASM_OP     "\t.text"
#define INIT_SECTION_ASM_OP     "\t.section \".init\", \"ax\", @progbits"
#define FINI_SECTION_ASM_OP     "\t.section \".fini\", \"ax\", @progbits"

/* Важно, что wbs = 0x6. Если изменять, то нужно также исправлять crti.c
 * (который идёт в составе glibc). А это нарушит двоичную совместимость. */
#define CRT_CALL_STATIC_FUNCTION(SECTION_OP, FUNC)      \
asm(SECTION_OP "\n"                                     \
    "\t{ nop 4; disp %ctpr1, " #FUNC " }\n"             \
    "\tcall %ctpr1, wbs = 0x6\n");

#endif /* defined(__sparc__) || defined(__e2k__) */

/* ************************************************************************** */

/* Общая часть для поддерживаемых платформ. */

/* Используется в crtstuff.c, libgcc2.c. */
#define OBJECT_FORMAT_ELF

/* Используется в crtstuff.c. Наш компилятор не умеет transactional memory,
 * поэтому 0. */
#define USE_TM_CLONE_REGISTRY 0

/* Включить в auto-host.h после включения поддержки .init_array, .fini_array. */
#ifdef HAVE_INITFINI_ARRAY_SUPPORT

/* Используется в crtstuff.c. */
#define USE_INITFINI_ARRAY

/* Если есть поддержка .init_array, .fini_array, эти макросы нужно
 * разопределить. */
#undef INIT_SECTION_ASM_OP
#undef FINI_SECTION_ASM_OP

/* Используется при определении __LIBGCC_INIT_ARRAY_SECTION_ASM_OP__. */
#define INIT_ARRAY_SECTION_ASM_OP

/* Используется в crtstuff.c */
#define FINI_ARRAY_SECTION_ASM_OP

#endif /* HAVE_INITFINI_ARRAY_SUPPORT */

#define BITS_PER_UNIT 8

/* Макрос с таким именем используется в gthr.h, gthr-posix.h. */
#define SUPPORTS_WEAK 1

/* Используется в crtstuff.c. */
#define TARGET_ATTRIBUTE_WEAK __attribute__ ((weak))

/* Используется для определения __LIBGCC_EH_FRAME_SECTION_NAME__. */
#define EH_FRAME_SECTION_NAME ".eh_frame"

/* Используется для определения __LIBGCC_EH_TABLES_CAN_BE_READ_ONLY__. */
#define EH_TABLES_CAN_BE_READ_ONLY 1

/* Используется для определения __LIBGCC_DWARF_FRAME_REGISTERS__. */
#define DWARF_FRAME_REGISTERS FIRST_PSEUDO_REGISTER

/* Используется в unwind-dw2.c. */
#define DWARF_REG_TO_UNWIND_COLUMN(REGNO) (REGNO)

/* Используется для определения __LIBGCC_VTABLE_USES_DESCRIPTORS__. */
#define TARGET_VTABLE_USES_DESCRIPTORS 0

/* Используется в libgcov.h, libgcov-driver-system.c. */
#define TARGET_POSIX_IO

/* Используется в libgcov.h. */
#define LONG_LONG_TYPE_SIZE 64

/* ************************************************************************** */

/* Определение макросов, которые по-хорошему должен определять компилятор
 * по опции -fbuilding-libgcc. По этой опции мы определяем часть из них;
 * остальные удобнее держать здесь. */

/* Используется в crtstuff.c. */
#define __LIBGCC_EH_TABLES_CAN_BE_READ_ONLY__ EH_TABLES_CAN_BE_READ_ONLY

/* Используется в crtstuff.c. */
#ifdef EH_FRAME_SECTION_NAME
#define __LIBGCC_EH_FRAME_SECTION_NAME__ EH_FRAME_SECTION_NAME
#endif /* EH_FRAME_SECTION_NAME */

/* Используется в crtstuff.c. */
#ifdef CTORS_SECTION_ASM_OP
#define __LIBGCC_CTORS_SECTION_ASM_OP__ CTORS_SECTION_ASM_OP
#endif /* CTORS_SECTION_ASM_OP */

/* Используется в crtstuff.c. */
#ifdef DTORS_SECTION_ASM_OP
#define __LIBGCC_DTORS_SECTION_ASM_OP__ DTORS_SECTION_ASM_OP
#endif /* DTORS_SECTION_ASM_OP */

/* Используется в crtstuff.c. */
#ifdef TEXT_SECTION_ASM_OP
#define __LIBGCC_TEXT_SECTION_ASM_OP__ TEXT_SECTION_ASM_OP
#endif /* TEXT_SECTION_ASM_OP */

/* Используется в crtstuff.c, libgcc2.c. */
#ifdef INIT_SECTION_ASM_OP
#define __LIBGCC_INIT_SECTION_ASM_OP__ INIT_SECTION_ASM_OP
#endif /* INIT_SECTION_ASM_OP */

/* Используется в crtstuff.c, libgcc2.c. */
#ifdef INIT_ARRAY_SECTION_ASM_OP
#define __LIBGCC_INIT_ARRAY_SECTION_ASM_OP__
#endif /* INIT_ARRAY_SECTION_ASM_OP */

/* Используется в unwind-dw2.c. */
#if STACK_GROWS_DOWNWARD
#define __LIBGCC_STACK_GROWS_DOWNWARD__
#endif /* STACK_GROWS_DOWNWARD */

/* Используется в unwind-dw2.c. */
#define __LIBGCC_DWARF_FRAME_REGISTERS__ DWARF_FRAME_REGISTERS

/* Используется в unwind-dw2.c */
#ifdef EH_RETURN_STACKADJ_RTX
#define __LIBGCC_EH_RETURN_STACKADJ_RTX__
#endif /* EH_RETURN_STACKADJ_RTX */

/* Используется в libgcov-profiler.c. */
#define __LIBGCC_VTABLE_USES_DESCRIPTORS__ TARGET_VTABLE_USES_DESCRIPTORS

#endif /* GCC_TM_H */
