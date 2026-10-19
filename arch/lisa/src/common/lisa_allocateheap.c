/****************************************************************************
 * arch/lisa/src/common/lisa_allocateheap.c
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
#include <nuttx/arch.h>
#include <nuttx/board.h>

#include "lisa_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_allocate_heap
 *
 * Description:
 *   The heap: from the end of the data areas (the area HEAP of the head)
 *   to the base of the idle thread's stack at the top of RAM.
 *
 ****************************************************************************/

#ifdef CONFIG_LISA_TASK_BANKS
/* The kernel heap: bank 0 above the kernel's data (every task sees it) */

void up_allocate_kheap(FAR void **heap_start, size_t *heap_size)
{
  uintptr_t base = lisa_heapbase();

  *heap_start = (FAR void *)base;
  *heap_size  = LISA_BANK_WINDOW - base;
}

/* The user heap umm_initialize makes is bank 1's (USR_HEAP is the running
 * task's bank heap, the idle thread's at this point); the other banks'
 * heaps are made when a task first needs them (lisa_banks.c)
 */

void up_allocate_heap(FAR void **heap_start, size_t *heap_size)
{
  *heap_start = (FAR void *)LISA_BANK_WINDOW;
  *heap_size  = LISA_BANK_HEAP_END(1) - LISA_BANK_WINDOW;
  board_autoled_on(LED_HEAPALLOCATE);
}
#else
void up_allocate_heap(FAR void **heap_start, size_t *heap_size)
{
  uintptr_t base = lisa_heapbase();

  *heap_start = (FAR void *)base;
  *heap_size  = LISA_IDLE_STACK_BASE - base;
  board_autoled_on(LED_HEAPALLOCATE);
}
#endif
