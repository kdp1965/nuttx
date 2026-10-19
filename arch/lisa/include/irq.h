/****************************************************************************
 * arch/lisa/include/irq.h
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

/* This file should never be included directly but, rather, only indirectly
 * through nuttx/irq.h
 */

#ifndef __ARCH_LISA_INCLUDE_IRQ_H
#define __ARCH_LISA_INCLUDE_IRQ_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/irq.h>
#ifndef __ASSEMBLY__
#  include <stdint.h>
#  include <stdbool.h>
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The interrupt sources: vector word 1 + n for the bit n of INT_STATUS
 * ([p:0x0f]), the lowest bit the highest priority.  Vector word 9 is taken
 * when the pending, enabled sources are not exactly one: lisa_doirq serves
 * them all from there.
 */

#define LISA_IRQ_TIMER1     0
#define LISA_IRQ_TIMER2     1
#define LISA_IRQ_UART1      2
#define LISA_IRQ_UART2      3
#define LISA_IRQ_GPIOA      4
#define LISA_IRQ_TTLC       5
#define LISA_IRQ_I2C        6
#define LISA_IRQ_7          7
#define LISA_IRQ_MULTIPLE   8

#define NR_IRQS             9

/* The register save area, a byte array (chipreg_t is a byte): the frame
 * that lisa_head.asm pushes on the interrupted task's stack, that
 * lisa_switchcontext builds for a voluntary switch and that
 * up_initial_state lays out at the top of a new stack.  The task resumes
 * with SP just above the frame.  The 16-bit values keep the order the
 * stack instructions give them: sra / push ix put the high byte, with its
 * condition bit in bit 7, at the lower address.
 */

#define XCPT_SP_LO          0   /* SP at the interrupt (informational) */
#define XCPT_SP_HI          1
#define XCPT_IE             2   /* 1: interrupts were enabled */
#define XCPT_SB             3   /* the stack bank [p:0x17] */
#define XCPT_DB             4   /* the data bank [p:0x16] */
#define XCPT_CB             5   /* the code bank [p:0x15] */
#define XCPT_PC_HI          6   /* the resume address: IA after an interrupt, RA after a call; bit 7 = the RA condition bit (1) */
#define XCPT_PC_LO          7
#define XCPT_FLAGS          8   /* the ISR flag shadows: {signed_inv, Z, C} (ldirq / stirq) */
#define XCPT_CSAVE          9   /* the cflag_save shadow in bit 0 (savec / restc) */
#define XCPT_IRQ            10  /* the vector taken (scratch) */
#define XCPT_RA_HI          11  /* RA with its condition bit */
#define XCPT_RA_LO          12
#define XCPT_IX_HI          13  /* IX with its condition bit */
#define XCPT_IX_LO          14
#define XCPT_A              15

#define XCPTCONTEXT_REGS    16
#define XCPTCONTEXT_SIZE    16

/* The flags byte of a frame is what ldirq reads and stirq writes back:
 * {amode[2:0], signed_inv_live, cflag_save, signed_inv, Z, C}.  A frame not
 * made by an interrupt (a new task, a voluntary switch) carries amode 1,
 * the mode the compiler's code runs in; rets takes amode from it.
 */

#define XCPT_FLAGS_AMODE1   0x20

#define STACKFRAME_ALIGN    2

/****************************************************************************
 * Public Types
 ****************************************************************************/

#ifndef __ASSEMBLY__

typedef unsigned char chipreg_t;

/* This struct defines the way the registers are stored. */

struct xcptcontext
{
  /* The register save area of a task that is not running: on its stack,
   * see XCPT_* above.  Valid only while the task is not running.
   */

  FAR chipreg_t *regs;

  /* The context a signal trampoline returns to (up_schedule_sigaction
   * duplicates the frame below it)
   */

  FAR chipreg_t *saved_regs;

#ifdef CONFIG_LISA_TASK_BANKS
  /* The task's stack bank (SB while it runs) and the granule of the data
   * window its heap is in (DB) - lisa_internal.h, "Task banks"; the bank
   * its saved frame lies in (the stack it stopped on: a pthread's is in
   * the granule, a visit's in the common 8K, where SB does not matter);
   * an argument for the visits (up_task_call) that have no room for one
   */

  uint8_t bank;
  uint8_t gran;
  uint8_t fbank;
  uint16_t varg;
#endif
};
#endif

/****************************************************************************
 * Inline functions
 ****************************************************************************/

#ifndef __ASSEMBLY__

/* The resume address and SP of a saved context */

#define up_getusrpc(regs) \
  ((uintptr_t)(((FAR chipreg_t *)(regs))[XCPT_PC_LO]) | \
   ((uintptr_t)(((FAR chipreg_t *)(regs))[XCPT_PC_HI] & 0x7f) << 8))

#define up_getusrsp(regs) \
  ((uintptr_t)(((FAR chipreg_t *)(regs))[XCPT_SP_LO]) | \
   ((uintptr_t)(((FAR chipreg_t *)(regs))[XCPT_SP_HI]) << 8))

/* 1 while lisa_doirq runs a handler (arch/lisa/src/common/lisa_irq.c);
 * up_interrupt_context() must be visible wherever arch/irq.h is (errno.h's
 * set_errno uses it): a macro, as the other architectures do.
 */

extern __data volatile uint8_t g_lisa_inirq;

#define up_interrupt_context() (g_lisa_inirq != 0)

#endif

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#ifndef __ASSEMBLY__
#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/* The interrupt enable bit cannot be read back, so the port keeps it in a
 * byte (g_lisa_ie, arch/lisa/src/common/lisa_irq.c): up_irq_save returns
 * it; these are plain calls (__nonbanked) in the common area.
 */

irqstate_t up_irq_save(void) __naked __nonbanked;
void       up_irq_restore(irqstate_t flags) __naked __nonbanked;
irqstate_t up_irq_enable(void) __naked __nonbanked;

/* The stack pointer (IX) */

uintptr_t  up_getsp(void) __naked __nonbanked;

#undef EXTERN
#ifdef __cplusplus
}
#endif
#endif

#endif /* __ARCH_LISA_INCLUDE_IRQ_H */
