#include "sfp-machine.h"

void __attribute__ ((optimize(0)))
__sfp_handle_exceptions (int _fex)
{
  const double max = __DBL_MAX__;
  const double min = __DBL_MIN__;
  const double zero = 0.0;
  const double one = 1.0;
  volatile double r;

  if (_fex & FP_EX_INVALID)
    {
      r = zero / zero;
    }
  if (_fex & FP_EX_DIVZERO)
    {
      r = one / zero;
    }
  if (_fex & FP_EX_OVERFLOW)
    {
      r = max + max;
    }
  if (_fex & FP_EX_UNDERFLOW)
    {
      r = min * min;
    }
  if (_fex & FP_EX_INEXACT)
    {
      r = max - one;
    }
}
