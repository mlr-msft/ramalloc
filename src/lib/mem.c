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

#include <ramalloc/mem.h>
#include <ramalloc/sys.h>
#include <stdlib.h>

#define __USE_GNU // required for `RTLD_NEXT` symbol.
#include <dlfcn.h>

#define RAMMEM_ISPAGE(Ptr, Mask) (0 == ((uintptr_t)(Ptr) & ~(Mask)))

typedef struct rammem_globals
{
   rammem_malloc_t rammemg_supmalloc;
   rammem_free_t rammemg_supfree;
   rammem_msize_t rammemg_supmsize;
   size_t rammemg_mmapgran;
   size_t rammemg_pagesize;
   uintptr_t rammemg_pagemask;
   int rammemg_initflag;
} rammem_globals_t;

static rammem_globals_t rammem_theglobals;

extern void *__libc_malloc(size_t);
extern void __libc_free(void *);
extern size_t __libc_malloc_usable_size(void *);

static ram_reply_t overridden_malloc_usable_size(rammem_msize_t *fn_out);

ram_reply_t rammem_initialize(rammem_malloc_t supmalloc_arg,
      rammem_free_t supfree_arg, rammem_msize_t supmsize_arg)
{
   /* i don't support redundant calls to this function yet. */
   if (rammem_theglobals.rammemg_initflag)
      return RAM_REPLY_UNSUPPORTED;
   else
   {
      if (NULL == supmalloc_arg) {
         rammem_theglobals.rammemg_supmalloc = &__libc_malloc;
      } else {
         rammem_theglobals.rammemg_supmalloc = supmalloc_arg;
      }

      if (NULL == supfree_arg) {
         rammem_theglobals.rammemg_supfree = &__libc_free;
      }
      else {
         rammem_theglobals.rammemg_supfree = supfree_arg;
      }

      if (NULL == supmsize_arg) {
         RAM_FAIL_TRAP(overridden_malloc_usable_size(&rammem_theglobals.rammemg_supmsize));
      }
      else {
         rammem_theglobals.rammemg_supmsize = supmsize_arg;
      }

      RAM_FAIL_TRAP(ramsys_pagesize(&rammem_theglobals.rammemg_pagesize));
      RAM_FAIL_TRAP(ramsys_mmapgran(&rammem_theglobals.rammemg_mmapgran));
      /* something's wrong with the platform-specific modules if any fields
       * in the structure are zero. */
      RAM_FAIL_EXPECT(RAM_REPLY_INSANE, rammem_theglobals.rammemg_mmapgran);
      RAM_FAIL_EXPECT(RAM_REPLY_INSANE, rammem_theglobals.rammemg_pagesize);
      /* i depend upon the granularity being evenly divisible by the
       * hardware page size. */
      RAM_FAIL_EXPECT(RAM_REPLY_UNSUPPORTED,
            0 == (rammem_theglobals.rammemg_mmapgran %
                  rammem_theglobals.rammemg_pagesize));

      /* i calculate the mask needed to obtain the page's address, given an
       * address on that page. */
      rammem_theglobals.rammemg_pagemask =
            ~(rammem_theglobals.rammemg_pagesize - 1);

      rammem_theglobals.rammemg_initflag = 1;
      return RAM_REPLY_OK;
   }
}

void * rammem_supmalloc(size_t size_arg)
{
   if (rammem_theglobals.rammemg_initflag) {
      return rammem_theglobals.rammemg_supmalloc(size_arg);
   }

   return __libc_malloc(size_arg);
}

void rammem_supfree(void *ptr_arg)
{
   if (rammem_theglobals.rammemg_initflag) {
      rammem_theglobals.rammemg_supfree(ptr_arg);
      return;
   }

   __libc_free(ptr_arg);
}

ram_reply_t rammem_supmsize(size_t *size_out, void *ptr_in)
{
   RAM_FAIL_NOTNULL(size_out);

   if (rammem_theglobals.rammemg_initflag) {
      *size_out = rammem_theglobals.rammemg_supmsize(ptr_in);
   } else {
      rammem_msize_t fn;

      RAM_FAIL_TRAP(overridden_malloc_usable_size(&fn));
      *size_out = fn(ptr_in);
   }

   if (0 == *size_out) {
      return RAM_REPLY_CRTFAIL;
   }

   return RAM_REPLY_OK;
}

ram_reply_t rammem_pagesize(size_t *pgsz_arg)
{
   RAM_FAIL_NOTNULL(pgsz_arg);
   *pgsz_arg = 0;
   RAM_FAIL_EXPECT(RAM_REPLY_INCONSISTENT,
         rammem_theglobals.rammemg_initflag);

   *pgsz_arg = rammem_theglobals.rammemg_pagesize;

   return RAM_REPLY_OK;
}

ram_reply_t rammem_mmapgran(size_t *mg_arg)
{
   RAM_FAIL_NOTNULL(mg_arg);
   *mg_arg = 0;
   RAM_FAIL_EXPECT(RAM_REPLY_INCONSISTENT,
         rammem_theglobals.rammemg_initflag);

   *mg_arg = rammem_theglobals.rammemg_mmapgran;

   return RAM_REPLY_OK;
}

ram_reply_t rammem_ispage(int *ispage_arg, const void *ptr_arg)
{
   RAM_FAIL_NOTNULL(ispage_arg);
   *ispage_arg = 0;
   RAM_FAIL_EXPECT(RAM_REPLY_INCONSISTENT,
         rammem_theglobals.rammemg_initflag);

   *ispage_arg = RAMMEM_ISPAGE(ptr_arg, rammem_theglobals.rammemg_pagemask);

   return RAM_REPLY_OK;
}

ram_reply_t rammem_getpage(char **page_arg, void *ptr_arg)
{
   RAM_FAIL_NOTNULL(page_arg);
   *page_arg = 0;
   RAM_FAIL_NOTNULL(ptr_arg);
   RAM_FAIL_EXPECT(RAM_REPLY_INCONSISTENT,
         rammem_theglobals.rammemg_initflag);

   *page_arg = (char *)((uintptr_t)ptr_arg &
         rammem_theglobals.rammemg_pagemask);

   return RAM_REPLY_OK;
}

// todo: abstract this away.
static ram_reply_t overridden_malloc_usable_size(rammem_msize_t *fn_out) {
   RAM_FAIL_NOTNULL(fn_out);

   /* from `man dlsym`: the correct way to test for an error is to call
    * dlerror() to clear any old error conditions, then call dlsym(), and
    * then call dlerror() again, saving its return value into a variable,
    * and check whether this saved value is not NULL */
   dlerror();
   *fn_out = dlsym(RTLD_NEXT, "malloc_usable_size");
   if (NULL == *fn_out) {
      char *err = NULL;

      err = dlerror();
      if (NULL == err) {
         return RAM_REPLY_CRTFAIL;
      }
   }

   return RAM_REPLY_OK;
}

