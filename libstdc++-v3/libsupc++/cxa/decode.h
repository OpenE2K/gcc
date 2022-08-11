/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1996-2018 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*
Redistribution and use in source and binary forms are permitted
provided that the above copyright notice and this paragraph are
duplicated in all source code forms.  The name of Edison Design
Group, Inc. may not be used to endorse or promote products derived
from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
Any use of this software is at the user's own risk.
*/
/*
decode.h -- Declarations for decode.c (name demangler for C++).
*/

/* Avoid including these declarations more than once: */
#ifndef DECODE_H
#define DECODE_H 1

void decode_identifier(a_const_char *id,
                       char         *output_buffer,
                       sizeof_t     output_buffer_size,
                       a_boolean    *err,
                       a_boolean    *buffer_overflow_err,
                       sizeof_t     *required_buffer_size);

#endif /* ifndef DECODE_H */


/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
*                                                             /  | |  \       *
* Copyright 1996-2018 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
