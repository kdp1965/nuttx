/****************************************************************************
 * sched/task/task_argvstr.c
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
#include <nuttx/kmalloc.h>

#include <sched.h>
#include <stddef.h>
#include <stdio.h>

#include <nuttx/addrenv.h>
#include <nuttx/tls.h>

#include "sched/sched.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nxtask_argvstr
 *
 * Description:
 *   Safely read the contents of a task's argument vector, into a a safe
 *   buffer. Function skips the process's name.
 *
 * Input Parameters:
 *   tcb  - tcb of the task.
 *   args - Output buffer for the argument vector.
 *   size - Size of the buffer.
 *
 * Returned Value:
 *   The actual string length that was written.
 *
 ****************************************************************************/

#ifdef CONFIG_ARCH_BANKED_TASKS
/* The argv strings are on the task's stack, in its bank: formatted there
 * (up_task_call) into a buffer in the common bank
 */

struct argvstr_s
{
  FAR struct tcb_s *tcb;
  FAR char *buf;
  size_t size;
  size_t n;
};

static int nxtask_argvstr_op(FAR void *arg)
{
  FAR struct argvstr_s *a = (FAR struct argvstr_s *)arg;
  FAR char **argv = nxsched_get_stackargs(a->tcb);

  if (argv++)
    {
      while (*argv != NULL && a->n < a->size)
        {
          a->n += snprintf(a->buf + a->n, a->size - a->n, " %s", *argv++);
        }
    }

  return OK;
}
#endif

size_t nxtask_argvstr(FAR struct tcb_s *tcb, FAR char *args, size_t size)
{
  size_t n = 0;
#ifdef CONFIG_ARCH_ADDRENV
  FAR struct addrenv_s *oldenv;
#endif

  /* Sanity checks and idle tasks */

  if (!tcb || size < 1 || is_idle_task(tcb))
    {
      *args = '\0';
      return 0;
    }

#ifdef CONFIG_ARCH_ADDRENV
  if (tcb->addrenv_own != NULL)
    {
      addrenv_select(tcb->addrenv_own, &oldenv);
    }
#endif

#ifndef CONFIG_DISABLE_PTHREAD
  if ((tcb->flags & TCB_FLAG_TTYPE_MASK) == TCB_FLAG_TTYPE_PTHREAD)
    {
      FAR struct pthread_entry_s *entry =
        (FAR struct pthread_entry_s *)(tcb + 1);

      n += snprintf(args, size, " %p %p", tcb->entry.main, entry->arg);
    }
  else
#endif
    {
#ifdef CONFIG_ARCH_BANKED_TASKS
      FAR struct argvstr_s *a = kmm_malloc(sizeof(*a) + size);

      *args = '\0';
      if (a != NULL)
        {
          a->tcb    = tcb;
          a->buf    = (FAR char *)(a + 1);
          a->size   = size;
          a->n      = 0;
          a->buf[0] = '\0';
          up_task_call(tcb, nxtask_argvstr_op, a);
          n = a->n < size - 1 ? a->n : size - 1;
          memcpy(args, a->buf, n);
          args[n] = '\0';
          kmm_free(a);
        }
#else
      FAR char **argv = nxsched_get_stackargs(tcb);

      if (argv++)
        {
          while (*argv != NULL && n < size)
            {
              n += snprintf(args + n, size - n, " %s", *argv++);
            }
        }
#endif
    }

#ifdef CONFIG_ARCH_ADDRENV
  if (tcb->addrenv_own != NULL)
    {
      addrenv_restore(oldenv);
    }
#endif

  return n < size - 1 ? n : size - 1;
}
