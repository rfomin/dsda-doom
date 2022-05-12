//
// Copyright(C) 2022 by Ryan Krafnick
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
//	DSDA Build Mode
//

#include "d_event.h"
#include "d_ticcmd.h"

angle_t dsda_BuildModeViewAngleOffset(void);
dboolean dsda_AllowBuilding(void);
dboolean dsda_BuildMode(void);
void dsda_CopyBuildCmd(ticcmd_t* cmd);
void dsda_ReadBuildCmd(ticcmd_t* cmd);
void dsda_EnterBuildMode(void);
dboolean dsda_BuildResponder(event_t *ev);
dboolean dsda_AdvanceFrame(void);
