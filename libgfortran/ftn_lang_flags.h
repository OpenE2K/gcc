#pragma once

/* this file must be the same as in liblfortran/liblfortran (for fixing Bug 654) */

/* Flags to specify which standard/extension contains a feature.
Note that no features were obsoleted nor deleted in F2003.
Please remember to keep those definitions in sync with
gfortran.texi.  */
/* For now, use F2015 = FTN_STD_GNU.  */
#define FTN_STD_F2015	    (1<<5)	/* PLACEHOLDER for Fortran 2015.  */
#define FTN_STD_F2008_TS	(1<<9)	/* POST-F2008 technical reports.  */
#define FTN_STD_F2008_OBS	(1<<8)	/* Obsolescent in F2008.  */
#define FTN_STD_F2008		(1<<7)	/* New in F2008.  */
#define FTN_STD_LEGACY		(1<<6)	/* Backward compatibility.  */
#define FTN_STD_GNU         (1<<5)	/* GNU Fortran extension.  */
#define FTN_STD_F2003		(1<<4)	/* New in F2003.  */
#define FTN_STD_F95		    (1<<3)	/* New in F95.  */
#define FTN_STD_F95_DEL		(1<<2)	/* Deleted in F95.  */
#define FTN_STD_F95_OBS		(1<<1)	/* Obsolescent in F95.  */
#define FTN_STD_F77		    (1<<0)	/* Included in F77, but not deleted or obsolescent in later standards.  */
