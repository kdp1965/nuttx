/****************************************************************************
 * arch/lisa/src/common/lisa_registerdump.c
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
#include <stdint.h>
#include <debug.h>
#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include "lisa_internal.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_dump_register
 *
 * Description:
 *   Dump a register save area (a crash dump)
 *
 ****************************************************************************/

void up_dump_register(FAR void *dumpregs)
{
  FAR chipreg_t *regs = (FAR chipreg_t *)dumpregs;

  if (regs == NULL)
    {
      return;
    }

  _alert("PC: %02x%02x  CB: %02x  IE: %d  IRQ: %d\n",
         regs[XCPT_PC_HI] & 0x7f, regs[XCPT_PC_LO], regs[XCPT_CB],
         regs[XCPT_IE], regs[XCPT_IRQ]);
  _alert("SP: %02x%02x  SB: %02x  DB: %02x\n",
         regs[XCPT_SP_HI], regs[XCPT_SP_LO], regs[XCPT_SB], regs[XCPT_DB]);
  _alert("A: %02x  IX: %02x%02x  RA: %02x%02x  FLAGS: %02x  CSAVE: %02x\n",
         regs[XCPT_A], regs[XCPT_IX_HI], regs[XCPT_IX_LO],
         regs[XCPT_RA_HI], regs[XCPT_RA_LO], regs[XCPT_FLAGS],
         regs[XCPT_CSAVE]);
}
