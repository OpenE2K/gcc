/* Handling of compile-time options that influence the library.
   Copyright (C) 2005-2017 Free Software Foundation, Inc.

This file is part of the GNU Fortran runtime library (libgfortran).

Libgfortran is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

Libgfortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#include "liblfortran.h"
#include <signal.h>
#include <string.h>
#include <assert.h>

/* Useful compile-time options will be stored in here.  */
compile_options_t compile_options;

#ifndef LIBGFOR_MINIMAL
static volatile sig_atomic_t fatal_error_in_progress = 0;


/* Helper function for backtrace_handler to write information about the
   received signal to stderr before actually giving the backtrace.  */
static void
show_signal (int signum)
{
  const char * name = NULL, * desc = NULL;

  switch (signum)
    {
#if defined(SIGQUIT)
      case SIGQUIT:
	name = "SIGQUIT";
	desc = "Terminal quit signal";
	break;
#endif

      /* The following 4 signals are defined by C89.  */
      case SIGILL:
	name = "SIGILL";
	desc = "Illegal instruction";
	break;

      case SIGABRT:
	name = "SIGABRT";
	desc = "Process abort signal";
	break;

      case SIGFPE:
	name = "SIGFPE";
	desc = "Floating-point exception - erroneous arithmetic operation";
	break;

      case SIGSEGV:
	name = "SIGSEGV";
	desc = "Segmentation fault - invalid memory reference";
	break;

#if defined(SIGBUS)
      case SIGBUS:
	name = "SIGBUS";
	desc = "Access to an undefined portion of a memory object";
	break;
#endif

#if defined(SIGSYS)
      case SIGSYS:
	name = "SIGSYS";
	desc = "Bad system call";
	break;
#endif

#if defined(SIGTRAP)
      case SIGTRAP:
	name = "SIGTRAP";
	desc = "Trace/breakpoint trap";
	break;
#endif

#if defined(SIGXCPU)
      case SIGXCPU:
	name = "SIGXCPU";
	desc = "CPU time limit exceeded";
	break;
#endif

#if defined(SIGXFSZ)
      case SIGXFSZ:
	name = "SIGXFSZ";
	desc = "File size limit exceeded";
	break;
#endif
    }

  if (name)
    st_printf ("\nProgram received signal %s: %s.\n", name, desc);
  else
    st_printf ("\nProgram received signal %d.\n", signum);
}


/* A signal handler to allow us to output a backtrace.  */
void
backtrace_handler (int signum)
{
  /* Since this handler is established for more than one kind of signal, 
     it might still get invoked recursively by delivery of some other kind
     of signal.  Use a static variable to keep track of that. */
  if (fatal_error_in_progress)
    raise (signum);
  fatal_error_in_progress = 1;

  show_signal (signum);
  estr_write ("\nBacktrace for this error:\n");
  show_backtrace (true);

  /* Now reraise the signal.  We reactivate the signal's
     default handling, which is to terminate the process.
     We could just call exit or abort,
     but reraising the signal sets the return status
     from the process correctly. */
  signal (signum, SIG_DFL);
  raise (signum);
}
#endif

/* Set the usual compile-time options.  */
extern void set_options (int , int []);
export_proto(set_options);

void
set_options (int num, int options[])
{
  if (num >= 1)
    compile_options.warn_std = options[0];
  if (num >= 2)
    compile_options.allow_std = options[1];
  if (num >= 3)
    compile_options.pedantic = options[2];
  if (num >= 4)
    compile_options.backtrace = options[3];
  if (num >= 5)
    compile_options.sign_zero = options[4];
  if (num >= 6)
    compile_options.bounds_check = options[5];
  if (num >= 7)
    compile_options.fpe_summary = options[6];
  if (num >= 8)
    compile_options.lang_message = options[7];

#ifndef LIBGFOR_MINIMAL
  /* If backtrace is required, we set signal handlers on the POSIX
     2001 signals with core action.  */
  if (compile_options.backtrace)
    {
#if defined(SIGQUIT)
      signal (SIGQUIT, backtrace_handler);
#endif

      /* The following 4 signals are defined by C89.  */
      signal (SIGILL, backtrace_handler);
      signal (SIGABRT, backtrace_handler);
      signal (SIGFPE, backtrace_handler);
      signal (SIGSEGV, backtrace_handler);

#if defined(SIGBUS)
      signal (SIGBUS, backtrace_handler);
#endif

#if defined(SIGSYS)
      signal (SIGSYS, backtrace_handler);
#endif

#if defined(SIGTRAP)
      signal (SIGTRAP, backtrace_handler);
#endif

#if defined(SIGXCPU)
      signal (SIGXCPU, backtrace_handler);
#endif

#if defined(SIGXFSZ)
      signal (SIGXFSZ, backtrace_handler);
#endif
    }
#endif
	if (compile_options.lang_message > 0)
	{
		init_ru_messages();
	}

}


/* Default values for the compile-time options.  Keep in sync with
   gcc/fortran/options.c (gfc_init_options).  */
void
init_compile_options (void)
{
  compile_options.warn_std = FTN_STD_F95_DEL | FTN_STD_LEGACY;
  compile_options.allow_std = FTN_STD_F95_OBS | FTN_STD_F95_DEL
    | FTN_STD_F2003 | FTN_STD_F2008 | FTN_STD_F95 | FTN_STD_F77
    | FTN_STD_F2008_OBS | FTN_STD_GNU | FTN_STD_LEGACY;
  compile_options.pedantic = 0;
  compile_options.backtrace = 0;
  compile_options.sign_zero = 1;
  compile_options.fpe_summary = 0;
  compile_options.lang_message = 0;
}

/* Function called by the front-end to tell us the
   default for unformatted data conversion.  */

extern void set_convert (int);
export_proto (set_convert);

void
set_convert (int conv)
{
  compile_options.convert = conv;
}

extern void set_record_marker (int);
export_proto (set_record_marker);


void
set_record_marker (int val)
{

  switch(val)
    {
    case 4:
      compile_options.record_marker = sizeof (GFC_INTEGER_4);
      break;

    case 8:
      compile_options.record_marker = sizeof (GFC_INTEGER_8);
      break;

    default:
      runtime_error ("Invalid value for record marker");
      break;
    }
}

extern void set_max_subrecord_length (int);
export_proto (set_max_subrecord_length);

void set_max_subrecord_length(int val)
{
  if (val > GFC_MAX_SUBRECORD_LENGTH || val < 1)
    {
      runtime_error ("Invalid value for maximum subrecord length");
      return;
    }

  compile_options.max_subrecord_length = val;
}

//ADDED messages

/* The name of the locale encoding.  */
const char *locale_encoding = NULL;

/* Opening quotation mark for diagnostics.  */
const char *ftn_open_quote = "'";

/* Closing quotation mark for diagnostics.  */
const char *ftn_close_quote = "'";

/* The name of the locale encoding.  */
//const char *ftn_locale_encoding = NULL;

/* Whether the locale is using UTF-8.  */
bool ftn_locale_utf8 = false;


const char* mess_en_ru_koi8 [] = {
#ifdef _WIN32
#include "mess_en_ru_w.h"
#else
#include "mess_en_ru_l.h"
#endif
};

const char** mess_ru_utf8 = NULL;

static int mess_count = 0;


void init_ru_messages (void)
{
	while (*mess_en_ru_koi8[mess_count] != 0)
	{
		mess_count += 2;
	}
	mess_ru_utf8 = (const char**)calloc(mess_count / 2 + 1, sizeof(char*));

#if defined HAVE_LANGINFO_CODESET
  locale_encoding = NULL; // nl_langinfo (CODESET);
  if (locale_encoding != NULL
      && (!strcasecmp (locale_encoding, "utf-8")
	  || !strcasecmp (locale_encoding, "utf8")))
    ftn_locale_utf8 = true;
#endif

  if (!strcmp (ftn_open_quote, "`") && !strcmp (ftn_close_quote, "'"))
    {
      /* Untranslated quotes that it may be possible to replace with
	 U+2018 and U+2019; but otherwise use "'" instead of "`" as
	 opening quote.  */
      ftn_open_quote = "'";
#if defined HAVE_LANGINFO_CODESET
      if (ftn_locale_utf8)
	{
	  ftn_open_quote = "\xe2\x80\x98";
	  ftn_close_quote = "\xe2\x80\x99";
	}
#endif
    }
}

#ifndef _WIN32

/* Тип для хранения значения, соответствующее unicode'овскому коду символа */
typedef unsigned ui_UCode_t;

/* Выдрано из файла iconvdata/koi8-r.h, который образуется в процессе
 * сборки glibc */
static const ui_UCode_t ui_KOI8RToUnicodeTable[256] =
{
  [0x00] = 0x0000, [0x01] = 0x0001, [0x02] = 0x0002, [0x03] = 0x0003, [0x04] = 0x0004, [0x05] = 0x0005, [0x06] = 0x0006, [0x07] = 0x0007,
  [0x08] = 0x0008, [0x09] = 0x0009, [0x0a] = 0x000A, [0x0b] = 0x000B, [0x0c] = 0x000C, [0x0d] = 0x000D, [0x0e] = 0x000E, [0x0f] = 0x000F,
  [0x10] = 0x0010, [0x11] = 0x0011, [0x12] = 0x0012, [0x13] = 0x0013, [0x14] = 0x0014, [0x15] = 0x0015, [0x16] = 0x0016, [0x17] = 0x0017,
  [0x18] = 0x0018, [0x19] = 0x0019, [0x1a] = 0x001A, [0x1b] = 0x001B, [0x1c] = 0x001C, [0x1d] = 0x001D, [0x1e] = 0x001E, [0x1f] = 0x001F,
  [0x20] = 0x0020, [0x21] = 0x0021, [0x22] = 0x0022, [0x23] = 0x0023, [0x24] = 0x0024, [0x25] = 0x0025, [0x26] = 0x0026, [0x27] = 0x0027,
  [0x28] = 0x0028, [0x29] = 0x0029, [0x2a] = 0x002A, [0x2b] = 0x002B, [0x2c] = 0x002C, [0x2d] = 0x002D, [0x2e] = 0x002E, [0x2f] = 0x002F,
  [0x30] = 0x0030, [0x31] = 0x0031, [0x32] = 0x0032, [0x33] = 0x0033, [0x34] = 0x0034, [0x35] = 0x0035, [0x36] = 0x0036, [0x37] = 0x0037,
  [0x38] = 0x0038, [0x39] = 0x0039, [0x3a] = 0x003A, [0x3b] = 0x003B, [0x3c] = 0x003C, [0x3d] = 0x003D, [0x3e] = 0x003E, [0x3f] = 0x003F,
  [0x40] = 0x0040, [0x41] = 0x0041, [0x42] = 0x0042, [0x43] = 0x0043, [0x44] = 0x0044, [0x45] = 0x0045, [0x46] = 0x0046, [0x47] = 0x0047,
  [0x48] = 0x0048, [0x49] = 0x0049, [0x4a] = 0x004A, [0x4b] = 0x004B, [0x4c] = 0x004C, [0x4d] = 0x004D, [0x4e] = 0x004E, [0x4f] = 0x004F,
  [0x50] = 0x0050, [0x51] = 0x0051, [0x52] = 0x0052, [0x53] = 0x0053, [0x54] = 0x0054, [0x55] = 0x0055, [0x56] = 0x0056, [0x57] = 0x0057,
  [0x58] = 0x0058, [0x59] = 0x0059, [0x5a] = 0x005A, [0x5b] = 0x005B, [0x5c] = 0x005C, [0x5d] = 0x005D, [0x5e] = 0x005E, [0x5f] = 0x005F,
  [0x60] = 0x0060, [0x61] = 0x0061, [0x62] = 0x0062, [0x63] = 0x0063, [0x64] = 0x0064, [0x65] = 0x0065, [0x66] = 0x0066, [0x67] = 0x0067,
  [0x68] = 0x0068, [0x69] = 0x0069, [0x6a] = 0x006A, [0x6b] = 0x006B, [0x6c] = 0x006C, [0x6d] = 0x006D, [0x6e] = 0x006E, [0x6f] = 0x006F,
  [0x70] = 0x0070, [0x71] = 0x0071, [0x72] = 0x0072, [0x73] = 0x0073, [0x74] = 0x0074, [0x75] = 0x0075, [0x76] = 0x0076, [0x77] = 0x0077,
  [0x78] = 0x0078, [0x79] = 0x0079, [0x7a] = 0x007A, [0x7b] = 0x007B, [0x7c] = 0x007C, [0x7d] = 0x007D, [0x7e] = 0x007E, [0x7f] = 0x007F,
  [0x80] = 0x2500, [0x81] = 0x2502, [0x82] = 0x250C, [0x83] = 0x2510, [0x84] = 0x2514, [0x85] = 0x2518, [0x86] = 0x251C, [0x87] = 0x2524,
  [0x88] = 0x252C, [0x89] = 0x2534, [0x8a] = 0x253C, [0x8b] = 0x2580, [0x8c] = 0x2584, [0x8d] = 0x2588, [0x8e] = 0x258C, [0x8f] = 0x2590,
  [0x90] = 0x2591, [0x91] = 0x2592, [0x92] = 0x2593, [0x93] = 0x2320, [0x94] = 0x25A0, [0x95] = 0x2219, [0x96] = 0x221A, [0x97] = 0x2248,
  [0x98] = 0x2264, [0x99] = 0x2265, [0x9a] = 0x00A0, [0x9b] = 0x2321, [0x9c] = 0x00B0, [0x9d] = 0x00B2, [0x9e] = 0x00B7, [0x9f] = 0x00F7,
  [0xa0] = 0x2550, [0xa1] = 0x2551, [0xa2] = 0x2552, [0xa3] = 0x0451, [0xa4] = 0x2553, [0xa5] = 0x2554, [0xa6] = 0x2555, [0xa7] = 0x2556,
  [0xa8] = 0x2557, [0xa9] = 0x2558, [0xaa] = 0x2559, [0xab] = 0x255A, [0xac] = 0x255B, [0xad] = 0x255C, [0xae] = 0x255D, [0xaf] = 0x255E,
  [0xb0] = 0x255F, [0xb1] = 0x2560, [0xb2] = 0x2561, [0xb3] = 0x0401, [0xb4] = 0x2562, [0xb5] = 0x2563, [0xb6] = 0x2564, [0xb7] = 0x2565,
  [0xb8] = 0x2566, [0xb9] = 0x2567, [0xba] = 0x2568, [0xbb] = 0x2569, [0xbc] = 0x256A, [0xbd] = 0x256B, [0xbe] = 0x256C, [0xbf] = 0x00A9,
  [0xc0] = 0x044E, [0xc1] = 0x0430, [0xc2] = 0x0431, [0xc3] = 0x0446, [0xc4] = 0x0434, [0xc5] = 0x0435, [0xc6] = 0x0444, [0xc7] = 0x0433,
  [0xc8] = 0x0445, [0xc9] = 0x0438, [0xca] = 0x0439, [0xcb] = 0x043A, [0xcc] = 0x043B, [0xcd] = 0x043C, [0xce] = 0x043D, [0xcf] = 0x043E,
  [0xd0] = 0x043F, [0xd1] = 0x044F, [0xd2] = 0x0440, [0xd3] = 0x0441, [0xd4] = 0x0442, [0xd5] = 0x0443, [0xd6] = 0x0436, [0xd7] = 0x0432,
  [0xd8] = 0x044C, [0xd9] = 0x044B, [0xda] = 0x0437, [0xdb] = 0x0448, [0xdc] = 0x044D, [0xdd] = 0x0449, [0xde] = 0x0447, [0xdf] = 0x044A,
  [0xe0] = 0x042E, [0xe1] = 0x0410, [0xe2] = 0x0411, [0xe3] = 0x0426, [0xe4] = 0x0414, [0xe5] = 0x0415, [0xe6] = 0x0424, [0xe7] = 0x0413,
  [0xe8] = 0x0425, [0xe9] = 0x0418, [0xea] = 0x0419, [0xeb] = 0x041A, [0xec] = 0x041B, [0xed] = 0x041C, [0xee] = 0x041D, [0xef] = 0x041E,
  [0xf0] = 0x041F, [0xf1] = 0x042F, [0xf2] = 0x0420, [0xf3] = 0x0421, [0xf4] = 0x0422, [0xf5] = 0x0423, [0xf6] = 0x0416, [0xf7] = 0x0412,
  [0xf8] = 0x042C, [0xf9] = 0x042B, [0xfa] = 0x0417, [0xfb] = 0x0428, [0xfc] = 0x042D, [0xfd] = 0x0429, [0xfe] = 0x0427, [0xff] = 0x042A,
};

static ui_UCode_t
ui_KOI8RToUnicode( unsigned char c)
{
    ui_UCode_t res;

    assert ( c > 0 && c < sizeof( ui_KOI8RToUnicodeTable) );
    res = ui_KOI8RToUnicodeTable[c];
    assert ( res != 0 );

    return res;
} /* ui_KOI8RToUnicode */

/**
 * Преобразование кода Unicode в последовательность байт UTF-8
 * По сути дела копия edg'шной функции unicode_to_utf8, чтобы в данноме
 * месте не вызывать коды из edg
 */
static int
ui_UnicodeToUTF8( ui_UCode_t uc,
                  char chars[4])
{
  int len;

  if (uc <= 0x7f) {
    /* One byte of UTF-8 is needed. */
    len = 1;
    chars[0] = (char)uc;
  } else if (uc <= 0x7ff) {
    /* Two bytes of UTF-8 are needed. */
    len = 2;
    chars[0] = (char)((uc >> 6) | 0xc0);
    chars[1] = (char)((uc & 0x3f) | 0x80);
  } else if (uc <= 0xffff) {
    /* Three bytes of UTF-8 are needed. */
    len = 3;
    chars[0] = (char)((uc >> 12) | 0xe0);
    chars[1] = (char)(((uc >> 6) & 0x3f) | 0x80);
    chars[2] = (char)((uc & 0x3f) | 0x80);
  } else {
    /* Four bytes of UTF-8 are needed. */
    len = 4;
    chars[0] = (char)(((uc >> 18) & 0x7) | 0xf0);
    chars[1] = (char)(((uc >> 12) & 0x3f) | 0x80);
    chars[2] = (char)(((uc >> 6) & 0x3f) | 0x80);
    chars[3] = (char)((uc & 0x3f) | 0x80);
  }  /* if */
  return len;
}  /* unicode_to_utf8 */

/* ------------------------------------------------------------------------- */

/**
 * Конвертация одного символа из KOI8-R (т.к. в исходниках компилятора все
 * сообщения написаны в KOI8-R) в UTF-8
 */
const char*
ui_ConvertSymbolFromKOI8RToUTF8( char c)
{
    unsigned char koi8r_code = (unsigned char)c;
    ui_UCode_t ucode;
    int len;
    static char res_str[8]; /* в UTF-8 одна последовательность занимает максимум 5 байт */

    ucode = ui_KOI8RToUnicode( koi8r_code);
    len = ui_UnicodeToUTF8( ucode, res_str);
    assert ( len < (int)sizeof( res_str) ); /* bug118407 - комментарий пока не удалять */
    res_str[len] = '\0';

    return res_str;
} /* ui_ConvertSymbolFromKOI8RToUTF8 */

/**
 * Конвертация строки из KOI8-R (т.к. в исходниках компилятора все сообщения
 * написаны в KOI8-R) в UTF-8
 */
const char*
ui_ConvertStringFromKOI8RToUTF8( const char *str)
{
    const char *p;
    char *buf;
    unsigned long len;

    /* Вычисляем размер буфера, необходимый для хранения строки в UTF-8 */
    len = 1;
    for ( p = str; *p != '\0'; p++ )
    {
        len += strlen( ui_ConvertSymbolFromKOI8RToUTF8( *p));
    }

    /* Выделяем память под строку в UTF-8
     * Тут будет утечка памяти, но с edg по другому работать уж слишком
     * проблематично */
    buf = calloc(len, sizeof(char));

    /* Заполняем строку */
    *buf = '\0';
    for ( p = str; *p != '\0'; p++ )
    {
        strcat( buf, ui_ConvertSymbolFromKOI8RToUTF8( *p));
        assert ( strlen( buf) < len );
    }

    return buf;
} /* ui_ConvertStringFromKOI8RToUTF8 */

#endif

const char* get_mess_text(const char* mess)
{
#if !defined _WIN32
	if (compile_options.lang_message == LOCALE_ENG)
		return mess;
#endif
    const char **p = mess_en_ru_koi8;
	int n1, t, n = mess_count / 2;
	do 
	{
		n1 = n >> 1;
		if (!(t = strcmp(mess, p[n1 << 1])))
        {
#if defined _WIN32
	        if (compile_options.lang_message == LOCALE_ENG)
		        return mess;
#endif
            int i = p - mess_en_ru_koi8 + (n1 << 1);
		    if (compile_options.lang_message == LOCALE_RU_KOI8)
				return  mess_en_ru_koi8[i+1];
#ifndef _WIN32
            else if (mess_ru_utf8[i/2] == NULL) /* utf8 */
				mess_ru_utf8[i/2] = ui_ConvertStringFromKOI8RToUTF8(mess_en_ru_koi8[i+1]);
#endif
			return mess_ru_utf8[i/2];
        }
        if (t < 0)
			n = n1;
		else 
		{
			n -= ++n1;
			p += n1 * 2;
		}
	}
	while(n > 0);
 
	return mess;
}


