/* Implementation of the RENAME intrinsic.
   Copyright (C) 2005-2017 Free Software Foundation, Inc.
   Contributed by François-Xavier Coudert <coudert@clipper.ens.fr>

This file is part of the GNU Fortran runtime library (libgfortran).

Libgfortran is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

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

#include <errno.h>


static int
rename_internal (char *path1, char *path2, gfc_charlen_type path1_len,
		 gfc_charlen_type path2_len)
{
  char *str1 = fc_strdup (path1, path1_len);
  char *str2 = fc_strdup (path2, path2_len);
  int val = rename (str1, str2);
  free (str1);
  free (str2);
  return ((val == 0) ? 0 : errno);
}


/* SUBROUTINE RENAME(PATH1, PATH2, STATUS)
   CHARACTER(len=*), INTENT(IN) :: PATH1, PATH2
   INTEGER, INTENT(OUT), OPTIONAL :: STATUS  */

extern void rename_i4_sub (char *, char *, GFC_INTEGER_4 *, gfc_charlen_type,
	                   gfc_charlen_type);
iexport_proto(rename_i4_sub);

void
rename_i4_sub (char *path1, char *path2, GFC_INTEGER_4 *status,
	       gfc_charlen_type path1_len, gfc_charlen_type path2_len)
{
  int val = rename_internal (path1, path2, path1_len, path2_len);
  if (status != NULL) 
    *status = val;
}
iexport(rename_i4_sub);

extern void rename_i8_sub (char *, char *, GFC_INTEGER_8 *, gfc_charlen_type,
	                   gfc_charlen_type);
iexport_proto(rename_i8_sub);

void
rename_i8_sub (char *path1, char *path2, GFC_INTEGER_8 *status,
	       gfc_charlen_type path1_len, gfc_charlen_type path2_len)
{
  int val = rename_internal (path1, path2, path1_len, path2_len);
  if (status != NULL) 
    *status = val;
}
iexport(rename_i8_sub);

extern GFC_INTEGER_4 rename_i4 (char *, char *, gfc_charlen_type,
	                        gfc_charlen_type);
export_proto(rename_i4);

GFC_INTEGER_4
rename_i4 (char *path1, char *path2, gfc_charlen_type path1_len,
	   gfc_charlen_type path2_len)
{
  return rename_internal (path1, path2, path1_len, path2_len);
}

extern GFC_INTEGER_8 rename_i8 (char *, char *, gfc_charlen_type,
	                        gfc_charlen_type);
export_proto(rename_i8);

GFC_INTEGER_8
rename_i8 (char *path1, char *path2, gfc_charlen_type path1_len,
	   gfc_charlen_type path2_len)
{
  return rename_internal (path1, path2, path1_len, path2_len);
}
