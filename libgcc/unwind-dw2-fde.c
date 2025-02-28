/* Subroutines needed for unwinding stack frames for exception handling.  */
/* Copyright (C) 1997-2017 Free Software Foundation, Inc.
   Contributed by Jason Merrill <jason@cygnus.com>.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

Under Section 7 of GPL version 3, you are granted additional
permissions described in the GCC Runtime Library Exception, version
3.1, as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License and
a copy of the GCC Runtime Library Exception along with this program;
see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
<http://www.gnu.org/licenses/>.  */

#ifndef _Unwind_Find_FDE
#include "tconfig.h"
#include "tsystem.h"
#include "coretypes.h"
#include "tm.h"
#include "libgcc_tm.h"
#include "dwarf2.h"
#include "unwind.h"
#define NO_BASE_OF_ENCODED_VALUE
#include "unwind-pe.h"
#include "unwind-dw2-fde.h"
#include "gthr.h"
#else
#if (defined(__GTHREAD_MUTEX_INIT) || defined(__GTHREAD_MUTEX_INIT_FUNCTION)) \
    && defined(__GCC_HAVE_SYNC_COMPARE_AND_SWAP_4)
#define ATOMIC_FDE_FAST_PATH 1
#endif
#endif

/* The unseen_objects list contains objects that have been registered
   but not yet categorized in any way.  The seen_objects list has had
   its pc_begin and count fields initialized at minimum, and is sorted
   by decreasing value of pc_begin.  */
static struct object *unseen_objects;
static struct object *seen_objects;
#ifdef ATOMIC_FDE_FAST_PATH
static int any_objects_registered;
#endif

#ifdef __GTHREAD_MUTEX_INIT
static __gthread_mutex_t object_mutex = __GTHREAD_MUTEX_INIT;
#define init_object_mutex_once()
#else
#ifdef __GTHREAD_MUTEX_INIT_FUNCTION
static __gthread_mutex_t object_mutex;

static void
init_object_mutex (void)
{
  __GTHREAD_MUTEX_INIT_FUNCTION (&object_mutex);
}

static void
init_object_mutex_once (void)
{
  static __gthread_once_t once = __GTHREAD_ONCE_INIT;
  __gthread_once (&once, init_object_mutex);
}
#else
/* ???  Several targets include this file with stubbing parts of gthr.h
   and expect no locking to be done.  */
#define init_object_mutex_once()
static __gthread_mutex_t object_mutex;
#endif
#endif

/* Called from crtbegin.o to register the unwind info for an object.  */

void
__register_frame_info_bases (const void *begin, struct object *ob,
#if ! (defined (__e2k__) && defined (__ptr128__))
			     void *tbase, void *dbase
#else /* defined (__e2k__) && defined (__ptr128__)  */
			     _Unwind_Ptr tbase, _Unwind_Ptr dbase
#endif /* defined (__e2k__) && defined (__ptr128__)  */
			     )
{
  /* If .eh_frame is empty, don't register at all.  */
  if ((const uword *) begin == 0 || *(const uword *) begin == 0)
    return;

#if ! (defined (__e2k__) && defined (__ptr128__))
  ob->pc_begin = (void *)-1;
#else /* defined (__e2k__) && defined (__ptr128__)  */
  ob->pc_begin = (_Unwind_Ptr) -1;
#endif /* defined (__e2k__) && defined (__ptr128__)  */
  ob->tbase = tbase;
  ob->dbase = dbase;
  ob->u.single = begin;
  ob->s.i = 0;
  ob->s.b.encoding = DW_EH_PE_omit;
#ifdef DWARF2_OBJECT_END_PTR_EXTENSION
  ob->fde_end = NULL;
#endif

  init_object_mutex_once ();
  __gthread_mutex_lock (&object_mutex);

  ob->next = unseen_objects;
  unseen_objects = ob;
#ifdef ATOMIC_FDE_FAST_PATH
  /* Set flag that at least one library has registered FDEs.
     Use relaxed MO here, it is up to the app to ensure that the library
     loading/initialization happens-before using that library in other
     threads (in particular unwinding with that library's functions
     appearing in the backtraces).  Calling that library's functions
     without waiting for the library to initialize would be racy.  */
  if (!any_objects_registered)
    __atomic_store_n (&any_objects_registered, 1, __ATOMIC_RELAXED);
#endif

  __gthread_mutex_unlock (&object_mutex);
}

void
__register_frame_info (const void *begin, struct object *ob)
{
#if defined (__e2k__) && defined (__ptr128__)
  _Unwind_Ptr tbase;
  _Unwind_Ptr dbase;

  tbase = ({
      register unsigned long cud;
      asm volatile ("rrd %%cud.lo, %0" : "=r" (cud));
      (_Unwind_Ptr) (cud & 0xffffffff);
    });

  dbase = ({
      register unsigned long gd;
      asm volatile ("rrd %%gd.lo, %0" : "=r" (gd));
      (_Unwind_Ptr) (gd & 0xffffffff);
    });
#endif /* defined (__e2k__) && defined (__ptr128__)  */

  __register_frame_info_bases (begin, ob,
#if ! (defined (__e2k__) && defined (__ptr128__))
			       0, 0
#else /* defined (__e2k__) && defined (__ptr128__)  */
			       tbase, dbase
#endif /* defined (__e2k__) && defined (__ptr128__)  */
			       );

}

void
__register_frame (void *begin)
{
  struct object *ob;

  /* If .eh_frame is empty, don't register at all.  */
  if (*(uword *) begin == 0)
    return;

  ob = malloc (sizeof (struct object));
  __register_frame_info (begin, ob);
}

/* Similar, but BEGIN is actually a pointer to a table of unwind entries
   for different translation units.  Called from the file generated by
   collect2.  */

void
__register_frame_info_table_bases (void *begin, struct object *ob,
#if ! (defined (__e2k__) && defined (__ptr128__))
				   void *tbase, void *dbase
#else /* defined (__e2k__) && defined (__ptr128__)  */
				   _Unwind_Ptr tbase, _Unwind_Ptr dbase
#endif /* defined (__e2k__) && defined (__ptr128__)  */
				   )
{
#if ! (defined (__e2k__) && defined (__ptr128__))
  ob->pc_begin = (void *)-1;
#else /* defined (__e2k__) && defined (__ptr128__)  */
  ob->pc_begin = (_Unwind_Ptr) -1;
#endif /* defined (__e2k__) && defined (__ptr128__)  */
  ob->tbase = tbase;
  ob->dbase = dbase;
  ob->u.array = begin;
  ob->s.i = 0;
  ob->s.b.from_array = 1;
  ob->s.b.encoding = DW_EH_PE_omit;

  init_object_mutex_once ();
  __gthread_mutex_lock (&object_mutex);

  ob->next = unseen_objects;
  unseen_objects = ob;
#ifdef ATOMIC_FDE_FAST_PATH
  /* Set flag that at least one library has registered FDEs.
     Use relaxed MO here, it is up to the app to ensure that the library
     loading/initialization happens-before using that library in other
     threads (in particular unwinding with that library's functions
     appearing in the backtraces).  Calling that library's functions
     without waiting for the library to initialize would be racy.  */
  if (!any_objects_registered)
    __atomic_store_n (&any_objects_registered, 1, __ATOMIC_RELAXED);
#endif

  __gthread_mutex_unlock (&object_mutex);
}

void
__register_frame_info_table (void *begin, struct object *ob)
{
  __register_frame_info_table_bases (begin, ob, 0, 0);
}

void
__register_frame_table (void *begin)
{
  struct object *ob = malloc (sizeof (struct object));
  __register_frame_info_table (begin, ob);
}

/* Called from crtbegin.o to deregister the unwind info for an object.  */
/* ??? Glibc has for a while now exported __register_frame_info and
   __deregister_frame_info.  If we call __register_frame_info_bases
   from crtbegin (wherein it is declared weak), and this object does
   not get pulled from libgcc.a for other reasons, then the
   invocation of __deregister_frame_info will be resolved from glibc.
   Since the registration did not happen there, we'll die.

   Therefore, declare a new deregistration entry point that does the
   exact same thing, but will resolve to the same library as
   implements __register_frame_info_bases.  */

void *
__deregister_frame_info_bases (const void *begin)
{
  struct object **p;
  struct object *ob = 0;

  /* If .eh_frame is empty, we haven't registered.  */
  if ((const uword *) begin == 0 || *(const uword *) begin == 0)
    return ob;

  init_object_mutex_once ();
  __gthread_mutex_lock (&object_mutex);

  for (p = &unseen_objects; *p ; p = &(*p)->next)
    if ((*p)->u.single == begin)
      {
	ob = *p;
	*p = ob->next;
	goto out;
      }

  for (p = &seen_objects; *p ; p = &(*p)->next)
    if ((*p)->s.b.sorted)
      {
	if ((*p)->u.sort->orig_data == begin)
	  {
	    ob = *p;
	    *p = ob->next;
	    free (ob->u.sort);
	    goto out;
	  }
      }
    else
      {
	if ((*p)->u.single == begin)
	  {
	    ob = *p;
	    *p = ob->next;
	    goto out;
	  }
      }

 out:
  __gthread_mutex_unlock (&object_mutex);
  gcc_assert (ob);
  return (void *) ob;
}

void *
__deregister_frame_info (const void *begin)
{
  return __deregister_frame_info_bases (begin);
}

void
__deregister_frame (void *begin)
{
  /* If .eh_frame is empty, we haven't registered.  */
  if (*(uword *) begin != 0)
    free (__deregister_frame_info (begin));
}


/* Like base_of_encoded_value, but take the base from a struct object
   instead of an _Unwind_Context.  */

static _Unwind_Ptr
base_from_object (unsigned char encoding, struct object *ob)
{
  if (encoding == DW_EH_PE_omit)
    return 0;

  switch (encoding & 0x70)
    {
    case DW_EH_PE_absptr:
#if ! (defined (__e2k__) && defined (__ptr128__))
    case DW_EH_PE_pcrel:
#endif /* ! (defined (__e2k__) && defined (__ptr128__))  */
    case DW_EH_PE_aligned:
      return 0;

#if defined (__e2k__) && defined (__ptr128__)
    case DW_EH_PE_pcrel:
      /* Take into account that in PM the value calculated by means of the
	 relative encoding and the current position in `.eh_frame' belong to
	 different segments.  */
      return (_Unwind_Ptr) ob->tbase - (_Unwind_Ptr) ob->dbase;
#endif /* defined (__e2k__) && defined (__ptr128__)  */

    case DW_EH_PE_textrel:
      return (_Unwind_Ptr) ob->tbase;
    case DW_EH_PE_datarel:
      return (_Unwind_Ptr) ob->dbase;
    default:
      gcc_unreachable ();
    }
}

/* Return the FDE pointer encoding from the CIE.  */
/* ??? This is a subset of extract_cie_info from unwind-dw2.c.  */

static int
get_cie_encoding (const struct dwarf_cie *cie)
{
  const unsigned char *aug, *p;
#if ! (defined (__e2k__) && defined (__ptr128__))
  _Unwind_Ptr dummy;
#else /* defined (__e2k__) && defined (__ptr128__)  */
  void *dummy;
#endif /* defined (__e2k__) && defined (__ptr128__)  */
  _uleb128_t utmp;
  _sleb128_t stmp;

  aug = cie->augmentation;
  p = aug + strlen ((const char *)aug) + 1; /* Skip the augmentation string.  */
  if (__builtin_expect (cie->version >= 4, 0))
    {
      if (p[0] != sizeof (void *) || p[1] != 0)
	return DW_EH_PE_omit;		/* We are not prepared to handle unexpected
					   address sizes or segment selectors.  */
      p += 2;				/* Skip address size and segment size.  */
    }

  if (aug[0] != 'z')
    return DW_EH_PE_absptr;

  p = read_uleb128 (p, &utmp);		/* Skip code alignment.  */
  p = read_sleb128 (p, &stmp);		/* Skip data alignment.  */
  if (cie->version == 1)		/* Skip return address column.  */
    p++;
  else
    p = read_uleb128 (p, &utmp);

  aug++;				/* Skip 'z' */
  p = read_uleb128 (p, &utmp);		/* Skip augmentation length.  */
  while (1)
    {
      /* This is what we're looking for.  */
      if (*aug == 'R')
	return *p;
      /* Personality encoding and pointer.  */
      else if (*aug == 'P')
	{
	  /* ??? Avoid dereferencing indirect pointers, since we're
	     faking the base address.  Gotta keep DW_EH_PE_aligned
	     intact, however.  */
	  p =
#if ! (defined (__e2k__) && defined (__ptr128__))
	    read_encoded_value_with_base
#else /* defined (__e2k__) && defined (__ptr128__)  */
	    read_encoded_ptr_with_base
#endif /* defined (__e2k__) && defined (__ptr128__)  */
	    (*p & 0x7F, 0, p + 1, &dummy);
	}
      /* LSDA encoding.  */
      else if (*aug == 'L')
	p++;
      /* Otherwise end of string, or unknown augmentation.  */
      else
	return DW_EH_PE_absptr;
      aug++;
    }
}

static inline int
get_fde_encoding (const struct dwarf_fde *f)
{
  return get_cie_encoding (get_cie (f));
}


/* Sorting an array of FDEs by address.
   (Ideally we would have the linker sort the FDEs so we don't have to do
   it at run time. But the linkers are not yet prepared for this.)  */

/* Comparison routines.  Three variants of increasing complexity.  */

static int
fde_unencoded_compare (struct object *ob __attribute__((unused)),
		       const fde *x, const fde *y)
{
  _Unwind_Ptr x_ptr, y_ptr;
  memcpy (&x_ptr, x->pc_begin, sizeof (_Unwind_Ptr));
  memcpy (&y_ptr, y->pc_begin, sizeof (_Unwind_Ptr));

  if (x_ptr > y_ptr)
    return 1;
  if (x_ptr < y_ptr)
    return -1;
  return 0;
}

static int
fde_single_encoding_compare (struct object *ob, const fde *x, const fde *y)
{
  _Unwind_Ptr base, x_ptr, y_ptr;

  base = base_from_object (ob->s.b.encoding, ob);
  read_encoded_value_with_base (ob->s.b.encoding, base, x->pc_begin, &x_ptr);
  read_encoded_value_with_base (ob->s.b.encoding, base, y->pc_begin, &y_ptr);

  if (x_ptr > y_ptr)
    return 1;
  if (x_ptr < y_ptr)
    return -1;
  return 0;
}

static int
fde_mixed_encoding_compare (struct object *ob, const fde *x, const fde *y)
{
  int x_encoding, y_encoding;
  _Unwind_Ptr x_ptr, y_ptr;

  x_encoding = get_fde_encoding (x);
  read_encoded_value_with_base (x_encoding, base_from_object (x_encoding, ob),
				x->pc_begin, &x_ptr);

  y_encoding = get_fde_encoding (y);
  read_encoded_value_with_base (y_encoding, base_from_object (y_encoding, ob),
				y->pc_begin, &y_ptr);

  if (x_ptr > y_ptr)
    return 1;
  if (x_ptr < y_ptr)
    return -1;
  return 0;
}

typedef int (*fde_compare_t) (struct object *, const fde *, const fde *);


/* This is a special mix of insertion sort and heap sort, optimized for
   the data sets that actually occur. They look like
   101 102 103 127 128 105 108 110 190 111 115 119 125 160 126 129 130.
   I.e. a linearly increasing sequence (coming from functions in the text
   section), with additionally a few unordered elements (coming from functions
   in gnu_linkonce sections) whose values are higher than the values in the
   surrounding linear sequence (but not necessarily higher than the values
   at the end of the linear sequence!).
   The worst-case total run time is O(N) + O(n log (n)), where N is the
   total number of FDEs and n is the number of erratic ones.  */

struct fde_accumulator
{
  struct fde_vector *linear;
  struct fde_vector *erratic;
};

static inline int
start_fde_sort (struct fde_accumulator *accu, size_t count)
{
  size_t size;
  if (! count)
    return 0;

  size = sizeof (struct fde_vector) + sizeof (const fde *) * count;
  if ((accu->linear = malloc (size)))
    {
      accu->linear->count = 0;
      if ((accu->erratic = malloc (size)))
	accu->erratic->count = 0;
      return 1;
    }
  else
    return 0;
}

static inline void
fde_insert (struct fde_accumulator *accu, const fde *this_fde)
{
  if (accu->linear)
    accu->linear->array[accu->linear->count++] = this_fde;
}

/* Split LINEAR into a linear sequence with low values and an erratic
   sequence with high values, put the linear one (of longest possible
   length) into LINEAR and the erratic one into ERRATIC. This is O(N).

   Because the longest linear sequence we are trying to locate within the
   incoming LINEAR array can be interspersed with (high valued) erratic
   entries.  We construct a chain indicating the sequenced entries.
   To avoid having to allocate this chain, we overlay it onto the space of
   the ERRATIC array during construction.  A final pass iterates over the
   chain to determine what should be placed in the ERRATIC array, and
   what is the linear sequence.  This overlay is safe from aliasing.  */

static inline void
fde_split (struct object *ob, fde_compare_t fde_compare,
	   struct fde_vector *linear, struct fde_vector *erratic)
{
  static const fde *marker;
  size_t count = linear->count;
  const fde *const *chain_end = &marker;
  size_t i, j, k;

  /* This should optimize out, but it is wise to make sure this assumption
     is correct. Should these have different sizes, we cannot cast between
     them and the overlaying onto ERRATIC will not work.  */
  gcc_assert (sizeof (const fde *) == sizeof (const fde **));

  for (i = 0; i < count; i++)
    {
      const fde *const *probe;

      for (probe = chain_end;
	   probe != &marker && fde_compare (ob, linear->array[i], *probe) < 0;
	   probe = chain_end)
	{
	  chain_end = (const fde *const*) erratic->array[probe - linear->array];
	  erratic->array[probe - linear->array] = NULL;
	}
      erratic->array[i] = (const fde *) chain_end;
      chain_end = &linear->array[i];
    }

  /* Each entry in LINEAR which is part of the linear sequence we have
     discovered will correspond to a non-NULL entry in the chain we built in
     the ERRATIC array.  */
  for (i = j = k = 0; i < count; i++)
    if (erratic->array[i])
      linear->array[j++] = linear->array[i];
    else
      erratic->array[k++] = linear->array[i];
  linear->count = j;
  erratic->count = k;
}

#define SWAP(x,y) do { const fde * tmp = x; x = y; y = tmp; } while (0)

/* Convert a semi-heap to a heap.  A semi-heap is a heap except possibly
   for the first (root) node; push it down to its rightful place.  */

static void
frame_downheap (struct object *ob, fde_compare_t fde_compare, const fde **a,
		int lo, int hi)
{
  int i, j;

  for (i = lo, j = 2*i+1;
       j < hi;
       j = 2*i+1)
    {
      if (j+1 < hi && fde_compare (ob, a[j], a[j+1]) < 0)
	++j;

      if (fde_compare (ob, a[i], a[j]) < 0)
	{
	  SWAP (a[i], a[j]);
	  i = j;
	}
      else
	break;
    }
}

/* This is O(n log(n)).  BSD/OS defines heapsort in stdlib.h, so we must
   use a name that does not conflict.  */

static void
frame_heapsort (struct object *ob, fde_compare_t fde_compare,
		struct fde_vector *erratic)
{
  /* For a description of this algorithm, see:
     Samuel P. Harbison, Guy L. Steele Jr.: C, a reference manual, 2nd ed.,
     p. 60-61.  */
  const fde ** a = erratic->array;
  /* A portion of the array is called a "heap" if for all i>=0:
     If i and 2i+1 are valid indices, then a[i] >= a[2i+1].
     If i and 2i+2 are valid indices, then a[i] >= a[2i+2].  */
  size_t n = erratic->count;
  int m;

  /* Expand our heap incrementally from the end of the array, heapifying
     each resulting semi-heap as we go.  After each step, a[m] is the top
     of a heap.  */
  for (m = n/2-1; m >= 0; --m)
    frame_downheap (ob, fde_compare, a, m, n);

  /* Shrink our heap incrementally from the end of the array, first
     swapping out the largest element a[0] and then re-heapifying the
     resulting semi-heap.  After each step, a[0..m) is a heap.  */
  for (m = n-1; m >= 1; --m)
    {
      SWAP (a[0], a[m]);
      frame_downheap (ob, fde_compare, a, 0, m);
    }
#undef SWAP
}

/* Merge V1 and V2, both sorted, and put the result into V1.  */
static inline void
fde_merge (struct object *ob, fde_compare_t fde_compare,
	   struct fde_vector *v1, struct fde_vector *v2)
{
  size_t i1, i2;
  const fde * fde2;

  i2 = v2->count;
  if (i2 > 0)
    {
      i1 = v1->count;
      do
	{
	  i2--;
	  fde2 = v2->array[i2];
	  while (i1 > 0 && fde_compare (ob, v1->array[i1-1], fde2) > 0)
	    {
	      v1->array[i1+i2] = v1->array[i1-1];
	      i1--;
	    }
	  v1->array[i1+i2] = fde2;
	}
      while (i2 > 0);
      v1->count += v2->count;
    }
}

static inline void
end_fde_sort (struct object *ob, struct fde_accumulator *accu, size_t count)
{
  fde_compare_t fde_compare;

  gcc_assert (!accu->linear || accu->linear->count == count);

  if (ob->s.b.mixed_encoding)
    fde_compare = fde_mixed_encoding_compare;
  else if (ob->s.b.encoding == DW_EH_PE_absptr)
    fde_compare = fde_unencoded_compare;
  else
    fde_compare = fde_single_encoding_compare;

  if (accu->erratic)
    {
      fde_split (ob, fde_compare, accu->linear, accu->erratic);
      gcc_assert (accu->linear->count + accu->erratic->count == count);
      frame_heapsort (ob, fde_compare, accu->erratic);
      fde_merge (ob, fde_compare, accu->linear, accu->erratic);
      free (accu->erratic);
    }
  else
    {
      /* We've not managed to malloc an erratic array,
	 so heap sort in the linear one.  */
      frame_heapsort (ob, fde_compare, accu->linear);
    }
}


/* Update encoding, mixed_encoding, and pc_begin for OB for the
   fde array beginning at THIS_FDE.  Return the number of fdes
   encountered along the way.  */

static size_t
classify_object_over_fdes (struct object *ob, const fde *this_fde)
{
  const struct dwarf_cie *last_cie = 0;
  size_t count = 0;
  int encoding = DW_EH_PE_absptr;
  _Unwind_Ptr base = 0;

  for (; ! last_fde (ob, this_fde); this_fde = next_fde (this_fde))
    {
      const struct dwarf_cie *this_cie;
      _Unwind_Ptr mask, pc_begin;

      /* Skip CIEs.  */
      if (this_fde->CIE_delta == 0)
	continue;

      /* Determine the encoding for this FDE.  Note mixed encoded
	 objects for later.  */
      this_cie = get_cie (this_fde);
      if (this_cie != last_cie)
	{
	  last_cie = this_cie;
	  encoding = get_cie_encoding (this_cie);
	  if (encoding == DW_EH_PE_omit)
	    return -1;
	  base = base_from_object (encoding, ob);
	  if (ob->s.b.encoding == DW_EH_PE_omit)
	    ob->s.b.encoding = encoding;
	  else if (ob->s.b.encoding != encoding)
	    ob->s.b.mixed_encoding = 1;
	}

      read_encoded_value_with_base (encoding, base, this_fde->pc_begin,
				    &pc_begin);

      /* Take care to ignore link-once functions that were removed.
	 In these cases, the function address will be NULL, but if
	 the encoding is smaller than a pointer a true NULL may not
	 be representable.  Assume 0 in the representable bits is NULL.  */
      mask = size_of_encoded_value (encoding);

      /* FIXME: in PM `size_of_encoded_value ()' returns 8, which is probably
	 unreasonably large and should be fixed. This makes the calculations
	 below result in an unexpected `mask == 0' somehow taking into account
	 that `_Unwind_Ptr' is a 32-bit type. Temporarely overcome this issue
	 this way . . .  */
#if ! (defined (__e2k__) && defined (__ptr128__))
      if (mask < sizeof (void *))
	mask = (((_Unwind_Ptr) 1) << (mask << 3)) - 1;
      else
#endif /* ! (defined (__e2k__) && defined (__ptr128__))  */
	mask = -1;

      if ((pc_begin & mask) == 0)
	continue;

      count += 1;
#if ! (defined (__e2k__) && defined (__ptr128__))
      if ((void *) pc_begin < ob->pc_begin)
	ob->pc_begin = (void *) pc_begin;
#else /* defined (__e2k__) && defined (__ptr128__)  */
      if (pc_begin < ob->pc_begin)
	ob->pc_begin = pc_begin;
#endif /* defined (__e2k__) && defined (__ptr128__)  */
    }

  return count;
}

static void
add_fdes (struct object *ob, struct fde_accumulator *accu, const fde *this_fde)
{
  const struct dwarf_cie *last_cie = 0;
  int encoding = ob->s.b.encoding;
  _Unwind_Ptr base = base_from_object (ob->s.b.encoding, ob);

  for (; ! last_fde (ob, this_fde); this_fde = next_fde (this_fde))
    {
      const struct dwarf_cie *this_cie;

      /* Skip CIEs.  */
      if (this_fde->CIE_delta == 0)
	continue;

      if (ob->s.b.mixed_encoding)
	{
	  /* Determine the encoding for this FDE.  Note mixed encoded
	     objects for later.  */
	  this_cie = get_cie (this_fde);
	  if (this_cie != last_cie)
	    {
	      last_cie = this_cie;
	      encoding = get_cie_encoding (this_cie);
	      base = base_from_object (encoding, ob);
	    }
	}

      if (encoding == DW_EH_PE_absptr)
	{
	  _Unwind_Ptr ptr;
	  memcpy (&ptr, this_fde->pc_begin, sizeof (_Unwind_Ptr));
	  if (ptr == 0)
	    continue;
	}
      else
	{
	  _Unwind_Ptr pc_begin, mask;

	  read_encoded_value_with_base (encoding, base, this_fde->pc_begin,
					&pc_begin);

	  /* Take care to ignore link-once functions that were removed.
	     In these cases, the function address will be NULL, but if
	     the encoding is smaller than a pointer a true NULL may not
	     be representable.  Assume 0 in the representable bits is NULL.  */

	  mask = size_of_encoded_value (encoding);
	  /* See above.  */
#if ! (defined (__e2k__) &&  defined (__ptr128__))
	  if (mask < sizeof (void *))
	    mask = (((_Unwind_Ptr) 1) << (mask << 3)) - 1;
	  else
#endif /* defined (__e2k__) &&  defined (__ptr128__)  */
	    mask = -1;

	  if ((pc_begin & mask) == 0)
	    continue;
	}

      fde_insert (accu, this_fde);
    }
}

/* Set up a sorted array of pointers to FDEs for a loaded object.  We
   count up the entries before allocating the array because it's likely to
   be faster.  We can be called multiple times, should we have failed to
   allocate a sorted fde array on a previous occasion.  */

static inline void
init_object (struct object* ob)
{
  struct fde_accumulator accu;
  size_t count;

  count = ob->s.b.count;
  if (count == 0)
    {
      if (ob->s.b.from_array)
	{
	  fde **p = ob->u.array;
	  for (count = 0; *p; ++p)
	    {
	      size_t cur_count = classify_object_over_fdes (ob, *p);
	      if (cur_count == (size_t) -1)
		goto unhandled_fdes;
	      count += cur_count;
	    }
	}
      else
	{
	  count = classify_object_over_fdes (ob, ob->u.single);
	  if (count == (size_t) -1)
	    {
	      static const fde terminator;
	    unhandled_fdes:
	      ob->s.i = 0;
	      ob->s.b.encoding = DW_EH_PE_omit;
	      ob->u.single = &terminator;
	      return;
	    }
	}

      /* The count field we have in the main struct object is somewhat
	 limited, but should suffice for virtually all cases.  If the
	 counted value doesn't fit, re-write a zero.  The worst that
	 happens is that we re-count next time -- admittedly non-trivial
	 in that this implies some 2M fdes, but at least we function.  */
      ob->s.b.count = count;
      if (ob->s.b.count != count)
	ob->s.b.count = 0;
    }

  if (!start_fde_sort (&accu, count))
    return;

  if (ob->s.b.from_array)
    {
      fde **p;
      for (p = ob->u.array; *p; ++p)
	add_fdes (ob, &accu, *p);
    }
  else
    add_fdes (ob, &accu, ob->u.single);

  end_fde_sort (ob, &accu, count);

  /* Save the original fde pointer, since this is the key by which the
     DSO will deregister the object.  */
  accu.linear->orig_data = ob->u.single;
  ob->u.sort = accu.linear;

  ob->s.b.sorted = 1;
}

/* A linear search through a set of FDEs for the given PC.  This is
   used when there was insufficient memory to allocate and sort an
   array.  */

static const fde *
linear_search_fdes (struct object *ob, const fde *this_fde,
#if ! (defined (__e2k__) && defined (__ptr128__))
		    void *pc
#else /* defined (__e2k__) && defined (__ptr128__)  */
		    _Unwind_Ptr pc
#endif /* defined (__e2k__) && defined (__ptr128__)  */
		    )
{
  const struct dwarf_cie *last_cie = 0;
  int encoding = ob->s.b.encoding;
  _Unwind_Ptr base = base_from_object (ob->s.b.encoding, ob);

  for (; ! last_fde (ob, this_fde); this_fde = next_fde (this_fde))
    {
      const struct dwarf_cie *this_cie;
      _Unwind_Ptr pc_begin, pc_range;

      /* Skip CIEs.  */
      if (this_fde->CIE_delta == 0)
	continue;

      if (ob->s.b.mixed_encoding)
	{
	  /* Determine the encoding for this FDE.  Note mixed encoded
	     objects for later.  */
	  this_cie = get_cie (this_fde);
	  if (this_cie != last_cie)
	    {
	      last_cie = this_cie;
	      encoding = get_cie_encoding (this_cie);
	      base = base_from_object (encoding, ob);
	    }
	}

      if (encoding == DW_EH_PE_absptr)
	{
	  const _Unwind_Ptr *pc_array = (const _Unwind_Ptr *) this_fde->pc_begin;
	  pc_begin = pc_array[0];
	  pc_range = pc_array[1];
	  if (pc_begin == 0)
	    continue;
	}
      else
	{
	  _Unwind_Ptr mask;
	  const unsigned char *p;

	  p = read_encoded_value_with_base (encoding, base,
					    this_fde->pc_begin, &pc_begin);
	  read_encoded_value_with_base (encoding & 0x0F, 0, p, &pc_range);

	  /* Take care to ignore link-once functions that were removed.
	     In these cases, the function address will be NULL, but if
	     the encoding is smaller than a pointer a true NULL may not
	     be representable.  Assume 0 in the representable bits is NULL.  */
	  mask = size_of_encoded_value (encoding);
	  if (mask < sizeof (void *))
	    mask = (((_Unwind_Ptr) 1) << (mask << 3)) - 1;
	  else
	    mask = -1;

	  if ((pc_begin & mask) == 0)
	    continue;
	}

      if ((_Unwind_Ptr) pc - pc_begin < pc_range)
	return this_fde;
    }

  return NULL;
}

/* Binary search for an FDE containing the given PC.  Here are three
   implementations of increasing complexity.  */

static inline const fde *
binary_search_unencoded_fdes (struct object *ob,
#if ! (defined (__e2k__) && defined (__ptr128__))
			      void *pc
#else /* defined (__e2k__) && defined (__ptr128__)  */
			      _Unwind_Ptr pc
#endif /* defined (__e2k__) && defined (__ptr128__)  */
			      )
{
  struct fde_vector *vec = ob->u.sort;
  size_t lo, hi;

  for (lo = 0, hi = vec->count; lo < hi; )
    {
      size_t i = (lo + hi) / 2;
      const fde *const f = vec->array[i];
#if ! (defined (__e2k__) && defined (__ptr128__))
      void *pc_begin;
#else /* defined (__e2k__) && defined (__ptr128__)  */
	_Unwind_Ptr pc_begin;
#endif /* defined (__e2k__) && defined (__ptr128__)  */
      uaddr pc_range;
      memcpy (&pc_begin,
#if ! (defined (__e2k__) && defined (__ptr128__))
	      (const void * const *) f->pc_begin, sizeof (void *)
#else /* defined (__e2k__) && defined (__ptr128__)  */
	      (const _Unwind_Ptr *) f->pc_begin, sizeof (_Unwind_Ptr)
#endif /* defined (__e2k__) && defined (__ptr128__)  */
	      );
      memcpy (&pc_range, (const uaddr *) f->pc_begin + 1, sizeof (uaddr));

      if (pc < pc_begin)
	hi = i;
      else if (pc >= pc_begin + pc_range)
	lo = i + 1;
      else
	return f;
    }

  return NULL;
}

static inline const fde *
binary_search_single_encoding_fdes (struct object *ob,
#if ! (defined (__e2k__) && defined (__ptr128__))
				    void *pc
#else /* defined (__e2k__) && defined (__ptr128__)  */
				    _Unwind_Ptr pc
#endif /* defined (__e2k__) && defined (__ptr128__)  */
				    )
{
  struct fde_vector *vec = ob->u.sort;
  int encoding = ob->s.b.encoding;
  _Unwind_Ptr base = base_from_object (encoding, ob);
  size_t lo, hi;

  for (lo = 0, hi = vec->count; lo < hi; )
    {
      size_t i = (lo + hi) / 2;
      const fde *f = vec->array[i];
      _Unwind_Ptr pc_begin, pc_range;
      const unsigned char *p;

      p = read_encoded_value_with_base (encoding, base, f->pc_begin,
					&pc_begin);
      read_encoded_value_with_base (encoding & 0x0F, 0, p, &pc_range);

      if ((_Unwind_Ptr) pc < pc_begin)
	hi = i;
      else if ((_Unwind_Ptr) pc >= pc_begin + pc_range)
	lo = i + 1;
      else
	return f;
    }

  return NULL;
}

static inline const fde *
binary_search_mixed_encoding_fdes (struct object *ob,
#if ! (defined (__e2k__) && defined (__ptr128__))
				   void *pc
#else /* defined (__e2k__) && defined (__ptr128__)  */
				   _Unwind_Ptr pc
#endif /* defined (__e2k__) && defined (__ptr128__)  */
				   )
{
  struct fde_vector *vec = ob->u.sort;
  size_t lo, hi;

  for (lo = 0, hi = vec->count; lo < hi; )
    {
      size_t i = (lo + hi) / 2;
      const fde *f = vec->array[i];
      _Unwind_Ptr pc_begin, pc_range;
      const unsigned char *p;
      int encoding;

      encoding = get_fde_encoding (f);
      p = read_encoded_value_with_base (encoding,
					base_from_object (encoding, ob),
					f->pc_begin, &pc_begin);
      read_encoded_value_with_base (encoding & 0x0F, 0, p, &pc_range);

      if ((_Unwind_Ptr) pc < pc_begin)
	hi = i;
      else if ((_Unwind_Ptr) pc >= pc_begin + pc_range)
	lo = i + 1;
      else
	return f;
    }

  return NULL;
}

static const fde *
search_object (struct object* ob,
#if ! (defined (__e2k__) && defined (__ptr128__))
	       void *pc
#else /* defined (__e2k__) && defined (__ptr128__)  */
	       _Unwind_Ptr pc
#endif /* defined (__e2k__) && defined (__ptr128__)  */
	       )
{
  /* If the data hasn't been sorted, try to do this now.  We may have
     more memory available than last time we tried.  */
  if (! ob->s.b.sorted)
    {
      init_object (ob);

      /* Despite the above comment, the normal reason to get here is
	 that we've not processed this object before.  A quick range
	 check is in order.  */
      if (pc < ob->pc_begin)
	return NULL;
    }

  if (ob->s.b.sorted)
    {
      if (ob->s.b.mixed_encoding)
	return binary_search_mixed_encoding_fdes (ob, pc);
      else if (ob->s.b.encoding == DW_EH_PE_absptr)
	return binary_search_unencoded_fdes (ob, pc);
      else
	return binary_search_single_encoding_fdes (ob, pc);
    }
  else
    {
      /* Long slow laborious linear search, cos we've no memory.  */
      if (ob->s.b.from_array)
	{
	  fde **p;
	  for (p = ob->u.array; *p ; p++)
	    {
	      const fde *f = linear_search_fdes (ob, *p, pc);
	      if (f)
		return f;
	    }
	  return NULL;
	}
      else
	return linear_search_fdes (ob, ob->u.single, pc);
    }
}

const fde *
_Unwind_Find_FDE (
#if ! (defined (__e2k__) && defined (__ptr128__))
		  void *pc,
#else /* defined (__e2k__) && defined (__ptr128__)  */
		  _Unwind_Ptr pc,
#endif /* defined (__e2k__) && defined (__ptr128__)  */
		  struct dwarf_eh_bases *bases)
{
  struct object *ob;
  const fde *f = NULL;

#ifdef ATOMIC_FDE_FAST_PATH
  /* For targets where unwind info is usually not registered through these
     APIs anymore, avoid taking a global lock.
     Use relaxed MO here, it is up to the app to ensure that the library
     loading/initialization happens-before using that library in other
     threads (in particular unwinding with that library's functions
     appearing in the backtraces).  Calling that library's functions
     without waiting for the library to initialize would be racy.  */
  if (__builtin_expect (!__atomic_load_n (&any_objects_registered,
					  __ATOMIC_RELAXED), 1))
    return NULL;
#endif

  init_object_mutex_once ();
  __gthread_mutex_lock (&object_mutex);

  /* Linear search through the classified objects, to find the one
     containing the pc.  Note that pc_begin is sorted descending, and
     we expect objects to be non-overlapping.  */
  for (ob = seen_objects; ob; ob = ob->next)
    if (pc >= ob->pc_begin)
      {
	f = search_object (ob, pc);
	if (f)
	  goto fini;
	break;
      }

  /* Classify and search the objects we've not yet processed.  */
  while ((ob = unseen_objects))
    {
      struct object **p;

      unseen_objects = ob->next;
      f = search_object (ob, pc);

      /* Insert the object into the classified list.  */
      for (p = &seen_objects; *p ; p = &(*p)->next)
	if ((*p)->pc_begin < ob->pc_begin)
	  break;
      ob->next = *p;
      *p = ob;

      if (f)
	goto fini;
    }

 fini:
  __gthread_mutex_unlock (&object_mutex);

  if (f)
    {
      int encoding;
      _Unwind_Ptr func;

      bases->tbase = ob->tbase;
      bases->dbase = ob->dbase;

      encoding = ob->s.b.encoding;
      if (ob->s.b.mixed_encoding)
	encoding = get_fde_encoding (f);
      read_encoded_value_with_base (encoding, base_from_object (encoding, ob),
				    f->pc_begin, &func);
      bases->func = (void *) func;
    }

  return f;
}
