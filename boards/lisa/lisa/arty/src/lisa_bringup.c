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

#include "lisa_internal.h"

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
}
#endif
