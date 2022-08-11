/* DWARF2 exception handling and frame unwind runtime interface routines.
   Copyright (C) 1997-2015 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include "tconfig.h"
#include "tsystem.h"
#include "coretypes.h"
#include "tm.h"
#include "libgcc_tm.h"
#include "unwind.h"
#include "unwind-pe.h"
#include "unwind-dw2-fde.h"
#include "unwind-dw2.h"

#ifdef HAVE_SYS_SDT_H
#include <sys/sdt.h>
#endif

#if defined __linux__
# include <asm/e2k_syswork.h>
/* For SYS_{access_hw_stacks,set_backtrace} which . . .  */
# include <sys/syscall.h>
#elif defined __QNX__
# include <sys/neutrino.h>
/* . . . are not provided by QNX libc headers.  */
# define SYS_access_hw_stacks	__NR_access_hw_stacks
# define SYS_set_backtrace	__NR_set_backtrace
#else
#error "Unsupported OS"
#endif

#if ! defined (__ptr128__)
# define syshw(m, f, b, s, r)						\
  syscall (SYS_access_hw_stacks, (unsigned long) m, f, b, s, r)
#else /* defined (__ptr128__)  */
# if ! defined (__NEW_PM_SYSCALLS)
#  define syshw(m, f, b, s, r)						\
  syscall (SYS_access_hw_stacks, (((unsigned long) m) << 32) | s, f, b, r)
#else /* defined (__NEW_PM_SYSCALLS)  */
#  define syshw(m, f, b, s, r)						\
  syscall (SYS_access_hw_stacks, (void *) m, (void *) f,		\
	   (void *) b, (void *) s, (void *) r)
#endif /* defined __NEW_PM_SYSCALLS  */
#endif /* defined (__ptr128__)  */

#if ! defined (__ptr128__) || ! defined (__NEW_PM_SYSCALLS)
# define sys_set_backtrace(a, b, c, d) syscall (SYS_set_backtrace, a, b, c, d)
#else /* defined (__ptr128__) && defined (__NEW_PM_SYSCALLS)  */
# define sys_set_backtrace(a, b, c, d) syscall (SYS_set_backtrace,	\
						(void *) a, (void *) b, \
						(void *) c, (void *) d)
#endif /* defined (__ptr128__) && defined (__NEW_PM_SYSCALLS)  */

#ifndef __USING_SJLJ_EXCEPTIONS__

/* Definitions of E2K-specific register types and macros.  */

#define	E2K_VA_SIZE	48
#define	E2K_VA_MSB	(E2K_VA_SIZE - 1)

typedef	struct e2k_rusd_lo_fields {	/* Fields of lower word */
  unsigned long long base : E2K_VA_SIZE;		/* [47: 0] */
  unsigned long long unused2 : 57 - E2K_VA_MSB;	/* [57:48] */
  unsigned long long p : 1;			/* [58] */
  unsigned long long rw : 2;			/* [60:59] */
  unsigned long long unused : 3;			/* [63:61] */
} e2k_rusd_lo_fields_t;

/* Structure of lower word */
typedef	union e2k_rusd_lo_struct {
  e2k_rusd_lo_fields_t fields;		/* as USD fields */
  unsigned long long word;		/* as entire register */
} e2k_rusd_lo_struct_t;



typedef	struct e2k_rwap_lo_fields {	/* Fields of lower word */
  unsigned long long base : E2K_VA_SIZE;	/* [47: 0] */
  unsigned long long unused2 : 55 - E2K_VA_MSB; /* [55:48] */
  unsigned long long stub3 : 1;			/* [56] */
  unsigned long long stub2 : 1;			/* [57] */
  unsigned long long stub1 : 1;			/* [58] */
  unsigned long long rw : 2;			/* [60:59] */
  unsigned long long itag : 3;			/* [63:61] */
} e2k_rwap_lo_fields_t;


/* Structure of lower word */
typedef	union e2k_rwap_lo_struct {
  e2k_rwap_lo_fields_t fields;		/* as AP fields */
  unsigned long long word;		/* as entire register */
} e2k_rwap_lo_struct_t;

/* Fields of high word */
typedef	struct e2k_rwap_hi_fields {
unsigned long long curptr : 32;			/* [31: 0] */
unsigned long long size : 32;			/* [63:32] */
} e2k_rwap_hi_fields_t;

/* Structure of high word */
typedef	union e2k_rwap_hi_struct {
  e2k_rwap_hi_fields_t	fields;	/* as AP fields */
  unsigned long long word;	/* as entire register */
} e2k_rwap_hi_struct_t;

typedef e2k_rwap_lo_struct_t e2k_pcsp_lo_t;
typedef e2k_rwap_lo_struct_t e2k_psp_lo_t;
typedef e2k_rwap_hi_struct_t e2k_pcsp_hi_t;
typedef e2k_rwap_hi_struct_t e2k_psp_hi_t;
typedef e2k_rusd_lo_struct_t e2k_rusd_lo_t;
typedef	e2k_rwap_hi_struct_t e2k_rusd_hi_t;


/* Structure of cr0_hi chain reg */
typedef	struct e2k_cr0_hi_fields {
  unsigned long long unused_1 : 3;	/* [ 2: 0] 	*/
  unsigned long long ip : 45;		/* [47: 3] 	*/
  unsigned long long unused_2 : 16;	/* [48:63]	*/
} e2k_cr0_hi_fields_t;

typedef	union e2k_cr0_hi {	
  e2k_cr0_hi_fields_t fields;	/* as fields 		*/
  unsigned long long word;	/* as entire register 	*/
} e2k_cr0_hi_t;


/* Structure of cr1_lo chain reg */
typedef union e2k_cr1_lo_fields {
  struct {
    unsigned long long tr : 15;		/* [14: 0] 	*/
    unsigned long long unused1	:  1;	/* [15] 	*/
    unsigned long long ein	:  8;	/* [23:16] 	*/
    unsigned long long unused2	:  1;	/* [24] 	*/
    unsigned long long wfx	:  1;	/* [25] 	*/
    unsigned long long wpsz	:  7;	/* [32:26] 	*/
    unsigned long long wbs	:  7;	/* [39:33] 	*/
    unsigned long long cuir	: 17;	/* [56:40] 	*/
    unsigned long long psr	:  7;	/* [63:57]	*/
	};
  struct {
    unsigned long long __x1	: 57;	/* [56:0] 	*/
    unsigned long long pm	: 1;	/* [57] 	*/
    unsigned long long ie	: 1;	/* [58] 	*/
    unsigned long long sge	: 1;	/* [59] 	*/
    unsigned long long lw	: 1;	/* [60] last wish */
    unsigned long long uie	: 1;	/* [61] user interrupts enable */
    unsigned long long nmie	: 1;	/* [62] not masked interrupts enable */
    unsigned long long unmie	: 1;	/* [63] user not masked interrupts */
					/* enable */
  };
} e2k_cr1_lo_fields_t;

typedef	union e2k_cr1_lo {
  e2k_cr1_lo_fields_t	fields;	/* as fields 		*/
  unsigned long long	word;	/* as entire register 	*/
} e2k_cr1_lo_t;

typedef	union e2k_cr1_hi_fields {	/* Structure of cr1_hi chain reg */
  struct {
    unsigned long long br : 28;	/* [27: 0] 	*/
    unsigned long long unused : 7;	/* [34:28] 	*/
    unsigned long long wdbl : 1;	/* [35:35]	*/
    unsigned long long ussz : 28;	/* [63:36] 	*/
  };
} e2k_cr1_hi_fields_t;
typedef	union e2k_cr1_hi {	
  e2k_cr1_hi_fields_t fields;	/* as fields 		*/
  unsigned long long word;	/* as entire register 	*/
} e2k_cr1_hi_t;


typedef	struct e2k_mem_crstack {
  unsigned long long	cr0_lo;
  e2k_cr0_hi_t		cr0_hi;
  e2k_cr1_lo_t		cr1_lo;
  e2k_cr1_hi_t		cr1_hi;
} e2k_mem_crs_t;


#define AW(x) (x.word)
#define AS(x) (x.fields)

#define E2K_PSHTP_SIZE 12
#define PSHTP_SIGN_EXTEND(pshtp)					\
  ((unsigned long long) (((long long) (pshtp) << (64 - E2K_PSHTP_SIZE))	\
			 >> (64 - E2K_PSHTP_SIZE)))

#define E2K_PCSHTP_SIZE 11
#define PCSHTP_SIGN_EXTEND(pcshtp)					\
  ((unsigned long long) (((long long) (pcshtp) << (64 - E2K_PCSHTP_SIZE)) \
			 >> (64 - E2K_PCSHTP_SIZE)))


#define E2K_READ_STACK_REGS(pcsp_lo, pcsp_hi, psp_lo, psp_hi, pcshtp, pshtp) \
do {									\
  char tmp = 0, val;							\
  asm volatile ("1:\n"							\
		"ldb,0 0, %[tmp], %[val], mas=0x7\n"			\
		"rrd %%pcshtp, %[__pcshtp]\n"				\
		"rrd %%pcsp.lo, %[__pcsp_lo]\n"				\
		"rrd %%pcsp.hi, %[__pcsp_hi]\n"				\
		"rrd %%pshtp, %[__pshtp]\n"				\
		"rrd %%psp.lo, %[__psp_lo]\n"				\
		"rrd %%psp.hi, %[__psp_hi]\n"				\
		"{\n"							\
		"  stb,2 0, %[tmp], %[val], mas = 0x2\n"		\
		"  ibranch 1b ? %%MLOCK\n"				\
		"}\n"							\
		: [val] "=&r" (val),					\
		  [__pcsp_lo] "=&r" (pcsp_lo),				\
		  [__pcsp_hi] "=&r" (pcsp_hi),				\
		  [__psp_lo] "=&r" (psp_lo),				\
		  [__psp_hi] "=&r" (psp_hi),				\
		  [__pcshtp] "=&r" (pcshtp),				\
		  [__pshtp] "=&r" (pshtp)				\
		: [tmp] "r" (&tmp));					\
 } while (0)


#define	EXT_QREG_SZ	32

#if ! defined (__ptr128__)
typedef void * typeof_ra;
#else /* defined (__ptr128__)  */
typedef _Unwind_Ptr typeof_ra;
#endif /* defined (__ptr128__)  */

/* The length of chain stack cache allowing us not to make access_hw_stacks
   syscall for each unwound frame.  */
#define CHAIN_STACK_CACHE_LEN   32

/* This is the register and unwind state for a particular frame.  This
   provides the information necessary to unwind up past a frame and return
   to its caller.  */
struct _Unwind_Context
{
  size_t psp_offset;
  e2k_mem_crs_t *chain_stack;
  size_t chain_stack_size;
  size_t pcsp_idx;
  unsigned long long us_base;
  typeof_ra ra;
  unsigned long long prev_ps_frame_size;
  unsigned long long prev_us_size;
  void *lsda;
  struct dwarf_eh_bases bases;
  size_t level;

  /* Which parameters have been set by _Unwind_SetGR () and . . .  */
  int set[2];
  /* . . . their values in case they have.  */
#if ! defined __ptr128__
  unsigned long long param0;
#else /* defined __ptr128__  */
  void *param0;
#endif /* defined __ptr128__  */
  unsigned long long param1;
};


/* Read unaligned data from the instruction buffer.  */

union unaligned
{
  void *p;
  unsigned u2 __attribute__ ((mode (HI)));
  unsigned u4 __attribute__ ((mode (SI)));
  unsigned u8 __attribute__ ((mode (DI)));
  signed s2 __attribute__ ((mode (HI)));
  signed s4 __attribute__ ((mode (SI)));
  signed s8 __attribute__ ((mode (DI)));
} __attribute__ ((packed));

static void uw_update_context (struct _Unwind_Context *, _Unwind_FrameState *);
static _Unwind_Reason_Code uw_frame_state_for (struct _Unwind_Context *,
					       _Unwind_FrameState *);

static inline void *
read_pointer (const void *p) { const union unaligned *up = p; return up->p; }

static inline int
read_1u (const void *p) { return *(const unsigned char *) p; }

static inline int
read_1s (const void *p) { return *(const signed char *) p; }

static inline int
read_2u (const void *p) { const union unaligned *up = p; return up->u2; }

static inline int
read_2s (const void *p) { const union unaligned *up = p; return up->s2; }

static inline unsigned int
read_4u (const void *p) { const union unaligned *up = p; return up->u4; }

static inline int
read_4s (const void *p) { const union unaligned *up = p; return up->s4; }

static inline unsigned long
read_8u (const void *p) { const union unaligned *up = p; return up->u8; }

static inline unsigned long
read_8s (const void *p) { const union unaligned *up = p; return up->s8; }

static inline _Unwind_Word
_Unwind_IsSignalFrame (struct _Unwind_Context *context
		       __attribute__ ((unused)))
{
  return 0;
}

static inline void
_Unwind_SetSignalFrame (struct _Unwind_Context *context
			__attribute__ ((unused)),
			int val __attribute__ ((unused)))
{
}


/* Get the value of register INDEX as saved in CONTEXT.  */

inline _Unwind_Word
_Unwind_GetGR (struct _Unwind_Context *context __attribute__ ((unused)),
	       int index __attribute__ ((unused)))
{
  return 0;
}

static inline void *
_Unwind_GetPtr (struct _Unwind_Context *context, int index)
{
  return (void *)(_Unwind_Ptr) _Unwind_GetGR (context, index);
}

/* Get the value of the CFA as saved in CONTEXT.  */
_Unwind_Word
_Unwind_GetCFA (struct _Unwind_Context *context)
{
  return (_Unwind_Word) (context->us_base + context->prev_us_size);
}


/* Get the value of the previous %pcsp as saved in CONTEXT.  */
_Unwind_Word
_Unwind_GetPCSP (struct _Unwind_Context *context)
{
  return (_Unwind_Word) ((context->pcsp_idx - 1) << 5);
}


/* Overwrite the saved value for register INDEX in CONTEXT with VAL.  */

inline void
_Unwind_SetGR (struct _Unwind_Context *context, int index, _Unwind_Word val)
{
  /* LCC rather naively believes that `%b[0], . . ., %b[63]' registers have
     their "gnu numbers" (have you ever heard of such a concept?) in the range
     from 64  to 127 inclusively (see `mdes_e2k_RegTable[]' in `mdes/mdes_e2k_
     reg.c'; interestingly enough `%b[64], . . ., %b[127]' have `-1' specified
     for their "gnu numbers"). Support this point of view  here.

     FIXME: e2k-linux-gcc is currently aware only of `%r0, . . ., %r15' and
     `%b[0], . . ., %b[7]' and  enumerates them from 0 to 15 and from 17 to 24
     inclusively. This is almost sure to make libgcc compiled by `{e2k-linux-gcc
     ,lcc}' and other libraries like libstdc++ making use of `_Unwind_SetGR
     (__builtin_eh_return_data_regno ())' compiled by `{lcc,e2k-linux-gcc}'
     incompatible with each other at runtime.  */

#if defined __LCC__
# define FIRST_DIRECT_REG	0
# define LAST_DIRECT_REG	63
# define FIRST_BASED_REG	64
# define LAST_BASED_REG		127
#else /* ! defined __LCC__  */
# define FIRST_DIRECT_REG	0
# define LAST_DIRECT_REG	15
# define FIRST_BASED_REG	17
# define LAST_BASED_REG		24
#endif /* ! defined __LCC__  */

#if ! defined __ptr128__
  if (index == FIRST_BASED_REG)
    {
      context->param0 = (unsigned long long) val;
      context->set[0] = 1;
    }
  else
#endif /* ! defined __ptr128__  */
  if (
#if ! defined __ptr128__
      index == FIRST_BASED_REG + 1
#else /* defined __ptr128__  */
      index == FIRST_BASED_REG + 2
#endif /* defined __ptr128__  */
      )
    {
      context->param1 = (unsigned long long) val;
      context->set[1] = 1;
    }
  else
    abort ();
}

/* Get the pointer to a register INDEX as saved in CONTEXT.  */

static inline void *
_Unwind_GetGRPtr (struct _Unwind_Context *context __attribute__ ((unused)),
		  int index __attribute__ ((unused)))
{
  return (void *) 0;
}

/* Set the pointer to a register INDEX as saved in CONTEXT.  */
#if defined (__ptr128__)
void
_Unwind_SetGRPtr (struct _Unwind_Context *context, int index, void *p)
{
  if (index == FIRST_BASED_REG)
    {
      context->param0 = p;
      context->set[0] = 1;
    }
  else
    /* Should not be used otherwise.  */
    abort ();
}
#endif /* defined (__ptr128__)  */

/* Overwrite the saved value for register INDEX in CONTEXT with VAL.  */

static inline void
_Unwind_SetGRValue (struct _Unwind_Context *context __attribute__ ((unused)),
		    int index __attribute__ ((unused)),
		    _Unwind_Word val __attribute__ ((unused)))
{
}

/* Return nonzero if register INDEX is stored by value rather than
   by reference.  */

static inline int
_Unwind_GRByValue (struct _Unwind_Context *context __attribute__ ((unused)),
		   int index __attribute__ ((unused)))
{
  return 0;
}

/* Retrieve the return address for CONTEXT.  */

inline _Unwind_Ptr
_Unwind_GetIP (struct _Unwind_Context *context)
{
  return (_Unwind_Ptr) context->ra;
}

/* Retrieve the return address and flag whether that IP is before
   or after first not yet fully executed instruction.  */

inline _Unwind_Ptr
_Unwind_GetIPInfo (struct _Unwind_Context *context,
		   int *ip_before_insn __attribute__ ((unused)))
{
  *ip_before_insn = _Unwind_IsSignalFrame (context);
  return (_Unwind_Ptr) context->ra;
}

/* Overwrite the return address for CONTEXT with VAL.  */

inline void
_Unwind_SetIP (struct _Unwind_Context *context, _Unwind_Ptr val)
{
  context->ra = (typeof_ra) val;
}

void *
_Unwind_GetLanguageSpecificData (struct _Unwind_Context *context)
{
  return context->lsda;
}

_Unwind_Ptr
_Unwind_GetRegionStart (struct _Unwind_Context *context)
{
  return (_Unwind_Ptr) context->bases.func;
}

void *
_Unwind_FindEnclosingFunction (
#ifndef __ptr128__
			       void *pc
#else
			       _Unwind_Ptr pc
#endif
			       )
{
  struct dwarf_eh_bases bases;
  const struct dwarf_fde *fde = _Unwind_Find_FDE (pc - 1, &bases);
  if (fde)
    return bases.func;
  else
    return NULL;
}

_Unwind_Ptr
_Unwind_GetDataRelBase (struct _Unwind_Context *context)
{
  return (_Unwind_Ptr) context->bases.dbase;
}

_Unwind_Ptr
_Unwind_GetTextRelBase (struct _Unwind_Context *context)
{
  return (_Unwind_Ptr) context->bases.tbase;
}

#include "md-unwind-support.h"

/* Extract any interesting information from the CIE for the translation
   unit F belongs to.  Return a pointer to the byte after the augmentation,
   or NULL if we encountered an undecipherable augmentation.  */

static const unsigned char *
extract_cie_info (const struct dwarf_cie *cie, struct _Unwind_Context *context,
		  _Unwind_FrameState *fs)
{
  const unsigned char *aug = cie->augmentation;
  const unsigned char *p = aug + strlen ((const char *)aug) + 1;
  const unsigned char *ret = NULL;
  _uleb128_t utmp;
  _sleb128_t stmp;

  /* g++ v2 "eh" has pointer immediately following augmentation string,
     so it must be handled first.  */
  if (aug[0] == 'e' && aug[1] == 'h')
    {
      fs->eh_ptr = read_pointer (p);
      p += sizeof (void *);
      aug += 2;
    }

  /* After the augmentation resp. pointer for "eh" augmentation
     follows for CIE version >= 4 address size byte and
     segment size byte.  */
  if (__builtin_expect (cie->version >= 4, 0))
    {
      if (p[0] != sizeof (void *) || p[1] != 0)
	return NULL;
      p += 2;
    }
  /* Immediately following this are the code and
     data alignment and return address column.  */
  p = read_uleb128 (p, &utmp);
  fs->code_align = (_Unwind_Word)utmp;
  p = read_sleb128 (p, &stmp);
  fs->data_align = (_Unwind_Sword)stmp;
  if (cie->version == 1)
    fs->retaddr_column = *p++;
  else
    {
      p = read_uleb128 (p, &utmp);
      fs->retaddr_column = (_Unwind_Word)utmp;
    }
  fs->lsda_encoding = DW_EH_PE_omit;

  /* If the augmentation starts with 'z', then a uleb128 immediately
     follows containing the length of the augmentation field following
     the size.  */
  if (*aug == 'z')
    {
      p = read_uleb128 (p, &utmp);
      ret = p + utmp;

      fs->saw_z = 1;
      ++aug;
    }

  /* Iterate over recognized augmentation subsequences.  */
  while (*aug != '\0')
    {
      /* "L" indicates a byte showing how the LSDA pointer is encoded.  */
      if (aug[0] == 'L')
	{
	  fs->lsda_encoding = *p++;
	  aug += 1;
	}

      /* "R" indicates a byte indicating how FDE addresses are encoded.  */
      else if (aug[0] == 'R')
	{
	  fs->fde_encoding = *p++;
	  aug += 1;
	}

      /* "P" indicates a personality routine in the CIE augmentation.  */
      else if (aug[0] == 'P')
	{
#ifndef __ptr128__
	  _Unwind_Ptr personality;
	  p = read_encoded_value (context, *p, p + 1, &personality);
	  fs->personality = (_Unwind_Personality_Fn) personality;
#else
	  p = read_encoded_ptr (context, *p, p + 1,
				(void **) (void *) &fs->personality);
#endif

	  aug += 1;
	}

      /* "S" indicates a signal frame.  */
      else if (aug[0] == 'S')
	{
	  fs->signal_frame = 1;
	  aug += 1;
	}

      /* Otherwise we have an unknown augmentation string.
	 Bail unless we saw a 'z' prefix.  */
      else
	return ret;
    }

  return ret ? ret : p;
}



/* Given the _Unwind_Context CONTEXT for a stack frame, look up the FDE for
   its caller and decode it into FS.  This function also sets the
   args_size and lsda members of CONTEXT, as they are really information
   about the caller's frame.  */

static _Unwind_Reason_Code
uw_frame_state_for (struct _Unwind_Context *context, _Unwind_FrameState *fs)
{
  const struct dwarf_fde *fde;
  const struct dwarf_cie *cie;
  const unsigned char *aug, *insn;

  memset (fs, 0, sizeof (*fs));
  context->lsda = 0;

  /* At `pcsp_idx == 1' we are almost certainly left with one or two pure C
     frames containing no unwind data. However, find out why it turns out to
     be  impossible to reach `pcsp_idx == 0' here.  */
  if (context->pcsp_idx == 1
      || context->ra == 0)
    return _URC_END_OF_STACK;

  fde = _Unwind_Find_FDE (context->ra + _Unwind_IsSignalFrame (context) - 1,
			  &context->bases);

  /* Make E2K unwinder silently skip C frames compiled without `-fexceptions'
     instead of stopping at the first of them which was the reason for
     Bug #90378. This is a significant benefit of E2K architecture which is
     going to save us the trouble of recompiling tons of C sources intended for
     use in (together with) C++ exception throwing applications.  */
  if (fde == NULL)
    return _URC_NO_REASON;


  cie = get_cie (fde);
  insn = extract_cie_info (cie, context, fs);
  if (insn == NULL)
    /* CIE contained unknown augmentation.  */
    return _URC_FATAL_PHASE1_ERROR;


  /* Locate augmentation for the fde.  */
  aug = (const unsigned char *) fde + sizeof (*fde);
  aug += 2 * size_of_encoded_value (fs->fde_encoding);
  insn = NULL;
  if (fs->saw_z)
    {
      _uleb128_t i;
      aug = read_uleb128 (aug, &i);
      insn = aug + i;
    }
  if (fs->lsda_encoding != DW_EH_PE_omit)
    {
#if ! defined (__ptr128__)
      _Unwind_Ptr lsda;
      aug = read_encoded_value (context, fs->lsda_encoding, aug, &lsda);
      context->lsda = (void *) lsda;
#else /* defined (__ptr128__)  */
      aug = read_encoded_ptr (context, fs->lsda_encoding, aug, &context->lsda);
#endif /* defined (__ptr128__)  */
    }

  return _URC_NO_REASON;
}

typedef union { _Unwind_Ptr ptr; _Unwind_Word word; } _Unwind_SpTmp;

static void
fill_in_chain_stack_fragm (struct _Unwind_Context *context)
{
  unsigned long long frames_num
    = (context->pcsp_idx < CHAIN_STACK_CACHE_LEN
       ? context->pcsp_idx
       : CHAIN_STACK_CACHE_LEN);
  size_t size = 32 * frames_num;
  unsigned long long top = 32 * (context->pcsp_idx - frames_num);

  if (syshw (E2K_READ_CHAIN_STACK_EX, &top,
	     &context->chain_stack[CHAIN_STACK_CACHE_LEN - frames_num], size,
             NULL) != 0)
    abort ();
}



/* CONTEXT describes the unwind state for a frame, and FS describes the FDE
   of its caller.  Update CONTEXT to refer to the caller as well.  Note
   that the args_size and lsda members are not updated here, but later in
   uw_frame_state_for.  */

static void
uw_update_context (struct _Unwind_Context *context,
		   _Unwind_FrameState *fs __attribute__ ((unused)))
{
  do {
    e2k_mem_crs_t crs;

    context->psp_offset -= context->prev_ps_frame_size;

    if ((context->chain_stack_size - context->pcsp_idx) % CHAIN_STACK_CACHE_LEN
        == 0)
      fill_in_chain_stack_fragm (context);

    crs = context->chain_stack[CHAIN_STACK_CACHE_LEN - 1
                               - ((context->chain_stack_size
                                   - context->pcsp_idx)
                                  % CHAIN_STACK_CACHE_LEN)];
    context->pcsp_idx--;

    context->prev_ps_frame_size = AS (crs.cr1_lo).wbs * EXT_QREG_SZ;
    context->prev_us_size = AS (crs.cr1_hi).ussz << 4;
    context->ra = (typeof_ra) (unsigned long) (AS (crs.cr0_hi).ip << 3);
  }
  while (context->ra == 0);

  ++context->level;
}

static void
uw_advance_context (struct _Unwind_Context *context, _Unwind_FrameState *fs)
{
  uw_update_context (context, fs);
}

/* This macro is separately called from `_Unwind_RaiseException ()' via `reinit
   _chain_stack_cache ()' to make chain stack cache in THIS_CONTEXT match its
   state prior to starting phase 2 as it may have been changed at phase 1 on
   behalf of CUR_CONTEXT now that it's shared between `{THIS,CUR}_CONTEXT'.
   FIXME: consider unifying it with `fill_in_chain_stack_fragm ()'.  */
#define init_chain_stack_cache(context)					\
  {									\
    size_t fragm_idx;							\
    size_t fragm_size;							\
    size_t obtained_size;						\
    unsigned long long top;						\
									\
    if (context->pcsp_idx < CHAIN_STACK_CACHE_LEN)			\
      {									\
	fragm_idx = CHAIN_STACK_CACHE_LEN - context->pcsp_idx;		\
	fragm_size = context->pcsp_idx;					\
      }									\
    else								\
      {									\
	fragm_idx = 0;							\
	fragm_size = CHAIN_STACK_CACHE_LEN;				\
      }									\
									\
  obtained_size = 32 * fragm_size;					\
									\
  top = 32 * (context->chain_stack_size - fragm_size);			\
									\
  if (syshw (E2K_READ_CHAIN_STACK_EX, &top,				\
	     &context->chain_stack[fragm_idx],				\
	     obtained_size, NULL)					\
      != 0)								\
    abort ();								\
									\
  }									\

static void __attribute__ ((noinline))
reinit_chain_stack_cache (struct _Unwind_Context *context)
{
  init_chain_stack_cache (context);
}

/* Fill in CONTEXT for top-of-stack.  The only valid registers at this
   level will be the return address and the CFA.  */
static void __attribute__((noinline))
uw_init_context (struct _Unwind_Context *context,
		 e2k_mem_crs_t *chain_stack)
{
  int i;
  e2k_rusd_lo_t usd_lo;
  e2k_rusd_hi_t usd_hi;
  typeof_ra ra;
  _Unwind_FrameState fs;
  _Unwind_Reason_Code code;
#if defined (__ptr128__) && defined SHARED
  static int syscall_lazily_resolved;
#endif /* defined (__ptr128__) && defined SHARED  */

  ra = (typeof_ra) __builtin_extract_return_addr (__builtin_return_address (0));

  memset (context, 0, sizeof (struct _Unwind_Context));

  /* I can't rely on the equivalent "standard" macro provided by <asm/e2k_api.h>
     since it has been renamed in linux-4.x (Bug #101373).  */
#define GET_STATE_REG(reg_mnemonic)		      \
  ({						      \
    register unsigned long long res;		      \
    asm volatile ("rrd \t%%" #reg_mnemonic ", %0"     \
		  : "=r" (res));		      \
    res;					      \
  })

  AW (usd_lo) = GET_STATE_REG (usd.lo);
  AW (usd_hi) = GET_STATE_REG (usd.hi);
#undef GET_STATE_REG

  context->us_base = AS (usd_lo).base - AS (usd_hi).size;

#if defined (__ptr128__) && defined SHARED
  /* Ensure that `syscall ()' has already been resolved lazily by the time of
     `syshw (E2K_GET_PROCEDURE_STACK_SIZE)' invocation below. Otherwise, one may
     get inconsistent sizes for procedure and chain stacks due to an extra call
     frame imposed by lazy binding in PM.  */
  if (! syscall_lazily_resolved)
    {
      /* Use any unimplemented syscall number here to avoid any side effects.
	 However, avoid using `__NR_restart_syscall == 0' as this results in
	 a rather weird halt or an infinite loop (?) if the thread is cancelled
	 while being within a (restartable?) system call.  */
      syscall (424);

      /* FIXME: shouldn't this be done in MT-safe way? The failure to do so may
	 probably result in redundant invocations of `syscall (0)' in the worst
	 case.  */
      syscall_lazily_resolved = 1;
    }
#endif /* defined (__ptr128__) && defined SHARED  */

  /* Determine the sizes of hardware stacks.  */
  if (syshw (E2K_GET_PROCEDURE_STACK_SIZE, NULL, NULL, 0,
	     &context->psp_offset) != 0)
    abort ();/***/

  if (syshw (E2K_GET_CHAIN_STACK_SIZE, NULL, NULL, 0,
	     &context->chain_stack_size) != 0)
    abort ();


  if ((context->chain_stack_size & 0x1f) != 0)
    abort ();

  context->chain_stack_size >>= 5;
  context->pcsp_idx = context->chain_stack_size;

#if defined __ptr128__
  /* FIXME: chain stack cache needs to be zero out because of a rather
     suspicious LSIM behaviour in PM: something irrelevant is found in
     it after E2K_READ_CHAIN_STACK_EX if it hasn't been preinitialized.  */
  memset (chain_stack, 0, CHAIN_STACK_CACHE_LEN * sizeof (e2k_mem_crs_t));
#endif /* defined __ptr128__  */
  context->chain_stack = chain_stack;
  init_chain_stack_cache (context);

  for (i = CHAIN_STACK_CACHE_LEN - 1; i >= 0 && context->pcsp_idx != 0; i--)
    {
      e2k_mem_crs_t crs;

      crs = context->chain_stack[i];

      context->ra = (typeof_ra) (unsigned long) (AS (crs.cr0_hi).ip << 3);
      context->prev_ps_frame_size = AS (crs.cr1_lo).wbs * EXT_QREG_SZ;
      context->prev_us_size = AS (crs.cr1_hi).ussz << 4;

      context->pcsp_idx--;

      if (context->ra != ra)
	context->psp_offset -= context->prev_ps_frame_size;
      else
	break;
    }

  if (i < 0 || context->pcsp_idx == 0)
    abort ();

  context->level = 0;

  code = uw_frame_state_for (context, &fs);
  gcc_assert (code == _URC_NO_REASON);

  uw_update_context (context, &fs);
}

static void _Unwind_DebugHook (void *, void *)
  __attribute__ ((__noinline__, __used__, __noclone__));

/* This function is called during unwinding.  It is intended as a hook
   for a debugger to intercept exceptions.  CFA is the CFA of the
   target frame.  HANDLER is the PC to which control will be
   transferred.  */
static void
_Unwind_DebugHook (void *cfa __attribute__ ((__unused__)),
		   void *handler __attribute__ ((__unused__)))
{
  /* We only want to use stap probes starting with v3.  Earlier
     versions added too much startup cost.  */
#if defined (HAVE_SYS_SDT_H) && defined (STAP_PROBE2) && _SDT_NOTE_TYPE >= 3
  STAP_PROBE2 (libgcc, unwind, cfa, handler);
#else
  asm ("");
#endif
}


/* Other platforms probably manage to do without `__attribute__ ((noreturn))'
   because `uw_install_context()' is implemented as a _macro_ containing
   `__builtin_eh_return ()' at the end. Without that one would obtain compiler
   warnings on returning from non-void functions in `libgcc/unwind.inc' ending
   with `uw_install_context ()' invocation.  */
static void __attribute__ ((noinline)) __attribute__ ((noreturn))
uw_install_context (struct _Unwind_Context *current __attribute__ ((unused)),
		    struct _Unwind_Context *target)
{
  size_t i;
  int res;
  int old_stack_layout;
  /* Reuse (hopefully) no longer needed `context->chain_stack' so as to avoid
     redundant stack waste.  */
#define SET_BACKTRACE_LEN (CHAIN_STACK_CACHE_LEN * sizeof (e2k_mem_crs_t) \
			   / sizeof (long))
  unsigned long *buf = (unsigned long *) target->chain_stack;
  size_t level = target->level;
  /* Skip return address of `syscall ()', i.e. return from it to this function
     (or to `syscall ()' PLT entry in PM) in a regular way.  */
  size_t extra_frames_to_skip = 1;

  /* Install register values previously set up by _Unwind_SetGR{,Ptr} ()'.  */
  old_stack_layout = (__builtin_cpu_is ("elbrus-v1")
		      || __builtin_cpu_is ("elbrus-v2")
		      || __builtin_cpu_is ("elbrus-v3")
		      || __builtin_cpu_is ("elbrus-v4"));

  for (i = 0; i < 2; i++)
    {
      unsigned long long off;
      int index;
      size_t len;
      void *p;

      if (! target->set[i])
        continue;

      p = (i == 0 ? &target->param0 : &target->param1);
#if ! defined __ptr128__
      /* Whereas in ordinary modes `%db[{0,1}]' correspond to `param{0,1}'
         respectively, . . .  */
      index = i;
#else /* defined __ptr128__  */
      /* . . . in PM these are `%qb[0]' and `%db[2]'.  */
      index = (i == 0 ? 0 : 2);
#endif /* defined __ptr128__  */

      len = (
#if defined __ptr128__
             (old_stack_layout && i == 0) ? 16
             :
#endif /* defined __ptr128__  */
             8);

      /* Take into account the layout of registers in procedure stack for
         different iset versions according to C.23.1.1.  */
      off = (target->psp_offset
             + (old_stack_layout
                ? 32 * (index >> 1) + 8 * (index & 0x1)
                : 16 * index));

      if (syshw (E2K_WRITE_PROCEDURE_STACK_EX, &off, p, len, NULL)
          != 0)
        abort ();

#if defined __ptr128__
      /* With the new procedure stack layout in use we've written only the
         lower half of `%qb[0]' so far.  */
      if (i == 0 && len == 8)
        {
          off += 16;
          if (syshw (E2K_WRITE_PROCEDURE_STACK_EX, &off,
                     (char *) p + 8, len, NULL)
              != 0)
            abort ();
        }
#endif /* defined __ptr128__  */
    }

  /* In PM a call via PLT pushes an extra "frame" on chain stack unlike ordinary
     modes. In case the address of a target function is resolved lazily during
     the call, two extra frames are pushed. However, `syscall ()' must have
     already been called by the time one finds himself here, therefore, exactly
     one extra frame should be skipped. This lets me return from `syscall ()'
     PLT entry to this function in a regular way.

     There's also an extra frame that should be skipped between syscall () and
     the actual kernel entry point in QNX.  */
#if ((defined (__ptr128__) && defined SHARED) || defined (__QNX__))
  extra_frames_to_skip += 1;
#endif /* (defined (__ptr128__) && defined SHARED) || defined (__QNX__)  */


  /* There is probably no point in returning to any of our callers, therefore
     to "return" to the caller of TARGET (i.e. TARGET->ra) allocate a buffer
     containing (LEVEL + 1) return addresses (keep in mind that 'LEVEL == 0'
     corresponds to our current context in fact, whereas 'TARGET->level' should
     be greater or equal than 1). Set the first LEVEL entries to point to a
     return instruction (i.e. RET_ADDR) and the last one to 'TARGET->ra'.  */
  for (i = 0; i < level; )
    {
      size_t j;

      if (i == 0)
	{
	  /* All -1 elements can be set during the first iteration of the outer
	     loop.  */
	  for (j = 0; (j < SET_BACKTRACE_LEN) && (j < level); j++)
	    buf[j] = -1UL;
	}
      else
	{
	  j = level - i;
	  if (j > SET_BACKTRACE_LEN)
	    j = SET_BACKTRACE_LEN;
	}

      if (j < SET_BACKTRACE_LEN)
	{
	  /* It's time that we set the trailing element in BUF[] to RA.  */
	  buf[j++] = (unsigned long) target->ra;
	}

      /* Skip `extra_frames_to_skip' frames so as to return here in a regular
	 way plus I elements set up at the preceding iterations. Note that
	 there's no point in returning to any of our callers prior to passing
	 control to `TARGET->ra', which is why all modifiable return addresses
	 except for the last one are set up to `-1'.  */
      if ((res = sys_set_backtrace (buf, j, extra_frames_to_skip + i,
				    0UL)) < 0)
	{
	  perror ("set_backtrace");
	  abort ();
	}

      i += j;
    }

  /* FIXME: currently this just lets me eliminate warnings on returning from
     `noreturn' function. For E2K the underlying builtin should be expanded to
     nothing and I don't even remember what its parameters mean . . .  */
  __builtin_eh_return (0, target->ra);
}

static inline
#if ! defined (__ptr128__)
_Unwind_Ptr
#else /* defined (__ptr128__)  */
/* The result of this function is used to set up `exc->private_2' which
   should be capable of holding `void *stop_argument' in PM in case of
   forced unwinding.  */
void *
#endif /* defined (__ptr128__)  */
uw_identify_context (struct _Unwind_Context *context)
{
  return (
#if defined (__ptr128__)
	  (void *)
#endif /* defined (__ptr128__)  */
	  _Unwind_GetPCSP (context));
}

#include "unwind.inc"

#if defined (USE_GAS_SYMVER) && defined (SHARED) && defined (USE_LIBUNWIND_EXCEPTIONS)
alias (_Unwind_Backtrace);
alias (_Unwind_DeleteException);
alias (_Unwind_FindEnclosingFunction);
alias (_Unwind_ForcedUnwind);
alias (_Unwind_GetDataRelBase);
alias (_Unwind_GetTextRelBase);
alias (_Unwind_GetCFA);
alias (_Unwind_GetGR);
alias (_Unwind_GetIP);
alias (_Unwind_GetLanguageSpecificData);
alias (_Unwind_GetRegionStart);
alias (_Unwind_RaiseException);
alias (_Unwind_Resume);
alias (_Unwind_Resume_or_Rethrow);
alias (_Unwind_SetGR);
alias (_Unwind_SetIP);
#endif

#endif /* !USING_SJLJ_EXCEPTIONS */
