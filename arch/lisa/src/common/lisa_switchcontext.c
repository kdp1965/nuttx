/****************************************************************************
 * arch/lisa/src/common/lisa_switchcontext.c
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
#include <sched.h>
#include <assert.h>
#include <nuttx/arch.h>
#include <nuttx/sched.h>

#include "sched/sched.h"
#include "lisa_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_switch_context
 *
 * Description:
 *   Switch from rtcb (the running task) to tcb.  Inside a handler nothing
 *   is done here: the running task's frame is already in its TCB
 *   (lisa_doirq) and the frame of this_task() is resumed when the handler
 *   returns.  Otherwise the running task's context is saved on its stack
 *   and the other task's resumed; the call returns when this task runs
 *   again.
 *
 ****************************************************************************/

void up_switch_context(FAR struct tcb_s *tcb, FAR struct tcb_s *rtcb)
{
  if (!up_interrupt_context())
    {
      /* Update scheduler parameters */

      nxsched_switch_context(rtcb, tcb);

      /* Record the new "running" task: the next interrupt stores its
       * frame through g_running_tasks[]
       */

      g_running_tasks[this_cpu()] = tcb;

      /* Then switch contexts */

      lisa_switchcontext(&rtcb->xcp.regs, tcb->xcp.regs);
    }
}
