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
 * them (lisa_heapbase), the idle thread's stack at the top.  Without the
 * task banks the port keeps the data bank and the stack bank at 1, the
 * flat 32K every task shares.
 */

#define LISA_RAM_END          (CONFIG_RAM_START + CONFIG_RAM_SIZE)
#define LISA_IDLE_STACK_BASE  (LISA_RAM_END - CONFIG_IDLETHREAD_STACKSIZE)

#define LISA_DATA_BANK_FLAT   1
#define LISA_STACK_BANK_FLAT  1

#ifdef CONFIG_LISA_TASK_BANKS
/* Task banks (ttsky26d RESPIN 22, the data map of RESPIN 28).  The data
 * space has three regions, by address whatever the form of the access:
 * below 0x2000 the common 8K, the kernel's (its data, its heap: every
 * TCB, group, driver buffer; reachable from everywhere); 0x2000-0x3FFF
 * the data window on one of 32 granules of 8K (DB, 5 bits); 0x4000 up
 * the stack window on one of 16 banks of 16K (SB; bank n is granules 2n
 * and 2n+1).  Each task has a granule of its own in the data window with
 * the heap its malloc() draws from, and a stack in a stack bank: a small
 * stack (under CONFIG_LISA_TASK_OWN_BANK_STACK) shares a bank with other
 * tasks' stacks, a big one gets a bank of its own at its top, and then the
 * task's heap runs from 0x2000 up to the stack, 24 KB less the stack.  A
 * pthread's stack is malloc()ed from its group's heap (the banks go by
 * address, so a stack in the data window works); the kernel threads and
 * the idle thread (its stack at the top of bank 1) share bank 1 and
 * granule 1.  The kernel reaches a task's stack or heap in another bank
 * through a visit: lisa_bank_call runs a function on a scratch stack in
 * the common 8K with SB and DB set to the task's (lisa_banks.c); a visit
 * from an interrupt takes a second scratch, since it may cut into one.
 */

#  define LISA_COMMON_END       0x2000u                      /* the common 8K */
#  define LISA_DATA_WINDOW      0x2000u                      /* the data window: a granule */
#  define LISA_GRANULE_SIZE     0x2000u
#  define LISA_NGRANULES        32
#  define LISA_BANK_WINDOW      0x4000u                      /* the stack window: a bank */
#  define LISA_BANK_SIZE        0x4000u
#  define LISA_BANK_END         0x8000u
#  define LISA_NBANKS           16
#  define LISA_BANK_TOP(n)      ((n) == 1 ? LISA_IDLE_STACK_BASE : LISA_BANK_END)
#  define LISA_VISIT_STACK      512u                         /* the kernel's visits */
#  define LISA_IRQ_VISIT_STACK  256u                         /* a visit made from an interrupt */
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

extern __data volatile uint8_t g_lisa_bank;  /* SB for the frame lisa_restore resumes (the stack it is on) */
extern __data volatile uint8_t g_lisa_gran;  /* DB for it (a pthread's stack is in its group's granule) */
extern __data uint8_t g_bank_new;            /* lisa_bank_call's temporaries: SB and DB to visit under */
extern __data uint8_t g_bank_newdb;
extern __data uint8_t g_bank_ie;
extern __data uint8_t g_bank_sb;
extern __data uint8_t g_bank_db;
extern __data uint16_t g_bank_sp;
extern __data uint16_t g_bank_top;           /* the scratch stack's top for the visit */
extern struct lisa_bankop_s g_bank_op;       /* the visit */
extern int g_bank_ret;                       /* what it returned */

void lisa_bank_call(uint8_t bank) __nonbanked;     /* lisa_context.asm: g_bank_op under SB = bank, DB = g_bank_newdb, on the scratch g_bank_top */
void lisa_bank_op(void) __nonbanked;               /* its callee */

#ifdef CONFIG_LISA_TASK_BANKS
int lisa_visit(uint8_t sb, uint8_t db, CODE int (*fn)(FAR void *),
               FAR void *arg);
FAR void *lisa_bank_malloc(uint8_t bank, size_t size);   /* a stack in a shared bank */
void lisa_bank_free(uint8_t bank, FAR void *mem);
void lisa_bank_task_add(uint8_t bank);
void lisa_bank_task_del(uint8_t bank);
int lisa_task_place(FAR struct tcb_s *tcb, size_t stack_size);   /* a task's bank, granule, stack and heap */
void lisa_task_unplace(FAR struct tcb_s *tcb);
void lisa_bank_initialize(void);
uint8_t lisa_sb(void) __nonbanked;
uint8_t lisa_db(void) __nonbanked;
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
