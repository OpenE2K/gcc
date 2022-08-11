#include <sys/prctl.h>

static void __attribute__((constructor))
__permit_unaligned_ldd_std (void)
{
  /* óÍ. bug #81602 */
  prctl (PR_SET_UNALIGN, PR_UNALIGN_NOPRINT, 0, 0, 0);
}
