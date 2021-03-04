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

#define ANGLE_SUB_FRAMES 2
#define SUB_FRAME_FRAC (FRACUNIT / ANGLE_SUB_FRAMES)

static angle_t dsda_GetRawViewAngle(player_t* player, fixed_t frac) {
  static const fixed_t sub_frame_frac = FRACUNIT / ANGLE_SUB_FRAMES;
  static angle_t sub_angle;
  static angle_t prev_sub_angle;
  static int last_frac_index;
  int frac_index;
  angle_t angleturn;
  fixed_t sub_frac;

  frac_index = frac / sub_frame_frac;
  if (frac == FRACUNIT)
    frac_index--;
  if (frac_index != last_frac_index) {
    last_frac_index = frac_index;

    angleturn = mouse_carry - mousex;
    angleturn = (angleturn + 128) & 0xff00;

    prev_sub_angle = sub_angle;
    sub_angle = player->mo->angle + (angleturn << 16);
  }

  sub_frac = (frac - frac_index * sub_frame_frac);
  sub_frac = FRACUNIT * sub_frac / sub_frame_frac;
  sub_frac = BETWEEN(0, FRACUNIT, sub_frac);

  lprintf(
    LO_INFO,
    "Raw angle drawing: f = %f, full: %f\n",
    (double)sub_frac / FRACUNIT,
    (double)frac / FRACUNIT
  );

  return prev_sub_angle + FixedMul(sub_frac, sub_angle - prev_sub_angle);
}

static angle_t dsda_GetInterpolatedViewAngle(player_t* player, fixed_t frac) {
  return player->prev_viewangle +
         FixedMul(frac, R_SmoothPlaying_Get(player) - player->prev_viewangle);
}

angle_t dsda_GetViewAngle(player_t* player, fixed_t frac) {
  if (dsda_raw_mouse)
    return dsda_GetRawViewAngle(player, frac);
  else
    if (movement_smooth)
      return dsda_GetInterpolatedViewAngle(player, frac);
    else
      return R_SmoothPlaying_Get(player);
}
