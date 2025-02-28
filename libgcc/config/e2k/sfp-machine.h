#define _FP_W_TYPE_SIZE         64
#define _FP_W_TYPE              unsigned long long
#define _FP_WS_TYPE             signed long long
#define _FP_I_TYPE              long long

typedef int TItype __attribute__ ((mode (TI)));
typedef unsigned int UTItype __attribute__ ((mode (TI)));

#define TI_BITS (__CHAR_BIT__ * (int)sizeof(TItype))

/* The type of the result of a floating point comparison.  This must
   match `__libgcc_cmp_return__' in GCC for the target.  */
typedef int __gcc_CMPtype __attribute__ ((mode (__libgcc_cmp_return__)));
#define CMPtype __gcc_CMPtype

#define _FP_MUL_MEAT_Q(R,X,Y)                           \
  _FP_MUL_MEAT_2_wide(_FP_WFRACBITS_Q,R,X,Y,umul_ppmm)

#define _FP_DIV_MEAT_Q(R,X,Y)   _FP_DIV_MEAT_2_udiv(Q,R,X,Y)

#define _FP_NANFRAC_S           _FP_QNANBIT_S
#define _FP_NANFRAC_D           _FP_QNANBIT_D
#define _FP_NANFRAC_E           _FP_QNANBIT_E, 0
#define _FP_NANFRAC_Q           _FP_QNANBIT_Q, 0

#define _FP_KEEPNANFRACP        1
#define _FP_QNANNEGATEDP        0

#define _FP_NANSIGN_S           1
#define _FP_NANSIGN_D           1
#define _FP_NANSIGN_E           1
#define _FP_NANSIGN_Q           1

/* Here is something Intel misdesigned: the specs don't define
   the case where we have two NaNs with same mantissas, but
   different sign. Different operations pick up different NaNs.  */
#define _FP_CHOOSENAN(fs, wc, R, X, Y, OP)                      \
  do {                                                          \
    if (_FP_FRAC_GT_##wc(X, Y)                                  \
        || (_FP_FRAC_EQ_##wc(X,Y) && (OP == '+' || OP == '*'))) \
      {                                                         \
        R##_s = X##_s;                                          \
        _FP_FRAC_COPY_##wc(R,X);                                \
      }                                                         \
    else                                                        \
      {                                                         \
        R##_s = Y##_s;                                          \
        _FP_FRAC_COPY_##wc(R,Y);                                \
      }                                                         \
    R##_c = FP_CLS_NAN;                                         \
  } while (0)

#define _FP_TININESS_AFTER_ROUNDING 1

#define FP_EX_INVALID           0x01
#define FP_EX_DENORM            0x02
#define FP_EX_DIVZERO           0x04
#define FP_EX_OVERFLOW          0x08
#define FP_EX_UNDERFLOW         0x10
#define FP_EX_INEXACT           0x20
#define FP_EX_ALL \
        (FP_EX_INVALID | FP_EX_DENORM | FP_EX_DIVZERO | FP_EX_OVERFLOW \
         | FP_EX_UNDERFLOW | FP_EX_INEXACT)

void __sfp_handle_exceptions (int);

#define FP_HANDLE_EXCEPTIONS                    \
  do {                                          \
    if (__builtin_expect (_fex, 0))             \
      __sfp_handle_exceptions (_fex);           \
  } while (0)

#define FP_TRAPPING_EXCEPTIONS ((~_pfpfr >> 7) & FP_EX_ALL)

#define FP_RND_NEAREST          0x0000
#define FP_RND_ZERO             0x6000
#define FP_RND_PINF             0x4000
#define FP_RND_MINF             0x2000
#define FP_RND_MASK             0x6000

#define _FP_DECL_EX \
  unsigned int _pfpfr __attribute__ ((unused)) = FP_RND_NEAREST

#define FP_INIT_ROUNDMODE \
  _Pragma ("asm_inline") __asm__ __volatile ("rrs %%pfpfr, %0" : "=r" (_pfpfr))

#define FP_ROUNDMODE (_pfpfr & FP_RND_MASK)

#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN    4321

#define __BYTE_ORDER __LITTLE_ENDIAN

/* Define ALIASNAME as a strong alias for NAME.  */
#define strong_alias(name, aliasname) _strong_alias(name, aliasname)
#define _strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name)));

#pragma diag_suppress 62 /* "shift count is negative" */
#pragma diag_suppress 63 /* "shift count is too large" */
