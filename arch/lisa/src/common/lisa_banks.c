/****************************************************************************
 * arch/lisa/src/common/lisa_banks.c
 *
 * Task banks: each task's heap in a granule of the data window and its
 * stack in a stack bank of the second LISA chip (lisa_internal.h, "Task
 * banks").
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
 * task banks every frame is in bank 1 / granule 1 and the visit is never
 * made
 */

__data volatile uint8_t g_lisa_bank = 1;
__data volatile uint8_t g_lisa_gran = 1;
__data uint8_t g_bank_new;
__data uint8_t g_bank_newdb;
__data uint8_t g_bank_ie;
__data uint8_t g_bank_sb;
__data uint8_t g_bank_db;
__data uint16_t g_bank_sp;
__data uint16_t g_bank_top;
struct lisa_bankop_s g_bank_op;
int g_bank_ret;

/* lisa_bank_call's callee: on the scratch stack, under the visited banks */

void lisa_bank_op(void) __nonbanked
{
  g_bank_ret = g_bank_op.fn(g_bank_op.arg);
}

#ifdef CONFIG_LISA_TASK_BANKS

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* A granule of the data window: free, a task's heap, or half of a stack
 * bank in use (bank n is granules 2n and 2n + 1; granule 0 is the common
 * 8K, granule 1 the kernel threads' heap)
 */

enum lisa_gran_e
{
  GRAN_FREE = 0,
  GRAN_HEAP,
  GRAN_BANK
};

struct lisa_bank_s
{
  FAR struct mm_heap_s *heap;       /* a shared bank: the heap its stacks come from (the structure inside the bank); NULL: closed or owned */
  uint8_t ntasks;                   /* the threads with a stack in it */
  uint8_t owner;                    /* an owned bank: the granule of the task whose heap extends into it; 0: shared */
  uint16_t stack;                   /* an owned bank: the owner's stack base */
  struct procfs_meminfo_entry_s entry;
};

/****************************************************************************
 * Public Data
 ****************************************************************************/

__sfr __at(LISA_DATA_BANK)  g_db_reg;
__sfr __at(LISA_STACK_BANK) g_sb_reg;

/* USR_HEAP: the heap of the data window, at LISA_DATA_WINDOW under every
 * task's own granule (umm_initialize makes granule 1's, the kernel
 * threads'; lisa_task_place makes a task's)
 */

FAR struct mm_heap_s *g_lisa_user_heap;

/****************************************************************************
 * Private Data
 ****************************************************************************/

static struct lisa_bank_s g_bank[LISA_NBANKS];
static uint8_t g_gran[LISA_NGRANULES];        /* enum lisa_gran_e */
static uint8_t g_gran_bank[LISA_NGRANULES];   /* a GRAN_HEAP granule: the owned bank its heap extends into, 0: none */

/* The scratch stacks of the visits, in the common 8K: the kernel's (under
 * g_bank_lock) and one for a visit made from an interrupt, which may cut
 * into the other
 */

static uint8_t g_visit_stack[LISA_VISIT_STACK];
static uint8_t g_irq_visit_stack[LISA_IRQ_VISIT_STACK];

/* One visit at a time (the scratch stack, the temporaries); recursive: a
 * visit may make another (it then runs on the same scratch, in place)
 */

static rmutex_t g_bank_lock = NXRMUTEX_INITIALIZER;

static const char g_bank_names[LISA_NBANKS][7] =
{
  "bank0",  "bank1",  "bank2",  "bank3",  "bank4",  "bank5",  "bank6",
  "bank7",  "bank8",  "bank9",  "bank10", "bank11", "bank12", "bank13",
  "bank14", "bank15"
};

static struct procfs_meminfo_entry_s g_banks_entry;   /* "Banks": the total */
static struct procfs_meminfo_entry_s g_umem_entry;    /* "Umem": the calling task's heap */

/* The arguments and results of the visits: in the common 8K, under
 * g_bank_lock
 */

static struct
{
  uint8_t bank;
  size_t size;
  FAR void *mem;
} g_bank_alloc;

static uint16_t g_heap_end;                   /* the heap lisa_heap_make_op makes: up to here */
static struct mallinfo g_bank_info;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* The visits (on the scratch stack, under the task's banks) */

static int lisa_bank_open_op(FAR void *arg)
{
  uint8_t n = (uint8_t)(uintptr_t)arg;
  struct mm_heap_config_s config;

  memset(&config, 0, sizeof(config));
  config.name     = g_bank_names[n];
  config.start    = (FAR void *)LISA_BANK_WINDOW;
  config.size     = LISA_BANK_TOP(n) - LISA_BANK_WINDOW;
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

/* A task's heap: from the data window to g_heap_end (0x4000, or the stack
 * of an owned bank above it); under the task's granule and bank
 */

static int lisa_heap_make_op(FAR void *arg)
{
  struct mm_heap_config_s config;

  memset(&config, 0, sizeof(config));
  config.name     = "Umem";
  config.start    = (FAR void *)LISA_DATA_WINDOW;
  config.size     = g_heap_end - LISA_DATA_WINDOW;
  config.noprocfs = true;
  return mm_initialize_heap(&config) ? OK : -ENOMEM;
}

static int lisa_heap_mallinfo_op(FAR void *arg)
{
  g_bank_info = mm_mallinfo(g_lisa_user_heap);
  return OK;
}

static void lisa_mallinfo_add(FAR struct mallinfo *total)
{
  total->ordblks  += g_bank_info.ordblks;
  total->aordblks += g_bank_info.aordblks;
  total->uordblks += g_bank_info.uordblks;
  total->fordblks += g_bank_info.fordblks;
  if (g_bank_info.mxordblk > total->mxordblk)
    {
      total->mxordblk = g_bank_info.mxordblk;
    }
}

static void lisa_mallinfo_free(FAR struct mallinfo *total, long size)
{
  total->ordblks++;
  total->fordblks += size;
  if (size > total->mxordblk)
    {
      total->mxordblk = size;
    }
}

/* /proc/meminfo: one stack bank (the entry's heap field carries the
 * number): a shared bank's stack heap, an owned bank wholly its owner's
 */

static struct mallinfo lisa_bank_mallinfo(FAR struct mm_heap_s *key)
{
  struct mallinfo info;
  uint8_t n = (uint8_t)(uintptr_t)key;

  memset(&info, 0, sizeof(info));
  nxrmutex_lock(&g_bank_lock);
  if (g_bank[n].owner != 0)
    {
      info.arena    = (long)(LISA_BANK_TOP(n) - LISA_BANK_WINDOW);
      info.uordblks = info.arena;
      info.aordblks = 1;
    }
  else if (g_bank[n].heap != NULL)
    {
      lisa_visit(n, g_db_reg, lisa_bank_mallinfo_op, (FAR void *)(uintptr_t)n);
      info = g_bank_info;
    }

  nxrmutex_unlock(&g_bank_lock);
  return info;
}

/* /proc/meminfo: the calling task's heap */

static struct mallinfo lisa_umem_mallinfo(FAR struct mm_heap_s *key)
{
  struct mallinfo info;

  info = mm_mallinfo(g_lisa_user_heap);
  return info;
}

static void lisa_umem_memdump(FAR struct mm_heap_s *key,
                              FAR const struct mm_memdump_s *dump)
{
  mm_memdump(g_lisa_user_heap, dump);
}

/* /proc/meminfo: the 31 granules - the shared stack banks, the tasks'
 * heaps (an owned bank with its owner's: the heap part and the stack),
 * the free ones wholly free
 */

static struct mallinfo lisa_banks_mallinfo(FAR struct mm_heap_s *key)
{
  struct mallinfo total;
  uint8_t n;
  long size;

  memset(&total, 0, sizeof(total));
  nxrmutex_lock(&g_bank_lock);
  for (n = 1; n < LISA_NBANKS; n++)
    {
      if (g_bank[n].owner != 0)
        {
          continue;                    /* with its owner's granule below */
        }

      if (g_bank[n].heap != NULL)
        {
          total.arena += (long)(LISA_BANK_TOP(n) - LISA_BANK_WINDOW);
          lisa_visit(n, g_db_reg, lisa_bank_mallinfo_op,
                     (FAR void *)(uintptr_t)n);
          lisa_mallinfo_add(&total);
        }
    }

  for (n = 1; n < LISA_NGRANULES; n++)
    {
      size = (long)LISA_GRANULE_SIZE;
      if (g_gran[n] == GRAN_BANK)
        {
          continue;                    /* counted as its bank */
        }

      if (g_gran[n] == GRAN_HEAP)
        {
          uint8_t b = g_gran_bank[n];

          if (b != 0)
            {
              size += (long)(LISA_BANK_TOP(b) - LISA_BANK_WINDOW);
            }

          total.arena += size;
          lisa_visit(b != 0 ? b : g_sb_reg, n, lisa_heap_mallinfo_op, NULL);
          lisa_mallinfo_add(&total);
          if (b != 0)
            {
              total.uordblks += (long)(LISA_BANK_TOP(b) - g_bank[b].stack);
              total.aordblks++;
            }
        }
      else
        {
          total.arena += size;
          lisa_mallinfo_free(&total, size);
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

  if (entry->name != NULL)
    {
      return;                          /* registered when first opened */
    }

  entry->name     = g_bank_names[n];
  entry->heap     = (FAR struct mm_heap_s *)(uintptr_t)n;
  entry->mallinfo = lisa_bank_mallinfo;
  entry->memdump  = lisa_bank_memdump;
  procfs_register_meminfo(entry);
}

/* A shared bank's stack heap (the whole bank) */

static int lisa_bank_open(uint8_t n)
{
  int ret = OK;

  nxrmutex_lock(&g_bank_lock);
  if (g_bank[n].heap == NULL)
    {
      ret = lisa_visit(n, g_db_reg, lisa_bank_open_op,
                       (FAR void *)(uintptr_t)n);
      if (ret == OK)
        {
          g_gran[2 * n]     = GRAN_BANK;
          g_gran[2 * n + 1] = GRAN_BANK;
          lisa_bank_register(n);
        }
    }

  nxrmutex_unlock(&g_bank_lock);
  return ret;
}

static bool lisa_bank_is_free(uint8_t n)
{
  return g_gran[2 * n] == GRAN_FREE && g_gran[2 * n + 1] == GRAN_FREE;
}

/* A visit made from a stack in the common 8K (the scratch of a visit, a
 * nested one): switch the banks in place, the stack stays visible
 */

static int lisa_visit_here(uint8_t sb, uint8_t db,
                           CODE int (*fn)(FAR void *), FAR void *arg)
{
  uint8_t osb = g_sb_reg;
  uint8_t odb = g_db_reg;
  int ret;

  g_sb_reg = sb;
  g_db_reg = db;
  ret = fn(arg);
  g_sb_reg = osb;
  g_db_reg = odb;
  return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

uint8_t lisa_sb(void) __nonbanked
{
  return g_sb_reg;
}

uint8_t lisa_db(void) __nonbanked
{
  return g_db_reg;
}

/* Run fn(arg) with the stack bank sb and the granule db visible: directly
 * when they are the ones in force, in place when this stack is in the
 * common 8K (it stays visible), else on the scratch stack through
 * lisa_bank_call.  arg must point into the common 8K.
 */

int lisa_visit(uint8_t sb, uint8_t db, CODE int (*fn)(FAR void *),
               FAR void *arg)
{
  int ret;

  if (sb == g_sb_reg && db == g_db_reg)
    {
      return fn(arg);
    }

  if (up_getsp() < LISA_COMMON_END)
    {
      return lisa_visit_here(sb, db, fn, arg);
    }

  if (up_interrupt_context())
    {
      /* No lock, and a scratch of its own: this interrupt may have cut
       * into a visit on the other
       */

      g_bank_op.fn  = fn;
      g_bank_op.arg = arg;
      g_bank_newdb  = db;
      g_bank_top    = (uint16_t)(uintptr_t)
                      &g_irq_visit_stack[LISA_IRQ_VISIT_STACK - 1];
      lisa_bank_call(sb);
      return g_bank_ret;
    }

  nxrmutex_lock(&g_bank_lock);
  g_bank_op.fn  = fn;
  g_bank_op.arg = arg;
  g_bank_newdb  = db;
  g_bank_top    = (uint16_t)(uintptr_t)&g_visit_stack[LISA_VISIT_STACK - 1];
  lisa_bank_call(sb);
  ret = g_bank_ret;
  nxrmutex_unlock(&g_bank_lock);
  return ret;
}

int up_task_call(FAR struct tcb_s *tcb, CODE int (*fn)(FAR void *),
                 FAR void *arg)
{
  return lisa_visit(tcb->xcp.bank, tcb->xcp.gran, fn, arg);
}

/* The running task's heap: USR_HEAP, the data window's under its granule */

FAR struct mm_heap_s **up_task_heap_ref(void)
{
  return &g_lisa_user_heap;
}

/* A stack in a shared bank's heap */

FAR void *lisa_bank_malloc(uint8_t bank, size_t size)
{
  FAR void *mem;

  if (g_bank[bank].heap == NULL)
    {
      return NULL;
    }

  nxrmutex_lock(&g_bank_lock);
  g_bank_alloc.bank = bank;
  g_bank_alloc.size = size;
  g_bank_alloc.mem  = NULL;
  lisa_visit(bank, g_db_reg, lisa_bank_malloc_op, NULL);
  mem = g_bank_alloc.mem;
  nxrmutex_unlock(&g_bank_lock);
  return mem;
}

void lisa_bank_free(uint8_t bank, FAR void *mem)
{
  if (g_bank[bank].heap == NULL)
    {
      return;
    }

  nxrmutex_lock(&g_bank_lock);
  g_bank_alloc.bank = bank;
  g_bank_alloc.mem  = mem;
  lisa_visit(bank, g_db_reg, lisa_bank_free_op, NULL);
  nxrmutex_unlock(&g_bank_lock);
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

/* A task's place: a granule for its heap; a stack of
 * CONFIG_LISA_TASK_OWN_BANK_STACK bytes or more at the top of a free bank
 * of its own, the heap then up to the stack, else in the first shared
 * bank with room (a new one when none has) and the heap the granule.
 * Sets the TCB's bank, granule and stack; -ENOMEM leaves the stack NULL.
 */

int lisa_task_place(FAR struct tcb_s *tcb, size_t size)
{
  FAR void *mem = NULL;
  uint16_t end = LISA_DATA_WINDOW + LISA_GRANULE_SIZE;
  uint8_t n = 0;
  uint8_t g;
  int ret = -ENOMEM;

  nxrmutex_lock(&g_bank_lock);

  /* The granule: the highest free one, taken now so that the bank search
   * below never picks the bank it is half of (the banks are taken from
   * the bottom, so the two meet late)
   */

  for (g = LISA_NGRANULES - 1; g > 1 && g_gran[g] != GRAN_FREE; g--);
  if (g <= 1)
    {
      goto out;
    }

  g_gran[g] = GRAN_HEAP;

  if (size >= CONFIG_LISA_TASK_OWN_BANK_STACK)
    {
      for (n = 2; n < LISA_NBANKS && !lisa_bank_is_free(n); n++);
      if (n < LISA_NBANKS)
        {
          end = (uint16_t)(LISA_BANK_TOP(n) - size);
          mem = (FAR void *)end;
          g_gran[2 * n]     = GRAN_BANK;
          g_gran[2 * n + 1] = GRAN_BANK;
          g_bank[n].owner   = g;
          g_bank[n].stack   = end;
          g_gran_bank[g]    = n;
          lisa_bank_register(n);
        }
      else
        {
          n = 0;
        }
    }

  if (n == 0)
    {
      for (n = 1; n < LISA_NBANKS; n++)
        {
          if (g_bank[n].heap != NULL && g_bank[n].owner == 0)
            {
              mem = lisa_bank_malloc(n, size);
              if (mem != NULL)
                {
                  break;
                }
            }
        }

      if (mem == NULL)
        {
          for (n = 2; n < LISA_NBANKS; n++)
            {
              if (lisa_bank_is_free(n) && lisa_bank_open(n) == OK)
                {
                  mem = lisa_bank_malloc(n, size);
                  if (mem != NULL)
                    {
                      break;
                    }
                }
            }
        }

      if (mem == NULL)
        {
          g_gran[g] = GRAN_FREE;
          goto out;
        }
    }

  g_bank[n].ntasks++;
  tcb->xcp.bank  = n;
  tcb->xcp.gran  = g;
  tcb->xcp.fbank = n;
  tcb->stack_alloc_ptr = mem;

  /* Its heap, made under its banks */

  g_heap_end = end;
  ret = lisa_visit(n, g, lisa_heap_make_op, NULL);
  if (ret != OK)
    {
      lisa_task_unplace(tcb);
    }

out:
  nxrmutex_unlock(&g_bank_lock);
  return ret;
}

/* The task's stack and granule back; a shared bank left empty is closed
 * (bank 1, the kernel threads', stays)
 */

void lisa_task_unplace(FAR struct tcb_s *tcb)
{
  uint8_t n = tcb->xcp.bank;
  uint8_t g = tcb->xcp.gran;

  nxrmutex_lock(&g_bank_lock);
  if (n != 0)
    {
      if (g_bank[n].owner == g && g != 0)
        {
          g_bank[n].owner   = 0;
          g_bank[n].stack   = 0;
          g_gran[2 * n]     = GRAN_FREE;
          g_gran[2 * n + 1] = GRAN_FREE;
        }
      else if (tcb->stack_alloc_ptr != NULL)
        {
          lisa_bank_free(n, tcb->stack_alloc_ptr);
        }

      lisa_bank_task_del(n);
      if (n != 1 && g_bank[n].owner == 0 && g_bank[n].ntasks == 0 &&
          g_bank[n].heap != NULL)
        {
          g_bank[n].heap    = NULL;
          g_gran[2 * n]     = GRAN_FREE;
          g_gran[2 * n + 1] = GRAN_FREE;
        }
    }

  if (g > 1 && g_gran[g] == GRAN_HEAP)
    {
      g_gran[g]      = GRAN_FREE;
      g_gran_bank[g] = 0;
    }

  tcb->stack_alloc_ptr = NULL;
  nxrmutex_unlock(&g_bank_lock);
}

/* At up_initialize: granule 1's heap is USR_HEAP since umm_initialize
 * (the kernel threads' and the idle thread's); bank 1 gets its stack heap
 * (the idle stack at its top), /proc/meminfo its entries
 */

void lisa_bank_initialize(void)
{
  g_gran[0] = GRAN_BANK;               /* the common 8K: never a heap */
  g_gran[1] = GRAN_HEAP;
  lisa_bank_open(1);
  g_bank[1].ntasks = 1;

  g_umem_entry.name     = "Umem";
  g_umem_entry.heap     = NULL;
  g_umem_entry.mallinfo = lisa_umem_mallinfo;
  g_umem_entry.memdump  = lisa_umem_memdump;
  procfs_register_meminfo(&g_umem_entry);

  g_banks_entry.name     = "Banks";
  g_banks_entry.heap     = NULL;
  g_banks_entry.mallinfo = lisa_banks_mallinfo;
  g_banks_entry.memdump  = lisa_bank_memdump;
  procfs_register_meminfo(&g_banks_entry);
}

#endif /* CONFIG_LISA_TASK_BANKS */
