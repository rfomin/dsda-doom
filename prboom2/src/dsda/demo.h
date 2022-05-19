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
//	DSDA Demo
//

#ifndef __DSDA_DEMO__
#define __DSDA_DEMO__

#include "d_ticcmd.h"

const char* dsda_DemoNameBase(void);
void dsda_SetDemoBaseName(const char* name);
void dsda_ExportDemo(const char* name);
void dsda_MarkCompatibilityLevelUnspecified(void);
int dsda_BytesPerTic(void);
void dsda_EvaluateBytesPerTic(void);
void dsda_RestoreCommandHistory(void);
void dsda_InitDemoRecording(void);
void dsda_WriteToDemo(void* buffer, size_t length);
void dsda_WriteTicToDemo(void* buffer, size_t length);
void dsda_WriteDemoToFile(void);
void dsda_CopyPendingCmd(ticcmd_t* cmd);
void dsda_JoinDemoCmd(ticcmd_t* cmd);
const byte* dsda_StripDemoVersion255(const byte* demo_p, const byte* header_p, size_t size);
void dsda_WriteDSDADemoHeader(byte** p);
void dsda_ApplyDSDADemoFormat(byte** demo_p);
void dsda_EndDemoRecording(void);
int dsda_DemoDataSize(byte complete);
void dsda_StoreDemoData(byte** save_p, byte complete);
void dsda_RestoreDemoData(byte** save_p, byte complete);
int dsda_DemoTicsCount(const byte* p, const byte* demobuffer, int demolength);
const byte* dsda_DemoMarkerPosition(byte* buffer, size_t file_size);

#endif
