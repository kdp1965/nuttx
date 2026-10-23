/****************************************************************************
 * arch/lisa/src/common/lisa_spi.c
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

/* The third chip's SPI master (ttsky26d RESPIN 29, src/lisa_spi.v) as
 * NuttX SPI bus 0.  A byte is written to DATA, BUSY polled, the received
 * byte read back from DATA (which clears DONE): at 12.5 MHz a byte is
 * 32 clocks, shorter than an interrupt would take, so no interrupt is
 * used.  The select is the device id's index: SPIDEV_FLASH(0) asserts CS0,
 * SPIDEV_xxx(1) CS1.  The TTLC's pins (mux 3, CTRL CLAIM) are not used.
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <nuttx/arch.h>
#include <nuttx/mutex.h>
#include <nuttx/spi/spi.h>
#include <arch/board/board.h>
#include <arch/lisa/chip.h>

#include "lisa_internal.h"

#ifdef CONFIG_LISA_SPI

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct lisa_spidev_s
{
  struct spi_dev_s dev;      /* first: the driver's handle */
  mutex_t lock;
  uint32_t frequency;        /* asked for */
  uint32_t actual;           /* clk / (2 (div + 1)) */
  uint8_t ctrl;              /* CTRL less the selects: mode and MISO */
  uint8_t sel;               /* the selects asserted */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int      spi_lock(FAR struct spi_dev_s *dev, bool lock);
static void     spi_select(FAR struct spi_dev_s *dev, uint32_t devid,
                           bool selected);
static uint32_t spi_setfrequency(FAR struct spi_dev_s *dev,
                                 uint32_t frequency);
static void     spi_setmode(FAR struct spi_dev_s *dev,
                            enum spi_mode_e mode);
static void     spi_setbits(FAR struct spi_dev_s *dev, int nbits);
static uint8_t  spi_status(FAR struct spi_dev_s *dev, uint32_t devid);
static uint32_t spi_send(FAR struct spi_dev_s *dev, uint32_t wd);
#ifdef CONFIG_SPI_EXCHANGE
static void     spi_exchange(FAR struct spi_dev_s *dev,
                             FAR const void *txbuffer, FAR void *rxbuffer,
                             size_t nwords);
#else
static void     spi_sndblock(FAR struct spi_dev_s *dev,
                             FAR const void *buffer, size_t nwords);
static void     spi_recvblock(FAR struct spi_dev_s *dev, FAR void *buffer,
                              size_t nwords);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

__sfr __at(LISA_SPI_DATA)   g_spi_data;
__sfr __at(LISA_SPI_CTRL)   g_spi_ctrl;
__sfr __at(LISA_SPI_DIV)    g_spi_div;
__sfr __at(LISA_SPI_STATUS) g_spi_status;

static const struct spi_ops_s g_spiops =
{
  spi_lock,
  spi_select,
  spi_setfrequency,
#ifdef CONFIG_SPI_DELAY_CONTROL
  NULL,
#endif
  spi_setmode,
  spi_setbits,
#ifdef CONFIG_SPI_HWFEATURES
  NULL,
#endif
  spi_status,
#ifdef CONFIG_SPI_CMDDATA
  NULL,
#endif
  spi_send,
#ifdef CONFIG_SPI_EXCHANGE
  spi_exchange,
#else
  spi_sndblock,
  spi_recvblock,
#endif
#ifdef CONFIG_SPI_TRIGGER
  NULL,
#endif
  NULL                       /* registercallback */
};

static struct lisa_spidev_s g_spidev;
static bool g_spi_initialized;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* One byte out and one in */

static uint8_t spi_xfer(uint8_t b)
{
  g_spi_data = b;
  while (g_spi_status & LISA_SPI_ST_BUSY)
    {
    }

  return g_spi_data;
}

static int spi_lock(FAR struct spi_dev_s *dev, bool lock)
{
  FAR struct lisa_spidev_s *priv = (FAR struct lisa_spidev_s *)dev;

  if (lock)
    {
      return nxmutex_lock(&priv->lock);
    }

  return nxmutex_unlock(&priv->lock);
}

static void spi_select(FAR struct spi_dev_s *dev, uint32_t devid,
                       bool selected)
{
  FAR struct lisa_spidev_s *priv = (FAR struct lisa_spidev_s *)dev;
  uint8_t cs = (SPIDEVID_INDEX(devid) & 1) ? LISA_SPI_CTRL_CS1 :
                                           LISA_SPI_CTRL_CS0;

  if (selected)
    {
      priv->sel |= cs;
    }
  else
    {
      priv->sel &= ~cs;
    }

  g_spi_ctrl = priv->ctrl | priv->sel;
}

/* The fastest clock not above the one asked for: clk / (2 (DIV + 1)),
 * DIV 0-255 (25 MHz to 98 kHz at 50 MHz)
 */

static uint32_t spi_setfrequency(FAR struct spi_dev_s *dev,
                                 uint32_t frequency)
{
  FAR struct lisa_spidev_s *priv = (FAR struct lisa_spidev_s *)dev;
  uint32_t half;
  uint32_t div;

  if (frequency == 0)
    {
      frequency = 1;
    }

  if (frequency == priv->frequency)
    {
      return priv->actual;
    }

  half = 2 * frequency;
  div  = (BOARD_CLOCK_HZ + half - 1) / half;      /* clocks per period / 2, rounded up */
  div  = div > 0 ? div - 1 : 0;
  if (div > 255)
    {
      div = 255;
    }

  g_spi_div       = (uint8_t)div;
  priv->frequency = frequency;
  priv->actual    = BOARD_CLOCK_HZ / (2 * (div + 1));
  return priv->actual;
}

static void spi_setmode(FAR struct spi_dev_s *dev, enum spi_mode_e mode)
{
  FAR struct lisa_spidev_s *priv = (FAR struct lisa_spidev_s *)dev;
  uint8_t ctrl = priv->ctrl & ~(LISA_SPI_CTRL_CPOL | LISA_SPI_CTRL_CPHA);

  switch (mode)
    {
      case SPIDEV_MODE1:
        ctrl |= LISA_SPI_CTRL_CPHA;
        break;

      case SPIDEV_MODE2:
        ctrl |= LISA_SPI_CTRL_CPOL;
        break;

      case SPIDEV_MODE3:
        ctrl |= LISA_SPI_CTRL_CPOL | LISA_SPI_CTRL_CPHA;
        break;

      default:
        break;
    }

  priv->ctrl = ctrl;
  g_spi_ctrl = ctrl | priv->sel;
}

/* Bytes only: the shift register is 8 bits */

static void spi_setbits(FAR struct spi_dev_s *dev, int nbits)
{
  UNUSED(dev);
  DEBUGASSERT(nbits == 8);
  UNUSED(nbits);
}

static uint8_t spi_status(FAR struct spi_dev_s *dev, uint32_t devid)
{
  UNUSED(dev);
  UNUSED(devid);
  return SPI_STATUS_PRESENT;
}

static uint32_t spi_send(FAR struct spi_dev_s *dev, uint32_t wd)
{
  UNUSED(dev);
  return spi_xfer((uint8_t)wd);
}

#ifdef CONFIG_SPI_EXCHANGE
static void spi_exchange(FAR struct spi_dev_s *dev,
                         FAR const void *txbuffer, FAR void *rxbuffer,
                         size_t nwords)
{
  FAR const uint8_t *tx = (FAR const uint8_t *)txbuffer;
  FAR uint8_t *rx = (FAR uint8_t *)rxbuffer;
  uint8_t b;

  UNUSED(dev);
  while (nwords-- > 0)
    {
      b = spi_xfer(tx ? *tx++ : 0xff);
      if (rx)
        {
          *rx++ = b;
        }
    }
}
#else
static void spi_sndblock(FAR struct spi_dev_s *dev,
                         FAR const void *buffer, size_t nwords)
{
  FAR const uint8_t *tx = (FAR const uint8_t *)buffer;

  UNUSED(dev);
  while (nwords-- > 0)
    {
      spi_xfer(*tx++);
    }
}

static void spi_recvblock(FAR struct spi_dev_s *dev, FAR void *buffer,
                          size_t nwords)
{
  FAR uint8_t *rx = (FAR uint8_t *)buffer;

  UNUSED(dev);
  while (nwords-- > 0)
    {
      *rx++ = spi_xfer(0xff);
    }
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lisa_spibus_initialize
 *
 * Description:
 *   SPI bus 0, the SPI master, in mode 0 at 400 kHz with nothing selected
 *   until a driver sets its own mode and frequency.  NULL for any other bus.
 *
 ****************************************************************************/

FAR struct spi_dev_s *lisa_spibus_initialize(int bus)
{
  FAR struct lisa_spidev_s *priv = &g_spidev;

  if (bus != 0)
    {
      return NULL;
    }

  if (!g_spi_initialized)
    {
      priv->dev.ops = &g_spiops;
      nxmutex_init(&priv->lock);
      priv->ctrl      = (uint8_t)(CONFIG_LISA_SPI_MISO <<
                                  LISA_SPI_CTRL_MISO_SHIFT);
      priv->sel       = 0;
      priv->frequency = 0;
      g_spi_ctrl      = priv->ctrl;
      spi_setfrequency(&priv->dev, 400000);
      g_spi_initialized = true;
    }

  return &priv->dev;
}

#endif /* CONFIG_LISA_SPI */
