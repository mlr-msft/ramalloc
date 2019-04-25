/* ex: set softtabstop=3 shiftwidth=3 expandtab: */

/* This file is part of the *ramalloc* project at <http://fmrl.org>.
 * Copyright (c) 2011, Michael Lowell Roberts.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  * Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  * Neither the name of the copyright holder nor the names of
 *  contributors may be used to endorse or promote products derived
 *  from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#ifndef RAMCOMPAT_H_IS_INCLUDED
#define RAMCOMPAT_H_IS_INCLUDED

#include "compiler.h"
#include "reply.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ramcompat_mktag_t)(void **, const void *, size_t, void *);

RAMSYS_EXPORT void * ramcompat_malloc(size_t size_arg);
RAMSYS_EXPORT void ramcompat_free(void *ptr_arg);
RAMSYS_EXPORT void * ramcompat_calloc(size_t count_arg, size_t size_arg);
RAMSYS_EXPORT void * ramcompat_realloc(void *ptr_arg, size_t size_arg);
RAMSYS_EXPORT ram_reply_t ramcompat_tag(void **tag_out, const void *ptr_arg, ramcompat_mktag_t mktag_in, void *context_in);

#ifdef RAM_WANT_OVERRIDE
RAMSYS_EXPORT void * malloc(size_t size_arg);
RAMSYS_EXPORT void free(void *ptr_arg);
RAMSYS_EXPORT void * calloc(size_t count_arg, size_t size_arg);
RAMSYS_EXPORT void * realloc(void *ptr_arg, size_t size_arg);
#endif // RAM_WANT_OVERRIDE

#ifdef __cplusplus
}
#endif

#endif /* RAMCOMPAT_H_IS_INCLUDED */
