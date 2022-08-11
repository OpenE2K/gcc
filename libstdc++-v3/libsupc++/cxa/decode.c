/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1996-2018 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*
Redistribution and use in source and binary forms are permitted
provided that the above copyright notice and this paragraph are
duplicated in all source code forms.  The name of Edison Design
Group, Inc. may not be used to endorse or promote products derived
from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
Any use of this software is at the user's own risk.
*/
/*

decode.c -- Name demangler for C++.

The demangling is intended to work only on names of external entities.
There is some name mangling done for internal entities, or by the
C-generating back end, that this program does not try to decode.

When IA64_ABI is defined as 1, the demangling matches the IA-64 ABI,
which in addition to its use on Itanium is used on a lot of versions of
gcc.
*/

#if COMPILE_DECODE_FOR_LIB_SRC
/*
When COMPILE_DECODE_FOR_LIB_SRC is TRUE, this file is being cross compiled for
inclusion in the runtime library (so that __cxa_demangle is available).
Compiling in this mode implies that IA64_ABI is TRUE (no externally visible
symbols are defined in Cfront mode).  When cross compiling, don't include any
header files from the front end.  Since we don't have access to the settings of
the front end configuration macros, make sure the ones we use have been
defined.
*/

#ifndef IA64_ABI
#define IA64_ABI 1
#else /* defined(IA64_ABI) */
#if !IA64_ABI
 #error IA64_ABI macro must be TRUE when COMPILE_DECODE_FOR_LIB_SRC is TRUE
#endif /* !IA64_ABI */
#endif /* ifndef IA64_ABI */

#ifndef DEFAULT_EMULATE_GNU_ABI_BUGS
 #error DEFAULT_EMULATE_GNU_ABI_BUGS macro must be set when \
        COMPILE_DECODE_FOR_LIB_SRC is TRUE
#endif /* ifndef DEFAULT_EMULATE_GNU_ABI_BUGS */

#ifndef USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
 #error USE_LONG_DOUBLE_FOR_HOST_FP_VALUE macro must be set when \
        COMPILE_DECODE_FOR_LIB_SRC is TRUE
#endif /* ifndef USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */

#include "basics.h"   /* Includes version in lib_src directory. */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#ifndef sizeof_t
typedef size_t sizeof_t;
#endif /* ifndef sizeof_t */
#ifndef true_size_t
typedef size_t true_size_t;
#endif /* ifndef true_size_t */

#else /* !COMPILE_DECODE_FOR_LIB_SRC */

#include "basics.h"   /* Includes version in src directory. */
#include "host_envir.h"
#if IA64_ABI
#include "targ_def.h" /* For DEFAULT_EMULATE_GNU_ABI_BUGS and others. */
#endif /* IA64_ABI */
#include "decode.h"

#endif /* COMPILE_DECODE_FOR_LIB_SRC */

/*
Block used to hold state variables.  A block is used so that these routines
will be reentrant.
*/
typedef struct a_decode_control_block *a_decode_control_block_ptr;
typedef struct a_decode_control_block {
  char		*output_id;
			/* Pointer to buffer for demangled version of
			   the current identifier. */
  sizeof_t	output_id_len;
			/* Length of output_id, not counting the final
			   null. */
  sizeof_t	output_id_size;
			/* Allocated size of output_id. */
  a_boolean	err_in_id;
			/* TRUE if any error was encountered in the current
			   identifier. */
  a_boolean	output_overflow_err;
			/* TRUE if the demangled output overflowed the
			   output buffer. */
  unsigned long	suppress_id_output;
			/* If > 0, demangled id output is suppressed.  This
			   might be because of an error or just as a way
			   of avoiding output during some processing. */
  sizeof_t	uncompressed_length;
			/* If non-zero, the original name was compressed,
			   and this indicates the length of the uncompressed
			   (but still mangled) name. */
#if !IA64_ABI
  a_const_char	*end_of_name;
			/* Set to the character position just after the end of
			   the mangled name.  When sections with indicated
			   lengths are scanned, set temporarily to just after
			   that section of the name. */
  unsigned long	mangling_nesting_level;
			/* The nesting level of calls to
			   full_demangle_identifier.  Used to ensure that
			   template parameter names for recursive calls are
			   given different nesting_levels (and therefore
			   different template parameter names). */
#else /* IA64_ABI */
  unsigned long	suppress_substitution_recording;
			/* If > 0, suppress recording of substitutions. */
  a_boolean	contains_conversion_operator;
			/* TRUE if the name being demangled contains a
			   conversion operator (i.e., "cv <type>").  Such
			   names may require a second pass at demangling
			   if the first pass ends in failure. */
  a_boolean	parse_template_args_after_conversion_operator;
			/* TRUE if template arguments should be parsed
			   as part of the type following a templated conversion
			   operator.  The initial attempt at demangling uses
			   a value of FALSE, but a subsequent attempt will
			   have this field set to TRUE. */
#endif /* IA64_ABI */
} a_decode_control_block;


static void clear_control_block(a_decode_control_block_ptr dctl)
/*
Clear a decoding control block.
*/
{
  dctl->output_id = NULL;
  dctl->output_id_len = 0;
  dctl->output_id_size = 0;
  dctl->err_in_id = FALSE;
  dctl->output_overflow_err = FALSE;
  dctl->suppress_id_output = 0;
  dctl->uncompressed_length = 0;
#if !IA64_ABI
  dctl->end_of_name = NULL;
  dctl->mangling_nesting_level = 0;
#else /* IA64_ABI */
  dctl->suppress_substitution_recording = 0;
  dctl->contains_conversion_operator = FALSE;
  dctl->parse_template_args_after_conversion_operator = FALSE;
#endif /* IA64_ABI */
}  /* clear_control_block */

#if !IA64_ABI

/*
Block that contains information used to control the output of template
parameter lists.
*/
typedef struct a_template_param_block *a_template_param_block_ptr;
typedef struct a_template_param_block {
  unsigned long	nesting_level;
			/* Number of levels of template nesting at this
			   point (1 == top level). */
  a_const_char	*final_specialization;
			/* Set to point to the mangled encoding for the final
			   specialization encountered while working from
			   outermost template to innermost.  NULL if
			   no specialization has been found yet. */
  a_boolean	set_final_specialization;
			/* TRUE if final_specialization should be set while
			   scanning. */
  a_boolean	actual_template_args_until_final_specialization;
			/* TRUE if template parameter names should not be
			   put out.  Reset when the final_specialization
			   position is reached. */
  a_boolean	output_only_correspondences;
			/* TRUE if doing a post-pass to output only template
			   parameter/argument correspondences and not
			   anything else.  suppress_id_output will have been
			   incremented to suppress everything else, and
			   gets decremented temporarily when correspondences
			   are output. */
  a_boolean	first_correspondence;
			/* TRUE until the first template parameter/argument
			   correspondence is put out. */
  a_boolean	use_old_form_for_template_output;
			/* TRUE if templates should be output in the old
			   form that always puts actual argument values
			   in template argument lists. */
} a_template_param_block;


/*
Declarations needed because of forward references:
*/
static a_const_char *demangle_identifier_with_preceding_length(
                     a_const_char               *ptr,
                     a_boolean                  suppress_parent_and_local_info,
                     a_decode_control_block_ptr dctl);
static a_const_char *demangle_operation(a_const_char               *ptr,
                                        a_boolean                  need_parens,
                                        a_decode_control_block_ptr dctl);
static a_const_char *demangle_operator(a_const_char               *ptr,
                               int                        *mangled_length,
                               a_boolean                  *takes_type,
                               a_boolean                  *is_new_style_cast,
                               a_boolean                  *is_postfix,
                               a_boolean                  *need_adl_parens,
                               a_boolean                  *is_initializer_list,
                               a_boolean                  *ud_suffix_follows,
                               a_decode_control_block_ptr dctl);
static a_const_char *demangle_type(a_const_char               *ptr,
                                   a_decode_control_block_ptr dctl);
static a_const_char *full_demangle_type_name(
                                 a_const_char               *ptr,
                                 a_boolean                  base_name_only,
                                 a_template_param_block_ptr temp_par_info,
                                 a_boolean                  is_destructor_name,
                                 a_decode_control_block_ptr dctl);
static a_const_char *demangle_template_arguments(
                            a_const_char               *ptr,
                            a_boolean                  emit_arg_values,
                            a_boolean                  suppress_angle_brackets,
                            a_template_param_block_ptr temp_par_info,
                            a_decode_control_block_ptr dctl);
static a_boolean is_mangled_type_name(a_const_char               *ptr,
                                      a_decode_control_block_ptr dctl);
static a_const_char *demangle_name(
                                a_const_char               *ptr,
                                unsigned long              nchars,
                                a_boolean                  stop_on_underscores,
                                unsigned long              *nchars_left,
                                a_const_char               *mclass,
                                a_template_param_block_ptr temp_par_info,
                                a_boolean                  *instance_emitted,
                                a_decode_control_block_ptr dctl);
static a_const_char *demangle_name_with_preceding_length(
                                              a_const_char               *ptr,
                                              a_decode_control_block_ptr dctl);
static a_const_char *demangle_ref_qualifiers(
                                         a_const_char               *p,
                                         a_const_char               **ref_qual,
                                         a_decode_control_block_ptr dctl);
static a_const_char *demangle_type_qualifiers(
                                     a_const_char               *ptr,
                                     a_boolean                  trailing_space,
                                     a_decode_control_block_ptr dctl);

/*
Interface to full_demangle_type_name for the simple case.
*/
#define demangle_type_name(ptr, dctl)                                 \
  full_demangle_type_name((ptr), /*base_name_only=*/FALSE,            \
                          /*temp_par_info=*/(a_template_param_block_ptr)NULL, \
                          /*is_destructor_name=*/FALSE,               \
                          (dctl))
static a_const_char *full_demangle_identifier(
                     a_const_char               *ptr,
                     unsigned long              nchars,
                     a_boolean                  suppress_parent_and_local_info,
                     a_decode_control_block_ptr dctl);
/* Interface to full_demangle_identifier for the simple case. */
#define demangle_identifier(ptr, dctl)                                \
  full_demangle_identifier((ptr), (unsigned long)0, FALSE, (dctl))

#endif /* !IA64_ABI */

static void write_id_ch(char                       ch,
                        a_decode_control_block_ptr dctl)
/*
Add the indicated character to the demangled version of the current identifier.
*/
{
  if (!dctl->suppress_id_output) {
    if (!dctl->output_overflow_err) {
      /* Test for buffer overflow, leaving room for a terminating null. */
      if (dctl->output_id_len+1 >= dctl->output_id_size) {
        /* There's no room for the character in the buffer. */
        dctl->output_overflow_err = TRUE;
        /* Make sure the (truncated) output is null-terminated. */
        if (dctl->output_id_size != 0) {
          dctl->output_id[dctl->output_id_size-1] = '\0';
        }  /* if */
      } else {
        /* No overflow; put the character in the buffer. */
        dctl->output_id[dctl->output_id_len] = ch;
      }  /* if */
    }  /* if */
    /* Keep track of the number of characters (even if output has overflowed
       the buffer). */
    dctl->output_id_len++;
  }  /* if */
}  /* write_id_ch */


static void write_id_str(a_const_char              *str,
                        a_decode_control_block_ptr dctl)
/*
Add the indicated string to the demangled version of the current identifier.
*/
{
  a_const_char *p = str;

  if (!dctl->suppress_id_output) {
    for (; *p != '\0'; p++) write_id_ch(*p, dctl);
  }  /* if */
}  /* write_id_str */


static void write_id_number(unsigned long              num,
                            a_decode_control_block_ptr dctl)
/*
Utility to write the specified non-negative number to the demangled version
of the current identifier.
*/
{
  char          buffer[50];

  (void)sprintf(buffer, "%lu", num);
  write_id_str(buffer, dctl);
}  /* write_id_number */

#if IA64_ABI

static void write_id_signed_number(long                       num,
                                   a_decode_control_block_ptr dctl)
/*
Utility to write the specified signed number to the demangled version
of the current identifier.
*/
{
  char          buffer[50];

  (void)sprintf(buffer, "%ld", num);
  write_id_str(buffer, dctl);
}  /* write_id_signed_number */

#endif /* IA64_ABI */

static void bad_mangled_name(a_decode_control_block_ptr dctl)
/*
A bad name mangling has been encountered.  Record an error.
*/
{
  if (!dctl->err_in_id) {
    dctl->err_in_id = TRUE;
    dctl->suppress_id_output++;
#if IA64_ABI
    dctl->suppress_substitution_recording++;
#endif /* IA64_ABI */
  }  /* if */
}  /* bad_mangled_name */

#if IA64_ABI

/*ARGSUSED*/
static a_const_char get_char(a_const_char               *ptr,
                             a_decode_control_block_ptr dctl)
/*
Get and return the character pointed to by ptr.  Stub version; this
does nothing in the IA-64 ABI, but it's called from some low-level routines.
*/
{
  return *ptr;
}  /* get_char */


static a_boolean start_of_id_is(a_const_char *str,
                                a_const_char *id)
/*
Return TRUE if the part of the mangled name at id begins with the string str.
*/
{
  a_boolean is_start = FALSE;

  for (;;) {
    char chs = *str++;
    if (chs == '\0') {
      is_start = TRUE;
      break;
    }  /* if */
    if (chs != *id++) break;
  }  /* for */
  return is_start;
}  /* start_of_id_is */

#else /* !IA64_ABI */

static a_const_char get_char(a_const_char               *ptr,
                             a_decode_control_block_ptr dctl)
/*
Get and return the character pointed to by ptr.  However, if that
position is at or beyond dctl->end_of_name, return a null character
instead.
*/
{
  char ch;

  if (ptr >= dctl->end_of_name) {
    ch = '\0';
  } else {
    ch = *ptr;
  }  /* if */
  return ch;
}  /* get_char */


static a_boolean start_of_id_is(a_const_char               *str,
                                a_const_char               *id,
                                a_decode_control_block_ptr dctl)
/*
Return TRUE if the part of the mangled name at id begins with the string str.
*/
{
  a_boolean is_start = FALSE;

  for (;;) {
    char chs = *str++;
    if (chs == '\0') {
      is_start = TRUE;
      break;
    }  /* if */
    if (chs != get_char(id++, dctl)) break;
  }  /* for */
  return is_start;
}  /* start_of_id_is */

#endif /* IA64_ABI */

static a_const_char *advance_past(a_const_char               ch,
                                  a_const_char               *p,
                                  a_decode_control_block_ptr dctl)
/*
The character ch is expected at *p.  If it's there, advance past it.  If
not, call bad_mangled_name.  In either case, return the updated value of p.
*/
{
  if (get_char(p, dctl) == ch) {
    p++;
  } else {
    bad_mangled_name(dctl);
  }  /* if */
  return p;
}  /* advance_past */


static a_const_char *advance_past_underscore(a_const_char               *p,
                                             a_decode_control_block_ptr dctl)
/*
An underscore is expected at *p.  If it's there, advance past it.  If
not, call bad_mangled_name.  In either case, return the updated value of p.
*/
{
  return advance_past('_', p, dctl);
}  /* advance_past_underscore */

#if IA64_ABI
static a_const_char *get_number(a_const_char               *p,
                                long                       *num,
                                a_decode_control_block_ptr dctl);
#else /* !IA64_ABI */
static a_const_char *get_number(a_const_char               *p,
                                unsigned long              *num,
                                a_decode_control_block_ptr dctl);
#endif /* IA64_ABI */

static a_const_char *demangle_module_id(a_const_char               *ptr,
                                        unsigned long              num,
                                        a_const_char               *prefix,
                                        a_decode_control_block_ptr dctl)
/*
Demangle a module id name (an EDG extension), which has the form

        _ <file-name-length> _ <file-name> _ <str1> [ _ <str2> ]

Only the file name part is parsed and put out.  num specifies the number of
characters in the entire module id.  prefix points earlier in the mangled
name to a prefix that may precede the module id (if no such prefix is used,
prefix == NULL).  Return a pointer to the character position following the
entire module id.
*/
{
#if IA64_ABI
  long		num_chars_to_output;
#else /* !IA64_ABI */
  unsigned long num_chars_to_output;
#endif /* IA64_ABI */
  a_const_char  *start;

  if (*ptr != '_' || !isdigit((unsigned char)ptr[1])) {
    /* May not be an EDG module_id, in which case, emit the entire string
       (including any prefix that may have been parsed by the caller). */
    if (prefix != NULL) {
      while (prefix != ptr) write_id_ch(*prefix++, dctl);
    }  /* if */
    num_chars_to_output = num;
    start = ptr;
  } else {
    start = get_number(ptr+1, &num_chars_to_output, dctl);
    if (!dctl->err_in_id) {
      uint32_t prefix_len = (uint32_t)((start-ptr)+1);
      if (*start != '_' ||
#if IA64_ABI
          num_chars_to_output <= 0 ||
#endif /* IA64_ABI */
          num < ((unsigned long)num_chars_to_output + prefix_len)) {
        bad_mangled_name(dctl);
      } else {
        /* Skip the underscore. */
        start++;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!dctl->err_in_id) {
    /* Write the filename (or entire module id). */
    while (num_chars_to_output-- > 0) write_id_ch(*start++, dctl);
  }  /* if */
  return ptr+num;
}  /* demangle_module_id */

#if !IA64_ABI

static a_const_char *get_length(a_const_char               *p,
                                unsigned long              *num,
                                a_const_char               **prev_end,
                                a_decode_control_block_ptr dctl)
/*
Accumulate a number indicating a length, starting at position p, and
return its value in *num.  Return a pointer to the character position
following the number.  dctl->end_of_name is updated to reflect the location
after the end of the entity with the length, and *prev_end is set to the
previous value of dctl->end_of_name for later restoration.
*/
{
  unsigned long n = 0;
  char     ch;

  *prev_end = dctl->end_of_name;
  ch = get_char(p, dctl);
  if (!isdigit((unsigned char)ch)) {
    bad_mangled_name(dctl);
    goto end_of_routine;
  }  /* if */
  do {
    n = n*10 + (ch - '0');
    if (n > (unsigned long)((dctl->end_of_name - p) - 1)) {
      /* Bad number (bigger than the amount of text remaining). */
      bad_mangled_name(dctl);
      n = (unsigned long)((dctl->end_of_name - p) - 1);
      goto end_of_routine;
    }  /* if */
    p++;
    ch = get_char(p, dctl);
  } while (isdigit((unsigned char)ch));
  dctl->end_of_name = p + n;
end_of_routine:
  *num = n;
  return p;
}  /* get_length */


static a_const_char *get_number(a_const_char               *p,
                                unsigned long              *num,
                                a_decode_control_block_ptr dctl)
/*
Accumulate a number starting at position p and return its value in *num.
Return a pointer to the character position following the number.
*/
{
  unsigned long n = 0;
  char     ch;

  ch = get_char(p, dctl);
  if (!isdigit((unsigned char)ch)) {
    bad_mangled_name(dctl);
    goto end_of_routine;
  }  /* if */
  do {
    n = n*10 + (ch - '0');
    p++;
    ch = get_char(p, dctl);
  } while (isdigit((unsigned char)ch));
end_of_routine:
  *num = n;
  return p;
}  /* get_number */


static a_const_char *get_single_digit_number(a_const_char               *p,
                                             unsigned long              *num,
                                             a_decode_control_block_ptr dctl)
/*
Accumulate a number starting at position p and return its value in *num.
The number is a single digit.  Return a pointer to the character position
following the number.
*/
{
  char ch;

  *num = 0;
  ch = get_char(p, dctl);
  if (!isdigit((unsigned char)ch)) {
    bad_mangled_name(dctl);
    goto end_of_routine;
  }  /* if */
  *num = (ch - '0');
  p++;
end_of_routine:
  return p;
}  /* get_single_digit_number */


static a_const_char *get_single_digit_length(
                                         a_const_char               *p,
                                         unsigned long              *num,
                                         a_const_char               **prev_end,
                                         a_decode_control_block_ptr dctl)
/*
Accumulate a length starting at position p and return its value in *num.
The length is a single digit.  Return a pointer to the character position
following the length.  dctl->end_of_name is updated to reflect the location
after the end of the entity with the length, and *prev_end is set to the
previous value of dctl->end_of_name for later restoration.
*/
{
  p = get_single_digit_number(p, num, dctl);
  *prev_end = dctl->end_of_name;
  if (*num > (unsigned long)(dctl->end_of_name - p)) {
    /* Bad length (too large). */
    bad_mangled_name(dctl);
  } else {
    dctl->end_of_name = p + *num;
  }  /* if */
  return p;
}  /* get_single_digit_length */


static a_const_char *get_length_with_optional_underscore(
                                         a_const_char               *p,
                                         unsigned long              *num,
                                         a_const_char               **prev_end,
                                         a_decode_control_block_ptr dctl)
/*
Accumulate a number starting at position p and return its value in *num.
If the number has more than one digit, it is followed by an underscore.
(Or, in a newer representation, surrounded by underscores.)
Return a pointer to the character position following the number.
dctl->end_of_name is updated to reflect the location after the end
of the entity with the length, and *prev_end is set to the previous value
of dctl->end_of_name for later restoration.
*/
{
  if (get_char(p, dctl) == '_') {
    /* New encoding (not from cfront) -- the length is surrounded by
       underscores whether it's a single digit or several digits,
       e.g., "L_10_1234567890". */
    p++;
    /* Multi-digit number followed by underscore. */
    p = get_length(p, num, prev_end, dctl);
    p = advance_past_underscore(p, dctl);
    dctl->end_of_name++;  /* Adjust for underscore. */
  } else if (isdigit((unsigned char)get_char(p, dctl)) &&
             isdigit((unsigned char)get_char(p+1, dctl)) &&
             get_char(p+2, dctl) == '_') {
    /* The cfront version -- a multi-digit length is followed by an
       underscore, e.g., "L10_1234567890".  This doesn't work well because
       something like "L11", intended to have a one-digit length, can
       be made ambiguous by following it by a "_" for some other reason.
       So this form is not used in new cases where that can come up, e.g.,
       nontype template arguments for functions.  In any case, interpret
       "multi-digit" as "2-digit" and don't look further for the underscore. */
    /* Multi-digit number followed by underscore. */
    p = get_length(p, num, prev_end, dctl);
    p = advance_past_underscore(p, dctl);
    dctl->end_of_name++;  /* Adjust for underscore. */
  } else {
    /* Single-digit number not followed by underscore. */
    p = get_single_digit_length(p, num, prev_end, dctl);
  }  /* if */
  return p;
}  /* get_length_with_optional_underscore */


static a_const_char *get_number_with_optional_underscore(
                                               a_const_char               *p,
                                               unsigned long              *num,
                                               a_decode_control_block_ptr dctl)
/*
Accumulate a number starting at position p and return its value in *num.
If the number has more than one digit, it is followed by an underscore.
(Or, in a newer representation, surrounded by underscores.)
Return a pointer to the character position following the number.
Parses the same string as get_length_with_optional_underscore, except that
dctl->end_of_name is not altered (meaning that this routine can be used to
retrieve a count of something other than the number of characters that
immediately follow the number).
*/
{
  if (get_char(p, dctl) == '_') {
    /* New encoding (not from cfront) -- the number is surrounded by
       underscores whether it's a single digit or several digits,
       e.g., "L_10_". */
    p++;
    /* Multi-digit number followed by underscore. */
    p = get_number(p, num, dctl);
    p = advance_past_underscore(p, dctl);
  } else if (isdigit((unsigned char)get_char(p, dctl)) &&
             isdigit((unsigned char)get_char(p+1, dctl)) &&
             get_char(p+2, dctl) == '_') {
    /* The cfront version -- a multi-digit number is followed by an
       underscore, e.g., "L10_".  This doesn't work well because
       something like "L11", intended to have a one-digit length, can
       be made ambiguous by following it by a "_" for some other reason.
       So this form is not used in new cases where that can come up, e.g.,
       nontype template arguments for functions.  In any case, interpret
       "multi-digit" as "2-digit" and don't look further for the underscore. */
    /* Multi-digit number followed by underscore. */
    p = get_number(p, num, dctl);
    p = advance_past_underscore(p, dctl);
  } else {
    /* Single-digit number not followed by underscore. */
    p = get_single_digit_number(p, num, dctl);
  }  /* if */
  return p;
}  /* get_number_with_optional_underscore */


static a_boolean is_immediate_type_qualifier(a_const_char               *p,
                                             a_decode_control_block_ptr dctl)
/*
Return TRUE if the encoding pointed to is one that indicates type
qualification.
*/
{
  a_boolean is_type_qual = FALSE;
  char      ch;

  ch = get_char(p, dctl);
  if (ch == 'C' || ch == 'V' || (ch == 'D' && get_char(p+1, dctl) == 'r')) {
    /* This is a type qualifier. */
    is_type_qual = TRUE;
  }  /* if */
  return is_type_qual;
}  /* is_immediate_type_qualifier */


static a_const_char *remove_immediate_type_qualifiers(
                                              a_const_char               *p,
                                              a_decode_control_block_ptr dctl)
/*
Return a pointer to the mangled name after removing any type qualifiers
that might be present.
*/
{
  while (is_immediate_type_qualifier(p, dctl)) {
    if (get_char(p, dctl) == 'D' && get_char(p+1, dctl) == 'r') {
      /* Two-character qualifier. */
      p+=2;
    } else {
      /* One-character qualifier. */
      p++;
    }  /* if */
  }  /* while */
  return p;
}  /* remove_immediate_type_qualifiers */


static void write_template_parameter_name(unsigned long              depth,
                                          unsigned long              position,
                                          a_boolean                  nontype,
                                          a_decode_control_block_ptr dctl)
/*
Output a representation of a template parameter with depth and position
as indicated.  It's a nontype parameter if nontype is TRUE.
*/
{
  char buffer[100];
  char letter = '\0';

  if (nontype) {
    /* Nontype parameter. */
    /* Use a code letter for the first few levels, then the depth number. */
    if (depth == 1) {
      letter = 'N';
    } else if (depth == 2) {
      letter = 'O';
    } else if (depth == 3) {
      letter = 'P';
    }  /* if */
    if (letter != '\0') {
      (void)sprintf(buffer, "%c%lu", letter, position);
    } else {
      (void)sprintf(buffer, "N_%lu_%lu", depth, position);
    }  /* if */
  } else {
    /* Normal type parameter. */
    /* Use a code letter for the first few levels, then the depth number. */
    if (depth == 1) {
      letter = 'T';
    } else if (depth == 2) {
      letter = 'U';
    } else if (depth == 3) {
      letter = 'V';
    }  /* if */
    if (letter != '\0') {
      (void)sprintf(buffer, "%c%lu", letter, position);
    } else {
      (void)sprintf(buffer, "T_%lu_%lu", depth, position);
    }  /* if */
  }  /* if */
  write_id_str(buffer, dctl);
}  /* write_template_parameter_name */


static a_const_char *demangle_template_parameter_name(
                                            a_const_char               *ptr,
                                            a_boolean                  nontype,
                                            a_decode_control_block_ptr dctl)
/*
Demangle a template parameter name at the indicated location.  The parameter
is a nontype parameter if nontype is TRUE.  Return a pointer to the character
position following what was demangled.
*/
{
  a_const_char  *p = ptr;
  unsigned long position, depth = 1;

  /* This comes up with the modern mangling for template functions.
     Form is "ZnZ" or "Zn_mZ", where n is the parameter number and m
     is the depth number (1 if not specified). */
  p++;  /* Advance past the "Z". */
  /* Get the position number. */
  p = get_number(p, &position, dctl);
  if (get_char(p, dctl) == '_' && get_char(p+1, dctl) != '_') {
    /* Form including depth ("Zn_mZ"). */
    p++;
    p = get_number(p, &depth, dctl);
  }  /* if */
  /* Output the template parameter name. */
  write_template_parameter_name(depth, position, nontype, dctl);
  if (get_char(p  , dctl) == '_' &&
      get_char(p+1, dctl) == '_' &&
      get_char(p+2, dctl) == 't' &&
      get_char(p+3, dctl) == 'm' &&
      get_char(p+4, dctl) == '_' &&
      get_char(p+5, dctl) == '_') {
    /* A template template parameter followed by a template
       argument list. */
    p = demangle_template_arguments(p+6, /*emit_arg_values=*/FALSE,
                                    /*suppress_angle_brackets=*/FALSE,
                                    (a_template_param_block_ptr)NULL, dctl);
  }  /* if */
  /* Check for the final "Z".  This appears in the mangling to avoid
     ambiguities when the template parameter is followed by something whose
     encoding begins with a digit, e.g., a class name. */
  if (get_char(p, dctl) != 'Z') {
    bad_mangled_name(dctl);
  } else {
    p++;
  }  /* if */
  return p;
}  /* demangle_template_parameter_name */


static a_const_char *demangle_constant_value(
                                         a_const_char               *ptr,
                                         a_boolean                  is_bool,
                                         a_boolean                  is_nullptr,
                                         a_decode_control_block_ptr dctl)
/*
Demangle a constant value that is part of a literal.  The form of the
constant has an initial length (which may or may not use the new underscore
for specifying the length), followed by some number of characters, for example:

  3n12     encoding for -12
   ^^^---- Characters of constant.  Some characters get remapped:
             n --> -
             p --> +
             d --> .
  ^------- Length of constant (may or may not include underscores).

When is_bool is TRUE, emit "true"/"false" instead of 1/0.  Likewise when
is_nullptr is TRUE (emits "nullptr" rather than 0).
*/
{
  a_const_char  *p = ptr, *prev_end;
  char          ch;
  unsigned long nchars;
  a_boolean     is_nonzero = FALSE;

  /* Get the length of the constant. */
  p = get_length_with_optional_underscore(p, &nchars, &prev_end, dctl);
  /* Process the characters of the literal constant. */
  for (; nchars > 0; nchars--, p++) {
    /* Remap characters where necessary. */
    ch = get_char(p, dctl);
    switch (ch) {
      case '\0':
      case '_':
        /* Ran off end of string. */
        bad_mangled_name(dctl);
        goto end_of_routine;
      case 'p':
        ch = '+';
        break;
      case 'n':
        ch = '-';
        break;
      case 'd':
        ch = '.';
        break;
    }  /* switch */
    if (is_bool) {
      /* For the bool case, just keep track of whether the constant is
         non-zero; true or false will be output later. */
      if (ch != '0') is_nonzero = TRUE;
    } else if (is_nullptr) {
      /* Constant should only ever be zero.  Suppress it. */
    } else {
      /* Normal (non-bool, non-nullptr) case.  Output the character of the
         constant. */
      write_id_ch(ch, dctl);
    }  /* if */
  }  /* for */
  dctl->end_of_name = prev_end;
  if (is_bool) {
    /* For bool, output true or false. */
    write_id_str((char *)(is_nonzero ? "true" : "false"), dctl);
  }  /* if */
  if (is_nullptr) write_id_str("nullptr", dctl);
end_of_routine:
  return p;
}  /* demangle_constant_value */


static a_const_char *demangle_constant(
                                a_const_char               *ptr,
                                a_boolean                  suppress_address_of,
                                a_boolean                  need_parens,
                                a_decode_control_block_ptr dctl)
/*
Demangle a constant (e.g., a nontype template class argument) beginning at
ptr, and output the demangled form.  When suppress_address_of is TRUE, the
ampersand that is normally emitted before an address constant is suppressed
(this is used when demangling expressions where any "address of" operation is
explicit).  When need_parens is TRUE, parentheses are emitted around literals
and expressions (but not addresses or template parameters).  Return a pointer
to the character position following what was demangled.
*/
{
  a_const_char  *p = ptr, *type = NULL, *index, *prev_end, *quals;
  unsigned long nchars;
  char          ch;

  /* A constant has a form like
       CiL15   <-- integer constant 5
           ^-- Literal constant representation.
          ^--- Length of literal constant.
         ^---- L indicates literal constant; c indicates address
               of variable, etc.
       ^^----- Type of template argument, with "const" added.
     A template parameter constant or a constant expression does not have
     the initial "C" and type.
  */
  if (get_char(p, dctl) == 'C') {
    /* Advance past the type. */
    type = p;
    dctl->suppress_id_output++;
    p = demangle_type(p, dctl);
    dctl->suppress_id_output--;
  }  /* if */
  /* The next thing has one of the following forms:
       3abc        Address of "abc".
       L211        Literal constant; length ("2") followed by the characters of
                   the constant ("11").
       LM0_L2n1_1j Pointer-to-member-function constant; the three parts
                   correspond to the triplet of values in the __mptr
                   data structure.
       Z1Z         Template parameter.
       Opl2Z1ZZ2ZO Expression.
  */
  ch = get_char(p, dctl);
  if (isdigit((unsigned char)ch)) {
    /* A name preceded by its length, e.g., "3abc".  Put out "&name". */
    if (!suppress_address_of) write_id_ch('&', dctl);
    /* Process the length and name. */
    p = demangle_identifier_with_preceding_length(
                                      p,
                                      /*suppress_parent_and_local_info=*/FALSE,
                                      dctl);
  } else if (ch == 'L') {
    /* Emit parentheses around the literal if requested. */
    if (need_parens) write_id_ch('(', dctl);
    if (type == NULL) {
      bad_mangled_name(dctl);
    } else if (get_char(p+1, dctl) == 'M') {
      /* Pointer-to-member-function.  The form of the constant is
           LM0_L2n1_1j  Non-virtual function
           LM0_L11_0    Virtual function
           LM0_L10_0    Null pointer
         The three parts match the three components of the __mptr structure:
         (delta, index, function or offset).  The index is -1 for a non-virtual
         function, 0 for a null pointer, and greater than 0 for a virtual
         function.  The index is represented like an integer constant (see
         above).  For virtual functions, the last component is always "0"
         even if the offset is not zero. */
      /* Advance past the "LM". */
      p += 2;
      /* Advance over the first component, ignoring it. */
      while (isdigit((unsigned char)get_char(p, dctl))) p++;
      p = advance_past_underscore(p, dctl);
      /* The index component should be next. */
      if (get_char(p, dctl) != 'L') {
        bad_mangled_name(dctl);
        goto end_of_routine;
      }  /* if */
      p++;
      /* Get the index length. */
      /* Note that get_length_with_optional_underscore is not used because
         this is an ambiguous situation: an underscore follows the index
         value, and there's no way to tell if it's the multi-digit
         indicator for the length or the separator between fields. */
      if (get_char(p, dctl) == '_') {
        /* New-form encoding, no ambiguity. */
        p = get_length_with_optional_underscore(p, &nchars, &prev_end, dctl);
      } else {
        p = get_single_digit_length(p, &nchars, &prev_end, dctl);
      }  /* if */
      /* Remember the start of the index. */
      index = p;
      /* Skip the rest of the index. */
      while (isdigit((unsigned char)get_char(p, dctl)) ||
             (get_char(p, dctl) == 'n')) p++;
      dctl->end_of_name = prev_end;
      p = advance_past_underscore(p, dctl);
      /* If the index number starts with 'n', this is a non-virtual
         function. */
      if (*index == 'n') {
        /* Non-virtual function. */
        /* The third component is a name preceded by its length, e.g.,
           "1f".  Put out "&A::f", where "A" is the class type retrieved
           from the type. */
        write_id_ch('&', dctl);
        /* Start at type+2 to skip the "C" for const and the "M" for
           pointer-to-member. */
        quals = demangle_type_name(type+2, dctl);
        write_id_str("::", dctl);
        /* Demangle the length and name. */
        p = demangle_identifier_with_preceding_length(
                                      p,
                                      /*suppress_parent_and_local_info=*/TRUE,
                                      dctl);
        if (is_immediate_type_qualifier(quals, dctl)) {
          /* Get any optional cv-qualifiers. */
          write_id_ch(' ', dctl);
          quals = demangle_type_qualifiers(quals, /*trailing_space=*/FALSE,
                                           dctl);
        }  /* if */
        if (get_char(quals, dctl) == 'F') {
          /* See if there is a ref-qualifier. */
          a_const_char *ref_qual;
          (void)demangle_ref_qualifiers(quals+1, &ref_qual, dctl);
          if (ref_qual != NULL) {
            write_id_str(ref_qual, dctl);
          }  /* if */
        }  /* if */
      } else {
        /* Not a non-virtual function.  The encoding for the third component
           should be simply "0". */
        if (get_char(p, dctl) != '0') {
          bad_mangled_name(dctl);
          goto end_of_routine;
        }  /* if */
        p++;
        if (nchars == 1 && *index == '0') {
          /* Null pointer constant.  Output "(type)0", that is, a zero cast
             to the pointer-to-member type. */
          write_id_ch('(', dctl);
          (void)demangle_type(type, dctl);
          write_id_str(")0", dctl);
        } else {
          /* Virtual function.  This case can't really be demangled properly,
             because the mangled name doesn't have enough information.
             Output "&A::virtual-function-n". */
          write_id_ch('&', dctl);
          /* Start at type+2 to skip the "C" for const and the "M" for
             pointer-to-member. */
          (void)demangle_type_name(type+2, dctl);
          write_id_str("::", dctl);
          write_id_str("virtual-function-", dctl);
          /* Write the index number. */
          for (; nchars > 0; nchars--, index++) write_id_ch(*index, dctl);
        }  /* if */
      }  /* if */
    } else if (get_char(p+1, dctl) == 'S') {
      /* String literal constant. */
      p+=2;
      if (type == NULL) {
        bad_mangled_name(dctl);
      } else {
        /* The type is the type of the string.  Emit "..." cast to the
           proper type. */
        write_id_ch('(', dctl);
        (void)demangle_type(type+1, dctl);
        write_id_str(")\"...\"", dctl);
      }  /* if */
    } else {
      /* Normal literal constant.  Form is something like
           L3n12     encoding for -12
             ^^^---- Characters of constant.  Some characters get remapped:
                       n --> -
                       p --> +
                       d --> .
            ^------- Length of constant.
         Output is
           (type)constant
         That is, the literal constant preceded by a cast to the right type.
      */
      /* See if the type is bool. */
      a_boolean is_bool = (type+2 == p && *(type+1) == 'b');
      a_boolean is_managed_nullptr = (type+2 == p && *(type+1) == 'j');
      a_boolean is_nullptr = (type+2 == p && *(type+1) == 'n') ||
                             is_managed_nullptr;
      a_boolean is_complex = (type+3 == p && *(type+1) == 'x');
      /* If the type is bool or nullptr, don't put out the cast. */
      if (!(is_bool || is_nullptr)) {
        write_id_ch('(', dctl);
        /* Start at type+1 to avoid the "C" for const. */
        (void)demangle_type(type+1, dctl);
        write_id_ch(')', dctl);
      }  /* if */
      if (is_complex) write_id_ch('(', dctl);
      p++;  /* Advance past the "L". */
      if (is_managed_nullptr) {
        /* If this is a managed C++/CLI __nullptr, emit the underscores to
           distinguish it from the standard nullptr. */
        write_id_str("__", dctl);
      }  /* if */
      p = demangle_constant_value(p, is_bool, is_nullptr, dctl);
      if (!dctl->err_in_id && is_complex) {
        /* Now emit the imaginary portion of the complex number. */
        write_id_ch('+', dctl);
        p = demangle_constant_value(p, /*is_bool=*/FALSE, /*is_nullptr=*/FALSE,
                                    dctl);
        write_id_str("i)", dctl);
      }  /* if */
    }  /* if */
    if (need_parens) write_id_ch(')', dctl);
  } else if (ch == 'Z') {
    /* A template parameter. */
    p = demangle_template_parameter_name(p, /*nontype=*/TRUE, dctl);
  } else if (ch == 'O') {
    /* An operation. */
    p = demangle_operation(p, need_parens, dctl);
  } else {
    /* The constant starts with something unexpected. */
    bad_mangled_name(dctl);
  }  /* if */
end_of_routine:
  return p;
}  /* demangle_constant */

static a_const_char *demangle_parameter_reference(
                                               a_const_char               *ptr,
                                               a_decode_control_block_ptr dctl)
/*
Demangle a function parameter reference (e.g., in a late specified return
type) as pointed to by ptr:

      v-vv----- These are optional.
     IC1_2I <-- "const param#1 two levels up"
          ^---- Terminating non-digit character so parameter number won't run
                into an entity with an initial length.
        ^^----- Number of "levels up" for this parameter (0-based).  Omitted
                if zero.
       ^------- Parameter number (1-based) or 0 for "this".
      ^-------- Optional cv-qualifiers.
     ^--------- "I" indicates parameter reference.
*/
{
  a_const_char  *p = ptr;
  unsigned long num, level = 0;
  char          buffer[60];

  /* Advance past the initial "I" (verified by caller). */
  p++;
  if (is_immediate_type_qualifier(p, dctl)) {
    /* Get any optional cv-qualifiers. */
    p = demangle_type_qualifiers(p, /*trailing_space=*/TRUE, dctl);
  }  /* if */
  p = get_number(p, &num, dctl);
  if (!dctl->err_in_id) {
    if (get_char(p, dctl) != 'I') {
      p = advance_past_underscore(p, dctl);
      if (!dctl->err_in_id) {
        p = get_number(p, &level, dctl);
      }  /* if */
    }  /* if */
  }  /* if */
  if (!dctl->err_in_id) {
    if (num == 0) {
      /* An explicit "this" in a trailing return type. */
      write_id_str("this", dctl);
    } else {
      if (level == 0) {
        (void)sprintf(buffer, "param#%ld", num);
      } else {
        (void)sprintf(buffer, "param#%ld[up %ld level%s]", num, level,
                              level > 1 ? "s" : "");
      }  /* if */
      write_id_str(buffer, dctl);
    }  /* if */
    p = advance_past('I', p, dctl);
  }  /* if */
  return p;
}  /* demangle_parameter_reference */


static a_const_char *demangle_expression(
                                        a_const_char               *ptr,
                                        a_boolean                  need_parens,
                                        a_decode_control_block_ptr dctl)
/*
Demangle an expression; ensure that the expression is enclosed in
parentheses when necessary if need_parens is TRUE (names aren't parenthesized
even when need_parens is TRUE).
*/
{
  a_const_char  *p = ptr;

  if (get_char(p, dctl) == 'I') {
    /* A function parameter reference. */
    p = demangle_parameter_reference(p, dctl);
  } else if (get_char(p, dctl) == '_' && get_char(p+1, dctl) == '_') {
    /* Certain special names can occur here, for example, an operator name
       that appears as the first operand of a call. */
    p = demangle_name(p, (unsigned long)0, /*stop_on_underscores=*/TRUE,
                      (unsigned long *)NULL, (char *)NULL,
                      (a_template_param_block_ptr)NULL, (a_boolean *)NULL,
                      dctl);
    if (get_char(p, dctl) == '_' && get_char(p+1, dctl) == '_') {
      p += 2;
    } else {
      bad_mangled_name(dctl);
    }  /* if */
  } else {
    /* Used to demangle literals as well as template parameters, operations.
       Within an expression, suppress implicit "&"s during the demangling. */
    p = demangle_constant(p, /*suppress_address_of=*/TRUE, need_parens, dctl);
  }  /* if */
  return p;
}  /* demangle_expression */


static a_const_char *demangle_operation(a_const_char               *ptr,
                                        a_boolean                  need_parens,
                                        a_decode_control_block_ptr dctl)
/*
Demangle an operation in a constant expression (these come up in template
arguments and array sizes, in template function parameter lists) beginning
at ptr, and output the demangled form.  When need_parens is TRUE, parentheses
are emitted around the operation.  Return a pointer to the character
position following what was demangled.
*/
{
  a_const_char  *p = ptr, *operator_str, *close_str = "";
  int           op_length;
  unsigned long num_operands, i, num_dimensions;
  a_boolean     takes_type, is_new_style_cast, is_postfix, need_adl_parens;
  a_boolean     has_variable_number_of_operands = FALSE, is_initializer_list;
  a_boolean     is_call = FALSE, is_cli_subscript = FALSE;
  a_boolean     ud_suffix_follows;

  /* An operation has the form
       Opl2Z1ZZ2ZO <-- "Z1 + Z2", Z1/Z2 indicating nontype template parameters.
                 ^---- "O" to end the operation encoding.
              ^^^----- Second operand.
           ^^^-------- First operand.
          ^----------- Count of operands (which may or may not have an
                       initial "_" indicating possibly more than 9 operands).
        ^^------------ Operation, using same encoding as for operator
                       function names.
       ^-------------- "O" for operation.
  */
  p++;  /* Advance past the "O". */
  /* Decode the operator name, e.g., "pl" is "+". */
  operator_str = demangle_operator(p, &op_length, &takes_type,
                                   &is_new_style_cast, &is_postfix,
                                   &need_adl_parens, &is_initializer_list,
                                   &ud_suffix_follows, dctl);
  if (operator_str == NULL) {
    bad_mangled_name(dctl);
  } else {
    /* Put parentheses around the operation if necessary. */
    if (need_parens) write_id_ch('(', dctl);
    if (*operator_str == 'f' && strcmp(operator_str, "fold-ex") == 0) {
      /* A fold expression; handle it here (since it's significantly
         different than a standard operation). */
      a_boolean    unary, left;
      p++;
      switch (get_char(p, dctl)) {
        case 'l': unary = TRUE;  left = TRUE;  break;
        case 'L': unary = FALSE; left = TRUE;  break;
        case 'r': unary = TRUE;  left = FALSE; break;
        case 'R': unary = FALSE; left = FALSE; break;
        default:
          bad_mangled_name(dctl);
          break;
      }  /* switch */
      p++;
      operator_str = demangle_operator(p, &op_length, &takes_type,
                                       &is_new_style_cast, &is_postfix,
                                       &need_adl_parens, &is_initializer_list,
                                       &ud_suffix_follows, dctl);
      if (operator_str == NULL) {
        bad_mangled_name(dctl);
      } else {
        p += op_length;
        write_id_ch('(', dctl);
        if (unary) {
          if (left) {
            write_id_str("...", dctl);
            write_id_str(operator_str, dctl);
            p = demangle_expression(p, /*need_parens=*/FALSE, dctl);
          } else {
            p = demangle_expression(p, /*need_parens=*/FALSE, dctl);
            write_id_str(operator_str, dctl);
            write_id_str("...", dctl);
          }  /* if */
        } else {
          p = demangle_expression(p, /*need_parens=*/FALSE, dctl);
          write_id_str(operator_str, dctl);
          write_id_str("...", dctl);
          write_id_str(operator_str, dctl);
          p = demangle_expression(p, /*need_parens=*/FALSE, dctl);
        }  /* if */
        write_id_ch(')', dctl);
      }  /* if */
      goto skip_operand_loop;
    }  /* if */
    p += op_length;
    if (is_initializer_list) {
      /* An initializer list (with an optional type). */
      if (takes_type) {
        p = demangle_type(p, dctl);
      }  /* if */
      write_id_str(operator_str, dctl);
      has_variable_number_of_operands = TRUE;
      close_str = "}";
    } else if (takes_type) {
      /* For casts, sizeof, __alignof__, __uuidof__, typeid, new, or sizeof...
         get the type. */
      if (strcmp(operator_str, "cast") == 0) {
        a_const_char *num_args_ptr;
        /* A "cast" can have zero or more operands (aside from the type).
           For casts with exactly one operand, emit "(type)arg", but for
           other cases, emit the functional-notation type conversion syntax:
           "type(args)".  Look ahead at the number of arguments to determine
           which case we have. */
        dctl->suppress_id_output++;
        num_args_ptr = demangle_type(p, dctl);
        dctl->suppress_id_output--;
        (void)get_number_with_optional_underscore(num_args_ptr,
                                                  &num_operands, dctl);
        if (!dctl->err_in_id) {
          operator_str = "";
          if (num_operands == 1) {
            /* Output as "(type)arg". */
            write_id_ch('(', dctl);
            p = demangle_type(p, dctl);
            write_id_ch(')', dctl);
          } else {
            /* Output as "type(args)". */
            p = demangle_type(p, dctl);
            write_id_ch('(', dctl);
            has_variable_number_of_operands = TRUE;
            close_str = ")";
          }  /* if */
        }  /* if */
      } else if (strcmp(operator_str, "sizeof(") == 0 ||
                 strcmp(operator_str, "__alignof__(") == 0 ||
                 strcmp(operator_str, "__uuidof(") == 0 ||
                 strcmp(operator_str, "typeid(") == 0 ||
                 strcmp(operator_str, "sizeof...(") == 0) {
        /* These manglings have three forms, dependent on the next character
           in the mangled name.  They're sufficiently different that they
           are handled (mostly separately) here. */
        write_id_str(operator_str, dctl);
        operator_str = "";
        if (get_char(p, dctl) == 'e') {
          /* An "old style expression" where the expression was not
             encoded (and the operand count is zero).  Just note that there
             was an expression and we're done. */
          write_id_str("expr)", dctl);
          p++;
        } else if (get_char(p, dctl) == 'X') {
          /* A "new style expression" where the expression is
             included in the mangled name and will be demangled below. */
          close_str = ")";
          p++;
        } else {
          /* The "type" case; simply decode the type (the mangled
             encoding specifies zero operands -- which are ignored below). */
          p = demangle_type(p, dctl);
          write_id_ch(')', dctl);
        }  /* if */
      } else if (strcmp(operator_str, "::typeid") == 0) {
        /* C++/CLI T::typeid. */
        p = demangle_type(p, dctl);
        write_id_str(operator_str, dctl);
      } else {
        /* Generic processing of items that take a type (e.g., static_cast). */
        write_id_str(operator_str, dctl);
        p = demangle_type(p, dctl);
        if (is_new_style_cast) {
          /* Something like static_cast<type>(expression).  The operator and
             type have been emitted, close the type with a right angle
             bracket and parse the expression below. */
          operator_str = "";
          write_id_str(">(", dctl);
          close_str = ")";
        } else {
          write_id_ch(')', dctl);
        }  /* if */
      }  /* if */
    } else if (strcmp(operator_str, "builtin-operation") == 0) {
      unsigned long kind;
      /* A builtin operation. */
      has_variable_number_of_operands = TRUE;
      write_id_str("builtin-operation-", dctl);
      /* Extract the operation number following the "bi". */
      p = advance_past_underscore(p, dctl);
      p = get_number(p, &kind, dctl);
      if (kind > 99) {
        bad_mangled_name(dctl);
      } else {
        write_id_number(kind, dctl);
      }  /* if */
      p = advance_past_underscore(p, dctl);
      write_id_ch('(', dctl);
      close_str = ")";
    } else if (strcmp(operator_str, "__real(") == 0 ||
               strcmp(operator_str, "__imag(") == 0 ||
               strcmp(operator_str, "noexcept(") == 0) {
      /* These need a closing paren after their operand. */
      close_str = ")";
    } else if (strcmp(operator_str, "()") == 0) {
      /* A call operation.  The first operand is the target of the call,
         the rest are arguments. */
      operator_str = "";
      is_call = TRUE;
      has_variable_number_of_operands = TRUE;
    } else if (strcmp(operator_str, "new") == 0 ||
               strcmp(operator_str, "new[]") == 0) {
      /* new has an optional "g" (indicating that ::new was used), followed by
         an optional initial list of placement expressions, followed
         by a type and then another optional list of initializer
         expressions.  Handle the first expression list and the type here,
         then let the generic loop below handle the initializer list. */
      /* new may have an optional "g" indicating a global scope new. */
      if (get_char(p, dctl) == 'g') {
        p++;
        write_id_str("::", dctl);
      }  /* if */
      write_id_str(operator_str, dctl);
      write_id_ch(' ', dctl);
      operator_str = "";
      has_variable_number_of_operands = TRUE;
      /* Get the count of operands. */
      p = get_number_with_optional_underscore(p, &num_operands, dctl);
      if (num_operands != 0) {
        write_id_ch('(', dctl);
        for (i = 1; i <= num_operands; i++) {
          p = demangle_expression(p, /*need_parens=*/FALSE, dctl);
          if (i != num_operands) write_id_str(", ", dctl);
        }  /* for */
        write_id_str(") ", dctl);
      }  /* if */
      p = demangle_type(p, dctl);
handle_new_operands:
      if (get_char(p, dctl) == 'O') {
        /* There are no initializers; skip the loop below. */
        goto skip_operand_loop;
      }  /* if */
      if (get_char(p, dctl) == 'b' && get_char(p+1, dctl) == 'i') {
        /* A brace-enclosed initializer list. */
        p += 2;
        write_id_ch('{', dctl);
        close_str = "}";
      } else {
        /* A parenthesized initializer list. */
        write_id_ch('(', dctl);
        close_str = ")";
      }  /* if */
    } else if (strcmp(operator_str, "gcnew") == 0) {
      /* C++/CLI gcnew. */
      write_id_str(operator_str, dctl);
      write_id_ch(' ', dctl);
      operator_str = "";
      has_variable_number_of_operands = TRUE;
      /* Get the count of dimensions. */
      p = get_number_with_optional_underscore(p, &num_dimensions, dctl);
      if (num_dimensions == 0) {
        /* Non-array case. */
        p = demangle_type(p, dctl);
      } else {
        a_const_char *dim_p = p;
        /* Array case; emit the dimensions (but emit the type first). */
        dctl->suppress_id_output++;
        for (i = 1; i <= num_dimensions; i++) {
          p = demangle_expression(p, /*need_parens=*/FALSE, dctl);
        }  /* for */
        dctl->suppress_id_output--;
        p = demangle_type(p, dctl);
        write_id_ch('(', dctl);
        for (i = 1; i <= num_dimensions; i++) {
          dim_p = demangle_expression(dim_p, /*need_parens=*/FALSE, dctl);
          if (i != num_dimensions) write_id_str(", ", dctl);
        }  /* for */
        write_id_str(") ", dctl);
      }  /* if */
      goto handle_new_operands;
    } else if (strcmp(operator_str, "delete") == 0 ||
               strcmp(operator_str, "delete[]") == 0) {
      /* delete may have an optional "g" indicating a global scope delete. */
      if (get_char(p, dctl) == 'g') {
        p++;
        write_id_str("::", dctl);
      }  /* if */
    } else if (strcmp(operator_str, "subscript") == 0) {
      /* A C++/CLI subscript operation (with a variable number of operands). */
      has_variable_number_of_operands = TRUE;
      is_cli_subscript = TRUE;
    }  /* if */
    /* Get the count of operands. */
    p = get_number_with_optional_underscore(p, &num_operands, dctl);
    /* Some operations (e.g., sizeof(type), __alignof__(type), etc.) take
       zero operands. */
    if (num_operands != 0) {
      if (has_variable_number_of_operands) {
        /* Operation has a variable number of operations, and
           they may be type operands (i.e., builtin-operation). */
        for (i = 1; i <= num_operands; i++) {
          if (get_char(p, dctl) == 'T') {
            /* Type operand. */
            p = demangle_type(p+1, dctl);
          } else {
            p = demangle_expression(p, need_adl_parens, dctl);
          }  /* if */
          if (is_call) {
            /* This is a call to the target just emitted; the rest are
               arguments. */
            write_id_str("(", dctl);
            close_str = ")";
            is_call = FALSE;
          } else if (is_cli_subscript) {
            /* This is a C++/CLI subscript operation, we've just emitted
               the array, the remaining operands are subscripts. */
            write_id_str("[", dctl);
            close_str = "]";
            is_cli_subscript = FALSE;
          } else if (i != num_operands) {
            write_id_str(", ", dctl);
          }  /* if */
        }  /* for */
      } else {
        /* Normal case, i.e., the operation has one, two, or three operands
           (and isn't an operation that has a variable number of operands --
           some of which may be types -- like a builtin-operation). */
        if (num_operands == 1 && !is_postfix) {
          /* Prefix unary operator -- operator comes first. */
          write_id_str(operator_str, dctl);
          if (strcmp(operator_str, "delete") == 0 ||
              strcmp(operator_str, "delete[]") == 0) {
            /* Add a space to separate from expression. */
            write_id_ch(' ', dctl);
          }  /* if */
        }  /* if */
        /* Process the first operand. */
        p = demangle_expression(p, /*need_parens=*/TRUE, dctl);
        if (num_operands == 1 && is_postfix) {
          /* Postfix unary operator -- operator comes last. */
          write_id_str(operator_str, dctl);
        }  /* if */
        if (num_operands > 1) {
          /* Binary and ternary operators -- operator comes after first
             operand. */
          if (strcmp(operator_str, "[]") == 0) {
            /* For subscripting, put one "[" between the operands and one
               at the end. */
            operator_str = "[";
            close_str = "]";
          }  /* if */
          write_id_str(operator_str, dctl);
          /* Process the second operand. */
          p = demangle_expression(p, /*need_parens=*/TRUE, dctl);
          if (num_operands > 2) {
            /* Ternary operand -- "?". */
            write_id_ch(':', dctl);
            /* Process the third operand. */
            p = demangle_expression(p, /*need_parens=*/TRUE, dctl);
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (strcmp(operator_str, "throw ") == 0) {
      /* A rethrow has no operands, just put out the string. */
      write_id_str(operator_str, dctl);
    }  /* if */
    write_id_str(close_str, dctl);
skip_operand_loop:
    if (need_parens) write_id_ch(')', dctl);
    /* Check for the final "O". */
    if (get_char(p, dctl) != 'O') {
      bad_mangled_name(dctl);
    } else {
      p++;
    }  /* if */
  }  /* if */
  return p;
}  /* demangle_operation */


static void clear_template_param_block(a_template_param_block_ptr tpbp)
/*
Clear the fields of the indicated template parameter block.
*/
{
  tpbp->nesting_level = 0;
  tpbp->final_specialization = NULL;
  tpbp->set_final_specialization = FALSE;
  tpbp->actual_template_args_until_final_specialization = FALSE;
  tpbp->output_only_correspondences = FALSE;
  tpbp->first_correspondence = FALSE;
  tpbp->use_old_form_for_template_output = FALSE;
}  /* clear_template_param_block */


static a_const_char *demangle_template_arguments(
                            a_const_char               *ptr,
                            a_boolean                  emit_arg_values,
                            a_boolean                  suppress_angle_brackets,
                            a_template_param_block_ptr temp_par_info,
                            a_decode_control_block_ptr dctl)
/*
Demangle the template class arguments or template parameter pack beginning at
ptr and output the demangled form.  Return a pointer to the character position
following what was demangled.  ptr points to just past the "__tm__", "__ps__",
"__pt__", or "__pk__" string.  emit_arg_values is TRUE if the template
argument "values" (i.e., type or nontype value) should be emitted rather than
the template parameter name.  This is used for a partial-specialization
parameter list ("__ps__") or parameter pack ("__pk__").  Angle brackets are
suppressed when suppress_angle_brackets is TRUE (when called to demangle
a parameter pack).  When temp_par_info != NULL, it points to a block that
controls output of extra information on template parameters.
*/
{
  a_const_char  *p = ptr, *arg_base, *prev_end;
  char          ch;
  unsigned long nchars, position;
  a_boolean     nontype, skipped, unskipped, is_pack;

  if (temp_par_info != NULL && !emit_arg_values) {
    temp_par_info->nesting_level++;
  }  /* if */
  /* A template argument list looks like
       __tm__3_ii
               ^^---- Argument types.
             ^------- Size of argument types, including the underscore.
             ^------- ptr points here.
     For the first argument list of a partial specialization, "__tm__" is
     replaced by "__ps__".  For old-form mangling of templates, "__tm__"
     is replaced by "__pt__".  Template arguments can be either nontype
     (as identified by an "X"), a template argument pack (as identified
     by "__pk__"), or types (otherwise).
  */
  if (!suppress_angle_brackets) write_id_ch('<', dctl);
  /* Scan the size. */
  p = get_length(p, &nchars, &prev_end, dctl);
  arg_base = p;
  p = advance_past_underscore(p, dctl);
  /* Loop to process the arguments. */
  for (position = 1;; position++) {
    /* Check for zero arguments case. */
    if ((unsigned long)(p - arg_base) >= nchars) break;
    if (dctl->err_in_id) break;  /* Avoid infinite loops on errors. */
    if (start_of_id_is("__pk__", p, dctl)) {
      /* Template argument packs are encoded much like a template argument
         list, except "__pk__" is used to introduce them:
            __pk__3_sc
                    ^^---- Argument types.
                  ^------- Size of argument types, including the underscore.
      */
      is_pack = TRUE;
      p+=6; /* Advance past the "__pk__". */
    } else {
      is_pack = FALSE;
    }  /* if */
    ch = get_char(p, dctl);
    if (ch == '\0' || (ch == '_' && !is_pack)) {
      /* We ran off the end of the string. */
      bad_mangled_name(dctl);
      break;
    }  /* if */
    /* "X" identifies the beginning of a nontype argument. */
    nontype = (ch == 'X');
    skipped = unskipped = FALSE;
    if (!emit_arg_values && temp_par_info != NULL &&
        !temp_par_info->use_old_form_for_template_output &&
        !temp_par_info->actual_template_args_until_final_specialization) {
      /* Doing something special: writing out the template parameter name. */
      if (temp_par_info->output_only_correspondences) {
        /* This is the second pass, which writes out parameter/argument
           correspondences, e.g., "T1=int".  Output has been suppressed
           in general, and is turned on briefly here. */
        dctl->suppress_id_output--;
        unskipped = TRUE;
        /* Put out a comma between entries and a left bracket preceding the
           first entry. */
        if (temp_par_info->first_correspondence) {
          write_id_str(" [with ", dctl);
          temp_par_info->first_correspondence = FALSE;
        } else {
          write_id_str(", ", dctl);
        }  /* if */
      }  /* if */
      /* Write the template parameter name. */
      write_template_parameter_name(temp_par_info->nesting_level +
                                                  dctl->mangling_nesting_level,
                                    position,
                                    nontype, dctl);
      if (temp_par_info->output_only_correspondences) {
        /* This is the second pass, to write out correspondences, so put the
           argument value out after the parameter name. */
        if (is_pack) {
          /* Indicate this is a pack (only in the correspondences). */
          write_id_str("...", dctl);
        }  /* if */
        write_id_ch('=', dctl);
      } else {
        /* This is the first pass.  The argument value is skipped.  In
           the second pass, its value will be written out. */
        /* We still have to scan over the argument value, but suppress
           output. */
        dctl->suppress_id_output++;
        skipped = TRUE;
      }  /* if */
    }  /* if */
    /* Write the argument value. */
    if (nontype) {
      /* Nontype argument. */
      p++;  /* Advance past the "X". */
      p = demangle_constant(p, /*suppress_address_of=*/FALSE,
                            /*need_parens=*/FALSE, dctl);
    } else if (is_pack) {
      /* A template argument pack. */
      a_template_param_block pack_temp_par_info;
      clear_template_param_block(&pack_temp_par_info);
      /* Recurse to handle the template argument pack. */
      p = demangle_template_arguments(p, /*emit_arg_values=*/TRUE,
                                      /*suppress_angle_brackets=*/TRUE,
                                      &pack_temp_par_info, dctl);
    } else {
      /* Type argument. */
      p = demangle_type(p, dctl);
    }  /* if */
    if (skipped) dctl->suppress_id_output--;
    if (unskipped) dctl->suppress_id_output++;
    /* Stop after the last argument. */
    if ((unsigned long)(p - arg_base) >= nchars) break;
    write_id_str(", ", dctl);
  }  /* for */
  dctl->end_of_name = prev_end;
  if (!suppress_angle_brackets) write_id_ch('>', dctl);
  return p;
}  /* demangle_template_arguments */


static a_const_char *demangle_operator(
                               a_const_char               *ptr,
                               int                        *mangled_length,
                               a_boolean                  *takes_type,
                               a_boolean                  *is_new_style_cast,
                               a_boolean                  *is_postfix,
                               a_boolean                  *need_adl_parens,
                               a_boolean                  *is_initializer_list,
                               a_boolean                  *ud_suffix_follows,
                               a_decode_control_block_ptr dctl)
/*
Examine the first few characters at ptr to see if they are an encoding for
an operator (e.g., "pl" for plus).  If so, return a pointer to a string for
the operator (e.g., "+"), set *mangled_length to the number of characters
in the encoding, and *takes_type to TRUE if the operator takes a type
modifier (e.g., cast).  *is_new_style_cast is set to TRUE if the operator
is a new style cast (and needs a closing '>' and expression emitted).
*is_postfix is set to TRUE if the operator is a postfix operator (unary
operators are typically emitted as prefix).  *need_adl_parens is set to TRUE
if the operator is a call that requires parentheses to suppress ADL.
*is_initializer_list is set to TRUE if the operator is an initializer list.
*ud_suffix_follows is set to TRUE if a literal operator (i.e., operator "")
is scanned and the ud-suffix follows the decoded operator name (which
must then be demangled by the caller).
If the first few characters are not an operator encoding, return NULL.
*/
{
  a_const_char *s;
  int  len = 2;

  *takes_type = FALSE;
  *is_new_style_cast = FALSE;
  *is_postfix = FALSE;
  *need_adl_parens = FALSE;
  *is_initializer_list = FALSE;
  *ud_suffix_follows = FALSE;
  /* The length-3 codes are tested first to avoid taking their first two
     letters as one of the length-2 codes. */
  if (start_of_id_is("apl", ptr, dctl)) {
    s = "+=";
    len = 3;
  } else if (start_of_id_is("ami", ptr, dctl)) {
    s = "-=";
    len = 3;
  } else if (start_of_id_is("amu", ptr, dctl)) {
    s = "*=";
    len = 3;
  } else if (start_of_id_is("adv", ptr, dctl)) {
    s = "/=";
    len = 3;
  } else if (start_of_id_is("amd", ptr, dctl)) {
    s = "%=";
    len = 3;
  } else if (start_of_id_is("aer", ptr, dctl)) {
    s = "^=";
    len = 3;
  } else if (start_of_id_is("aad", ptr, dctl)) {
    s = "&=";
    len = 3;
  } else if (start_of_id_is("aor", ptr, dctl)) {
    s = "|=";
    len = 3;
  } else if (start_of_id_is("ars", ptr, dctl)) {
    s = ">>=";
    len = 3;
  } else if (start_of_id_is("als", ptr, dctl)) {
    s = "<<=";
    len = 3;
  } else if (start_of_id_is("ppe", ptr, dctl)) {
    s = "++";
    len = 3;
  } else if (start_of_id_is("mme", ptr, dctl)) {
    s = "--";
    len = 3;
  } else if (start_of_id_is("nwa", ptr, dctl)) {
    s = "new[]";
    len = 3;
  } else if (start_of_id_is("dla", ptr, dctl)) {
    s = "delete[]";
    len = 3;
  } else if (start_of_id_is("nw", ptr, dctl)) {
    s = "new";
  } else if (start_of_id_is("gc", ptr, dctl)) {
    s = "gcnew";
  } else if (start_of_id_is("dl", ptr, dctl)) {
    s = "delete";
  } else if (start_of_id_is("pl", ptr, dctl)) {
    s = "+";
  } else if (start_of_id_is("mi", ptr, dctl)) {
    s = "-";
  } else if (start_of_id_is("ml", ptr, dctl)) {
    s = "*";
  } else if (start_of_id_is("dv", ptr, dctl)) {
    s = "/";
  } else if (start_of_id_is("md", ptr, dctl)) {
    s = "%";
  } else if (start_of_id_is("er", ptr, dctl)) {
    s = "^";
  } else if (start_of_id_is("ad", ptr, dctl)) {
    s = "&";
  } else if (start_of_id_is("or", ptr, dctl)) {
    s = "|";
  } else if (start_of_id_is("co", ptr, dctl)) {
    s = "~";
  } else if (start_of_id_is("nt", ptr, dctl)) {
    s = "!";
  } else if (start_of_id_is("as", ptr, dctl)) {
    s = "=";
  } else if (start_of_id_is("lt", ptr, dctl)) {
    s = "<";
  } else if (start_of_id_is("gt", ptr, dctl)) {
    s = ">";
  } else if (start_of_id_is("ls", ptr, dctl)) {
    s = "<<";
  } else if (start_of_id_is("rs", ptr, dctl)) {
    s = ">>";
  } else if (start_of_id_is("eq", ptr, dctl)) {
    s = "==";
  } else if (start_of_id_is("ne", ptr, dctl)) {
    s = "!=";
  } else if (start_of_id_is("le", ptr, dctl)) {
    s = "<=";
  } else if (start_of_id_is("ge", ptr, dctl)) {
    s = ">=";
  } else if (start_of_id_is("aa", ptr, dctl)) {
    s = "&&";
  } else if (start_of_id_is("oo", ptr, dctl)) {
    s = "||";
  } else if (start_of_id_is("pp", ptr, dctl)) {
    s = "++";
    *is_postfix = TRUE;
  } else if (start_of_id_is("mm", ptr, dctl)) {
    s = "--";
    *is_postfix = TRUE;
  } else if (start_of_id_is("cm", ptr, dctl)) {
    s = ",";
  } else if (start_of_id_is("rm", ptr, dctl)) {
    s = "->*";
  } else if (start_of_id_is("rf", ptr, dctl)) {
    s = "->";
  } else if (start_of_id_is("cl", ptr, dctl)) {
    s = "()";
  } else if (start_of_id_is("cp", ptr, dctl)) {
    *need_adl_parens = TRUE;
    s = "()";
  } else if (start_of_id_is("vc", ptr, dctl)) {
    s = "[]";
  } else if (start_of_id_is("qs", ptr, dctl)) {
    s = "?";
  } else if (start_of_id_is("mn", ptr, dctl)) {
    s = "<?";
  } else if (start_of_id_is("mx", ptr, dctl)) {
    s = ">?";
  } else if (start_of_id_is("ds", ptr, dctl)) {
    s = ".*";
  } else if (start_of_id_is("dt", ptr, dctl)) {
    s = ".";
  } else if (start_of_id_is("ps", ptr, dctl)) {
    s = "+";
  } else if (start_of_id_is("ng", ptr, dctl)) {
    s = "-";
  } else if (start_of_id_is("de", ptr, dctl)) {
    s = "*";
  } else if (start_of_id_is("ao", ptr, dctl)) {
    s = "&";
  } else if (start_of_id_is("rl", ptr, dctl)) {
    s = "__real(";
  } else if (start_of_id_is("im", ptr, dctl)) {
    s = "__imag(";
  } else if (start_of_id_is("dc", ptr, dctl)) {
    s = "dynamic_cast<";
    *is_new_style_cast = TRUE;
    *takes_type = TRUE;
  } else if (start_of_id_is("sc", ptr, dctl)) {
    s = "static_cast<";
    *is_new_style_cast = TRUE;
    *takes_type = TRUE;
  } else if (start_of_id_is("cc", ptr, dctl)) {
    s = "const_cast<";
    *is_new_style_cast = TRUE;
    *takes_type = TRUE;
  } else if (start_of_id_is("rc", ptr, dctl)) {
    s = "reinterpret_cast<";
    *is_new_style_cast = TRUE;
    *takes_type = TRUE;
  } else if (start_of_id_is("sf", ptr, dctl)) {
    s = "safe_cast<";
    *is_new_style_cast = TRUE;
    *takes_type = TRUE;
  } else if (start_of_id_is("tw", ptr, dctl)) {
    s = "throw ";
  } else if (start_of_id_is("sz", ptr, dctl)) {
    s = "sizeof(";
    *takes_type = TRUE;
  } else if (start_of_id_is("cs", ptr, dctl)) {
    s = "cast";
    *takes_type = TRUE;
  } else if (start_of_id_is("af", ptr, dctl)) {
    s = "__alignof__(";
    *takes_type = TRUE;
  } else if (start_of_id_is("uu", ptr, dctl)) {
    s = "__uuidof(";
    *takes_type = TRUE;
  } else if (start_of_id_is("ty", ptr, dctl)) {
    s = "typeid(";
    *takes_type = TRUE;
  } else if (start_of_id_is("ct", ptr, dctl)) {
    s = "::typeid";
    *takes_type = TRUE;
  } else if (start_of_id_is("bi", ptr, dctl)) {
    s = "builtin-operation";
  } else if (start_of_id_is("sp", ptr, dctl)) {
    s = "...";
    *is_postfix = TRUE;
  } else if (start_of_id_is("sk", ptr, dctl)) {
    s = "sizeof...(";
    *takes_type = TRUE;
  } else if (start_of_id_is("ht", ptr, dctl)) {
    s = "%";
  } else if (start_of_id_is("sb", ptr, dctl)) {
    s = "subscript";
  } else if (start_of_id_is("il", ptr, dctl)) {
    s = "{";
    *is_initializer_list = TRUE;
  } else if (start_of_id_is("tl", ptr, dctl)) {
    s = "{";
    *takes_type = TRUE;
    *is_initializer_list = TRUE;
  } else if (start_of_id_is("nx", ptr, dctl)) {
    s = "noexcept(";
  } else if (start_of_id_is("li", ptr, dctl)) {
    /* Note that the ud-suffix follows the "li" and needs to be
       demangled by the caller. */
    s = "\"\"";
    *ud_suffix_follows = TRUE;
  } else if (start_of_id_is("aw", ptr, dctl)) {
    s = "co_await";
  } else if (start_of_id_is("fr", ptr, dctl) ||
             start_of_id_is("fl", ptr, dctl) ||
             start_of_id_is("fR", ptr, dctl) ||
             start_of_id_is("fL", ptr, dctl)) {
    /* A fold expression.  The actual length is not easily known and must be
       determined by the caller.  The returned value is simply a flag (and
       not an actual operation string). */
    s = "fold-ex";
  } else {
    s = NULL;
  }  /* if */
  *mangled_length = len;
  return s;
}  /* demangle_operator */


static a_boolean is_operator_function_name(
                                 a_const_char               *ptr,
                                 a_const_char               **demangled_name,
                                 int                        *mangled_length,
                                 a_boolean                  *ud_suffix_follows,
                                 a_decode_control_block_ptr dctl)
/*
Examine the string beginning at ptr to see if it is the mangled name for
an operator function.  If so, return TRUE and set *demangled_name to
the demangled form, and *mangled_length to the length of the mangled form.
*ud_suffix_follows is set to TRUE if the encoding for the operator function
name is followed by a user-defined literal suffix (which must be demangled
by the caller).
*/
{
  a_const_char *s, *end_ptr;
  int         len;
  a_boolean   takes_type, is_new_style_cast, is_postfix, need_adl_parens;
  a_boolean   is_initializer_list;

  /* Get the operator name. */
  s = demangle_operator(ptr, &len, &takes_type, &is_new_style_cast, 
                        &is_postfix, &need_adl_parens, &is_initializer_list,
                        ud_suffix_follows, dctl);
  if (s != NULL) {
    /* Make sure we took the whole name and nothing more. */
    end_ptr = ptr + len;
    if (*ud_suffix_follows && end_ptr != NULL && !dctl->err_in_id &&
        isdigit((unsigned char)get_char(end_ptr, dctl))) {
      /* If a ud-suffix follows, make sure its length is accounted for.
         Note that prev_end is restored here as this is just speculative
         look ahead. */
      unsigned long  num;
      a_const_char   *prev_end;
      end_ptr = get_length(end_ptr, &num, &prev_end, dctl);
      end_ptr = end_ptr + num;
      /* Restore any fields that might have been changed by the speculative
         lookahead. */
      dctl->end_of_name = prev_end;
      dctl->err_in_id = FALSE;
    }  /* if */
    if (get_char(end_ptr, dctl) == '\0' ||
        (get_char(end_ptr, dctl) == '_' && get_char(end_ptr+1, dctl) == '_')) {
      /* Okay. */
    } else {
      s = NULL;
      *ud_suffix_follows = FALSE;
    }  /* if */
  }  /* if */
  *demangled_name = s;
  *mangled_length = len;
  return (s != NULL);
}  /* is_operator_function_name */


static void note_specialization(a_const_char               *ptr,
                                a_template_param_block_ptr temp_par_info)
/*
Note the fact that a specialization indication has been encountered at ptr
while scanning a mangled name.  temp_par_info, if non-NULL, points to
a block of information related to template parameter processing.
*/
{
  if (temp_par_info != NULL) {
    if (temp_par_info->set_final_specialization) {
      /* Remember the location of the last specialization seen. */
      temp_par_info->final_specialization = ptr;
    } else if (temp_par_info->actual_template_args_until_final_specialization&&
               ptr == temp_par_info->final_specialization) {
      /* Stop doing the special processing for specializations when the
         final specialization is reached. */
      temp_par_info->actual_template_args_until_final_specialization = FALSE;
    }  /* if */
  }  /* if */
}  /* note_specialization */


static a_const_char *demangle_function_local_indication(
                                          a_const_char               *ptr,
                                          unsigned long              nchars,
                                          unsigned long              *instance,
                                          a_decode_control_block_ptr dctl)
/*
Demangle the function name and id number in a function-local indication:

    __L2__f__Fv
               ^-- returned pointer points here
          ^------- mangled function name
       ^---------- instance number within function (ptr points here on entry)

ptr points to the character after the "__L".  If nchars is non-zero, it
indicates the length of the string, starting from ptr.  Return a pointer
to the character following the mangled function name.  Output a function
indication like "f(void)::".  The instance number is simply a way of
differentiating between similarly named entities in the same function and
may be a discriminator (for class/scoped enums), scope number, or block number
depending what is being mangled and is returned to the caller in *instance.
This allows the caller to emit it later (after the name of the entity) or
suppress it (in cases where it is duplicated).
*/
{
  a_const_char  *p = ptr, *prev_end = NULL;

  if (nchars != 0) {
    prev_end = dctl->end_of_name;
    dctl->end_of_name = ptr + nchars;
  }  /* if */
  /* Get the instance number. */
  p = get_number(ptr, instance, dctl);
  /* Check for the two underscores following the instance number.  For local
     class names in some older versions of the mangling scheme, there is no
     following function name. */
  if (get_char(p, dctl) == '_' && get_char(p+1, dctl) == '_') {
    p += 2;
    /* Put out the function name. */
    if (nchars != 0) nchars -= (unsigned long)(p - ptr);
    p = full_demangle_identifier(p, nchars,
                                 /*suppress_parent_and_local_info=*/FALSE,
                                 dctl);
    write_id_str("::", dctl);
  }  /* if */
  if (prev_end != NULL) dctl->end_of_name = prev_end;
  return p;
}  /* demangle_function_local_indication */


static void emit_instance(unsigned long              instance,
                          a_decode_control_block_ptr dctl)
/*
The instance number is part of a local function mangling (used to
differentiate between entities with the same name within the same function).
This could represent a discriminator, scope number or block number depending
on what has been mangled.  Emit it as an instance number.
*/
{
  if (!dctl->err_in_id) {
    write_id_str(" (instance ", dctl);
    write_id_number(instance, dctl);
    write_id_str(")", dctl);
  }  /* if */
}  /* emit_instance */


static a_const_char *demangle_name(
                                a_const_char               *ptr,
                                unsigned long              nchars,
                                a_boolean                  stop_on_underscores,
                                unsigned long              *nchars_left,
                                a_const_char               *mclass,
                                a_template_param_block_ptr temp_par_info,
                                a_boolean                  *instance_emitted,
                                a_decode_control_block_ptr dctl)
/*
Demangle the name at ptr and output the demangled form.  Return a pointer
to the character position following what was demangled.  A "name" is
usually just a string of alphanumeric characters.  However, names of
constructors, destructors, and operator functions require special
handling, as do template entity names.  A name at this level
does not include any associated parent or function-local information,
nor function-parameter information.  nchars indicates the number
of characters in the name, or is zero if the name is open-ended
(it's ended by a null or double underscore).  A double underscore
ends the name if stop_on_underscores is TRUE (though some sequences
beginning with two underscores and related to templates, e.g., "__pt",
are recognized and processed locally regardless of the setting of
stop_on_underscores).  If nchars_left is non-NULL, no error is
issued if too few characters are taken to satisfy nchars;
the count of remaining characters is placed in *nchars_left.
mclass, when non-NULL, points to the mangled form of the class of
which this name is a member.  When it's non-NULL, constructor and
destructor names will be put out in the proper form (otherwise,
they are left in their original forms).  If instance_emitted is non-NULL,
it is set to TRUE if the name has an instance number (as is the case
with unnamed types and lambdas); this allows the caller to suppress 
duplicate instance numbers when the type appears in a local environment.
instance_emitted is set to FALSE otherwise.  When temp_par_info != NULL,
it points to a block that controls output of extra information on
template parameters.
*/
{
  a_const_char  *p, *end_ptr = NULL, *prev_end = NULL;
  a_boolean     is_special_name = FALSE, is_pt, is_partial_spec = FALSE;
  a_boolean     partial_spec_output_suppressed = FALSE, ud_suffix_follows;
  a_const_char  *demangled_name;
  int           mangled_length;
  unsigned long discriminator;

  if (instance_emitted != NULL) *instance_emitted = FALSE;
  if (nchars != 0) {
    prev_end = dctl->end_of_name;
    dctl->end_of_name = ptr + nchars;
  }  /* if */
  if (nchars_left != NULL) *nchars_left = 0;
  /* See if the name is special in some way. */
  if (get_char(ptr, dctl) == '_' && get_char(ptr+1, dctl) == '_') {
    /* Name beginning with two underscores. */
    p = ptr + 2;
    if (start_of_id_is("ct__", p, dctl) ||
        start_of_id_is("st__", p, dctl)) {
      /* Constructor or C++/CLI static constructor. */
      end_ptr = p + 2;
      if (mclass == NULL) {
        /* The mangled name for the class is not provided, so handle this as
           a normal name. */
      } else {
        /* Output the class name for the constructor name. */
        is_special_name = TRUE;
        (void)full_demangle_type_name(mclass, /*base_name_only=*/TRUE,
                                      /*temp_par_info=*/
                                              (a_template_param_block_ptr)NULL,
                                      /*is_destructor_name=*/FALSE,
                                      dctl);
        if (start_of_id_is("st__", p, dctl)) {
          /* Add an indication that this is a C++/CLI static constructor. */
          write_id_str("[static]", dctl);
        }  /* if */
      }  /* if */
    } else if (start_of_id_is("dt__", p, dctl) ||
               start_of_id_is("df__", p, dctl)) {
      /* Destructor or C++/CLI finalizer. */
      end_ptr = p + 2;
      if (mclass == NULL) {
        /* The mangled name for the class is not provided, so handle this as
           a normal name. */
      } else {
        /* Output ~class-name for the destructor name, or !class-name for
           a C++/CLI finalizer. */
        is_special_name = TRUE;
        if (start_of_id_is("df__", p, dctl)) {
          write_id_ch('!', dctl);
        } else {
          write_id_ch('~', dctl);
        }  /* if */
        (void)full_demangle_type_name(mclass, /*base_name_only=*/TRUE,
                                      /*temp_par_info=*/
                                              (a_template_param_block_ptr)NULL,
                                      /*is_destructor_name=*/FALSE,
                                      dctl);
      }  /* if */
    } else if (start_of_id_is("dn__", p, dctl)) {
      /* Destructor name. */
      /* This differs from the dt__ case above in two ways: its demangling
         doesn't always have a scope operator (i.e., ::), and it doesn't
         require that the destructor name be the same as the qualifying type
         (e.g., it can handle T::~X()).  What follows (a "destructor name")
         can be parsed as a nested type, but has an implied ~ before the
         final qualifier.  For example, Q4_1A1B1C1D would demangle as
         A::B::C::~D and 1A would demangle as ~A (as in a.~A()). */
      is_special_name = TRUE;
      if (get_char(p+4, dctl) == 'Q') {
        /* Destructor is qualified. */
        end_ptr = full_demangle_type_name(p+4, /*base_name_only=*/FALSE,
                                          /*temp_par_info=*/
                                              (a_template_param_block_ptr)NULL,
                                          /*is_destructor_name=*/TRUE,
                                          dctl);
      } else {
        /* An unqualified type. */
        write_id_ch('~', dctl);
        end_ptr = demangle_type(p+4, dctl);
      }  /* if */
    } else if (start_of_id_is("op", p, dctl)) {
      /* Conversion function.  Name looks like __opi__... where the part
         after "op" encodes the type (e.g., "opi" is "operator int"). */
      is_special_name = TRUE;
      write_id_str("operator ", dctl);
      end_ptr = demangle_type(p+2, dctl);
    } else if (is_operator_function_name(p, &demangled_name,
                                         &mangled_length, &ud_suffix_follows,
                                         dctl)) {
      /* Operator function. */
      is_special_name = TRUE;
      write_id_str("operator ", dctl);
      write_id_str(demangled_name, dctl);
      end_ptr = p + mangled_length;
      if (ud_suffix_follows) {
        /* A literal operator (i.e., operator ""); the ud-suffix follows. */
        end_ptr = demangle_name_with_preceding_length(end_ptr, dctl);
      }  /* if */
    } else if (nchars != 0 && start_of_id_is("N", p, dctl)) {
      /* __Nxxxx: unnamed namespace name.  Put out "<unnamed>" and ignore
         the characters after "__N".  For nested unnamed namespaces there
         is no number after the "__N". */
      is_special_name = TRUE;
      write_id_str("<unnamed>", dctl);
      end_ptr = p + nchars - 2;
    } else if (nchars != 0 && start_of_id_is("INTERNAL", p, dctl)) {
      /* __INTERNAL<module_id>: An individuated namespace name. */
      is_special_name = TRUE;
      write_id_str("[local to ", dctl);
      end_ptr = demangle_module_id(p+8, nchars-(8+2), p-2, dctl);
      write_id_str("]", dctl);
    } else if (start_of_id_is("Ut", p, dctl)) {
      /* __Utnn: An unnamed type. */
      write_id_str("[unnamed type", dctl);
      p = get_number(p+2, &discriminator, dctl);
      if (discriminator > 0) {
        write_id_str(" (instance ", dctl);
        write_id_number(discriminator, dctl);
        write_id_str(")", dctl);
        is_special_name = TRUE;
        end_ptr = p;
        if (instance_emitted != NULL) *instance_emitted = TRUE;
      } else {
        bad_mangled_name(dctl);
      }  /* if */
      write_id_str("]", dctl);
    } else if (start_of_id_is("Ul", p, dctl) ||
               start_of_id_is("Um", p, dctl)) {
      /* __Ulnn_<function-type> or __Umnn_<function-type>: Lambda closure.
         For demangling purposes, treat these the same; the member initializer
         case will be preceded by the name of the member being initialized,
         so no further words are necessary. */
      p = get_number(p+2, &discriminator, dctl);
      if (get_char(p, dctl) == '_') {
        write_id_str("[lambda", dctl);
        p = demangle_type(p+1, dctl);
        if (discriminator > 0) {
          write_id_str(" (instance ", dctl);
          write_id_number(discriminator, dctl);
          write_id_str(")", dctl);
          is_special_name = TRUE;
          end_ptr = p;
          if (instance_emitted != NULL) *instance_emitted = TRUE;
        } else {
          bad_mangled_name(dctl);
        }  /* if */
        write_id_str("]", dctl);
      } else {
        bad_mangled_name(dctl);
      }  /* if */
    } else if (start_of_id_is("Ud", p, dctl)) {
      /* __Udnn_p_<function-type>: Lambda closure in default argument.
         Note that this will always appear in a local function context, but
         that is handled at a higher level. */
      unsigned long param_num;
      p = get_number(p+2, &discriminator, dctl);
      if (get_char(p, dctl) == '_') {
        p = get_number(p+1, &param_num, dctl);
        if (get_char(p, dctl) == '_') {
          write_id_str("[lambda", dctl);
          p = demangle_type(p+1, dctl);
          write_id_str(" in default argument ", dctl);
          write_id_number(param_num, dctl);
          write_id_str(" (from end)", dctl);
          if (discriminator > 0) {
            write_id_str(" (instance ", dctl);
            write_id_number(discriminator, dctl);
            write_id_str(")", dctl);
            is_special_name = TRUE;
            end_ptr = p;
            if (instance_emitted != NULL) *instance_emitted = TRUE;
          } else {
            bad_mangled_name(dctl);
          }  /* if */
          write_id_str("]", dctl);
        } else {
          bad_mangled_name(dctl);
        }  /* if */
      }  /* if */
    } else if (start_of_id_is("ab", p, dctl)) {
      /* __abnn<tag>: Mangling for __attribute(abi_tag((tag)).  Multiple
         "abi_tag" attributes can be specified. */
      unsigned long count;
      p = p+2;
      write_id_str("[abi:", dctl);
      for (;;) {
        p = get_number(p, &count, dctl);
        if (count == 0 || p+count > dctl->end_of_name) {
          bad_mangled_name(dctl);
          break;
        }  /* if */
        while (count--) {
          write_id_ch(*p++, dctl);
        }  /* while */
        if (start_of_id_is("__ab", p, dctl)) {
          p = p+4;
          write_id_ch(',', dctl);
        } else {
          break;
        }  /* if */
      }  /* for */
      write_id_ch(']', dctl);
      if (!dctl->err_in_id) {
        /* This mangling is basically a prefix; what remains is still a name
           (possibly with special names that are checked for above); recurse
           to handle that case. */
        if (nchars != 0) {
          if (nchars > (p - ptr)) {
            nchars -= (unsigned long)(p - ptr);
          } else {
            bad_mangled_name(dctl);
          }  /* if */
        }  /* if */
        end_ptr = demangle_name(p, nchars, stop_on_underscores, nchars_left,
                                mclass, temp_par_info, instance_emitted, dctl);
      } else {
        end_ptr = p;
      }  /* if */
      goto end_of_routine;
    } else if (start_of_id_is("SBC__", p, dctl)) {
      /* Mangled name for a structured binding container. */
      write_id_str("structured binding for [", dctl);
      for (p = p+5; *p != '\0';) {
        if (get_char(p, dctl) == '_') {
          if (get_char(p+1, dctl) == '_') {
            if (get_char(p+2, dctl) == '_') {
              if (get_char(p+3, dctl) == '_') {
                /* Quadruple underscore -- end of structured binding. */
                p += 4;
              } else {
                p += 3;
                bad_mangled_name(dctl);
              }  /* if */
              break;
            } else {
              /* Double underscore -- structured binding delimiter. */
              write_id_ch(',', dctl);
              p += 2;
            }  /* if */
          } else {
            /* Underscore followed by non-underscore -- just copy to output. */
            write_id_ch('_', dctl);
            write_id_ch(p[1], dctl);
            p += 2;
          }  /* if */
        } else {
          /* Not an underscore -- just copy to output. */
          write_id_ch(*p, dctl);
          p += 1;
        } /* if */
      }  /* for */
      write_id_ch(']', dctl);
      end_ptr = p;
      goto end_of_routine;
    } else {
      /* Something unrecognized. */
    }  /* if */
  }  /* if */
  /* Here, end_ptr non-null means the end of the string has been found
     already (because the name is special in some way). */
  if (end_ptr == NULL) {
    /* Not a special name. Find the end of the string and set end_ptr.
       Also look for template-related things that terminate the name
       earlier. */
    for (p = ptr; ; p++) {
      char ch = get_char(p, dctl);
      /* Stop at the end of the string. */
      if (ch == '\0') break;
      /* Stop on a double underscore, but not one at the start of the string.
         More than 2 underscores in a row does not terminate the string,
         so that something like the name for "void f_()" (i.e., "f___Fv")
         can be demangled successfully. */
      if (ch == '_' && p != ptr &&
          get_char(p+1, dctl) == '_' &&
          get_char(p+2, dctl) != '_' &&
          /* When stop_on_underscores is FALSE, stop only on "__tm__",
             "__ps__", "__pt__", or "__S".  Double underscores can appear
             in the middle of some names, e.g., member names used as
             template arguments. */
          (stop_on_underscores ||
           (get_char(p+2, dctl) == 't' &&
            get_char(p+3, dctl) == 'm' &&
            get_char(p+4, dctl) == '_' &&
            get_char(p+5, dctl) == '_') ||
           (get_char(p+2, dctl) == 'p' &&
            get_char(p+3, dctl) == 's' &&
            get_char(p+4, dctl) == '_' &&
            get_char(p+5, dctl) == '_') ||
           (get_char(p+2, dctl) == 'p' &&
            get_char(p+3, dctl) == 't' &&
            get_char(p+4, dctl) == '_' &&
            get_char(p+5, dctl) == '_') ||
           get_char(p+2, dctl) == 'S')) {
        break;
      }  /* if */
    }  /* for */
    end_ptr = p;
  }  /* if */
  /* Here, end_ptr indicates the character after the end of the initial
     part of the name. */
  if (!is_special_name) {
    /* Output the characters of the base name. */
    for (p = ptr; p < end_ptr; p++) write_id_ch(*p, dctl);
  }  /* if */
  /* If there's a template argument list for a partial specialization
     (beginning with "__ps__"), process it. */
  if (start_of_id_is("__ps__", end_ptr, dctl)) {
    /* Write the arguments.  This first argument list gives the arguments
       that appear in the partial specialization declaration:
         template <class T, class U> struct A { ... };
         template <class T> struct A<T *, int> { ... };
                                     ^^^^^^^^this argument list
       This first argument list will be followed by another argument list
       that gives the arguments according to the partial specialization.
       For A<int *, int> according to the example above, the second
       argument list is <int>.  The second argument list is scanned but
       not put out, except when argument correspondences are output. */
    end_ptr = demangle_template_arguments(end_ptr+6, /*emit_arg_values=*/TRUE,
                                          /*suppress_angle_brackets=*/FALSE,
                                          temp_par_info, dctl);
    note_specialization(end_ptr, temp_par_info);
    is_partial_spec = TRUE;
  }  /* if */
  /* If there's a specialization indication ("__S"), ignore it. */
  if (get_char(end_ptr,   dctl) == '_' &&
      get_char(end_ptr+1, dctl) == '_' &&
      get_char(end_ptr+2, dctl) == 'S' &&
      (!stop_on_underscores ||
       get_char(end_ptr+3, dctl) == '\0' ||
       (get_char(end_ptr+3, dctl) == '_' &&
        get_char(end_ptr+4, dctl) == '_'))) {
    note_specialization(end_ptr, temp_par_info);
    end_ptr += 3;
  }  /* if */
  /* If there's a template argument list (beginning with "__pt__" or "__tm__"),
     process it. */
  if ((is_pt = start_of_id_is("__pt__", end_ptr, dctl)) ||
      start_of_id_is("__tm__", end_ptr, dctl)) {
    /* The "__pt__ form indicates an old-style mangled template name. */
    if (is_pt && temp_par_info != NULL ) {
      temp_par_info->use_old_form_for_template_output = TRUE;
    }  /* if */
    /* For the second argument list of a partial specialization,
       process the argument list but suppress output. */
    if (is_partial_spec && temp_par_info != NULL &&
        !temp_par_info->output_only_correspondences) {
      dctl->suppress_id_output++;
      partial_spec_output_suppressed = TRUE;
    }  /* if */
    /* Write the arguments. */
    end_ptr = demangle_template_arguments(end_ptr+6, /*emit_arg_values=*/FALSE,
                                          /*suppress_angle_brackets=*/FALSE,
                                          temp_par_info, dctl);
    if (partial_spec_output_suppressed) dctl->suppress_id_output--;
    /* If there's a(nother) specialization indication ("__S"), ignore it. */
    if (get_char(end_ptr,   dctl) == '_' &&
        get_char(end_ptr+1, dctl) == '_' &&
        get_char(end_ptr+2, dctl) == 'S' &&
        (!stop_on_underscores ||
         get_char(end_ptr+3, dctl) == '\0' ||
         (get_char(end_ptr+3, dctl) == '_' &&
          get_char(end_ptr+4, dctl) == '_'))) {
      note_specialization(end_ptr, temp_par_info);
      end_ptr += 3;
    }  /* if */
  }  /* if */
  /* Check that we took exactly the characters we should have. */
  if (nchars_left != NULL) {
    /* Return the count of characters not taken.  We're not required to
       end at the right place. */
    *nchars_left = nchars - (unsigned long)(end_ptr-ptr);
  } else if (((nchars != 0) ? (end_ptr-ptr == nchars) : (*end_ptr == '\0')) ||
             (stop_on_underscores &&
              get_char(end_ptr,   dctl) == '_' &&
              get_char(end_ptr+1, dctl) == '_')) {
    /* Okay. */
  } else {
    bad_mangled_name(dctl);
  }  /* if */
end_of_routine:
  if (prev_end != NULL) dctl->end_of_name = prev_end;
  return end_ptr;
}  /* demangle_name */


static a_const_char *demangle_type_name_with_preceding_length(
                                   a_const_char               *ptr,
                                   a_boolean                  base_name_only,
                                   unsigned long              nchars,
                                   unsigned long              *nchars_left,
                                   a_template_param_block_ptr temp_par_info,
                                   a_decode_control_block_ptr dctl)
/*
Demangle a type name (or namespace name) that is preceded by a length, e.g.,
"3abc" for the type name "abc".  The name can include template parameters or a
function-local indication but is not a nested type.  If nchars is non-zero on
input, the length has already been scanned and nchars gives its value.  In that
case, not all nchars characters of input need be taken, scanning will
stop on a "__", and *nchars_left is set to the number of characters not
taken.  Return a pointer to the character position following what was
demangled.  When temp_par_info != NULL, it points to a block that controls
output of extra information on template parameters.  When base_name_only
is TRUE, suppress any function-local information.
*/
{
  a_const_char  *p = ptr, *orig_end, *prev_end;
  a_const_char  *p2;
  unsigned long nchars2, instance;
  a_boolean     has_function_local_info = FALSE;
  a_boolean     instance_emitted;
  a_boolean     stop_on_underscores;

  if (nchars == 0) {
    /* Get the length. */
    p = get_length(p, &nchars, &prev_end, dctl);
    nchars_left = NULL;
    stop_on_underscores = FALSE;
  } else {
    /* Length was gotten by the caller. */
    if (nchars_left != NULL) *nchars_left = 0;
    prev_end = dctl->end_of_name;
    dctl->end_of_name = orig_end = ptr+nchars;
    stop_on_underscores = TRUE;
  }  /* if */
  if (nchars >= 8) {
    /* Look for a function-local indication, e.g., "__Ln__f" for block
       "n" of function "f". */
    for (p2 = p+1; p2+6 < p+nchars; p2++) {
      if (get_char(p2,   dctl) == '_' &&
          get_char(p2+1, dctl) == '_') {
        if (get_char(p2+2, dctl) == 't' &&
            get_char(p2+3, dctl) == 'm' &&
            get_char(p2+4, dctl) == '_' &&
            get_char(p2+5, dctl) == '_') {
          /* Beware of a local type in a template argument list; don't
             decode the local information yet, it will be decoded when the
             local type is processed. */
          unsigned long skip;
          a_const_char  *dummy;
          p2 = get_length(p2+6, &skip, &dummy, dctl);
          p2 += skip;
        } else if (get_char(p2+2, dctl) == 'L') {
          has_function_local_info = TRUE;
          nchars2 = nchars;
          /* Set the length for the scan below to stop just before "__L". */
          nchars = (unsigned long)(p2 - p);
          p2 += 3;  /* Points to block number after "__L". */
          nchars2 -= (unsigned long)(p2 - p);
          /* Output the block number and function name. */
          if (base_name_only) dctl->suppress_id_output++;
          p2 = demangle_function_local_indication(p2, nchars2, &instance,
                                                  dctl);
          if (base_name_only) dctl->suppress_id_output--;
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  /* Demangle the name. */
  p = demangle_name(p, nchars, stop_on_underscores,
                    nchars_left, (char *)NULL, temp_par_info, 
                    &instance_emitted, dctl);
  if (has_function_local_info) {
    /* Don't write the instance number in cases where an unnamed type or
       lambda has already emitted it. */
    if (!instance_emitted && !base_name_only) emit_instance(instance, dctl);
    p = p2;
    if (nchars_left != NULL) *nchars_left = (unsigned long)(orig_end - p2);
  }  /* if */
  dctl->end_of_name = prev_end;
  return p;
}  /* demangle_type_name_with_preceding_length */


static a_const_char *demangle_name_with_preceding_length(
                                               a_const_char               *ptr,
                                               a_decode_control_block_ptr dctl)
/*
Demangle a name with a preceding length (e.g., "3abc") and return a pointer
to the character position following what was demangled.
*/
{
  ptr = demangle_type_name_with_preceding_length(
                                              ptr,
                                              /*base_name_only=*/TRUE,
                                              (unsigned long)0,
                                              (unsigned long *)NULL,
                                              (a_template_param_block_ptr)NULL,
                                              dctl);
  return ptr;
}  /* demangle_name_with_preceding_length */


static a_const_char *demangle_simple_type_name(
                                   a_const_char               *ptr,
                                   a_boolean                  base_name_only,
                                   a_template_param_block_ptr temp_par_info,
                                   a_decode_control_block_ptr dctl)
/*
Demangle a type name (or namespace name) that can appear as part of a
nested name.  Return a pointer to the character position following what
was demangled.  The name is not a nested name, but it can have template
arguments.  When temp_par_info != NULL, it points to a block that
controls output of extra information on template parameters.
When base_name_only is TRUE, suppress any function-local information.
*/
{
  a_const_char *p = ptr;

  if (get_char(p, dctl) == 'Z') {
    /* A template parameter name. */
    p = demangle_template_parameter_name(p, /*nontype=*/FALSE, dctl);
  } else if (get_char(p, dctl) == 'G') {
    /* A global scope indicator (e.g., ::A).  This only occurs in the
       context of a qualified name, so the caller will emit the requisite
       "::" string (this is basically treated as a null qualifier). */
    p++;
  } else if (isdigit((unsigned char)get_char(p, dctl))) {
    /* A simple mangled type name consists of digits indicating the length of
       the name followed by the name itself, e.g., "3abc". */
    p = demangle_type_name_with_preceding_length(p, base_name_only,
                                                 (unsigned long)0,
                                                 (unsigned long *)NULL,
                                                 temp_par_info, dctl);
  } else {
    /* Presumably a decltype or typeof. */
    p = demangle_type(p, dctl);
  }  /* if */
  return p;
}  /* demangle_simple_type_name */


static a_const_char *full_demangle_type_name(
                                 a_const_char               *ptr,
                                 a_boolean                  base_name_only,
                                 a_template_param_block_ptr temp_par_info,
                                 a_boolean                  is_destructor_name,
                                 a_decode_control_block_ptr dctl)
/*
Demangle the type name at ptr and output the demangled form.  Return a pointer
to the character position following what was demangled.  The name can be
a simple type name or a nested type name, or the name of a namespace.
If base_name_only is TRUE, do not put out any nested type qualifiers,
e.g., put out "A::x" as simply "x".  When temp_par_info != NULL, it
points to a block that controls output of extra information on template
parameters.  Note that this routine is called for namespaces too
(the mangling is the same as for class names; you can't actually tell
the difference in a mangled name).  If is_destructor_name is TRUE, this type is
actually the name of a destructor and an implied "~" should be emitted before
the last component of a qualified name (e.g., T::~X).  See demangle_type_name
for an interface to this routine for the simple case.
*/
{
  a_const_char  *p = ptr;
  unsigned long nquals;

  if (get_char(p, dctl) == 'Q') {
    /* A nested type name has the form
         Q2_5outer5inner   (outer::inner)
            ^-----^--------Names from outermost to innermost
          ^----------------Number of levels of qualification.
       Note that the levels in the qualifier can be class names or namespace
       names. */
    p = get_number(p+1, &nquals, dctl);
    p = advance_past_underscore(p, dctl);
    /* Handle each level of qualification. */
    for (; nquals > 0; nquals--) {
      if (dctl->err_in_id) break;  /* Avoid infinite loops on errors. */
      /* Do not put out the nested type qualifiers if base_name_only is
         TRUE. */
      if (base_name_only && nquals != 1) dctl->suppress_id_output++;
      if (is_destructor_name && nquals == 1) write_id_ch('~', dctl);
      p = demangle_simple_type_name(p, base_name_only, temp_par_info, dctl);
      if (nquals != 1) write_id_str("::", dctl);
      if (base_name_only && nquals != 1) dctl->suppress_id_output--;
    }  /* for */
  } else {
    /* A simple (non-nested) type name. */
    if (is_destructor_name) write_id_ch('~', dctl);
    p = demangle_simple_type_name(p, base_name_only, temp_par_info, dctl);
  }  /* if */
  return p;
}  /* full_demangle_type_name */


static a_const_char *demangle_vtbl_class_name(a_const_char               *ptr,
                                              a_decode_control_block_ptr dctl)
/*
Demangle a class or base class name that is one component of a virtual
function table name.  Such names are mangled mostly as types, but with
a few special quirks.
*/
{
  a_const_char  *p = ptr, *prev_end;
  unsigned long nchars, nchars_left;

  /* This code handles both the base class part of the name and
     the class part.  A base class name has the form
       <length> followed by one or more <class spec> optionally followed
         by an ambiguity specification, __A optionally followed by a number.
         The ambiguity specification is not included in the length.
     A <class spec> is a class name mangling without preceding length, or
       a "Q" nested-type-name specification.
     A class name has the form
       <length> <class spec>
     or 
       Q nested-type-name specification (i.e., without preceding length).
  */
  if (get_char(p, dctl) == 'Q') {
    /* Nested-type-name "Q" without preceding length.  This is used only
       for the complete object class (the last section), not for the
       base classes. */
    p = demangle_type_name(p, dctl);
  } else {
    /* Get the length. */
    p = get_length(p, &nchars, &prev_end, dctl);
    while (!dctl->err_in_id) {
      a_boolean nested_name_case = FALSE;
      /* Check a "Q" nested-type-name specification by checking for "Q",
         some digits, and an underscore.  This rules out class names that
         start with "Q".  A class whose name starts with something like
         "Q2_" is still going to be a problem, but that's a truly
         ambiguous case.  This is inherited from Cfront. */
      if (get_char(p, dctl) == 'Q') {
        a_const_char *p2 = p+1;
        if (isdigit((unsigned char)get_char(p2, dctl))) {
          do { p2++; } while (isdigit((unsigned char)get_char(p2, dctl)));
          if (get_char(p2, dctl) == '_') {
            nested_name_case = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
      if (nested_name_case) {
        /* Nested class name. */
        a_const_char  *end_ptr = demangle_type_name(p, dctl);
        unsigned long chars_taken = (unsigned long)(end_ptr - p);
        nchars -= chars_taken;
        p = end_ptr;
      } else {
        /* Non-nested class name without preceding length. */
        p = demangle_type_name_with_preceding_length(
                                              p, /*base_name_only=*/FALSE,
                                              nchars, &nchars_left,
                                              (a_template_param_block_ptr)NULL,
                                              dctl);
        nchars = nchars_left;
      }  /* if */
      /* Leave the loop if there is not another base class in the
         derivation. */
      if (nchars < 3 || !start_of_id_is("__", p, dctl)) break;
      p += 2;
      nchars -= 2;
      write_id_str(" in ", dctl);
    }  /* while */
    /* Make sure we took all the characters indicated by the length. */
    if (nchars != 0) {
      bad_mangled_name(dctl);
    }  /* if */
    dctl->end_of_name = prev_end;
    if (start_of_id_is("__A", p, dctl)) {
      /* "__A" indicates an ambiguous base class.  This is used only on
         the base class specifications. */
      write_id_str(" (ambiguous)", dctl);
      p += 3;
      /* Ignore the number following __A, if any. */
      while (isdigit((unsigned char)get_char(p, dctl))) p++;
    }  /* if */
  }  /* if */
  return p;
}  /* demangle_vtbl_class_name */


static a_const_char *demangle_type_qualifiers(
                                     a_const_char               *ptr,
                                     a_boolean                  trailing_space,
                                     a_decode_control_block_ptr dctl)
/*
Demangle any type qualifiers (const/volatile/restrict) at the indicated
location.  Return a pointer to the character position following what was
demangled.  If trailing_space is TRUE, add a space at the end if any qualifiers
were put out.
*/
{
  a_const_char *p = ptr;
  a_boolean    any_quals = FALSE;

  for (;; p++) {
    if (get_char(p, dctl) == 'C') {
      if (any_quals) write_id_ch(' ', dctl);
      write_id_str("const", dctl);
    } else if (get_char(p, dctl) == 'V') {
      if (any_quals) write_id_ch(' ', dctl);
      write_id_str("volatile", dctl);
    } else if (get_char(p, dctl) == 'D' && get_char(p+1, dctl) == 'r') {
      if (any_quals) write_id_ch(' ', dctl);
      write_id_str("restrict", dctl);
      p++;
    } else {
      break;
    }  /* if */
    any_quals = TRUE;
  }  /* for */
  if (any_quals && trailing_space) write_id_ch(' ', dctl);
  return p;
}  /* demangle_type_qualifiers */


static a_const_char *demangle_ref_qualifiers(
                                         a_const_char               *p,
                                         a_const_char               **ref_qual,
                                         a_decode_control_block_ptr dctl)
/*
The character preceding *p is an "F", indicating a function type; see if
there is a ref-qualifier, and if so, set *ref_qual to a string suitable for
output (set to NULL otherwise).  Returns a pointer to the character position
following the optional ref-qualifier.
*/
{
  *ref_qual = NULL;
  if (get_char(p, dctl) == '_' && (get_char(p+1, dctl) == 'R')) {
    p += 2;
    *ref_qual = "&";
  } else if (get_char(p, dctl) == '_' && (get_char(p+1, dctl) == 'E')) {
    p += 2;
    *ref_qual = "&&";
  }  /* if */
  return p;
}  /* demangle_ref_qualifiers */


static a_const_char *demangle_type_specifier(a_const_char               *ptr,
                                             a_decode_control_block_ptr dctl)
/*
Demangle the type at ptr and output the specifier part.  Return a pointer
to the character position following what was demangled.
*/
{
  a_const_char *p = ptr, *s;
  char         ch;

  /* Process type qualifiers. */
  p = demangle_type_qualifiers(p, /*trailing_space=*/TRUE, dctl);
  ch = get_char(p, dctl);
  if (isdigit((unsigned char)ch) || ch == 'Q' || ch == 'Z') {
    /* Named type, like class or enum, e.g., "3abc". */
    p = demangle_type_name(p, dctl);
  } else {
    /* Builtin type. */
    if (ch == 'a') {
      /* GNU vector_size attribute. */
      write_id_str("__attribute__((vector_size(", dctl);
      p++;
      /* Scan the size. */
      while (ch = get_char(p, dctl), isdigit((unsigned char)ch)) {
        write_id_ch(ch, dctl);
        p++;
      }  /* while */
      write_id_str("))) ", dctl);
      /* The underlying type follows an underscore. */
      p = advance_past_underscore(p, dctl);
      ch = get_char(p, dctl);
    }  /* if */
    /* Handle signed and unsigned, and _Complex. */
    if (ch == 'S') {
      write_id_str("signed ", dctl);
      p++;
    } else if (ch == 'U') {
      write_id_str("unsigned ", dctl);
      p++;
    } else if (ch == 'x') {
      write_id_str("_Complex ", dctl);
      p++;
    }  /* if */
    switch (get_char(p++, dctl)) {
      case 'v':
        s = "void";
        break;
      case 'c':
        s = "char";
        break;
      case 'w':
        s = "wchar_t";
        break;
      case 'b':
        s = "bool";
        break;
      case 's':
        s = "short";
        break;
      case 'i':
        s = "int";
        break;
      case 'l':
        s = "long";
        break;
      case 'L':
        s = "long long";
        break;
      case 'f':
        s = "float";
        break;
      case 'd':
        s = "double";
        break;
      case 'r':
        s = "long double";
        break;
      case 'm':
        /* Microsoft intrinsic __intN types (Visual C++ 6.0 and later), as
           well as GNU 128-bit integers (m16) and GNU __float80/__float128. */
        switch (get_char(p++, dctl)) {
          case '1':
            if (get_char(p, dctl) == '6') {
              s = "__int128";
              p++;
            } else {
              s = "__int8";
            }  /* if */
            break;
          case '2':
            s = "__int16";
            break;
          case '4':
            s = "__int32";
            break;
          case '8':
            s = "__int64";
            break;
          case 'f':
            if (get_char(p++, dctl) == '1') {
              switch (get_char(p++, dctl)) {
                case '0':
                  s = "__float80";
                  break;
                case '6':
                  s = "__float128";
                  break;
                default:
                  bad_mangled_name(dctl);
                  s = "";
              }  /* switch */
            } else {
              bad_mangled_name(dctl);
              s = "";
            }  /* if */
            break;
          default:
            bad_mangled_name(dctl);
            s = "";
        }  /* switch */
        break;
      case 'n':
        s = "std::nullptr_t";
        break;
      case 'j':
        s = "__nullptr";
        break;
      case 'u':
        s = "auto";
        break;
      case 'q':
        s = "decltype(auto)";
        break;
      case 'g':
        s = "char16_t";
        break;
      case 'k':
        s = "char32_t";
        break;
      case 't':
        /* typeof(type) */
        write_id_str("typeof(", dctl);
        p = demangle_type(p, dctl);
        s = ")";
        break;
      case 'p':
        /* typeof(expression) */
        write_id_str("typeof(", dctl);
        p = demangle_expression(p, /*need_parens=*/FALSE, dctl);
        s = ")";
        break;
      case 'y':
        /* decltype of an id-expression or class member access. */
        write_id_str("decltype(", dctl);
        p = demangle_expression(p, /*need_parens=*/FALSE, dctl);
        s = ")";
        break;
      case 'Y':
        /* decltype of an expression. */
        write_id_str("decltype((", dctl);
        p = demangle_expression(p, /*need_parens=*/FALSE, dctl);
        s = "))";
        break;
      case 'o':
        /* __underlying_type(type) */
        write_id_str("__underlying_type(", dctl);
        p = demangle_type(p, dctl);
        s = ")";
        break;
      default:
        bad_mangled_name(dctl);
        s = "";
    }  /* switch */
    write_id_str(s, dctl);
  }  /* if */
  return p;
}  /* demangle_type_specifier */


static a_const_char *demangle_function_parameters(
                                               a_const_char               *ptr,
                                               a_decode_control_block_ptr dctl)
/*
Demangle the parameter list beginning at ptr and output the demangled form.
Return a pointer to the character position following what was demangled.
*/
{
  a_const_char  *p = ptr;
  a_const_char  *param_pos[10];
  unsigned long curr_param_num, param_num, nreps;
  a_boolean     any_params = FALSE;

  write_id_ch('(', dctl);
  if (get_char(p, dctl) == 'v') {
    /* Void parameter list. */
    p++;
  } else {
    any_params = TRUE;
    /* Loop for each parameter. */
    curr_param_num = 1;
    for (;;) {
      char ch;
      if (dctl->err_in_id) break;  /* Avoid infinite loops on errors. */
      ch = get_char(p, dctl);
      if ((ch == 'T' && isdigit((unsigned char)get_char(p+1, dctl))) ||
          ch == 'N') {
        /* Tn means repeat the type of parameter "n".  Note that a type can
           begin with "Tr" (i.e., a C++/CLI tracking reference), so check for
           a digit following the "T" to differentiate the two cases. */
        /* Nmn means "m" repetitions of the type of parameter "n".  "m"
           is a one-digit number. */
        /* "n" is also treated as a single-digit number; the front end enforces
           that (in non-cfront object code compatibility mode).  cfront does
           not, which leads to some ambiguities when "n" is followed by
           a class name. */
        if (get_char(p++, dctl) == 'N') {
          /* Get the number of repetitions. */
          p = get_single_digit_number(p, &nreps, dctl);
        } else {
          nreps = 1;
        }  /* if */
        /* Get the parameter number. */
        p = get_single_digit_number(p, &param_num, dctl);
        if (param_num < 1 || param_num >= curr_param_num ||
            param_pos[param_num] == NULL) {
          /* Parameter number out of range. */
          bad_mangled_name(dctl);
          goto end_of_routine;
        }  /* if */
        /* Produce "nreps" copies of parameter "param_num". */
        for (; nreps > 0; nreps--) {
          if (dctl->err_in_id) break;  /* Avoid infinite loops on errors. */
          if (curr_param_num < 10) param_pos[curr_param_num] = NULL;
          (void)demangle_type(param_pos[param_num], dctl);
          if (nreps != 1) write_id_str(", ", dctl);
          curr_param_num++;
        }  /* if */
      } else {
        /* A normal parameter. */
        if (curr_param_num < 10) param_pos[curr_param_num] = p;
        p = demangle_type(p, dctl);
        curr_param_num++;
      }  /* if */
      /* Stop after the last parameter. */
      ch = get_char(p, dctl);
      if (ch == '\0' || ch == 'e' || ch == '_' || ch == 'F') break;
      write_id_str(", ", dctl);
    }  /* for */
  }  /* if */
  if (get_char(p, dctl) == 'e') {
    /* Ellipsis. */
    if (any_params) write_id_str(", ", dctl);
    write_id_str("...", dctl);
    p++;
  }  /* if */
  write_id_ch(')', dctl);
end_of_routine:
  return p;
}  /* demangle_function_parameters */


static a_const_char *skip_extern_C_indication(a_const_char               *ptr,
                                              a_decode_control_block_ptr dctl)
/*
ptr points to the character after the "F" of a function type.  Skip over
and ignore an indication of extern "C" following the "F", if one is present.
Return a pointer to the character following the extern "C" indication.
There's no syntax for representing the extern "C" in the function type, so
just ignore it.
*/
{
  if (get_char(ptr, dctl) == 'K') ptr++;
  return ptr;
}  /* skip_extern_C_indication */


static a_const_char *demangle_type_first_part(
                               a_const_char               *ptr,
                               a_boolean                  under_lhs_declarator,
                               a_boolean                  need_trailing_space,
                               a_decode_control_block_ptr dctl)
/*
Demangle the type at ptr and output the specifier part and the part of the
declarator that precedes the name.  Return a pointer to the character
position following what was demangled.  If under_lhs_declarator is TRUE,
this type is directly under a type that uses a left-side declarator,
e.g., a pointer type.  (That's used to control use of parentheses around
parts of the declarator.)  If need_trailing_space is TRUE, put a space
at the end of the type first part (needed if the declarator part is
not empty, because it contains a name or a derived type).
*/
{
  a_const_char *p = ptr, *qualp = p;
  char    kind, ext_kind;

  /* Remove type qualifiers. */
  p = remove_immediate_type_qualifiers(p, dctl);
  kind = get_char(p, dctl);
  if (kind == 'P' || kind == 'R' || kind == 'E' || kind == 'H') {
    a_boolean need_space = TRUE;
    /* Pointer, reference, rvalue reference, or C++/CLI pointer-like type.
       For example, "Pc" is pointer to char. */
    if (kind == 'H') {
      /* Some kind of C++/CLI pointer-like type (handle, tracking reference,
         interior_ptr, pin_ptr). */
      p++;
      ext_kind = get_char(p, dctl);
      if (ext_kind == 'i') {
        write_id_str("interior_ptr<", dctl);
        need_space = FALSE;
      } else if (ext_kind == 'p') {
        write_id_str("pin_ptr<", dctl);
        need_space = FALSE;
      }  /* if */
    }  /* if */
    p = demangle_type_first_part(p+1, /*under_lhs_declarator=*/TRUE,
                                 need_space, dctl);
    /* Output "*" (pointer), "&" (reference), "&&" (rvalue reference),
       "^" (handle), or "%" (tracking reference). */
    if (kind == 'R') {
      write_id_ch('&', dctl);
    } else if (kind == 'E') {
      write_id_str("&&", dctl);
    } else if (kind == 'H') {
      if (ext_kind == 'h') {
        write_id_ch('^', dctl);
      } else if (ext_kind == 't') {
        write_id_ch('%', dctl);
      } else if (ext_kind == 'i') {
        write_id_ch('>', dctl);
      } else if (ext_kind == 'p') {
        write_id_ch('>', dctl);
      } else {
        bad_mangled_name(dctl);
      }  /* if */
    } else {
      write_id_ch('*', dctl);
    }  /* if */
    /* Output the type qualifiers on the pointer, if any. */
    (void)demangle_type_qualifiers(qualp, need_trailing_space, dctl);
  } else if (kind == 'M') {
    /* Pointer-to-member type, e.g., "M1Ai" is pointer to member of A of
       type int. */
    a_const_char *classp = p+1;
    /* Skip over the class name. */
    dctl->suppress_id_output++;
    p = demangle_type_name(classp, dctl);
    dctl->suppress_id_output--;
    p = demangle_type_first_part(p, /*under_lhs_declarator=*/TRUE,
                                 /*need_trailing_space=*/TRUE, dctl);
    /* Output Classname::*. */
    (void)demangle_type_name(classp, dctl);
    write_id_str("::*", dctl);
    /* Output the type qualifiers on the pointer, if any. */
    (void)demangle_type_qualifiers(qualp, need_trailing_space, dctl);
  } else if (kind == 'F') {
    /* Function type, e.g., "Fii_f" is function(int, int) returning float.
       The return type is not present for top-level function types (except
       for template functions). */
    p++;
    /* An optional ref-qualifier is indicated if the 'F' is followed by
       an underscore.  Skip it on this pass. */
    if (get_char(p, dctl) == '_' &&
        (get_char(p+1, dctl) == 'R' || get_char(p+1, dctl) == 'E')) {
      p += 2;
    }  /* if */
    /* An optional exception specification.  Skip it on this pass. */
    if (get_char(p, dctl) == 'D' &&
        (get_char(p+1, dctl) == 'o' || get_char(p+1, dctl) == 'O')) {
      if (get_char(p+1, dctl) == 'O') {
        dctl->suppress_id_output++;
        p = demangle_constant(p+2, /*suppress_address_of=*/FALSE,
                              /*need_parens=*/FALSE, dctl);
        dctl->suppress_id_output--;
      } else {
        p += 2;
      }  /* if */
    }  /* if */
    p = skip_extern_C_indication(p, dctl);
    /* Skip over the parameter types without outputting anything. */
    dctl->suppress_id_output++;
    p = demangle_function_parameters(p, dctl);
    dctl->suppress_id_output--;
    if (get_char(p, dctl) == '_' && get_char(p+1, dctl) != '_') {
      /* The return type is present. */
      p = demangle_type_first_part(p+1, /*under_lhs_declarator=*/FALSE,
                                   /*need_trailing_space=*/TRUE, dctl);
    }  /* if */
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) write_id_ch('(', dctl);
  } else if (kind == 'A') {
    /* Array type, e.g., "A10_i" is array[10] of int. */
    p++;
    if (get_char(p, dctl) == '_') {
      /* Length is specified by a constant expression based on template
         parameters.  Ignore the expression. */
      p++;
      dctl->suppress_id_output++;
      p = demangle_constant(p, /*suppress_address_of=*/FALSE,
                            /*need_parens=*/FALSE, dctl);
      dctl->suppress_id_output--;
    } else if (get_char(p, dctl) == 'O') {
      /* Length is specified as an expression. */
      dctl->suppress_id_output++;
      p = demangle_expression(p, /*need_parens=*/FALSE, dctl);
      dctl->suppress_id_output--;
    } else {
      /* Normal constant number of elements. */
      /* Skip the array size. */
      while (isdigit((unsigned char)get_char(p, dctl))) p++;
    }  /* if */
    p = advance_past_underscore(p, dctl);
    /* Process the element type. */
    p = demangle_type_first_part(p, /*under_lhs_declarator=*/FALSE,
                                 /*need_trailing_space=*/TRUE, dctl);
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) write_id_ch('(', dctl);
  } else if (kind == 'D') {
    /* The 'D' is used as an "escape" character.  The following character
       determines the actual action to be taken. */
    p++;
    kind = get_char(p, dctl);
    if (kind == 'p') {
      /* A pack expansion. */
      p = demangle_type_first_part(p+1, /*under_lhs_declarator=*/FALSE,
                                   /*need_trailing_space=*/FALSE, dctl);
    } else {
      bad_mangled_name(dctl);
    }  /* if */
  } else {
    /* No declarator part to process.  Handle the specifier type. */
    p = demangle_type_specifier(qualp, dctl);
    if (need_trailing_space) write_id_ch(' ', dctl);
  }  /* if */
  return p;
}  /* demangle_type_first_part */


static void demangle_type_second_part(
                               a_const_char               *ptr,
                               a_boolean                  under_lhs_declarator,
                               a_decode_control_block_ptr dctl)
/*
Demangle the type at ptr and output the part of the declarator that follows the
name.  This routine does not return a pointer to the character position
following what was demangled; it is assumed that the caller will save
that from the call of demangle_type_first_part, and it saves a lot of
time if this routine can avoid scanning the specifiers again.
If under_lhs_declarator is TRUE, this type is directly under a type that
uses a left-side declarator, e.g., a pointer type.  (That's used to control
use of parentheses around parts of the declarator.)
*/
{
  a_const_char *p = ptr, *qualp = p;
  char         kind;

  /* Remove type qualifiers. */
  p = remove_immediate_type_qualifiers(p, dctl);
  kind = get_char(p, dctl);
  if (kind == 'P' || kind == 'R' || kind == 'E' || kind == 'H') {
    /* Pointer, reference, rvalue reference, or C++/CLI pointer-like type.
       For example, "Pc" is pointer to char. */
    /* If it's a C++/CLI pointer-like type, there's a second character after
       the "H", but we ignore that here. */
    if (kind == 'H') p++;
    demangle_type_second_part(p+1, /*under_lhs_declarator=*/TRUE, dctl);
  } else if (kind == 'M') {
    /* Pointer-to-member type, e.g., "M1Ai" is pointer to member of A of
       type int. */
    /* Advance over the class name. */
    dctl->suppress_id_output++;
    p = demangle_type_name(p+1, dctl);
    dctl->suppress_id_output--;
    demangle_type_second_part(p, /*under_lhs_declarator=*/TRUE, dctl);
  } else if (kind == 'F') {
    a_const_char *ref_qual, *save_noexcept_constant = NULL;
    a_boolean    is_noexcept = FALSE;
    /* Function type, e.g., "Fii_f" is function(int, int) returning float.
       The return type is not present for top-level function types (except
       for template functions). */
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) write_id_ch(')', dctl);
    p++;
    /* An optional ref-qualifier is indicated if the 'F' is followed by
       an underscore.  Emit the ref-qualifier at the end of the type. */
    p = demangle_ref_qualifiers(p, &ref_qual, dctl);
    /* An optional exception specification. */
    if (get_char(p, dctl) == 'D' &&
        (get_char(p+1, dctl) == 'o' || get_char(p+1, dctl) == 'O')) {
      if (get_char(p+1, dctl) == 'O') {
        dctl->suppress_id_output++;
        save_noexcept_constant = p+2;
        p = demangle_constant(save_noexcept_constant,
                              /*suppress_address_of=*/FALSE,
                              /*need_parens=*/FALSE, dctl);
        dctl->suppress_id_output--;
      } else {
        is_noexcept = TRUE;
        p += 2;
      }  /* if */
    }  /* if */
    p = skip_extern_C_indication(p, dctl);
    /* Put out the parameter types. */
    p = demangle_function_parameters(p, dctl);
    /* Put out any cv-qualifiers (member functions). */
    /* Note that such things could come up on nonmember functions in the
       presence of typedefs.  In such a case what we generate here will not
       be valid C, but it's a reasonable representation of the mangled
       type, and there's no way of getting the typedef name in there,
       so let it be. */
    if (*qualp != 'F') {
      write_id_ch(' ', dctl);
      (void)demangle_type_qualifiers(qualp, /*trailing_space=*/FALSE, dctl);
    }  /* if */
    if (get_char(p, dctl) == '_' && get_char(p+1, dctl) != '_') {
      /* Process the return type. */
      demangle_type_second_part(p+1, /*under_lhs_declarator=*/FALSE, dctl);
    }  /* if */
    if (ref_qual != NULL) write_id_str(ref_qual, dctl);
    if (is_noexcept) {
      write_id_str(" noexcept", dctl);
    } else if (save_noexcept_constant != NULL) {
      write_id_str(" noexcept(", dctl);
      (void)demangle_constant(save_noexcept_constant,
                              /*suppress_address_of=*/FALSE,
                              /*need_parens=*/FALSE, dctl);
      write_id_ch(')', dctl);
    }  /* if */
  } else if (kind == 'A') {
    /* Array type, e.g., "A10_i" is array[10] of int. */
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) write_id_ch(')', dctl);
    write_id_ch('[', dctl);
    p++;
    if (get_char(p, dctl) == '_') {
      /* Length is specified by a constant expression based on template
         parameters. */
      p++;
      p = demangle_constant(p, /*suppress_address_of=*/FALSE,
                            /*need_parens=*/FALSE, dctl);
    } else if (get_char(p, dctl) == 'O') {
      /* Length is specified as an expression. */
      p = demangle_expression(p, /*need_parens=*/FALSE, dctl);
    } else {
      /* Normal constant number of elements. */
      if (get_char(p, dctl) == '0' && get_char(p+1, dctl) == '_') {
        /* Size is zero, so do not put out a size (the result is "[]"). */
        p++;
      } else {
        /* Put out the array size. */
        while (isdigit((unsigned char)get_char(p, dctl))) {
          write_id_ch(*p++, dctl);
        }  /* while */
      }  /* if */
    }  /* if */
    p = advance_past_underscore(p, dctl);
    write_id_ch(']', dctl);
    /* Process the element type. */
    demangle_type_second_part(p, /*under_lhs_declarator=*/FALSE, dctl);
  } else if (kind == 'D') {
    /* The 'D' is used as an "escape" character.  The following character
       determines the actual action to be taken. */
    p++;
    kind = get_char(p, dctl);
    if (kind == 'p') {
      /* A pack expansion. */
      p++;
      write_id_str("...", dctl);
      demangle_type_second_part(p, /*under_lhs_declarator=*/FALSE, dctl);
    } else {
      bad_mangled_name(dctl);
    }  /* if */
  } else {
    /* No declarator part to process.  No need to scan the specifiers type --
       it was done by demangle_type_first_part. */
  }  /* if */
}  /* demangle_type_second_part */


static a_const_char *demangle_type(a_const_char               *ptr,
                                   a_decode_control_block_ptr dctl)
/*
Demangle the type at ptr and output the demangled form.  Return a pointer to
the character position following what was demangled.
*/
{
  a_const_char *p;

  /* Generate the specifier part of the type. */
  p = demangle_type_first_part(ptr, /*under_lhs_declarator=*/FALSE,
                               /*need_trailing_space=*/FALSE, dctl);
  /* Generate the declarator part of the type. */
  demangle_type_second_part(ptr, /*under_lhs_declarator=*/FALSE, dctl);
  return p;
}  /* demangle_type */


static a_const_char *demangle_identifier_with_preceding_length(
                     a_const_char               *ptr,
                     a_boolean                  suppress_parent_and_local_info,
                     a_decode_control_block_ptr dctl)
/*
Demangle the identifier at ptr and output the demangled form.  The
identifier is preceded by a length.  Return a pointer to the character
position following what was demangled.  An identifier can include template
argument, parent, and function-local information.
If suppress_parent_and_local_info is TRUE, do not output parent and
function-local information if present (but do scan over it).
*/
{
  a_const_char  *p = ptr, *prev_end;
  unsigned long nchars;

  p = get_length(p, &nchars, &prev_end, dctl);
  dctl->mangling_nesting_level++;
  p = full_demangle_identifier(p, nchars, suppress_parent_and_local_info,
                               dctl);
  dctl->mangling_nesting_level--;
  dctl->end_of_name = prev_end;
  return p;
}  /* demangle_identifier_with_preceding_length */


static a_const_char *full_demangle_identifier(
                     a_const_char               *ptr,
                     unsigned long              nchars,
                     a_boolean                  suppress_parent_and_local_info,
                     a_decode_control_block_ptr dctl)
/*
Demangle the identifier at ptr and output the demangled form.  Return
a pointer to the character position following what was demangled.
If nchars > 0, take no more than that many characters.
If suppress_parent_and_local_info is TRUE, do not output parent
and function-local information if present (but do scan over it).
An identifier can include template argument, parent, and function-local
information.
*/
{
  a_const_char  *p = ptr, *pname, *end_ptr, *function_local_end_ptr = NULL;
  a_const_char  *final_specialization, *end_ptr_first_scan, *prev_end = NULL;
  char          ch;
  a_const_char  *oname;
  a_boolean     is_function = TRUE;
  a_template_param_block
                temp_par_info;
  a_boolean     is_externalized_static = FALSE;
  a_boolean     has_function_local_info = FALSE;
  unsigned long instance;

  clear_template_param_block(&temp_par_info);
  if (nchars != 0) {
    prev_end = dctl->end_of_name;
    dctl->end_of_name = ptr + nchars;
  }  /* if */
  if (start_of_id_is("__STF__", ptr, dctl)) {
    /* Static function made external by addition of prefix "__STF__" and
       suffix of module id. */
    is_externalized_static = TRUE;
    /* Advance past __STF__. */
    ptr += 7;
    if (nchars != 0) nchars -= 7;
    p = ptr;
  }  /* if */
  /* Scan through the name (the first part of the mangled name) without
     generating output, to see what's beyond it.  Special processing is
     necessary for names of constructors, conversion routines, etc. */
  /* If the name has a specialization indication in it (which can happen for
     function names), note that fact. */
  temp_par_info.set_final_specialization = TRUE;
  dctl->suppress_id_output++;
  p = demangle_name(ptr, nchars, /*stop_on_underscores=*/TRUE,
                    (unsigned long *)NULL,
                    (char *)NULL, &temp_par_info, 
                    (a_boolean *)NULL, dctl);
  dctl->suppress_id_output--;
  final_specialization = temp_par_info.final_specialization;
  clear_template_param_block(&temp_par_info);
  temp_par_info.final_specialization = final_specialization;
  if (get_char(p, dctl) == '\0') {
    /* There is no mangled part of the name.  This happens for strange
       cases like
         extern "C" int operator +(A, A);
       which gets mangled as "__pl".  Just write out the name and stop. */
    end_ptr = demangle_name(ptr, nchars,
                            /*stop_on_underscores=*/TRUE,
                            (unsigned long *)NULL,
                            (char *)NULL,
                            (a_template_param_block_ptr)NULL, 
                            (a_boolean *)NULL, dctl);
  } else {
    /* There's more.  There should be a "__" between the name and the
       additional mangled information. */
    if (get_char(p, dctl) != '_' || get_char(p+1, dctl) != '_') {
      bad_mangled_name(dctl);
      end_ptr = p;
      goto end_of_routine;
    }  /* if */
    end_ptr = p + 2;
    /* Now ptr points to the original-name part of the mangled name, and
       end_ptr points to the mangled-name part at the end.
         f__1AFv
            ^---- end_ptr
         ^------- ptr
       The mangled-name part is
         (a)  A class name for a static data member.
         (b)  A class name followed by "F" followed by the encoding for the
              parameter types for a member function.
         (c)  "F" followed by the encoding for the parameter types for a
              nonmember function.
         (d)  "L" plus a local block number, followed by the mangled function
              name, for a function-local entity.
       Members of namespaces are encoded similarly. */
    p = end_ptr;
    pname = NULL;
    if (suppress_parent_and_local_info) dctl->suppress_id_output++;
    ch = get_char(end_ptr, dctl);
    if (ch == 'L') {
      unsigned long nchars2 = nchars;
      /* The name of an entity within a function, mangled on promotion out
         of the function.  For example, "i__L1__f__Fv" for "i" from block 1
         of function "f(void)".  Note that this is not the same mangling
         used by cfront (in the cfront scheme, the __L1 is at the end, and
         the number is different). */
      /* Set a length for the name without the function-local indication,
         for the processing in the rest of this routine. */
      nchars = (unsigned long)((p - 2) - ptr);
      /* Demangle the function name and block number. */
      p++;  /* Points to the block number following "__L". */
      if (nchars2 != 0) nchars2 -= (unsigned long)(p - ptr);
      function_local_end_ptr =
              demangle_function_local_indication(p, nchars2, &instance, dctl);
      has_function_local_info = TRUE;
      p = end_ptr = ptr + nchars;
      is_function = FALSE;
      /* Go on to demangle the name of the local entity. */
    } else if (ch != 'F') {
      /* A class (or namespace) name must be next. */
      /* Remember the location of the parent entity name. */
      pname = end_ptr;
      /* Scan over the class name, producing no output, and remembering the
         position of the final specialization, if any.  If we already
         found a specialization on the function name, it's the final one
         and we shouldn't change it. */
      dctl->suppress_id_output++;
      if (temp_par_info.final_specialization == NULL) {
        temp_par_info.set_final_specialization = TRUE;
      }  /* if */
      end_ptr = full_demangle_type_name(pname, /*base_name_only=*/FALSE,
                                        &temp_par_info,
                                        /*is_destructor_name=*/FALSE,
                                        dctl);
      temp_par_info.set_final_specialization = FALSE;
      dctl->suppress_id_output--;
      /* If the name ends here, this is a simple member (e.g., a static
         data member). */
      ch = get_char(end_ptr, dctl);
      if (ch == '\0' ||
          (ch == '_' && get_char(end_ptr+1, dctl) == '_')) {
        is_function = FALSE;
      }  /* if */
    }  /* if */
    if (suppress_parent_and_local_info) dctl->suppress_id_output--;
    oname = NULL;
    if (is_function) {
      /* "S" here means a static member function (ignore). */
      if (get_char(end_ptr, dctl) == 'S') end_ptr++;
      /* "O" here means the base class of a function that this function
         explicitly overrides (a Microsoft extension) is next. */
      if (get_char(end_ptr, dctl) == 'O') {
        /* Skip over the class name, producing no output.  Remember its
           position for later output. */
        oname = ++end_ptr;
        dctl->suppress_id_output++;
        end_ptr = demangle_type_name(oname, dctl);
        dctl->suppress_id_output--;
      }  /* if */
      /* Write the specifier part of the type. */
      end_ptr_first_scan =
                  demangle_type_first_part(end_ptr,
                                           /*under_lhs_declarator=*/FALSE,
                                           /*need_trailing_space=*/TRUE, dctl);
    }  /* if */
    temp_par_info.nesting_level = 0;
    if (pname != NULL &&
        !suppress_parent_and_local_info) {
      /* Write the parent class or namespace qualifier. */
      if (temp_par_info.final_specialization != NULL) {
        /* Up to the final specialization, put out actual template arguments
           for specializations. */
        temp_par_info.actual_template_args_until_final_specialization = TRUE;
      }  /* if */
      (void)full_demangle_type_name(pname, /*base_name_only=*/FALSE,
                                    &temp_par_info,
                                    /*is_destructor_name=*/FALSE,
                                    dctl);
      /* Force template parameter information out on the function even if
         it is specialized. */
      temp_par_info.actual_template_args_until_final_specialization = FALSE;
      write_id_str("::", dctl);
    }  /* if */
    /* Write the name of the member. */
    (void)demangle_name(ptr, nchars, /*stop_on_underscores=*/TRUE,
                        (unsigned long *)NULL,
                        pname, &temp_par_info, 
                        (a_boolean *)NULL, dctl);
    if (oname != NULL) {
      /* Put out the name of the class of the function explicitly overridden,
         if noted above. */
      write_id_str(" [overriding function in ", dctl);
      (void)demangle_type_name(oname, dctl);
      write_id_str("] ", dctl);
    }  /* if */
    if (is_function) {
      /* Write the declarator part of the type. */
      demangle_type_second_part(end_ptr, /*under_lhs_declarator=*/FALSE,
                                dctl);
      end_ptr = end_ptr_first_scan;
    }  /* if */
    if (!temp_par_info.use_old_form_for_template_output &&
        temp_par_info.nesting_level != 0) {
      /* Put out correspondences for template parameters, e.g, "T=int". */
      temp_par_info.nesting_level = 0;
      temp_par_info.first_correspondence = TRUE;
      temp_par_info.output_only_correspondences = TRUE;
      /* Output is suppressed in general, and turned on only where
         appropriate. */
      dctl->suppress_id_output++;
      if (pname != NULL) {
        /* Write the parent class or namespace qualifier. */
        if (temp_par_info.final_specialization != NULL) {
          /* Up to the final specialization, put out actual template arguments
             for specializations. */
          temp_par_info.actual_template_args_until_final_specialization = TRUE;
        }  /* if */
        (void)full_demangle_type_name(pname, /*base_name_only=*/FALSE,
                                      &temp_par_info,
                                      /*is_destructor_name=*/FALSE,
                                      dctl);
      }  /* if */
      /* Force template parameter information out on the function even if
         it is specialized. */
      temp_par_info.actual_template_args_until_final_specialization = FALSE;
      /* Write the name of the member. */
      (void)demangle_name(ptr, nchars, /*stop_on_underscores=*/TRUE,
                          (unsigned long *)NULL,
                          pname, &temp_par_info, 
                          (a_boolean *)NULL, dctl);
      dctl->suppress_id_output--;
      if (!temp_par_info.first_correspondence) {
        /* End the list of correspondences. */
        write_id_ch(']', dctl);
      }  /* if */
    }  /* if */
  }  /* if */
end_of_routine:
  /* If the identifier had local function information, write the instance
     number now. */
  if (has_function_local_info) emit_instance(instance, dctl);
  /* When a function-local indication is scanned, end_ptr has been set
     to the end of the local entity name, and needs to be set to after the
     function-local indication at the end of the whole name. */
  if (function_local_end_ptr != NULL) end_ptr = function_local_end_ptr;
  if (is_externalized_static) {
    /* Advance over the module id part of the name. */
    while (get_char(end_ptr, dctl) != '\0') end_ptr++;
  }  /* if */
  if (prev_end != NULL) dctl->end_of_name = prev_end;
  return end_ptr;
}  /* full_demangle_identifier */


static a_boolean is_mangled_type_name(a_const_char               *ptr,
                                      a_decode_control_block_ptr dctl)
/*
Return TRUE if the encoding beginning at ptr appears to be a mangled
type name.  This is used to distinguish a local mangled non-nested
type name with template arguments (e.g., __15MyTemp__tm__2_i) from a
cfront-style local name (e.g., __2name); the character passed in is
the one after the double underscore.
*/
{
  a_boolean    is_type_name = FALSE;
  a_const_char *p = ptr;

  if (isdigit((unsigned char)get_char(p, dctl))) {
    /* Skip over the number. */
    do { p++; } while (isdigit((unsigned char)get_char(p, dctl)));
    /* The next character is typically alphabetic. */
    if (isalpha((unsigned char)get_char(p, dctl))) {
      /* This doesn't have to be a full recognizer; it just has to distinguish
         the two cases given above.  To do that, look for the double underscore
         that must appear in a mangled name that has template arguments. */
      for (p++; get_char(p, dctl) != '\0'; p++) {
        if (get_char(p, dctl) == '_' && get_char(p+1, dctl) == '_') {
          is_type_name = TRUE;
          break;
        }  /* if */
      }  /* for */
    } else if (get_char(p, dctl) == '_' &&
               get_char(p+1, dctl) == '_' &&
               get_char(p+2, dctl) == 'U' &&
               (get_char(p+3, dctl) == 't' ||
                get_char(p+3, dctl) == 'l' ||
                get_char(p+3, dctl) == 'm' ||
                get_char(p+3, dctl) == 'd')) {
      /* An unnamed type or lambda. */
      is_type_name = TRUE;
    }  /* if */
  }  /* if */
  return is_type_name;
}  /* is_mangled_type_name */


static a_const_char *demangle_static_variable_name(
                                               a_const_char               *ptr,
                                               a_decode_control_block_ptr dctl)
/*
Demangle the name of a static variable promoted to being external by
addition of a prefix "__STV__" and a suffix of a module id.  Just put out
the part in the middle, which is the original name.
*/
{
  a_const_char *start_ptr;

  ptr += 7;  /* Move to after "__STV__". */
  /* Copy the name until "__". */
  start_ptr = ptr;
  while (get_char(ptr,   dctl) != '_' ||
         get_char(ptr+1, dctl) != '_' ||
         ptr == start_ptr) {
    if (get_char(ptr, dctl) == '\0') {
      bad_mangled_name(dctl);
      break;
    }  /* if */
    write_id_ch(*ptr, dctl);
    ptr++;
  }  /* while */
  /* Advance over the module id part of the name. */
  while (get_char(ptr, dctl) != '\0') ptr++;
  return ptr;
}  /* demangle_static_variable_name */


static a_const_char *demangle_local_name(a_const_char               *ptr,
                                         a_decode_control_block_ptr dctl)
/*
Demangle the local name at ptr and output the demangled form.  Return
a pointer to the character position following what was demangled.
This demangles the "__nn_mm_name" form produced by the C-generating
back end.  This is not something visible unless the C-generating back end
is used, and it's a local name, which is ordinarily outside the charter
of these demangling routines, but it's an easy and common case, so...

Also handles the cfront-style __nnName form.
*/
{
  a_const_char *p = ptr+2;

  /* Check for the initial two numbers and underscores.  The caller checked
     for the two initial underscores and the digit following that. */
  do { p++; } while (isdigit((unsigned char)get_char(p, dctl)));
  if (isalpha((unsigned char)get_char(p, dctl))) {
    /* Cfront-style local name, like "__2name". */
  } else {
    if (get_char(p, dctl) != '_') {
      bad_mangled_name(dctl);
      goto end_of_routine;
    }  /* if */
    p++;
    if (!isdigit((unsigned char)get_char(p, dctl))) {
      bad_mangled_name(dctl);
      goto end_of_routine;
    }  /* if */
    do { p++; } while (isdigit((unsigned char)get_char(p, dctl)));
    if (get_char(p, dctl) != '_') {
      bad_mangled_name(dctl);
      goto end_of_routine;
    }  /* if */
    p++;
  }  /* if */
  /* Copy the rest of the string to output. */
  while (get_char(p, dctl) != '\0') {
    write_id_ch(*p, dctl);
    p++;
  }  /* while */
end_of_routine:
  return p;
}  /* demangle_local_name */


static a_const_char *uncompress_mangled_name(a_const_char               *id,
                                             a_decode_control_block_ptr dctl)
/*
Uncompress the compressed mangled name beginning at id.  Return the
address of the uncompressed name.
*/
{
  a_const_char  *uncompressed_name = id, *src_end = dctl->end_of_name;
  unsigned long length;

  /* Advance past "__CPR". */
  id += 5;
  /* Accumulate the length of the uncompressed name. */
  if (!isdigit((unsigned char)*id)) {
    bad_mangled_name(dctl);
    goto end_of_routine;
  }  /* if */
  id = get_number(id, &length, dctl);
  /* Check for the two underscores following the length. */
  if (id[0] != '_' || id[1] != '_') {
    bad_mangled_name(dctl);
    goto end_of_routine;
  }  /* if */
  /* Save the uncompressed length so it can be used later in telling the
     caller how big a buffer is required. */
  dctl->uncompressed_length = length;
  id += 2;
  if (length+1 >= dctl->output_id_size) {
    /* The buffer supplied by the caller is too small to contain the
       uncompressed name. */
    dctl->output_overflow_err = TRUE;
    goto end_of_routine;
  } else {
    a_const_char *src, *dst_end = dctl->output_id+dctl->output_id_size;
    char         *dst;
    /* Uncompress to the end of the buffer supplied by the caller, then
       do the demangling in the space remaining at the beginning. */
    uncompressed_name = dst_end-(length+1);
    dctl->output_id_size -= length+1;
    dst = (char *)uncompressed_name;
    for (src = id; *src != '\0';) {
      char ch = *src++;
      if (ch != 'J') {
        /* Just copy this character. */
        if (dst >= dst_end) {
          /* Overflowed buffer (probably malformed input). */
          bad_mangled_name(dctl);
          goto end_of_routine;
        }  /* if */
        *dst++ = ch;
      } else {
        if (*src == 'J') {
          /* "JJ" indicates a simple "J". */
          /* Simple "J". */
          if (dst >= dst_end) {
            /* Overflowed buffer (probably malformed input). */
            bad_mangled_name(dctl);
            goto end_of_routine;
          }  /* if */
          *dst++ = 'J';
        } else {
          /* "JnnnJ" indicates a repetition of a string that appeared
             earlier, at position "nnn". */
          unsigned long pos, prev_len;
          a_const_char  *prev_str, *prev_str2, *prev_end;
          dctl->end_of_name = src_end;
          src = get_number(src, &pos, dctl);
          if (*src != 'J' || pos > length) {
            bad_mangled_name(dctl);
            goto end_of_routine;
          }  /* if */
          prev_str = uncompressed_name+pos;
          if (!isdigit(*prev_str)) {
            bad_mangled_name(dctl);
            goto end_of_routine;
          }  /* if */
          /* Get the length of the repeated string. */
          dctl->end_of_name = uncompressed_name + length;
          prev_str2 = get_length(prev_str, &prev_len, &prev_end, dctl);
          /* Copy the repeated string to the uncompressed output. */
          prev_str2 += prev_len;
          if (dst+prev_len >= dst_end) {
            /* Overflowed buffer (probably malformed input). */
            bad_mangled_name(dctl);
            goto end_of_routine;
          }  /* if */
          while (prev_str < prev_str2) *dst++ = *prev_str++;
        }  /* if */
        /* Advance past the final "J". */
        src++;
      }  /* if */
    }  /* for */
    if (dst - uncompressed_name != length) {
      /* The length didn't come out right. */
      bad_mangled_name(dctl);
    }  /* if */
    if (dst >= dst_end) {
      /* Overflowed buffer (probably malformed input). */
      bad_mangled_name(dctl);
      goto end_of_routine;
    }  /* if */
    /* Add the final null. */
    *dst++ = '\0';
    dctl->end_of_name = uncompressed_name + length;
  }  /* if */
end_of_routine:;
  return uncompressed_name;
}  /* uncompress_mangled_name */


void decode_identifier(a_const_char *id,
                       char         *output_buffer,
                       sizeof_t     output_buffer_size,
                       a_boolean    *err,
                       a_boolean    *buffer_overflow_err,
                       sizeof_t     *required_buffer_size)
/*
Demangle the identifier id (which is null-terminated), and put the demangled
form (null-terminated) into the output_buffer provided by the caller.
output_buffer_size gives the allocated size of output_buffer.  If there
is some error in the demangling process, *err will be returned TRUE.
In addition, if the error is that the output buffer is too small,
*buffer_overflow_err will (also) be returned TRUE, and *required_buffer_size
is set to the size of buffer required to do the demangling.  Note that
if the mangled name is compressed, and the buffer size is smaller than
the size of the uncompressed mangled name, the size returned will be
enough to uncompress the name but not enough to produce the demangled form.
The caller must be prepared in that case to loop a second time (the
length returned the second time will be correct).
*/
{
  a_const_char               *end_ptr, *p;
  a_decode_control_block     control_block;
  a_decode_control_block_ptr dctl = &control_block;

  clear_control_block(dctl);
  dctl->end_of_name = strchr(id, '\0');
  dctl->output_id = output_buffer;
  dctl->output_id_size = output_buffer_size;
  if (start_of_id_is("__CPR", id, dctl)) {
    /* Uncompress a compressed name. */
    id = uncompress_mangled_name(id, dctl);
  }  /* if */
  /* Check for special cases. */
  if (dctl->output_overflow_err) {
    /* Previous error (not enough room in the buffer to uncompress). */
  } else if (dctl->err_in_id) {
    /* Invalid compressed input. */
  } else if (start_of_id_is("__vtbl__", id, dctl)) {
    write_id_str("virtual function table for ", dctl);
    /* The overall mangled name is one of
         __vtbl__ <class mangling>
         __vtbl__ <base class mangling> __ <class mangling>
         __vtbl__ <base class mangling> __ <base class mangling>
                                        __ <class mangling>
    */
    end_ptr = demangle_vtbl_class_name(id+8, dctl);
    while (start_of_id_is("__", end_ptr, dctl)) {
      /* Further derived class. */
      end_ptr += 2;
      write_id_str(" in ", dctl);
      end_ptr = demangle_vtbl_class_name(end_ptr, dctl);
    }  /* while */
  } else if (start_of_id_is("__CBI__", id, dctl)) {
    write_id_str("can-be-instantiated flag for ", dctl);
    end_ptr = demangle_identifier(id+7, dctl);
  } else if (start_of_id_is("__DNI__", id, dctl)) {
    write_id_str("do-not-instantiate flag for ", dctl);
    end_ptr = demangle_identifier(id+7, dctl);
  } else if (start_of_id_is("__TIR__", id, dctl)) {
    write_id_str("template-instantiation-request flag for ", dctl);
    end_ptr = demangle_identifier(id+7, dctl);
  } else if (start_of_id_is("__LSG__", id, dctl)) {
    write_id_str("initialization guard variable for ", dctl);
    end_ptr = demangle_identifier(id+7, dctl);
  } else if (start_of_id_is("__THI__", id, dctl)) {
    write_id_str("thread_local initialization routine for ", dctl);
    end_ptr = demangle_identifier(id+7, dctl);
  } else if (start_of_id_is("__IFV__", id, dctl)) {
    id += 7;
    write_id_str("ifunc variable for ", dctl);
    if (start_of_id_is("__IFC__", id, dctl)) {
      /* This can be nested (and this routine isn't recursive). */
      write_id_str("ifunc function for ", dctl);
      id += 7;
    }  /* if */
    end_ptr = demangle_identifier(id, dctl);
  } else if (start_of_id_is("__RES__", id, dctl)) {
    id += 7;
    write_id_str("resolver function for ", dctl);
    if (start_of_id_is("__IFC__", id, dctl)) {
      /* This can be nested (and this routine isn't recursive). */
      write_id_str("ifunc function for ", dctl);
      id += 7;
    }  /* if */
    end_ptr = demangle_identifier(id, dctl);
  } else if (start_of_id_is("__IFC__", id, dctl)) {
    write_id_str("ifunc function for ", dctl);
    end_ptr = demangle_identifier(id+7, dctl);
  } else if (start_of_id_is("__TGT__", id, dctl)) {
    /* A "target" attribute: look for closing "__". */
    a_const_char *target_attr, *target_attr_end;
    id += 7;
    target_attr = id;
    while (id < dctl->end_of_name) {
      if (start_of_id_is("__", id, dctl)) {
        target_attr_end = id;
        id += 2;
        break;
      }  /* if */
      id++;
    }  /* while */
    if (id < dctl->end_of_name) {
      end_ptr = demangle_identifier(id, dctl);
      /* Emit the "target" attribute.  Note that this isn't exactly in the
         same format as the input (commas and periods have been replaced by
         underscores), but it should convey the idea. */
      write_id_str(" __attribute__((target(", dctl);
      while (target_attr < target_attr_end) {
        write_id_ch(*target_attr++, dctl);
      }  /* if */
      write_id_str(")))", dctl);
    } else {
      bad_mangled_name(dctl);
      end_ptr = id;
    }  /* if */
  } else if (start_of_id_is("__TWR__", id, dctl)) {
    write_id_str("thread_local wrapper for ", dctl);
    end_ptr = demangle_identifier(id+7, dctl);
  } else if (start_of_id_is("__TID_", id, dctl)) {
    write_id_str("type identifier for ", dctl);
    end_ptr = demangle_type(id+6, dctl);
  } else if (start_of_id_is("__T_", id, dctl)) {
    write_id_str("typeinfo for ", dctl);
    end_ptr = demangle_type(id+4, dctl);
  } else if (start_of_id_is("__VFE__", id, dctl)) {
    write_id_str("surrogate in class ", dctl);
    p = demangle_type(id+7, dctl);
    if (get_char(p, dctl) != '_' || get_char(p+1, dctl) != '_') {
      bad_mangled_name(dctl);
      end_ptr = p;
    } else {
      write_id_str(" for ", dctl);
      end_ptr = demangle_identifier(p+2, dctl);
    }  /* if */
  } else if (start_of_id_is("__Q", id, dctl) ||
             (start_of_id_is("__", id, dctl) &&
              is_mangled_type_name(id+2, dctl))) {
    /* Mangled type name. */
    end_ptr = demangle_type_name(id+2, dctl);
  } else if (start_of_id_is("__STV__", id, dctl)) {
    /* Static variable made external by addition of prefix "__STV__" and
       suffix of module id. */
    end_ptr = demangle_static_variable_name(id, dctl);
  } else if (start_of_id_is("__", id, dctl) && isdigit((unsigned char)id[2])) {
    /* Local variable mangled by the C-generating back end: __nn_mm_name,
       where "nn" and "mm" are decimal integers. */
    end_ptr = demangle_local_name(id, dctl);
  } else {
    /* Normal case: function name, static data member name, or
       name of type or variable promoted out of function. */
    end_ptr = demangle_identifier(id, dctl);
  }  /* if */
  if (dctl->output_overflow_err) {
    dctl->err_in_id = TRUE;
  } else {
    /* Add a terminating null. */
    dctl->output_id[dctl->output_id_len] = 0;
  }  /* if */
  /* Make sure the whole identifier was taken. */
  if (!dctl->err_in_id && *end_ptr != '\0') bad_mangled_name(dctl);
  *err = dctl->err_in_id;
  *buffer_overflow_err = dctl->output_overflow_err;
  *required_buffer_size = dctl->output_id_len + 1; /* +1 for final null. */
  /* If the name is compressed, we need room for the uncompressed
     form, and a null, in the buffer. */
  if (dctl->uncompressed_length != 0) {
    *required_buffer_size += dctl->uncompressed_length+1;
  }  /* if */
}  /* decode_identifier */

#else /* IA64_ABI */

/*
Start of demangling code for IA-64 ABI.
*/

/*
TRUE if the bugs in the g++ 3.2 implementation of the IA-64 ABI should
be emulated.  Can be changed by a command line option.
*/
a_boolean	emulate_gnu_abi_bugs = DEFAULT_EMULATE_GNU_ABI_BUGS;

/*
TRUE if the host integer representation is little-endian.
External because it's declared extern in host_envir.h.
*/
a_boolean	host_little_endian;

/*
Bits used to represent cv-qualifiers in a bit set.
*/
typedef int a_cv_qualifier_set;
#define CVQ_NONE	((a_cv_qualifier_set)0)
#define CVQ_CONST	((a_cv_qualifier_set)0x1)
#define CVQ_VOLATILE	((a_cv_qualifier_set)0x2)
#define CVQ_RESTRICT	((a_cv_qualifier_set)0x4)

/*
Values used to represent an optional ref-qualifier.
*/
typedef int a_ref_qualifier;
#define REFQ_NONE	((a_ref_qualifier)0)
#define REFQ_LVALUE	((a_ref_qualifier)0x1)
#define REFQ_RVALUE	((a_ref_qualifier)0x2)


/*
Information about a function that has to be preserved from the
time of scanning of the name (e.g., in a <nested-name>) until later use
in processing the <bare-function-type>.
*/
typedef struct a_func_block {
  a_boolean	no_return_type;
			/* TRUE if the function is one that will not have
			   a return type encoded in the function type. */
  a_cv_qualifier_set
		cv_quals;
			/* If the function is a cv-qualified member function,
			   the set of cv-qualifiers.  0 otherwise. */
  a_ref_qualifier
		ref_qual;
			/* If the function is a ref-qualified member function,
			   the ref-qualifier.  0 otherwise. */
  char		ctor_dtor_kind;
			/* If the function is a constructor or destructor,
			   the character from the mangled name identifying its
			   kind, e.g., '2' for a subobject constructor/
			   destructor.  ' ' if the function is not a
			   constructor or destructor. */
} a_func_block;


/*
Information about an entity in the mangled name that may be reused
by referring back to it by number as a "substitution".
*/
/* Code for type of syntactic object substituted for: */
typedef enum a_substitution_kind {
  subk_unscoped_template_name,
			/* An <unscoped-template-name>. */
  subk_prefix,		/* A <prefix>. */
  subk_template_prefix,	/* A <template-prefix>. */
  subk_type,		/* A <type>. */
  subk_template_template_param
			/* A <template-template-param>. */
} a_substitution_kind;

typedef struct a_substitution {
  a_const_char	*start;	/* First character of the encoding of the entity. */
  a_substitution_kind
		kind;	/* Kind of entity. */
  unsigned long	num_levels;
			/* For subk_prefix and subk_template_prefix, the
			   number of levels of the prefix included.  That is,
			   is the substitution A:: or A::B:: or A::B::C::.
			   For the subk_template_prefix case, the count
			   is the number of complete levels (name plus
			   optional template argument list) that precede
			   the final name (and no template argument list)
			   that is part of the substitution.  (Therefore,
			   the count could be zero.) */
  a_boolean	parse_template_args;
			/* The value of the parse_template_args boolean
			   at the time the subk_type substitution is recorded
			   (so that value can be used on subsequent
			   demanglings of that substitution string).  Ignored
			   for other substitution kinds. */
} a_substitution;

static a_substitution
		*substitutions = NULL;
			/* A dynamically allocated array.  substitutions[n]
			   gives the meaning of the substitution numbered
			   "n". */
static unsigned long
		num_substitutions = 0;
			/* The number of substitutions currently defined, i.e.,
			   the number of elements of the array that have
			   been set. */
static unsigned long
		allocated_substitutions = 0;
			/* The allocated size of the array, as a number of
			   elements. */

static char     *ud_suffix_buffer = NULL;
                        /* A dynamically allocated buffer into which
                           ud-suffixes are copied (when demangling literal
                           operators). */
static unsigned long
                ud_suffix_buffer_length = 0;
                        /* The allocated length of ud_suffix_buffer. */

static a_const_char *demangle_type_first_part(
                               a_const_char               *ptr,
                               a_cv_qualifier_set         cv_quals,
                               a_boolean                  under_lhs_declarator,
                               a_boolean                  need_trailing_space,
                               a_boolean                  parse_template_args,
                               a_decode_control_block_ptr dctl);
static void demangle_type_second_part(
                               a_const_char               *ptr,
                               a_cv_qualifier_set         cv_quals,
                               a_boolean                  under_lhs_declarator,
                               a_decode_control_block_ptr dctl);
static a_const_char *full_demangle_type(
                                a_const_char               *ptr,
                                a_boolean                  parse_template_args,
                                a_boolean                  is_pack_expansion,
                                a_decode_control_block_ptr dctl);
static a_const_char *demangle_simple_id(a_const_char               *ptr,
                                        a_decode_control_block_ptr dctl);
/*
Macro to invoke full_demangle_type in the usual case where parse_template_args
is TRUE and is_pack_expansion is FALSE.
*/
#define demangle_type(ptr, dctl)                                          \
  full_demangle_type(ptr, /*parse_template_args=*/TRUE,                   \
                     /*is_pack_expansion=*/FALSE, dctl)

static a_const_char *demangle_template_args(a_const_char               *ptr,
                                            a_decode_control_block_ptr dctl);

/*
Bit mask used to determine which portion(s) of a <name> should
be emitted by demangle_name.  For most cases, DNO_ALL is correct, but in
cases where a <name> is scanned more than once, different portions of the
name may be emitted on different passes.
*/
typedef int a_demangle_name_option;
#define DNO_NONE	((a_demangle_name_option)0)
#define DNO_EXTERNALIZATION \
                        ((a_demangle_name_option)0x1)
#define DNO_NAME	((a_demangle_name_option)0x2)
#define DNO_ALL 	(DNO_EXTERNALIZATION | DNO_NAME)

static a_const_char *demangle_name(a_const_char               *ptr,
                                   a_func_block               *func_block,
                                   a_demangle_name_option     options,
                                   a_decode_control_block_ptr dctl);
static a_const_char *demangle_unresolved_name(a_const_char               *ptr,
                                              a_decode_control_block_ptr dctl);
static a_const_char *demangle_expression(a_const_char               *ptr,
                                         a_decode_control_block_ptr dctl);
static a_const_char *demangle_encoding(
                                a_const_char               *ptr,
                                a_boolean                  include_func_params,
                                a_decode_control_block_ptr dctl);
static a_const_char *demangle_nested_name_components(
                              a_const_char               *ptr,
                              unsigned long              num_levels,
                              a_boolean                  *is_no_return_name,
                              a_boolean                  *has_templ_arg_list,
                              char                       *ctor_dtor_kind,
                              a_const_char               **last_component_name,
                              a_decode_control_block_ptr dctl);
static a_const_char *demangle_unscoped_name(
                                        a_const_char               *ptr,
                                        a_func_block               *func_block,
                                        a_decode_control_block_ptr dctl);
static a_const_char *demangle_unqualified_name(
                                 a_const_char               *ptr,
                                 a_boolean                  *is_no_return_name,
                                 a_decode_control_block_ptr dctl);
static a_const_char *demangle_template_param(a_const_char               *ptr,
                                             a_decode_control_block_ptr dctl);
static void output_cv_qualifiers(a_cv_qualifier_set         cv_quals,
                                 a_boolean                  trailing_space,
                                 a_decode_control_block_ptr dctl);


static void clear_func_block(a_func_block *func_block)
/*
Clear a function information block to default values.
*/
{
  func_block->no_return_type = FALSE;
  func_block->cv_quals = 0;
  func_block->ref_qual = REFQ_NONE;
  func_block->ctor_dtor_kind = ' ';
}  /* clear_func_block */


static a_const_char *get_number(a_const_char               *p,
                                long                       *num,
                                a_decode_control_block_ptr dctl)
/*
Accumulate a number starting at position p and return its value in *num.
Return a pointer to the character position following the number.
A negative number is indicated by a leading "n".
*/
{
  long      n = 0;
  a_boolean negative = FALSE;

  if (*p == 'n') {
    negative = TRUE;
    p++;
  }  /* if */
  if (!isdigit((unsigned char)*p)) {
    bad_mangled_name(dctl);
  } else {
    do {
      n = n*10 + (*p - '0');
      p++;
    } while (isdigit((unsigned char)*p));
  }  /* if */
  if (negative) n = -n;
  *num = n;
  return p;
}  /* get_number */


static void record_substitutable_entity(
                                a_const_char               *start,
                                a_substitution_kind        kind,
                                unsigned long              num_levels,
                                a_boolean                  parse_template_args,
                                a_decode_control_block_ptr dctl)
/*
Record the entity whose mangled name starts at "start", and whose
kind (of syntax term) is given by "kind", as a potentially
substitutable entity, one that can be used again by referencing
it in a later substitution.  num_levels gives added information
for the subk_prefix and subk_template_prefix cases.
For subk_type cases, the value of parse_template_args is stored with
the substitution information (so that it can be used when demangling
the type when it is used as a substitution).
*/
{
  /* Do not record anything if we are suppressing recording of substitutions.
     One case in which that is true is when an error has been detected. */
  if (!dctl->suppress_substitution_recording) {
    unsigned long  number = num_substitutions++;
    a_substitution *subp;
    if (num_substitutions > allocated_substitutions) {
      /* Need to allocate or extend the substitutions array. */
      true_size_t new_size;
      allocated_substitutions += 500;
      new_size = allocated_substitutions*sizeof(a_substitution);
      if (substitutions == NULL) {
        substitutions = (a_substitution*)malloc(new_size);
      } else {
        substitutions = (a_substitution*)realloc(substitutions, new_size);
      }  /* if */
      if (substitutions == NULL) {
        bad_mangled_name(dctl);
        return;
      }  /* if */
    } /* if */
    subp = &substitutions[number];
    subp->start = start;
    subp->kind = kind;
    subp->num_levels = num_levels;
    subp->parse_template_args = parse_template_args;
  }  /* if */
}  /* record_substitutable_entity */


static a_const_char *demangle_substitution(
                             a_const_char               *ptr,
                             int                        type_pass_num,
                             a_cv_qualifier_set         cv_quals,
                             a_boolean                  under_lhs_declarator,
                             a_boolean                  need_trailing_space,
                             a_const_char               **last_component_name,
                             a_const_char               **substitution,
                             a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <substitution> and output the demangled form.
Return a pointer to the character position following what was demangled.
A <substitution> repeats a construct previously encoded in the
mangled name by referring to it by number, as a way to reduce the
size of mangled names.  There are also some predefined substitutions,
for entities from the standard library that are likely to come up
often.  The syntax is:

   <substitution> ::= S <seq-id> _
                  ::= S_
   <substitution> ::= St # ::std::
   <substitution> ::= Sa # ::std::allocator
   <substitution> ::= Sb # ::std::basic_string
   <substitution> ::= Ss # ::std::basic_string < char,
                                                 ::std::char_traits<char>,
                                                 ::std::allocator<char> >
   <substitution> ::= Si # ::std::basic_istream<char,  std::char_traits<char> >
   <substitution> ::= So # ::std::basic_ostream<char,  std::char_traits<char> >
   <substitution> ::= Sd # ::std::basic_iostream<char, std::char_traits<char> >

When the substitution is a type, type_pass_num indicates whether to
do the first-part (1) or second-part (2) processing.  type_pass_num == 0
means do both parts, i.e., the whole type.  cv_quals,
under_lhs_declarator, and need_trailing_space give extra information
to be passed through to the type demangling routines in that case.  If
last_component_name is non-NULL, and the substitution decoded is a
prefix of a nested name, a pointer to the encoding for the last
component of the nested name is returned in *last_component_name.  It
will not be a substitution.  This is needed for generating the names
of constructors and destructors.  If substitution is non-NULL, *substitution
is set to the point in the mangled name where the substitution source
occurs (in case the caller needs to examine it -- for example to see what
type the substitution represents).
*/
{
  char ch2 = ptr[1];

  if (last_component_name != NULL) *last_component_name = NULL;
  if (substitution != NULL) *substitution = NULL;
  if (islower((unsigned char)ch2)) {
    /* Predefined substitution. */
    a_const_char *str = "";
    a_const_char *last_name = "";
    if (ch2 == 't') {
      str = "std";
      last_name = "3std";
    } else if (ch2 == 'a') {
      str = "std::allocator";
      last_name = "9allocator";
    } else if (ch2 == 'b') {
      str = "std::basic_string";
      last_name = "12basic_string";
    } else if (ch2 == 's') {
      str = 
       "std::basic_string<char, std::char_traits<char>, std::allocator<char>>";
      last_name = "12basic_string";
    } else if (ch2 == 'i') {
      str = "std::basic_istream<char, std::char_traits<char>>";
      last_name = "13basic_istream";
    } else if (ch2 == 'o') {
      str = "std::basic_ostream<char, std::char_traits<char>>";
      last_name = "13basic_ostream";
    } else if (ch2 == 'd') {
      str = "std::basic_iostream<char, std::char_traits<char>>";
      last_name = "14basic_iostream";
    }  /* if */
    /* Output nothing if we want only the second-pass output. */
    if (type_pass_num != 2) {
      output_cv_qualifiers(cv_quals, TRUE, dctl);
      write_id_str(str, dctl);
    }  /* if */
    ptr += 2;
    if (last_component_name != NULL) *last_component_name = last_name;
  } else {
    /* Not a predefined substitution.  Convert the base-36 sequence number. */
    uint32_t        number = 0;
    a_substitution *subp;
    a_const_char   *p;
    ptr++;
    if (ch2 != '_') {
      do {
        static a_const_char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        number *= 36;
        if (*ptr == '\0') {
          bad_mangled_name(dctl);
          break;
        }  /* if */
        p = strchr(digits, *ptr);
        if (p == NULL) {
          bad_mangled_name(dctl);
          break;
        }  /* if */
        number += (uint32_t)(p - digits);
        ptr++;
      } while (*ptr != '_');
      number++;
    }  /* if */
    if (number >= num_substitutions) {
      bad_mangled_name(dctl);
    } else {
      a_func_block func_block;
      ptr = advance_past_underscore(ptr, dctl);
      subp = &substitutions[number];
      p = subp->start;
      if (substitution != NULL) *substitution = p;
      /* Rescan the encoding for the entity, outputting the demangled form
         again at the current output position.  Don't record substitutions
         when rescanning. */
      dctl->suppress_substitution_recording++;
      if (type_pass_num == 2 && subp->kind != subk_type) {
        /* If we're expecting a type, and we want the second-pass
           processing, and we get something other than a type, ignore it.
           It's a type specifier, and we don't put those out on the
           second pass. */
      } else {
        switch (subp->kind) {
          case subk_unscoped_template_name:
            if (type_pass_num == 1 || type_pass_num == 0) {
              /* Emit any cv-qualifiers that are applicable to this
                 substitution. */
              output_cv_qualifiers(cv_quals, TRUE, dctl);
            }  /* if */
            (void)demangle_unscoped_name(p, &func_block, dctl);
            break;
          case subk_prefix:
          case subk_template_prefix:
            { a_boolean is_no_return_name, has_templ_arg_list;
              char      ctor_dtor_kind;
              output_cv_qualifiers(cv_quals, TRUE, dctl);
              /* Take the right number of levels of the name.  Note that a
                 substitution counts as one level even if it represents
                 several. */
              if (subp->num_levels > 0) {
                p = demangle_nested_name_components(p,
                                                    subp->num_levels,
                                                    &is_no_return_name,
                                                    &has_templ_arg_list,
                                                    &ctor_dtor_kind,
                                                    last_component_name,
                                                    dctl);
              }  /* if */
              if (subp->kind == subk_template_prefix) {
                /* For the template prefix case, take one more
                   <unqualified-name>. */
                if (subp->num_levels > 0) write_id_str("::", dctl);
                p = demangle_unqualified_name(p, &is_no_return_name, dctl);
              }  /* if */
            }
            break;
          case subk_type:
            if (type_pass_num == 1 || type_pass_num == 0) {
              /* Make sure to use the value of parse_template_args that
                 was in effect when the type substitution was first
                 recorded. */
              (void)demangle_type_first_part(p, cv_quals,
                                             under_lhs_declarator,
                                             need_trailing_space,
                                             subp->parse_template_args,
                                             dctl);
            }  /* if */
            if (type_pass_num == 2 || type_pass_num == 0) {
              demangle_type_second_part(p, cv_quals,
                                        under_lhs_declarator, dctl);
            }  /* if */
            break;
          case subk_template_template_param:
            (void)demangle_template_param(p, dctl);
            break;
          default:
            bad_mangled_name(dctl);
        }  /* switch */
      }  /* if */
      dctl->suppress_substitution_recording--;
    }  /* if */
  }  /* if */
  return ptr;
}  /* demangle_substitution */


/*
Bit mask used to determine which portion(s) of a <bare-function-type> should
be emitted.  The <bare-function-type> contains an optional return type
as well as one or more parameter types.
*/
typedef int a_bare_function_type_option;
#define BFT_NONE	((a_bare_function_type_option)0)
#define BFT_RETURN	((a_bare_function_type_option)0x1)
#define BFT_PARAMS	((a_bare_function_type_option)0x2)


static a_const_char *demangle_bare_function_type(
                                    a_const_char                *ptr,
                                    a_boolean                   no_return_type,
                                    a_bare_function_type_option options,
                                    a_decode_control_block_ptr  dctl)
/*
Demangle an IA-64 <bare-function-type> and output selected pieces of the
demangled form.  Return a pointer to the character position following what was
demangled.  A <bare-function-type> encodes the return and parameter types of a
function type without the surrounding F/E delimiters.  It is used in cases
where the only possibility is a function type, e.g., in a top-level encoding
for a function.  The syntax is:

  <bare-function-type> ::= <signature type>+
        # types are possible return type, then parameter types

That is, the <bare-function-type> is one or more type encodings.
no_return_type is TRUE if the return type is not present in the mangled
encoding.  The pieces of the <bare-function-type> that are emitted are
controlled by the options bit mask; when BFT_RETURN is specified the return
type (and a space character) is emitted, when BFT_PARAMS is specified the
parameter types (with surrounding "( )") are emitted.  In all cases, the
returned value reflects the end position of <bare-function-type> (regardless of
what portion(s) of it were emitted).
*/
{
/* Stop on a (possibly ref-qualified) "E" or end of the input. */
#define end_of_param_list(p) (*(p) == 'E' || *(p) == '\0' || \
                              ((*(p) == 'R' || *(p) == 'O') && \
                               *((p)+1) == 'E'))

  /* Handle the return type first. */
  if ((options & BFT_RETURN) == 0) dctl->suppress_id_output++;
  if (!no_return_type) {
    /* A return type is present and must be scanned. */
    ptr = demangle_type(ptr, dctl);
    write_id_ch(' ', dctl);
  }  /* if */
  if ((options & BFT_RETURN) == 0) dctl->suppress_id_output--;
  /* The remaining portion is the parameter type(s). */
  if ((options & BFT_PARAMS) == 0) dctl->suppress_id_output++;
  write_id_ch('(', dctl);
  if (end_of_param_list(ptr)) {
    /* Error, there are no parameter types (there's supposed to be at
       least a "v" for a void parameter list).  This is likely caused
       by absence of a return type when one is expected. */
    bad_mangled_name(dctl);
  } else if (*ptr == 'v' && end_of_param_list(ptr+1)) {
    /* An empty parameter list is encoded as a single void type.
       Put out just "()" for that case. */
    ptr++;
  } else {
    for (;;) {
      if (*ptr == 'z') {
        /* Encoding for ellipsis. */
        write_id_str("...", dctl);
        ptr++;
        if (!end_of_param_list(ptr)) {
          bad_mangled_name(dctl);
          break;
        }  /* if */
      } else {
        /* Normal type, not an ellipsis. */
        ptr = demangle_type(ptr, dctl);
      }  /* if */
      /* Stop on a (possibly ref-qualified) "E" or at the end of the input. */
      if (end_of_param_list(ptr)) break;
      /* Stop on an error. */
      if (dctl->err_in_id) break;
      /* Continuing, so we need a comma between parameter types. */
      write_id_str(", ", dctl);
    }  /* if */
  }  /* if */
  write_id_ch(')', dctl);
  if ((options & BFT_PARAMS) == 0) dctl->suppress_id_output--;
  return ptr;
#undef end_of_param_list
}  /* demangle_bare_function_type */


static a_const_char *get_cv_qualifiers(a_const_char       *ptr,
                                       a_cv_qualifier_set *cv_quals)
/*
Advance over any cv-qualifiers (const/volatile) at the indicated location
and return in *cv_quals a bit set indicating the qualifiers encountered.
Return a pointer to the character position following what was demangled.
Note that the IA-64 ABI defines a general-purpose "vendor extended type 
qualifier" that is not implemented (as yet).  Certain vendor extended type
qualifiers are however used by the front end to represent C++/CLI declarators;
those are handled in the declarator processing rather than here
(order-insensitive vendor extended type qualifiers should be handled here,
but currently the front end doesn't use any).
*/
{
  *cv_quals = 0;
  for (;; ptr++) {
    if (*ptr == 'K') {
      *cv_quals |= CVQ_CONST;
    } else if (*ptr == 'V') {
      *cv_quals |= CVQ_VOLATILE;
    } else if (*ptr == 'r') {
      *cv_quals |= CVQ_RESTRICT;
    } else {
      break;
    }  /* if */
  }  /* for */
  return ptr;
}  /* get_cv_qualifiers */


static a_const_char *get_ref_qualifier(a_const_char    *ptr,
                                       a_ref_qualifier *ref_qual)
/*
Advance over a ref-qualifier (lvalue/rvalue) at the indicated location
and return in *ref_qual a value indicating the ref-qualifier encountered.
Return a pointer to the character position following what was demangled.
*/
{
  *ref_qual = REFQ_NONE;
  if (*ptr == 'R') {
    *ref_qual = REFQ_LVALUE;
    ptr++;
  } else if (*ptr == 'O') {
    *ref_qual = REFQ_RVALUE;
    ptr++;
  }  /* if */
  return ptr;
}  /* get_ref_qualifier */


static a_boolean is_vendor_extended_declarator(a_const_char *ptr)
/*
Returns TRUE if the location pointed to by ptr contains an EDG-specific vendor
extended type qualifier and the extension is being used as a declarator.
Note that these vendor extended type qualifiers are treated as order-sensitive.
*/
{
  return (start_of_id_is("U8__handle", ptr) ||
          start_of_id_is("U8__trkref", ptr) ||
          start_of_id_is("U14__interior_ptr", ptr) ||
          start_of_id_is("U9__pin_ptr", ptr));
}  /* is_vendor_extended_declarator */


static a_const_char *demangle_vector_size_qualifier(
                                               a_const_char               *ptr,
                                               a_decode_control_block_ptr dctl)
/*
Demangle the GNU vector_size qualifier if it appears at the indicated
location.  Return a pointer to the character position following what was
demangled.
*/
{
  if (start_of_id_is("U8__vector", ptr)) {
    ptr += 10;
    write_id_str("__attribute__((vector_size(?))) ", dctl);
  }  /* for */
  return ptr;
}  /* demangle_vector_size_qualifier */


static void output_cv_qualifiers(a_cv_qualifier_set         cv_quals,
                                 a_boolean                  trailing_space,
                                 a_decode_control_block_ptr dctl)
/*
Output any cv-qualifiers (const/volatile) in the bit set cv_quals.
If trailing_space is TRUE, add a space at the end if any qualifiers were
put out.
*/
{
  a_boolean any_previous = FALSE;

  if (cv_quals & CVQ_CONST   ) {
    write_id_str("const", dctl);
    any_previous = TRUE;
  }  /* if */
  if (cv_quals & CVQ_VOLATILE) {
    if (any_previous) write_id_ch(' ', dctl);
    write_id_str("volatile", dctl);
    any_previous = TRUE;
  }  /* if */
  if (cv_quals & CVQ_RESTRICT) {
    if (any_previous) write_id_ch(' ', dctl);
    write_id_str("restrict", dctl);
    any_previous = TRUE;
  }  /* if */
  if (any_previous && trailing_space) write_id_ch(' ', dctl);
}  /* output_cv_qualifiers */


static void output_ref_qualifier(a_ref_qualifier            ref_qual,
                                 a_decode_control_block_ptr dctl)
/*
Output a ref-qualifier (lvalue/rvalue) if ref_qual indicates there is one.
*/
{
  if (ref_qual == REFQ_LVALUE) {
    write_id_str("&", dctl);
  } else if (ref_qual & REFQ_RVALUE) {
    write_id_str("&&", dctl);
  }  /* if */
}  /* output_ref_qualifier */


static a_const_char *demangle_template_param(a_const_char               *ptr,
                                             a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <template-param> and output the demangled form.  Return
a pointer to the character position following what was demangled.
A <template-param> encodes a reference to a template parameter.
The syntax is:

  <template-param> ::= T_       # first template parameter
                   ::= T <parameter-2 non-negative number> _

*/
{
  long num = 1;
  char buffer[50];

  /* Advance past the "T". */
  ptr++;
  if (*ptr != '_') {
    ptr = get_number(ptr, &num, dctl);
    if (num < 0) {
      bad_mangled_name(dctl);
      num = 0;
    } else {
      num += 2;
    }  /* if */
  }  /* if */
  ptr = advance_past_underscore(ptr, dctl);
  (void)sprintf(buffer, "T%ld", num);
  write_id_str(buffer, dctl);
  return ptr;
}  /* demangle_template_param */


static a_const_char *demangle_parameter_reference(
                                               a_const_char               *ptr,
                                               a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <function-param> and output the demangled form.
Function parameter placeholders are needed for late-specified return types.
Return a pointer to the character position following what was demangled.
A <function-param> encodes a reference to a function parameter.
The syntax is:

  <function-param> ::= fpT                # "this"
                   ::= fp <top-level CV-qualifiers> _
                                          # L == 0, first parameter
                   ::= fp <top-level CV-qualifiers>
                          <parameter-2 non-negative number> _
                                          # L == 0, second and later parameters
                   ::= fL <L-1 non-negative number> p
                          <top-level CV-qualifiers> _         
                                          # L > 0, first parameter
                   ::= fL <L-1 non-negative number> p 
                          <top-level CV-qualifiers>
                          <parameter-2 non-negative number> _   
                                          # L > 0, second and later parameters

*/
{
  long               num = 1, level = -1;
  char               buffer[50];
  a_cv_qualifier_set cv_quals;

  /* Advance past the "f". */
  ptr++;
  if (*ptr == 'L') {
    /* Get the optional level information. */
    ptr++;
    ptr = get_number(ptr, &level, dctl);
    if (level < 0) {
      bad_mangled_name(dctl);
      goto end_of_routine;
    } else {
      level += 1;
    }  /* if */
  }  /* if */
  if (*ptr != 'p') {
    bad_mangled_name(dctl);
    goto end_of_routine;
  }  /* if */
  ptr++;
  if (*ptr == 'T') {
    /* Implicit "this" in trailing return type. */
    ptr++;
    write_id_str("this", dctl);
  } else {
    if (*ptr != '_' && !isdigit((unsigned char)*ptr)) {
      /* Optional cv-qualifiers. */
      ptr = get_cv_qualifiers(ptr, &cv_quals);
      output_cv_qualifiers(cv_quals, /*trailing_space=*/TRUE, dctl);
    }  /* if */
    if (*ptr != '_') {
      /* Parameter number. */
      ptr = get_number(ptr, &num, dctl);
      if (num < 0) {
        bad_mangled_name(dctl);
        goto end_of_routine;
      } else {
        num += 2;
      }  /* if */
    }  /* if */
    ptr = advance_past_underscore(ptr, dctl);
    write_id_str("param#", dctl);
    if (level == -1) {
      (void)sprintf(buffer, "%ld", num);
    } else {
      (void)sprintf(buffer, "%ld[up %ld level%s]", num, level,
                            level > 1 ? "s" : "");
    }  /* if */
    write_id_str(buffer, dctl);
  }  /* if */
end_of_routine:
  return ptr;
}  /* demangle_parameter_reference */


/* Forward reference. */
static a_const_char *demangle_source_name(
                                 a_const_char               *ptr,
                                 a_boolean                  is_module_id,
                                 a_decode_control_block_ptr dctl);


/*
Macro to determine if the character string pointed to by "p" is a
<builtin-type>.  <builtin-type>s are a single lower-case letter or two
characters starting with the character "D".  Exceptions to this rule are
the mangling for decltype (i.e., "DT" and "Dt") as well as the EDG extension
for typeof (i.e., "DY" and "Dy"), __underlying_type (i.e., "Du") and pack
expansions (i.e., "Dp").  The lower case letter "r" is used in <CV-qualifiers>
for "restrict" and is not a <builtin-type>.
*/
#define is_builtin_type(p)                                                \
  ((islower((unsigned char)*(p)) &&                                       \
    *(p) != 'r') ||                                                       \
   (*(p) == 'D' &&                                                        \
    !((p)[1] == 'p' || (p)[1] == 'u' ||                                   \
      (p)[1] == 'T' || (p)[1] == 't' ||                                   \
      (p)[1] == 'Y' || (p)[1] == 'y')))

/*
Macro to determine if the type pointed to by "p" needs a substitution
recorded for it.  <builtin-type>s are not recorded with the exception of
vendor extended types which are recorded.
*/
#define record_substitution_for_type(p) (!(is_builtin_type(p)) || *(p) == 'u')

static a_const_char *demangle_type_specifier(
                                a_const_char               *ptr,
                                a_boolean                  parse_template_args,
                                a_decode_control_block_ptr dctl)
/*
Demangle the type at ptr and output the specifier part.  Return a pointer
to the character position following what was demangled.  The syntax is:

  <type> ::= <builtin-type>
         ::= <class-enum-type>
         ::= <template-param>
         ::= <template-template-param> <template-args>
         ::= Dp <type>          # pack expansion of (C++11)
         ::= Dt <expression> E  # decltype of an id-expression or class member
                                # access (C++11)
         ::= DT <expression> E  # decltype of an expression (C++11)
         ::= Dy <type> E        # typeof(type) (EDG extension)
         ::= DY <expression> E  # typeof(expression) (EDG extension)
         ::= Du <type> E        # __underlying_type(type) (EDG extension)

Other parts of <type> are handled in demangle_type_first_part and
demangle_type_second_part.  In particular, substitutions are handled
at that level.  cv-qualifiers have been handled by the caller.
If parse_template_args is TRUE then any <template-args> in the type should be
parsed as part of the type.  parse_template_args is FALSE when parsing the
<type> of a conversion function operator-name (the <template-args> are
demangled as part of the template function instead).
*/
{
  a_const_char *p = ptr, *s;

  /* Builtin type encodings are typically lower-case (with some exceptions).
     Names begin with a digit or an upper-case letter. */
  if (!is_builtin_type(p)) {
    if (*p == 'T') {
      /* A template parameter, possibly a template template parameter. */
      a_const_char *tstart = p;
      p = demangle_template_param(p, dctl);
      if (*p == 'I' && parse_template_args) {
        /* A <template-args> list. */
        /* Record the template template parameter as a potential
           substitution. */
        record_substitutable_entity(tstart, subk_template_template_param, 0L,
                                    /*parse_template_args=*/FALSE, dctl);
        p = demangle_template_args(p, dctl);
      }  /* if */
    } else if (*p == 'D' && p[1] == 'p') {
      /* A pack expansion. */
      p = full_demangle_type(p+2, /*parse_template_args=*/TRUE,
                             /*is_pack_expansion=*/TRUE, dctl);
    } else if (*p == 'D' && 
               (p[1] == 't' || p[1] == 'T')) {
      /* decltype:
         Dt <expression> E  # decltype of an id-expression or class member
                            # access
         DT <expression> E  # decltype of an expression */
      write_id_str("decltype(", dctl);
      if (p[1] == 't') {
        p = demangle_expression(p+2, dctl);
      } else {
        write_id_ch('(', dctl);
        p = demangle_expression(p+2, dctl);
        write_id_ch(')', dctl);
      }  /* if */
      write_id_ch(')', dctl);
      p = advance_past('E', p, dctl);
    } else if (*p == 'D' &&
               (p[1] == 'y' || p[1] == 'Y')) {
      /* typeof:
         This is an EDG extension to the IA-64 ABI spec to handle GNU typeof
         (and GNU doesn't provide a mangling that we can follow):

            <type> ::= Dy <type> E       # typeof(type)
                   ::= DY <expression> E # typeof(expression)
         */
      write_id_str("typeof(", dctl);
      if (p[1] == 'y') {
        p = demangle_type(p+2, dctl);
      } else {
        p = demangle_expression(p+2, dctl);
      }  /* if */
      write_id_ch(')', dctl);
      p = advance_past('E', p, dctl);
    } else if (*p == 'D' && p[1] == 'u') {
      /* __underlying_type:
         This is an EDG extension to the IA-64 ABI spec to handle
         __underlying_type (a helper function used to implement the C++11
         underlying_type type trait):

            <type> ::= Du <type> E       # __underlying_type(type)
         */
      write_id_str("__underlying_type(", dctl);
      p = demangle_type(p+2, dctl);
      write_id_ch(')', dctl);
      p = advance_past('E', p, dctl);
    } else {
      /* <class-enum-type>, i.e., <name> */
      a_func_block func_block;
      p = demangle_name(p, &func_block, /*options=*/DNO_ALL, dctl);
    }  /* if */
  } else {
    /* Builtin type. */
    switch (*p++) {
      case 'v':
        s = "void";
        break;
      case 'w':
        s = "wchar_t";
        break;
      case 'b':
        s = "bool";
        break;
      case 'c':
        s = "char";
        break;
      case 'a':
        s = "signed char";
        break;
      case 'h':
        s = "unsigned char";
        break;
      case 's':
        s = "short";
        break;
      case 't':
        s = "unsigned short";
        break;
      case 'i':
        s = "int";
        break;
      case 'j':
        s = "unsigned int";
        break;
      case 'l':
        s = "long";
        break;
      case 'm':
        s = "unsigned long";
        break;
      case 'x':
        s = "long long";
        break;
      case 'y':
        s = "unsigned long long";
        break;
      case 'n':
        s = "__int128";
        break;
      case 'o':
        s = "unsigned __int128";
        break;
      case 'f':
        s = "float";
        break;
      case 'd':
        s = "double";
        break;
      case 'e':
        /* Also __float80 in some configurations. */
        s = "long double";
        break;
      case 'g':
        s = "__float128";
        break;
      case 'u':
        /* A vendor extended type is specified as:
           <builtin-type> ::= u <source-name> . */
        p = demangle_source_name(p, /*is_module_id=*/FALSE, dctl);
        s = "";
        break;
      case 'D':
        /* Additional built-in types (too many to assign a single character
           to each one). */
        switch (*p++) {
          case 'a':
            s = "auto";
            break;
          case 'c':
            s = "decltype(auto)";
            break;
          case 'n':
            s = "std::nullptr_t";
            break;
          case 'N':
            /* EDG extension for C++/CLI managed __nullptr. */
            s = "__nullptr";
            break;
          case 's':
            s = "char16_t";
            break;
          case 'i':
            s = "char32_t";
            break;
          case 'v':
            /* Vector type.  Format is: "Dv<number>_<type>".  Scan the type
               portion twice in order to get the proper "vector_size" value
               (by "multiplying" by sizeof(type) -- since the demangling
               doesn't know the size of types). */
            { long num;
              a_const_char *typep;
              p = get_number(p, &num, dctl);
              if (*p != '_') {
                bad_mangled_name(dctl);
              } else {
                p++;
                typep = p;
                p = demangle_type(p, dctl);
                write_id_str(" __attribute((vector_size(", dctl);
                write_id_signed_number(num, dctl);
                write_id_str("*sizeof(", dctl);
                dctl->suppress_substitution_recording++;
                (void)demangle_type(typep, dctl);
                dctl->suppress_substitution_recording--;
                write_id_str(")))) ", dctl);
              }  /* if */
              s = "";
            }
            break;
          default:
            bad_mangled_name(dctl);
            s = "";
        }  /* switch */
        break;
      case 'z':  /* Ellipsis not handled here;
                    see demangle_bare_function_type. */
      default:
        bad_mangled_name(dctl);
        s = "";
    }  /* switch */
    write_id_str(s, dctl);
  }  /* if */
  return p;
}  /* demangle_type_specifier */


static a_const_char *skip_extern_C_indication(a_const_char *ptr)
/*
ptr points to the character after the "F" of a function type.  Skip over
and ignore an indication of extern "C" following the "F", if one is present.
Return a pointer to the character following the extern "C" indication.
There's no syntax for representing the extern "C" in the function type, so
just ignore it.
*/
{
  if (*ptr == 'Y') ptr++;
  return ptr;
}  /* skip_extern_C_indication */


static a_const_char *demangle_type_first_part(
                               a_const_char               *ptr,
                               a_cv_qualifier_set         cv_quals,
                               a_boolean                  under_lhs_declarator,
                               a_boolean                  need_trailing_space,
                               a_boolean                  parse_template_args,
                               a_decode_control_block_ptr dctl)
/*
Demangle the type at ptr and output the specifier part and the part of the
declarator that precedes the name.  Return a pointer to the character
position following what was demangled.  If under_lhs_declarator is TRUE,
this type is directly under a type that uses a left-side declarator,
e.g., a pointer type.  (That's used to control use of parentheses around
parts of the declarator.)  If need_trailing_space is TRUE, put a space
at the end of the specifiers part (needed if the declarator part is
not empty, because it contains a name or a derived type).  cv_quals
indicates any previously-scanned cv-qualifiers that are to be considered
to be on top of the type.  If parse_template_args is TRUE then any
<template-args> in the type should be parsed as part of the type.
*/
{
  a_const_char       *p = ptr, *qualp = p, *unqualp;
  char               kind;
  a_cv_qualifier_set local_cv_quals;
  a_boolean          record_substitution = TRUE;
  a_boolean          record_cv_qual_substitution = TRUE;

  /* Accumulate cv-qualifiers. */
  p = get_cv_qualifiers(p, &local_cv_quals);
  cv_quals |= local_cv_quals;
  unqualp = p;
  kind = *p;
  if (kind == 'S' &&
      /* "St" for "std::" is the beginning of a name, not a type. */
      p[1] != 't') {
    /* A substitution. */
    p = demangle_substitution(p, 1, cv_quals,
                              under_lhs_declarator,
                              need_trailing_space,
                              (a_const_char **)NULL,
                              (a_const_char **)NULL,
                              dctl);
    record_substitution = FALSE;
    if (*p == 'I') {
      /* A <template-args> list (the substitution must be a template). */
      p = demangle_template_args(p, dctl);
      record_substitution = TRUE;
    }  /* if */
  } else if (kind == 'P' || kind == 'R' || kind == 'O' || kind == 'C' ||
             (kind == 'U' && is_vendor_extended_declarator(p))) {
    a_const_char *vendor_ext = NULL;
    a_boolean    need_space = TRUE;
    /* Look for type qualifiers:
        <type> ::= <CV-qualifiers> <type>
               ::= P <type> # pointer-to
               ::= R <type> # reference-to
               ::= O <type> # rvalue reference-to (C++11)
               ::= C <type> # complex pair (C 2000)
               ::= U <source-name> <type> # vendor extended type qualifier
       */
    p++;
    if (kind == 'U') {
      /* This is a vendor extended type qualifier that is being used by the
         front end to encode C++/CLI pointer-like types (i.e., handles,
         tracking references, interior_ptrs, and pin_ptrs).  This avoids adding
         EDG-specific manglings for these entities that might be used in
         subsequent IA-64 ABI revisions.  Note that these extensions are
         treated as "order-sensitive" for the purposes of substitutions. */
      long num;
      if (start_of_id_is("8__handle", p)) {
        vendor_ext = "^";
      } else if (start_of_id_is("8__trkref", p)) {
        vendor_ext = "%";
      } else if (start_of_id_is("14__interior_ptr", p)) {
        write_id_str("interior_ptr<", dctl);
        vendor_ext = ">";
        need_space = FALSE;
      } else if (start_of_id_is("9__pin_ptr", p)) {
        write_id_str("pin_ptr<", dctl);
        vendor_ext = ">";
        need_space = FALSE;
      } else {
        bad_mangled_name(dctl);
      }  /* if */
      /* Advance past the vendor string. */
      p = get_number(p, &num, dctl);
      p += num;
    }  /* if */
    if (kind == 'C') {
      write_id_str("_Complex ", dctl);
    }  /* if */
    p = demangle_type_first_part(p, CVQ_NONE, /*under_lhs_declarator=*/TRUE,
                                 need_space, parse_template_args, dctl);
    if (kind == 'P') {
      write_id_ch('*', dctl);
    } else if (kind == 'R') {
      write_id_ch('&', dctl);
    } else if (kind == 'O') {
      write_id_str("&&", dctl);
    } else if (vendor_ext != NULL) {
      write_id_str(vendor_ext, dctl);
    }  /* if */
    /* Output the cv-qualifiers on the pointer, if any. */
    output_cv_qualifiers(cv_quals, /*trailing_space=*/TRUE, dctl);
  } else if (kind == 'M') {
    /* Pointer-to-member type, M <class type> <member type>. */
    a_const_char *classp = p+1;
    /* Skip over the class name. */
    /* Substitutions do get recorded on this scan. */
    dctl->suppress_id_output++;
    p = demangle_type(classp, dctl);
    dctl->suppress_id_output--;
    p = demangle_type_first_part(p, CVQ_NONE, /*under_lhs_declarator=*/TRUE,
                                 /*need_trailing_space=*/TRUE, 
                                 parse_template_args, dctl);
    /* Output Classname::*. */
    dctl->suppress_substitution_recording++;
    (void)demangle_type(classp, dctl);
    dctl->suppress_substitution_recording--;
    write_id_str("::*", dctl);
    /* Output the cv-qualifiers on the pointer, if any. */
    output_cv_qualifiers(cv_quals, /*trailing_space=*/TRUE, dctl);
  } else if (kind == 'F' ||
             (kind == 'D' &&
              (p[1] == 'o' || p[1] == 'O'))) {
    a_ref_qualifier dummy;
    /* Function type, <exc-spec> F [Y] <bare-function-type> [<ref-qualifier>] E
       where "Y" indicates extern "C" (and is ignored here).  An <exc-spec>
       looks like: [Do | DO <expression> E | Dw <type>* E ].  The front end
       doesn't implement the "Dw" mangling, so the demangling is omitted. */
    if (kind == 'D') {
      /* An exception specification is present (skip it here). */
      switch (p[1]) {
        case 'o':
          p += 2;
          break;
        case 'O':
          dctl->suppress_id_output++;
          p = demangle_expression(p+2, dctl);
          dctl->suppress_id_output--;
          p = advance_past('E', p, dctl);
          break;
        default:
          bad_mangled_name(dctl);
          break;
      }  /* switch */
    }  /* if */
    p = skip_extern_C_indication(p+1);
    /* Output the return type. */
    p = demangle_type_first_part(p, CVQ_NONE, /*under_lhs_declarator=*/FALSE,
                                 /*need_trailing_space=*/TRUE, 
                                 parse_template_args, dctl);
    /* Skip over the parameter types without outputting anything. */
    /* Substitutions do get recorded on this scan. */
    p = demangle_bare_function_type(p, /*no_return_type=*/TRUE, BFT_NONE,
                                    dctl);
    /* Look for an optional ref-qualifier (and skip it in this pass). */
    p = get_ref_qualifier(p, &dummy);
    p = advance_past('E', p, dctl);
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) write_id_ch('(', dctl);
    /* Cv-qualifiers that are applied to function types are considered an
       indivisible portion of the type and no substitution is made for the
       unqualified type. */
    record_cv_qual_substitution = FALSE;
  } else if (kind == 'A') {
    /* Array type,
         A <positive dimension number> _ <element type>
         A [ <dimension expression> ]  _ <element type>
    */
    p++;
    if (!isdigit((unsigned char)*p)) {
      if (*p != '_') {
        /* Length is specified by an expression based on template
           parameters.  Ignore the expression. */
        /* Substitutions do get recorded on this scan. */
        dctl->suppress_id_output++;
        p = demangle_expression(p, dctl);
        dctl->suppress_id_output--;
      }  /* if */
    } else {
      /* Normal constant number of elements. */
      /* Skip the array size. */
      while (isdigit((unsigned char)*p)) p++;
    }  /* if */
    p = advance_past_underscore(p, dctl);
    /* Process the element type. */
    p = demangle_type_first_part(p, CVQ_NONE, /*under_lhs_declarator=*/FALSE,
                                 /*need_trailing_space=*/TRUE, 
                                 parse_template_args, dctl);
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) write_id_ch('(', dctl);
  } else {
    /* No declarator part to process.  Handle the specifier type. */
    output_cv_qualifiers(cv_quals, /*trailing_space=*/TRUE, dctl);
    p = demangle_vector_size_qualifier(p, dctl);
    p = demangle_type_specifier(p, parse_template_args, dctl);
    if (need_trailing_space) write_id_ch(' ', dctl);
    if (!record_substitution_for_type(unqualp)) {
      /* Do not record a substitution for (most) <builtin-type>s. */
      record_substitution = FALSE;
    }  /* if */
  }  /* if */
  if (record_substitution) {
    /* Record the non-cv-qualified version of the type as a potential
       substitution. */
    record_substitutable_entity(unqualp, subk_type, 0L, parse_template_args,
                                dctl);
  }  /* if */
  if (qualp != unqualp && record_cv_qual_substitution) {
    /* The type is cv-qualified, so record another potential substitution
       for the fully-qualified type. */
    record_substitutable_entity(qualp, subk_type, 0L, parse_template_args,
                                dctl);
  }  /* if */
  return p;
}  /* demangle_type_first_part */


static void demangle_type_second_part(
                               a_const_char               *ptr,
                               a_cv_qualifier_set         cv_quals,
                               a_boolean                  under_lhs_declarator,
                               a_decode_control_block_ptr dctl)
/*
Demangle the type at ptr and output the part of the declarator that follows the
name.  This routine does not return a pointer to the character position
following what was demangled; it is assumed that the caller will save
that from the call of demangle_type_first_part, and it saves a lot of
time if this routine can avoid scanning the specifiers again.
If under_lhs_declarator is TRUE, this type is directly under a type that
uses a left-side declarator, e.g., a pointer type.  (That's used to control
use of parentheses around parts of the declarator.)  cv_quals
indicates any previously-scanned cv-qualifiers that are to considered
to be on top of the type.
*/
{
  a_const_char       *p = ptr;
  char               kind;
  a_cv_qualifier_set local_cv_quals;

  /* Accumulate cv-qualifiers. */
  p = get_cv_qualifiers(p, &local_cv_quals);
  cv_quals |= local_cv_quals;
  kind = *p;
  if (kind == 'S' &&
      /* "St" for "std::" is the beginning of a name, not a type. */
      p[1] != 't') {
    /* A substitution. */
    p = demangle_substitution(p, 2, cv_quals,
                              under_lhs_declarator,
                              /*need_trailing_space=*/FALSE,
                              (a_const_char **)NULL,
                              (a_const_char **)NULL,
                              dctl);
    /* No need to scan the <template-args> list if there is one -- 
       that was done by demangle_type_first_part. */
  } else if (kind == 'P' || kind == 'R' || kind == 'O' || kind == 'C' ||
             (kind == 'U' && is_vendor_extended_declarator(p))) {
    /* Look for type qualifiers:
        <type> ::= <CV-qualifiers> <type>
               ::= P <type> # pointer-to
               ::= R <type> # reference-to
               ::= O <type> # rvalue reference-to (C++11)
               ::= C <type> # complex pair (C 2000)
               ::= U <source-name> <type> # vendor extended type qualifier
       */
    p++;
    if (kind == 'U') {
      /* This is a vendor extended type qualifier that is being used
         by the front end as a declarator; skip over it. */
      dctl->suppress_id_output++;
      p = demangle_source_name(p, /*is_module_id=*/FALSE, dctl);
      dctl->suppress_id_output--;
    }  /* if */
    demangle_type_second_part(p, CVQ_NONE, /*under_lhs_declarator=*/TRUE,
                              dctl);
  } else if (kind == 'M') {
    /* Pointer-to-member type, M <class type> <member type>. */
    /* Advance over the class name. */
    dctl->suppress_id_output++;
    dctl->suppress_substitution_recording++;
    p = demangle_type(p+1, dctl);
    dctl->suppress_substitution_recording--;
    dctl->suppress_id_output--;
    demangle_type_second_part(p, CVQ_NONE, /*under_lhs_declarator=*/TRUE,
                              dctl);
  } else if (kind == 'F' ||
             (kind == 'D' &&
              (p[1] == 'o' || p[1] == 'O' || p[1] == 'w'))) {
    a_const_char *returnt, *exception_spec = NULL;
    a_const_char *save_exception_expr = NULL;
    a_ref_qualifier ref_qual;
    /* Function type, <exc-spec> F [Y] <bare-function-type> [<ref-qualifier>] E
       where "Y" indicates extern "C" (and is ignored here).  An <exc-spec>
       looks like: [Do | DO <expression> E | Dw <type>* E ].  The front end
       doesn't implement the "Dw" mangling, so the demangling is omitted. */
    if (kind == 'D') {
      /* An exception specification is present. */
      switch (p[1]) {
        case 'o':
          exception_spec = " noexcept";
          p += 2;
          break;
        case 'O':
          /* Emit the expression later (save a pointer to it), but skip it
             now. */
          save_exception_expr = p + 2;
          dctl->suppress_id_output++;
          p = demangle_expression(save_exception_expr, dctl);
          dctl->suppress_id_output--;
          p = advance_past('E', p, dctl);
          break;
        default:
          bad_mangled_name(dctl);
          break;
      }  /* switch */
    }  /* if */
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) write_id_ch(')', dctl);
    p = skip_extern_C_indication(p+1);
    /* Put out the parameter types (the return type is skipped and not
       output). */
    returnt = p;
    dctl->suppress_substitution_recording++;
    p = demangle_bare_function_type(p, /*no_return_type=*/FALSE, BFT_PARAMS,
                                    dctl);
    dctl->suppress_substitution_recording--;
    /* Get a ref-qualifier if there is one. */
    p = get_ref_qualifier(p, &ref_qual);
    p = advance_past('E', p, dctl);
    /* Put out any cv-qualifiers (member functions). */
    /* Note that such things could come up on nonmember functions in the
       presence of typedefs.  In such a case what we generate here will not
       be valid C, but it's a reasonable representation of the mangled
       type, and there's no way of getting the typedef name in there,
       so let it be. */
    if (cv_quals != 0) {
      write_id_ch(' ', dctl);
      output_cv_qualifiers(cv_quals, /*trailing_space=*/FALSE, dctl);
    }  /* if */
    if (ref_qual != REFQ_NONE) {
      /* Output a ref-qualifier, if any. */
      write_id_ch(' ', dctl);
      output_ref_qualifier(ref_qual, dctl);
    }  /* if */
    /* Output the return type. */
    demangle_type_second_part(returnt, CVQ_NONE,
                              /*under_lhs_declarator=*/FALSE, dctl);
    if (exception_spec != NULL) {
      write_id_str(exception_spec, dctl);
    } else if (save_exception_expr != NULL) {
      write_id_str(" noexcept(", dctl);
      (void)demangle_expression(save_exception_expr, dctl);
      write_id_ch(')', dctl);
    }  /* if */
  } else if (kind == 'A') {
    /* Array type,
         A <positive dimension number> _ <element type>
         A [ <dimension expression> ]  _ <element type>
    */
    /* This is a right-side declarator, so if it's under a left-side declarator
       parentheses are needed. */
    if (under_lhs_declarator) write_id_ch(')', dctl);
    write_id_ch('[', dctl);
    p++;
    if (!isdigit((unsigned char)*p)) {
      if (*p != '_') {
        /* Length is specified by a constant expression based on template
           parameters. */
        dctl->suppress_substitution_recording++;
        p = demangle_expression(p, dctl);
        dctl->suppress_substitution_recording--;
      }  /* if */
    } else {
      /* Normal constant number of elements. */
      /* Put out the array size. */
      while (isdigit((unsigned char)*p)) write_id_ch(*p++, dctl);
    }  /* if */
    p = advance_past_underscore(p, dctl);
    write_id_ch(']', dctl);
    /* Process the element type. */
    demangle_type_second_part(p, CVQ_NONE, /*under_lhs_declarator=*/FALSE,
                              dctl);
  } else {
    /* No declarator part to process.  No need to scan the specifiers type --
       it was done by demangle_type_first_part. */
  }  /* if */
}  /* demangle_type_second_part */


static a_const_char *full_demangle_type(
                                a_const_char               *ptr,
                                a_boolean                  parse_template_args,
                                a_boolean                  is_pack_expansion,
                                a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <type> and output the demangled form.  Return a pointer
to the character position following what was demangled.  A <type> encodes
a type.  The syntax is:

  <type> ::= <builtin-type>
         ::= <function-type>
         ::= <class-enum-type>
         ::= <array-type>
         ::= <pointer-to-member-type>
         ::= <template-param>
         ::= <template-template-param> <template-args>
         ::= <substitution>
         ::= <CV-qualifiers> <type>
         ::= P <type>   # pointer-to
         ::= R <type>   # reference-to
  <function-type> ::= F [Y] <bare-function-type> E
  <class-enum-type> ::= <name>
  <array-type> ::= A <positive dimension number> _ <element type>
               ::= A [<dimension expression>] _ <element type>
  <pointer-to-member-type> ::= M <class type> <member type>

If parse_template_args is TRUE then any <template-args> in the type should be
parsed as part of the type.  When is_pack_expansion is TRUE, emit an
indication that the type is a pack expansion.
*/
{
  a_const_char *p;

  /* Generate the specifier part of the type. */
  p = demangle_type_first_part(ptr, CVQ_NONE, /*under_lhs_declarator=*/FALSE,
                               /*need_trailing_space=*/FALSE, 
                               parse_template_args, dctl);
  if (is_pack_expansion) {
    /* Emit the pack expansion indication between processing the two type
       parts so that function and pointer to member function types are handled
       properly. */
    write_id_str("...", dctl);
  }  /* if */
  /* Generate the declarator part of the type. */
  demangle_type_second_part(ptr, CVQ_NONE, /*under_lhs_declarator=*/FALSE,
                            dctl);
  return p;
}  /* full_demangle_type */


static a_const_char *get_operator_name(
                                      a_const_char               *ptr,
                                      int                        *num_operands,
                                      int                        *length,
                                      a_const_char               **close_str,
                                      a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <operator-name> and return the demangled form.
Return NULL if the operator is invalid.  An <operator-name> encodes
an operator in an expression or operator function name.
*num_operands is set to the number of operands expected by the operator.
*length is set to the mangled name length (2 except for vendor extended
operators).  *close_str is set to a string that closes the operator,
if necessary, e.g., "]" for subscripting; it is set to "" if not needed.
Note that for the literal operator case (i.e., for the "li" mangled operator),
the string that is returned is in a temporary buffer (and a subsequent call
with another "li" mangled name will overwrite it), so the string should
be copied quickly.
*/
{
  a_const_char *str = NULL;

  *num_operands = 2;
  *close_str = "";
  *length = 0;
  if (*ptr == '\0') {
    bad_mangled_name(dctl);
  } else {
    char ch2 = ptr[1];
    switch (*ptr) {
      case 'a':
        if (ch2 == 'a') {
          str = "&&";
        } else if (ch2 == 'd') {
          str = "&";
          *num_operands = 1;
        } else if (ch2 == 'n') {
          str = "&";
        } else if (ch2 == 'N') {
          str = "&=";
        } else if (ch2 == 'S') {
          str = "=";
        } else if (ch2 == 't') {
          /* alignof(type) -- newer mangling form */
          str = "alignof(";
          *num_operands = 0;
          *close_str = ")";
        } else if (ch2 == 'w') {
          /* co_await */
          str = "co_await";
          *num_operands = 1;
        } else if (ch2 == 'z') {
          /* alignof(expression) -- newer mangling form */
          str = "alignof(";
          *close_str = ")";
          *num_operands = 1;
        }  /* if */
        break;
      case 'c':
        if (ch2 == 'c') {
          str = "const_cast";
          *num_operands = 1;
        } else if (ch2 == 'l') {
          str = "()";
          *num_operands = 0;  /* Call is variable-length. */
        } else if (ch2 == 'm') {
          str = ",";
        } else if (ch2 == 'o') {
          str = "~";
          *num_operands = 1;
        } else if (ch2 == 'v') {
          str = "cast";
          *num_operands = 1;
        }  /* if */
        break;
      case 'd':
        if (ch2 == 'a') {
          str = "delete[] ";
          *num_operands = 1;
        } else if (ch2 == 'c') {
          str = "dynamic_cast";
          *num_operands = 1;
        } else if (ch2 == 'e') {
          str = "*";
          *num_operands = 1;
        } else if (ch2 == 'l') {
          str = "delete ";
          *num_operands = 1;
        } else if (ch2 == 's') {
          str = ".*";
        } else if (ch2 == 'v') {
          str = "/";
        } else if (ch2 == 'V') {
          str = "/=";
        }  /* if */
        break;
      case 'e':
        if (ch2 == 'o') {
          str = "^";
        } else if (ch2 == 'O') {
          str = "^=";
        } else if (ch2 == 'q') {
          str = "==";
        }  /* if */
        break;
      case 'g':
        if (ch2 == 'e') {
          str = ">=";
        } else if (ch2 == 't') {
          str = ">";
        }  /* if */
        break;
      case 'i':
        if (ch2 == 'x') {
          str = "[";
          *close_str = "]";
        }  /* if */
        break;
      case 'l':
        if (ch2 == 'e') {
          str = "<=";
        } else if (ch2 == 'i') {
          /* A literal operator is followed by a ud-suffix (with a pre-pended
             length).  Copy the ud-suffix into a buffer following the ""
             for the literal operator (we need to concatenate '"" ' and
             the ud-suffix). */
          long          ud_suffix_len;
          a_const_char  *ud_suffix_ptr;
          ud_suffix_ptr = get_number(ptr+2, &ud_suffix_len, dctl);
          /* Note that a literal operator can have zero, one, or two operands,
             but we can't tell which case that is here, so consume zero
             operands and let the operands be parsed by the "cl" mangling
             for the implicit call to the literal operator. */
          *num_operands = 0;
          str = NULL;
          if (!dctl->err_in_id) {
            if (ud_suffix_len <= 0) {
              bad_mangled_name(dctl);
            } else {
              if (ud_suffix_buffer == NULL) {
                ud_suffix_buffer_length = 128;
                ud_suffix_buffer = (char *)malloc(
                                         (true_size_t)ud_suffix_buffer_length);
              } else if (ud_suffix_len + 3 + 1 > ud_suffix_buffer_length) {
                ud_suffix_buffer_length = ud_suffix_len + 3 + 1;
                ud_suffix_buffer = (char *)realloc(ud_suffix_buffer,
                                         (true_size_t)ud_suffix_buffer_length);
              }  /* if */
              if (ud_suffix_buffer != NULL) {
                if (strlen(ud_suffix_ptr) < ud_suffix_len) {
                  bad_mangled_name(dctl);
                } else {
                  strcpy(ud_suffix_buffer, "\"\"");
                  strncpy(&ud_suffix_buffer[2], ud_suffix_ptr, ud_suffix_len);
                  ud_suffix_buffer[2 + ud_suffix_len] = '\0';
                  str = (a_const_char *)ud_suffix_buffer;
                  *length = ud_suffix_ptr + ud_suffix_len - ptr;
                }  /* if */
              } else {
                bad_mangled_name(dctl);
              }  /* if */
            }  /* if */
          }  /* if */
        } else if (ch2 == 's') {
          str = "<<";
        } else if (ch2 == 'S') {
          str = "<<=";
        } else if (ch2 == 't') {
          str = "<";
        }  /* if */
        break;
      case 'm':
        if (ch2 == 'i') {
          str = "-";
        } else if (ch2 == 'I') {
          str = "-=";
        } else if (ch2 == 'l') {
          str = "*";
        } else if (ch2 == 'L') {
          str = "*=";
        } else if (ch2 == 'm') {
          str = "--";
          *num_operands = 1;
        }  /* if */
        break;
      case 'n':
        if (ch2 == 'a') {
          str = "new[] ";
        } else if (ch2 == 'e') {
          str = "!=";
        } else if (ch2 == 'g') {
          str = "-";
          *num_operands = 1;
        } else if (ch2 == 't') {
          str = "!";
          *num_operands = 1;
        } else if (ch2 == 'w') {
          str = "new ";
        } else if (ch2 == 'x') {
          str = "noexcept(";
          *close_str = ")";
          *num_operands = 1;
        }  /* if */
        break;
      case 'o':
        if (ch2 == 'o') {
          str = "||";
        } else if (ch2 == 'r') {
          str = "|";
        } else if (ch2 == 'R') {
          str = "|=";
        }  /* if */
        break;
      case 'p':
        if (ch2 == 'l') {
          str = "+";
        } else if (ch2 == 'L') {
          str = "+=";
        } else if (ch2 == 'm') {
          str = "->*";
        } else if (ch2 == 'p') {
          str = "++";
          *num_operands = 1;
        } else if (ch2 == 's') {
          str = "+";
          *num_operands = 1;
        } else if (ch2 == 't') {
          str = "->";
        }  /* if */
        break;
      case 'q':
        if (ch2 == 'u') {
          str = "?";
          *num_operands = 3;
        }  /* if */
        break;
      case 'r':
        if (ch2 == 'c') {
          str = "reinterpret_cast";
          *num_operands = 1;
        } else if (ch2 == 'm') {
          str = "%";
        } else if (ch2 == 'M') {
          str = "%=";
        } else if (ch2 == 's') {
          str = ">>";
        } else if (ch2 == 'S') {
          str = ">>=";
        }  /* if */
        break;
      case 's':
        if (ch2 == 'c') {
          str = "static_cast";
          *num_operands = 1;
        } else if (ch2 == 't') {
          /* sizeof(type) */
          str = "sizeof(";
          *num_operands = 0;
          *close_str = ")";
        } else if (ch2 == 'z') {
          /* sizeof(expression) */
          str = "sizeof(";
          *close_str = ")";
          *num_operands = 1;
        }  /* if */
        break;
      case 't':
        if (ch2 == 'e') {
          /* typeid(expression) -- newer mangling form */
          str = "typeid(";
          *close_str = ")";
          *num_operands = 1;
        } else if (ch2 == 'i') {
          /* typeid(type) -- newer mangling form */
          str = "typeid(";
          *close_str = ")";
          *num_operands = 0;
        } else if (ch2 == 'r') {
          /* rethrow (no arguments) */
          str = "throw";
          *num_operands = 0;
        } else if (ch2 == 'w') {
          /* throw (expression) */
          str = "throw ";
          *num_operands = 1;
        }  /* if */
        break;
      case 'v':
        /* Vendor extended operators. */
        if (start_of_id_is("v18alignofe", ptr)) {
          /* __alignof__(expr) -- older mangling form */
          str = "__alignof__(";
          *close_str = ")";
          *num_operands = 1;
          *length = 11;
        } else if (start_of_id_is("v17alignof", ptr)) {
          /* __alignof__(type) -- older mangling form */
          str = "__alignof__(";
          *close_str = ")";
          *num_operands = 0;
          *length = 10;
        } else if (start_of_id_is("v19__uuidofe", ptr)) {
          /* __uuidof(expr) */
          str = "__uuidof(";
          *close_str = ")";
          *num_operands = 1;
          *length = 12;
        } else if (start_of_id_is("v18__uuidof", ptr)) {
          /* __uuidof(type) */
          str = "__uuidof(";
          *close_str = ")";
          *num_operands = 0;
          *length = 11;
        } else if (start_of_id_is("v17typeide", ptr)) {
          /* typeid(expr) -- older mangling form */
          str = "typeid(";
          *close_str = ")";
          *num_operands = 1;
          *length = 10;
        } else if (start_of_id_is("v16typeid", ptr)) {
          /* typeid(type) -- older mangling form */
          str = "typeid(";
          *close_str = ")";
          *num_operands = 0;
          *length = 9;
        } else if (start_of_id_is("v19clitypeid", ptr)) {
          /* C++/CLI T::typeid. */
          str = "::typeid";
          *num_operands = 0;
          *length = 12;
        } else if (start_of_id_is("v23min", ptr)) {
          /* GNU "<?" */
          str = "<?";
          *length = 6;
          *num_operands = 2;
        } else if (start_of_id_is("v23max", ptr)) {
          /* GNU ">?" */
          str = ">?";
          *length = 6;
          *num_operands = 2;
        } else if (start_of_id_is("v18__real__", ptr)) {
          /* __real(expr) */
          str = "__real(";
          *close_str = ")";
          *length = 11;
          *num_operands = 1;
        } else if (start_of_id_is("v18__imag__", ptr)) {
          /* __imag(expr) */
          str = "__imag(";
          *close_str = ")";
          *length = 11;
          *num_operands = 1;
        } else if (start_of_id_is("v19clihandle", ptr)) {
          /* C++/CLI handle-to */
          str = "%";
          *length = 12;
          *num_operands = 1;
        } else if (start_of_id_is("v112clisafe_cast", ptr)) {
          /* C++/CLI safe_cast<T>() */
          str = "safe_cast";
          *length = 16;
          *num_operands = 1;
        } else if (start_of_id_is("9builtin", ptr+2)) {
          /* Builtin operation.  Name is
               vN9builtinXX
                         ^^-- Operation number
                ^------------ Number of operands (<= 9)
          */
          static char builtin_name[] = "builtin-operation-XX";
          str = builtin_name;
          builtin_name[18] = ptr[10];
          builtin_name[19] = ptr[11];
          *length = 12;
          *num_operands = ptr[1]-'0';
        } else if (start_of_id_is("12clisubscript", ptr+2) &&
                   ptr[1] >= '0' && ptr[1] <= '9') {
          /* C++/CLI subscript operation with variable number of operands
             (<= 9).  The caller handles this as a special case. */
          str = "subscript";
          *length = 16;
          *num_operands = ptr[1]-'0';
        }  /* if */
        break;
      default:
        break;
    }  /* switch */
    if (*length == 0) *length = 2;
  }  /* if */
  return str;
}  /* get_operator_name */


static a_const_char *demangle_source_name(
                                 a_const_char               *ptr,
                                 a_boolean                  is_module_id,
                                 a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <source-name> and output the demangled form.
Return a pointer to the character position following what was demangled.
A <source-name> encodes an unqualified name as a length plus the
characters of the name. The syntax is:

    <source-name> ::= <positive length number> <identifier>
    <identifier> ::= <unqualified source code identifier>

If is_module_id is TRUE, the identifier is a module id string, which
begins with a (second) count that gives the length of the file name
part.  Just put out the file name part (continue scanning, but do not
output the rest of the string).  This is used for an EDG extension.
*/
{
  long      num;
  a_boolean output_chars = TRUE;

  ptr = get_number(ptr, &num, dctl);
  if (num <= 0) {
    bad_mangled_name(dctl);
  } else if (is_module_id) {
    /* A module id name (an EDG extension), which has the form
         <length> _ <file-name-length> _ <file-name> <rest-of-module-id>
       Only the file name part is put out. */
    ptr = demangle_module_id(ptr, (unsigned long)num, (char *)NULL, dctl);
  } else if (num >= 9 && start_of_id_is("_INTERNAL", ptr)) {
    /* An EDG extension to individuate certain entities so they don't
       collide with similarly named (or unnamed) entities in other
       translation units. */
    write_id_str("[local to ", dctl);
    ptr = demangle_module_id(ptr+9, (unsigned long)num-9, ptr, dctl);
    write_id_str("]", dctl);
  } else {
    if (num >= 11 && start_of_id_is("_GLOBAL__N_", ptr)) {
      /* g++ uses names beginning with "_GLOBAL__N_" to identify unnamed
         namespaces, and the EDG C++ Front End does also to be compatible
         with that. */
      write_id_str("<unnamed>", dctl);
      output_chars = FALSE;
    }  /* if */
    for (; num > 0; ptr++, num--) {
      if (*ptr == '\0') {
        /* The name string ends before enough characters have been
           accumulated. */
        bad_mangled_name(dctl);
        break;
      } else if (!isalnum((unsigned char)*ptr) && *ptr != '_' && *ptr != '$') {
        /* Invalid character in identifier. */
        /* g++ names for unnamed namespaces contain bad characters,
           e.g., periods. */
        if (output_chars) {
          bad_mangled_name(dctl);
          break;
        }  /* if */
      } else if (output_chars) {
        write_id_ch(*ptr, dctl);
      }  /* if */
    }  /* for */
  }  /* if */
  return ptr;
}  /* demangle_source_name */


static a_const_char *get_instance_number(a_const_char               *p,
                                         unsigned long              *instance,
                                         a_decode_control_block_ptr dctl)
/*
An underscore optionally preceded by a non-negative instance number is
expected at *p.  Advance past the underscore.  Return the instance number
(non-negative number plus two -- or one if no number is present) in *instance.
*/
{
  *instance = 1;
  if (isdigit((unsigned char)*p)) {
    long num;
    p = get_number(p, &num, dctl);
    if (num < 0) {
      bad_mangled_name(dctl);
    } else {
      *instance = num+2;
    }  /* if */
  }  /* if */
  if (*p == '_') {
    p += 1;
  } else {
    bad_mangled_name(dctl);
  }  /* if */
  return p;
}  /* get_instance_number */


static a_const_char *demangle_unnamed_type(a_const_char               *ptr,
                                           a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <unnamed-type-name>.  Return a pointer to the character
position following what was demangled.

  <unnamed-type-name> ::= Ut [ <nonnegative number> ] _ 
                      ::= <closure-type-name>
  <closure-type-name> ::= Ul <lambda-sig> E [ <nonnegative number> ] _ 
  <lambda-sig> ::= <parameter type>+  
                      # Parameter types or "v" if the lambda has no parameters

                            
*/
{
  unsigned long instance;

  if (*ptr == 'U' && ptr[1] == 't') {
    /* An unnamed type has an optional instance number followed by an
       underscore. */
    ptr = get_instance_number(ptr+2, &instance, dctl);
    if (!dctl->err_in_id) {
      write_id_str("[unnamed type (instance ", dctl);
      write_id_number(instance, dctl);
      write_id_str(")]", dctl);
    }  /* if */
  } else if (*ptr == 'U' && ptr[1] == 'l') {
    /* A lambda has the encoding for the operator() bare function type (without
       the return type) and an optional instance number followed by an
       underscore. */
    write_id_str("[lambda", dctl);
    ptr = demangle_bare_function_type(ptr+2, /*no_return_type=*/TRUE,
                                      BFT_PARAMS, dctl);
    if (*ptr == 'E') {
      ptr = get_instance_number(ptr+1, &instance, dctl);
      if (!dctl->err_in_id) {
        write_id_str(" (instance ", dctl);
        write_id_number(instance, dctl);
        write_id_str(")", dctl);
      }  /* if */
    } else {
      bad_mangled_name(dctl);
    }  /* if */
    write_id_str("]", dctl);
  } else {
    bad_mangled_name(dctl);
  }  /* if */
  return ptr;
}  /* demangle_unnamed_type */


static a_const_char *demangle_abi_tag_attribute(
                                               a_const_char               *ptr,
                                               a_decode_control_block_ptr dctl)
/*
GNU uses a 'B' suffix (ostensibly to <source-name>, but it appears in other
contexts as well) to indicate a list of "abi_tag" attributes -- decode that
list here.  Note that the resulting "__attribute(...)" string doesn't appear
in the proper location in the demangled name (but it serves the purpose of
differentiating the unmangled name from those without the "abi_tag" attribute).
This is a GNU-specific extension that uses the same letter ('B') as an
EDG-specific extension used for the module id of an externalized name but they
don't collide as the EDG-specific use uses 'B' as a prefix while the
GNU-specific extension uses 'B' as a suffix.
*/
{
  long num;

  write_id_str("[abi:", dctl);
  while (*ptr == 'B') {
    ptr++;
    ptr = get_number(ptr, &num, dctl);
    if (num <= 0) {
      bad_mangled_name(dctl);
      break;
    } else {
      for (; num > 0; ptr++, num--) {
        if (*ptr == '\0') {
          bad_mangled_name(dctl);
          break;
        } else {
          write_id_ch(*ptr, dctl);
        }  /* if */
      }  /* for */
      if (*ptr == 'B') {
        /* Another "abi_tag" attribute follows. */
        write_id_ch(',', dctl);
      }  /* if */
    }  /* if */
  }  /* while */
  write_id_ch(']', dctl);
  return ptr;
}  /* demangle_abi_tag_attribute */


static a_const_char *demangle_unqualified_name(
                                 a_const_char               *ptr,
                                 a_boolean                  *is_no_return_name,
                                 a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <unqualified-name> and output the demangled form.
Return a pointer to the character position following what was demangled.
An <unqualified-name> encodes a name that is not qualified, e.g.,
"f" rather than "A::f".  The syntax is:

    <unqualified-name> ::= <operator-name>
                       ::= <ctor-dtor-name>  # Not handled here
                       ::= <source-name>   
                       ::= <unnamed-type-name>   
                       ::= DC <source-name>+ E    # structured binding

Constructor and destructor names do not get here; see
demangle_nested_name_components.  *is_no_return_name is returned TRUE
if the name is one that does not get a return type (e.g., a
conversion function).  is_no_return_name can be NULL if the
caller does not need the value.
*/
{
  if (is_no_return_name != NULL) *is_no_return_name = FALSE;
  if (isdigit((unsigned char)*ptr)) {
    /* A <source-name>, which has a length followed by the characters
       of the identifier, as in "3abc". */
    ptr = demangle_source_name(ptr, /*is_module_id=*/FALSE, dctl);
  } else if (*ptr == 'U' &&
             (ptr[1] == 't' ||
              ptr[1] == 'l')) {
    /* <unnamed-type-name> */
    ptr = demangle_unnamed_type(ptr, dctl);
  } else if (*ptr == 'D' && ptr[1] == 'C') {
    /* A mangled name for a structured binding container. */
    ptr += 2;
    write_id_str("[structured binding for ", dctl);
    while (*ptr != 'E' && *ptr != '\0') {
      ptr = demangle_source_name(ptr, /*is_module_id=*/FALSE, dctl);
      if (*ptr != 'E' && *ptr != '\0') write_id_ch(',', dctl);
    }  /* while */
    if (*ptr != 'E') {
      bad_mangled_name(dctl);
    } else {
      write_id_ch(']', dctl);
      ptr++;
    }  /* if */
  } else {
    /* <operator-name> */
    write_id_str("operator ", dctl);
    if (*ptr == 'c' && ptr[1] == 'v') {
      /* A conversion function. */
      if (is_no_return_name != NULL) *is_no_return_name = TRUE;
      /* A demangling ambiguity exists in the IA-64 ABI when parsing a
         templated conversion operator.  We can't differentiate between
         these type productions in this case:
             <type> ::= <template-param>
                    ::= <template-template-param> <template-args>
         For example, when presented with T_I1BIS4_IiEEEEv, should just the T_
         be parsed (as a <template-param>) or should the entire T_I1BIS4_IiEEEE
         be parsed (as a <template-template-param> <template-args>)?
         We can't do a local retry here because the type may parse just
         fine both ways and we only find out later that there is a problem when
         a substitution number is too large.  On the initial attempt, prefer
         the <template-param> case (parse_template_args); on a
         subsequent attempt (if the demangling fails), we'll try the other
         case. */
      ptr = full_demangle_type(ptr+2,
                           dctl->parse_template_args_after_conversion_operator,
                               /*is_pack_expansion=*/FALSE,
                               dctl);
      dctl->contains_conversion_operator = TRUE;
    } else {
      /* Other operator function (not conversion function). */
      int          num_operands, length;
      a_const_char *op_str, *close_str;
      op_str = get_operator_name(ptr, &num_operands, &length, &close_str,
                                 dctl);
      if (op_str == NULL) {
        bad_mangled_name(dctl);
      } else {
        write_id_str(op_str, dctl);
        write_id_str(close_str, dctl);
        ptr += length;
      }  /* if */
    }  /* if */
  }  /* if */
  if (*ptr == 'B') {
    /* A 'B' suffix for a <source-name> is a GNU-specific extension for
       an abi_tag attribute. */
    ptr = demangle_abi_tag_attribute(ptr, dctl);
  }  /* if */
  return ptr;
}  /* demangle_unqualified_name */


static unsigned char get_hex_digit(a_const_char               *ptr,
                                   a_decode_control_block_ptr dctl)
/*
Convert a hexadecimal digit at ptr to an integral value, and return the
value.
*/
{
  unsigned char value;
  unsigned char ch = (unsigned char)ptr[0];

  if (isdigit(ch)) {
    value = (ch - '0');
  } else if (isxdigit(ch) && islower(ch)) {
    value = (ch - 'a' + 10);
  } else {
    bad_mangled_name(dctl);
    value = 0;
  }  /* if */
  return value;
}  /* get_hex_digit */


static a_const_char *demangle_float_number(a_const_char               *ptr,
                                           a_decode_control_block_ptr dctl)
/*
Demangle a floating point number as specified in an IA-64 float or complex
literal and output the demangled form.  The floating point number is
terminated by either an E or underscore, and the return value will point
to the terminating character.
*/
{
  sizeof_t i, length;
  char     *p;
  union {
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
    long double ld;
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
    double d;
    float f;
  } x;

  /* Zero the bits of x. */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
  x.ld = 0.0;
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  x.d = 0.0;
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
  /* Determine the number of digits in the value by scanning to the
     terminating "E" or "_". */
  length = 0;
  p = (char *)ptr;
  while (*p != 'E' && *p != '_' && *p != '\0') {
    length++;
    p++;
  }  /* while */
  if (length % 2 != 0) {
    /* An odd number of bytes is an error. */
    bad_mangled_name(dctl);
    length -= 1;
  }  /* if */
  /* Convert the length to a byte count. */
  length /= 2;
  if (length > sizeof(x)) {
    /* Too many bytes is an error. */
    bad_mangled_name(dctl);
    length = sizeof(x);
  }  /* if */
  /* Convert the right number of bytes. */
  for (i = 0; i < length; i++, ptr+=2) {
    unsigned char byte = get_hex_digit(ptr, dctl);
    if (dctl->err_in_id) break;
    byte = byte<<4 | get_hex_digit(ptr+1, dctl);
    if (dctl->err_in_id) break;
    if (host_little_endian) {
      ((unsigned char *)&x)[length-1-i] = byte;
    } else {
      ((unsigned char *)&x)[i] = byte;
    }  /* if */
  }  /* for */
  if (!dctl->err_in_id) {
    /* Convert the floating-point value in x to a string. */
    char str[60];
    int  ndig;
    if (i <= sizeof(float)) {
#ifdef FLT_DIG
      ndig = FLT_DIG;
#else /* !defined(FLT_DIG) */
      ndig = 6;
#endif /* ifdef FLT_DIG */
      (void)sprintf(str, "%.*g", ndig, x.f);
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
    } else if (i > sizeof(double)) {
#ifdef LDBL_DIG
      ndig = LDBL_DIG;
#else /* !defined(LDBL_DIG) */
      ndig = 18;
#endif /* ifdef LDBL_DIG */
      (void)sprintf(str, "%.*Lg", ndig, x.ld);
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
    } else {
#ifdef DBL_DIG
      ndig = DBL_DIG;
#else /* !defined(DBL_DIG) */
      ndig = 15;
#endif /* ifdef DBL_DIG */
      (void)sprintf(str, "%.*g", ndig, x.d);
    }  /* if */
    /* Add trailing ".0" if no decimal point or exponent indication was put out
       and the last character of the string is a digit (i.e., not
       "inf" or "nan"). */
    p = str + strlen(str) - 1;
    if (strchr(str, '.') == NULL &&
        strchr(str, 'e') == NULL &&
        isdigit((unsigned char)*p)) {
      p++;
      *p++ = '.';
      *p++ = '0';
      *p++ = '\0';
    }  /* if */
    write_id_str(str, dctl);
  }  /* if */
  return ptr;
}  /* demangle_float_number */


static a_const_char *demangle_float_literal(a_const_char               *ptr,
                                            a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 float literal and output the demangled form.
Return a pointer to the character position following what was demangled.
The syntax is:

  <expr-primary> ::= L <type> <value float> E

<float> is the hexadecimal representation of the floating-point value,
high-order bytes first, using lower-case letters.
*/
{
  /* Put parentheses around the type to make a cast. */
  write_id_ch('(', dctl);
  ptr = demangle_type(ptr+1, dctl);
  write_id_ch(')', dctl);
  if (!dctl->err_in_id) {
    ptr = demangle_float_number(ptr, dctl);
    if (!dctl->err_in_id) {
      ptr = advance_past('E', ptr, dctl);
    }  /* if */
  }  /* if */
  return ptr;
}  /* demangle_float_literal */


static a_const_char *demangle_complex_literal(a_const_char               *ptr,
                                              a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 complex float literal and output the demangled form.
Return a pointer to the character position following what was demangled.
The syntax is:

  <expr-primary> ::= L <type> <real-part float> _ <imag-part float> E 

<float> is the hexadecimal representation of the floating-point value,
high-order bytes first, using lower-case letters.
*/
{
  /* Put parentheses around the type to make a cast. */
  write_id_ch('(', dctl);
  ptr = demangle_type(ptr+1, dctl);
  write_id_str(")(", dctl);
  /* Emit the literal as ( <real> + <imag> i). */
  if (!dctl->err_in_id) {
    ptr = demangle_float_number(ptr, dctl);
    if (!dctl->err_in_id) {
      ptr = advance_past('_', ptr, dctl);
      if (!dctl->err_in_id) {
        write_id_ch('+', dctl);
        ptr = demangle_float_number(ptr, dctl);
        if (!dctl->err_in_id) {
          write_id_str("i)", dctl);
          if (!dctl->err_in_id) {
            ptr = advance_past('E', ptr, dctl);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return ptr;
}  /* demangle_complex_literal */

/*
Macro that returns TRUE if the character represents a floating point type.
*/
#define is_floating_point_type(ch)                                        \
 ((ch) == 'd' || (ch) == 'e' || (ch) == 'f' || (ch) == 'g')

static a_const_char *demangle_expr_primary(a_const_char               *ptr,
                                           a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 literal or external name and output the demangled form.
Return a pointer to the character position following what was demangled.
The syntax is:

  <expr-primary> ::= L <type> <value number> E # integer literal
                 ::= L <type> <value float> E  # floating literal
                 ::= L <string type> E         # string literal
                 ::= L <nullptr type> E        # nullptr literal (i.e., "LDnE")
                 ::= L <type> <real-part float> _ <imag-part float> E   
                                      # complex floating point literal (C 2000)
                 ::= L <mangled-name> E        # external name

*/
{
  a_const_char *sub = NULL;

  if (ptr[1] == 'S') {
    /* Most of the types used in literals are <builtin-type>s, so there are no
       substitutions, but complex literals and string literals can have
       substitutions, so watch out for these. */
    dctl->suppress_id_output++;
    (void)demangle_substitution(ptr+1, 0, CVQ_NONE,
                                /*under_lhs_declarator=*/FALSE,
                                /*need_trailing_space=*/FALSE,
                                (a_const_char **)NULL,
                                &sub,
                                dctl);
    dctl->suppress_id_output--;
  }  /* if */
  if (ptr[1] == '_') {
    /* External name, L_Z <encoding> E. */
    if (ptr[2] != 'Z') {
      bad_mangled_name(dctl);
    } else {
      ptr = demangle_encoding(ptr+3, /*include_func_params=*/FALSE, dctl);
      ptr = advance_past('E', ptr, dctl);
    }  /* if */
  } else if (is_floating_point_type(ptr[1])) {
    /* Float literal, L <type> <hex> E, where <hex> is the hexadecimal
       representation of the value, high-order bytes first, with
       lower-case hex letters. */
    ptr = demangle_float_literal(ptr, dctl);
  } else if ((ptr[1] == 'C' && is_floating_point_type(ptr[2])) ||
             (sub != NULL &&
              (sub[0] == 'C' && is_floating_point_type(sub[1])))) {
    /* Complex floating point literal. */
    ptr = demangle_complex_literal(ptr, dctl);
  } else if (ptr[1] == 'D' &&
             (ptr[2] == 'n' || ptr[2] == 'N') &&
             ptr[3] == 'E') {
    /* Recognize the literal for nullptr or __nullptr (the mangling is an
       EDG extension for the C++/CLI managed __nullptr keyword). */
    dctl->suppress_id_output++;
    (void)demangle_type(ptr+1, dctl);
    dctl->suppress_id_output--;
    if (ptr[2] == 'N') {
      write_id_str("__nullptr", dctl);
    } else {
      write_id_str("nullptr", dctl);
    }  /* if */
    ptr += 4;
  } else {
    /* Integer literal, or string literal. */
    /* Put parentheses around the type to make a cast. */
    write_id_ch('(', dctl);
    ptr = demangle_type(ptr+1, dctl);
    write_id_ch(')', dctl);
    if (*ptr == 'E') {
      /* There's no value -- must have been a string literal. */
      write_id_str("\"...\"", dctl);
    } else {
      /* Copy the literal value.  "n" is translated to a "-". */
      if (*ptr == 'n') {
        write_id_ch('-', dctl);
        ptr++;
      }  /* if */
      /* g++ 3.2 puts out L1xE instead of L_Z1xE, which gets demangled
         sort of okay in the g++ demangler because the name is treated
         as a type and a cast is put out with nothing following it: (x) */
      if (!isdigit((unsigned char)*ptr) && !emulate_gnu_abi_bugs) {
        bad_mangled_name(dctl);
      } else {
        while (isdigit((unsigned char)*ptr)) {
          write_id_ch(*ptr, dctl);
          ptr++;
        }  /* while */
      }  /* if */
    }  /* if */
    ptr = advance_past('E', ptr, dctl);
  }  /* if */
  return ptr;
}  /* demangle_expr_primary */


static a_const_char *demangle_expression_list_full(
                                 a_const_char               *ptr,
                                 char                       stop_char,
                                 char                       open_paren,
                                 char                       close_paren,
                                 a_decode_control_block_ptr dctl)
/*
Demangle zero or more expressions, terminated by stop_char.  The expression
list output is enclosed by open_paren/close_paren and separated by commas.
Returns a pointer to the terminating character (unless an error occurs).
*/
{
  a_boolean first_time = TRUE;

  write_id_ch(open_paren, dctl);
  while (*ptr != stop_char && !dctl->err_in_id) {
    if (*ptr == '\0') {
      bad_mangled_name(dctl);
      break;
    }  /* if */
    if (!first_time) {
      write_id_str(", ", dctl);
    } else {
      first_time = FALSE;
    }  /* if */
    ptr = demangle_expression(ptr, dctl);
  }  /* while */
  write_id_ch(close_paren, dctl);
  return ptr;
}  /* demangle_expression_list_full */


static a_const_char *demangle_expression_list(
                                 a_const_char               *ptr,
                                 char                       stop_char,
                                 a_decode_control_block_ptr dctl)
/*
Demangle zero or more expressions, terminated by stop_char.  The expression
list output is enclosed in parentheses and separated by commas.  Returns a
pointer to the terminating character (unless an error occurs).
*/
{
  return demangle_expression_list_full(ptr, stop_char, '(', ')', dctl);
}  /* demangle_expression_list */


static a_const_char *demangle_initializer(
                                 a_const_char               *ptr,
                                 a_decode_control_block_ptr dctl)
/*
Demangle an <initializer> (or an 'E') starting at ptr.

  <initializer> ::= pi <expression>* E  # parenthesized initialization
  <initializer> ::= il <expression>* E  # braced-init list

*/
{
  if (*ptr == 'E') {
    ptr++;
  } else {
    if (*ptr == 'p' && ptr[1] == 'i') {
      ptr = demangle_expression_list(ptr+2, 'E', dctl);
      ptr = advance_past('E', ptr, dctl);
    } else if (*ptr == 'i' && ptr[1] == 'l') {
      ptr = demangle_expression_list_full(ptr+2, 'E', '{', '}', dctl);
      ptr = advance_past('E', ptr, dctl);
    } else {
      bad_mangled_name(dctl);
    }  /* if */
  }  /* if */
  return ptr;
}  /* demangle_initializer */


static a_const_char *demangle_expression(a_const_char               *ptr,
                                         a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <expression> and output the demangled form.
Return a pointer to the character position following what was demangled.
An <expression> encodes an expression (usually for a nontype
template argument value written in terms of template parameters or 
trailing return types specified using decltype).
The syntax is:

  <expression> ::= <unary operator-name> <expression>
               ::= <binary operator-name> <expression> <expression>
               ::= <ternary operator-name> <expression> <expression>
                                                                   <expression>
               ::= cl <expression>+ E                                   
                              # call
               ::= cp <simple-id> <expression>* E
                              # call (with ADL suppressed)
               ::= cv <type> <expression>                               
                              # conversion with one argument
               ::= cv <type> _ <expression>* E                          
                              # conversion with a different number of arguments
               ::= [gs] nw <expression>* _ <type> E                     
                              # new (expr-list) type
               ::= [gs] nw <expression>* _ <type> <initializer>         
                              # new (expr-list) type (init)
               ::= [gs] na <expression>* _ <type> E                     
                              # new[] (expr-list) type
               ::= [gs] na <expression>* _ <type> <initializer>         
                              # new[] (expr-list) type (init)
               ::= [gs] dl <expression>                                 
                              # delete expression
               ::= [gs] da <expression>                                 
                              # delete[] expression
               ::= pp_ <expression>                                     
                              # prefix ++
               ::= mm_ <expression>                                     
                              # prefix --
               ::= ti <type>                                            
                              # typeid (type)
               ::= te <expression>                                      
                              # typeid (expression)
               ::= dc <type> <expression>                               
                              # dynamic_cast<type> (expression)
               ::= sc <type> <expression>                               
                              # static_cast<type> (expression)
               ::= cc <type> <expression>                               
                              # const_cast<type> (expression)
               ::= rc <type> <expression>                               
                              # reinterpret_cast<type> (expression)
               ::= st <type>                                            
                              # sizeof (a type)
               ::= at <type>                                            
                              # alignof (a type)
               ::= <template-param>
               ::= <function-param>
               ::= dt <expression> <unresolved-name>                    
                              # expr.name
               ::= pt <expression> <unresolved-name>                    
                              # expr->name
               ::= ds <expression> <expression>                         
                              # expr.*expr
               ::= tw <expression>                                      
                              # throw expression
               ::= tr                                                   
                              # throw with no operand (rethrow)
               ::= <unresolved-name>                                    
                              # f(p), N::f(p), ::f(p),
                              # freestanding dependent name (e.g., T::x),
                              # objectless nonstatic member reference
               ::= sZ <template-param>
                              # size of a parameter pack
               ::= sZ <function-param>
                              # size of a function parameter pack
               ::= sp <expression>
                              # pack expansion
               ::= il <expression>* E
                              # initializer list
               ::= tl <type> <expression>* E
                              # typed initializer list
               ::= fl <binary operator-name> <expression>
                              # ( ... op pack )
               ::= fr <binary operator-name> <expression>
                              # ( pack op ... )
               ::= fL <binary operator-name> <expression>
                              # ( expr op ... op pack )
               ::= fR <binary operator-name> <expression>
                              # ( pack op ... op expr )
               ::= <expr-primary>

Also, these non-standard expressions (EDG-specific) are demangled:

               ::= gc _ <type> E
                              # gcnew type
               ::= gc _ <type> <initializer>
                              # gcnew type (init)
               ::= gc <expression>* _ <type> E
                              # gcnew array<type>(dims)
               ::= gc <expression>* _ <type> <initializer>
                              # gcnew array<type>(dims) {init}

*/
{
  int          num_operands, length;
  a_const_char *op_str, *close_str;

  if (*ptr == 'L') {
    /* A literal or external name. */
    ptr = demangle_expr_primary(ptr, dctl);
  } else if (*ptr == 'T') {
    /* A template parameter. */
    ptr = demangle_template_param(ptr, dctl);
  } else if (*ptr == 'f') {
    if (ptr[1] == 'p' ||
        (ptr[1] == 'L' && isdigit((unsigned char)ptr[2]))) {
      /* A reference to a function parameter. */
      ptr = demangle_parameter_reference(ptr, dctl);
    } else {
      /* A "fold expression". */
      a_boolean    unary, left;
      a_const_char *op_str, *close_str;
      int          num_operands, length;
      switch (ptr[1]) {
        case 'l': unary = TRUE;  left = TRUE;  break;
        case 'L': unary = FALSE; left = TRUE;  break;
        case 'r': unary = TRUE;  left = FALSE; break;
        case 'R': unary = FALSE; left = FALSE; break;
        default:
          bad_mangled_name(dctl);
          break;
      }  /* switch */
      ptr += 2;
      op_str = get_operator_name(ptr, &num_operands, &length, &close_str,
                                 dctl);
      if (op_str == NULL) {
        bad_mangled_name(dctl);
      } else {
        ptr += length;
        write_id_ch('(', dctl);
        if (unary) {
          if (left) {
            write_id_str("...", dctl);
            write_id_str(op_str, dctl);
            ptr = demangle_expression(ptr, dctl);
          } else {
            ptr = demangle_expression(ptr, dctl);
            write_id_str(op_str, dctl);
            write_id_str("...", dctl);
          }  /* if */
        } else {
          ptr = demangle_expression(ptr, dctl);
          write_id_str(op_str, dctl);
          write_id_str("...", dctl);
          write_id_str(op_str, dctl);
          ptr = demangle_expression(ptr, dctl);
        }  /* if */
        write_id_ch(')', dctl);
      }  /* if */
    }  /* if */
  } else if (*ptr == 'c' && ptr[1] == 'l') {
    /* Call expression: "cl <expression>+ E" */
    ptr += 2;
    ptr = demangle_expression(ptr, dctl);
    ptr = demangle_expression_list(ptr, 'E', dctl);
    ptr = advance_past('E', ptr, dctl);
  } else if (*ptr == 'c' && ptr[1] == 'p') {
    /* Call expression (w/ADL suppressed): "cp <simple-id> <expression>* E" */
    ptr += 2;
    write_id_ch('(', dctl);
    ptr = demangle_simple_id(ptr, dctl);
    write_id_ch(')', dctl);
    ptr = demangle_expression_list(ptr, 'E', dctl);
    ptr = advance_past('E', ptr, dctl);
  } else if (*ptr == 'c' && ptr[1] == 'v') {
    /* Cast/conversion (with one type and zero or more arguments).  When
       exactly one expression is specified, emit "(T)expr", otherwise
       emit T(expr). */
    a_const_char *nptr;
    a_boolean one_argument = FALSE;
    /* Take a peek (without emitting the type, but recording substitutions)
       to see which case we have. */
    dctl->suppress_id_output++;
    nptr = demangle_type(ptr+2, dctl);
    dctl->suppress_id_output--;
    if (!dctl->err_in_id && *nptr != '_') {
      one_argument = TRUE;
      write_id_ch('(', dctl);
    }  /* if */
    /* Re-scan the type, this time emitting it (but not recording
       substitutions). */
    dctl->suppress_substitution_recording++;
    ptr = demangle_type(ptr+2, dctl);
    dctl->suppress_substitution_recording--;
    if (!dctl->err_in_id) {
      if (one_argument) {
        /* Exactly one expression. */
        write_id_ch(')', dctl);
        ptr = demangle_expression(ptr, dctl);
      } else {
        /* Some number of expressions (other than one). */
        if (*ptr != '_') {
          bad_mangled_name(dctl);
        } else {
          ptr = demangle_expression_list(ptr+1, 'E', dctl);
          ptr = advance_past('E', ptr, dctl);
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (*ptr == 'g' && ptr[1] == 's') {
    /* global scope: "::".  This prefix precedes new/delete operations as
       well as the scope-resolution operator in <unresolved-name>. */
    write_id_str("::", dctl);
    ptr = demangle_expression(ptr+2, dctl);
  } else if (*ptr == 'n' && (ptr[1] == 'w' || ptr[1] == 'a')) {
    /* new or new[] */
    if (ptr[1] == 'w') {
      write_id_str("new ", dctl);
    } else {
      write_id_str("new[] ", dctl);
    }  /* if */
    ptr+=2;
    /* Optional placement expressions. */
    if (*ptr != '_') {
      ptr = demangle_expression_list(ptr, '_', dctl);
      write_id_ch(' ', dctl);
    }  /* if */
    ptr = advance_past('_', ptr, dctl);
    if (!dctl->err_in_id) {
      ptr = demangle_type(ptr, dctl);
      if (!dctl->err_in_id) {
        ptr = demangle_initializer(ptr, dctl);
      }  /* if */
    }  /* if */
  } else if (*ptr == 'g' && ptr[1] == 'c') {
    /* C++/CLI gcnew (EDG-specific mangling). */
    ptr+=2;
    write_id_str("gcnew ", dctl);
    /* Optional array dimension expressions. */
    if (*ptr != '_') {
      a_const_char *optr = ptr, *ptr2;
      /* We need the type before the dimension list, so suppress the list
         to get to the type. */
      dctl->suppress_id_output++;
      dctl->suppress_substitution_recording++;
      ptr2 = demangle_expression_list(ptr, '_', dctl);
      dctl->suppress_id_output--;
      dctl->suppress_substitution_recording--;
      if (!dctl->err_in_id) {
        ptr2 = advance_past('_', ptr2, dctl);
        ptr = demangle_type(ptr2, dctl);
        (void)demangle_expression_list(optr, '_', dctl);
        write_id_ch(' ', dctl);
      }  /* if */
    } else {
      ptr = advance_past('_', ptr, dctl);
      ptr = demangle_type(ptr, dctl);
    }  /* if */
    if (!dctl->err_in_id) {
      ptr = demangle_initializer(ptr, dctl);
    }  /* if */
  } else if (*ptr == 'd' && ptr[1] == 't') {
    /* expr.name */
    write_id_ch('(', dctl);
    ptr = demangle_expression(ptr+2, dctl);
    if (!dctl->err_in_id) {
      write_id_ch('.', dctl);
      ptr = demangle_unresolved_name(ptr, dctl);
      write_id_ch(')', dctl);
    }  /* if */
  } else if (*ptr == 'p' && ptr[1] == 't') {
    /* expr->name */
    write_id_ch('(', dctl);
    ptr = demangle_expression(ptr+2, dctl);
    if (!dctl->err_in_id) {
      write_id_str("->", dctl);
      ptr = demangle_unresolved_name(ptr, dctl);
      write_id_ch(')', dctl);
    }  /* if */
  } else if (*ptr == 's' && ptr[1] == 'Z') {
    /* Size of a parameter pack. */
    write_id_str("sizeof...(", dctl);
    ptr+=2;
    if (*ptr == 'T') {
      /* sizeof...(<template-param>) */
      ptr = demangle_template_param(ptr, dctl);
    } else if (*ptr == 'f' ) {
      /* sizeof...(<function-param>) */
      ptr = demangle_parameter_reference(ptr, dctl);
    } else {
      bad_mangled_name(dctl);
    }  /* if */
    write_id_ch(')', dctl);
  } else if (*ptr == 's' && ptr[1] == 'p') {
    /* Pack expansion. */
    ptr+=2;
    ptr = demangle_expression(ptr, dctl);
    write_id_str("...", dctl);
  } else if ((*ptr == 't' || *ptr == 'i') && ptr[1] == 'l') {
    /* Initializer list. */
    if (*ptr == 't') {
      /* A type is included. */
      ptr = demangle_type(ptr+2, dctl);
    } else {
      ptr += 2;
    }  /* if */
    if (!dctl->err_in_id) {
      ptr = demangle_expression_list_full(ptr, 'E', '{', '}', dctl);
      ptr = advance_past('E', ptr, dctl);
    }  /* if */
  } else if ((op_str = get_operator_name(ptr, &num_operands, &length,
                                         &close_str, dctl)) != NULL) {
    /* An expression beginning with an operator name. */
    /* As a heuristic, to avoid extraneous parentheses in the demangled output,
       assume that any operator that has a closing string doesn't need
       parentheses around it. */
    a_boolean needs_parens = strcmp(close_str, "") == 0;
    ptr += length;
    if (needs_parens) write_id_ch('(', dctl);
    if (strncmp(op_str, "builtin-operation-", 18) == 0) {
      /* Builtin operation.  Has a variable number of operands. */
      int i;
      write_id_str(op_str, dctl);
      write_id_ch('(', dctl);
      for (i = 1; i <= num_operands; i++) {
        if (*ptr == 'T' && ptr[1] == 'O') {
          /* "TO" indicates a type operand. */
          ptr = demangle_type(ptr+2, dctl);
        } else {
          ptr = demangle_expression(ptr, dctl);
        }  /* if */
        if (i != num_operands) write_id_str(", ", dctl);
      }  /* for */
      write_id_ch(')', dctl);
    } else if (strncmp(op_str, "subscript", 9) == 0) {
      /* C++/CLI subscript operation.  Has a variable number of operands. */
      int i;
      ptr = demangle_expression(ptr, dctl);
      write_id_ch('[', dctl);
      for (i = 2; i <= num_operands; i++) {
        ptr = demangle_expression(ptr, dctl);
        if (i != num_operands) write_id_str(", ", dctl);
      }  /* for */
      write_id_ch(']', dctl);
    } else if (num_operands == 1) {
      char cast_close = 0;
      /* Unary operations (old style cast is handled above). */
      if (strcmp(op_str, "++") == 0 ||
          strcmp(op_str, "--") == 0) {
        if (*ptr == '_') {
          /* Prefix version. */
          ptr++;
        } else {
          /* Postfix version. */
          close_str = op_str;
          op_str = "";
        }  /* if */
      }  /* if */
      write_id_str(op_str, dctl);
      if (strcmp(op_str, "static_cast") == 0 ||
          strcmp(op_str, "dynamic_cast") == 0 ||
          strcmp(op_str, "const_cast") == 0 ||
          strcmp(op_str, "reinterpret_cast") == 0 ||
          strcmp(op_str, "safe_cast") == 0) {
        /* New style cast. */
        write_id_ch('<', dctl);
        ptr = demangle_type(ptr, dctl);
        write_id_str(">(", dctl);
        cast_close = ')';
      }  /* if */
      ptr = demangle_expression(ptr, dctl);
      if (cast_close != 0) write_id_ch(cast_close, dctl);
    } else if (num_operands == 2) {
      /* Binary operations. */
      ptr = demangle_expression(ptr, dctl);
      write_id_str(op_str, dctl);
      ptr = demangle_expression(ptr, dctl);
    } else if (num_operands == 3) {
      /* Ternary operations ("?"). */
      ptr = demangle_expression(ptr, dctl);
      write_id_str(op_str, dctl);
      ptr = demangle_expression(ptr, dctl);
      write_id_str(":", dctl);
      ptr = demangle_expression(ptr, dctl);
    } else {
      /* Special cases: sizeof(type), __alignof__(type),
         __uuidof(type), typeid(type), T::typeid, scope resolution "::",
         throw (just the rethrow variety). */
      if (strcmp(op_str, "sizeof(") == 0) {
        /* sizeof(type). */
        write_id_str(op_str, dctl);
        ptr = demangle_type(ptr, dctl);
      } else if (strcmp(op_str, "alignof(") == 0 ||
                 strcmp(op_str, "__alignof__(") == 0) {
        /* __alignof__(type). */
        write_id_str(op_str, dctl);
        ptr = demangle_type(ptr, dctl);
      } else if (strcmp(op_str, "__uuidof(") == 0) {
        /* __uuidof(type). */
        write_id_str(op_str, dctl);
        ptr = demangle_type(ptr, dctl);
      } else if (strcmp(op_str, "typeid(") == 0) {
        /* typeid(type). */
        write_id_str(op_str, dctl);
        ptr = demangle_type(ptr, dctl);
      } else if (strcmp(op_str, "::typeid") == 0) {
        /* C++/CLI T::typeid. */
        ptr = demangle_type(ptr, dctl);
        write_id_str(op_str, dctl);
      } else if (strcmp(op_str, "throw") == 0) {
        /* throw.  This handles the rethrow variety, throw-expression is
           handled separately. */
        write_id_str(op_str, dctl);
      } else if (strncmp(op_str, "\"\"", 2) == 0) {
        /* A literal operator.  The EDG front end doesn't produce a mangled
           name that should get here (i.e., there should be no "clli" mangled
           names), but other compilers produce this mangling, so handle it
           by inserting the implied operator keyword. */
        write_id_str("operator ", dctl);
        write_id_str(op_str, dctl);
      } else {
        bad_mangled_name(dctl);
      }  /* if */
    }  /* if */
    write_id_str(close_str, dctl);
    if (needs_parens) write_id_ch(')', dctl);
  } else {
    /* Assume it's an <unresolved-name>. */
    ptr = demangle_unresolved_name(ptr, dctl);
  }  /* if */
  return ptr;
}  /* demangle_expression */


static a_const_char *demangle_template_args(a_const_char               *ptr,
                                            a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <template-args> and output the demangled form.
Return a pointer to the character position following what was demangled.
A <template-args> encodes a template argument list.  The syntax is:

  <template-args> ::= I <template-arg>+ E
  <template-arg> ::= <type>                     # type or template
                 ::= X <expression> E           # expression
                 ::= <expr-primary>             # simple expressions
                 ::= J <template-arg>* E        # argument pack

*/
{
  a_boolean suppress = FALSE;

  if (*ptr == 'J') {
    /* In pack expansion cases, suppress generation of angle brackets. */
    suppress = TRUE;
  }  /* if */
  /* Advance past the "I" or "J". */
  ptr++;
  if (!suppress) write_id_ch('<', dctl);
  for (;;) {
    if (*ptr == 'X') {
      /* An expression, X <expression> E. */
      ptr = demangle_expression(ptr+1, dctl);
      ptr = advance_past('E', ptr, dctl);
    } else if (*ptr == 'L') {
      /* Literal or external name. */
      ptr = demangle_expr_primary(ptr, dctl);
    } else if (*ptr == 'J' ||
              (*ptr == 'I' && emulate_gnu_abi_bugs)) {
      /* Template argument pack. */
      ptr = demangle_template_args(ptr, dctl);
    } else if (*ptr == 'E') {
      /* No template arguments. */
      break;
    } else {
      /* Type template argument. */
      ptr = demangle_type(ptr, dctl);
    }  /* if */
    /* "E" ends the template argument list. */
    if (*ptr == 'E') break;
    /* Stop on an error. */
    if (dctl->err_in_id) break;
    /* Continuing, so put out a comma between template arguments. */
    write_id_str(", ", dctl);
  }  /* for */
  ptr = advance_past('E', ptr, dctl);
  if (!suppress) write_id_ch('>', dctl);
  return ptr;
}  /* demangle_template_args */


static a_const_char *demangle_nested_name_components(
                              a_const_char               *ptr,
                              unsigned long              num_levels,
                              a_boolean                  *is_no_return_name,
                              a_boolean                  *has_templ_arg_list,
                              char                       *ctor_dtor_kind,
                              a_const_char               **last_component_name,
                              a_decode_control_block_ptr dctl)
/*
Demangle one or more name level components of an IA-64 <nested-name>.
Each level is either an unqualified name or a substitution, optionally
followed by a template argument list.  ptr points to the beginning of
the <nested-name>, after the initial "N" and the <CV-qualifiers> if any.
If num_levels is zero, scan all components of the nested name, stopping
on the final "E"; otherwise, scan num_levels levels and then stop.
Note that a substitution counts as one level even if it represents
several.  Return a pointer to the character position following what
was demangled.  *is_no_return_name is returned TRUE if the final
component scanned is a function name of a kind that does not take a
return type (constructor, destructor, or conversion function).
*has_templ_arg_list is returned TRUE if the final component includes a
template argument list.  If the final component is a constructor or
destructor name, *ctor_dtor_kind is set to the character identifying
the kind of constructor or destructor.  If last_component_name is non-NULL,
*last_component_name will be set to the start position of the encoding
for the name of the last component.  If the last component is a
substitution, the name of the last component in the substitution is used.
*/
{
  a_const_char  *prev_component_name = NULL;
  a_const_char  *first_component_start = ptr;
  unsigned long level_num = 0;

  *is_no_return_name = FALSE;
  *has_templ_arg_list = FALSE;
  *ctor_dtor_kind = ' ';
  for (;;) {
    /* Demangle one level of the nested name. */
    a_boolean is_substitution = FALSE;
    a_boolean suppress_qualification = FALSE;
    level_num++;
    *is_no_return_name = FALSE;
    *has_templ_arg_list = FALSE;
    if (*ptr == 'E' || *ptr == '\0') {
      /* Error, unexpected end of nested name. */
      bad_mangled_name(dctl);
    } else if (*ptr == 'S') {
      /* A substitution. */
      is_substitution = TRUE;
      ptr = demangle_substitution(ptr, 0, CVQ_NONE,
                                  /*under_lhs_declarator=*/FALSE,
                                  /*need_trailing_space=*/FALSE,
                                  &prev_component_name,
                                  (a_const_char **)NULL,
                                  dctl);
      /* A substitution cannot be the last thing; it must be followed
         by another name or a template argument list. */
      if (*ptr == 'E') {
        bad_mangled_name(dctl);
      }  /* if */
    } else if (*ptr == 'T') {
      /* A <template-param>. */
      ptr = demangle_template_param(ptr, dctl);
    } else if (*ptr == 'D' && (ptr[1] == 't' || ptr[1] == 'T')) {
      /* A <decltype>. */
      ptr = demangle_type(ptr, dctl);
    } else {
      /* Not a substitution or template parameter, so an <unqualified-name>. */
      if (*ptr != 'C' && (*ptr != 'D' || ptr[1] == 'C')) {
        /* Normal case, not a constructor or destructor name. */
        prev_component_name = ptr;
        ptr = demangle_unqualified_name(ptr, is_no_return_name, dctl);
      } else {
        /* A constructor or destructor name (or their C++/CLI counterparts:
           a static constructor or finalizer).  Put out the class name again
           (it's provided by prev_component_name). */
        *is_no_return_name = TRUE;
        if (*ptr == 'D') {
          if (ptr[1] == '7') {
            /* A C++/CLI finalizer. */
            write_id_ch('!', dctl);
          } else {
            /* Some type of destructor. */
            write_id_ch('~', dctl);
          }  /* if */
        }  /* if */
        if (prev_component_name == NULL ||
            *prev_component_name == 'S') {
          /* The constructor or destructor code is the first thing in the
             nested name or the previous name is a substitution (we're
             supposed to have gotten the name from inside the
             substitution). */
          bad_mangled_name(dctl);
        } else {
          a_boolean dummy;
          /* Rescan and output the class name (no template argument list). */
          (void)demangle_unqualified_name(prev_component_name, &dummy, dctl);
          /* Check that the second character of the constructor/destructor
             name is a valid digit. */
          /* "C3" is for an allocating constructor (not generated by the
             EDG Front End but part of the ABI spec). */
          /* "D7" is the code used by the EDG C++ Front End for C++/CLI
             finalizers.  It's not part of the ABI spec. */
          /* "C8" is the code used by the EDG C++ Front End for C++/CLI
             static constructors.  It's not part of the ABI spec. */
          /* "C9" is the code used by the EDG C++ Front End for the
             common routine called by the various delegating constructor
             entry points.  It's not part of the ABI spec. */
          /* "D9" is the code used by the EDG C++ Front End for the
             delegation destructor alternate entry point.  It's not part
             of the ABI spec. */
          if (ptr[1] == '1' || ptr[1] == '2' || ptr[1] == '9' ||
              (ptr[0] == 'C' ? (ptr[1] == '3' || ptr[1] == '8') :
                               (ptr[1] == '0' || ptr[1] == '7'))) {
            /* Okay. */
            *ctor_dtor_kind = ptr[1];
            ptr += 2;
            if (*ptr == 'B') {
              /* A 'B' suffix for a <source-name> is a GNU-specific extension
                 for an abi_tag attribute. */
              ptr = demangle_abi_tag_attribute(ptr, dctl);
            }  /* if */
          } else {
            /* The second character of the constructor or destructor name
               encoding is bad. */
            bad_mangled_name(dctl);
          }  /* if */
        }  /* if */
      }  /* if */
      if (*ptr == 'M') {
        /* A <data-member-prefix>.  No further output is required (the
           member's <source-name> has been emitted above). */
        ptr++;
      }  /* if */
    }  /* if */
    if (*ptr == 'I') {
      /* A <template-args> list. */
      /* Record a potential substitution on the template prefix up to
         this point, but not if the entire prefix is a substitution. */
      if (!is_substitution) {
        record_substitutable_entity(first_component_start,
                                    subk_template_prefix, level_num-1,
                                    /*parse_template_args=*/FALSE, dctl);
      }  /* if */
      /* Scan the template argument list. */
      ptr = demangle_template_args(ptr, dctl);
      *has_templ_arg_list = TRUE;
      is_substitution = FALSE;
    }  /* if */
    /* "E" marks the end of the list. */
    if (*ptr == 'E') break;
    if (!is_substitution) {
      /* Record a potential substitution on the prefix up to this point,
         but not if the entire prefix is a substitution (without
         template argument list). */
      record_substitutable_entity(first_component_start, subk_prefix,
                                  level_num,
                                  /*parse_template_args=*/FALSE, dctl);
    }  /* if */
    /* Stop on an error. */
    if (dctl->err_in_id) break;
    /* Stop if we've done enough levels. */
    if (num_levels != 0 && level_num >= num_levels) break;
    /* Going around again, so the part put out so far is a qualifier and
       needs to be followed by "::". */
    if (!suppress_qualification) write_id_str("::", dctl);
  }  /* for */
  if (last_component_name != NULL) *last_component_name = prev_component_name;
  return ptr;
}  /* demangle_nested_name_components */


static a_const_char *demangle_nested_name(
                                        a_const_char               *ptr,
                                        a_func_block               *func_block,
                                        a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <nested-name> and output the demangled form.  Return
a pointer to the character position following what was demangled.
A <nested-name> represents a qualified name, e.g., A::B::x.
The syntax is:

    <nested-name> ::= N [<CV-qualifiers>] [<ref-qualifier>]
                                            <prefix> <unqualified-name> E
                  ::= N [<CV-qualifiers>] [<ref-qualifier>]
                                            <template-prefix> <template-args> E
    <prefix> ::= <prefix> <unqualified-name>
             ::= <template-prefix> <template-args>
             ::= <template-param>
             ::= <decltype>
             ::= # empty
             ::= <substitution>
             ::= <prefix> <data-member-prefix>
    <template-prefix> ::= <prefix> <template unqualified-name>
                      ::= <template-param>
                      ::= <substitution>
    <data-member-prefix> := <member source-name> M

For function names, additional information is returned in *func_block.
*/
{
  a_boolean has_templ_arg_list;
  a_boolean is_no_return_name;

  clear_func_block(func_block);
  /* Skip the initial "N". */
  ptr++;
  /* Accumulate <CV-qualifiers> if present. */
  ptr = get_cv_qualifiers(ptr, &func_block->cv_quals);
  /* Save a <ref-qualifier> if present. */
  ptr = get_ref_qualifier(ptr, &func_block->ref_qual);
  /* Get all the components of the nested name. */
  ptr = demangle_nested_name_components(ptr,
                                        /*num_levels=*/0,
                                        &is_no_return_name,
                                        &has_templ_arg_list,
                                        &func_block->ctor_dtor_kind,
                                        (a_const_char **)NULL,
                                        dctl);
  ptr = advance_past('E', ptr, dctl);
  /* The function will have no return type if it is not a template. */
  if (!has_templ_arg_list) {
    func_block->no_return_type = TRUE;
  }  /* if */
  /* The function will have no return type if it is a constructor,
     destructor, or conversion function. */
  if (is_no_return_name) {
    func_block->no_return_type = TRUE;
  }  /* if */
  return ptr;
}  /* demangle_nested_name */


static a_const_char *demangle_local_name(
                                        a_const_char               *ptr,
                                        a_func_block               *func_block,
                                        a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <local-name> and output the demangled form.  Return
a pointer to the character position following what was demangled.
A <local-name> represents an entity local to a function, and
includes the mangled name of the enclosing function.
The syntax is:

  <local-name> := Z <function encoding> E [d [<trailing-param number>] _]
                  <entity name> [<discriminator>]
               := Z <function encoding> E s [<discriminator>]
  <discriminator> := _ <non-negative number>     # when number <= 9
                  := __ <non-negative number> _  # when number >= 10

For function names, additional information is returned in *func_block.
*/
{
  clear_func_block(func_block);
  /* Skip over the "Z". */
  ptr++;
  /* Demangle the function name. */
  ptr = demangle_encoding(ptr, /*include_func_params=*/TRUE, dctl);
  ptr = advance_past('E', ptr, dctl);
  write_id_str("::", dctl);
  if (*ptr == 's') {
    /* String literal. */
    write_id_str("string", dctl);
    ptr++;
  } else {
    if (*ptr == 'd') {
      /* Demangle the optional trailing parameter number. */
      long param = -1;
      ptr += 1;
      if (*ptr != '_') {
        ptr = get_number(ptr, &param, dctl);
        if (param < 0 || *ptr != '_') {
          bad_mangled_name(dctl);
        } else {
          /* Advance past underscore. */
          ptr += 1;
        }  /* if */
      } else {
        /* Advance past underscore. */
        ptr += 1;
      }  /* if */
      if (!dctl->err_in_id) {
        write_id_str("[default argument ", dctl);
        write_id_signed_number(param+2, dctl);
        write_id_str(" (from end)]::", dctl);
      }  /* if */
    }  /* if */
    /* Demangle the entity name. */
    ptr = demangle_name(ptr, func_block, /*options=*/DNO_ALL, dctl);
  }  /* if */
  if (!dctl->err_in_id && *ptr == '_') {
    /* Demangle the discriminator. */
    long num = -1;
    if (isdigit((unsigned char)ptr[1])) {
      /* _n (n is single digit) case: */
      num = (char)ptr[1] - '0';
      ptr += 2;
    } else if (ptr[1] == '_' && isdigit((unsigned char)ptr[2])) {
      /* __nn_ (nn is at least two digits) case: */
      ptr = get_number(ptr+2, &num, dctl);
      if (*ptr == '_') {
        ptr += 1;
      } else {
        num = -1;
      }  /* if */
    }  /* if */
    if (num < 0) {
      bad_mangled_name(dctl);
    } else {
      write_id_str(" (instance ", dctl);
      write_id_signed_number(num+2, dctl);
      write_id_ch(')', dctl);
    }  /* if */
  }  /* if */
  return ptr;
}  /* demangle_local_name */


static a_const_char *demangle_unscoped_name(
                                        a_const_char               *ptr,
                                        a_func_block               *func_block,
                                        a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <unscoped-name> and output the demangled form.
Return a pointer to the character position following what was demangled.
The syntax is:

    <unscoped-name> ::= <unqualified-name>
                    ::= St <unqualified-name>   # ::std::

For function names, additional information is updated in *func_block.
*/
{
  a_boolean is_no_return_name;

  if (*ptr == 'S' && ptr[1] == 't') {
    /* "St" for "std::". */
    write_id_str("std::", dctl);
    ptr += 2;
  }  /* if */
  ptr = demangle_unqualified_name(ptr, &is_no_return_name, dctl);
  func_block->no_return_type = is_no_return_name;
  return ptr;
}  /* demangle_unscoped_name */


static a_const_char *demangle_name(a_const_char               *ptr,
                                   a_func_block               *func_block,
                                   a_demangle_name_option     options,
                                   a_decode_control_block_ptr dctl)
/*
Demangle selected portions of an IA-64 <name> and output the demangled form.
Return a pointer to the character position following what was demangled.
The syntax is:

    <name> ::= <nested-name>
           ::= <unscoped-name>
           ::= <unscoped-template-name> <template-args>
           ::= <local-name>
    <unscoped-template-name> ::= <unscoped-name>
                             ::= <substitution>

For function names, additional information is returned in *func_block.
options is a bit mask that specifies which portion(s) of the name should
be emitted (the entire name is scanned, i.e., the returned value does not
depend on the options specified).

As an EDG extension, allow

    B <source-name>

as a prefix to specify a module id for an externalized name.
*/
{
  clear_func_block(func_block);
  if (*ptr == 'B') {
    /* Module-id prefix for externalized name. */
    if ((options & DNO_EXTERNALIZATION) == 0) dctl->suppress_id_output++;
    write_id_str("[static from ", dctl);
    ptr = demangle_source_name(ptr+1, /*is_module_id=*/TRUE, dctl);
    write_id_str("] ", dctl);
    if ((options & DNO_EXTERNALIZATION) == 0) dctl->suppress_id_output--;
  }  /* if */
  if ((options & DNO_NAME) == 0) dctl->suppress_id_output++;
  if (*ptr == 'N') {
    /* Nested name, for something like "A::f". */
    ptr = demangle_nested_name(ptr, func_block, dctl);
  } else if (*ptr == 'Z') {
    /* Local name, identifies function and entity local to the function. */
    ptr = demangle_local_name(ptr, func_block, dctl);
  } else {
    /* <unscoped-name> or <unscoped-template-name> <template-args>. */
    if (*ptr == 'S' && ptr[1] != '\0' && ptr[2] == 'I') {
      /* <substitution> in <unscoped-template-name>, because it's
         followed by the "I" beginning a <template-args>. */
      ptr = demangle_substitution(ptr, 0, CVQ_NONE,
                                  /*under_lhs_declarator=*/FALSE,
                                  /*need_trailing_space=*/FALSE,
                                  (a_const_char **)NULL,
                                  (a_const_char **)NULL,
                                  dctl);
    } else {
      /* An <unscoped-name>, possibly as the whole of an
         <unscoped-template-name>.  */
      a_const_char *start = ptr;
      ptr = demangle_unscoped_name(ptr, func_block, dctl);
      if (*ptr == 'I') {
        /* This is a template because it is followed by a template arguments
           list.  Record the template as a potential substitution. */
        record_substitutable_entity(start, subk_unscoped_template_name, 0L,
                                    /*parse_template_args=*/FALSE, dctl);
      }  /* if */
    }  /* if */
    if (*ptr == 'I') {
      /* A <template-args> list. */
      ptr = demangle_template_args(ptr, dctl);
    } else {
      /* Non-template functions do not have return types encoded. */
      func_block->no_return_type = TRUE;
    }  /* if */
  }  /* if */
  if ((options & DNO_NAME) == 0) dctl->suppress_id_output--;
  return ptr;
}  /* demangle_name */


static a_const_char *demangle_simple_id(a_const_char               *ptr,
                                        a_decode_control_block_ptr dctl)
/*
Demangle a <simple-id>:

  <simple-id> ::= <source-name> [ <template-args> ]

*/
{
  ptr = demangle_source_name(ptr, /*is_module_id=*/FALSE, dctl);
  if (!dctl->err_in_id && *ptr == 'I') {
    /* A <template-args> list is present. */
    ptr = demangle_template_args(ptr, dctl);
  }  /* if */
  return ptr;
}  /* demangle_simple_id */


static a_const_char *demangle_base_unresolved_name(
                                               a_const_char               *ptr,
                                               a_decode_control_block_ptr dctl)
/*
Demangle a <base-unresolved-name>:

  <base-unresolved-name> ::= <simple-id>
                                        # unresolved name
                         ::= on <operator-name>                         
                                        # unresolved operator-function-id
                         ::= on <operator-name> <template-args>         
                                        # unresolved operator template-id
                         ::= dn <destructor-name>                       
                                        # destructor or pseudo-destructor;
                                        # e.g. ~X or ~X<N-1>

  <destructor-name> ::= <unresolved-type>   # e.g., ~T or ~decltype(f())
                    ::= <simple-id>         # e.g., ~A<2*N>

*/
{
  int          num_operands, length;
  a_const_char *op_str, *close_str;

  if (*ptr == 'o' && ptr[1] == 'n') {
    /* Operator name. */
    ptr += 2;
    op_str = get_operator_name(ptr, &num_operands, &length, &close_str,
                               dctl);
    if (op_str == NULL) {
      bad_mangled_name(dctl);
    } else {
      ptr += length;
      write_id_str("operator ", dctl);
      if (strcmp(op_str, "cast") == 0) {
        /* A conversion operator has a type. */
        ptr = demangle_type(ptr, dctl);
      } else {
        write_id_str(op_str, dctl);
      }  /* if */
      if (!dctl->err_in_id && *ptr == 'I') {
        /* A <template-args> list is present. */
        ptr = demangle_template_args(ptr, dctl);
      }  /* if */
    }  /* if */
  } else if (*ptr == 'd' && ptr[1] == 'n') {
    /* <destructor-name> */
    ptr += 2;
    write_id_ch('~', dctl);
    if (isdigit((unsigned char)*ptr)) {
      ptr = demangle_simple_id(ptr, dctl);
    } else {
      ptr = demangle_type(ptr, dctl);
    }  /* if */
  } else {
    /* <simple-id> */
    ptr = demangle_simple_id(ptr, dctl);
  }  /* if */
  return ptr;
}  /* demangle_base_unresolved_name */


static a_const_char *demangle_unresolved_name(a_const_char               *ptr,
                                              a_decode_control_block_ptr dctl)
/*
Demangle an <unresolved-name>:

  <unresolved-name> ::= [gs] <base-unresolved-name>                     
                                # x or (with "gs") ::x
                    ::= sr <unresolved-type> <base-unresolved-name>     
                                # T::x / decltype(p)::x
                    ::= srN <unresolved-type> <unresolved-qualifier-level>+ E
                        <base-unresolved-name>
                                # T::N::x /decltype(p)::N::x
                    ::= [gs] sr <unresolved-qualifier-level>+ E 
                        <base-unresolved-name>  
                                # A::x, N::y, A<T>::z; "gs" means leading "::"

  <unresolved-type> ::= <template-param>
                    ::= <decltype>
                    ::= <substitution>

  <unresolved-qualifier-level> ::= <simple-id>

Note that the "gs" may already have been stripped by the caller (since it
can also appear at the <expression> level).
*/
{
  a_boolean    gpp_qualified_name = FALSE;

  if (*ptr == 'g' && ptr[1] == 's') {
    /* Global scope: "::". */
    write_id_str("::", dctl);
    ptr += 2;
  }  /* if */
  if (*ptr == 's' && ptr[1] == 'r') {
    /* Scope resolution "::":

        ::= sr <unresolved-type> <base-unresolved-name>     
        ::= srN <unresolved-type> <unresolved-qualifier-level>+ E
            <base-unresolved-name>
        ::= [gs] sr <unresolved-qualifier-level>+ E <base-unresolved-name>  

       Differentiate between the first and third cases by looking to see if
       the character after the "sr" is numeric (in which case it must be
       an <unresolved-qualifier-level>).
       */
    ptr += 2;
    if (isdigit((unsigned char)*ptr)) {
      /* We've got this case:
         ::= [gs] sr <unresolved-qualifier-level>+ E <base-unresolved-name>  
         */
      while (!dctl->err_in_id && *ptr != 'E') {
        if (*ptr == '\0') {
          bad_mangled_name(dctl);
        } else {
          ptr = demangle_simple_id(ptr, dctl);
          write_id_str("::", dctl);
        }  /* if */
      }  /* while */
      ptr = advance_past('E', ptr, dctl);
    } else {
      if (emulate_gnu_abi_bugs) {
        /* g++ 3.2 sometimes puts out a qualified name as the second
           operand.  Look ahead to see whether that form is used.
           If so, we want to skip over the type but not output it,
           because the qualified name repeats that type. */
        a_const_char *ptr2;
        dctl->suppress_id_output++;
        dctl->suppress_substitution_recording++;
        ptr2 = demangle_type(ptr, dctl);
        dctl->suppress_id_output--;
        dctl->suppress_substitution_recording--;
        if (*ptr2 == 'N') {
          gpp_qualified_name = TRUE;
          /* Scan the type again to get substitutions recorded. */
          dctl->suppress_id_output++;
          ptr = demangle_type(ptr, dctl);
          dctl->suppress_id_output--;
        }  /* if */
      }  /* if */
      if (!gpp_qualified_name) {
        /* We've got one of these two cases:

          ::= sr <unresolved-type> <base-unresolved-name>     
          ::= srN <unresolved-type> <unresolved-qualifier-level>+ E
              <base-unresolved-name>
          */
        if (*ptr == 'N') {
          ptr = demangle_type(ptr+1, dctl);
          write_id_str("::", dctl);
          while (!dctl->err_in_id && *ptr != 'E') {
            if (*ptr == '\0') {
              bad_mangled_name(dctl);
            } else {
              ptr = demangle_simple_id(ptr, dctl);
              write_id_str("::", dctl);
            }  /* if */
          }  /* while */
          ptr = advance_past('E', ptr, dctl);
        } else {
          ptr = demangle_type(ptr, dctl);
          write_id_str("::", dctl);
        }  /* if */
      }  /* if */
    }  /* if */
    if (!dctl->err_in_id) {
      /* The qualifiers have been processed, now only a <base-unresolved-name>
         remains. */
      ptr = demangle_base_unresolved_name(ptr, dctl);
    }  /* if */
  } else {
    /* <base-unresolved-name> */
    ptr = demangle_base_unresolved_name(ptr, dctl);
  }  /* if */
  return ptr;
}  /* demangle_unresolved_name */


static a_const_char *demangle_call_offset(a_const_char               *ptr,
                                          a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <call_offset> and output the demangled form.  Return
a pointer to the character position following what was demangled.
A <call-offset> is used in the encoded name for a thunk for a
virtual function.  The syntax is:

  <call-offset> ::= h <nv-offset> _
                ::= v <v-offset> _
  <nv-offset> ::= <offset number> # non-virtual base override
  <v-offset>  ::= <offset number> _ <virtual offset number>
                                  # virtual base override, with vcall offset

*/
{
  long      num;
  a_boolean v_form = FALSE;

  if (*ptr != 'h' && *ptr != 'v') {
    bad_mangled_name(dctl);
  } else {
    v_form = (*ptr == 'v');
    write_id_str("(offset ", dctl);
    ptr = get_number(ptr+1, &num, dctl);
    write_id_signed_number(num, dctl);
    if (v_form) {
      write_id_str(", virtual offset ", dctl);
      ptr = advance_past_underscore(ptr, dctl);
      ptr = get_number(ptr, &num, dctl);
      write_id_signed_number(num, dctl);
    }  /* if */
    ptr = advance_past_underscore(ptr, dctl);
    write_id_str(") ", dctl);
  }  /* if */
  return ptr;
}  /* demangle_call_offset */


static a_const_char *demangle_special_name(a_const_char               *ptr,
                                           a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <special-name> and output the demangled form.  Return
a pointer to the character position following what was demangled.
Special names are used for generated things like virtual function tables.
The syntax is:

  <special-name> ::= TV <type>  # virtual table
                 ::= TT <type>  # VTT structure (construction vtable index)
                 ::= TI <type>  # typeinfo structure
                 ::= TS <type>  # typeinfo name (null-terminated byte string)
                 ::= GV <object name> # Guard variable for one-time init
                 ::= TW <object name> # Thread-local wrapper
                 ::= TH <object name> # Thread-local initialization
                 ::= T <call-offset> <base encoding>
                      # base is the nominal target function of thunk
                 ::= Tc <call-offset> <call-offset> <base encoding>
                      # base is the nominal target function of thunk
                      # first call-offset is 'this' adjustment
                      # second call-offset is result adjustment

*/
{
  a_func_block func_block;

  if (*ptr == 'G') {
    if (ptr[1] == 'V') {
      /* Guard variable, GV <object name>. */
      write_id_str("Initialization guard variable for ", dctl);
      ptr = demangle_name(ptr+2, &func_block, /*options=*/DNO_ALL, dctl);
    } else {
      bad_mangled_name(dctl);
    }  /* if */
  } else if (*ptr == 'T') {
    if (ptr[1] == 'V') {
      /* Virtual table, TV <type>. */
      write_id_str("Virtual function table for ", dctl);
      ptr = demangle_type(ptr+2, dctl);
    } else if (ptr[1] == 'T') {
      /* Virtual table table, TT <type>. */
      write_id_str("Virtual table table for ", dctl);
      ptr = demangle_type(ptr+2, dctl);
    } else if (ptr[1] == 'I') {
      /* Typeinfo, TI <type>. */
      write_id_str("Typeinfo for ", dctl);
      ptr = demangle_type(ptr+2, dctl);
    } else if (ptr[1] == 'S') {
      /* Typeinfo name, TS <type>. */
      write_id_str("Typeinfo name for ", dctl);
      ptr = demangle_type(ptr+2, dctl);
    } else if (ptr[1] == 'c') {
      /* Covariant thunk, Tc <call-offset> <call-offset> <base encoding>. */
      write_id_str("Covariant thunk for ", dctl);
      ptr = demangle_call_offset(ptr+2, dctl);
      ptr = demangle_call_offset(ptr, dctl);
      ptr = demangle_encoding(ptr, /*include_func_params=*/TRUE, dctl);
    } else if (ptr[1] == 'h' || ptr[1] == 'v') {
      /* Thunk, T <call-offset> <base encoding>. */
      write_id_str("Thunk for ", dctl);
      ptr = demangle_call_offset(ptr+1, dctl);
      ptr = demangle_encoding(ptr, /*include_func_params=*/TRUE, dctl);
    } else if (ptr[1] == 'H') {
      /* Thread-local initialization alias for <object name>. */
      write_id_str("Thread-local initialization routine for ", dctl);
      ptr = demangle_name(ptr+2, &func_block, /*options=*/DNO_ALL, dctl);
    } else if (ptr[1] == 'W') {
      /* Thread-local wrapper for <object name>. */
      write_id_str("Thread-local wrapper routine for ", dctl);
      ptr = demangle_name(ptr+2, &func_block, /*options=*/DNO_ALL, dctl);
    } else {
      bad_mangled_name(dctl);
    }  /* if */
  } else {
    bad_mangled_name(dctl);
  }  /* if */
  return ptr;
}  /* demangle_special_name */


static a_const_char *demangle_function_or_data_name(
                               a_const_char               *ptr,
                               a_boolean                  include_func_params,
                               a_boolean                  first_scan,
                               a_decode_control_block_ptr dctl)
/*
Demangle selected pieces of an IA-64 <function name><bare-function-type> or
<data name> and output the demangled form.  Return a pointer to the character
position following what was demangled.  Do not output function parameters if
include_func_params is FALSE.  This demangling occurs in two passes, on the
first scan (when first_scan is TRUE), only the return type of the template
function is emitted; on the second scan (when first_scan is FALSE) the
remainder of the demangling is produced.  Only template functions require
two scans, but since it's not known ahead of time if a template function
is being demangled, all names are subject to the two pass method (with only
the optional externalization information being emitted on the first pass for
non-template functions).
*/
{
  a_func_block                func_block;
  a_bare_function_type_option bft_option;
  a_demangle_name_option      dno_option;

  /* This routine is invoked in two passes, and in turn invokes
     demangle_name and demangle_bare_function_type to emit various pieces
     of the name at various times.  For example for this mangled name:

     _ZB19_7_x4280_C_a9e5c9ef4sft7IiEiT_

     the demangled name along with the pass in which each element is
     emitted and the option that controls its output is:

     [static from x4280_C] int sft7<int>(T1)
                                   ^^^^^---- second pass, BFT_PARAMS
                               ^^^^--------- second pass, DNO_NAME
                           ^^^^------------- first pass, BFT_RETURN
     ^^^^^^^^^^^^^^^^^^^^^^----------------- first pass, DNO_EXTERNALIZATION
     */
  if (first_scan) {
    /* The return type of a template function (if present) and 
       externalization information (if present) is emitted on the first
       scan.  */
    dno_option = DNO_EXTERNALIZATION;
    bft_option = BFT_RETURN;
  } else {
    /* On the second scan, emit the name and any template parameters
       (if present). */
    dno_option = DNO_NAME;
    bft_option = BFT_PARAMS;
    /* The first pass recorded all of the substitutions, so suppress
       substitution recording on the second pass. */
    dctl->suppress_substitution_recording++;
  }  /* if */
  ptr = demangle_name(ptr, &func_block, dno_option, dctl);
  /* For the first scan, generally speaking, suppress the remaining output
     (except for the call to demangle_bare_function_type below). */
  if (first_scan) dctl->suppress_id_output++;
  /* If there's more, it's the <bare-function-type>. */
  if (*ptr != '\0' && *ptr != 'E') {
    /* Q <nested-name> indicates a function that is explicitly
       overridden.  This is an extension over the IA-64 ABI spec. */
    if (*ptr == 'Q') {
      a_func_block dummy_func_block;
      write_id_str(" [overriding ", dctl);
      ptr = demangle_name(ptr+1, &dummy_func_block, dno_option, dctl);
      write_id_str("] ", dctl);
    }  /* if */
    if (first_scan) dctl->suppress_id_output--;
    if (!include_func_params) dctl->suppress_id_output++;
    ptr = demangle_bare_function_type(ptr, func_block.no_return_type, 
                                      bft_option, dctl);
    if (!include_func_params) dctl->suppress_id_output--;
    if (first_scan) dctl->suppress_id_output++;
    if (func_block.cv_quals != 0) {
      /* Put out cv-qualifiers for a member function. */
      write_id_ch(' ', dctl);
      output_cv_qualifiers(func_block.cv_quals,
                           /*trailing_space=*/FALSE, dctl);
    }  /* if */
    if (func_block.ref_qual != REFQ_NONE) {
      /* Put out ref-qualifier for a member function. */
      write_id_ch(' ', dctl);
      output_ref_qualifier(func_block.ref_qual, dctl);
    }  /* if */
  }  /* if */
  if (func_block.ctor_dtor_kind != ' ') {
    /* Identify the kind of constructor or destructor if necessary. */
    switch (func_block.ctor_dtor_kind) {
      case '0':
        write_id_str(" [deleting]", dctl);
        break;
      case '1':
        /* Complete constructor or destructor gets no extra label. */
        break;
      case '2':
        write_id_str(" [subobject]", dctl);
        break;
      case '3':
        /* Not generated by the EDG front end, but part of the ABI spec. */
        write_id_str(" [allocating]", dctl);
        break;
      case '7':
        /* An EDG extension for C++/CLI finalizers (no extra label). */
        break;
      case '8':
        /* An EDG extension for C++/CLI static constructors. */
        write_id_str(" [static]", dctl);
        break;
      case '9':
        /* The EDG front end uses "C9" for the common constructor routine
           called by delegating constructor alternate entry points, and
           "D9" for (corresponding) delegation destructors. */
        write_id_str(" [delegation]", dctl);
        break;
      default:
        /* Bad character.  This shouldn't happen, because the character
           was checked earlier. */
        bad_mangled_name(dctl);
    }  /* switch */
  }  /* if */
  if (first_scan) {
    dctl->suppress_id_output--;
  } else {
    dctl->suppress_substitution_recording--;
  }  /* if */
  return ptr;
}  /* demangle_function_or_data_name */


static a_const_char *demangle_encoding(
                                a_const_char               *ptr,
                                a_boolean                  include_func_params,
                                a_decode_control_block_ptr dctl)
/*
Demangle an IA-64 <encoding> and output the demangled form.  Return
a pointer to the character position following what was demangled.
<encoding> is almost the top-level term in the grammar; it's what
follows the initial "_Z" in a mangled name.  The syntax is:

    <encoding> ::= <function name> <bare-function-type>
               ::= <data name>
               ::= <special-name>

Do not output function parameters if include_func_params is FALSE.
*/
{
  /* Special names begin with "T" (e.g., TV for a virtual function table)
     or "GV" for a guard variable. */
  if (*ptr == 'T' || (*ptr == 'G' && ptr[1] == 'V')) {
    ptr = demangle_special_name(ptr, dctl);
  } else {
    /* Function or data name. */
    /* Scan the <function name> or <data name> twice in order to emit a
       potential return type for a template function (emitted in the first
       scan) before the name of the template function (second scan). */
    (void)demangle_function_or_data_name(ptr, include_func_params,
                                        /*first_scan=*/TRUE, dctl);
    if (!dctl->err_in_id) {
      ptr = demangle_function_or_data_name(ptr, include_func_params,
                                           /*first_scan=*/FALSE, dctl);
    }  /* if */
  }  /* if */
  return ptr;
}  /* demangle_encoding */


static void init_demangle_state(char                       *output_buffer,
                                sizeof_t                   output_buffer_size,
                                a_decode_control_block_ptr dctl)
/*
Utility to set the state of the demangler to its initial values.
*/
{
  clear_control_block(dctl);
  dctl->output_id = output_buffer;
  dctl->output_id_size = output_buffer_size;
  num_substitutions = 0;
}  /* init_demangle_state */


/* Make sure that decode_identifier doesn't collide with symbols in user
   programs when being compiled as part of lib_src. */
#if COMPILE_DECODE_FOR_LIB_SRC
static
#endif /* COMPILE_DECODE_FOR_LIB_SRC */
void decode_identifier(a_const_char *id,
                       char         *output_buffer,
                       sizeof_t     output_buffer_size,
                       a_boolean    *err,
                       a_boolean    *buffer_overflow_err,
                       sizeof_t     *required_buffer_size)
/*
Demangle the identifier id (which is null-terminated), and put the demangled
form (null-terminated) into the output_buffer provided by the caller.
A name that does not begin with the "_Z" indicating an external name is
demangled as a type name (see the ABI description of __cxa_demangle).
output_buffer_size gives the allocated size of output_buffer.  If there
is some error in the demangling process, *err will be returned TRUE.
In addition, if the error is that the output buffer is too small,
*buffer_overflow_err will (also) be returned TRUE, and *required_buffer_size
is set to the size of buffer required to do the demangling.  Note that
if the mangled name is compressed, and the buffer size is smaller than
the size of the uncompressed mangled name, the size returned will be
enough to uncompress the name but not enough to produce the demangled form.
The caller must be prepared in that case to loop a second time (the
length returned the second time will be correct).
*/
{
  a_const_char               *end_ptr;
  a_decode_control_block     control_block;
  a_decode_control_block_ptr dctl = &control_block;

  init_demangle_state(output_buffer, output_buffer_size, dctl);
  {
    /* Determine whether host is little-endian or big-endian. */
    int i = 1;
    host_little_endian = (*(char *)&i) == 1;
  }
  for (;;) {
    if (start_of_id_is("_Z", id)) {
      /* A mangled name, beginning with "_Z". */
      end_ptr = demangle_encoding(id+2, /*include_func_params=*/TRUE, dctl);
    } else {
      /* A non-external name, assumed to be a mangled type name. */
      end_ptr = demangle_type(id, dctl);
    }  /* if */
    if (dctl->err_in_id &&
        dctl->contains_conversion_operator &&
        !dctl->parse_template_args_after_conversion_operator) {
      /* If demangling failed and the mangled name contained a conversion
         operator (i.e., "cv <type>"), retry the demangling operation, but
         this time, parse any template args that may appear after a
         templated conversion operator.  This needed because of a demangling
         ambiguity that exists for templated conversion operators. */
      init_demangle_state(output_buffer, output_buffer_size, dctl);
      dctl->parse_template_args_after_conversion_operator = TRUE;
    } else {
      break;
    }  /* if */
  }  /* for */
  if (dctl->output_overflow_err) {
    dctl->err_in_id = TRUE;
  } else {
    /* Add a terminating null. */
    dctl->output_id[dctl->output_id_len] = 0;
  }  /* if */
  /* Make sure the whole identifier was taken. */
  if (!dctl->err_in_id && end_ptr != NULL && *end_ptr != '\0') {
    bad_mangled_name(dctl);
  }  /* if */
  *err = dctl->err_in_id;
  *buffer_overflow_err = dctl->output_overflow_err;
  *required_buffer_size = dctl->output_id_len + 1; /* +1 for final null. */
}  /* decode_identifier */


/*
Result status codes used by __cxa_demangle.
*/
#define CXA_DEMANGLE_SUCCESS		 0
#define CXA_DEMANGLE_ALLOC_FAILURE	-1
#define CXA_DEMANGLE_INVALID_NAME	-2
#define CXA_DEMANGLE_INVALID_ARGUMENTS	-3


#if COMPILE_DECODE_FOR_LIB_SRC && defined(__EDG_RUNTIME_USES_NAMESPACES)
namespace __cxxabiv1 {
#endif /* COMPILE_DECODE_FOR_LIB_SRC&&defined(__EDG_RUNTIME_USES_NAMESPACES) */

EXTERN_C char *__cxa_demangle(char		*mangled_name,
			      char		*user_buffer,
			      true_size_t	*user_buffer_size,
			      int		*status)
/*
Demangling library interface specified by the IA-64 ABI. "mangled_name"
is the name to be demangled.  "user_buffer" is the buffer into which the
demangled name should be placed.  "user_buffer_size" is the size of
"user_buffer".  If "user_buffer" is NULL or is too small, it is reallocated
and "user_buffer_size" is set to the new size.
*/
{
#define TEMP_BUFFER_SIZE 256
  int		result_status = CXA_DEMANGLE_SUCCESS;
  char		temp_buffer[TEMP_BUFFER_SIZE];
  char		*buf_to_use = NULL;
  a_boolean	temp_buffer_used = FALSE;
  sizeof_t	buf_size = 0;

  if (user_buffer != NULL && user_buffer_size == NULL) {
    /* A buffer was provided but its size is not specified. */
    result_status = CXA_DEMANGLE_INVALID_ARGUMENTS;
  } else {
    /* Demangle the name. */
    a_boolean	err;
    a_boolean	buffer_overflow_err;
    sizeof_t	required_buffer_size;
    /* If no buffer was provided by the caller, try using temp_buffer. */
    if (user_buffer == NULL) {
      buf_to_use = temp_buffer;
      temp_buffer_used = TRUE;
      buf_size = TEMP_BUFFER_SIZE;
    } else {
      buf_to_use = user_buffer;
      buf_size = *user_buffer_size;
    }  /* if */
    do {
      decode_identifier(mangled_name, buf_to_use, buf_size, &err,
                        &buffer_overflow_err, &required_buffer_size);
      if (buffer_overflow_err) {
        /* The buffer was too small.  Allocate a new buffer. */
        if (temp_buffer_used || buf_to_use == user_buffer) {
          /* We previously used a local buffer or we used the buffer
             supplied by the user.  Allocate a new one.  Note that we
             don't free the user buffer yet because an error might still
             occur and we can only provide the new buffer address in cases
             where we return successfully. */
          buf_to_use = (char*)malloc((true_size_t)required_buffer_size);
          temp_buffer_used = FALSE;
        } else {
          /* We are using a user-buffer.  Reallocate that buffer. */
          buf_to_use = (char*)realloc(buf_to_use, 
                                      (true_size_t)required_buffer_size);
        }  /* if */
        buf_size = required_buffer_size;
        if (buf_to_use == NULL) {
          /* The allocation failed. */
          result_status = CXA_DEMANGLE_ALLOC_FAILURE;
        }  /* if */
      } else if (err) {
        /* A name decoding error occurred. */
        result_status = CXA_DEMANGLE_INVALID_NAME;
      }  /* if */
      /* Continue looping until decode_identifier succeeds.  If an error
         was detected, terminate the loop. */
    } while (err && result_status == CXA_DEMANGLE_SUCCESS);
    if (result_status == CXA_DEMANGLE_SUCCESS && temp_buffer_used) {
      /* The temporary buffer was used.  Copy the result to a dynamically
         allocated buffer. */
      true_size_t	size;
      size = strlen(temp_buffer) + 1;
      buf_to_use = (char*)malloc(size);
      if (buf_to_use == NULL) {
        result_status = CXA_DEMANGLE_ALLOC_FAILURE;
      } else {
        (void)strcpy(buf_to_use, temp_buffer);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Return the status to the caller. */
  if (status != NULL) *status = result_status;
  /* Return NULL if there was an error. */
  if (result_status != CXA_DEMANGLE_SUCCESS) {
    /* If the buffer being used was allocated above, free it now. */
    if (!temp_buffer_used &&
        user_buffer != NULL && buf_to_use != user_buffer) {
       free(buf_to_use);
    }  /* if */
    buf_to_use = NULL;
  } else {
    /* The demangling was successful. */
    /* If the buffer being returned is not the buffer supplied by the
       user, free the user buffer. */
    if (user_buffer != NULL && buf_to_use != user_buffer) {
      free(user_buffer);
      /* Update the size parameter passed in. */
      if (user_buffer_size != NULL) {
        *user_buffer_size = buf_size;
      }  /* if */
    }  /* if */
  }  /* if */
  return buf_to_use;
#undef TEMP_BUFFER_SIZE
}  /* __cxa_demangle */

#if COMPILE_DECODE_FOR_LIB_SRC && defined(__EDG_RUNTIME_USES_NAMESPACES)
}  /* namespace __cxxabiv1 */
#endif /* COMPILE_DECODE_FOR_LIB_SRC&&defined(__EDG_RUNTIME_USES_NAMESPACES) */

#endif /* !IA64_ABI */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1996-2018 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
