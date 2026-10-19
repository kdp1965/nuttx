/****************************************************************************
 * arch/lisa/src/common/lisa_banks.c
 *
 * Task banks: each task's stack and heap in one of the window banks of
 * the second LISA chip (lisa_internal.h, "Task banks").
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
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/sched.h>
#include <nuttx/mutex.h>
#include <nuttx/mm/mm.h>
#include <nuttx/fs/procfs.h>
#include <arch/irq.h>
#include <sched/sched.h>

#include "lisa_internal.h"

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* lisa_context.asm refers to these whatever the configuration: without the
 * task banks every frame is in bank 1 and the visit is never made
 */

__data volatile uint8_t g_lisa_bank = 1;
__data uint8_t g_bank_new;
__data uint8_t g_bank_ie;
__data uint8_t g_bank_sb;
__data uint8_t g_bank_db;
__data uint16_t g_bank_sp;
__data uint16_t g_bank_top;
struct lisa_bankop_s g_bank_op;
int g_bank_ret;

/* lisa_bank_call's callee: on the bank's scratch stack */

void lisa_bank_op(void) __nonbanked
{
  g_bank_ret = g_bank_op.fn(g_bank_op.arg);
}

#ifdef CONFIG_LISA_TASK_BANKS

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct lisa_bank_s
{
  FAR struct mm_heap_s *heap;       /* its heap (the structure inside the bank); NULL: closed */
  uint8_t ntasks;                   /* the tasks living in it */
  struct procfs_meminfo_entry_s entry;
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

__sfr __at(LISA_DATA_BANK)  g_db_reg;
__sfr __at(LISA_STACK_BANK) g_sb_reg;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct lisa_bank_s g_bank[LISA_NBANKS];

/* One visit at a time (the scratch stacks, the temporaries); recursive:
 * a visit may allocate in the bank it is in
 */

static rmutex_t g_bank_lock = NXRMUTEX_INITIALIZER;

static const char g_bank_names[LISA_NBANKS][7] =
{
  "bank0",  "bank1",  "bank2",  "bank3",  "bank4",  "bank5",  "bank6",
  "bank7",  "bank8",  "bank9",  "bank10", "bank11", "bank12", "bank13",
  "bank14", "bank15"
};

static struct procfs_meminfo_entry_s g_banks_entry;

/* The arguments and results of the visits: in the common bank, under
 * g_bank_lock
 */

static struct
{
  uint8_t bank;
  size_t size;
  FAR void *mem;
} g_bank_alloc;

static struct mallinfo g_bank_info;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* The visits (run on the bank's scratch stack with SB = DB = the bank) */

static int lisa_bank_open_op(FAR void *arg)
{
  uint8_t n = (uint8_t)(uintptr_t)arg;
  struct mm_heap_config_s config;

  memset(&config, 0, sizeof(config));
  config.name     = g_bank_names[n];
  config.start    = (FAR void *)LISA_BANK_WINDOW;
  config.size     = LISA_BANK_HEAP_END(n) - LISA_BANK_WINDOW;
  config.noprocfs = true;
  g_bank[n].heap  = mm_initialize_heap(&config);
  return g_bank[n].heap ? OK : -ENOMEM;
}

static int lisa_bank_malloc_op(FAR void *arg)
{
  g_bank_alloc.mem = mm_malloc(g_bank[g_bank_alloc.bank].heap,
                               g_bank_alloc.size);
  return OK;
}

static int lisa_bank_free_op(FAR void *arg)
{
  mm_free(g_bank[g_bank_alloc.bank].heap, g_bank_alloc.mem);
  return OK;
}

static int lisa_bank_mallinfo_op(FAR void *arg)
{
  uint8_t n = (uint8_t)(uintptr_t)arg;

  g_bank_info = mm_mallinfo(g_bank[n].heap);
  return OK;
}

/* /proc/meminfo: one bank (the entry's heap field carries the number) */

static struct mallinfo lisa_bank_mallinfo(FAR struct mm_heap_s *key)
{
  struct mallinfo info;
  uint8_t n = (uint8_t)(uintptr_t)key;

  nxrmutex_lock(&g_bank_lock);
  if (g_bank[n].heap != NULL)
    {
      lisa_bank_visit(n, lisa_bank_mallinfo_op, (FAR void *)(uintptr_t)n);
      info = g_bank_info;
    }
  else
    {
      memset(&info, 0, sizeof(info));
    }

  nxrmutex_unlock(&g_bank_lock);
  return info;
}

/* /proc/meminfo: all 15 window banks, a closed one wholly free */

static struct mallinfo lisa_banks_mallinfo(FAR struct mm_heap_s *key)
{
  struct mallinfo total;
  uint8_t n;
  long size;

  memset(&total, 0, sizeof(total));
  nxrmutex_lock(&g_bank_lock);
  for (n = 1; n < LISA_NBANKS; n++)
    {
      size = (long)(LISA_BANK_HEAP_END(n) - LISA_BANK_WINDOW);
      total.arena += size;
      if (g_bank[n].heap != NULL)
        {
          lisa_bank_visit(n, lisa_bank_mallinfo_op,
                          (FAR void *)(uintptr_t)n);
          total.ordblks  += g_bank_info.ordblks;
          total.aordblks += g_bank_info.aordblks;
          total.uordblks += g_bank_info.uordblks;
          total.fordblks += g_bank_info.fordblks;
          if (g_bank_info.mxordblk > total.mxordblk)
            {
              total.mxordblk = g_bank_info.mxordblk;
            }
        }
      else
        {
          total.ordblks++;
          total.fordblks += size;
          if (size > total.mxordblk)
            {
              total.mxordblk = size;
            }
        }
    }

  nxrmutex_unlock(&g_bank_lock);
  return total;
}

static void lisa_bank_memdump(FAR struct mm_heap_s *key,
                              FAR const struct mm_memdump_s *dump)
{
  /* Not from another bank */
}

static void lisa_bank_register(uint8_t n)
{
  FAR struct procfs_meminfo_entry_s *entry = &g_bank[n].entry;

  entry->name     = g_bank_names[n];
  entry->heap     = (FAR struct mm_heap_s *)(uintptr_t)n;
  entry->mallinfo = lisa_bank_mallinfo;
  entry->memdump  = lisa_bank_memdump;
  procfs_register_meminfo(entry);
}

/* Make a bank's heap (the whole bank under its scratch stack) */

static int lisa_bank_open(uint8_t n)
{
  int ret = OK;

  nxrmutex_lock(&g_bank_lock);
  if (g_bank[n].heap == NULL)
    {
      ret = lisa_bank_visit(n, lisa_bank_open_op, (FAR void *)(uintptr_t)n);
      if (ret == OK)
        {
          lisa_bank_register(n);
        }
    }

  nxrmutex_unlock(&g_bank_lock);
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

uint8_t lisa_sb(void) __nonbanked
{
  return g_sb_reg;
}

/* Run fn(arg) with the bank visible: directly when it is the bank we are
 * in (the running task's own, or inside a visit), else on its scratch
 * stack through lisa_bank_call.  arg must point into the common bank.
 */

int lisa_bank_visit(uint8_t bank, CODE int (*fn)(FAR void *),
                    FAR void *arg)
{
  int ret;

  if (bank == g_db_reg && bank == g_sb_reg)
    {
      return fn(arg);
    }

  if (up_interrupt_context())
    {
      /* No lock, and the lower part of the scratch: above it may sit the
       * visit this interrupt cut into
       */

      g_bank_op.fn  = fn;
      g_bank_op.arg = arg;
      g_bank_top    = LISA_BANK_TOP(bank) - LISA_BANK_IRQ_SCRATCH - 1;
      lisa_bank_call(bank);
      return g_bank_ret;
    }

  nxrmutex_lock(&g_bank_lock);
  g_bank_op.fn  = fn;
  g_bank_op.arg = arg;
  g_bank_top    = LISA_BANK_TOP(bank) - 1;
  lisa_bank_call(bank);
  ret = g_bank_ret;
  nxrmutex_unlock(&g_bank_lock);
  return ret;
}

int up_task_call(FAR struct tcb_s *tcb, CODE int (*fn)(FAR void *),
                 FAR void *arg)
{
  return lisa_bank_visit(tcb->xcp.bank, fn, arg);
}

/* The running task's heap: USR_HEAP */

FAR struct mm_heap_s **up_task_heap_ref(void)
{
  FAR struct tcb_s *tcb = this_task();
  uint8_t n = tcb ? tcb->xcp.bank : 1;

  if (n == 0)
    {
      n = 1;
    }

  return &g_bank[n].heap;
}

FAR void *lisa_bank_malloc(uint8_t bank, size_t size)
{
  FAR void *mem;

  if (g_bank[bank].heap == NULL)
    {
      return NULL;
    }

  if (bank == g_db_reg && bank == g_sb_reg)
    {
      return mm_malloc(g_bank[bank].heap, size);
    }

  nxrmutex_lock(&g_bank_lock);
  g_bank_alloc.bank = bank;
  g_bank_alloc.size = size;
  g_bank_alloc.mem  = NULL;
  lisa_bank_visit(bank, lisa_bank_malloc_op, NULL);
  mem = g_bank_alloc.mem;
  nxrmutex_unlock(&g_bank_lock);
  return mem;
}

void lisa_bank_free(uint8_t bank, FAR void *mem)
{
  if (bank == g_db_reg && bank == g_sb_reg)
    {
      mm_free(g_bank[bank].heap, mem);
      return;
    }

  nxrmutex_lock(&g_bank_lock);
  g_bank_alloc.bank = bank;
  g_bank_alloc.mem  = mem;
  lisa_bank_visit(bank, lisa_bank_free_op, NULL);
  nxrmutex_unlock(&g_bank_lock);
}

/* The free bytes of an open bank's heap (a visit) */

static long lisa_bank_free_bytes(uint8_t n)
{
  lisa_bank_visit(n, lisa_bank_mallinfo_op, (FAR void *)(uintptr_t)n);
  return g_bank_info.fordblks;
}

/* A bank for a stack: the first open one that keeps
 * CONFIG_LISA_TASK_HEAP_RESERVE free after it (the tasks there need heap
 * too), else the first closed one, opened; as a last resort any bank
 * with room for the stack alone.  0 when none.
 */

uint8_t lisa_bank_pick(size_t size, FAR void **mem)
{
  uint8_t n;

  nxrmutex_lock(&g_bank_lock);
  for (n = 1; n < LISA_NBANKS; n++)
    {
      if (g_bank[n].heap != NULL &&
          lisa_bank_free_bytes(n) >= (long)size + CONFIG_LISA_TASK_HEAP_RESERVE)
        {
          *mem = lisa_bank_malloc(n, size);
          if (*mem != NULL)
            {
              goto out;
            }
        }
    }

  for (n = 2; n < LISA_NBANKS; n++)
    {
      if (g_bank[n].heap == NULL && lisa_bank_open(n) == OK)
        {
          *mem = lisa_bank_malloc(n, size);
          if (*mem != NULL)
            {
              goto out;
            }
        }
    }

  for (n = 1; n < LISA_NBANKS; n++)
    {
      if (g_bank[n].heap != NULL)
        {
          *mem = lisa_bank_malloc(n, size);
          if (*mem != NULL)
            {
              goto out;
            }
        }
    }

  n = 0;
  *mem = NULL;

out:
  nxrmutex_unlock(&g_bank_lock);
  return n;
}

void lisa_bank_task_add(uint8_t bank)
{
  g_bank[bank].ntasks++;
}

void lisa_bank_task_del(uint8_t bank)
{
  if (g_bank[bank].ntasks > 0)
    {
      g_bank[bank].ntasks--;
    }
}

/* Bank 1 is open since umm_initialize (its heap is USR_HEAP, the idle
 * thread's); list it and the total in /proc/meminfo
 */

void lisa_bank_initialize(void)
{
  g_bank[1].ntasks = 1;
  lisa_bank_register(1);

  g_banks_entry.name     = "Banks";
  g_banks_entry.heap     = NULL;
  g_banks_entry.mallinfo = lisa_banks_mallinfo;
  g_banks_entry.memdump  = lisa_bank_memdump;
  procfs_register_meminfo(&g_banks_entry);
}

#endif /* CONFIG_LISA_TASK_BANKS */
