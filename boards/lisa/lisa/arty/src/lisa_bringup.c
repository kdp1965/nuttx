/****************************************************************************
 * boards/lisa/lisa/arty/src/lisa_bringup.c
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
#include <sys/mount.h>
#include <debug.h>
#include <nuttx/board.h>
#include <nuttx/fs/fs.h>

#ifdef CONFIG_ARTY_SPIFLASH
#  include <nuttx/spi/spi.h>
#  include <nuttx/mtd/mtd.h>
#  include <syslog.h>
#endif

#ifdef CONFIG_ARTY_OLED
#  include <errno.h>
#  include <syslog.h>
#  include <nuttx/i2c/i2c_master.h>
#  include <nuttx/lcd/lcd.h>
#  include <nuttx/lcd/ssd1306.h>
#  include <nuttx/video/fb.h>
#endif

#ifdef CONFIG_ARTY_LCD_BACKPACK
#  include <syslog.h>
#  include <nuttx/lcd/pcf8574_lcd_backpack.h>
#endif

#include "lisa_internal.h"

/****************************************************************************
 * Private Data
 ****************************************************************************/

#ifdef CONFIG_ARTY_OLED
static FAR struct lcd_dev_s *g_oled;
#endif

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifdef CONFIG_ARTY_SPIFLASH
/* The SST25VF064C on the SPI master's CS0: an MTD device (sst25xx), SMART
 * over it as /dev/smart0 and smartfs mounted at CONFIG_ARTY_SPIFLASH_MOUNT.
 * A blank or foreign flash does not mount: `mksmartfs /dev/smart0` once,
 * then `mount -t smartfs /dev/smart0 <dir>` (or reboot).
 */

static void arty_spiflash(void)
{
  FAR struct spi_dev_s *spi;
  FAR struct mtd_dev_s *mtd;
  int ret;

  spi = lisa_spibus_initialize(0);
  if (spi == NULL)
    {
      syslog(LOG_ERR, "spiflash: no SPI bus 0\n");
      return;
    }

  mtd = sst25xx_initialize(spi);
  if (mtd == NULL)
    {
      syslog(LOG_ERR, "spiflash: no SST25 flash on SPI CS0\n");
      return;
    }

  ret = smart_initialize(0, mtd, NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "spiflash: smart_initialize: %d\n", ret);
      return;
    }

  ret = nx_mount("/dev/smart0", CONFIG_ARTY_SPIFLASH_MOUNT, "smartfs", 0,
                 NULL);
  if (ret < 0)
    {
      syslog(LOG_ERR, "spiflash: smartfs on /dev/smart0 not mounted (%d): "
             "mksmartfs /dev/smart0 first\n", ret);
      return;
    }

  syslog(LOG_INFO, "spiflash: smartfs on %s\n", CONFIG_ARTY_SPIFLASH_MOUNT);
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#ifdef CONFIG_ARTY_OLED
/****************************************************************************
 * Name: board_lcd_initialize / board_lcd_getdev / board_lcd_uninitialize
 *
 * Description:
 *   The SSD1309 128x64 OLED on I2C bus 0 (io mux mode 3: SCL / SDA on
 *   uio[4] / uio[5], the Arty's JD pins 1 / 2 with SW2 up) through the
 *   SSD1306 driver; the LCD framebuffer (/dev/fb0) calls these.
 *
 ****************************************************************************/

int board_lcd_initialize(void)
{
  FAR struct i2c_master_s *i2c;

  if (g_oled != NULL)
    {
      return OK;
    }

  i2c = lisa_i2cbus_initialize(0);
  if (i2c == NULL)
    {
      syslog(LOG_ERR, "oled: no I2C bus 0\n");
      return -ENODEV;
    }

  g_oled = ssd1306_initialize(i2c, NULL, 0);
  if (g_oled == NULL)
    {
      syslog(LOG_ERR, "oled: no SSD1306 / SSD1309 at 0x%02x\n",
             CONFIG_SSD1306_I2CADDR);
      return -ENODEV;
    }

  g_oled->setpower(g_oled, CONFIG_LCD_MAXPOWER);
  syslog(LOG_INFO, "oled: SSD1309 at 0x%02x on I2C bus 0\n",
         CONFIG_SSD1306_I2CADDR);
  return OK;
}

FAR struct lcd_dev_s *board_lcd_getdev(int devno)
{
  return devno == 0 ? g_oled : NULL;
}

void board_lcd_uninitialize(void)
{
  if (g_oled != NULL)
    {
      g_oled->setpower(g_oled, 0);
    }
}
#endif

/****************************************************************************
 * Name: board_late_initialize
 *
 * Description:
 *   Called after up_initialize() and the board's early initialization: the
 *   place for the board's device drivers.  Mounts procfs (nsh's ps and free
 *   read it; without boardctl nothing else would).
 *
 ****************************************************************************/

#ifdef CONFIG_BOARD_LATE_INITIALIZE
void board_late_initialize(void)
{
#ifdef CONFIG_FS_PROCFS
  int ret = nx_mount(NULL, "/proc", "procfs", 0, NULL);
  if (ret < 0)
    {
      serr("ERROR: Failed to mount procfs at /proc: %d\n", ret);
    }
#endif

#ifdef CONFIG_ARTY_SPIFLASH
  arty_spiflash();
#endif

#ifdef CONFIG_ARTY_LCD_BACKPACK
  {
    /* YwRobot / SainSmart: P0 RS, P1 RW, P2 E, P3 backlight (high on),
     * P4-P7 D4-D7
     */

    static struct pcf8574_lcd_backpack_config_s cfg =
      LCD_I2C_BACKPACK_CFG_SAINSMART;
    int r;

    cfg.addr = CONFIG_ARTY_LCD_BACKPACK_ADDR;
    cfg.rows = CONFIG_ARTY_LCD_BACKPACK_ROWS;
    cfg.cols = CONFIG_ARTY_LCD_BACKPACK_COLS;
    r = pcf8574_lcd_backpack_register("/dev/slcd0",
                                      lisa_i2cbus_initialize(0), &cfg);
    if (r < 0)
      {
        syslog(LOG_ERR, "slcd: no backpack at 0x%02x (%d)\n", cfg.addr, r);
      }
    else
      {
        syslog(LOG_INFO, "slcd: %dx%d LCD at 0x%02x as /dev/slcd0\n",
               cfg.cols, cfg.rows, cfg.addr);
      }
  }
#endif

#ifdef CONFIG_ARTY_OLED
#  ifdef CONFIG_I2C_DRIVER
  i2c_register(lisa_i2cbus_initialize(0), 0);       /* /dev/i2c0: i2ctool */
#  endif
#  ifdef CONFIG_VIDEO_FB
  if (fb_register(0, 0) < 0)                        /* /dev/fb0 */
    {
      syslog(LOG_ERR, "oled: no framebuffer\n");
    }
#  else
  board_lcd_initialize();
#  endif
#endif
}
#endif
