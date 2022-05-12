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
//	DSDA Console
//

#include "doomstat.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include "m_cheat.h"
#include "m_menu.h"
#include "v_video.h"

#include "dsda/brute_force.h"
#include "dsda/exhud.h"
#include "dsda/global.h"
#include "dsda/playback.h"
#include "dsda/tas.h"
#include "dsda/utility.h"

#include "console.h"

extern patchnum_t hu_font2[HU_FONTSIZE];

#define CONSOLE_ENTRY_SIZE 64

static char console_prompt[CONSOLE_ENTRY_SIZE + 3] = { '$', ' ' };
static char* console_entry = console_prompt + 2;
static int console_entry_index;
static hu_textline_t hu_console_prompt;

static void dsda_DrawConsole(void) {
  V_FillRect(0, 0, 0, SCREENWIDTH, 8 * SCREENHEIGHT / 200, 0);
  HUlib_drawTextLine(&hu_console_prompt, false);
}

menu_t dsda_ConsoleDef = {
  0,
  NULL,
  NULL,
  dsda_DrawConsole,
  0, 0
};

static void dsda_UpdateConsoleDisplay(void) {
  char* s = console_prompt;
  HUlib_clearTextLine(&hu_console_prompt);
  while (*s) HUlib_addCharToTextLine(&hu_console_prompt, *(s++));
  HUlib_addCharToTextLine(&hu_console_prompt, '_');
}

static void dsda_ResetConsoleEntry(void) {
  memset(console_entry, 0, CONSOLE_ENTRY_SIZE);
  console_entry_index = 0;
  dsda_UpdateConsoleDisplay();
}

dboolean dsda_OpenConsole(void) {
  static dboolean firsttime = true;

  if (gamestate != GS_LEVEL)
    return false;

  if (firsttime) {
    firsttime = false;

    HUlib_initTextLine(
      &hu_console_prompt,
      0,
      0,
      hu_font2,
      HU_FONTSTART,
      g_cr_gray,
      VPT_ALIGN_LEFT_TOP
    );
  }

  M_StartControlPanel();
  M_SetupNextMenu(&dsda_ConsoleDef);
  dsda_ResetConsoleEntry();

  return true;
}

static dboolean console_PlayerSetHealth(const char* command, const char* args) {
  int health;

  if (sscanf(args, "%i", &health)) {
    players[consoleplayer].mo->health = health;
    players[consoleplayer].health = health;

    return true;
  }

  return false;
}

static dboolean console_PlayerSetArmor(const char* command, const char* args) {
  int arg_count;
  int armorpoints, armortype;

  arg_count = sscanf(args, "%i %i", &armorpoints, &armortype);

  if (arg_count != 2 || (armortype != 1 && armortype != 2))
    armortype = players[consoleplayer].armortype;

  if (arg_count) {
    players[consoleplayer].armorpoints[ARMOR_ARMOR] = armorpoints;

    if (armortype == 0) armortype = 1;
    players[consoleplayer].armortype = armortype;

    return true;
  }

  return false;
}

static dboolean console_PlayerSetCoordinate(const char* args, int* dest) {
  int x, x_frac = 0;
  double x_double;

  if (sscanf(args, "%i.%i", &x, &x_frac)) {
    *dest = FRACUNIT * x;

    if (args[0] == '-')
      *dest -= x_frac;
    else
      *dest += x_frac;

    return true;
  }

  return false;
}

static dboolean console_PlayerSetX(const char* command, const char* args) {
  return console_PlayerSetCoordinate(args, &players[consoleplayer].mo->x);
}

static dboolean console_PlayerSetY(const char* command, const char* args) {
  return console_PlayerSetCoordinate(args, &players[consoleplayer].mo->y);
}

static dboolean console_PlayerSetZ(const char* command, const char* args) {
  return console_PlayerSetCoordinate(args, &players[consoleplayer].mo->z);
}

static void console_PlayerRoundCoordinate(int* x) {
  int bits = *x & 0xffff;
  if (!bits) return;

  if (*x > 0) {
    if (bits >= 0x8000)
      *x = (*x & ~0xffff) + FRACUNIT;
    else
      *x = *x & ~0xffff;
  }
  else {
    if (bits < 0x8000)
      *x = (*x & ~0xffff) - FRACUNIT;
    else
      *x = *x & ~0xffff;
  }
}

static dboolean console_PlayerRoundX(const char* command, const char* args) {
  console_PlayerRoundCoordinate(&players[consoleplayer].mo->x);

  return true;
}

static dboolean console_PlayerRoundY(const char* command, const char* args) {
  console_PlayerRoundCoordinate(&players[consoleplayer].mo->y);

  return true;
}

static dboolean console_PlayerRoundXY(const char* command, const char* args) {
  console_PlayerRoundCoordinate(&players[consoleplayer].mo->x);
  console_PlayerRoundCoordinate(&players[consoleplayer].mo->y);

  return true;
}

static dboolean console_CommandLock(const char* command, const char* args) {
  char element[CONSOLE_ENTRY_SIZE];
  int value;

  if (sscanf(args, "%s %i", element, &value) == 2)
    return dsda_UpdatePersistentCommand(element, value);

  return false;
}

static dboolean console_CommandUnlock(const char* command, const char* args) {
  dsda_DisablePersistentCommand();

  return true;
}

static dboolean console_TrackerAddLine(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_TrackLine(id);

  return false;
}

static dboolean console_TrackerRemoveLine(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_UntrackLine(id);

  return false;
}

static dboolean console_TrackerAddSector(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_TrackSector(id);

  return false;
}

static dboolean console_TrackerRemoveSector(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_UntrackSector(id);

  return false;
}

static dboolean console_TrackerAddMobj(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_TrackMobj(id);

  return false;
}

static dboolean console_TrackerRemoveMobj(const char* command, const char* args) {
  int id;

  if (sscanf(args, "%i", &id))
    return dsda_UntrackMobj(id);

  return false;
}

static dboolean console_TrackerAddPlayer(const char* command, const char* args) {
  return dsda_TrackPlayer(0);
}

static dboolean console_TrackerRemovePlayer(const char* command, const char* args) {
  return dsda_UntrackPlayer(0);
}

static dboolean console_JumpTic(const char* command, const char* args) {
  int tic;

  if (sscanf(args, "%i", &tic)) {
    if (tic < 0)
      tic = logictic + tic;

    dsda_JumpToLogicTic(tic);

    return true;
  }

  return false;
}

static dboolean console_BruteForceStart(const char* command, const char* args) {
  int depth;
  int forwardmove_min, forwardmove_max;
  int sidemove_min, sidemove_max;
  int angleturn_min, angleturn_max;
  char condition_args[CONSOLE_ENTRY_SIZE];
  int arg_count;

  dsda_ResetBruteForceConditions();

  arg_count = sscanf(
    args, "%i %i,%i %i,%i %i,%i %[^;]", &depth,
    &forwardmove_min, &forwardmove_max,
    &sidemove_min, &sidemove_max,
    &angleturn_min, &angleturn_max,
    condition_args
  );

  if (arg_count == 8) {
    int i;
    char** conditions;

    conditions = dsda_SplitString(condition_args, ",");

    if (!conditions)
      return false;

    for (i = 0; conditions[i]; ++i) {
      dsda_bf_attribute_t attribute;
      dsda_bf_operator_t operator;
      fixed_t value;
      char attr_s[4] = { 0 };
      char oper_s[5] = { 0 };

      if (sscanf(conditions[i], "%3s %4s %i", attr_s, oper_s, &value) == 3) {
        int attr_i, oper_i;

        for (attr_i = 0; attr_i < dsda_bf_attribute_max; ++attr_i)
          if (!strcmp(attr_s, dsda_bf_attribute_names[attr_i]))
            break;

        if (attr_i == dsda_bf_attribute_max)
          return false;

        for (oper_i = dsda_bf_limit_trio_zero; oper_i < dsda_bf_limit_trio_max; ++oper_i)
          if (!strcmp(oper_s, dsda_bf_limit_names[oper_i])) {
            dsda_SetBruteForceTarget(attr_i, oper_i, value);
            continue;
          }

        for (oper_i = 0; oper_i < dsda_bf_operator_max; ++oper_i)
          if (!strcmp(oper_s, dsda_bf_operator_names[oper_i]))
            break;

        if (oper_i == dsda_bf_operator_max)
          return false;

        dsda_AddBruteForceCondition(attr_i, oper_i, value);
      }
      else if (sscanf(conditions[i], "%3s %4s", attr_s, oper_s) == 2) {
        int attr_i, oper_i;

        for (attr_i = 0; attr_i < dsda_bf_attribute_max; ++attr_i)
          if (!strcmp(attr_s, dsda_bf_attribute_names[attr_i]))
            break;

        if (attr_i == dsda_bf_attribute_max)
          return false;

        for (oper_i = dsda_bf_limit_duo_zero; oper_i < dsda_bf_limit_duo_max; ++oper_i)
          if (!strcmp(oper_s, dsda_bf_limit_names[oper_i]))
            break;

        if (oper_i == dsda_bf_limit_duo_max)
          return false;

        dsda_SetBruteForceTarget(attr_i, oper_i, 0);
      }
    }

    free(conditions);

    dsda_StartBruteForce(depth,
                         forwardmove_min, forwardmove_max,
                         sidemove_min, sidemove_max,
                         angleturn_min, angleturn_max);

    return true;
  }

  return false;
}

static dboolean console_Exit(const char* command, const char* args) {
  extern void M_ClearMenus(void);

  M_ClearMenus();

  return true;
}

static dboolean console_BasicCheat(const char* command, const char* args) {
  return M_CheatEntered(command, args);
}

typedef dboolean (*console_command_t)(const char*, const char*);

typedef struct {
  const char* command_name;
  console_command_t command;
} console_command_entry_t;

static console_command_entry_t console_commands[] = {
  // commands
  { "player.sethealth", console_PlayerSetHealth },
  { "player.setarmor", console_PlayerSetArmor },
  { "player.setx", console_PlayerSetX },
  { "player.sety", console_PlayerSetY },
  { "player.setz", console_PlayerSetZ },
  { "player.roundx", console_PlayerRoundX },
  { "player.roundy", console_PlayerRoundY },
  { "player.roundxy", console_PlayerRoundXY },
  { "command.lock", console_CommandLock },
  { "command.unlock", console_CommandUnlock },

  // tracking
  { "tracker.addline", console_TrackerAddLine },
  { "t.al", console_TrackerAddLine },
  { "tracker.removeline", console_TrackerRemoveLine },
  { "t.rl", console_TrackerRemoveLine },
  { "tracker.addsector", console_TrackerAddSector },
  { "t.as", console_TrackerAddSector },
  { "tracker.removesector", console_TrackerRemoveSector },
  { "t.rs", console_TrackerRemoveSector },
  { "tracker.addmobj", console_TrackerAddMobj },
  { "t.am", console_TrackerAddMobj },
  { "tracker.removemobj", console_TrackerRemoveMobj },
  { "t.rm", console_TrackerRemoveMobj },
  { "tracker.addplayer", console_TrackerAddPlayer },
  { "t.ap", console_TrackerAddPlayer },
  { "tracker.removeplayer", console_TrackerRemovePlayer },
  { "t.rp", console_TrackerRemovePlayer },

  // traversing time
  { "jump.tic", console_JumpTic },

  // brute force
  { "bruteforce.start", console_BruteForceStart },
  { "bf.start", console_BruteForceStart },

  // cheats
  { "idchoppers", console_BasicCheat },
  { "iddqd", console_BasicCheat },
  { "idkfa", console_BasicCheat },
  { "idfa", console_BasicCheat },
  { "idspispopd", console_BasicCheat },
  { "idclip", console_BasicCheat },
  { "idmypos", console_BasicCheat },
  { "idrate", console_BasicCheat },
  { "iddt", console_BasicCheat },
  { "iddst", console_BasicCheat },
  { "iddkt", console_BasicCheat },
  { "iddit", console_BasicCheat },

  { "tntcomp", console_BasicCheat },
  { "tntem", console_BasicCheat },
  { "tnthom", console_BasicCheat },
  { "tntka", console_BasicCheat },
  { "tntsmart", console_BasicCheat },
  { "tntpitch", console_BasicCheat },
  { "tntfast", console_BasicCheat },
  { "tntice", console_BasicCheat },
  { "tntpush", console_BasicCheat },

  { "notarget", console_BasicCheat },
  { "fly", console_BasicCheat },

  { "quicken", console_BasicCheat },
  { "ponce", console_BasicCheat },
  { "kitty", console_BasicCheat },
  { "massacre", console_BasicCheat },
  { "rambo", console_BasicCheat },
  { "skel", console_BasicCheat },
  { "shazam", console_BasicCheat },
  { "ravmap", console_BasicCheat },
  { "cockadoodledoo", console_BasicCheat },

  { "satan", console_BasicCheat },
  { "clubmed", console_BasicCheat },
  { "butcher", console_BasicCheat },
  { "nra", console_BasicCheat },
  { "indiana", console_BasicCheat },
  { "locksmith", console_BasicCheat },
  { "sherlock", console_BasicCheat },
  { "casper", console_BasicCheat },
  { "init", console_BasicCheat },
  { "mapsco", console_BasicCheat },
  { "deliverance", console_BasicCheat },

  // exit
  { "exit", console_Exit },
  { "quit", console_Exit },
  { NULL }
};

static void dsda_ExecuteConsole(void) {
  char command[CONSOLE_ENTRY_SIZE];
  char args[CONSOLE_ENTRY_SIZE];
  int scan_count;

  scan_count = sscanf(console_entry, "%s %[^;]", command, args);

  if (scan_count) {
    console_command_entry_t* entry;

    if (scan_count == 1) args[0] = '\0';

    for (entry = console_commands; entry->command; entry++) {
      if (!stricmp(command, entry->command_name)) {
        entry->command(command, args);
        break;
      }
    }
  }

  dsda_ResetConsoleEntry();
}

void dsda_UpdateConsole(int ch, int action) {
  if (action == MENU_BACKSPACE && console_entry_index > 0) {
    --console_entry_index;
    console_entry[console_entry_index] = '\0';
    dsda_UpdateConsoleDisplay();
  }
  else if (action == MENU_ENTER) {
    dsda_ExecuteConsole();
  }
  else if (ch > 0) {
    ch = tolower(ch);
    if (
      (ch >= 'a' && ch <= 'z') ||
      (ch >= '0' && ch <= '9') ||
      (ch == ' ' || ch == '.' || ch == '-' || ch == ';' || ch == ',')
    ) {
      console_entry[console_entry_index] = ch;
      if (console_entry_index < CONSOLE_ENTRY_SIZE)
        ++console_entry_index;

      dsda_UpdateConsoleDisplay();
    }
  }
}
