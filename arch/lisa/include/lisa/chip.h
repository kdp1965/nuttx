/****************************************************************************
 * arch/lisa/include/lisa/chip.h
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

#ifndef __ARCH_LISA_INCLUDE_LISA_CHIP_H
#define __ARCH_LISA_INCLUDE_LISA_CHIP_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The peripheral registers (lisa_isa.md, the second chip's lisa_periph.v)
 * sit in the direct address space at 0x200 + the register: a C file
 * declares one as __sfr __at(LISA_UART1_DATA) name;
 */

#define LISA_PORT_A_IN        0x200
#define LISA_PORT_B_OUT       0x201
#define LISA_PORT_C_DATA      0x202
#define LISA_PORT_C_DIR       0x203
#define LISA_PORT_D_IN        0x204
#define LISA_PORT_E_OUT       0x205
#define LISA_PORT_A_INT_CFG0  0x206
#define LISA_PORT_A_INT_CFG1  0x207
#define LISA_TIM1_PREDIV_LO   0x208   /* a 1 ms unit: prediv + 1 clocks */
#define LISA_TIM1_PREDIV_HI   0x209
#define LISA_TIM1_PRELOAD_LO  0x20a   /* ms per interrupt */
#define LISA_TIM1_PRELOAD_HI  0x20b
#define LISA_TIM1_CTRL        0x20c   /* bit 0 enable; bit 7 rollover (cleared by a read) */
#define LISA_TIM1_TICK        0x20d   /* the tick count; a write restarts the timer */
#define LISA_INT_ENABLE       0x20e   /* the per-source mask, LISA_IRQ_* bits */
#define LISA_INT_STATUS       0x20f   /* the pending sources, write 1 to clear */
#define LISA_UART1_DATA       0x210
#define LISA_UART1_STATUS     0x211   /* bit 0 RX data, bit 1 TX buffer empty, bit 2 RX overrun */
#define LISA_UART2_DATA       0x212
#define LISA_UART2_STATUS     0x213
#define LISA_UART_INT_EN      0x214   /* bit 0 UART1 RX, bit 1 UART1 TX, bits 2-3 UART2 */
#define LISA_CODE_BANK        0x215
#define LISA_DATA_BANK        0x216
#define LISA_STACK_BANK       0x217
#define LISA_TIM2_PREDIV_LO   0x218
#define LISA_TIM2_PREDIV_HI   0x219
#define LISA_TIM2_PRELOAD_LO  0x21a
#define LISA_TIM2_PRELOAD_HI  0x21b
#define LISA_TIM2_CTRL        0x21c
#define LISA_TIM2_TICK        0x21d
#define LISA_UART2_BRG        0x21e
#define LISA_SPI_DATA         0x228   /* write: send a byte (ignored while busy); read: the byte received */
#define LISA_SPI_CTRL         0x229   /* LISA_SPI_CTRL_* bits */
#define LISA_SPI_DIV          0x22a   /* half an SCLK period is DIV + 1 clocks */
#define LISA_SPI_STATUS       0x22b   /* LISA_SPI_ST_* bits */

#define LISA_UART_RX_AVAIL    0x01
#define LISA_UART_TX_EMPTY    0x02
#define LISA_UART_RX_OVERRUN  0x04

#define LISA_UART1_RX_IE      0x01
#define LISA_UART1_TX_IE      0x02
#define LISA_UART2_RX_IE      0x04
#define LISA_UART2_TX_IE      0x08

/* The SPI master (ttsky26d RESPIN 29): CTRL and STATUS bits */

#define LISA_SPI_CTRL_CS0     0x01    /* select 0 asserted (the pin low) */
#define LISA_SPI_CTRL_CS1     0x02
#define LISA_SPI_CTRL_CPOL    0x04
#define LISA_SPI_CTRL_CPHA    0x08
#define LISA_SPI_CTRL_CLAIM   0x10    /* ask for the TTLC's pins (mux 3) */
#define LISA_SPI_CTRL_MISO_SHIFT 5    /* MISO from ui_in[0], [1], [2] or [7] */
#define LISA_SPI_CTRL_IE      0x80    /* DONE requests an interrupt */

#define LISA_SPI_ST_BUSY      0x01
#define LISA_SPI_ST_DONE      0x02
#define LISA_SPI_ST_OWNED     0x04
#define LISA_SPI_ST_TTLC_WAIT 0x08

#endif /* __ARCH_LISA_INCLUDE_LISA_CHIP_H */
