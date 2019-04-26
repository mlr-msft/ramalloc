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
#include <uthash.h>

struct tag {
   void *tag_ptr;
   size_t tag_length;
   void *tag_value;
   UT_hash_handle hh;
};

static struct tag *tags = NULL;

void * ramcompat_malloc(size_t size_arg) {
   ram_reply_t e = RAM_REPLY_INSANE;
   void *p = NULL;

   if (0 == size_arg) {
      return rammem_supmalloc(size_arg);
   }

   e = ram_default_acquire(&p, size_arg);
   switch (e) {
      default:
         errno = (int)e;
         return NULL;
      case RAM_REPLY_OK:
         return p;
      case RAM_REPLY_UNINITIALIZED:
      case RAM_REPLY_RANGEFAIL:
         /* `ram_default_acquire()` will return `RAM_REPLY_RANGEFAIL` if the
          * allocator cannot accomidate an object of size 'size_arg' (i.e.
          * too big or too small). i can defer to the supplimental allocator
          * for this. */
         return rammem_supmalloc(size_arg);
         break;
   }
}

void ramcompat_free(void *ptr_arg) {
   ram_reply_t e = RAM_REPLY_INSANE;

   if (NULL == ptr_arg) {
      rammem_supfree(ptr_arg);
      return;
   }

   e = ram_default_discard(ptr_arg);
   switch (e)
   {
   default:
      errno = (int)e;
      return;
   case RAM_REPLY_OK:
      return;
   case RAM_REPLY_UNINITIALIZED:
   case RAM_REPLY_NOTFOUND:
      /* `ram_default_query()` will return `RAM_REPLY_NOTFOUND` if `ptr_arg`
       * was allocated with a different allocator. */
      rammem_supfree(ptr_arg);
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

   if (0 == size_arg) {
      ramcompat_free(ptr_arg);
      return NULL;
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

size_t ramcompat_msize(void *ptr_in) {
   ram_reply_t e = RAM_REPLY_INSANE;
   size_t sz = 0;

   if (NULL != ptr_in) {
      e = ram_default_query(&sz, ptr_in);
      switch (e)
      {
      default:
         errno = (int)e;
         return 0;
      case RAM_REPLY_OK:
         return sz;
      case RAM_REPLY_UNINITIALIZED:
      case RAM_REPLY_NOTFOUND:
         /* `ram_default_query()` will return `RAM_REPLY_NOTFOUND` if
          *`ptr_arg` was allocated with a different allocator. */
         break;
      }
   }

   e = rammem_supmsize(&sz, ptr_in);
   if (RAM_REPLY_OK != e) {
      errno = (int)e;
      return 0;
   }

   return sz;
}

ram_reply_t ramcompat_tag(void **tag_out, const void *ptr_in, ramcompat_mktag_t mktag_in, void *context_in) {
   struct tag *tag = NULL;

   RAM_FAIL_NOTNULL(tag_out);
   *tag_out = NULL;

   HASH_FIND_PTR(tags, &ptr_in, tag);
   if (NULL == tag) {
      struct tag t;

      t.tag_length = ramcompat_msize((void *)ptr_in);
      RAM_FAIL_EXPECT(RAM_REPLY_CRTFAIL, 0 != t.tag_length);
      RAM_FAIL_TRAP(mktag_in(&t.tag_value, ptr_in, t.tag_length, context_in));

      tag = ramcompat_malloc(sizeof(*tag));
      *tag = t;
      HASH_ADD_PTR(tags, tag_ptr, tag);
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

size_t malloc_usable_size(void *ptr_in) {
   return ramcompat_msize(ptr_in);
}

#endif // RAM_WANT_OVERRIDE

