/* auto-host.h.  Generated from config.in by configure.  */
/* config.in.  Generated from configure.ac by autoheader.  */

/* Define if you want runtime assertions enabled. This is a cheap check. */
#define ENABLE_RUNTIME_CHECKING 1

/* Define 0/1 if your assembler supports CFI directives. */
#define HAVE_GAS_CFI_DIRECTIVE 1

/* Define 0/1 if your assembler supports .cfi_personality. */
#define HAVE_GAS_CFI_PERSONALITY_DIRECTIVE 1

/* Define 0/1 if your assembler supports .cfi_sections. */
#define HAVE_GAS_CFI_SECTIONS_DIRECTIVE 1

/* Define if your assembler and linker support .hidden. */
#define HAVE_GAS_HIDDEN 1

/* Define .init_array/.fini_array sections are available and working. */
#ifndef USED_FOR_TARGET
/* Раньше в компиляторе взводили макрос __LCC_INITFINI_ARRAY__ там,
 * где есть поддержка .init_array/.fini_array. В самом компиляторе
 * ничего дополнительного не делалось, просто была поддержка опции
 * -print-mode=initfini. Сейчас постфактум все архитектуры, где
 * нет такой поддержки, попали в категорию убогих, и для них вместо
 * libgcc используется libgcc.mcst. Т.е. внутри libgcc постфактум
 * компилятор (точнее, система программирования) всегда умеет работать
 * с этими секциями, поэтому здесь принудительно включим. Для контроля
 * поставлю ещё проверку, чтобы не забыть про этот момент, если вдруг
 * захотим перевести еще какую-нибудь убогую систему (на данный момент
 * из таковых остался лишь solaris) на нормальные рельсы.  */
# if ! defined __linux__ && ! defined __QNX__
#  error "При сборке на под другие операционные системы надо правильно настроить макрос HAVE_INITFINI_ARRAY_SUPPORT"
# endif
# if 1 /* defined __LCC_INITFINI_ARRAY__ */
#  define HAVE_INITFINI_ARRAY_SUPPORT 1
# else /* ! defined __LCC_INITFINI_ARRAY__  */
/* #undef HAVE_INITFINI_ARRAY_SUPPORT */
# endif /* ! defined __LCC_INITFINI_ARRAY__  */
#endif


/* Define if your linker supports .eh_frame_hdr. */
#define HAVE_LD_EH_FRAME_HDR 1

/* Define if your target C library provides sys/sdt.h */
/* #undef HAVE_SYS_SDT_H */

/* Define if your target C library provides the `dl_iterate_phdr' function. */
/* #undef TARGET_DL_ITERATE_PHDR */

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
#  define WORDS_BIGENDIAN 1
# endif
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
#ifndef USED_FOR_TARGET
#define _FILE_OFFSET_BITS 64
#endif


/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

