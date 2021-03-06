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

#include "test.h"
#include <ramalloc/mtx.h>
#include <ramalloc/thread.h>
#include <ramalloc/misc.h>
#include <ramalloc/cast.h>
#include <trio.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

typedef struct ramtest_allocrec
{
   ramtest_allocdesc_t ramtestar_desc;
   rammtx_mutex_t ramtestar_mtx;
} ramtest_allocrec_t;

typedef struct ramtest_test
{
   ramtest_params_t ramtestt_params;
   ramtest_allocrec_t *ramtestt_records;
   ramthread_thread_t *ramtestt_threads;
   rammtx_mutex_t ramtestt_mtx;
   size_t ramtestt_nextrec;
   size_t *ramtestt_sequence;
} ramtest_test_t;

typedef struct ramtest_start
{
   ramtest_test_t *ramtests_test;
   size_t ramtests_threadidx;
} ramtest_start_t;

static ram_reply_t ramtest_inittest(ramtest_test_t *test_arg,
      const ramtest_params_t *params_arg);
static ram_reply_t ramtest_fintest(ramtest_test_t *test_arg);
static ram_reply_t ramtest_inittest2(ramtest_test_t *test_arg,
      const ramtest_params_t *params_arg);
static ram_reply_t ramtest_describe(FILE *out_arg,
      const ramtest_params_t *params_arg);
static ram_reply_t ramtest_test2(ramtest_test_t *test_arg);
static ram_reply_t ramtest_start(ramtest_test_t *test_arg);
static ram_reply_t ramtest_thread(void *arg);
static ram_reply_t ramtest_thread2(ramtest_test_t *test_arg,
      size_t threadidx_arg);
static ram_reply_t ramtest_next(size_t *next_arg,
      ramtest_test_t *test_arg);
static ram_reply_t ramtest_join(ramtest_test_t *test_arg);
static ram_reply_t ramtest_alloc(ramtest_allocdesc_t *desc_arg,
      ramtest_test_t *test_arg, size_t threadidx_arg);
static ram_reply_t ramtest_dealloc(ramtest_allocdesc_t *ptrdesc_arg,
      ramtest_test_t *test_arg, size_t threadidx_arg);
static ram_reply_t ramtest_fill(char *ptr_arg, size_t sz_arg);
static ram_reply_t ramtest_chkfill(char *ptr_arg, size_t sz_arg);
static ram_reply_t ramtest_vfprintf(size_t *count_arg, FILE *file_arg,
      const char *fmt_arg, va_list moar_arg);

#define RAMTEST_SCALERAND(Type, Rand, LowerBound, UpperBound) \
         (((Type)((double)((UpperBound) - (LowerBound)) * (Rand) / \
         ((double)RAND_MAX + 1))) + (LowerBound))

ram_reply_t ramtest_randuint32(uint32_t *result_arg, uint32_t n0_arg,
      uint32_t n1_arg)
{
   uint32_t n = 0;

   RAM_FAIL_NOTNULL(result_arg);
   RAM_FAIL_EXPECT(RAM_REPLY_DISALLOWED, n0_arg < n1_arg);

   /* this assertion tests the boundaries of the scaling formula. */
   assert(RAMTEST_SCALERAND(uint32_t, 0, n0_arg, n1_arg) >= n0_arg);
   assert(RAMTEST_SCALERAND(uint32_t, RAND_MAX, n0_arg, n1_arg) < n1_arg);
   n = RAMTEST_SCALERAND(uint32_t, rand(), n0_arg, n1_arg);

   assert(n >= n0_arg);
   assert(n < n1_arg);
   *result_arg = n;
   return RAM_REPLY_OK;
}

ram_reply_t ramtest_randint32(int32_t *result_arg, int32_t n0_arg,
      int32_t n1_arg)
{
   int32_t n = 0;

   RAM_FAIL_NOTNULL(result_arg);
   RAM_FAIL_EXPECT(RAM_REPLY_DISALLOWED, n0_arg < n1_arg);

   /* this assertion tests the boundaries of the scaling formula. */
   assert(RAMTEST_SCALERAND(int32_t, 0, n0_arg, n1_arg) >= n0_arg);
   assert(RAMTEST_SCALERAND(int32_t, RAND_MAX, n0_arg, n1_arg) < n1_arg);
   n = RAMTEST_SCALERAND(int32_t, rand(), n0_arg, n1_arg);

   assert(n >= n0_arg);
   assert(n < n1_arg);
   *result_arg = n;
   return RAM_REPLY_OK;
}

ram_reply_t ramtest_shuffle(void *array_arg, size_t size_arg,
      size_t count_arg)
{
   char *p = (char *)array_arg;
   size_t i = 0;

   RAM_FAIL_NOTNULL(array_arg);
   RAM_FAIL_NOTZERO(size_arg);

   if (0 < count_arg)
   {
      for (i = count_arg - 1; i > 0; --i)
      {
		 uint32_t j = 0, ii = 0;

		 RAM_FAIL_TRAP(ram_cast_sztou32(&ii, i));
         RAM_FAIL_TRAP(ramtest_randuint32(&j, 0, ii));
         RAM_FAIL_TRAP(rammisc_swap(&p[i * size_arg], &p[j * size_arg],
               size_arg));
      }
   }

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_inittest(ramtest_test_t *test_arg,
      const ramtest_params_t *params_arg)
{
   ram_reply_t e = RAM_REPLY_INSANE;

   RAM_FAIL_NOTNULL(test_arg);

   memset(test_arg, 0, sizeof(*test_arg));
   e = ramtest_inittest2(test_arg, params_arg);
   if (RAM_REPLY_OK == e)
      return RAM_REPLY_OK;
   {
      RAM_FAIL_PANIC(ramtest_fintest(test_arg));
      RAM_FAIL_TRAP(e);
      return RAM_REPLY_INSANE;
   }
}

ram_reply_t ramtest_inittest2(ramtest_test_t *test_arg,
      const ramtest_params_t *params_arg)
{
   size_t i = 0;
   size_t seqlen = 0;
   size_t maxthreads = 0;
   size_t unused = 0;

   RAM_FAIL_NOTNULL(params_arg);
   RAM_FAIL_NOTZERO(params_arg->ramtestp_alloccount);
   RAM_FAIL_EXPECT(RAM_REPLY_RANGEFAIL, params_arg->ramtestp_minsize > 0);
   RAM_FAIL_EXPECT(RAM_REPLY_RANGEFAIL,
         params_arg->ramtestp_minsize <= params_arg->ramtestp_maxsize);
   RAM_FAIL_EXPECT(RAM_REPLY_RANGEFAIL, params_arg->ramtestp_mallocchance >= 0);
   RAM_FAIL_EXPECT(RAM_REPLY_RANGEFAIL, params_arg->ramtestp_mallocchance <= 100);
   RAM_FAIL_NOTNULL(params_arg->ramtestp_acquire);
   RAM_FAIL_NOTNULL(params_arg->ramtestp_release);
   RAM_FAIL_NOTNULL(params_arg->ramtestp_query);
   /* *params_arg->ramtestp_flush* is allowed to be NULL. */
   RAM_FAIL_NOTNULL(params_arg->ramtestp_check);

   RAM_FAIL_TRAP(ramtest_maxthreadcount(&maxthreads));
   RAM_FAIL_EXPECT(RAM_REPLY_DISALLOWED,
         params_arg->ramtestp_threadcount <= maxthreads);

   test_arg->ramtestt_params = *params_arg;
   if (0 == test_arg->ramtestt_params.ramtestp_threadcount)
   {
      RAM_FAIL_TRAP(ramtest_defaultthreadcount(
            &test_arg->ramtestt_params.ramtestp_threadcount));
   }
   test_arg->ramtestt_records =
         calloc(test_arg->ramtestt_params.ramtestp_alloccount,
         sizeof(*test_arg->ramtestt_records));
   RAM_FAIL_EXPECT(RAM_REPLY_RESOURCEFAIL,
         NULL != test_arg->ramtestt_records);
   test_arg->ramtestt_threads =
         calloc(test_arg->ramtestt_params.ramtestp_threadcount,
         sizeof(*test_arg->ramtestt_threads));
   RAM_FAIL_EXPECT(RAM_REPLY_RESOURCEFAIL,
         NULL != test_arg->ramtestt_threads);
   seqlen = test_arg->ramtestt_params.ramtestp_alloccount * 2;
   test_arg->ramtestt_sequence = calloc(seqlen,
         sizeof(*test_arg->ramtestt_sequence));
   RAM_FAIL_EXPECT(RAM_REPLY_RESOURCEFAIL,
         NULL != test_arg->ramtestt_sequence);

   RAM_FAIL_TRAP(rammtx_mkmutex(&test_arg->ramtestt_mtx));
   for (i = 0; i < test_arg->ramtestt_params.ramtestp_alloccount; ++i)
   {
      RAM_FAIL_TRAP(rammtx_mkmutex(
            &test_arg->ramtestt_records[i].ramtestar_mtx));
   }
   /* the sequence array must contain two copies of each index into
    * *test_arg->ramtestt_records*. the first represents an allocation.
    * the second, a deallocation. */
   for (i = 0; i < seqlen; ++i)
      test_arg->ramtestt_sequence[i] = (i / 2);
   /* i shuffle the sequence array to ensure a randomized order of
    * operations. */
   RAM_FAIL_TRAP(ramtest_shuffle(test_arg->ramtestt_sequence,
         sizeof(test_arg->ramtestt_sequence[0]), seqlen));

   if (!test_arg->ramtestt_params.ramtestp_userngseed)
      test_arg->ramtestt_params.ramtestp_rngseed = (unsigned int)time(NULL);
   srand(test_arg->ramtestt_params.ramtestp_rngseed);
   RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
         "[0] i seeded the random generator with the value %u.\n",
         test_arg->ramtestt_params.ramtestp_rngseed));

   test_arg->ramtestt_nextrec = 0;

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_fintest(ramtest_test_t *test_arg)
{
   RAM_FAIL_NOTNULL(test_arg);

   if (NULL != test_arg->ramtestt_records)
      free(test_arg->ramtestt_records);
   if (NULL != test_arg->ramtestt_threads)
   {
      /* TODO: tear down thread structures. */
      free(test_arg->ramtestt_threads);
   }
   if (NULL != test_arg->ramtestt_sequence)
      free(test_arg->ramtestt_sequence);

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_describe(FILE *out_arg,
      const ramtest_params_t *params_arg)
{
   size_t unused = 0;

   RAM_FAIL_NOTNULL(out_arg);
   RAM_FAIL_NOTNULL(params_arg);

   if (params_arg->ramtestp_dryrun)
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
            "you have specified the following test:\n\n"));
   }
   else
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
            "i will run the following test:\n\n"));
   }
   RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
         "%zu allocation(s) (and corresponding deallocations).\n",
         params_arg->ramtestp_alloccount));
   if (1 == params_arg->ramtestp_threadcount)
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
            "this test will not be parallelized.\n"));
   }
   else
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
            "%zu parallel operation(s) allowed.\n",
            params_arg->ramtestp_threadcount));
   }
   RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
         "%d%% of the allocations will be managed by malloc() "
         "and free().\n", params_arg->ramtestp_mallocchance));
   RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
         "allocations will not be smaller than %zu bytes.\n",
         params_arg->ramtestp_minsize));
   RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
         "allocations will not be larger than %zu bytes.\n",
         params_arg->ramtestp_maxsize));
   if (params_arg->ramtestp_userngseed)
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
            "the random number generator will use seed %u.\n",
            params_arg->ramtestp_rngseed));
   }
   else
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
            "the random number generator will use a randomly "
            "selected seed.\n"));
   }
#if RAM_WANT_OVERCONFIDENT
   RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
         "warning: this is an overconfident build, so the results cannot "
         "be trusted. rebuild with RAMOPT_UNSUPPORTED_OVERCONFIDENT "
         "#define'd as 0 if you wish to have reliable results.\n"));
#endif
   if (params_arg->ramtestp_dryrun)
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg,
            "\nto run this test, omit the --dry-run option.\n"));
   }
   else
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, out_arg, "-----\n"));

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_test(const ramtest_params_t *params_arg)
{
   ram_reply_t e = RAM_REPLY_INSANE;
   ramtest_test_t test = {0};
   size_t unused = 0;

   RAM_FAIL_NOTNULL(params_arg);

   RAM_FAIL_TRAP(ramtest_describe(stderr, params_arg));

   /* if a dry run has been specified, i'll quit now. */
   if (params_arg->ramtestp_dryrun)
      return RAM_REPLY_OK;

   RAM_FAIL_TRAP(ramtest_inittest(&test, params_arg));

   e = ramtest_test2(&test);
   RAM_FAIL_TRAP(ram_fail_accumulate(&e, ramtest_fintest(&test)));

   if (RAM_REPLY_OK == e)
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
            "[0] the test succeeded.\n"));
      return RAM_REPLY_OK;
   }
   else
   {
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
            "[0] the test failed (reply code %d).\n", e));
      RAM_FAIL_TRAP(e);
      return RAM_REPLY_INSANE;
   }
}

ram_reply_t ramtest_test2(ramtest_test_t *test_arg)
{
   size_t unused = 0;

   RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
         "[0] beginning test...\n"));

   RAM_FAIL_TRAP(ramtest_start(test_arg));
   RAM_FAIL_TRAP(ramtest_join(test_arg));

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_start(ramtest_test_t *test_arg)
{
   size_t i = 0;
   size_t unused = 0;

   RAM_FAIL_NOTNULL(test_arg);

   for (i = 0; i < test_arg->ramtestt_params.ramtestp_threadcount; ++i)
   {
      ramtest_start_t *start = NULL;
      const size_t threadid = i + 1;

      RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
            "[0] starting thread %zu...\n", threadid));
      /* i'm the sole producer of this memory; *ramtest_start()* is the sole
       * consumer. */
      start = (ramtest_start_t *)calloc(sizeof(*start), 1);
      RAM_FAIL_EXPECT(RAM_REPLY_RESOURCEFAIL, NULL != start);
      start->ramtests_test = test_arg;
      start->ramtests_threadidx = i;
      RAM_FAIL_TRAP(ramthread_mkthread(&test_arg->ramtestt_threads[i],
            &ramtest_thread, start));
      RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
            "[0] started thread %zu.\n", threadid));
   }

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_join(ramtest_test_t *test_arg)
{
   size_t i = 0;
   ram_reply_t myreply = RAM_REPLY_OK;
   size_t unused = 0;

   RAM_FAIL_NOTNULL(test_arg);

   RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
         "[0] i am waiting for my threads to finish...\n"));
   for (i = 0; i < test_arg->ramtestt_params.ramtestp_threadcount; ++i)
   {
      ram_reply_t e = RAM_REPLY_INSANE;

      RAM_FAIL_TRAP(ramthread_join(&e, test_arg->ramtestt_threads[i]));
      if (RAM_REPLY_OK != e)
      {
         RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr,
               "[0] thread %zu replied with an unexpected failure (%d).\n",
               i + 1, (int)e));
         /* if i haven't yet recorded an error as my reply, do so now. this
          * ensures that the primary symptom is recorded and not any echoes
          * of the problem. */
         RAM_FAIL_TRAP(ram_fail_accumulate(&myreply, e));
      }
   }

   return myreply;
}


ram_reply_t ramtest_next(size_t *next_arg, ramtest_test_t *test_arg)
{
   size_t i = 0;
   size_t j = 0;

   RAM_FAIL_NOTNULL(next_arg);
   RAM_FAIL_NOTNULL(test_arg);

   j = test_arg->ramtestt_params.ramtestp_alloccount;

   RAM_FAIL_TRAP(rammtx_wait(&test_arg->ramtestt_mtx));
   i = test_arg->ramtestt_nextrec;
   if (i < j)
      ++test_arg->ramtestt_nextrec;
   RAM_FAIL_PANIC(rammtx_quit(&test_arg->ramtestt_mtx));

   *next_arg = i;
   return RAM_REPLY_OK;
}

ram_reply_t ramtest_thread(void *arg)
{
   ramtest_start_t *start = (ramtest_start_t *)arg;
   ramtest_test_t *test = NULL;
   size_t threadidx = 0, threadid = 0;
   ram_reply_t e = RAM_REPLY_INSANE;
   size_t unused = 0;

   RAM_FAIL_NOTNULL(arg);

   test = start->ramtests_test;
   threadidx = start->ramtests_threadidx;
   /* i'm the sole consumer of this memory; *ramtest_start()* is the sole
    * producer. */
   free(start);
   threadid = threadidx + 1;
   RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr, "[%zu] testing...\n",
         threadid));
   e = ramtest_thread2(test, threadidx);
   RAM_FAIL_TRAP(ramtest_fprintf(&unused, stderr, "[%zu] finished.\n",
         threadid));

   return e;
}

ram_reply_t ramtest_thread2(ramtest_test_t *test_arg,
      size_t threadidx_arg)
{
   size_t i = 0;
   ram_reply_t e = RAM_REPLY_INSANE;
   int cachedflag = 0;
   ramtest_allocdesc_t cached = {0};

   RAM_FAIL_NOTNULL(test_arg);
   RAM_FAIL_EXPECT(RAM_REPLY_RANGEFAIL,
         threadidx_arg < test_arg->ramtestt_params.ramtestp_threadcount);

   while ((RAM_REPLY_OK == (e = ramtest_next(&i, test_arg)))
         && i < test_arg->ramtestt_params.ramtestp_alloccount)
   {
      ramtest_allocrec_t *info = NULL;
      ramtest_allocdesc_t condemned = {0};

      info = &test_arg->ramtestt_records[test_arg->ramtestt_sequence[i]];
      /* i don't want to allocate while i'm holding the allocation record
       * mutex, so i'll prepare an allocation ahead of time. */
      if (!cachedflag)
      {
         RAM_FAIL_TRAP(ramtest_alloc(&cached, test_arg,
               threadidx_arg));
      }
      /* there's actually a race condition between the call to
       * *ramtest_next()* and this point. the worst that could happen
       * (i think) is  that the first thread to draw a given record's index
       * might end up being the deallocating thread. */
      RAM_FAIL_TRAP(rammtx_wait(&info->ramtestar_mtx));
      /* if there's a pointer stored in *info->ramtestar_desc.ramtestad_ptr*
       * we'll assume we're the allocating thread. otherwise, we need to
       * deallocate. */
      if (NULL == info->ramtestar_desc.ramtestad_ptr)
      {
         info->ramtestar_desc = cached;
         /* i signal to the next loop iteration that i'll need a new
          * allocation. */
         cachedflag = 0;
      }
      else
         condemned = info->ramtestar_desc;
      RAM_FAIL_PANIC(rammtx_quit(&info->ramtestar_mtx));
      /* if i have a condemned pointer, i need to deallocate it. */
      if (condemned.ramtestad_ptr != NULL)
      {
         RAM_FAIL_TRAP(ramtest_dealloc(&condemned, test_arg,
               threadidx_arg));
         condemned.ramtestad_ptr = NULL;
      }

      RAM_FAIL_TRAP(test_arg->ramtestt_params.ramtestp_check(
            test_arg->ramtestt_params.ramtestp_extra, threadidx_arg));
   }

   RAM_FAIL_TRAP(test_arg->ramtestt_params.ramtestp_flush(
         test_arg->ramtestt_params.ramtestp_extra, threadidx_arg));
   RAM_FAIL_TRAP(test_arg->ramtestt_params.ramtestp_check(
         test_arg->ramtestt_params.ramtestp_extra, threadidx_arg));

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_alloc(ramtest_allocdesc_t *newptr_arg,
      ramtest_test_t *test_arg, size_t threadidx_arg)
{
   int32_t roll = 0;
   ramtest_allocdesc_t desc = {0};
   uint32_t n = 0, minsize = 0, maxsize = 0;

   RAM_FAIL_NOTNULL(newptr_arg);
   memset(newptr_arg, 0, sizeof(*newptr_arg));
   RAM_FAIL_NOTNULL(test_arg);
   RAM_FAIL_EXPECT(RAM_REPLY_RANGEFAIL,
         threadidx_arg < test_arg->ramtestt_params.ramtestp_threadcount);

   RAM_FAIL_TRAP(ram_cast_sztou32(&minsize, test_arg->ramtestt_params.ramtestp_minsize));
   RAM_FAIL_TRAP(ram_cast_sztou32(&maxsize, test_arg->ramtestt_params.ramtestp_maxsize));
   RAM_FAIL_TRAP(ramtest_randuint32(&n, minsize, maxsize + 1));
   desc.ramtestad_sz = n;
   /* i want a certain percentage of allocations to be performed by
    * an alternate allocator. */
   RAM_FAIL_TRAP(ramtest_randint32(&roll, 0, 100));
   /* splint reports a problem in the next line regarding the difference
    * in type between the two integers being compared. i don't understand
    * why it's necessary to consider int32_t and int separate types and i
    * can't find any information about 16-bit programming platforms, so
    * i'm going to suppress it. */
   if (/*@t1@*/roll < test_arg->ramtestt_params.ramtestp_mallocchance)
   {
      desc.ramtestad_pool = NULL;
      desc.ramtestad_ptr = malloc(desc.ramtestad_sz);
   }
   else
   {
      RAM_FAIL_TRAP(test_arg->ramtestt_params.ramtestp_acquire(&desc,
            desc.ramtestad_sz, test_arg->ramtestt_params.ramtestp_extra,
            threadidx_arg));
   }

   if (!test_arg->ramtestt_params.ramtestp_nofill)
      RAM_FAIL_TRAP(ramtest_fill(desc.ramtestad_ptr, desc.ramtestad_sz));

   *newptr_arg = desc;
   return RAM_REPLY_OK;
}

ram_reply_t ramtest_dealloc(ramtest_allocdesc_t *ptrdesc_arg,
      ramtest_test_t *test_arg, size_t threadidx_arg)
{
   void *pool = NULL;
   size_t sz = 0;
   ram_reply_t e = RAM_REPLY_INSANE;

   RAM_FAIL_NOTNULL(ptrdesc_arg);
   RAM_FAIL_NOTNULL(ptrdesc_arg->ramtestad_ptr);
   RAM_FAIL_NOTNULL(test_arg);
   RAM_FAIL_EXPECT(RAM_REPLY_RANGEFAIL,
         threadidx_arg < test_arg->ramtestt_params.ramtestp_threadcount);

   if (!test_arg->ramtestt_params.ramtestp_nofill)
   {
      RAM_FAIL_TRAP(ramtest_chkfill(ptrdesc_arg->ramtestad_ptr,
            ptrdesc_arg->ramtestad_sz));
   }
   e = test_arg->ramtestt_params.ramtestp_query(&pool, &sz,
         ptrdesc_arg->ramtestad_ptr,
         test_arg->ramtestt_params.ramtestp_extra);
   switch (e)
   {
   default:
      RAM_FAIL_TRAP(e);
      return RAM_REPLY_INSANE;
   case RAM_REPLY_OK:
      RAM_FAIL_EXPECT(RAM_REPLY_CORRUPT,
            ptrdesc_arg->ramtestad_pool == pool);
      /* the size won't always be identical due to the nature of mux pools.
       * the size will never be smaller, though. */
      RAM_FAIL_EXPECT(RAM_REPLY_CORRUPT, sz >= ptrdesc_arg->ramtestad_sz);
      RAM_FAIL_TRAP(
            test_arg->ramtestt_params.ramtestp_release(ptrdesc_arg));
      break;
   case RAM_REPLY_NOTFOUND:
      RAM_FAIL_EXPECT(RAM_REPLY_INSANE,
            NULL == ptrdesc_arg->ramtestad_pool);
      free(ptrdesc_arg->ramtestad_ptr);
      break;
   }

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_fill(char *ptr_arg, size_t sz_arg)
{
   RAM_FAIL_NOTNULL(ptr_arg);
   RAM_FAIL_NOTZERO(sz_arg);

   memset(ptr_arg, (int)(sz_arg & 0xff), sz_arg);

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_chkfill(char *ptr_arg, size_t sz_arg)
{
   char *p = NULL, *z = NULL;

   RAM_FAIL_NOTNULL(ptr_arg);
   RAM_FAIL_NOTZERO(sz_arg);

   for (p = ptr_arg, z = ptr_arg + sz_arg;
      p < z && ((char)(sz_arg & 0xff)) == *p; ++p)
      continue;

   if (p != z)
      return RAM_REPLY_CORRUPT;

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_defaultthreadcount(size_t *count_arg)
{
   size_t cpucount = 0;

   RAM_FAIL_NOTNULL(count_arg);
   *count_arg = 0;

   RAM_FAIL_TRAP(ramsys_cpucount(&cpucount));
   /* the default level of parallelism shall be 2.5 * the number of CPUs
    * detected on the system. tests that only support single-threaded
    * configurations will want to set this parameter in advance. */
   *count_arg = (size_t)(cpucount * 2.5);

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_maxthreadcount(size_t *count_arg)
{
   size_t cpucount = 0;

   RAM_FAIL_NOTNULL(count_arg);
   *count_arg = 0;

   RAM_FAIL_TRAP(ramsys_cpucount(&cpucount));
   /* if the thread count is greater than 5 times the number of CPU's,
    * i'm going to disallow it. */
   *count_arg = cpucount * 5;

   return RAM_REPLY_OK;
}

ram_reply_t ramtest_fprintf(size_t *count_arg, FILE *file_arg,
      const char *fmt_arg, ...)
{
   va_list moar;
   ram_reply_t reply = RAM_REPLY_INSANE;

   va_start(moar, fmt_arg);
   reply = ramtest_vfprintf(count_arg, file_arg, fmt_arg, moar);
   va_end(moar);

   return reply;
}

ram_reply_t ramtest_vfprintf(size_t *count_arg, FILE *file_arg,
      const char *fmt_arg, va_list moar_arg)
{
   int count = -1;

   RAM_FAIL_NOTNULL(count_arg);
   *count_arg = 0;
   RAM_FAIL_NOTNULL(file_arg);
   RAM_FAIL_NOTNULL(fmt_arg);

   count = trio_vfprintf(file_arg, fmt_arg, moar_arg);
   RAM_FAIL_EXPECT(RAM_REPLY_CRTFAIL, 0 <= count);

   RAM_FAIL_TRAP(ram_cast_inttosize(count_arg, count));
   return RAM_REPLY_OK;
}
