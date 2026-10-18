/****************************************************************************
 * arch/lisa/src/common/lisa_stackframe.c
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
#include <nuttx/arch.h>

#include "lisa_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_stack_frame
 *
 * Description:
 *   Allocate a frame of the given size at the base of the task's stack
 *   (the task has not started yet) and return its address.
 *
 ****************************************************************************/

FAR void *up_stack_frame(FAR struct tcb_s *tcb, size_t frame_size)
{
  FAR void *ret;

  frame_size = STACKFRAME_ALIGN_UP(frame_size);

  if (!tcb->stack_alloc_ptr || tcb->adj_stack_size <= frame_size)
    {
      return NULL;
    }

  ret = tcb->stack_base_ptr;
  memset(ret, 0, frame_size);

  tcb->stack_base_ptr  = (FAR uint8_t *)tcb->stack_base_ptr + frame_size;
  tcb->adj_stack_size -= frame_size;

  return ret;
}
