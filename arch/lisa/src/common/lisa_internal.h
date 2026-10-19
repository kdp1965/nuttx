/****************************************************************************
 * arch/lisa/src/common/lisa_internal.h
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __ARCH_LISA_SRC_COMMON_LISA_INTERNAL_H
#define __ARCH_LISA_SRC_COMMON_LISA_INTERNAL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>
#include <arch/irq.h>
#include <arch/chip/chip.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Determine which (if any) console driver to use */

#if !defined(CONFIG_DEV_CONSOLE)
#  undef  USE_SERIALDRIVER
#else
#  if defined(CONFIG_CONSOLE_SYSLOG)
#    undef  USE_SERIALDRIVER
#  else
#    define USE_SERIALDRIVER 1
#  endif
#endif

#if !defined(USE_SERIALDRIVER) && defined(CONFIG_STANDARD_SERIAL)
#  define USE_SERIALDRIVER 1
#endif

/* The RAM layout: the data areas from CONFIG_RAM_START, the heap after
 * them (lisa_heapbase), the idle thread's stack at the top.  The port keeps
 * the data bank and the stack bank at 1, the flat 32K every task shares:
 * the kernel reaches every stack through plain pointers (the TLS block at
 * the base of each, the arguments it copies in, the frames it reads), so a
 * stack bank of a task's own would need DB to follow SB.
 */

#define LISA_RAM_END          (CONFIG_RAM_START + CONFIG_RAM_SIZE)
#define LISA_IDLE_STACK_BASE  (LISA_RAM_END - CONFIG_IDLETHREAD_STACKSIZE)

#define LISA_DATA_BANK_FLAT   1
#define LISA_STACK_BANK_FLAT  1

#ifdef CONFIG_LISA_TASK_BANKS
/* Task banks (the data and stack bank registers, ttsky26d RESPIN 22).
 * Bank 0, the lower 16 KB, is the kernel's: its data, its heap (every
 * TCB, group, driver buffer), reachable from everywhere.  Each task lives
 * in one of the 15 window banks with its stack and the heap its malloc()
 * draws from, SB and DB both set to that bank while it runs - a pointer
 * to a stack local works only so.  Bank 1 also holds the idle thread's
 * stack at its top.  Below the top of every bank a scratch stack takes
 * the kernel's visits: lisa_bank_call runs a function in another bank
 * on that stack with SB = DB = the bank, so that C code there sees its
 * own locals (lisa_banks.c); an interrupt during a visit nests on the
 * same scratch, and a visit made from an interrupt takes the lower part.
 */

#  define LISA_BANK_WINDOW      0x4000u
#  define LISA_BANK_SIZE        0x4000u
#  define LISA_NBANKS           16
#  define LISA_BANK_SCRATCH     768u     /* 512 for the kernel's visits, 256 under it for an interrupt's */
#  define LISA_BANK_IRQ_SCRATCH 512u
#  define LISA_BANK_TOP(n)      ((n) == 1 ? LISA_IDLE_STACK_BASE : (LISA_BANK_WINDOW + LISA_BANK_SIZE))
#  define LISA_BANK_HEAP_END(n) (LISA_BANK_TOP(n) - LISA_BANK_SCRATCH)
#endif

/* The peripheral registers are reached through the direct addressing of
 * the data space (lda/sta [p:abs9]); these read and write them in C.
 */

#define getreg8(a)            (a)
#define putreg8(v,a)          ((a) = (v))

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifndef __ASSEMBLY__

/* The interrupt enable bit cannot be read back: the port shadows it.  1
 * while interrupts are enabled; 0 inside up_irq_save() ... up_irq_restore()
 * and inside a handler.  Near: the assembly reads it with lda.
 */

extern __data volatile uint8_t g_lisa_ie;

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif

/* lisa_context.asm: the context switch.  The frames live on the tasks'
 * stacks (arch/lisa/include/irq.h XCPT_*).
 */

void lisa_switchcontext(FAR chipreg_t **saveregs,
                        FAR chipreg_t *restoreregs) __nonbanked;

struct lisa_bankop_s
{
  CODE int (*fn)(FAR void *);
  FAR void *arg;
};

/* lisa_context.asm's part of the task banks, present whatever the
 * configuration (lisa_banks.c)
 */

extern __data volatile uint8_t g_lisa_bank;  /* the bank of the frame lisa_restore resumes */
extern __data uint8_t g_bank_new;            /* lisa_bank_call's temporaries */
extern __data uint8_t g_bank_ie;
extern __data uint8_t g_bank_sb;
extern __data uint8_t g_bank_db;
extern __data uint16_t g_bank_sp;
extern __data uint16_t g_bank_top;           /* the scratch stack's top for the visit */
extern struct lisa_bankop_s g_bank_op;       /* the visit */
extern int g_bank_ret;                       /* what it returned */

void lisa_bank_call(uint8_t bank) __nonbanked;     /* lisa_context.asm: g_bank_op in the bank, on its scratch */
void lisa_bank_op(void) __nonbanked;               /* its callee */

#ifdef CONFIG_LISA_TASK_BANKS
int lisa_bank_visit(uint8_t bank, CODE int (*fn)(FAR void *), FAR void *arg);
FAR void *lisa_bank_malloc(uint8_t bank, size_t size);
void lisa_bank_free(uint8_t bank, FAR void *mem);
uint8_t lisa_bank_pick(size_t size, FAR void **mem);
void lisa_bank_task_add(uint8_t bank);
void lisa_bank_task_del(uint8_t bank);
void lisa_bank_initialize(void);
uint8_t lisa_sb(void) __nonbanked;
#endif
void lisa_fullcontextrestore(FAR chipreg_t *regs) __nonbanked;
uintptr_t lisa_heapbase(void) __naked __nonbanked;

/* lisa_doirq.c: called from the interrupt entry in lisa_head.asm with the
 * vector and the frame; returns the frame to resume
 */

FAR chipreg_t *lisa_doirq(uint8_t irq, FAR chipreg_t *regs) __nonbanked;

/* lisa_sigdeliver.c: the signal trampoline a task is vectored to */

void lisa_sigdeliver(void);

/* The board's serial driver */

#ifdef USE_SERIALDRIVER
void lisa_serial_initialize(void);
#else
#  define lisa_serial_initialize()
#endif

#ifdef __cplusplus
}
#endif
#endif /* __ASSEMBLY__ */

#endif /* __ARCH_LISA_SRC_COMMON_LISA_INTERNAL_H */
