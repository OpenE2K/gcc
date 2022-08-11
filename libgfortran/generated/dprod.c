#include "liblfortran.h"

extern GFC_REAL_16 specific__dprod_r16(GFC_REAL_8 *a, GFC_REAL_8 *b);
export_proto(specific__dprod_r16);

GFC_REAL_16 specific__dprod_r16(GFC_REAL_8 *a, GFC_REAL_8 *b)
{
	return (GFC_REAL_16)*a * (GFC_REAL_16)*b;
}

#if defined (HAVE_GFC_REAL_10)

extern GFC_REAL_10 specific__dprod_r10(GFC_REAL_8 *a, GFC_REAL_8 *b);
export_proto(specific__dprod_r10);

GFC_REAL_10 specific__dprod_r10(GFC_REAL_8 *a, GFC_REAL_8 *b)
{
	return (GFC_REAL_10)*a * (GFC_REAL_10)*b;
}

#endif
