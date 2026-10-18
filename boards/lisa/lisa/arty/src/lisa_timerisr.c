/****************************************************************************
 * boards/lisa/lisa/arty/src/lisa_timerisr.c
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
#include <arch/board/board.h>

#include "clock/clock.h"
#include "lisa_internal.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

__sfr __at(LISA_TIM1_PREDIV_LO)  g_tim1_prediv_lo;
__sfr __at(LISA_TIM1_PREDIV_HI)  g_tim1_prediv_hi;
__sfr __at(LISA_TIM1_PRELOAD_LO) g_tim1_preload_lo;
__sfr __at(LISA_TIM1_PRELOAD_HI) g_tim1_preload_hi;
__sfr __at(LISA_TIM1_CTRL)       g_tim1_ctrl;
__sfr __at(LISA_TIM1_TICK)       g_tim1_tick;
__sfr __at(LISA_INT_ENABLE)      g_int_enable;
__sfr __at(LISA_INT_STATUS)      g_int_status;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lisa_timerisr
 *
 * Description:
 *   The system timer interrupt: acknowledge it, then the tick
 *
 ****************************************************************************/

static int lisa_timerisr(int irq, FAR void *context, FAR void *arg)
{
  g_int_status = 1 << LISA_IRQ_TIMER1;
  nxsched_process_timer();
  return 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: up_timer_initialize
 *
 * Description:
 *   Timer 1 as the system timer: a 1 ms unit (the pre-divider counts
 *   prediv + 1 clocks), an interrupt every CONFIG_USEC_PER_TICK.
 *
 ****************************************************************************/

void up_timer_initialize(void)
{
  uint16_t prediv = (uint16_t)(BOARD_CLOCK_HZ / 1000UL) - 1;
  uint16_t ms     = CONFIG_USEC_PER_TICK / 1000;

  g_tim1_ctrl       = 0;
  g_tim1_prediv_lo  = (uint8_t)prediv;
  g_tim1_prediv_hi  = (uint8_t)(prediv >> 8);
  g_tim1_preload_lo = (uint8_t)ms;
  g_tim1_preload_hi = (uint8_t)(ms >> 8);

  irq_attach(LISA_IRQ_TIMER1, lisa_timerisr, NULL);

  g_int_status  = 1 << LISA_IRQ_TIMER1;
  g_int_enable |= 1 << LISA_IRQ_TIMER1;
  g_tim1_tick   = 0;       /* restarts the timer with the preload */
  g_tim1_ctrl   = 1;
}
