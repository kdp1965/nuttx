/****************************************************************************
 * arch/lisa/src/common/lisa_initialstate.c
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
#include <string.h>
#include <nuttx/arch.h>
#include <nuttx/sched.h>

#include "lisa_internal.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

__sfr __at(LISA_DATA_BANK)  g_data_bank;
__sfr __at(LISA_STACK_BANK) g_stack_bank;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_initial_state
 *
 * Description:
 *   A new thread is being started and a new TCB has been created.  Set up
 *   the context the first lisa_restore of the task resumes: a frame at the
 *   top of its stack with the entry point (a 3-byte function pointer: the
 *   window address and the code bank), interrupts enabled and the banks
 *   as they are.
 *
 ****************************************************************************/

void up_initial_state(FAR struct tcb_s *tcb)
{
  FAR struct xcptcontext *xcp = &tcb->xcp;
  FAR chipreg_t *regs;
  uintptr_t top;
  union
  {
    start_t start;
    uint8_t b[3];
  } entry;

  /* Initialize the initial exception register context structure */

  memset(xcp, 0, sizeof(struct xcptcontext));

  /* The idle thread is running on its stack already (lisa_head.asm) */

  if (tcb->pid == IDLE_PROCESS_ID)
    {
      tcb->stack_alloc_ptr = (FAR void *)LISA_IDLE_STACK_BASE;
      tcb->stack_base_ptr  = tcb->stack_alloc_ptr;
      tcb->adj_stack_size  = CONFIG_IDLETHREAD_STACKSIZE;
      return;
    }

  /* The frame at the top of the stack; the task starts with SP above it */

  top  = (uintptr_t)tcb->stack_base_ptr + tcb->adj_stack_size;
  regs = (FAR chipreg_t *)(top - XCPTCONTEXT_SIZE);
  memset(regs, 0, XCPTCONTEXT_SIZE);

  regs[XCPT_SP_LO] = (chipreg_t)top;
  regs[XCPT_SP_HI] = (chipreg_t)(top >> 8);

  /* The entry point: its address with the RA condition bit (bit 7 of the
   * high byte: ret reloads the predicate from it) and its code bank
   */

  entry.start      = tcb->start;
  regs[XCPT_PC_LO] = entry.b[0];
  regs[XCPT_PC_HI] = entry.b[1] | 0x80;
  regs[XCPT_CB]    = entry.b[2];
  regs[XCPT_DB]    = g_data_bank;
  regs[XCPT_SB]    = g_stack_bank;
  regs[XCPT_FLAGS] = XCPT_FLAGS_AMODE1;

#ifndef CONFIG_SUPPRESS_INTERRUPTS
  regs[XCPT_IE]    = 1;
#endif

  xcp->regs = regs;
}
