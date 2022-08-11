#include "liblfortran.h"

extern gfc_charlen_type string_index (gfc_charlen_type, const char *,
				      gfc_charlen_type, const char *,
				      GFC_LOGICAL_4);
export_proto(string_index);

extern int specific__index_1_i4(char *a, char *b, gfc_charlen_type a_len, gfc_charlen_type b_len);
export_proto(specific__index_1_i4);

int specific__index_1_i4(char *a, char *b, gfc_charlen_type a_len, gfc_charlen_type b_len)
{
	return string_index (a_len, a, b_len, b, 0);
}

extern long int specific__index_1_i8(char *a, char *b, gfc_charlen_type a_len, gfc_charlen_type b_len);
export_proto(specific__index_1_i8);

long int specific__index_1_i8(char *a, char *b, gfc_charlen_type a_len,  gfc_charlen_type b_len)
{
	return (long int)string_index (a_len, a, b_len, b, 0);
}
