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

#include "shared/parseargs.h"
#include "shared/test.h"
#include <ramalloc/ramalloc.h>
#include <ramalloc/algn.h>
#include <ramalloc/misc.h>
#include <ramalloc/thread.h>
#include <ramalloc/barrier.h>
#include <ramalloc/stdint.h>
#include <ramalloc/annotate.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include "ramalloc/cast.h"

#define DEFAULT_ALLOCATION_COUNT 1024 * 100
#define DEFAULT_MINIMUM_ALLOCATION_SIZE 8
#define DEFAULT_MAXIMUM_ALLOCATION_SIZE 8
#define DEFAULT_MALLOC_CHANCE 30

typedef struct extra
{
   ramalgn_pool_t e_thepool;
   ramsig_signature_t e_sig;
} extra_t;

static ram_reply_t main2(int argc, char *argv[]);
static ram_reply_t initdefaults(ramtest_params_t *params_arg);
static ram_reply_t runtest(const ramtest_params_t *params_arg);
static ram_reply_t runtest2(const ramtest_params_t *params_arg,
      extra_t *extra_arg);
static ram_reply_t getpool(ramalgn_pool_t **pool_arg, void *extra_arg,
      size_t threadidx_arg);
static ram_reply_t acquire(ramtest_allocdesc_t *desc_arg,
      size_t size_arg, void *extra_arg, size_t threadidx_arg);
static ram_reply_t release(ramtest_allocdesc_t *desc_arg);
static ram_reply_t query(void **pool_arg, size_t *size_arg,
      void *ptr_arg, void *extra_arg);
static ram_reply_t flush(void *extra_arg, size_t threadidx_arg);
static ram_reply_t check(void *extra_arg, size_t threadidx_arg);

int main(int argc, char *argv[])
{
   ram_reply_t e = RAM_REPLY_INSANE;
   size_t unused = 0;

   e = main2(argc, argv);
   if (RAM_REPLY_OK != e)
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr, "fail (%d).", e));
   if (RAM_REPLY_INPUTFAIL == e)
   {
      usage(e, argc, argv);
      ram_fail_panic("unreachable code.");
      return RAM_REPLY_INSANE;
   }
   else
      return e;
}

ram_reply_t main2(int argc, char *argv[])
{
   ramtest_params_t testparams;
   ram_reply_t e = RAM_REPLY_INSANE;

   RAM_FAIL_TRAP(ram_initialize(NULL, NULL));

   RAM_FAIL_TRAP(initdefaults(&testparams));
   e = parseargs(&testparams, argc, argv);
   switch (e)
   {
   default:
      RAM_FAIL_TRAP(e);
   case RAM_REPLY_OK:
      break;
   case RAM_REPLY_INPUTFAIL:
      return e;
   }

   e = runtest(&testparams);
   switch (e)
   {
   default:
      RAM_FAIL_TRAP(e);
   case RAM_REPLY_OK:
      break;
   case RAM_REPLY_INPUTFAIL:
      return e;
   }

   return RAM_REPLY_OK;
}

ram_reply_t initdefaults(ramtest_params_t *params_arg)
{
   RAM_FAIL_NOTNULL(params_arg);
   memset(params_arg, 0, sizeof(*params_arg));

   params_arg->ramtestp_alloccount = DEFAULT_ALLOCATION_COUNT;
   /* aligned pools don't support multi-threaded access. */
   params_arg->ramtestp_threadcount = 1;
   params_arg->ramtestp_mallocchance = DEFAULT_MALLOC_CHANCE;
   params_arg->ramtestp_minsize = DEFAULT_MINIMUM_ALLOCATION_SIZE;
   params_arg->ramtestp_maxsize = DEFAULT_MAXIMUM_ALLOCATION_SIZE;

   return RAM_REPLY_OK;
}

ram_reply_t getpool(ramalgn_pool_t **pool_arg, void *extra_arg,
      size_t threadidx_arg)
{
   extra_t *x = NULL;

   RAMANNOTATE_UNUSEDARG(threadidx_arg);

   RAM_FAIL_NOTNULL(pool_arg);
   *pool_arg = NULL;
   RAM_FAIL_NOTNULL(extra_arg);
   x = (extra_t *)extra_arg;

   *pool_arg = &x->e_thepool;
   return RAM_REPLY_OK;
}

ram_reply_t acquire(ramtest_allocdesc_t *desc_arg,
      size_t size_arg, void *extra_arg, size_t threadidx_arg)
{
   ramalgn_pool_t *pool = NULL;
   void *p = NULL;

   RAM_FAIL_NOTNULL(desc_arg);
   memset(desc_arg, 0, sizeof(*desc_arg));
   RAM_FAIL_NOTZERO(size_arg);

   RAM_FAIL_TRAP(getpool(&pool, extra_arg, threadidx_arg));
   RAM_FAIL_TRAP(ramalgn_acquire(&p, pool));
   desc_arg->ramtestad_ptr = (char *)p;
   desc_arg->ramtestad_pool = pool;
   desc_arg->ramtestad_sz = size_arg;

   return RAM_REPLY_OK;
}

ram_reply_t release(ramtest_allocdesc_t *desc_arg)
{
   RAM_FAIL_NOTNULL(desc_arg);

   RAM_FAIL_TRAP(ramalgn_release(desc_arg->ramtestad_ptr));

   return RAM_REPLY_OK;
}

ram_reply_t query(void **pool_arg, size_t *size_arg, void *ptr_arg,
      void *extra_arg)
{
   ramalgn_pool_t *pool = NULL;
   ram_reply_t e = RAM_REPLY_INSANE;
   extra_t *x = NULL;
   const ramalgn_tag_t *tag = NULL;
   ramsig_signature_t sig = {0};

   RAM_FAIL_NOTNULL(pool_arg);
   *pool_arg = NULL;
   RAM_FAIL_NOTNULL(extra_arg);

   x = (extra_t *)extra_arg;

   e = ramalgn_query(&pool, ptr_arg);
   switch (e)
   {
   default:
      RAM_FAIL_TRAP(e);
      return RAM_REPLY_INSANE;
   case RAM_REPLY_OK:
      break;
   case RAM_REPLY_NOTFOUND:
      return e;
   }

   RAM_FAIL_TRAP(ramalgn_gettag(&tag, pool));
   /* note: `size_t` and `uintptr_t` should be identical types. */
   RAM_FAIL_TRAP(ram_cast_sztou32(&sig.ramsigs_n, tag->ramalgnt_values[0]));
   if (0 == RAMSIG_CMP(x->e_sig, sig))
   {
      *pool_arg = pool;
      RAM_FAIL_TRAP(ramalgn_getgranularity(size_arg, pool));
      return RAM_REPLY_OK;
   }
   else
      return RAM_REPLY_NOTFOUND;
}

ram_reply_t flush(void *extra_arg, size_t threadidx_arg)
{
   RAMANNOTATE_UNUSEDARG(extra_arg);
   RAMANNOTATE_UNUSEDARG(threadidx_arg);
   /* algn pools don't support the flush operation. */
   return RAM_REPLY_OK;
}

ram_reply_t check(void *extra_arg, size_t threadidx_arg)
{
   ramalgn_pool_t *pool = NULL;

   RAM_FAIL_TRAP(getpool(&pool, extra_arg, threadidx_arg));
   RAM_FAIL_TRAP(ramalgn_chkpool(pool));

   return RAM_REPLY_OK;
}

ram_reply_t runtest(const ramtest_params_t *params_arg)
{
   ram_reply_t e = RAM_REPLY_INSANE;
   extra_t x = {0};

   RAM_FAIL_NOTNULL(params_arg);

   e = runtest2(params_arg, &x);

   return e;
}

ram_reply_t runtest2(const ramtest_params_t *params_arg,
      extra_t *extra_arg)
{
   ramtest_params_t testparams = {0};
   ramalgn_tag_t tag = {0};
   size_t unused = 0;

   testparams = *params_arg;
   /* i am responsible for policing the minimum and maximum allocation
    * size here. */
   if (testparams.ramtestp_minsize < sizeof(void *) ||
         testparams.ramtestp_maxsize < sizeof(void *))
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
            "you cannot specify a size smaller than %zu bytes.\n",
            sizeof(void *)));
      return RAM_REPLY_INPUTFAIL;
   }
   /* TODO: shouldn't this test be moved into the framework? */
   if (testparams.ramtestp_minsize > testparams.ramtestp_maxsize)
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
            "please specify a minimum size (%zu bytes) that is smaller than "
            "or equal to the maximum (%zu bytes).\n",
            testparams.ramtestp_minsize, testparams.ramtestp_maxsize));
      return RAM_REPLY_INPUTFAIL;
   }
   /* the algnpool doesn't support multi-threaded access. */
   if (testparams.ramtestp_threadcount > 1)
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
            "the --parallelize option is not supported in this test.\n"));
      return RAM_REPLY_INPUTFAIL;
   }
   if (testparams.ramtestp_minsize < testparams.ramtestp_maxsize)
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
            "warning: this test doesn't support a range of sizes. i will "
            "use the smallest size specified (%zu bytes) for all "
            "allocations.\n", testparams.ramtestp_minsize));
      testparams.ramtestp_maxsize = testparams.ramtestp_minsize;
   }
   /* TODO: how do i determine the maximum allocation size ahead of time? */
   testparams.ramtestp_extra = extra_arg;
   testparams.ramtestp_acquire = &acquire;
   testparams.ramtestp_release = &release;
   testparams.ramtestp_query = &query;
   testparams.ramtestp_flush = &flush;
   testparams.ramtestp_check = &check;

   RAM_FAIL_TRAP(ramsig_init(&extra_arg->e_sig, "TEST"));
   tag.ramalgnt_values[0] = extra_arg->e_sig.ramsigs_n;
   RAM_FAIL_TRAP(ramalgn_mkpool(&extra_arg->e_thepool,
         RAM_WANT_DEFAULTAPPETITE, testparams.ramtestp_minsize, &tag));

   RAM_FAIL_TRAP(ramtest_test(&testparams));

   return RAM_REPLY_OK;
}

