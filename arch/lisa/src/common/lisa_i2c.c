/****************************************************************************
 * arch/lisa/src/common/lisa_i2c.c
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

/* The chip's I2C master (ttsky26d src/lisa_i2c.v, the OpenCores byte and
 * bit controllers) as NuttX I2C bus 0.  Each byte on the bus is one write
 * to CMD (STA / STO / RD / WR / ACK) and TIP polled: a byte is 9 SCL
 * periods, 22.5 us at 400 kHz, so no interrupt is used.  Messages follow
 * one another with a repeated start, or with none when the next one has
 * I2C_M_NOSTART (the SSD1306 driver sends the control byte and the data as
 * two such messages); the last ends with a stop unless I2C_M_NOSTOP.  The
 * pins come from the uio mux (debug register 0x1c, set by the host: 0x00fa
 * is io mux mode 3 with SCL / SDA on uio[4] / uio[5]).
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
#include <nuttx/i2c/i2c_master.h>
#include <arch/board/board.h>
#include <arch/lisa/chip.h>

#include "lisa_internal.h"

#ifdef CONFIG_LISA_I2C

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* TIP polls before a byte is given up: a byte at 10 kHz is 0.9 ms; a slave
 * may stretch SCL, but not forever
 */

#define I2C_POLLS  200000UL

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct lisa_i2cdev_s
{
  struct i2c_master_s dev;   /* first: the driver's handle */
  mutex_t lock;
  uint32_t frequency;        /* the prescaler's, 0 before the first */
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int i2c_transfer(FAR struct i2c_master_s *dev,
                        FAR struct i2c_msg_s *msgs, int count);
#ifdef CONFIG_I2C_RESET
static int i2c_reset(FAR struct i2c_master_s *dev);
#endif

/****************************************************************************
 * Private Data
 ****************************************************************************/

__sfr __at(LISA_I2C_PRE_LO) g_i2c_pre_lo;
__sfr __at(LISA_I2C_PRE_HI) g_i2c_pre_hi;
__sfr __at(LISA_I2C_CTRL)   g_i2c_ctrl;
__sfr __at(LISA_I2C_RX)     g_i2c_rx;
__sfr __at(LISA_I2C_STATUS) g_i2c_status;
__sfr __at(LISA_I2C_TX)     g_i2c_tx;
__sfr __at(LISA_I2C_CMD)    g_i2c_cmd;

static const struct i2c_ops_s g_i2cops =
{
  i2c_transfer,
#ifdef CONFIG_I2C_RESET
  i2c_reset,
#endif
  NULL,                      /* setup */
  NULL                       /* shutdown */
};

static struct lisa_i2cdev_s g_i2cdev;
static bool g_i2c_initialized;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/* The prescaler for an SCL frequency: clk / (5 SCL) - 1, written while the
 * core is disabled
 */

static void i2c_setfrequency(FAR struct lisa_i2cdev_s *priv,
                             uint32_t frequency)
{
  uint32_t pre;

  if (frequency == 0)
    {
      frequency = 100000;
    }

  if (frequency == priv->frequency)
    {
      return;
    }

  pre = BOARD_CLOCK_HZ / (5 * frequency);
  pre = pre > 0 ? pre - 1 : 0;
  if (pre > 0xffff)
    {
      pre = 0xffff;
    }

  g_i2c_ctrl   = 0;
  g_i2c_pre_lo = (uint8_t)pre;
  g_i2c_pre_hi = (uint8_t)(pre >> 8);
  g_i2c_ctrl   = LISA_I2C_CTRL_EN;
  priv->frequency = frequency;
}

/* One byte on the bus: cmd (with WR or RD) written, TIP waited for.  0, or
 * -ETIMEDOUT, or -EAGAIN when arbitration was lost
 */

static int i2c_cmd(uint8_t cmd)
{
  uint32_t n;

  g_i2c_cmd = cmd;
  for (n = 0; n < I2C_POLLS; n++)
    {
      if (!(g_i2c_status & LISA_I2C_ST_TIP))
        {
          return (g_i2c_status & LISA_I2C_ST_AL) ? -EAGAIN : 0;
        }
    }

  return -ETIMEDOUT;
}

/* A stop on its own, after an error */

static void i2c_stop(void)
{
  uint32_t n;

  g_i2c_cmd = LISA_I2C_CMD_STO;
  for (n = 0; n < I2C_POLLS && (g_i2c_status & LISA_I2C_ST_BUSY); n++)
    {
    }
}

static int i2c_transfer(FAR struct i2c_master_s *dev,
                        FAR struct i2c_msg_s *msgs, int count)
{
  FAR struct lisa_i2cdev_s *priv = (FAR struct lisa_i2cdev_s *)dev;
  FAR struct i2c_msg_s *msg;
  FAR uint8_t *p;
  bool last_byte;
  bool stop;
  uint8_t cmd;
  ssize_t n;
  int ret;
  int i;

  if (count <= 0)
    {
      return -EINVAL;
    }

  ret = nxmutex_lock(&priv->lock);
  if (ret < 0)
    {
      return ret;
    }

  for (i = 0; i < count; i++)
    {
      msg = &msgs[i];
      if (msg->flags & I2C_M_TEN)
        {
          ret = -ENOSYS;
          break;
        }

      i2c_setfrequency(priv, msg->frequency);

      /* The stop: after the last message, unless it says not; none between
       * messages (the next one restarts, or continues with I2C_M_NOSTART)
       */

      stop = (i == count - 1) && !(msg->flags & I2C_M_NOSTOP);

      if (!(msg->flags & I2C_M_NOSTART))
        {
          g_i2c_tx = (uint8_t)((msg->addr << 1) |
                               ((msg->flags & I2C_M_READ) ? 1 : 0));
          ret = i2c_cmd(LISA_I2C_CMD_STA | LISA_I2C_CMD_WR |
                        (msg->length == 0 && stop ? LISA_I2C_CMD_STO : 0));
          if (ret < 0)
            {
              break;
            }

          if (g_i2c_status & LISA_I2C_ST_RXACK)
            {
              ret = -ENXIO;              /* nobody at that address */
              break;
            }
        }

      p = msg->buffer;
      for (n = 0; n < msg->length; n++)
        {
          last_byte = n == msg->length - 1;
          if (msg->flags & I2C_M_READ)
            {
              /* NAK the last byte of a read: the slave lets go of SDA */

              cmd = LISA_I2C_CMD_RD |
                    (last_byte ? LISA_I2C_CMD_ACK : 0) |
                    (last_byte && stop ? LISA_I2C_CMD_STO : 0);
              ret = i2c_cmd(cmd);
              if (ret < 0)
                {
                  break;
                }

              *p++ = g_i2c_rx;
            }
          else
            {
              g_i2c_tx = *p++;
              ret = i2c_cmd(LISA_I2C_CMD_WR |
                            (last_byte && stop ? LISA_I2C_CMD_STO : 0));
              if (ret < 0)
                {
                  break;
                }

              if (g_i2c_status & LISA_I2C_ST_RXACK)
                {
                  ret = -EIO;            /* the slave refused a byte */
                  break;
                }
            }
        }

      if (ret < 0)
        {
          break;
        }
    }

  if (ret < 0)
    {
      i2c_stop();
    }

  nxmutex_unlock(&priv->lock);
  return ret < 0 ? ret : OK;
}

#ifdef CONFIG_I2C_RESET
static int i2c_reset(FAR struct i2c_master_s *dev)
{
  FAR struct lisa_i2cdev_s *priv = (FAR struct lisa_i2cdev_s *)dev;
  uint32_t frequency = priv->frequency;

  i2c_stop();
  priv->frequency = 0;
  i2c_setfrequency(priv, frequency);
  return OK;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: lisa_i2cbus_initialize
 *
 * Description:
 *   I2C bus 0, the I2C master, enabled at 100 kHz until a message asks for
 *   another frequency.  NULL for any other bus.
 *
 ****************************************************************************/

FAR struct i2c_master_s *lisa_i2cbus_initialize(int bus)
{
  FAR struct lisa_i2cdev_s *priv = &g_i2cdev;

  if (bus != 0)
    {
      return NULL;
    }

  if (!g_i2c_initialized)
    {
      priv->dev.ops = &g_i2cops;
      nxmutex_init(&priv->lock);
      priv->frequency = 0;
      i2c_setfrequency(priv, 100000);
      g_i2c_initialized = true;
    }

  return &priv->dev;
}

#endif /* CONFIG_LISA_I2C */
