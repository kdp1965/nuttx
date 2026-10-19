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
#include <assert.h>
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
/* The kernel heap: the common 8K above the kernel's data (every task sees
 * it)
 */

void up_allocate_kheap(FAR void **heap_start, size_t *heap_size)
{
  uintptr_t base = lisa_heapbase();

  DEBUGASSERT(base < LISA_COMMON_END);
  *heap_start = (FAR void *)base;
  *heap_size  = LISA_COMMON_END - base;
}

/* The user heap umm_initialize makes is granule 1's, the data window as
 * the reset banks map it: the kernel threads' and the idle thread's; a
 * task's own is made in its granule when it is placed (lisa_banks.c)
 */

void up_allocate_heap(FAR void **heap_start, size_t *heap_size)
{
  *heap_start = (FAR void *)LISA_DATA_WINDOW;
  *heap_size  = LISA_GRANULE_SIZE;
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
