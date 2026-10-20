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
#include <nuttx/kmalloc.h>

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
  uintptr_t base = (uintptr_t)lisa_heapbase();

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

/* The data space is flat: one kernel heap from the end of the data areas
 * to the idle thread's stack, in regions no larger than size_t counts
 * (16 bits: a region starts as one free node, and mm's node sizes are
 * size_t).  The first region is what up_allocate_heap() hands nx_start;
 * the rest are added by lisa_addregions() from up_initialize(), once the
 * heap exists.  CONFIG_MM_REGIONS has to cover them (8 MB / 64 KB = 129).
 */

#define LISA_REGION_MAX  0xfff0u
#define LISA_REGION_MIN  256u          /* a tail smaller than this is left out */

static uintptr_t g_lisa_heapnext;      /* the first byte not yet in a region */

static size_t lisa_region_size(uintptr_t start)
{
  uintptr_t size = LISA_IDLE_STACK_BASE - start;

  return size > LISA_REGION_MAX ? LISA_REGION_MAX : (size_t)size;
}

void up_allocate_heap(FAR void **heap_start, size_t *heap_size)
{
  uintptr_t base = (uintptr_t)lisa_heapbase();
  size_t size = lisa_region_size(base);

  *heap_start = (FAR void *)base;
  *heap_size  = size;
  g_lisa_heapnext = base + size;
  board_autoled_on(LED_HEAPALLOCATE);
}

#if CONFIG_MM_REGIONS > 1
void lisa_addregions(void)
{
  int n;

  for (n = 1; n < CONFIG_MM_REGIONS; n++)
    {
      size_t size = lisa_region_size(g_lisa_heapnext);

      if (size < LISA_REGION_MIN)
        {
          break;
        }

      kmm_addregion((FAR void *)g_lisa_heapnext, size);
      g_lisa_heapnext += size;
    }
}
#endif
#endif

