/****************************************************************************
 * libs/libc/stream/lib_ultoa_invert.c
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * SPDX-FileCopyrightText: 2017, Keith Packard. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include "lib_ultoa_invert.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The digits of v in base, least significant first, into str.  A power of
 * two base (shift != 0) takes a mask and a shift per digit; the others one
 * division and a multiply.  The macro keeps the arithmetic at the width of
 * type: on a small CPU a 64-bit division is thousands of instructions (an
 * 8-bit machine's is a bit-serial loop in its library), and the values
 * printf meets nearly always fit 16 or 32 bits.
 */

#define DIGITS(type, v, str, base, shift, upper)                            \
  do                                                                        \
    {                                                                       \
      int d;                                                                \
                                                                            \
      if (shift)                                                            \
        {                                                                   \
          d   = (int)((v) & ((base) - 1));                                  \
          (v) >>= (shift);                                                  \
        }                                                                   \
      else                                                                  \
        {                                                                   \
          type q = (v) / (type)(base);                                      \
                                                                            \
          d   = (int)((v) - q * (type)(base));                              \
          (v) = q;                                                          \
        }                                                                   \
                                                                            \
      if (d <= 9)                                                           \
        {                                                                   \
          d += '0';                                                         \
        }                                                                   \
      else if (upper)                                                       \
        {                                                                   \
          d += 'A' - 10;                                                    \
        }                                                                   \
      else                                                                  \
        {                                                                   \
          d += 'a' - 10;                                                    \
        }                                                                   \
                                                                            \
      *(str)++ = d;                                                         \
    }                                                                       \
  while (v)

/****************************************************************************
 * Public Functions
 ****************************************************************************/

FAR char *__ultoa_invert(unsigned long long val, FAR char *str, int base)
{
  int upper = 0;
  int shift = 0;

  if (base & XTOA_UPPER)
    {
      upper = 1;
      base &= ~XTOA_UPPER;
    }

  if (base == 16)
    {
      shift = 4;
    }
  else if (base == 8)
    {
      shift = 3;
    }
  else if (base == 2)
    {
      shift = 1;
    }

  /* Convert at the narrowest width that holds the value */

  if (val <= 0xffffu)
    {
      unsigned int u = (unsigned int)val;

      DIGITS(unsigned int, u, str, base, shift, upper);
    }
  else if (val <= 0xfffffffful)
    {
      unsigned long u = (unsigned long)val;

      DIGITS(unsigned long, u, str, base, shift, upper);
    }
  else
    {
      DIGITS(unsigned long long, val, str, base, shift, upper);
    }

  return str;
}
