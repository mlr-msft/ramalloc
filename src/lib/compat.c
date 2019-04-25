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

#include <ramalloc/compat.h>

#include <errno.h>
#include <memory.h>
#include <ramalloc/cast.h>
#include <ramalloc/default.h>
#include <ramalloc/mem.h>
#include <ramalloc/sig.h>
#include <string.h>

static ramsig_signature_t master_tag_signature = {.ramsigs_s = {'r', 'T', 'A', 'G'}};

struct tag {
   ramsig_signature_t tag_signature;
   void *tag_value;
   size_t tag_length;
   char tag_bytes[];
};

static ram_reply_t erasetag(struct tag *tag_in, size_t length_in);

void * ramcompat_malloc(size_t size_arg) {
   ram_reply_t e = RAM_REPLY_INSANE;
   void *p = NULL;
   size_t sz = 0;
   struct tag *tag = NULL;

   if (0 == size_arg) {
      return rammem_supmalloc(size_arg);
   }

   sz = sizeof(*tag) + size_arg;
   e = ram_default_acquire(&p, sz);
   switch (e) {
      default:
         errno = (int)e;
         return NULL;
      case RAM_REPLY_OK:
         break;
      case RAM_REPLY_RANGEFAIL:
         /* `ram_default_acquire()` will return `RAM_REPLY_RANGEFAIL` if the
          * allocator cannot accomidate an object of size 'size_arg' (i.e.
          * too big or too small). i can defer to the supplimental allocator
          * for this. */
         p = rammem_supmalloc(sz);
         break;
   }

   tag = (struct tag *)p;
   e = erasetag(tag, size_arg);
   if (RAM_REPLY_OK != e) {
      ramcompat_free(p);
      errno = (int)e;
      return NULL;
   }

   return &tag->tag_bytes;
}

void ramcompat_free(void *ptr_arg) {
   ram_reply_t e = RAM_REPLY_INSANE;
   size_t sz = 0;
   struct tag *tag = NULL;

   if (NULL == ptr_arg) {
      rammem_supfree(ptr_arg);
      return;
   }

   tag = RAM_CAST_STRUCTBASE(struct tag, tag_bytes, ptr_arg);
   if (0 != RAMSIG_CMP(master_tag_signature, tag->tag_signature)) {
      ram_fail_panic("i encountered a corrupt tag signature.");
      return;
   }

   e = ram_default_query(&sz, tag);
   switch (e)
   {
   default:
      errno = (int)e;
      return;
   case RAM_REPLY_OK:
      // todo: do i need to call `ram_default_query()` first?
      e = ram_default_discard(tag);
      if (RAM_REPLY_OK != e)
         ram_fail_panic("i got an unexpected error from ramdefault_discard().");
      return;
   case RAM_REPLY_NOTFOUND:
      /* `ram_default_query()` will return `RAM_REPLY_NOTFOUND` if `ptr_arg`
       * was allocated with a different allocator. */
      rammem_supfree(tag);
      return;
   }
}

void * ramcompat_calloc(size_t count_arg, size_t size_arg)
{
   void *p = NULL;
   size_t sz = 0;

   sz = count_arg * size_arg;
   p = ramcompat_malloc(sz);
   if (NULL == p)
      return NULL;
   else
   {
      memset(p, 0, sz);
      return p;
   }
}

void * ramcompat_realloc(void *ptr_arg, size_t size_arg)
{
   size_t old_sz = 0;
   size_t small_sz = 0;
   void *new_ptr = NULL;

   if (NULL == ptr_arg) {
      return ramcompat_malloc(size_arg);
   }

   new_ptr = ramcompat_malloc(size_arg);
   if (NULL == new_ptr) {
      return NULL;
   }

   small_sz = old_sz > size_arg ? size_arg : old_sz;
   memmove(new_ptr, ptr_arg, small_sz);
   ramcompat_free(ptr_arg);
   return new_ptr;
}

ram_reply_t ramcompat_tag(void **tag_out, const void *ptr_arg, ramcompat_mktag_t mktag_in, void *context_in) {
   struct tag *tag = NULL;

   RAM_FAIL_NOTNULL(tag_out);
   *tag_out = NULL;

   tag = RAM_CAST_STRUCTBASE(struct tag, tag_bytes, ptr_arg);
   RAM_FAIL_EXPECT(RAM_REPLY_CORRUPT, 0 == RAMSIG_CMP(master_tag_signature, tag->tag_signature));
   if (NULL == tag->tag_value && NULL != mktag_in) {
      RAM_FAIL_TRAP(mktag_in(&tag->tag_value, &tag->tag_bytes, tag->tag_length, context_in));
   }

   *tag_out = tag->tag_value;
   return RAM_REPLY_OK;
}

#ifdef RAM_WANT_OVERRIDE

void * malloc(size_t size_arg) {
   return ramcompat_malloc(size_arg);
}

void free(void *ptr_arg) {
   return ramcompat_free(ptr_arg);
}

void * calloc(size_t count_arg, size_t size_arg) {
   return ramcompat_calloc(count_arg, size_arg);
}

void * realloc(void *ptr_arg, size_t size_arg) {
   return ramcompat_realloc(ptr_arg, size_arg);
}

#endif // RAM_WANT_OVERRIDE

ram_reply_t erasetag(struct tag *tag_in, size_t length_in) {
   RAM_FAIL_NOTNULL(tag_in);
   RAM_FAIL_NOTZERO(length_in);

   tag_in->tag_signature = master_tag_signature;
   tag_in->tag_length = length_in;
   tag_in->tag_value = NULL;
   return RAM_REPLY_OK;
}
