/****************************************************************************
 * arch/lisa/src/common/lisa_doirq.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <assert.h>
#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/board.h>
#include <arch/irq.h>
#include <sched/sched.h>

#include "lisa_internal.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

__sfr __at(LISA_INT_ENABLE) g_int_enable;
__sfr __at(LISA_INT_STATUS) g_int_status;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lisa_doirq
 *
 * Description:
 *   Called from the interrupt entry (lisa_head.asm) with the vector taken
 *   and the frame it pushed on the interrupted task's stack.  Dispatches
 *   the interrupt and returns the frame to resume: the same one, or the
 *   saved frame of the task the handlers made ready.
 *
 *   Vector 9 (LISA_IRQ_MULTIPLE) is taken when the pending, enabled
 *   sources are not exactly one: every pending source is served from it.
 *
 ****************************************************************************/

FAR chipreg_t *lisa_doirq(uint8_t irq, FAR chipreg_t *regs) __nonbanked
{
  FAR struct tcb_s **running_task = &g_running_tasks[this_cpu()];
  FAR struct tcb_s *tcb;

  /* The running task's context is the frame (valid until it runs again) */

  (*running_task)->xcp.regs = regs;
#ifdef CONFIG_LISA_TASK_BANKS
  (*running_task)->xcp.fbank = lisa_sb();     /* the frame is on the stack we were on */
#endif
  g_lisa_inirq = 1;

  board_autoled_on(LED_INIRQ);

#ifdef CONFIG_SUPPRESS_INTERRUPTS
  PANIC();
#else
  if (irq < LISA_IRQ_MULTIPLE)
    {
      irq_dispatch(irq, regs);
    }
  else
    {
      uint8_t pending = g_int_status & g_int_enable;
      uint8_t i;

      for (i = 0; i < LISA_IRQ_MULTIPLE && pending != 0; i++, pending >>= 1)
        {
          if ((pending & 1) != 0)
            {
              irq_dispatch(i, regs);
            }
        }
    }

  /* A context switch? */

  tcb = this_task();
  if (*running_task != tcb)
    {
      /* Update scheduler parameters */

      nxsched_switch_context(*running_task, tcb);

      /* Record the new "running" task when context switch occurred.
       * g_running_tasks[] is only used by assertion logic for reporting
       * crashes.
       */

      *running_task = tcb;
    }
#endif

  g_lisa_inirq = 0;
  board_autoled_off(LED_INIRQ);

  /* The frame to resume; the running task's is about to become invalid */

  regs = tcb->xcp.regs;
  tcb->xcp.regs = NULL;
#ifdef CONFIG_LISA_TASK_BANKS
  g_lisa_bank = tcb->xcp.fbank;                /* lisa_restore reads it under those banks */
  g_lisa_gran = tcb->xcp.gran;
#endif
  return regs;
}
