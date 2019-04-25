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
#include <ramalloc/default.h>
#include <ramalloc/mem.h>
#include <memory.h>
#include <string.h>

struct tag {
   size_t tag_sz;
   void *tag_userdata;
   char tag_moar[];
};

void * ramcompat_malloc(size_t size_arg)
{
   ram_reply_t e = RAM_REPLY_INSANE;
   void *p = NULL;

   if (0 == size_arg || !rammem_isinit()) {
      return rammem_supmalloc(size_arg);
   }

   e = ram_default_acquire(&p, size_arg);
   switch (e)
   {
      default:
         return NULL;
      case RAM_REPLY_OK:
         return p;
      case RAM_REPLY_RANGEFAIL: {
         /* ramalloc will return `RAM_REPLY_RANGEFAIL` if the allocator
            * cannot accomidate an object of size `size_arg` (i.e. too big
            * or too small). i can defer to the supplimental allocator for
            * this. */
         char * const q = (char *)rammem_supmalloc(size_arg + sizeof(struct tag));

         memset(q, 0, sizeof(struct tag));
         return q + sizeof(struct tag);
      }
   }
}

void ramcompat_free(void *ptr_arg)
{
   ram_reply_t e = RAM_REPLY_INSANE;
   size_t sz = 0;

   if (NULL == ptr_arg || !rammem_isinit()) {
      rammem_supfree(ptr_arg);
      return;
   }

   e = ram_default_query(&sz, ptr_arg);
   switch (e)
   {
      default:
         /* i don't have any other avenue through which i can report an
            * error. */
         ram_fail_panic("i got an unexpected eror from ramdefault_query().");
         return;
      case RAM_REPLY_OK:
         e = ram_default_discard(ptr_arg);
         if (RAM_REPLY_OK != e)
            ram_fail_panic("i got an unexpected eror from ramdefault_discard().");
         return;
      case RAM_REPLY_NOTFOUND: {
         /* ramalloc will return `RAM_REPLY_NOTFOUND` if `ptr_arg` was
            * allocated with a different allocator, which we assume is the
            * supplimental allocator. */
         char * const p = (char *)ptr_arg;

         rammem_supfree(p - sizeof(struct tag));
         return;
      }
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
   ram_reply_t e = RAM_REPLY_INSANE;
   size_t old_sz = 0;
   void *new_ptr = NULL;
   size_t small_sz = 0;

   if (NULL == ptr_arg) {
      return ramcompat_malloc(size_arg);
   }

   if (!rammem_isinit()) {
      return rammem_suprealloc(ptr_arg, size_arg);
   }

   e = ram_default_query(&old_sz, ptr_arg);
   switch (e)
   {
   default:
      /* i don't have any other avenue through which i can report an error. */
      ram_fail_panic("i got an unexpected eror from ramdefault_query().");
      return NULL;
   case RAM_REPLY_OK:
      break;
   case RAM_REPLY_NOTFOUND:
      /* `ram_default_query()` will return `RAM_REPLY_NOTFOUND` if `ptr_arg`
       * was allocated with a different allocator. */
      return rammem_suprealloc(ptr_arg, size_arg);
   }

   new_ptr = ramcompat_malloc(size_arg);
   if (NULL == new_ptr) {
      ramcompat_free(ptr_arg);
      return NULL;
   }

   small_sz = old_sz > size_arg ? size_arg : old_sz;
   memmove(new_ptr, ptr_arg, small_sz);
   ramcompat_free(ptr_arg);
   return RAM_REPLY_OK;
}

ram_reply_t ramcompat_stoud(const void *ptr_arg, ramcompat_mkuserdata_t init_arg, void *context_arg) {
   ram_reply_t e = RAM_REPLY_INSANE;
   size_t sz = 0;
   char *p = NULL;
   struct tag *tag = NULL;

   RAM_FAIL_NOTNULL(ptr_arg);
   RAM_FAIL_NOTNULL(init_arg);

   e = ram_default_query(&sz, (void *)ptr_arg);
   switch (e) {
      default:
         /* i don't have any other avenue through which i can report an
          * error. */
         ram_fail_panic("i got an unexpected eror from ramdefault_query().");
         return RAM_REPLY_INSANE;
      case RAM_REPLY_OK:
         RAM_FAIL_TRAP(ram_default_stoud(ptr_arg, init_arg, context_arg));
         return RAM_REPLY_OK;
      case RAM_REPLY_NOTFOUND:
         /* `ram_default_query()` will return `RAM_REPLY_NOTFOUND` if
          * `ptr_arg` was allocated with a different allocator. */
         break;
   }

   p = (char *)ptr_arg;
   tag = (struct tag *)(p - sizeof(struct tag));
   RAM_FAIL_TRAP(init_arg(&tag->tag_userdata, ptr_arg, tag->tag_sz, context_arg));
   return 0;
}

ram_reply_t ramcompat_rclud(void **userdata_arg, const void *ptr_arg) {
   ram_reply_t e = RAM_REPLY_INSANE;
   size_t sz = 0;
   char *p = NULL;
   struct tag *tag = NULL;

   RAM_FAIL_NOTNULL(userdata_arg);
   *userdata_arg = NULL;
   RAM_FAIL_NOTNULL(ptr_arg);

   e = ram_default_query(&sz, (void *)ptr_arg);
   switch (e) {
      default:
         /* i don't have any other avenue through which i can report an
          * error. */
         ram_fail_panic("i got an unexpected eror from ramdefault_query().");
         return RAM_REPLY_INSANE;
      case RAM_REPLY_OK:
         RAM_FAIL_TRAP(ram_default_rclud(userdata_arg, ptr_arg));
         return RAM_REPLY_OK;
      case RAM_REPLY_NOTFOUND:
         /* `ram_default_query()` will return `RAM_REPLY_NOTFOUND` if
          * `ptr_arg` was allocated with a different allocator. */
         break;
   }

   p = (char *)ptr_arg;
   tag = (struct tag *)(p - sizeof(struct tag));
   *userdata_arg = tag->tag_userdata;
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
