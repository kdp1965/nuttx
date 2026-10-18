/****************************************************************************
 * boards/lisa/lisa/arty/include/board.h
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

#ifndef __BOARDS_LISA_LISA_ARTY_INCLUDE_BOARD_H
#define __BOARDS_LISA_LISA_ARTY_INCLUDE_BOARD_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* The second LISA chip's RTL on the Digilent Arty A7
 * (ttsky26d-um-lisa-ttlc fpga/arty_a7): 50 MHz, the console on UART1 (the
 * baud rate is the UART's own, 460800 by default).  lisa_sim --rev2 runs
 * the same image.
 */

#define BOARD_CLOCK_HZ      50000000UL

#endif /* __BOARDS_LISA_LISA_ARTY_INCLUDE_BOARD_H */
