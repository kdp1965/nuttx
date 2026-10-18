/****************************************************************************
 * arch/lisa/src/common/lisa_irq.c
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
#include <nuttx/arch.h>
#include <nuttx/irq.h>

#include "lisa_internal.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

__sfr __at(LISA_INT_ENABLE) g_int_enable;
__sfr __at(LISA_INT_STATUS) g_int_status;

/****************************************************************************
 * Public Data
 ****************************************************************************/

__data volatile uint8_t g_lisa_ie;
__data volatile uint8_t g_lisa_inirq;

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_irq_save
 *
 * Description:
 *   Disable interrupts and return the previous state: the shadow byte,
 *   swapped with 0 (A is the return value).
 *
 ****************************************************************************/

irqstate_t up_irq_save(void) __naked __nonbanked
{
  __asm
	di
	ldx	#_g_lisa_ie
	ldi	#0
	swap	0(ix)
	ret
  __endasm;
}

/****************************************************************************
 * Name: up_irq_restore
 *
 * Description:
 *   Restore the state up_irq_save returned (the byte parameter is in A)
 *
 ****************************************************************************/

void up_irq_restore(irqstate_t flags) __naked __nonbanked
{
  __asm
	sta	_g_lisa_ie
	cpi	#0
	bz	00001$
	ei
00001$:
	ret
  __endasm;
}

/****************************************************************************
 * Name: up_irq_enable
 *
 * Description:
 *   Enable interrupts and return the previous state
 *
 ****************************************************************************/

irqstate_t up_irq_enable(void) __naked __nonbanked
{
  __asm
	ldx	#_g_lisa_ie
	ldi	#1
	swap	0(ix)
	ei
	ret
  __endasm;
}

/****************************************************************************
 * Name: up_irqinitialize
 *
 * Description:
 *   No source enabled, nothing pending, then interrupts on: the drivers
 *   enable their sources in INT_ENABLE when they attach.
 *
 ****************************************************************************/

void up_irqinitialize(void)
{
  g_int_enable = 0;
  g_int_status = 0xff;
  g_lisa_ie    = 0;
  g_lisa_inirq = 0;

#ifndef CONFIG_SUPPRESS_INTERRUPTS
  up_irq_enable();
#endif
}
