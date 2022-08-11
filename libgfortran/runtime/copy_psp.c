#include "liblfortran.h"
#include <string.h>

/*Functions below were written for work with passports, bug N 3234*/

extern void copy_arr (void* source, void* dst, int copy_mem);
export_proto(copy_arr);

/*This function creates a copy of passport structure and allocates a memory for array descripted in the passport. 
1-st argument (source) is pointer to source passport structure.
2-st argument (dst) is pointer to destination passport structure.
3-rd argument indicates a kind of copy:
0 - copy values of sourse passport fields to destination passport fields, allocate new memory for base_addr
1 - copy fields values, allocate new memory for base_addr, copy memory from source base_addr to destination base_addr
2 - copy memory from source base_addr to destination base_addr 
*/
/* NOTE: real function name is _lfortran_copy_arr */

void copy_arr(void* source, void* dst, int copy_mem)
{
	gfc_array_char *psp_source = (gfc_array_char *) source;
	gfc_array_char *psp_dst = (gfc_array_char *) dst;
	index_type rank = GFC_DESCRIPTOR_RANK(psp_source);
	int element_size = GFC_DESCRIPTOR_SIZE(psp_source);
	
	index_type full_sz = 1;
	index_type lbound = 0;
	index_type ubound = 0;
	for (int i = 0; i < rank; i++)
	{
		lbound = GFC_DESCRIPTOR_LBOUND(psp_source, i);
		ubound = GFC_DESCRIPTOR_UBOUND(psp_source, i);
		full_sz *= ubound - lbound + 1;
	}

	if (copy_mem == 0 || copy_mem == 1)
	{
		int size_of_passport = sizeof(void*) + 2 * sizeof(index_type) + rank * sizeof(descriptor_dimension);
		memcpy(dst, source, size_of_passport);
		if (psp_source->base_addr != NULL)
			psp_dst->base_addr = (char*) malloc(full_sz * element_size);
	}

	if ((copy_mem == 1 || copy_mem == 2) && psp_source->base_addr != NULL)
		memcpy((void*) psp_dst->base_addr, (void*) psp_source->base_addr, full_sz * element_size);
}


extern void delete_arr (void*);
export_proto(delete_arr);

/*This function frees a memory of array descripted in the passport.
Argument is pointer to passport. */

/* NOTE: real function name is _lfortran_delete_arr */

void delete_arr(void* arr)
{
	gfc_array_char *psp = (gfc_array_char *) arr;
	free((void*) psp->base_addr);
	psp->base_addr = NULL;
}

