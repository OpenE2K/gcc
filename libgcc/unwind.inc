/* Exception handling and frame unwind runtime interface routines. -*- C -*-
   Copyright (C) 2001-2017 Free Software Foundation, Inc.

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

/* This is derived from the C++ ABI for IA-64.  Where we diverge
   for cross-architecture compatibility are noted with "@@@".  
   This file is included from unwind-dw2.c, unwind-sjlj.c or
   unwind-ia64.c.  */

/* Subroutine of _Unwind_RaiseException also invoked from _Unwind_Resume. 

   Unwind the stack calling the personality routine to find both the
   exception handler and intermediary cleanup code.  We'll only locate
   the first such frame here.  Cleanup code will call back into
   _Unwind_Resume and we'll continue Phase 2 there.  */

static _Unwind_Reason_Code
_Unwind_RaiseException_Phase2(struct _Unwind_Exception *exc,
			      struct _Unwind_Context *context)
{
  _Unwind_Reason_Code code;

  while (1)
    {
      _Unwind_FrameState fs;
      int match_handler;

      code = uw_frame_state_for (context, &fs);

      /* Identify when we've reached the designated handler context.  */
      match_handler = (uw_identify_context (context) == exc->private_2
		       ? _UA_HANDLER_FRAME : 0);

      if (code != _URC_NO_REASON)
	/* Some error encountered.  Usually the unwinder doesn't
	   diagnose these and merely crashes.  */
	return _URC_FATAL_PHASE2_ERROR;

      /* Unwind successful.  Run the personality routine, if any.  */
      if (fs.personality)
	{
	  code = (*fs.personality) (1, _UA_CLEANUP_PHASE | match_handler,
				    exc->exception_class, exc, context);
	  if (code == _URC_INSTALL_CONTEXT)
	    break;
	  if (code != _URC_CONTINUE_UNWIND) 
	    return _URC_FATAL_PHASE2_ERROR;
	}

      /* Don't let us unwind past the handler context.  */
      gcc_assert (!match_handler);

      uw_update_context (context, &fs);
    }

  return code;
}

/* Raise an exception, passing along the given exception object.  */

_Unwind_Reason_Code LIBGCC2_UNWIND_ATTRIBUTE
_Unwind_RaiseException(struct _Unwind_Exception *exc)
{
  struct _Unwind_Context this_context, cur_context;
  _Unwind_Reason_Code code;
#if defined __e2k__
  e2k_mem_crs_t chain_stack[CHAIN_STACK_CACHE_LEN];
#endif /* defined __e2k__  */

  /* Set up this_context to describe the current stack frame.  */
  uw_init_context (&this_context
#if defined __e2k__
		   , chain_stack
#endif /* defined __e2k__  */
		   );
  cur_context = this_context;

  /* Phase 1: Search.  Unwind the stack, calling the personality routine
     with the _UA_SEARCH_PHASE flag set.  Do not modify the stack yet.  */
  while (1)
    {
      _Unwind_FrameState fs;

      /* Set up fs to describe the FDE for the caller of cur_context.  The
	 first time through the loop, that means __cxa_throw.  */
      code = uw_frame_state_for (&cur_context, &fs);

      if (code == _URC_END_OF_STACK)
	/* Hit end of stack with no handler found.  */
	return _URC_END_OF_STACK;

      if (code != _URC_NO_REASON)
	/* Some error encountered.  Usually the unwinder doesn't
	   diagnose these and merely crashes.  */
	return _URC_FATAL_PHASE1_ERROR;

      /* Unwind successful.  Run the personality routine, if any.  */
      if (fs.personality)
	{
	  code = (*fs.personality) (1, _UA_SEARCH_PHASE, exc->exception_class,
				    exc, &cur_context);
	  if (code == _URC_HANDLER_FOUND)
	    break;
	  else if (code != _URC_CONTINUE_UNWIND)
	    return _URC_FATAL_PHASE1_ERROR;
	}

      /* Update cur_context to describe the same frame as fs.  */
      uw_update_context (&cur_context, &fs);
    }

  /* Indicate to _Unwind_Resume and associated subroutines that this
     is not a forced unwind.  Further, note where we found a handler.  */
  exc->private_1 = 0;
  exc->private_2 = uw_identify_context (&cur_context);

#if defined __e2k__
  /* Now that the chain stack cache is shared between `{THIS,CUR}_CONTEXT' it
     may have been changed while updating the latter above. Ensure that it
     matches THIS_CONTEXT prior to starting phase 2.  */
  reinit_chain_stack_cache (&this_context);
#endif /* defined __e2k__  */

  cur_context = this_context;
  code = _Unwind_RaiseException_Phase2 (exc, &cur_context);
  if (code != _URC_INSTALL_CONTEXT)
    return code;

  uw_install_context (&this_context, &cur_context);
}


/* Subroutine of _Unwind_ForcedUnwind also invoked from _Unwind_Resume.  */

static _Unwind_Reason_Code
_Unwind_ForcedUnwind_Phase2 (struct _Unwind_Exception *exc,
			     struct _Unwind_Context *context)
{
#if ! (defined (__e2k__) && defined (__ptr128__))
  _Unwind_Stop_Fn stop = (_Unwind_Stop_Fn) (_Unwind_Ptr) exc->private_1;
  void *stop_argument = (void *) (_Unwind_Ptr) exc->private_2;
#else /* defined (__e2k__) && defined (__ptr128__)  */
  _Unwind_Stop_Fn stop = (_Unwind_Stop_Fn) exc->private_1;
  void *stop_argument = exc->private_2;
#endif /* defined (__e2k__) && defined (__ptr128__)  */
  _Unwind_Reason_Code code, stop_code;

  while (1)
    {
      _Unwind_FrameState fs;
      int action;

      /* Set up fs to describe the FDE for the caller of cur_context.  */
      code = uw_frame_state_for (context, &fs);
      if (code != _URC_NO_REASON && code != _URC_END_OF_STACK)
	return _URC_FATAL_PHASE2_ERROR;

      /* Unwind successful.  */
      action = _UA_FORCE_UNWIND | _UA_CLEANUP_PHASE;
      if (code == _URC_END_OF_STACK)
	action |= _UA_END_OF_STACK;
      stop_code = (*stop) (1, action, exc->exception_class, exc,
			   context, stop_argument);
      if (stop_code != _URC_NO_REASON)
	return _URC_FATAL_PHASE2_ERROR;

      /* Stop didn't want to do anything.  Invoke the personality
	 handler, if applicable, to run cleanups.  */
      if (code == _URC_END_OF_STACK)
	break;
	
      if (fs.personality)
	{
	  code = (*fs.personality) (1, _UA_FORCE_UNWIND | _UA_CLEANUP_PHASE,
				    exc->exception_class, exc, context);
	  if (code == _URC_INSTALL_CONTEXT)
	    break;
	  if (code != _URC_CONTINUE_UNWIND) 
	    return _URC_FATAL_PHASE2_ERROR;
	}

      /* Update cur_context to describe the same frame as fs, and discard
	 the previous context if necessary.  */
      uw_advance_context (context, &fs);
    }

  return code;
}


/* Raise an exception for forced unwinding.  */

_Unwind_Reason_Code LIBGCC2_UNWIND_ATTRIBUTE
_Unwind_ForcedUnwind (struct _Unwind_Exception *exc,
		      _Unwind_Stop_Fn stop, void * stop_argument)
{
  struct _Unwind_Context this_context, cur_context;
  _Unwind_Reason_Code code;
#if defined __e2k__
  e2k_mem_crs_t chain_stack[CHAIN_STACK_CACHE_LEN];
#endif /* defined __e2k__  */

  uw_init_context (&this_context
#if defined __e2k__
		   , chain_stack
#endif /* defined __e2k__  */
		   );
  cur_context = this_context;

#if ! (defined (__e2k__) && defined (__ptr128__))
  exc->private_1 = (_Unwind_Ptr) stop;
  exc->private_2 = (_Unwind_Ptr) stop_argument;
#else /* defined (__e2k__) && defined (__ptr128__)  */
  exc->private_1 = (void *) stop;
  exc->private_2 = stop_argument;
#endif /* defined (__e2k__) && defined (__ptr128__)  */

  code = _Unwind_ForcedUnwind_Phase2 (exc, &cur_context);
  if (code != _URC_INSTALL_CONTEXT)
    return code;

  uw_install_context (&this_context, &cur_context);
}


/* Resume propagation of an existing exception.  This is used after
   e.g. executing cleanup code, and not to implement rethrowing.  */

void LIBGCC2_UNWIND_ATTRIBUTE
_Unwind_Resume (struct _Unwind_Exception *exc)
{
  struct _Unwind_Context this_context, cur_context;
  _Unwind_Reason_Code code;
#if defined __e2k__
  e2k_mem_crs_t chain_stack[CHAIN_STACK_CACHE_LEN];
#endif /* defined __e2k__  */


  uw_init_context (&this_context
#if defined __e2k__
		   , chain_stack
#endif /* defined __e2k__  */
		   );
  cur_context = this_context;

  /* Choose between continuing to process _Unwind_RaiseException
     or _Unwind_ForcedUnwind.  */
  if (exc->private_1 == 0)
    code = _Unwind_RaiseException_Phase2 (exc, &cur_context);
  else
    code = _Unwind_ForcedUnwind_Phase2 (exc, &cur_context);

  gcc_assert (code == _URC_INSTALL_CONTEXT);

  uw_install_context (&this_context, &cur_context);
}


/* Resume propagation of an FORCE_UNWIND exception, or to rethrow
   a normal exception that was handled.  */

_Unwind_Reason_Code LIBGCC2_UNWIND_ATTRIBUTE
_Unwind_Resume_or_Rethrow (struct _Unwind_Exception *exc)
{
  struct _Unwind_Context this_context, cur_context;
  _Unwind_Reason_Code code;
#if defined __e2k__
  e2k_mem_crs_t chain_stack[CHAIN_STACK_CACHE_LEN];
#endif /* defined __e2k__  */

  /* Choose between continuing to process _Unwind_RaiseException
     or _Unwind_ForcedUnwind.  */
  if (exc->private_1 == 0)
    return _Unwind_RaiseException (exc);

  uw_init_context (&this_context
#if defined __e2k__
		   , chain_stack
#endif /* defined __e2k__  */
		   );
  cur_context = this_context;

  code = _Unwind_ForcedUnwind_Phase2 (exc, &cur_context);

  gcc_assert (code == _URC_INSTALL_CONTEXT);

  uw_install_context (&this_context, &cur_context);
}


/* A convenience function that calls the exception_cleanup field.  */

void
_Unwind_DeleteException (struct _Unwind_Exception *exc)
{
  if (exc->exception_cleanup)
    (*exc->exception_cleanup) (_URC_FOREIGN_EXCEPTION_CAUGHT, exc);
}


/* Perform stack backtrace through unwind data.  */

_Unwind_Reason_Code LIBGCC2_UNWIND_ATTRIBUTE
_Unwind_Backtrace(_Unwind_Trace_Fn trace, void * trace_argument)
{
  struct _Unwind_Context context;
  _Unwind_Reason_Code code;
#if defined __e2k__
  e2k_mem_crs_t chain_stack[CHAIN_STACK_CACHE_LEN];
#endif /* defined __e2k__  */

  uw_init_context (&context
#if defined __e2k__
		   , chain_stack
#endif /* defined __e2k__  */
		   );

  while (1)
    {
      _Unwind_FrameState fs;

      /* Set up fs to describe the FDE for the caller of context.  */
      code = uw_frame_state_for (&context, &fs);
      if (code != _URC_NO_REASON && code != _URC_END_OF_STACK)
	return _URC_FATAL_PHASE1_ERROR;

      /* Call trace function.  */
      if ((*trace) (&context, trace_argument) != _URC_NO_REASON)
	return _URC_FATAL_PHASE1_ERROR;

      /* We're done at end of stack.  */	
      if (code == _URC_END_OF_STACK)
	break;

      /* Update context to describe the same frame as fs.  */
      uw_update_context (&context, &fs);
    }

  return code;
}
