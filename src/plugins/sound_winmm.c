/************************************************************************
 * sound_winmm.c  dopewars sound system (Windows MM driver)             *
 * Copyright (C)  1998-2022  Ben Webb                                   *
 *                Email: benwebb@users.sf.net                           *
 *                WWW: https://dopewars.sourceforge.io/                 *
 *                                                                      *
 * This program is free software; you can redistribute it and/or        *
 * modify it under the terms of the GNU General Public License          *
 * as published by the Free Software Foundation; either version 2       *
 * of the License, or (at your option) any later version.               *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program; if not, write to the Free Software          *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,               *
 *                   MA  02111-1307, USA.                               *
 ************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_WINMM
#include <windows.h>
#include <mmsystem.h>
#include <glib.h>
#include "../sound.h"

static gboolean SoundOpen_WinMM(void)
{
  return TRUE;
}

static void SoundClose_WinMM(void)
{
  sndPlaySound(NULL, 0);
}

static void SoundPlay_WinMM(const gchar *snd)
{
  sndPlaySound(snd, SND_ASYNC);
}

SoundDriver *sound_winmm_init(void)
{
  static SoundDriver driver;

  driver.name = "winmm";
  driver.open = SoundOpen_WinMM;
  driver.close = SoundClose_WinMM;
  driver.play = SoundPlay_WinMM;
  return &driver;
}

#endif /* HAVE_WINMM */
