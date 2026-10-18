/****************************************************************************
 * arch/lisa/src/common/lisa_usestack.c
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
#include <nuttx/kmalloc.h>
#include <nuttx/arch.h>
#include <nuttx/tls.h>

#include "lisa_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_use_stack
 *
 * Description:
 *   Setup up stack-related information in the TCB using pre-allocated
 *   stack memory.
 *
 ****************************************************************************/

int up_use_stack(FAR struct tcb_s *tcb, FAR void *stack, size_t stack_size)
{
  uintptr_t top_of_stack;
  size_t size_of_stack;

  /* Is there already a stack allocated? */

  if (tcb->stack_alloc_ptr)
    {
      up_release_stack(tcb, tcb->flags & TCB_FLAG_TTYPE_MASK);
    }

  tcb->stack_alloc_ptr = stack;
  tcb->stack_base_ptr  = (FAR void *)STACKFRAME_ALIGN_UP((uintptr_t)stack);
  top_of_stack = STACKFRAME_ALIGN_DOWN((uintptr_t)stack + stack_size);
  size_of_stack = top_of_stack - (uintptr_t)tcb->stack_base_ptr;
  tcb->adj_stack_size = size_of_stack;

#ifdef CONFIG_STACK_COLORATION
  memset(tcb->stack_alloc_ptr, 0xaa, stack_size);
#endif

  return OK;
}
