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

#include <ramalloc/fail.h>
#include <stdio.h>
#include <stdlib.h>

static void ram_fail_defaultreporter(ram_reply_t code_arg,
      const char *expr_arg, const char *funcn_arg, const char *filen_arg,
      int lineno_arg);

static ram_fail_reporter_t ram_fail_thereporter = &ram_fail_defaultreporter;

void ram_fail_panic(const char *why_arg)
{
   if (!why_arg)
      why_arg = "*unspecified*";
   /* there's really no point in checking the return code of fprintf().
    * if it fails, i don't have a backup plan for informing the
    * operator. */
   fprintf(stderr, "*** epic fail! %s\n", why_arg);
   abort();
}

void ram_fail_setreporter(ram_fail_reporter_t reporter_arg)
{
   if (NULL == reporter_arg)
      ram_fail_thereporter = &ram_fail_defaultreporter;
   else
      ram_fail_thereporter = reporter_arg;
}

void ram_fail_report(ram_reply_t code_arg, const char *expr_arg,
      const char *funcn_arg, const char *filen_arg, int lineno_arg)
{
   ram_fail_thereporter(code_arg, expr_arg, funcn_arg, filen_arg,
         lineno_arg);
}

void ram_fail_defaultreporter(ram_reply_t reply_arg, const char *expr_arg,
   const char *funcn_arg, const char *filen_arg, int lineno_arg)
{
   int n = -1;

   /* to my knowledge, Windows doesn't support providing the function name,
    * so i need to tolerate a NULL value for funcn_arg. */
   if (NULL == funcn_arg)
   {

      n = fprintf(stderr, "FAIL %d at %s, line %d: %s\n", reply_arg,
            filen_arg, lineno_arg, expr_arg);
      if (n < 1)
         ram_fail_panic("fprintf() failed.");
   }
   else
   {
      n = fprintf(stderr, "FAIL %d in %s, at %s, line %d: %s\n", reply_arg,
            funcn_arg, filen_arg, lineno_arg, expr_arg);
      if (n < 1)
         ram_fail_panic("fprintf() failed.");
   }
}

ram_reply_t ram_fail_accumulate(ram_reply_t *reply_arg,
      ram_reply_t newreply_arg)
{
   RAM_FAIL_NOTNULL(reply_arg);

   if (RAM_REPLY_OK == *reply_arg)
      *reply_arg = newreply_arg;

   return RAM_REPLY_OK;
}
