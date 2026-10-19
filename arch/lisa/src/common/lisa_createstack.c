/****************************************************************************
 * arch/lisa/src/common/lisa_createstack.c
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
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <sched.h>
#include <assert.h>
#include <nuttx/debug.h>
#include <nuttx/kmalloc.h>
#include <nuttx/arch.h>
#include <nuttx/tls.h>
#include <nuttx/board.h>

#include <sched/sched.h>

#include "lisa_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_create_stack
 *
 * Description:
 *   Allocate a stack for a new thread and setup up stack-related
 *   information in the TCB.  The stack grows down from the end of the
 *   allocation.
 *
 ****************************************************************************/

int up_create_stack(FAR struct tcb_s *tcb, size_t stack_size, uint8_t ttype)
{
  /* Is there already a stack allocated of a different size? */

  if (tcb->stack_alloc_ptr && tcb->adj_stack_size != stack_size)
    {
      up_release_stack(tcb, ttype);
    }

  /* Do we need to allocate a new stack? */

  if (!tcb->stack_alloc_ptr)
    {
#ifdef CONFIG_LISA_TASK_BANKS
      /* A pthread shares its group's bank (they share pointers), a kernel
       * thread lives in bank 1 with the idle thread, a task takes the
       * first bank with room for its stack, a new one when none has
       */

      uint8_t bank;

      if (ttype == TCB_FLAG_TTYPE_PTHREAD)
        {
          bank = this_task()->xcp.bank;
          tcb->stack_alloc_ptr = lisa_bank_malloc(bank, stack_size);
        }
      else if (ttype == TCB_FLAG_TTYPE_KERNEL)
        {
          bank = 1;
          tcb->stack_alloc_ptr = lisa_bank_malloc(bank, stack_size);
        }
      else
        {
          bank = lisa_bank_pick(stack_size, &tcb->stack_alloc_ptr);
        }

      tcb->xcp.bank  = bank;
      tcb->xcp.fbank = bank;
      if (tcb->stack_alloc_ptr)
        {
          lisa_bank_task_add(bank);
        }
#elif defined(CONFIG_MM_KERNEL_HEAP)
      if (ttype == TCB_FLAG_TTYPE_KERNEL)
        {
          tcb->stack_alloc_ptr = kmm_malloc(stack_size);
        }
      else
#endif
#ifndef CONFIG_LISA_TASK_BANKS
        {
          tcb->stack_alloc_ptr = kumm_malloc(stack_size);
        }
#endif

#ifdef CONFIG_DEBUG_FEATURES
      if (!tcb->stack_alloc_ptr)
        {
          serr("ERROR: Failed to allocate stack, size %d\n", stack_size);
        }
#endif
    }

  if (tcb->stack_alloc_ptr)
    {
#ifdef CONFIG_STACK_COLORATION
      memset(tcb->stack_alloc_ptr, 0xaa, stack_size);
#endif
      tcb->stack_base_ptr = tcb->stack_alloc_ptr;
      tcb->adj_stack_size = stack_size & ~(STACKFRAME_ALIGN - 1);
      tcb->flags |= TCB_FLAG_FREE_STACK;

      board_autoled_on(LED_STACKCREATED);
      return OK;
    }

  return ERROR;
}
