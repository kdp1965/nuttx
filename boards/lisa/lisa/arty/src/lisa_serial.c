/****************************************************************************
 * boards/lisa/lisa/arty/src/lisa_serial.c
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
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/serial/serial.h>
#include <arch/board/board.h>

#include "lisa_internal.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

/* UART1 (lisa_isa.md, RESPIN items 14 and 23): a data register, a status
 * register (RX data available, TX buffer empty, RX overrun), an 8-byte
 * receive FIFO.  Its interrupt (INT_STATUS bit 2) is raised by the rising
 * edge of RX data available or of TX buffer empty, each under its own
 * enable in UART_INT_EN, and is acknowledged by writing the bit.
 */

__sfr __at(LISA_UART1_DATA)   g_uart1_data;
__sfr __at(LISA_UART1_STATUS) g_uart1_status;
__sfr __at(LISA_UART_INT_EN)  g_uart_int_en;
__sfr __at(LISA_INT_ENABLE)   g_int_enable;
__sfr __at(LISA_INT_STATUS)   g_int_status;

#ifdef USE_SERIALDRIVER

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  lisa_setup(FAR struct uart_dev_s *dev);
static void lisa_shutdown(FAR struct uart_dev_s *dev);
static int  lisa_attach(FAR struct uart_dev_s *dev);
static void lisa_detach(FAR struct uart_dev_s *dev);
static int  lisa_ioctl(FAR struct file *filep, int cmd, unsigned long arg);
static int  lisa_receive(FAR struct uart_dev_s *dev, FAR unsigned int *status);
static void lisa_rxint(FAR struct uart_dev_s *dev, bool enable);
static bool lisa_rxavailable(FAR struct uart_dev_s *dev);
static void lisa_send(FAR struct uart_dev_s *dev, int ch);
static void lisa_txint(FAR struct uart_dev_s *dev, bool enable);
static bool lisa_txready(FAR struct uart_dev_s *dev);
static bool lisa_txempty(FAR struct uart_dev_s *dev);

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct uart_ops_s g_uart_ops =
{
  lisa_setup,                 /* setup */
  lisa_shutdown,              /* shutdown */
  lisa_attach,                /* attach */
  lisa_detach,                /* detach */
  lisa_ioctl,                 /* ioctl */
  lisa_receive,               /* receive */
  lisa_rxint,                 /* rxint */
  lisa_rxavailable,           /* rxavailable */
#ifdef CONFIG_SERIAL_IFLOWCONTROL
  NULL,                       /* rxflowcontrol */
#endif
#ifdef CONFIG_SERIAL_TXDMA
  NULL,                       /* dmasend */
#endif
#ifdef CONFIG_SERIAL_RXDMA
  NULL,                       /* dmareceive */
  NULL,                       /* dmarxfree */
#endif
#ifdef CONFIG_SERIAL_TXDMA
  NULL,                       /* dmatxavail */
#endif
  lisa_send,                  /* send */
  lisa_txint,                 /* txint */
  lisa_txready,               /* txready */
  lisa_txempty,               /* txempty */
};

static char g_uart1rxbuffer[CONFIG_UART_RXBUFSIZE];
static char g_uart1txbuffer[CONFIG_UART_TXBUFSIZE];

static uart_dev_t g_uart1port =
{
  .isconsole = true,
  .recv =
  {
    .size   = CONFIG_UART_RXBUFSIZE,
    .buffer = g_uart1rxbuffer,
  },
  .xmit =
  {
    .size   = CONFIG_UART_TXBUFSIZE,
    .buffer = g_uart1txbuffer,
  },
  .ops = &g_uart_ops,
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lisa_uart_isr
 *
 * Description:
 *   The UART1 interrupt: acknowledge it first (a byte arriving while the
 *   FIFO is drained raises a new one), then take what the FIFO holds and
 *   feed the transmitter.
 *
 ****************************************************************************/

static int lisa_uart_isr(int irq, FAR void *context, FAR void *arg)
{
  FAR struct uart_dev_s *dev = (FAR struct uart_dev_s *)arg;

  g_int_status = 1 << LISA_IRQ_UART1;

  if ((g_uart1_status & LISA_UART_RX_AVAIL) != 0)
    {
      uart_recvchars(dev);
    }

  if ((g_uart_int_en & LISA_UART1_TX_IE) != 0 &&
      (g_uart1_status & LISA_UART_TX_EMPTY) != 0)
    {
      uart_xmitchars(dev);
    }

  return OK;
}

/****************************************************************************
 * Name: lisa_setup
 *
 * Description:
 *   Configure the UART baud, bits, parity, fifos, etc.  The UART's rate is
 *   its own: only the receive FIFO is emptied.
 *
 ****************************************************************************/

static int lisa_setup(FAR struct uart_dev_s *dev)
{
  uint8_t ch;

  while ((g_uart1_status & LISA_UART_RX_AVAIL) != 0)
    {
      ch = g_uart1_data;
      UNUSED(ch);
    }

  return OK;
}

/****************************************************************************
 * Name: lisa_shutdown
 ****************************************************************************/

static void lisa_shutdown(FAR struct uart_dev_s *dev)
{
  g_uart_int_en &= ~(LISA_UART1_RX_IE | LISA_UART1_TX_IE);
}

/****************************************************************************
 * Name: lisa_attach
 *
 * Description:
 *   Attach the UART interrupt and enable its source; the RX and TX
 *   requests themselves are enabled by rxint / txint.
 *
 ****************************************************************************/

static int lisa_attach(FAR struct uart_dev_s *dev)
{
  int ret = irq_attach(LISA_IRQ_UART1, lisa_uart_isr, dev);
  if (ret == OK)
    {
      g_int_status  = 1 << LISA_IRQ_UART1;
      g_int_enable |= 1 << LISA_IRQ_UART1;
    }

  return ret;
}

/****************************************************************************
 * Name: lisa_detach
 ****************************************************************************/

static void lisa_detach(FAR struct uart_dev_s *dev)
{
  g_int_enable &= ~(1 << LISA_IRQ_UART1);
  irq_detach(LISA_IRQ_UART1);
}

/****************************************************************************
 * Name: lisa_ioctl
 ****************************************************************************/

static int lisa_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
  return -ENOTTY;
}

/****************************************************************************
 * Name: lisa_receive
 *
 * Description:
 *   Called (usually) from the interrupt level to receive one character
 *   from the UART.  Error bits associated with the receipt are provided in
 *   the return 'status'.
 *
 ****************************************************************************/

static int lisa_receive(FAR struct uart_dev_s *dev, FAR unsigned int *status)
{
  *status = g_uart1_status;
  return g_uart1_data;
}

/****************************************************************************
 * Name: lisa_rxint
 *
 * Description:
 *   Call to enable or disable RX interrupts
 *
 ****************************************************************************/

static void lisa_rxint(FAR struct uart_dev_s *dev, bool enable)
{
  irqstate_t flags = enter_critical_section();

  if (enable)
    {
      g_uart_int_en |= LISA_UART1_RX_IE;
    }
  else
    {
      g_uart_int_en &= ~LISA_UART1_RX_IE;
    }

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: lisa_rxavailable
 ****************************************************************************/

static bool lisa_rxavailable(FAR struct uart_dev_s *dev)
{
  return (g_uart1_status & LISA_UART_RX_AVAIL) != 0;
}

/****************************************************************************
 * Name: lisa_send
 ****************************************************************************/

static void lisa_send(FAR struct uart_dev_s *dev, int ch)
{
  g_uart1_data = (uint8_t)ch;
}

/****************************************************************************
 * Name: lisa_txint
 *
 * Description:
 *   Call to enable or disable TX interrupts.  The request comes with the
 *   edge of the buffer emptying: when it is empty already, feed it now.
 *
 ****************************************************************************/

static void lisa_txint(FAR struct uart_dev_s *dev, bool enable)
{
  irqstate_t flags = enter_critical_section();

  if (enable)
    {
      g_uart_int_en |= LISA_UART1_TX_IE;
      if ((g_uart1_status & LISA_UART_TX_EMPTY) != 0)
        {
          uart_xmitchars(dev);
        }
    }
  else
    {
      g_uart_int_en &= ~LISA_UART1_TX_IE;
    }

  leave_critical_section(flags);
}

/****************************************************************************
 * Name: lisa_txready
 ****************************************************************************/

static bool lisa_txready(FAR struct uart_dev_s *dev)
{
  return (g_uart1_status & LISA_UART_TX_EMPTY) != 0;
}

/****************************************************************************
 * Name: lisa_txempty
 ****************************************************************************/

static bool lisa_txempty(FAR struct uart_dev_s *dev)
{
  return (g_uart1_status & LISA_UART_TX_EMPTY) != 0;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lisa_serial_initialize
 *
 * Description:
 *   Register the console and the serial port
 *
 ****************************************************************************/

void lisa_serial_initialize(void)
{
  uart_register("/dev/console", &g_uart1port);
  uart_register("/dev/ttyS0", &g_uart1port);
}

#endif /* USE_SERIALDRIVER */

/****************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Provide priority, low-level access to support OS debug writes: polled
 *
 ****************************************************************************/

void up_putc(int ch)
{
  while ((g_uart1_status & LISA_UART_TX_EMPTY) == 0)
    {
    }

  g_uart1_data = (uint8_t)ch;
}
