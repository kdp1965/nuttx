/****************************************************************************
 * arch/lisa/src/common/lisa_schedulesigaction.c
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
#include <string.h>
#include <sched.h>
#include <nuttx/debug.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/sched.h>

#include "sched/sched.h"
#include "lisa_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_schedule_sigaction
 *
 * Description:
 *   Arrange for the task to run lisa_sigdeliver when it next runs: its
 *   saved frame is duplicated just below itself, the copy vectored to the
 *   trampoline with interrupts disabled, the original kept in saved_regs
 *   for the trampoline to resume.  The task signalling itself from outside
 *   a handler is delivered directly by the scheduler and never gets here.
 *
 ****************************************************************************/

#ifdef CONFIG_LISA_TASK_BANKS
static int lisa_sigaction_op(FAR void *arg);

void up_schedule_sigaction(FAR struct tcb_s *tcb)
{
  /* The frame to duplicate is on the stack the task stopped on: in its
   * stack bank, a pthread's in its granule (the common scratch if inside
   * a visit: then the banks do not matter)
   */

  lisa_visit(tcb->xcp.fbank, tcb->xcp.gran, lisa_sigaction_op, tcb);
}

static int lisa_sigaction_op(FAR void *arg)
{
  FAR struct tcb_s *tcb = (FAR struct tcb_s *)arg;
#else
void up_schedule_sigaction(FAR struct tcb_s *tcb)
{
#endif
  FAR chipreg_t *regs;
  uintptr_t sp;
  union
  {
    void (*fn)(void);
    uint8_t b[3];
  } entry;

  sinfo("tcb=%p regs=%p\n", tcb, tcb->xcp.regs);

  /* Save the current register context location */

  tcb->xcp.saved_regs = tcb->xcp.regs;

  /* Duplicate the register context below it.  These will be restored by
   * the signal trampoline after the signal has been delivered.
   */

  regs = tcb->xcp.regs - XCPTCONTEXT_SIZE;
  memcpy(regs, tcb->xcp.saved_regs, XCPTCONTEXT_SIZE);

  sp = (uintptr_t)regs + XCPTCONTEXT_SIZE;
  regs[XCPT_SP_LO]  = (chipreg_t)sp;
  regs[XCPT_SP_MID] = (chipreg_t)(sp >> 8);
  regs[XCPT_SP_HI]  = (chipreg_t)(sp >> 16);

  /* Then set up to vector to the trampoline with interrupts disabled */

  entry.fn         = lisa_sigdeliver;
  regs[XCPT_PC_LO]  = entry.b[0];
  regs[XCPT_PC_MID] = entry.b[1];
  regs[XCPT_PC_HI]  = entry.b[2] | 0x80;
  regs[XCPT_IE]    = 0;

  tcb->xcp.regs    = regs;
#ifdef CONFIG_LISA_TASK_BANKS
  return OK;
#endif
}
