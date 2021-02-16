//
// Copyright(C) 2020 by Ryan Krafnick
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	DSDA View Angle
//

#include "r_demo.h"

#include "view_angle.h"

int dsda_raw_mouse;
int dsda_raw_mouse_longtics = 1;

extern int movement_smooth;
extern signed short mouse_carry;
extern int mousex;
extern int leveltime;
extern int menuactive;

#include "lprintf.h"

static angle_t dsda_GetRawViewAngle(player_t* player) {
  angle_t angleturn;
  static angle_t angle_history[3];
  static int angle_index = 0;
  static int lasttime;

  angleturn = mouse_carry - mousex;

  if (!dsda_raw_mouse_longtics)
    angleturn = (angleturn + 128) & 0xff00;

  if (leveltime != lasttime)
  {
    lasttime = leveltime;
    angle_history[angle_index] = player->mo->angle;
    angle_index++;
    if (angle_index == 3) angle_index = 0;
  }

  if (angle_history[0] == angle_history[1] && angle_history[1] == angle_history[2])
    return player->mo->angle;

  return player->mo->angle + (angleturn << 16);
}

static angle_t dsda_GetInterpolatedViewAngle(player_t* player, fixed_t frac) {
  return player->prev_viewangle +
         FixedMul(frac, R_SmoothPlaying_Get(player) - player->prev_viewangle);
}

angle_t dsda_GetViewAngle(player_t* player, fixed_t frac) {
  if (dsda_raw_mouse)
    return dsda_GetRawViewAngle(player);
  else
    if (movement_smooth)
      return dsda_GetInterpolatedViewAngle(player, frac);
    else
      return R_SmoothPlaying_Get(player);
}
