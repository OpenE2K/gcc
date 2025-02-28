#pragma once

/* this file must be the same as in liblfortran/liblfortran (for fixing Bug 654) */

/* Header file to the Fortran front-end and runtime library
   Copyright (C) 2007-2014 Free Software Foundation, Inc.
*/
#include "ftn_lang_flags.h"

/* Bitmasks for the various FPE that can be enabled.  These need to be straight integers
   e.g., 8 instead of (1<<3), because they will be included in Fortran source.  */
#define GFC_FPE_INVALID      1
#define GFC_FPE_DENORMAL     2
#define GFC_FPE_ZERO         4
#define GFC_FPE_OVERFLOW     8
#define GFC_FPE_UNDERFLOW   16
#define GFC_FPE_INEXACT     32

/* Defines for floating-point rounding modes.  */
#define GFC_FPE_DOWNWARD   1
#define GFC_FPE_TONEAREST  2
#define GFC_FPE_TOWARDZERO 3
#define GFC_FPE_UPWARD     4

/* Size of the buffer required to store FPU state for any target.
   In particular, this has to be larger than fenv_t on all glibc targets.
   Currently, the winner is x86_64 with 32 bytes.  */
#define GFC_FPE_STATE_BUFFER_SIZE 32

/* Bitmasks for the various runtime checks that can be enabled.  */
#define GFC_RTCHECK_BOUNDS      (1<<0)
#define GFC_RTCHECK_ARRAY_TEMPS (1<<1)
#define GFC_RTCHECK_RECURSION   (1<<2)
#define GFC_RTCHECK_DO          (1<<3)
#define GFC_RTCHECK_POINTER     (1<<4)
#define GFC_RTCHECK_MEM         (1<<5)
#define GFC_RTCHECK_ALL        (GFC_RTCHECK_BOUNDS \
				| GFC_RTCHECK_RECURSION | GFC_RTCHECK_DO \
				| GFC_RTCHECK_POINTER | GFC_RTCHECK_MEM)

/* Special unit numbers used to convey certain conditions.  Numbers -4
   thru -9 available.  NEWUNIT values start at -10.  */
#define GFC_INTERNAL_UNIT  -1    /* KIND=1 Internal Unit.  */
#define GFC_INTERNAL_UNIT4 -2    /* KIND=4 Internal Unit.  */
#define GFC_INVALID_UNIT   -3

/* Possible values for the CONVERT I/O specifier.  */
typedef enum
{
  GFC_CONVERT_NONE = -1,
  GFC_CONVERT_NATIVE = 0,
  GFC_CONVERT_SWAP,
  GFC_CONVERT_BIG,
  GFC_CONVERT_LITTLE
}
unit_convert;


/* Runtime errors.  */
typedef enum
{
  LIBERROR_FIRST = -3,		/* Marker for the first error.  */
  LIBERROR_EOR = -2,		/* End of record, must be negative.  */
  LIBERROR_END = -1,		/* End of file, must be negative.  */
  LIBERROR_OK = 0,		/* Indicates success, must be zero.  */
  LIBERROR_OS = 5000,		/* OS error, more info in errno.  */
  LIBERROR_OPTION_CONFLICT,
  LIBERROR_BAD_OPTION,
  LIBERROR_MISSING_OPTION,
  LIBERROR_ALREADY_OPEN,
  LIBERROR_BAD_UNIT,
  LIBERROR_FORMAT,
  LIBERROR_BAD_ACTION,
  LIBERROR_ENDFILE,
  LIBERROR_BAD_US,
  LIBERROR_READ_VALUE,
  LIBERROR_READ_OVERFLOW,
  LIBERROR_INTERNAL,
  LIBERROR_INTERNAL_UNIT,
  LIBERROR_ALLOCATION,
  LIBERROR_DIRECT_EOR,
  LIBERROR_SHORT_RECORD,
  LIBERROR_CORRUPT_FILE,
  LIBERROR_INQUIRE_INTERNAL_UNIT, /* Must be different from STAT_STOPPED_IMAGE.  */
  LIBERROR_LAST			/* Not a real error, the last error # + 1.  */
}
libgfortran_error_codes;

/* Must kept in sync with libgfortrancaf.h.  */
typedef enum
{
  GFC_STAT_UNLOCKED = 0,
  GFC_STAT_LOCKED,
  GFC_STAT_LOCKED_OTHER_IMAGE,
  GFC_STAT_STOPPED_IMAGE = 6000, /* See LIBERROR_INQUIRE_INTERNAL_UNIT above. */
  GFC_STAT_FAILED_IMAGE
}
libgfortran_stat_codes;

typedef enum
{
  GFC_CAF_ATOMIC_ADD = 1,
  GFC_CAF_ATOMIC_AND,
  GFC_CAF_ATOMIC_OR,
  GFC_CAF_ATOMIC_XOR
} libcaf_atomic_codes;


/* For CO_REDUCE.  */
#define GFC_CAF_BYREF      (1<<0)
#define GFC_CAF_HIDDENLEN  (1<<1)
#define GFC_CAF_ARG_VALUE  (1<<2)
#define GFC_CAF_ARG_DESC   (1<<3)

/* Default unit number for preconnected standard input and output.  */
#define GFC_STDIN_UNIT_NUMBER 5
#define GFC_STDOUT_UNIT_NUMBER 6
#define GFC_STDERR_UNIT_NUMBER 0


/* FIXME: Increase to 15 for Fortran 2008. Also needs changes to
   GFC_DTYPE_RANK_MASK. See PR 36825.  */
#define GFC_MAX_DIMENSIONS 7

#define GFC_DTYPE_RANK_MASK 0x07
#define GFC_DTYPE_TYPE_SHIFT 3
#define GFC_DTYPE_TYPE_MASK 0x38
#define GFC_DTYPE_SIZE_SHIFT 6

/* Basic types.  BT_VOID is used by ISO C Binding so funcs like c_f_pointer
   can take any arg with the pointer attribute as a param.  These are also
   used in the run-time library for IO.  */
typedef enum
{ BT_UNKNOWN = 0, BT_INTEGER, BT_LOGICAL, BT_REAL, BT_COMPLEX,
  BT_DERIVED, BT_CHARACTER, BT_CLASS, BT_PROCEDURE, BT_HOLLERITH, BT_VOID,
  BT_ASSUMED, BT_UNION
}
bt;
