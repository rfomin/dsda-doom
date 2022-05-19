/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2004 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:  none
 *  The original Doom description was none, basically because this file
 *  has everything. This ties up the game logic, linking the menu and
 *  input code to the underlying game by creating & respawning players,
 *  building game tics, calling the underlying thing logic.
 *
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doomstat.h"
#include "d_net.h"
#include "f_finale.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_cheat.h"
#include "m_random.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "p_map.h"
#include "d_main.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_draw.h"
#include "p_map.h"
#include "s_sound.h"
#include "s_advsound.h"
#include "dstrings.h"
#include "sounds.h"
#include "r_data.h"
#include "r_sky.h"
#include "d_deh.h"              // Ty 3/27/98 deh declarations
#include "p_inter.h"
#include "g_game.h"
#include "lprintf.h"
#include "i_main.h"
#include "i_system.h"
#include "r_demo.h"
#include "r_fps.h"
#include "e6y.h"//e6y

#include "dsda.h"
#include "dsda/brute_force.h"
#include "dsda/build.h"
#include "dsda/command_display.h"
#include "dsda/demo.h"
#include "dsda/excmd.h"
#include "dsda/key_frame.h"
#include "dsda/save.h"
#include "dsda/settings.h"
#include "dsda/input.h"
#include "dsda/map_format.h"
#include "dsda/mapinfo.h"
#include "dsda/mouse.h"
#include "dsda/options.h"
#include "dsda/pause.h"
#include "dsda/playback.h"
#include "dsda/skip.h"
#include "dsda/tas.h"
#include "dsda/time.h"
#include "dsda/split_tracker.h"
#include "statdump.h"

#define SAVEGAMESIZE  0x20000
#define SAVESTRINGSIZE  24

struct
{
    int type;   // mobjtype_t
    int speed[2];
} MonsterMissileInfo[] = {
    { HERETIC_MT_IMPBALL, { 10, 20 } },
    { HERETIC_MT_MUMMYFX1, { 9, 18 } },
    { HERETIC_MT_KNIGHTAXE, { 9, 18 } },
    { HERETIC_MT_REDAXE, { 9, 18 } },
    { HERETIC_MT_BEASTBALL, { 12, 20 } },
    { HERETIC_MT_WIZFX1, { 18, 24 } },
    { HERETIC_MT_SNAKEPRO_A, { 14, 20 } },
    { HERETIC_MT_SNAKEPRO_B, { 14, 20 } },
    { HERETIC_MT_HEADFX1, { 13, 20 } },
    { HERETIC_MT_HEADFX3, { 10, 18 } },
    { HERETIC_MT_MNTRFX1, { 20, 26 } },
    { HERETIC_MT_MNTRFX2, { 14, 20 } },
    { HERETIC_MT_SRCRFX1, { 20, 28 } },
    { HERETIC_MT_SOR2FX1, { 20, 28 } },
    { -1, { -1, -1 } }                 // Terminator
};

// e6y
// It is signature for new savegame format with continuous numbering.
// Now it is not necessary to add a new level of compatibility in case
// of need to savegame format change from one minor version to another.
// The old format is still supported.
#define NEWFORMATSIG "\xff\xff\xff\xff"

size_t          savegamesize = SAVEGAMESIZE; // killough
static dboolean  netdemo;
static const byte *demobuffer;   /* cph - only used for playback */
static int demolength; // check for overrun (missing DEMOMARKER)

gameaction_t    gameaction;
gamestate_t     gamestate;
skill_t         gameskill;
dboolean         respawnmonsters;
int             gameepisode;
int             gamemap;
// CPhipps - moved *_loadgame vars here
static dboolean forced_loadgame = false;
static dboolean load_via_cmd = false;

dboolean         timingdemo;    // if true, exit with report on completion
dboolean         fastdemo;      // if true, run at full speed -- killough
dboolean         nodrawers;     // for comparative timing purposes
dboolean         noblit;        // for comparative timing purposes
int             starttime;     // for comparative timing purposes
dboolean         deathmatch;    // only if started as net death
dboolean         netgame;       // only true if packets are broadcast
dboolean         playeringame[MAX_MAXPLAYERS];
player_t        players[MAX_MAXPLAYERS];
pclass_t        PlayerClass[MAX_MAXPLAYERS];
int             upmove;
int             consoleplayer; // player taking events and displaying
int             displayplayer; // view being displayed
int             gametic;
int             basetic;       /* killough 9/29/98: for demo sync */
int             totalkills, totallive, totalitems, totalsecret;    // for intermission
int             show_alive;
dboolean         demorecording;
dboolean         demoplayback;
dboolean         singledemo;           // quit after playing a demo from cmdline
wbstartstruct_t wminfo;               // parms for world map / intermission
dboolean         haswolflevels = false;// jff 4/18/98 wolf levels present
byte            *savebuffer;
int             totalleveltimes;      // CPhipps - total time for all completed levels
int             longtics;

dboolean coop_spawns;

// e6y
// There is a new command-line switch "-shorttics".
// This makes it possible to practice routes and tricks
// (e.g. glides, where this makes a significant difference)
// with the same mouse behaviour as when recording,
// but without having to be recording every time.
int shorttics;

// automatic pistol start when advancing from one level to the next
int pistolstart;

//
// controls (have defaults)
//

#define MAXPLMOVE   (forwardmove[1])
#define SLOWTURNTICS  6
#define QUICKREVERSE (short)32768 // 180 degree reverse                    // phares

fixed_t forwardmove[2] = {0x19, 0x32};
fixed_t sidemove[2]    = {0x18, 0x28};
fixed_t angleturn[3]   = {640, 1280, 320};  // + slow turn
fixed_t flyspeed[2]    = {1*256, 3*256};

static int     turnheld;       // for accelerative turning

// Set to -1 or +1 to switch to the previous or next weapon.

static int next_weapon = 0;

// Used for prev/next weapon keys.

static const struct
{
  weapontype_t weapon;
  weapontype_t weapon_num;
} weapon_order_table[] = {
  { wp_fist,         wp_fist },
  { wp_chainsaw,     wp_fist },
  { wp_pistol,       wp_pistol },
  { wp_shotgun,      wp_shotgun },
  { wp_supershotgun, wp_shotgun },
  { wp_chaingun,     wp_chaingun },
  { wp_missile,      wp_missile },
  { wp_plasma,       wp_plasma },
  { wp_bfg,          wp_bfg }
};

// HERETIC_TODO: dynamically set these
static const struct
{
    weapontype_t weapon;
    weapontype_t weapon_num;
} heretic_weapon_order_table[] = {
    { wp_staff,       wp_staff },
    { wp_gauntlets,   wp_staff },
    { wp_goldwand,    wp_goldwand },
    { wp_crossbow,    wp_crossbow },
    { wp_blaster,     wp_blaster },
    { wp_skullrod,    wp_skullrod },
    { wp_phoenixrod,  wp_phoenixrod },
    { wp_mace,        wp_mace },
    { wp_beak,        wp_beak },
};

// mouse values are used once
static int   mousex;
static int   mousey;
static int   dclicktime;
static int   dclickstate;
static int   dclicks;
static int   dclicktime2;
static int   dclickstate2;
static int   dclicks2;

// joystick values are repeated
static int   joyxmove;
static int   joyymove;

// Game events info
static buttoncode_t special_event; // Event triggered by local player, to send
static int   savegameslot;         // Slot to load if gameaction == ga_loadgame
char         savedescription[SAVEDESCLEN];  // Description to save in savegame if gameaction == ga_savegame

//jff 3/24/98 define defaultskill here
int defaultskill;               //note 1-based

// killough 2/8/98: make corpse queue variable in size
int    bodyqueslot, bodyquesize;        // killough 2/8/98
mobj_t **bodyque = 0;                   // phares 8/10/98

// heretic
#include "p_user.h"
#include "heretic/def.h"

int inventoryTics;
int lookheld;

static dboolean InventoryMoveLeft(void);
static dboolean InventoryMoveRight(void);
// end heretic

// hexen
#include "heretic/sb_bar.h"
#include "hexen/a_action.h"
#include "hexen/p_acs.h"
#include "hexen/sn_sonix.h"
#include "hexen/sv_save.h"

// Position indicator for cooperative net-play reborn
int RebornPosition;

int LeaveMap;
static int LeavePosition;

void G_DoTeleportNewMap(void);
static void Hexen_G_DoCompleted(void);
static void Hexen_G_DoReborn(int playernum);
// end hexen

typedef enum
{
  carry_vertmouse,
  carry_mousex,
  carry_mousey,
  NUMDOUBLECARRY
} double_carry_t;

static double double_carry[NUMDOUBLECARRY];

static int G_CarryDouble(double_carry_t c, double value)
{
  int truncated_result;
  double true_result;

  true_result = double_carry[c] + value;
  truncated_result = (int) true_result;
  double_carry[c] = true_result - truncated_result;

  return truncated_result;
}

static void G_DoSaveGame(dboolean via_cmd);

//
// G_BuildTiccmd
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
static inline signed char fudgef(signed char b)
{
/*e6y
  static int c;
  if (!b || !demo_compatibility || longtics) return b;
  if (++c & 0x1f) return b;
  b |= 1; if (b>2) b-=2;*/
  return b;
}

void G_SetSpeed(dboolean reset)
{
  int p;
  dsda_pclass_t *player_class;
  static dsda_pclass_t *last_player_class = NULL;

  player_class = &pclass[players[consoleplayer].pclass];

  if (last_player_class == player_class && !reset)
    return;

  last_player_class = player_class;

  forwardmove[0] = player_class->forwardmove[0];
  forwardmove[1] = player_class->forwardmove[1];

  if(dsda_AlwaysSR50())
  {
    sidemove[0] = player_class->forwardmove[0];
    sidemove[1] = player_class->forwardmove[1];
  }
  else
  {
    sidemove[0] = player_class->sidemove[0];
    sidemove[1] = player_class->sidemove[1];
  }

  if ((p = M_CheckParm ("-turbo")))
  {
    int scale = ((p < myargc - 1) ? atoi(myargv[p + 1]) : 200);
    scale = BETWEEN(10, 400, scale);

    //jff 9/3/98 use logical output routine
    lprintf (LO_INFO,"turbo scale: %i%%\n",scale);
    forwardmove[0] = player_class->forwardmove[0]*scale/100;
    forwardmove[1] = player_class->forwardmove[1]*scale/100;
    sidemove[0] = sidemove[0]*scale/100;
    sidemove[1] = sidemove[1]*scale/100;
  }
}

static dboolean WeaponSelectable(weapontype_t weapon)
{
  if (heretic)
  {
    return weapon != wp_beak && players[consoleplayer].weaponowned[weapon];
  }
  else if (hexen)
  {
    return weapon < HEXEN_NUMWEAPONS && players[consoleplayer].weaponowned[weapon];
  }

  if (gamemode == shareware)
  {
    if (weapon == wp_plasma || weapon == wp_bfg)
      return false;
  }

  // Can't select the super shotgun in Doom 1.
  if (weapon == wp_supershotgun && gamemission == doom)
  {
    return false;
  }

  // Can't select a weapon if we don't own it.
  if (!players[consoleplayer].weaponowned[weapon])
  {
    return false;
  }

  return true;
}

static int G_NextWeapon(int direction)
{
  weapontype_t weapon;
  int start_i, i, arrlen;

  // Find index in the table.
  if (players[consoleplayer].pendingweapon == wp_nochange)
  {
    weapon = players[consoleplayer].readyweapon;
  }
  else
  {
    weapon = players[consoleplayer].pendingweapon;
  }

  arrlen = sizeof(weapon_order_table) / sizeof(*weapon_order_table);
  for (i = 0; i < arrlen; i++)
  {
    if (weapon_order_table[i].weapon == weapon)
    {
      break;
    }
  }

  // Switch weapon. Don't loop forever.
  start_i = i;
  do
  {
    i += direction;
    i = (i + arrlen) % arrlen;
  }
  while (i != start_i && !WeaponSelectable(weapon_order_table[i].weapon));

  return weapon_order_table[i].weapon_num;
}

void G_BuildTiccmd(ticcmd_t* cmd)
{
  int strafe;
  int bstrafe;
  int speed;
  int tspeed;
  int forward;
  int side;
  int newweapon;                                          // phares

  dsda_pclass_t *player_class = &pclass[players[consoleplayer].pclass];

  G_SetSpeed(false);
  dsda_EvaluateSkipModeBuildTiccmd();

  memset(cmd, 0, sizeof(*cmd));

  if (demoplayback && demorecording)
  {
    mousex = mousey = 0;
    return;
  }

  strafe = dsda_InputActive(dsda_input_strafe);
  //e6y: the "RUN" key inverts the autorun state
  speed = (dsda_InputActive(dsda_input_speed) ? !dsda_AutoRun() : dsda_AutoRun()); // phares

  forward = side = 0;

    // use two stage accelerative turning
    // on the keyboard and joystick
  if (joyxmove != 0 ||
      dsda_InputActive(dsda_input_turnright) ||
      dsda_InputActive(dsda_input_turnleft))
    ++turnheld;
  else
    turnheld = 0;

  if (turnheld < SLOWTURNTICS)
    tspeed = 2;             // slow turn
  else
    tspeed = speed;

  // turn 180 degrees in one keystroke?                           // phares
                                                                  //    |
  if (dsda_InputTickActivated(dsda_input_reverse))                    //    V
    {
      if (!strafe)
        cmd->angleturn += QUICKREVERSE;                           //    ^
    }                                                             // phares

  // let movement keys cancel each other out

  if (strafe)
  {
    if (dsda_InputActive(dsda_input_turnright))
      side += sidemove[speed];
    if (dsda_InputActive(dsda_input_turnleft))
      side -= sidemove[speed];
    if (joyxmove > 0)
      side += sidemove[speed];
    if (joyxmove < 0)
      side -= sidemove[speed];
  }
  else
  {
    if (dsda_InputActive(dsda_input_turnright))
      cmd->angleturn -= angleturn[tspeed];
    if (dsda_InputActive(dsda_input_turnleft))
      cmd->angleturn += angleturn[tspeed];
    if (joyxmove > 0)
      cmd->angleturn -= angleturn[tspeed];
    if (joyxmove < 0)
      cmd->angleturn += angleturn[tspeed];
  }

  if (dsda_InputActive(dsda_input_forward))
    forward += forwardmove[speed];
  if (dsda_InputActive(dsda_input_backward))
    forward -= forwardmove[speed];
  if (joyymove < 0)
    forward += forwardmove[speed];
  if (joyymove > 0)
    forward -= forwardmove[speed];
  if (dsda_InputActive(dsda_input_straferight))
    side += sidemove[speed];
  if (dsda_InputActive(dsda_input_strafeleft))
    side -= sidemove[speed];

  if (raven)
  {
    int lspeed;
    int look, arti;
    int flyheight;

    look = arti = flyheight = 0;

    if (dsda_InputActive(dsda_input_lookdown) || dsda_InputActive(dsda_input_lookup))
    {
      ++lookheld;
    }
    else
    {
      lookheld = 0;
    }
    if (lookheld < SLOWTURNTICS)
    {
      lspeed = 1;
    }
    else
    {
      lspeed = 2;
    }

    // Look up/down/center keys
    if (dsda_InputActive(dsda_input_lookup))
    {
      look = lspeed;
    }
    if (dsda_InputActive(dsda_input_lookdown))
    {
      look = -lspeed;
    }

    if (dsda_InputActive(dsda_input_lookcenter))
    {
      look = TOCENTER;
    }

    // Fly up/down/drop keys
    if (dsda_InputActive(dsda_input_flyup))
    {
      flyheight = 5;          // note that the actual flyheight will be twice this
    }
    if (dsda_InputActive(dsda_input_flydown))
    {
      flyheight = -5;
    }
    if (dsda_InputActive(dsda_input_flycenter))
    {
      flyheight = TOCENTER;
      look = TOCENTER;
    }

    // Use artifact key
    if (dsda_InputTickActivated(dsda_input_use_artifact))
    {
      if (inventory)
      {
        players[consoleplayer].readyArtifact = players[consoleplayer].inventory[inv_ptr].type;
        inventory = false;
        cmd->arti = 0;
      }
      else
      {
        cmd->arti |= players[consoleplayer].inventory[inv_ptr].type & AFLAG_MASK;
      }
    }

    if (hexen)
    {
      extern int mn_SuicideConsole;

      if (dsda_InputActive(dsda_input_jump))
      {
        cmd->arti |= AFLAG_JUMP;
      }

      if (mn_SuicideConsole)
      {
          cmd->arti |= AFLAG_SUICIDE;
          mn_SuicideConsole = false;
      }

      if (!cmd->arti)
      {
        if (dsda_InputTickActivated(dsda_input_arti_ring))
        {
          cmd->arti = hexen_arti_invulnerability;
        }
        else if (dsda_InputTickActivated(dsda_input_arti_quartz) &&
                 players[consoleplayer].mo->health < MAXHEALTH)
        {
          cmd->arti = hexen_arti_health;
        }
        else if (dsda_InputTickActivated(dsda_input_arti_urn))
        {
          cmd->arti = hexen_arti_superhealth;
        }
        else if (dsda_InputTickActivated(dsda_input_hexen_arti_incant))
        {
          cmd->arti = hexen_arti_healingradius;
        }
        else if (dsda_InputTickActivated(dsda_input_hexen_arti_summon))
        {
          cmd->arti = hexen_arti_summon;
        }
        else if (dsda_InputTickActivated(dsda_input_arti_torch))
        {
          cmd->arti = hexen_arti_torch;
        }
        else if (dsda_InputTickActivated(dsda_input_arti_morph))
        {
          cmd->arti = hexen_arti_egg;
        }
        else if (dsda_InputTickActivated(dsda_input_arti_wings))
        {
          cmd->arti = hexen_arti_fly;
        }
        else if (dsda_InputTickActivated(dsda_input_hexen_arti_disk))
        {
          cmd->arti = hexen_arti_blastradius;
        }
        else if (dsda_InputTickActivated(dsda_input_hexen_arti_flechette))
        {
          cmd->arti = hexen_arti_poisonbag;
        }
        else if (dsda_InputTickActivated(dsda_input_hexen_arti_banishment))
        {
          cmd->arti = hexen_arti_teleportother;
        }
        else if (dsda_InputTickActivated(dsda_input_hexen_arti_boots))
        {
          cmd->arti = hexen_arti_speed;
        }
        else if (dsda_InputTickActivated(dsda_input_hexen_arti_krater))
        {
          cmd->arti = hexen_arti_boostmana;
        }
        else if (dsda_InputTickActivated(dsda_input_hexen_arti_bracers))
        {
          cmd->arti = hexen_arti_boostarmor;
        }
        else if (dsda_InputTickActivated(dsda_input_arti_chaosdevice))
        {
          cmd->arti = hexen_arti_teleport;
        }
      }
    }
    else
    {
      if (dsda_InputTickActivated(dsda_input_arti_tome) && !cmd->arti
          && !players[consoleplayer].powers[pw_weaponlevel2])
      {
        cmd->arti = arti_tomeofpower;
      }
      else if (dsda_InputTickActivated(dsda_input_arti_quartz) && !cmd->arti
          && (players[consoleplayer].mo->health < MAXHEALTH))
      {
        cmd->arti = arti_health;
      }
      else if (dsda_InputTickActivated(dsda_input_arti_urn) && !cmd->arti)
      {
        cmd->arti = arti_superhealth;
      }
      else if (dsda_InputTickActivated(dsda_input_arti_bomb) && !cmd->arti)
      {
        cmd->arti = arti_firebomb;
      }
      else if (dsda_InputTickActivated(dsda_input_arti_ring) && !cmd->arti)
      {
        cmd->arti = arti_invulnerability;
      }
      else if (dsda_InputTickActivated(dsda_input_arti_chaosdevice) && !cmd->arti)
      {
        cmd->arti = arti_teleport;
      }
      else if (dsda_InputTickActivated(dsda_input_arti_shadowsphere) && !cmd->arti)
      {
        cmd->arti = arti_invisibility;
      }
      else if (dsda_InputTickActivated(dsda_input_arti_wings) && !cmd->arti)
      {
        cmd->arti = arti_fly;
      }
      else if (dsda_InputTickActivated(dsda_input_arti_torch) && !cmd->arti)
      {
        cmd->arti = arti_torch;
      }
      else if (dsda_InputTickActivated(dsda_input_arti_morph) && !cmd->arti)
      {
        cmd->arti = arti_egg;
      }
    }

    if (players[consoleplayer].playerstate == PST_LIVE)
    {
        if (look < 0)
        {
            look += 16;
        }
        cmd->lookfly = look;
    }
    if (flyheight < 0)
    {
        flyheight += 16;
    }
    cmd->lookfly |= flyheight << 4;
  }

  if (dsda_AllowJumping())
  {
    if (!hexen && dsda_InputActive(dsda_input_jump))
    {
      dsda_QueueExCmdJump();
    }
  }

  // buttons
  cmd->chatchar = HU_dequeueChatChar();

  if (dsda_InputActive(dsda_input_fire))
    cmd->buttons |= BT_ATTACK;

  if (dsda_InputActive(dsda_input_use) || dsda_InputTickActivated(dsda_input_use))
  {
    cmd->buttons |= BT_USE;
    // clear double clicks if hit use button
    dclicks = 0;
  }

  {
    extern dboolean boom_weapon_state_injection;
    static dboolean done_autoswitch = false;

    if (!players[consoleplayer].attackdown)
    {
      done_autoswitch = false;
    }

    // Toggle between the top 2 favorite weapons.                   // phares
    // If not currently aiming one of these, switch to              // phares
    // the favorite. Only switch if you possess the weapon.         // phares

    // killough 3/22/98:
    //
    // Perform automatic weapons switch here rather than in p_pspr.c,
    // except in demo_compatibility mode.
    //
    // killough 3/26/98, 4/2/98: fix autoswitch when no weapons are left

    // Make Boom insert only a single weapon change command on autoswitch.
    if (
      (
        !demo_compatibility &&
        players[consoleplayer].attackdown && // killough
        !P_CheckAmmo(&players[consoleplayer]) &&
        (
          (
            (
              dsda_SwitchWhenAmmoRunsOut() ||
              boom_weapon_state_injection
            ) &&
            !done_autoswitch
          ) || cmd->buttons & BT_ATTACK
        )
      ) || (!hexen && dsda_InputActive(dsda_input_toggleweapon))
    )
    {
      done_autoswitch = true;
      boom_weapon_state_injection = false;
      newweapon = P_SwitchWeapon(&players[consoleplayer]);           // phares
    }
    else
    {                                 // phares 02/26/98: Added gamemode checks
      if (next_weapon && players[consoleplayer].morphTics == 0)
      {
        newweapon = G_NextWeapon(next_weapon);
      }
      else if (hexen)
      {
        newweapon =
          dsda_InputActive(dsda_input_weapon1) ? wp_first :    // killough 5/2/98: reformatted
          dsda_InputActive(dsda_input_weapon2) ? wp_second :
          dsda_InputActive(dsda_input_weapon3) ? wp_third :
          dsda_InputActive(dsda_input_weapon4) ? wp_fourth :
          wp_nochange;
      }
      else
      {
        // HERETIC_TODO: fix this
        newweapon =
          dsda_InputActive(dsda_input_weapon1) ? wp_fist :    // killough 5/2/98: reformatted
          dsda_InputActive(dsda_input_weapon2) ? wp_pistol :
          dsda_InputActive(dsda_input_weapon3) ? wp_shotgun :
          dsda_InputActive(dsda_input_weapon4) ? wp_chaingun :
          dsda_InputActive(dsda_input_weapon5) ? wp_missile :
          dsda_InputActive(dsda_input_weapon6) && gamemode != shareware ? wp_plasma :
          dsda_InputActive(dsda_input_weapon7) && gamemode != shareware ? wp_bfg :
          dsda_InputActive(dsda_input_weapon8) ? wp_chainsaw :
          (!demo_compatibility && dsda_InputActive(dsda_input_weapon9) && gamemode == commercial) ? wp_supershotgun :
          wp_nochange;
      }

      // killough 3/22/98: For network and demo consistency with the
      // new weapons preferences, we must do the weapons switches here
      // instead of in p_user.c. But for old demos we must do it in
      // p_user.c according to the old rules. Therefore demo_compatibility
      // determines where the weapons switch is made.

      // killough 2/8/98:
      // Allow user to switch to fist even if they have chainsaw.
      // Switch to fist or chainsaw based on preferences.
      // Switch to shotgun or SSG based on preferences.

      if (!demo_compatibility)
      {
        const player_t *player = &players[consoleplayer];

        // only select chainsaw from '1' if it's owned, it's
        // not already in use, and the player prefers it or
        // the fist is already in use, or the player does not
        // have the berserker strength.

        if (newweapon==wp_fist && player->weaponowned[wp_chainsaw] &&
            player->readyweapon!=wp_chainsaw &&
            (player->readyweapon==wp_fist ||
             !player->powers[pw_strength] ||
             P_WeaponPreferred(wp_chainsaw, wp_fist)))
          newweapon = wp_chainsaw;

        // Select SSG from '3' only if it's owned and the player
        // does not have a shotgun, or if the shotgun is already
        // in use, or if the SSG is not already in use and the
        // player prefers it.

        if (newweapon == wp_shotgun && gamemode == commercial &&
            player->weaponowned[wp_supershotgun] &&
            (!player->weaponowned[wp_shotgun] ||
             player->readyweapon == wp_shotgun ||
             (player->readyweapon != wp_supershotgun &&
              P_WeaponPreferred(wp_supershotgun, wp_shotgun))))
          newweapon = wp_supershotgun;
      }
    }
  }

  next_weapon = 0;

  if (newweapon != wp_nochange && players[consoleplayer].chickenTics == 0)
  {
    cmd->buttons |= BT_CHANGE;
    cmd->buttons |= newweapon<<BT_WEAPONSHIFT;
  }

  // mouse

  if (mouse_doubleclick_as_use) {//e6y
    // forward double click
    if (dsda_InputMouseBActive(dsda_input_forward) != dclickstate && dclicktime > 1 )
    {
      dclickstate = dsda_InputMouseBActive(dsda_input_forward);
      if (dclickstate)
        dclicks++;
      if (dclicks == 2)
        {
          cmd->buttons |= BT_USE;
          dclicks = 0;
        }
      else
        dclicktime = 0;
    }
    else
      if (++dclicktime > 20)
      {
        dclicks = 0;
        dclickstate = 0;
      }

    // strafe double click
    bstrafe = dsda_InputMouseBActive(dsda_input_strafe) || dsda_InputJoyBActive(dsda_input_strafe);
    if (bstrafe != dclickstate2 && dclicktime2 > 1 )
    {
      dclickstate2 = bstrafe;
      if (dclickstate2)
        dclicks2++;
      if (dclicks2 == 2)
        {
          cmd->buttons |= BT_USE;
          dclicks2 = 0;
        }
      else
        dclicktime2 = 0;
    }
    else
      if (++dclicktime2 > 20)
      {
        dclicks2 = 0;
        dclickstate2 = 0;
      }
  }

  if (!dsda_NoVert())
  {
    forward += mousey;
  }

  if (strafe)
  {
    static double mousestrafe_carry = 0;
    int delta;
    double true_delta;

    true_delta = mousestrafe_carry +
                 (double) mousex / movement_mousestrafedivisor;

    delta = (int) true_delta;
    delta = (delta / 2) * 2;
    mousestrafe_carry = true_delta - delta;

    side += delta;
    side = (side / 2) * 2; // only even values are possible
  }
  else
    cmd->angleturn -= mousex; /* mead now have enough dynamic range 2-10-00 */

  if (!walkcamera.type || menuactive) //e6y
    mousex = mousey = 0;
#ifdef GL_DOOM
  motion_blur.curr_speed_pow2 = 0;
#endif

  if (forward > MAXPLMOVE)
    forward = MAXPLMOVE;
  else if (forward < -MAXPLMOVE)
    forward = -MAXPLMOVE;

  if (side > MAXPLMOVE)
    side = MAXPLMOVE;
  else if (side < -MAXPLMOVE)
    side = -MAXPLMOVE;

  //e6y
  if (dsda_AlwaysSR50())
  {
    if (!speed)
    {
      if (side > player_class->forwardmove[0])
        side = player_class->forwardmove[0];
      else if (side < -player_class->forwardmove[0])
        side = -player_class->forwardmove[0];
    }
    else if(!movement_strafe50onturns && !strafe && cmd->angleturn)
    {
      if (side > player_class->sidemove[1])
        side = player_class->sidemove[1];
      else if (side < -player_class->sidemove[1])
        side = -player_class->sidemove[1];
    }
  }

  if (players[consoleplayer].powers[pw_speed] && !players[consoleplayer].morphTics)
  {                           // Adjust for a player with a speed artifact
      forward = (3 * forward) >> 1;
      side = (3 * side) >> 1;
  }

  if (stroller) side = 0;

  cmd->forwardmove += fudgef((signed char)forward);
  cmd->sidemove += side;

  dsda_ApplyQuickstartMouseCache(cmd);

  if ((demorecording && !longtics) || shorttics)
  {
    // Chocolate Doom Mouse Behaviour
    // Don't discard mouse delta even if value is too small to
    // turn the player this tic
    if (mouse_carrytics) {
      static signed short carry = 0;
      signed short desired_angleturn = cmd->angleturn + carry;
      cmd->angleturn = (desired_angleturn + 128) & 0xff00;
      carry = desired_angleturn - cmd->angleturn;
    }

    cmd->angleturn = (((cmd->angleturn + 128) >> 8) << 8);
  }

  upmove = 0;
  if (dsda_InputActive(dsda_input_flyup))
    upmove += flyspeed[speed];
  if (dsda_InputActive(dsda_input_flydown))
    upmove -= flyspeed[speed];

  // CPhipps - special events (game new/load/save/pause)
  if (special_event & BT_SPECIAL) {
    cmd->buttons = special_event;
    special_event = 0;
  }

  dsda_PopExCmdQueue(cmd);

  if (!dsda_StrictMode()) {
    if (leveltime == 0 && totalleveltimes == 0) {
      int p = M_CheckParm("-first_input");

      if (p && (p + 3 < myargc)) {
        cmd->forwardmove = (signed char) atoi(myargv[p + 1]);
        cmd->sidemove = (signed char) atoi(myargv[p + 2]);
        cmd->angleturn = (signed short) (atoi(myargv[p + 3]) << 8);

        dsda_JoinDemoCmd(cmd);
      }
    }
    else
      dsda_ApplyTasCommand(cmd);
  }
}

//
// G_DoLoadLevel
//

static void G_DoLoadLevel (void)
{
  int i;

  // Set the sky map.
  // First thing, we have a dummy sky texture name,
  //  a flat. The data is in the WAD only because
  //  we look for an actual index, instead of simply
  //  setting one.

  skyflatnum = R_FlatNumForName(g_skyflatname);
  skytexture = dsda_SkyTexture();

  // [RH] Set up details about sky rendering
  R_InitSkyMap ();

#ifdef GL_DOOM
  R_SetBoxSkybox(skytexture);
#endif

  levelstarttic = gametic;        // for time calculation

  if (!demo_compatibility && !mbf_features)   // killough 9/29/98
    basetic = gametic;

  if (wipegamestate == GS_LEVEL)
    wipegamestate = -1;             // force a wipe

  gamestate = GS_LEVEL;

  for (i = 0; i < g_maxplayers; i++)
  {
    if (playeringame[i] && players[i].playerstate == PST_DEAD)
      players[i].playerstate = PST_REBORN;
    memset(players[i].frags, 0, sizeof(players[i].frags));
  }

  // automatic pistol start when advancing from one level to the next
  if (pistolstart)
  {
    if (singleplayer)
    {
      G_PlayerReborn(0);
    }
    else if ((demoplayback || netdemo) && !singledemo)
    {
      // no-op - silently ignore pistolstart when playing demo from the
      // demo reel
    }
    else
    {
      const char message[] = "The -pistolstart option is not supported"
                             " for demos and\n"
                             " network play.";
      demorecording = false;
      I_Error(message);
    }
  }

  // initialize the msecnode_t freelist.                     phares 3/25/98
  // any nodes in the freelist are gone by now, cleared
  // by Z_FreeTag() when the previous level ended or player
  // died.
  P_FreeSecNodeList();

  if (map_format.sndseq)
    SN_StopAllSequences();

  P_SetupLevel (gameepisode, gamemap, 0, gameskill);
  if (!demoplayback) // Don't switch views if playing a demo
    displayplayer = consoleplayer;    // view the guy you are playing
  gameaction = ga_nothing;

  // clear cmd building stuff
  dsda_InputFlush();
  joyxmove = joyymove = 0;
  mousex = mousey = 0;
  mlooky = 0;//e6y
  special_event = 0;
  dsda_ResetPauseMode();
  dsda_ResetExCmdQueue();

  if (dsda_BuildMode() || M_CheckParm("-build"))
    dsda_EnterBuildMode();

  // killough 5/13/98: in case netdemo has consoleplayer other than green
  ST_Start();
  HU_Start();

  // killough: make -timedemo work on multilevel demos
  // Move to end of function to minimize noise -- killough 2/22/98:

  if (timingdemo)
  {
    static int first=1;
    if (first)
      {
        starttime = dsda_GetTickRealTime();
        first=0;
      }
  }
}


//
// G_Responder
// Get info needed to make ticcmd_ts for the players.
//

dboolean G_Responder (event_t* ev)
{
  if (
    gamestate == GS_LEVEL && (
      HU_Responder(ev) ||
      ST_Responder(ev) ||
      AM_Responder(ev)
    )
  ) return true;

  // allow spy mode changes even during the demo
  // killough 2/22/98: even during DM demo
  //
  // killough 11/98: don't autorepeat spy mode switch
  if (dsda_InputActivated(dsda_input_spy) &&
      netgame && (demoplayback || !deathmatch) &&
      gamestate == GS_LEVEL)
  {
    do                                          // spy mode
      if (++displayplayer >= g_maxplayers)
        displayplayer = 0;
    while (!playeringame[displayplayer] && displayplayer!=consoleplayer);

    ST_Start();    // killough 3/7/98: switch status bar views too
    HU_Start();
    S_UpdateSounds(players[displayplayer].mo);
    R_ActivateSectorInterpolations();
    R_SmoothPlaying_Reset(NULL);
    return true;
  }

  // any other key pops up menu if in demos
  //
  // killough 8/2/98: enable automap in -timedemo demos
  //
  // killough 9/29/98: make any key pop up menu regardless of
  // which kind of demo, and allow other events during playback

  if (gameaction == ga_nothing && (demoplayback || gamestate == GS_DEMOSCREEN))
  {
    // killough 9/29/98: allow user to pause demos during playback
    if (dsda_InputActivated(dsda_input_pause))
    {
      dsda_TogglePauseMode(PAUSE_PLAYBACK);
      if (dsda_Paused())
        S_PauseSound();
      else
        S_ResumeSound();
      return true;
    }
  }

  if (gamestate == GS_FINALE && F_Responder(ev))
    return true;  // finale ate the event

  // If the next/previous weapon keys are pressed, set the next_weapon
  // variable to change weapons when the next ticcmd is generated.
  if (dsda_InputActivated(dsda_input_prevweapon))
  {
    next_weapon = -1;
  }
  else if (dsda_InputActivated(dsda_input_nextweapon))
  {
    next_weapon = 1;
  }

  if (dsda_InputActivated(dsda_input_invleft))
  {
    return InventoryMoveLeft();
  }
  if (dsda_InputActivated(dsda_input_invright))
  {
    return InventoryMoveRight();
  }

  if (dsda_BuildResponder(ev))
    return true;

  if (dsda_InputActivated(dsda_input_pause))
  {
    special_event = BT_SPECIAL | (BT_PAUSE & BT_SPECIALMASK);
    return true;
  }

  // Events that make it here should reach into the game logic
  dsda_InputTrackGameEvent(ev);

  switch (ev->type)
  {
    case ev_keydown:
      return true;    // eat key down events

    case ev_mousemotion:
    {
      double value;

      value = dsda_FineSensitivity(mouseSensitivity_horiz) * AccelerateMouse(ev->data2);
      mousex += G_CarryDouble(carry_mousex, value);
      if (dsda_MouseLook())
      {
        value = (double) mouseSensitivity_mlook * AccelerateMouse(ev->data3);
        if (movement_mouseinvert)
          mlooky += G_CarryDouble(carry_mousey, value);
        else
          mlooky -= G_CarryDouble(carry_mousey, value);
      }
      else
      {
        value = (double) mouseSensitivity_vert * AccelerateMouse(ev->data3) / 8;
        mousey += G_CarryDouble(carry_vertmouse, value);
      }

      return true;    // eat events
    }

    case ev_joystick:
      joyxmove = ev->data2;
      joyymove = ev->data3;
      return true;    // eat events

    default:
      break;
  }
  return false;
}

//
// G_Ticker
// Make ticcmd_ts for the players.
//

void G_Ticker (void)
{
  int i;
  int pause_mask;
  dboolean advance_frame = false;
  static gamestate_t prevgamestate;

  // CPhipps - player colour changing
  if (!demoplayback && mapcolor_plyr[consoleplayer] != mapcolor_me) {
    // Changed my multiplayer colour - Inform the whole game
    int net_cl = LittleLong(mapcolor_me);
    G_ChangedPlayerColour(consoleplayer, mapcolor_me);
  }
  P_MapStart();
  // do player reborns if needed
  for (i = 0; i < g_maxplayers; i++)
    if (playeringame[i] && players[i].playerstate == PST_REBORN)
      G_DoReborn(i);
  P_MapEnd();

  // do things to change the game state
  while (gameaction != ga_nothing)
  {
    switch (gameaction)
    {
      case ga_loadlevel:
        // force players to be initialized on level reload
        if (!hexen)
          for (i = 0; i < g_maxplayers; i++)
            players[i].playerstate = PST_REBORN;
        G_DoLoadLevel();
        break;
      case ga_newgame:
        G_DoNewGame();
        break;
      case ga_loadgame:
        G_DoLoadGame();
        break;
      case ga_playdemo:
        G_DoPlayDemo();
        break;
      case ga_completed:
        G_DoCompleted();
        break;
      case ga_victory:
        F_StartFinale();
        break;
      case ga_worlddone:
        G_DoWorldDone();
        break;
      case ga_screenshot:
        M_ScreenShot();
        gameaction = ga_nothing;
        break;
      case ga_leavemap:
        G_DoTeleportNewMap();
        break;
      case ga_nothing:
        break;
    }
  }

  dsda_EvaluateSkipModeGTicker();

  if (dsda_BruteForce())
    dsda_EvaluateBruteForce();

  if (dsda_AdvanceFrame())
  {
    advance_frame = true;
    pause_mask = dsda_MaskPause();
  }

  if (dsda_PausedOutsideDemo())
    basetic++;  // For revenant tracers and RNG -- we must maintain sync
  else {
    int buf = gametic % BACKUPTICS;

    dsda_UpdateAutoKeyFrames();

    if (dsda_BruteForce())
    {
      dsda_UpdateBruteForce();
      dsda_RemovePauseMode(PAUSE_BUILDMODE);
    }

    for (i = 0; i < g_maxplayers; i++)
    {
      if (playeringame[i])
      {
        ticcmd_t *cmd = &players[i].cmd;

        memcpy(cmd, &local_cmds[i][buf], sizeof *cmd);

        if (dsda_BuildMode())
          dsda_ReadBuildCmd(cmd);

        if (dsda_KeyFrameRestored())
          dsda_JoinDemoCmd(cmd);

        if (demoplayback)
          dsda_TryPlaybackOneTick(cmd);

        if (demorecording)
          G_WriteDemoTiccmd(cmd);
      }
    }

    dsda_InputFlushTick();
    dsda_WatchCommand();

    // check for special buttons
    for (i = 0; i < g_maxplayers; i++)
    {
      if (playeringame[i])
      {
        if (players[i].cmd.buttons & BT_SPECIAL)
        {
          switch (players[i].cmd.buttons & BT_SPECIALMASK)
          {
            case BT_PAUSE:
              dsda_TogglePauseMode(PAUSE_COMMAND);
              if (dsda_Paused())
                S_PauseSound();
              else
                S_ResumeSound();
              break;
          }
          if (!raven) players[i].cmd.buttons = 0;
        }

        if (dsda_AllowExCmd())
        {
          excmd_t *ex = &players[i].cmd.ex;

          if (ex->actions & XC_SAVE)
          {
            savegameslot = ex->save_slot;
            G_DoSaveGame(true);
          }

          if (ex->actions & XC_LOAD)
          {
            savegameslot = ex->load_slot;
            gameaction = ga_loadgame;
            forced_loadgame = true;
            load_via_cmd = true;
            R_SmoothPlaying_Reset(NULL);
          }

          if (ex->actions & XC_GOD)
          {
            M_CheatGod();
          }

          if (ex->actions & XC_NOCLIP)
          {
            M_CheatNoClip();
          }
        }
      }
    }

    // turn inventory off after a certain amount of time
    if (inventory && !(--inventoryTics))
    {
        players[consoleplayer].readyArtifact =
            players[consoleplayer].inventory[inv_ptr].type;
        inventory = false;
    }

    dsda_DisplayNotifications();
  }

  // cph - if the gamestate changed, we may need to clean up the old gamestate
  if (gamestate != prevgamestate) {
    switch (prevgamestate) {
      case GS_LEVEL:
        break;
      case GS_INTERMISSION:
        WI_End();
      default:
        break;
    }
    prevgamestate = gamestate;
  }

  // e6y
  // do nothing if a pause has been pressed during playback
  // pausing during intermission can cause desynchs without that
  if (dsda_PausedOutsideDemo() && gamestate != GS_LEVEL)
    return;

  // do main actions
  switch (gamestate)
  {
    case GS_LEVEL:
      P_Ticker();
      P_WalkTicker();
      mlooky = 0;
      AM_Ticker();
      ST_Ticker();
      HU_Ticker();
      break;

    case GS_INTERMISSION:
      WI_Ticker();
      break;

    case GS_FINALE:
      F_Ticker();
      break;

    case GS_DEMOSCREEN:
      D_PageTicker();
      break;
  }

  if (advance_frame)
    dsda_UnmaskPause(pause_mask);
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_PlayerFinishLevel
// Can when a player completes a level.
//

static void G_PlayerFinishLevel(int player)
{
  int i;
  player_t *p = &players[player];

  for (i = 0; i < p->inventorySlotNum; i++)
  {
    p->inventory[i].count = 1;
  }
  p->artifactCount = p->inventorySlotNum;
  if (!deathmatch)
  {
    for (i = 0; i < 16; i++)
    {
      P_PlayerUseArtifact(p, arti_fly);
    }
  }
  if (p->chickenTics)
  {
    p->readyweapon = p->mo->special1.i;       // Restore weapon
    p->chickenTics = 0;
  }
  p->lookdir = 0;
  p->rain1 = NULL;
  p->rain2 = NULL;
  playerkeys = 0;
  if (p == &players[consoleplayer])
  {
    SB_Start();          // refresh the status bar
  }

  memset(p->powers, 0, sizeof p->powers);
  memset(p->cards, 0, sizeof p->cards);
  p->mo = NULL;           // cph - this is allocated PU_LEVEL so it's gone
  p->extralight = 0;      // cancel gun flashes
  p->fixedcolormap = 0;   // cancel ir gogles
  p->damagecount = 0;     // no palette changes
  p->bonuscount = 0;
}

// CPhipps - G_SetPlayerColour
// Player colours stuff
//
// G_SetPlayerColour

#include "r_draw.h"

void G_ChangedPlayerColour(int pn, int cl)
{
  int i;

  if (!netgame) return;

  mapcolor_plyr[pn] = cl;

  // Rebuild colour translation tables accordingly
  R_InitTranslationTables();
  // Change translations on existing player mobj's
  for (i = 0; i < g_maxplayers; i++) {
    if ((gamestate == GS_LEVEL) && playeringame[i] && (players[i].mo != NULL)) {
      players[i].mo->flags &= ~MF_TRANSLATION;
      players[i].mo->flags |= ((uint_64_t)playernumtotrans[i]) << MF_TRANSSHIFT;
    }
  }
}

//
// G_PlayerReborn
// Called after a player dies
// almost everything is cleared and initialized
//

void G_PlayerReborn (int player)
{
  player_t *p;
  int i;
  int frags[MAX_MAXPLAYERS];
  int killcount;
  int itemcount;
  int secretcount;
  int maxkilldiscount; //e6y
  unsigned int worldTimer;

  memcpy (frags, players[player].frags, sizeof frags);
  killcount = players[player].killcount;
  itemcount = players[player].itemcount;
  secretcount = players[player].secretcount;
  maxkilldiscount = players[player].maxkilldiscount; //e6y
  worldTimer = players[player].worldTimer;

  p = &players[player];

  // killough 3/10/98,3/21/98: preserve cheats across idclev
  {
    int cheats = p->cheats;
    memset (p, 0, sizeof(*p));
    p->cheats = cheats;
  }

  memcpy(players[player].frags, frags, sizeof(players[player].frags));
  players[player].killcount = killcount;
  players[player].itemcount = itemcount;
  players[player].secretcount = secretcount;
  players[player].maxkilldiscount = maxkilldiscount; //e6y
  players[player].worldTimer = worldTimer;
  players[player].pclass = PlayerClass[player];

  p->usedown = p->attackdown = true;  // don't do anything immediately
  p->playerstate = PST_LIVE;
  p->health = initial_health;  // Ty 03/12/98 - use dehacked values

  if (hexen)
  {
    p->readyweapon = p->pendingweapon = wp_first;
    p->weaponowned[wp_first] = true;
  }
  else
  {
    p->readyweapon = p->pendingweapon = g_wp_pistol;
    p->weaponowned[g_wp_fist] = true;
    p->weaponowned[g_wp_pistol] = true;
    if (heretic)
      p->ammo[am_goldwand] = 50;
    else
      p->ammo[am_clip] = initial_bullets; // Ty 03/12/98 - use dehacked values
  }

  p->lookdir = 0;
  localQuakeHappening[player] = false;
  if (p == &players[consoleplayer])
  {
    SB_Start();             // refresh the status bar
    inv_ptr = 0;            // reset the inventory pointer
    curpos = 0;
    viewangleoffset = 0;
  }

  for (i=0 ; i<NUMAMMO ; i++)
    p->maxammo[i] = maxammo[i];
}

//
// G_CheckSpot
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//

static dboolean G_CheckSpot(int playernum, mapthing_t *mthing)
{
  fixed_t     x,y;
  subsector_t *ss;
  int         i;

  if (!players[playernum].mo)
    {
      // first spawn of level, before corpses
      for (i=0 ; i<playernum ; i++)
        if (players[i].mo->x == mthing->x << FRACBITS
            && players[i].mo->y == mthing->y << FRACBITS)
          return false;
      return true;
    }

  x = mthing->x << FRACBITS;
  y = mthing->y << FRACBITS;

  if (raven)
  {
    unsigned an;
    mobj_t *mo;

    players[playernum].mo->flags2 &= ~MF2_PASSMOBJ;
    if (!P_CheckPosition(players[playernum].mo, x, y))
    {
      players[playernum].mo->flags2 |= MF2_PASSMOBJ;
      return false;
    }
    players[playernum].mo->flags2 |= MF2_PASSMOBJ;

    // spawn a teleport fog
    ss = R_PointInSubsector(x, y);
    an = ((unsigned) ANG45 * (mthing->angle / 45)) >> ANGLETOFINESHIFT;

    mo = P_SpawnMobj(x + 20 * finecosine[an], y + 20 * finesine[an],
                     ss->sector->floorheight + TELEFOGHEIGHT, g_mt_tfog);

    if (players[consoleplayer].viewz != 1)
      S_StartSound(mo, g_sfx_telept);   // don't start sound on first frame

    return true;
  }

  // killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
  // corpse to detect collisions with other players in DM starts
  //
  // Old code:
  // if (!P_CheckPosition (players[playernum].mo, x, y))
  //    return false;

  players[playernum].mo->flags |=  MF_SOLID;
  i = P_CheckPosition(players[playernum].mo, x, y);
  players[playernum].mo->flags &= ~MF_SOLID;
  if (!i)
    return false;

  // flush an old corpse if needed
  // killough 2/8/98: make corpse queue have an adjustable limit
  // killough 8/1/98: Fix bugs causing strange crashes

  if (bodyquesize > 0)
    {
      static int queuesize;
      if (queuesize < bodyquesize)
  {
    bodyque = realloc(bodyque, bodyquesize*sizeof*bodyque);
    memset(bodyque+queuesize, 0,
     (bodyquesize-queuesize)*sizeof*bodyque);
    queuesize = bodyquesize;
  }
      if (bodyqueslot >= bodyquesize)
  P_RemoveMobj(bodyque[bodyqueslot % bodyquesize]);
      bodyque[bodyqueslot++ % bodyquesize] = players[playernum].mo;
    }
  else
    if (!bodyquesize)
      P_RemoveMobj(players[playernum].mo);

  // spawn a teleport fog
  ss = R_PointInSubsector (x,y);
  { // Teleport fog at respawn point
    fixed_t xa,ya;
    int an;
    mobj_t      *mo;

/* BUG: an can end up negative, because mthing->angle is (signed) short.
 * We have to emulate original Doom's behaviour, deferencing past the start
 * of the array, into the previous array (finetangent) */
    an = ( ANG45 * ((signed)mthing->angle/45) ) >> ANGLETOFINESHIFT;
    xa = finecosine[an];
    ya = finesine[an];

    if (compatibility_level <= finaldoom_compatibility || compatibility_level == prboom_4_compatibility)
      switch (an) {
      case -4096: xa = finetangent[2048];   // finecosine[-4096]
            ya = finetangent[0];      // finesine[-4096]
            break;
      case -3072: xa = finetangent[3072];   // finecosine[-3072]
            ya = finetangent[1024];   // finesine[-3072]
            break;
      case -2048: xa = finesine[0];   // finecosine[-2048]
            ya = finetangent[2048];   // finesine[-2048]
            break;
      case -1024:  xa = finesine[1024];     // finecosine[-1024]
            ya = finetangent[3072];  // finesine[-1024]
            break;
      case 1024:
      case 2048:
      case 3072:
      case 4096:
      case 0:  break; /* correct angles set above */
      default:  I_Error("G_CheckSpot: unexpected angle %d\n",an);
      }

    mo = P_SpawnMobj(x+20*xa, y+20*ya, ss->sector->floorheight, MT_TFOG);

    if (players[consoleplayer].viewz != 1)
      S_StartSound(mo, sfx_telept);  // don't start sound on first frame
  }

  return true;
}


// G_DeathMatchSpawnPlayer
// Spawns a player at one of the random death match spots
// called at level load and each death
//
void G_DeathMatchSpawnPlayer (int playernum)
{
  int j, selections = deathmatch_p - deathmatchstarts;

  if (selections < g_maxplayers)
    I_Error("G_DeathMatchSpawnPlayer: Only %i deathmatch spots, %d required",
    selections, g_maxplayers);

  for (j=0 ; j<20 ; j++)
    {
      int i = P_Random(pr_dmspawn) % selections;
      if (G_CheckSpot (playernum, &deathmatchstarts[i]) )
        {
          deathmatchstarts[i].type = playernum+1;
          P_SpawnPlayer (playernum, &deathmatchstarts[i]);
          return;
        }
    }

  // no good spot, so the player will probably get stuck
  P_SpawnPlayer (playernum, &playerstarts[0][playernum]);
}

//
// G_DoReborn
//

void G_DoReborn (int playernum)
{
  if (hexen)
    return Hexen_G_DoReborn(playernum);

  if (!netgame)
    gameaction = ga_loadlevel;      // reload the level from scratch
  else
    {                               // respawn at the start
      int i;

      // first dissasociate the corpse
      players[playernum].mo->player = NULL;

      // spawn at random spot if in death match
      if (deathmatch)
        {
          G_DeathMatchSpawnPlayer (playernum);
          return;
        }

      if (G_CheckSpot (playernum, &playerstarts[0][playernum]) )
        {
          P_SpawnPlayer (playernum, &playerstarts[0][playernum]);
          return;
        }

      // try to spawn at one of the other players spots
      for (i = 0; i < g_maxplayers; i++)
        {
          if (G_CheckSpot (playernum, &playerstarts[0][i]) )
            {
              P_SpawnPlayer (playernum, &playerstarts[0][i]);
              return;
            }
          // he's going to be inside something.  Too bad.
        }
      P_SpawnPlayer (playernum, &playerstarts[0][playernum]);
    }
}

void G_ScreenShot (void)
{
  gameaction = ga_screenshot;
}

// DOOM Par Times
int pars[5][10] = {
  {0},
  {0,30,75,120,90,165,180,180,30,165},
  {0,90,90,90,120,90,360,240,30,170},
  {0,90,45,90,150,90,90,165,30,135},
  // from Doom 3 BFG Edition
  {0,165,255,135,150,180,390,135,360,180}
};

// DOOM II Par Times
int cpars[34] = {
  30,90,120,120,90,150,120,120,270,90,  //  1-10
  210,150,150,150,210,150,420,150,210,150,  // 11-20
  240,150,180,150,150,300,330,420,300,180,  // 21-30
  120,30,30,30          // 31-34
};

dboolean secretexit;

void G_ExitLevel (void)
{
  secretexit = false;
  gameaction = ga_completed;
}

// Here's for the german edition.
// IF NO WOLF3D LEVELS, NO SECRET EXIT!

void G_SecretExitLevel (void)
{
  if (gamemode!=commercial || haswolflevels)
    secretexit = true;
  else
    secretexit = false;
  gameaction = ga_completed;
}

//
// G_DoCompleted
//

void G_DoCompleted (void)
{
  int i;
  int completed_behaviour;

  if (hexen)
    return Hexen_G_DoCompleted();

  gameaction = ga_nothing;

  for (i = 0; i < g_maxplayers; i++)
    if (playeringame[i])
      G_PlayerFinishLevel(i);        // take away cards and stuff

  if (automapmode & am_active)
    AM_Stop();

  wminfo.nextep = wminfo.epsd = gameepisode -1;
  wminfo.last = gamemap -1;

  dsda_UpdateLastMapInfo();

  dsda_PrepareIntermission(&completed_behaviour);

  if (completed_behaviour & DC_VICTORY)
  {
    gameaction = ga_victory;
    return;
  }

  dsda_UpdateNextMapInfo();
  wminfo.maxkills = totalkills;
  wminfo.maxitems = totalitems;
  wminfo.maxsecret = totalsecret;
  wminfo.maxfrags = 0;
  wminfo.pnum = consoleplayer;

  for (i = 0; i < g_maxplayers; i++)
    {
      wminfo.plyr[i].in = playeringame[i];
      wminfo.plyr[i].skills = players[i].killcount;
      wminfo.plyr[i].sitems = players[i].itemcount;
      wminfo.plyr[i].ssecret = players[i].secretcount;
      wminfo.plyr[i].stime = leveltime;
      memcpy (wminfo.plyr[i].frags, players[i].frags,
              sizeof(wminfo.plyr[i].frags));
    }

  /* cph - modified so that only whole seconds are added to the totalleveltimes
   *  value; so our total is compatible with the "naive" total of just adding
   *  the times in seconds shown for each level. Also means our total time
   *  will agree with Compet-n.
   */
  wminfo.totaltimes = (totalleveltimes += (leveltime - leveltime%35));

  gamestate = GS_INTERMISSION;
  automapmode &= ~am_active;

  // lmpwatch.pl engine-side demo testing support
  // print "FINISHED: <mapname>" when the player exits the current map
  if (nodrawers && (demoplayback || timingdemo)) {
    if (gamemode == commercial)
      lprintf(LO_INFO, "FINISHED: MAP%02d\n", gamemap);
    else
      lprintf(LO_INFO, "FINISHED: E%dM%d\n", gameepisode, gamemap);
  }

  e6y_G_DoCompleted();//e6y

  dsda_WatchLevelCompletion();

  if (gamemode == commercial || gamemap != 8)
  {
    StatCopy(&wminfo);
  }

  WI_Start (&wminfo);
}

//
// G_WorldDone
//

void G_WorldDone (void)
{
  int done_behaviour;

  gameaction = ga_worlddone;

  if (secretexit)
    players[consoleplayer].didsecret = true;

  dsda_PrepareFinale(&done_behaviour);

  if (done_behaviour & WD_VICTORY)
  {
    gameaction = ga_victory;

    return;
  }

  if (done_behaviour & WD_START_FINALE)
  {
    F_StartFinale();

    return;
  }
}

void G_DoWorldDone (void)
{
  idmusnum = -1;             //jff 3/17/98 allow new level's music to be loaded
  gamestate = GS_LEVEL;
  dsda_UpdateGameMap(wminfo.nextep + 1, wminfo.next + 1);
  G_DoLoadLevel();
  gameaction = ga_nothing;
  AM_clearMarks();           //jff 4/12/98 clear any marks on the automap
  dsda_EvaluateSkipModeDoWorldDone();
}

extern dboolean setsizeneeded;

//CPhipps - savename variable redundant

/* killough 12/98:
 * This function returns a signature for the current wad.
 * It is used to distinguish between wads, for the purposes
 * of savegame compatibility warnings, and options lookups.
 */

static uint_64_t G_UpdateSignature(uint_64_t s, const char *name)
{
  int i, lump = W_CheckNumForName(name);
  if (lump != -1 && (i = lump+10) < numlumps)
    do
      {
  int size = W_LumpLength(i);
  const byte *p = W_CacheLumpNum(i);
  while (size--)
    s <<= 1, s += *p++;
  W_UnlockLumpNum(i);
      }
    while (--i > lump);
  return s;
}

static uint_64_t G_Signature(void)
{
  static uint_64_t s = 0;
  static dboolean computed = false;
  char name[9];
  int episode, map;

  if (!computed) {
   computed = true;
   if (gamemode == commercial)
    for (map = haswolflevels ? 32 : 30; map; map--)
      sprintf(name, "map%02d", map), s = G_UpdateSignature(s, name);
   else
    for (episode = gamemode==retail ? 4 :
     gamemode==shareware ? 1 : 3; episode; episode--)
      for (map = 9; map; map--)
        sprintf(name, "E%dM%d", episode, map), s = G_UpdateSignature(s, name);
  }
  return s;
}

//
// killough 5/15/98: add forced loadgames, which allow user to override checks
//

void G_ForcedLoadGame(void)
{
  // CPhipps - net loadgames are always forced, so we only reach here
  //  in single player
  gameaction = ga_loadgame;
  forced_loadgame = true;
}

// killough 3/16/98: add slot info
void G_LoadGame(int slot)
{
  if (demorecording)
  {
    dsda_QueueExCmdLoad(slot);
    return;
  }

  if (!demoplayback)
  {
    forced_loadgame = netgame; // CPhipps - always force load netgames
  }
  else
  {
    forced_loadgame = false;
    demoplayback = false;
    // Don't stay in netgame state if loading single player save
    // while watching multiplayer demo
    netgame = false;
  }

  gameaction = ga_loadgame;
  savegameslot = slot;
  load_via_cmd = false;
  R_SmoothPlaying_Reset(NULL); // e6y
}

// killough 5/15/98:
// Consistency Error when attempting to load savegame.

static void G_LoadGameErr(const char *msg)
{
  Z_Free(savebuffer);                // Free the savegame buffer
  M_ForcedLoadGame(msg);             // Print message asking for 'Y' to force
}

// CPhipps - size of version header
#define VERSIONSIZE   16
#define SAVEVERSION "DSDA-DOOM 2"

const char * comp_lev_str[MAX_COMPATIBILITY_LEVEL] =
{ "Doom v1.2", "Doom v1.666", "Doom/Doom2 v1.9", "Ultimate Doom/Doom95", "Final Doom",
  "early DosDoom", "TASDoom", "\"boom compatibility\"", "boom v2.01", "boom v2.02", "lxdoom v1.3.2+",
  "MBF", "PrBoom 2.03beta", "PrBoom v2.1.0-2.1.1", "PrBoom v2.1.2-v2.2.6",
  "PrBoom v2.3.x", "PrBoom 2.4.0", "Current PrBoom", "", "", "", "MBF21" };

//e6y
unsigned int GetPackageVersion(void)
{
  static unsigned int PACKAGEVERSION = 0;

  //e6y: "2.4.8.2" -> 0x02040802
  if (PACKAGEVERSION == 0)
  {
    int b[4], i, k = 1;
    memset(b, 0, sizeof(b));
    sscanf(PACKAGE_VERSION, "%d.%d.%d.%d", &b[0], &b[1], &b[2], &b[3]);
    for (i = 3; i >= 0; i--, k *= 256)
    {
      PACKAGEVERSION += b[i] * k;
    }
  }
  return PACKAGEVERSION;
}

//==========================================================================
//
// RecalculateDrawnSubsectors
//
// In case the subsector data is unusable this function tries to reconstruct
// if from the linedefs' ML_MAPPED info.
//
//==========================================================================

void RecalculateDrawnSubsectors(void)
{
#ifdef GL_DOOM
  int i, j;

  for (i = 0; i < numsubsectors; i++)
  {
    subsector_t *sub = &subsectors[i];
    seg_t *seg = &segs[sub->firstline];
    for (j = 0; j < sub->numlines; j++, seg++)
    {
      if (seg->linedef && seg->linedef->flags & ML_MAPPED)
      {
        map_subsectors[i] = 1;
      }
    }
  }

  gld_ResetTexturedAutomap();
#endif
}

void G_DoLoadGame(void)
{
  int  length, i;
  // CPhipps - do savegame filename stuff here
  char *name;                // killough 3/22/98
  int savegame_compatibility = -1;
  //e6y: numeric version number of package should be zero before initializing from savegame
  unsigned int packageversion = 0;
  const char *maplump;
  int time, ttime;
  int epi, map;

  dsda_SetLastLoadSlot(savegameslot);

  name = dsda_SaveGameName(savegameslot, load_via_cmd);

  // [crispy] loaded game must always be single player.
  // Needed for ability to use a further game loading, as well as
  // cheat codes and other single player only specifics.
  if (!load_via_cmd)
  {
    netdemo = false;
    netgame = false;
    deathmatch = false;
  }

  gameaction = ga_nothing;

  length = M_ReadFile(name, &savebuffer);
  if (length<=0)
    I_Error("Couldn't read file %s: %s", name, "(Unknown Error)");
  free(name);
  save_p = savebuffer + SAVESTRINGSIZE;

  if (strncmp((char*)save_p, SAVEVERSION, VERSIONSIZE) && !forced_loadgame) {
    G_LoadGameErr("Unrecognised savegame version!\nAre you sure? (y/n) ");
    return;
  }

  save_p += VERSIONSIZE;

  // CPhipps - always check savegames even when forced,
  //  only print a warning if forced
  {  // killough 3/16/98: check lump name checksum (independent of order)
    uint_64_t checksum = 0;

    checksum = G_Signature();

    if (memcmp(&checksum, save_p, sizeof checksum))
    {
      if (!forced_loadgame)
      {
        char *msg = malloc(strlen((char*)save_p + sizeof checksum) + 128);
        strcpy(msg,"Incompatible Savegame!!!\n");
        if (save_p[sizeof checksum])
          strcat(strcat(msg,"Wads expected:\n\n"), (char*)save_p + sizeof checksum);
        strcat(msg, "\nAre you sure?");
        G_LoadGameErr(msg);
        free(msg);
        return;
      }
      else
        lprintf(LO_WARN, "G_DoLoadGame: Incompatible savegame\n");
    }
    save_p += sizeof checksum;
  }

  save_p += strlen((char*)save_p)+1;

  //e6y: check on new savegame format
  if (!memcmp(NEWFORMATSIG, save_p, strlen(NEWFORMATSIG)))
  {
    save_p += strlen(NEWFORMATSIG);
    memcpy(&packageversion, save_p, sizeof packageversion);
    save_p += sizeof packageversion;
  }

  compatibility_level = *save_p++;
  gameskill = *save_p++;

  epi = *save_p++;
  map = *save_p++;
  dsda_UpdateGameMap(epi, map);

  for (i = 0; i < g_maxplayers; i++)
    playeringame[i] = *save_p++;
  save_p += FUTURE_MAXPLAYERS - g_maxplayers;         // killough 2/28/98

  idmusnum = *save_p++;           // jff 3/17/98 restore idmus music
  if (idmusnum==255) idmusnum=-1; // jff 3/18/98 account for unsigned byte

  /* killough 3/1/98: Read game options
   * killough 11/98: move down to here
   */
  // Avoid assignment of const to non-const: add the difference
  // between the updated and original pointer onto the original
  save_p += (G_ReadOptions(save_p) - save_p);

  // load a base level
  G_InitNew (gameskill, gameepisode, gamemap, false);

  /* get the times - killough 11/98: save entire word */
  memcpy(&leveltime, save_p, sizeof leveltime);
  save_p += sizeof leveltime;

  /* cph - total episode time */
  //e6y: total level times are always saved since 2.4.8.1
  memcpy(&totalleveltimes, save_p, sizeof totalleveltimes);
  save_p += sizeof totalleveltimes;

  // killough 11/98: load revenant tracer state
  basetic = gametic - *save_p++;

  // dearchive all the modifications
  dsda_UnArchiveAll();

  R_ActivateSectorInterpolations();//e6y
  R_SmoothPlaying_Reset(NULL); // e6y

  if (musinfo.current_item != -1)
  {
    S_ChangeMusInfoMusic(musinfo.current_item, true);
  }

  RecalculateDrawnSubsectors();

  if (*save_p != 0xe6)
    I_Error ("G_DoLoadGame: Bad savegame");

  /* Print some information about the save game */
  maplump = MAPNAME(gameepisode, gamemap);
  time = leveltime / TICRATE;
  ttime = (totalleveltimes + leveltime) / TICRATE;

  lprintf(LO_INFO, "G_DoLoadGame: [%d] %s (%s), Skill %d, Level Time %02d:%02d:%02d, Total Time %02d:%02d:%02d\n",
    savegameslot + 1, maplump, W_GetLumpInfoByNum(W_GetNumForName(maplump))->wadfile->name, gameskill + 1,
    time/3600, (time%3600)/60, time%60, ttime/3600, (ttime%3600)/60, ttime%60);

  // done
  Z_Free (savebuffer);

  if (hexen)
  {
    SB_SetClassData();
  }

  if (raven)
  {
    players[consoleplayer].readyArtifact = players[consoleplayer].inventory[inv_ptr].type;
  }

  if (setsizeneeded)
    R_ExecuteSetViewSize ();

  // draw the pattern into the back screen
  R_FillBackScreen ();

  if (load_via_cmd)
  {
    // do nothing
  }
  else
  {
    singledemo = false;  /* Clear singledemo flag if loading from menu */
  }
}

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//

void G_SaveGame(int slot, const char *description)
{
  strcpy(savedescription, description);

  if (demorecording && dsda_AllowCasualExCmdFeatures())
  {
    dsda_QueueExCmdSave(slot);
  }
  else
  {
    savegameslot = slot;
    G_DoSaveGame(false);
  }
}

// Check for overrun and realloc if necessary -- Lee Killough 1/22/98
void (CheckSaveGame)(size_t size, const char* file, int line)
{
  size_t pos = save_p - savebuffer;

#ifdef RANGECHECK
  /* cph 2006/08/07 - after-the-fact sanity checking of CheckSaveGame calls */
  static size_t prev_check;
  static const char* prevf;
  static int prevl;

  if (pos > prev_check)
    I_Error("CheckSaveGame at %s:%d called for insufficient buffer (%u < %u)", prevf, prevl, prev_check, pos);
  prev_check = size + pos;
  prevf = file;
  prevl = line;
#endif

  size += 1024;  // breathing room
  if (pos+size > savegamesize)
    save_p = (savebuffer = realloc(savebuffer,
           savegamesize += (size+1023) & ~1023)) + pos;
}

static void G_DoSaveGame(dboolean via_cmd)
{
  char *name;
  char *description;
  int  i;
  //e6y: numeric version number of package
  unsigned int packageversion = GetPackageVersion();
  const char *maplump;
  int time, ttime;

  gameaction = ga_nothing; // cph - cancel savegame at top of this function,
    // in case later problems cause a premature exit

  dsda_SetLastSaveSlot(savegameslot);

  name = dsda_SaveGameName(savegameslot, via_cmd);

  description = savedescription;

  save_p = savebuffer = malloc(savegamesize);

  CheckSaveGame(SAVESTRINGSIZE+VERSIONSIZE+sizeof(uint_64_t));
  memcpy (save_p, description, SAVESTRINGSIZE);
  save_p += SAVESTRINGSIZE;

  memset(save_p, 0, VERSIONSIZE);
  strncpy(save_p, SAVEVERSION, VERSIONSIZE);
  save_p += VERSIONSIZE;

  { /* killough 3/16/98, 12/98: store lump name checksum */
    uint_64_t checksum = G_Signature();
    memcpy(save_p, &checksum, sizeof checksum);
    save_p += sizeof checksum;
  }

  // killough 3/16/98: store pwad filenames in savegame
  {
    // CPhipps - changed for new wadfiles handling
    size_t i;
    for (i = 0; i<numwadfiles; i++)
      {
        const char *const w = wadfiles[i].name;
        CheckSaveGame(strlen(w)+2);
        strcpy((char*)save_p, w);
        save_p += strlen((char*)save_p);
        *save_p++ = '\n';
      }
    *save_p++ = 0;
  }

  CheckSaveGame(dsda_GameOptionSize()+FUTURE_MAXPLAYERS+14+strlen(NEWFORMATSIG)+sizeof packageversion);

  //e6y: saving of the version number of package
  strcpy((char*)save_p, NEWFORMATSIG);
  save_p += strlen(NEWFORMATSIG);
  memcpy(save_p, &packageversion, sizeof packageversion);
  save_p += sizeof packageversion;

  *save_p++ = compatibility_level;

  *save_p++ = gameskill;
  *save_p++ = gameepisode;
  *save_p++ = gamemap;

  for (i = 0; i < g_maxplayers; i++)
    *save_p++ = playeringame[i];

  for (;i<FUTURE_MAXPLAYERS;i++)         // killough 2/28/98
    *save_p++ = 0;

  *save_p++ = idmusnum;               // jff 3/17/98 save idmus state

  save_p = G_WriteOptions(save_p);    // killough 3/1/98: save game options

  /* cph - FIXME - endianness? */
  /* killough 11/98: save entire word */
  memcpy(save_p, &leveltime, sizeof leveltime);
  save_p += sizeof leveltime;

  /* cph - total episode time */
  //e6y: always saved since 2.4.8
  memcpy(save_p, &totalleveltimes, sizeof totalleveltimes);
  save_p += sizeof totalleveltimes;

  // killough 11/98: save revenant tracer state
  *save_p++ = logictic & 255;

  dsda_ArchiveAll();

  *save_p++ = 0xe6;   // consistency marker

  doom_printf( "%s", M_WriteFile(name, savebuffer, save_p - savebuffer)
         ? s_GGSAVED /* Ty - externalised */
         : "Game save failed!"); // CPhipps - not externalised

  /* Print some information about the save game */
  maplump = MAPNAME(gameepisode, gamemap);
  time = leveltime / TICRATE;
  ttime = (totalleveltimes + leveltime) / TICRATE;

  lprintf(LO_INFO, "G_DoSaveGame: [%d] %s (%s), Skill %d, Level Time %02d:%02d:%02d, Total Time %02d:%02d:%02d\n",
    savegameslot + 1, maplump, W_GetLumpInfoByNum(W_GetNumForName(maplump))->wadfile->name, gameskill + 1,
    time/3600, (time%3600)/60, time%60, ttime/3600, (ttime%3600)/60, ttime%60);

  free(savebuffer);  // killough
  savebuffer = save_p = NULL;

  savedescription[0] = 0;
  free(name);
}

static skill_t d_skill;
static int     d_episode;
static int     d_map;

void G_DeferedInitNew(skill_t skill, int episode, int map)
{
  d_skill = skill;
  d_episode = episode;
  d_map = map;
  gameaction = ga_newgame;

  dsda_WatchDeferredInitNew(skill, episode, map);
}

/* cph -
 * G_Compatibility
 *
 * Initialises the comp[] array based on the compatibility_level
 * For reference, MBF did:
 * for (i=0; i < COMP_TOTAL; i++)
 *   comp[i] = compatibility;
 *
 * Instead, we have a lookup table showing at what version a fix was
 *  introduced, and made optional (replaces comp_options_by_version)
 */

void G_Compatibility(void)
{
  static const struct {
    complevel_t fix; // level at which fix/change was introduced
    complevel_t opt; // level at which fix/change was made optional
  } levels[] = {
    // comp_telefrag - monsters used to telefrag only on MAP30, now they do it for spawners only
    { mbf_compatibility, mbf_compatibility },
    // comp_dropoff - MBF encourages things to drop off of overhangs
    { mbf_compatibility, mbf_compatibility },
    // comp_vile - original Doom archville bugs like ghosts
    { boom_compatibility, mbf_compatibility },
    // comp_pain - original Doom limits Pain Elementals from spawning too many skulls
    { boom_compatibility, mbf_compatibility },
    // comp_skull - original Doom let skulls be spit through walls by Pain Elementals
    { boom_compatibility, mbf_compatibility },
    // comp_blazing - original Doom duplicated blazing door sound
    { boom_compatibility, mbf_compatibility },
    // e6y: "Tagged doors don't trigger special lighting" handled wrong
    // http://sourceforge.net/tracker/index.php?func=detail&aid=1411400&group_id=148658&atid=772943
    // comp_doorlight - MBF made door lighting changes more gradual
    { boom_compatibility, mbf_compatibility },
    // comp_model - improvements to the game physics
    { boom_compatibility, mbf_compatibility },
    // comp_god - fixes to God mode
    { boom_compatibility, mbf_compatibility },
    // comp_falloff - MBF encourages things to drop off of overhangs
    { mbf_compatibility, mbf_compatibility },
    // comp_floors - fixes for moving floors bugs
    { boom_compatibility_compatibility, mbf_compatibility },
    // comp_skymap
    { mbf_compatibility, mbf_compatibility },
    // comp_pursuit - MBF AI change, limited pursuit?
    { mbf_compatibility, mbf_compatibility },
    // comp_doorstuck - monsters stuck in doors fix
    { boom_202_compatibility, mbf_compatibility },
    // comp_staylift - MBF AI change, monsters try to stay on lifts
    { mbf_compatibility, mbf_compatibility },
    // comp_zombie - prevent dead players triggering stuff
    { lxdoom_1_compatibility, mbf_compatibility },
    // comp_stairs - see p_floor.c
    { boom_202_compatibility, mbf_compatibility },
    // comp_infcheat - FIXME
    { mbf_compatibility, mbf_compatibility },
    // comp_zerotags - allow zero tags in wads */
    { boom_compatibility, mbf_compatibility },
    // comp_moveblock - enables keygrab and mancubi shots going thru walls
    { lxdoom_1_compatibility, prboom_2_compatibility },
    // comp_respawn - objects which aren't on the map at game start respawn at (0,0)
    { prboom_2_compatibility, prboom_2_compatibility },
    // comp_sound - see s_sound.c
    { boom_compatibility_compatibility, prboom_3_compatibility },
    // comp_666 - emulate pre-Ultimate BossDeath behaviour
    { ultdoom_compatibility, prboom_4_compatibility },
    // comp_soul - enables lost souls bouncing (see P_ZMovement)
    { prboom_4_compatibility, prboom_4_compatibility },
    // comp_maskedanim - 2s mid textures don't animate
    { doom_1666_compatibility, prboom_4_compatibility },
    //e6y
    // comp_ouchface - Use Doom's buggy "Ouch" face code
    { prboom_1_compatibility, prboom_6_compatibility },
    // comp_maxhealth - Max Health in DEH applies only to potions
    { boom_compatibility_compatibility, prboom_6_compatibility },
    // comp_translucency - No predefined translucency for some things
    { boom_compatibility_compatibility, prboom_6_compatibility },
    // comp_ledgeblock - ground monsters are blocked by ledges
    { boom_compatibility, mbf21_compatibility },
    // comp_friendlyspawn - A_Spawn new mobj inherits friendliness
    { prboom_1_compatibility, mbf21_compatibility },
    // comp_voodooscroller - Voodoo dolls on slow scrollers move too slowly
    { mbf21_compatibility, mbf21_compatibility },
    // comp_reservedlineflag - ML_RESERVED clears extended flags
    { mbf21_compatibility, mbf21_compatibility }
  };
  unsigned int i;

  if (sizeof(levels)/sizeof(*levels) != MBF_COMP_TOTAL)
    I_Error("G_Compatibility: consistency error");

  for (i = 0; i < sizeof(levels)/sizeof(*levels); i++)
    if (compatibility_level < levels[i].opt)
      comp[i] = (compatibility_level < levels[i].fix);

  // These options were deoptionalized in mbf21
  if (mbf21)
  {
    comp[comp_moveblock] = 0;
    comp[comp_sound] = 0;
    comp[comp_666] = 0;
    comp[comp_maskedanim] = 0;
    comp[comp_ouchface] = 0;
    comp[comp_maxhealth] = 0;
    comp[comp_translucency] = 0;
  }

  e6y_G_Compatibility();//e6y

  if (!mbf_features) {
    monster_infighting = 1;
    monster_backing = 0;
    monster_avoid_hazards = 0;
    monster_friction = 0;
    help_friends = 0;

    dogs = 0;
    dog_jumping = 0;

    monkeys = 0;
  }
}

// killough 3/1/98: function to reload all the default parameter
// settings before a new game begins

void G_ReloadDefaults(void)
{
  const dsda_options_t* options;

  compatibility_level = default_compatibility_level;
  {
    int l;
    l = dsda_CompatibilityLevel();
    if (l != UNSPECIFIED_COMPLEVEL)
      compatibility_level = l;
    else
      dsda_MarkCompatibilityLevelUnspecified();
  }
  if (compatibility_level == -1)
    compatibility_level = best_compatibility;

  // killough 3/1/98: Initialize options based on config file
  // (allows functions above to load different values for demos
  // and savegames without messing up defaults).

  options = dsda_Options();

  weapon_recoil = options->weapon_recoil;    // weapon recoil

  player_bobbing = 1;  // whether player bobs or not

  variable_friction = 1;
  allow_pushers     = 1;
  monsters_remember = options->monsters_remember; // remember former enemies

  monster_infighting = options->monster_infighting; // killough 7/19/98

  dogs = netgame ? 0 : options->player_helpers; // killough 7/19/98
  dog_jumping = options->dog_jumping;

  distfriend = options->friend_distance; // killough 8/8/98

  monster_backing = options->monster_backing; // killough 9/8/98

  monster_avoid_hazards = options->monster_avoid_hazards; // killough 9/9/98

  monster_friction = options->monster_friction; // killough 10/98

  help_friends = options->help_friends; // killough 9/9/98

  monkeys = options->monkeys;

  // jff 1/24/98 reset play mode to command line spec'd version
  // killough 3/1/98: moved to here
  respawnparm = clrespawnparm;
  fastparm = clfastparm;
  nomonsters = clnomonsters;

  //jff 3/24/98 set startskill from defaultskill in config file, unless
  // it has already been set by a -skill parameter
  if (startskill==sk_none)
    startskill = (skill_t)(defaultskill-1);

  demoplayback = false;
  singledemo = false;            // killough 9/29/98: don't stop after 1 demo
  netdemo = false;

  // killough 2/21/98:
  memset(playeringame + 1, 0, sizeof(*playeringame) * (MAX_MAXPLAYERS - 1));

  consoleplayer = 0;

  // MBF introduced configurable compatibility settings
  if (mbf_features)
  {
    comp[comp_telefrag] = options->comp_telefrag;
    comp[comp_dropoff] = options->comp_dropoff;
    comp[comp_vile] = options->comp_vile;
    comp[comp_pain] = options->comp_pain;
    comp[comp_skull] = options->comp_skull;
    comp[comp_blazing] = options->comp_blazing;
    comp[comp_doorlight] = options->comp_doorlight;
    comp[comp_model] = options->comp_model;
    comp[comp_god] = options->comp_god;
    comp[comp_falloff] = options->comp_falloff;
    comp[comp_floors] = options->comp_floors;
    comp[comp_skymap] = options->comp_skymap;
    comp[comp_pursuit] = options->comp_pursuit;
    comp[comp_doorstuck] = options->comp_doorstuck;
    comp[comp_staylift] = options->comp_staylift;
    comp[comp_zombie] = options->comp_zombie;
    comp[comp_stairs] = options->comp_stairs;
    comp[comp_infcheat] = options->comp_infcheat;
    comp[comp_zerotags] = options->comp_zerotags;

    comp[comp_moveblock] = options->comp_moveblock;
    comp[comp_respawn] = options->comp_respawn;
    comp[comp_sound] = options->comp_sound;
    comp[comp_666] = options->comp_666;
    comp[comp_soul] = options->comp_soul;
    comp[comp_maskedanim] = options->comp_maskedanim;
    comp[comp_ouchface] = options->comp_ouchface;
    comp[comp_maxhealth] = options->comp_maxhealth;
    comp[comp_translucency] = options->comp_translucency;
    comp[comp_ledgeblock] = options->comp_ledgeblock;
    comp[comp_friendlyspawn] = options->comp_friendlyspawn;
    comp[comp_voodooscroller] = options->comp_voodooscroller;
    comp[comp_reservedlineflag] = options->comp_reservedlineflag;
  }

  G_Compatibility();

  // killough 3/31/98, 4/5/98: demo sync insurance
  demo_insurance = 0;

  rngseed += I_GetRandomTimeSeed() + gametic; // CPhipps
}

void G_DoNewGame (void)
{
  int realMap = d_map;
  int realEpisode = d_episode;

  // e6y: allow new level's music to be loaded
  idmusnum = -1;

  G_ReloadDefaults();            // killough 3/1/98
  netgame = solo_net;
  deathmatch = false;

  dsda_NewGameMap(&realEpisode, &realMap);

  G_InitNew (d_skill, realEpisode, realMap, true);
  gameaction = ga_nothing;

  dsda_WatchNewGame();

  //jff 4/26/98 wake up the status bar in case were coming out of a DM demo
  ST_Start();
  walkcamera.type=0; //e6y
}

// killough 4/10/98: New function to fix bug which caused Doom
// lockups when idclev was used in conjunction with -fast.

void G_SetFastParms(int fast_pending)
{
  static int fast = 0;            // remembers fast state
  int i;

  if (hexen)
  {
    return;
  }

  if (heretic)
  {
    for (i = 0; MonsterMissileInfo[i].type != -1; i++)
    {
      mobjinfo[MonsterMissileInfo[i].type].speed =
        MonsterMissileInfo[i].speed[fast_pending] << FRACBITS;
    }

    return;
  }

  if (fast != fast_pending) {     /* only change if necessary */
    for (i = 0; i < num_mobj_types; ++i)
      if (mobjinfo[i].altspeed != NO_ALTSPEED)
      {
        int swap = mobjinfo[i].speed;
        mobjinfo[i].speed = mobjinfo[i].altspeed;
        mobjinfo[i].altspeed = swap;
      }

    if ((fast = fast_pending))
    {
      for (i = 0; i < num_states; i++)
        if (states[i].flags & STATEF_SKILL5FAST && (states[i].tics != 1 || demo_compatibility))
          states[i].tics >>= 1;  // don't change 1->0 since it causes cycles
    }
    else
    {
      for (i = 0; i < num_states; i++)
        if (states[i].flags & STATEF_SKILL5FAST)
          states[i].tics <<= 1;
    }
  }
}

int G_ValidateMapName(const char *mapname, int *pEpi, int *pMap)
{
  // Check if the given map name can be expressed as a gameepisode/gamemap pair and be reconstructed from it.
  char lumpname[9], mapuname[9];
  int epi = -1, map = -1;

  if (strlen(mapname) > 8) return 0;
  strncpy(mapuname, mapname, 8);
  mapuname[8] = 0;
  M_Strupr(mapuname);

  if (gamemode != commercial)
  {
    if (sscanf(mapuname, "E%dM%d", &epi, &map) != 2) return 0;
    snprintf(lumpname, 9, "E%dM%d", epi, map);
  }
  else
  {
    if (sscanf(mapuname, "MAP%d", &map) != 1) return 0;
    snprintf(lumpname, 9, "MAP%02d", map);
    epi = 1;
  }
  if (pEpi) *pEpi = epi;
  if (pMap) *pMap = map;
  return !strcmp(mapuname, lumpname);
}

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.
//

extern int EpiCustom;

void G_InitNew(skill_t skill, int episode, int map, dboolean prepare)
{
  int i;

  // e6y
  // This variable is for correct checking for upper limit of episode.
  // Ultimate Doom, Final Doom and Doom95 have
  // "if (episode == 0) episode = 3/4" check instead of
  // "if (episode > 3/4) episode = 3/4"
  dboolean fake_episode_check =
    compatibility_level == ultdoom_compatibility ||
    compatibility_level == finaldoom_compatibility;

  if (prepare)
    dsda_PrepareInitNew();

  if (dsda_Paused())
  {
    dsda_ResetPauseMode();
    S_ResumeSound();
  }

  if (skill > sk_nightmare)
    skill = sk_nightmare;

  if (episode < 1)
    episode = 1;

  // Disable all sanity checks if there are custom episode definitions. They do not make sense in this case.
  if (!EpiCustom && W_CheckNumForName(MAPNAME(episode, map)) == -1)
  {
    if (heretic)
    {
      if (episode > 9)
        episode = 9;
      if (map < 1)
        map = 1;
      if (map > 9)
        map = 9;
    }
    else if (map_format.map99)
    {
      if (map < 1)
        map = 1;
      if (map > 99)
        map = 99;
    }
    else
    {
      //e6y: We need to remove the fourth episode for pre-ultimate complevels.
      if (compatibility_level < ultdoom_compatibility && episode > 3)
      {
        episode = 3;
      }

      //e6y: DosDoom has only this check
      if (compatibility_level == dosdoom_compatibility)
      {
        if (gamemode == shareware)
          episode = 1; // only start episode 1 on shareware
      }
      else
      if (gamemode == retail)
        {
          // e6y: Ability to play any episode with Ultimate Doom,
          // Final Doom or Doom95 compatibility and -warp command line switch
          // E5M1 from 2002ado.wad is an example.
          // Now you can play it with "-warp 5 1 -complevel 3".
          // 'Vanilla' Ultimate Doom executable also allows it.
          if (fake_episode_check ? episode == 0 : episode > 4)
            episode = 4;
        }
      else
        if (gamemode == shareware)
          {
            if (episode > 1)
              episode = 1; // only start episode 1 on shareware
          }
        else
          // e6y: Ability to play any episode with Ultimate Doom,
          // Final Doom or Doom95 compatibility and -warp command line switch
          if (fake_episode_check ? episode == 0 : episode > 3)
            episode = 3;

      if (map < 1)
        map = 1;
      if (map > 9 && gamemode != commercial)
        map = 9;
    }
  }

  {
    extern int dsda_startmap;

    dsda_startmap = map;
  }

  G_SetFastParms(fastparm || skill == sk_nightmare);  // killough 4/10/98

  M_ClearRandom();

  respawnmonsters = (!raven && skill == sk_nightmare) || respawnparm;

  // force players to be initialized upon first level load
  for (i = 0; i < g_maxplayers; i++)
  {
    players[i].playerstate = PST_REBORN;
    players[i].worldTimer = 0;
  }

  dsda_ResetPauseMode();
  dsda_ResetCommandHistory();
  automapmode &= ~am_active;
  gameskill = skill;
  dsda_UpdateGameMap(episode, map);

  totalleveltimes = 0; // cph

  dsda_EvaluateSkipModeInitNew();

  //jff 4/16/98 force marks on automap cleared every new level start
  AM_clearMarks();

  dsda_InitSky();

  G_DoLoadLevel ();
}

//
// DEMO RECORDING
//

#define DEMOHEADER_RESPAWN    0x20
#define DEMOHEADER_LONGTICS   0x10
#define DEMOHEADER_NOMONSTERS 0x02

void G_ReadOneTick(ticcmd_t* cmd, const byte **data_p)
{
  unsigned char at = 0; // e6y: for tasdoom demo format

  cmd->forwardmove = (signed char)(*(*data_p)++);
  cmd->sidemove = (signed char)(*(*data_p)++);
  if (!longtics)
  {
    cmd->angleturn = ((unsigned char)(at = *(*data_p)++))<<8;
  }
  else
  {
    unsigned int lowbyte = (unsigned char)(*(*data_p)++);
    cmd->angleturn = (((signed int)(*(*data_p)++))<<8) + lowbyte;
  }
  cmd->buttons = (unsigned char)(*(*data_p)++);

  if (raven)
  {
    cmd->lookfly = (unsigned char)(*(*data_p)++);
    cmd->arti = (unsigned char)(*(*data_p)++);
  }

  // e6y: ability to play tasdoom demos directly
  if (compatibility_level == tasdoom_compatibility)
  {
    signed char tmp = cmd->forwardmove;
    cmd->forwardmove = cmd->sidemove;
    cmd->sidemove = (signed char)at;
    cmd->angleturn = ((unsigned char)cmd->buttons)<<8;
    cmd->buttons = (byte)tmp;
  }

  dsda_ReadExCmd(cmd, data_p);
}

/* Demo limits removed -- killough
 * cph - record straight to file
 */
void G_WriteDemoTiccmd (ticcmd_t* cmd)
{
  char buf[10];
  char *p = buf;

  if (compatibility_level == tasdoom_compatibility)
  {
    *p++ = cmd->buttons;
    *p++ = cmd->forwardmove;
    *p++ = cmd->sidemove;
    *p++ = (cmd->angleturn+128)>>8;
  }
  else
  {
    *p++ = cmd->forwardmove;
    *p++ = cmd->sidemove;
    if (!longtics) {
      *p++ = (cmd->angleturn+128)>>8;
    } else {
      signed short a = cmd->angleturn;
      *p++ = a & 0xff;
      *p++ = (a >> 8) & 0xff;
    }
    *p++ = cmd->buttons;

    if (raven)
    {
      *p++ = cmd->lookfly;
      *p++ = cmd->arti;
    }
  }

  dsda_WriteExCmd(&p, cmd);

  dsda_WriteTicToDemo(buf, p - buf);

  p = buf; // make SURE it is exactly the same
  G_ReadOneTick(cmd, (const byte **) &p);
}

// These functions are used to read and write game-specific options in demos
// and savegames so that demo sync is preserved and savegame restoration is
// complete. Not all options (for example "compatibility"), however, should
// be loaded and saved here. It is extremely important to use the same
// positions as before for the variables, so if one becomes obsolete, the
// byte(s) should still be skipped over or padded with 0's.
// Lee Killough 3/1/98

byte *G_WriteOptions(byte *demo_p)
{
  byte *target;

  if (mbf21)
  {
    return dsda_WriteOptions21(demo_p);
  }

  target = demo_p + dsda_GameOptionSize();

  *demo_p++ = monsters_remember;  // part of monster AI

  *demo_p++ = variable_friction;  // ice & mud

  *demo_p++ = weapon_recoil;      // weapon recoil

  *demo_p++ = allow_pushers;      // MT_PUSH Things

  *demo_p++ = 0;

  *demo_p++ = player_bobbing;  // whether player bobs or not

  // killough 3/6/98: add parameters to savegame, move around some in demos
  *demo_p++ = respawnparm;
  *demo_p++ = fastparm;
  *demo_p++ = nomonsters;

  *demo_p++ = demo_insurance;        // killough 3/31/98

  // killough 3/26/98: Added rngseed. 3/31/98: moved here
  *demo_p++ = (byte)((rngseed >> 24) & 0xff);
  *demo_p++ = (byte)((rngseed >> 16) & 0xff);
  *demo_p++ = (byte)((rngseed >>  8) & 0xff);
  *demo_p++ = (byte)( rngseed        & 0xff);

  // Options new to v2.03 begin here

  *demo_p++ = monster_infighting;   // killough 7/19/98

  *demo_p++ = dogs;                 // killough 7/19/98

  *demo_p++ = 0;
  *demo_p++ = 0;

  *demo_p++ = (distfriend >> 8) & 0xff;  // killough 8/8/98
  *demo_p++ =  distfriend       & 0xff;  // killough 8/8/98

  *demo_p++ = monster_backing;         // killough 9/8/98

  *demo_p++ = monster_avoid_hazards;    // killough 9/9/98

  *demo_p++ = monster_friction;         // killough 10/98

  *demo_p++ = help_friends;             // killough 9/9/98

  *demo_p++ = dog_jumping;

  *demo_p++ = monkeys;

  {   // killough 10/98: a compatibility vector now
    int i;
    for (i = 0; i < MBF_COMP_TOTAL; i++)
      *demo_p++ = comp[i] != 0;
  }

  *demo_p++ = (compatibility_level >= prboom_2_compatibility) && forceOldBsp; // cph 2002/07/20

  //----------------
  // Padding at end
  //----------------
  while (demo_p < target)
    *demo_p++ = 0;

  if (demo_p != target)
    I_Error("G_WriteOptions: dsda_GameOptionSize is too small");

  return target;
}

/* Same, but read instead of write
 * cph - const byte*'s
 */

const byte *G_ReadOptions(const byte *demo_p)
{
  const byte *target;

  if (mbf21)
  {
    return dsda_ReadOptions21(demo_p);
  }

  target = demo_p + dsda_GameOptionSize();

  monsters_remember = *demo_p++;

  variable_friction = *demo_p;  // ice & mud
  demo_p++;

  weapon_recoil = *demo_p;       // weapon recoil
  demo_p++;

  allow_pushers = *demo_p;      // MT_PUSH Things
  demo_p++;

  demo_p++;

  player_bobbing = *demo_p;     // whether player bobs or not
  demo_p++;

  // killough 3/6/98: add parameters to savegame, move from demo
  respawnparm = *demo_p++;
  fastparm = *demo_p++;
  nomonsters = *demo_p++;

  demo_insurance = *demo_p++;              // killough 3/31/98

  // killough 3/26/98: Added rngseed to demos; 3/31/98: moved here

  rngseed  = *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;
  rngseed <<= 8;
  rngseed += *demo_p++ & 0xff;

  // Options new to v2.03
  if (mbf_features)
  {
    monster_infighting = *demo_p++;   // killough 7/19/98

    dogs = *demo_p++;                 // killough 7/19/98

    demo_p += 2;

    distfriend = *demo_p++ << 8;      // killough 8/8/98
    distfriend+= *demo_p++;

    monster_backing = *demo_p++;     // killough 9/8/98

    monster_avoid_hazards = *demo_p++; // killough 9/9/98

    monster_friction = *demo_p++;      // killough 10/98

    help_friends = *demo_p++;          // killough 9/9/98

    dog_jumping = *demo_p++;           // killough 10/98

    monkeys = *demo_p++;

    {   // killough 10/98: a compatibility vector now
      int i;
      for (i = 0; i < MBF_COMP_TOTAL; i++)
        comp[i] = *demo_p++;
    }

    forceOldBsp = *demo_p++; // cph 2002/07/20
  }
  else  /* defaults for versions <= 2.02 */
  {
    /* G_Compatibility will set these */
  }

  G_Compatibility();

  return target;
}

void G_BeginRecording (void)
{
  int i;
  byte *demostart, *demo_p;
  demostart = demo_p = malloc(1000);
  longtics = 0;

  dsda_ResetDemoSaveSlots();
  dsda_ApplyDSDADemoFormat(&demo_p);

  /* cph - 3 demo record formats supported: MBF+, BOOM, and Doom v1.9 */
  if (mbf_features) {
    { /* Write version code into demo */
      unsigned char v = 0;
      switch(compatibility_level) {
        case mbf_compatibility: v = 203; break; // e6y: Bug in MBF compatibility mode fixed
        case prboom_2_compatibility: v = 210; break;
        case prboom_3_compatibility: v = 211; break;
        case prboom_4_compatibility: v = 212; break;
        case prboom_5_compatibility: v = 213; break;
        case prboom_6_compatibility:
             v = 214;
             longtics = 1;
             break;
        case mbf21_compatibility:
             v = 221;
             longtics = 1;
             shorttics = !M_CheckParm("-longtics");
             break;
        default: I_Error("G_BeginRecording: PrBoom compatibility level unrecognised?");
      }
      *demo_p++ = v;
    }

    // signature
    *demo_p++ = 0x1d;
    *demo_p++ = 'M';
    *demo_p++ = 'B';
    *demo_p++ = 'F';
    *demo_p++ = 0xe6;
    *demo_p++ = '\0';

    if (!mbf21)
    {
      // boom compatibility mode flag, which has no meaning in mbf+
      *demo_p++ = 0;
    }

    *demo_p++ = gameskill;
    *demo_p++ = gameepisode;
    *demo_p++ = gamemap;
    *demo_p++ = deathmatch;
    *demo_p++ = consoleplayer;

    demo_p = G_WriteOptions(demo_p); // killough 3/1/98: Save game options

    for (i = 0; i < g_maxplayers; i++)
      *demo_p++ = playeringame[i];

    // killough 2/28/98:
    // We always store at least FUTURE_MAXPLAYERS bytes in demo, to
    // support enhancements later w/o losing demo compatibility

    for (; i < FUTURE_MAXPLAYERS; i++)
      *demo_p++ = 0;

  // FIXME } else if (compatibility_level >= boom_compatibility_compatibility) { //e6y
  } else if (compatibility_level > boom_compatibility_compatibility) {
    byte v = 0, c = 0; /* Nominally, version and compatibility bits */
    switch (compatibility_level) {
    case boom_compatibility_compatibility: v = 202, c = 1; break;
    case boom_201_compatibility: v = 201; c = 0; break;
    case boom_202_compatibility: v = 202, c = 0; break;
    default: I_Error("G_BeginRecording: Boom compatibility level unrecognised?");
    }
    *demo_p++ = v;

    // signature
    *demo_p++ = 0x1d;
    *demo_p++ = 'B';
    *demo_p++ = 'o';
    *demo_p++ = 'o';
    *demo_p++ = 'm';
    *demo_p++ = 0xe6;

    /* CPhipps - save compatibility level in demos */
    *demo_p++ = c;

    *demo_p++ = gameskill;
    *demo_p++ = gameepisode;
    *demo_p++ = gamemap;
    *demo_p++ = deathmatch;
    *demo_p++ = consoleplayer;

    demo_p = G_WriteOptions(demo_p); // killough 3/1/98: Save game options

    for (i = 0; i < g_maxplayers; i++)
      *demo_p++ = playeringame[i];

    // killough 2/28/98:
    // We always store at least FUTURE_MAXPLAYERS bytes in demo, to
    // support enhancements later w/o losing demo compatibility

    for (; i < FUTURE_MAXPLAYERS; i++)
      *demo_p++ = 0;
  } else if (!raven) { // cph - write old v1.9 demos (might even sync)
    unsigned char v = 109;
    longtics = M_CheckParm("-longtics");
    if (longtics)
    {
      v = 111;
    }
    else
    {
      switch (compatibility_level)
      {
      case doom_1666_compatibility:
        v = 106;
        break;
      case tasdoom_compatibility:
        v = 110;
        break;
      }
    }
    *demo_p++ = v;
    *demo_p++ = gameskill;
    *demo_p++ = gameepisode;
    *demo_p++ = gamemap;
    *demo_p++ = deathmatch;
    *demo_p++ = respawnparm;
    *demo_p++ = fastparm;
    *demo_p++ = nomonsters;
    *demo_p++ = consoleplayer;
    for (i=0; i<4; i++)  // intentionally hard-coded 4 -- killough
      *demo_p++ = playeringame[i];
  } else { // versionless raven
    *demo_p++ = gameskill;
    *demo_p++ = gameepisode;
    *demo_p++ = gamemap;

    // Write special parameter bits onto player one byte.
    // This aligns with vvHeretic demo usage:
    //   0x20 = -respawn
    //   0x10 = -longtics
    //   0x02 = -nomonsters
    *demo_p = 1; // assume player one exists
    if (respawnparm)
      *demo_p |= DEMOHEADER_RESPAWN;
    if (longtics)
      *demo_p |= DEMOHEADER_LONGTICS;
    if (nomonsters)
      *demo_p |= DEMOHEADER_NOMONSTERS;
    demo_p++;

    if (heretic)
    {
      for (i = 1; i < g_maxplayers; i++)
        *demo_p++ = playeringame[i];
    }
    else
    {
      *demo_p++ = PlayerClass[0] - 1;
      for (i = 1; i < g_maxplayers; i++)
      {
        *demo_p++ = playeringame[i];
        *demo_p++ = PlayerClass[i] - 1;
      }
    }
  }

  dsda_EvaluateBytesPerTic();

  dsda_WriteToDemo(demostart, demo_p - demostart);
  dsda_ContinueKeyFrame();
  dsda_ResetSplits();

  R_DemoEx_ResetMLook();

  free(demostart);
}

//
// G_PlayDemo
//

static const char *defdemoname;

void G_DeferedPlayDemo (const char* name)
{
  defdemoname = name;
  gameaction = ga_playdemo;
}

static int demolumpnum = -1;

static int G_GetOriginalDoomCompatLevel(int ver)
{
  int level;

  level = dsda_CompatibilityLevel();
  if (level >= 0) return level;

  if (ver == 110) return tasdoom_compatibility;
  if (ver < 107) return doom_1666_compatibility;
  if (gamemode == retail) return ultdoom_compatibility;
  if (gamemission == pack_tnt || gamemission == pack_plut) return finaldoom_compatibility;
  return doom2_19_compatibility;
}

//e6y: Check for overrun
static dboolean CheckForOverrun(const byte *start_p, const byte *current_p, size_t maxsize, size_t size, dboolean failonerror)
{
  size_t pos = current_p - start_p;
  if (pos + size > maxsize)
  {
    if (failonerror)
      I_Error("G_ReadDemoHeader: wrong demo header\n");
    else
      return true;
  }
  return false;
}

const byte* G_ReadDemoHeaderEx(const byte *demo_p, size_t size, unsigned int params)
{
  skill_t skill;
  int i, episode = 1, map = 0;

  // e6y
  // The local variable should be used instead of demobuffer,
  // because demobuffer can be uninitialized
  const byte *header_p = demo_p;

  dboolean failonerror = (params&RDH_SAFE);

  basetic = gametic;  // killough 9/29/98

  // killough 2/22/98, 2/28/98: autodetect old demos and act accordingly.
  // Old demos turn on demo_compatibility => compatibility; new demos load
  // compatibility flag, and other flags as well, as a part of the demo.

  //e6y: check for overrun
  if (CheckForOverrun(header_p, demo_p, size, 1, failonerror))
    return NULL;

  dsda_DisableExCmd();

  demover = *demo_p++;
  longtics = 0;

  // defunct extended header or unknown (e.g., eternity)
  if (demover == 255)
  {
    demo_p = dsda_StripDemoVersion255(demo_p, header_p, size);

    if (!demo_p)
    {
      if (failonerror)
      {
        I_Error("G_ReadDemoHeader: wrong demo header\n");
      }
      else
      {
        return NULL;
      }
    }

    // update the start of the demo header
    size -= demo_p - header_p;
    header_p = demo_p;

    if (CheckForOverrun(header_p, demo_p, size, 1, failonerror))
      return NULL;

    demover = *demo_p++;
  }

  // e6y
  // Handling of unrecognized demo formats
  // Versions up to 1.2 use a 7-byte header - first byte is a skill level.
  // Versions after 1.2 use a 13-byte header - first byte is a demoversion.
  // BOOM's demoversion starts from 200
  if (!((demover >=   0  && demover <=   4) ||
        (demover >= 104  && demover <= 111) ||
        (demover >= 200  && demover <= 214) ||
        (demover == 221)))
  {
    I_Error("G_ReadDemoHeader: Unknown demo format %d.", demover);
  }

  if (demover < 200)     // Autodetect old demos
  {
    if (demover >= 111) longtics = 1;

    // killough 3/2/98: force these variables to be 0 in demo_compatibility

    variable_friction = 0;

    weapon_recoil = 0;

    allow_pushers = 0;

    monster_infighting = 1;           // killough 7/19/98

    dogs = 0;                         // killough 7/19/98
    dog_jumping = 0;                  // killough 10/98

    monster_backing = 0;              // killough 9/8/98

    monster_avoid_hazards = 0;        // killough 9/9/98

    monster_friction = 0;             // killough 10/98
    help_friends = 0;                 // killough 9/9/98
    monkeys = 0;

    // killough 3/6/98: rearrange to fix savegame bugs (moved fastparm,
    // respawnparm, nomonsters flags to G_LoadOptions()/G_SaveOptions())

    if ((skill = demover) >= 100)         // For demos from versions >= 1.4
    {
      //e6y: check for overrun
      if (CheckForOverrun(header_p, demo_p, size, 8, failonerror))
        return NULL;

      compatibility_level = G_GetOriginalDoomCompatLevel(demover);
      skill = *demo_p++;
      episode = *demo_p++;
      map = *demo_p++;
      deathmatch = *demo_p++;
      respawnparm = *demo_p++;
      fastparm = *demo_p++;
      nomonsters = *demo_p++;
      consoleplayer = *demo_p++;
    }
    else
    {
      //e6y: check for overrun
      if (CheckForOverrun(header_p, demo_p, size, 2, failonerror))
        return NULL;

      compatibility_level = doom_12_compatibility;
      episode = *demo_p++;
      map = *demo_p++;
      deathmatch = respawnparm = fastparm =
        nomonsters = consoleplayer = 0;

      // e6y
      // Ability to force -nomonsters and -respawn for playback of 1.2 demos.
      // Demos recorded with Doom.exe 1.2 did not contain any information
      // about whether these parameters had been used. In order to play them
      // back, you should add them to the command-line for playback.
      // There is no more desynch on mesh.lmp @ mesh.wad
      // prboom -iwad doom.wad -file mesh.wad -playdemo mesh.lmp -nomonsters
      // http://www.doomworld.com/idgames/index.php?id=13976
      respawnparm = M_CheckParm("-respawn");
      fastparm = M_CheckParm("-fast");
      nomonsters = M_CheckParm("-nomonsters");

      // Read special parameter bits from player one byte.
      // This aligns with vvHeretic demo usage:
      //   0x20 = -respawn
      //   0x10 = -longtics
      //   0x02 = -nomonsters
      if (raven)
      {
        if (*demo_p & DEMOHEADER_RESPAWN)
          respawnparm = true;
        if (*demo_p & DEMOHEADER_LONGTICS || M_CheckParm("-longtics"))
          longtics = true;
        if (*demo_p & DEMOHEADER_NOMONSTERS)
          nomonsters = true;
      }

      // e6y: detection of more unsupported demo formats
      if (*(header_p + size - 1) == DEMOMARKER)
      {
        // file size test;
        // DOOM_old and HERETIC don't use maps>9;
        // 2 at 4,6 means playerclass=mage -> not DOOM_old or HERETIC;
        if ((size >= 8 && (size - 8) % 4 != 0 && !raven) ||
            (map > 9 && !hexen) ||
            (size >= 6 && (*(header_p + 4) == 2 || *(header_p + 6) == 2) && !hexen))
        {
          I_Error("Unrecognised demo format.");
        }
      }

    }
    G_Compatibility();
  }
  else    // new versions of demos
  {
    demo_p += 6;               // skip signature;
    switch (demover) {
      case 200: /* BOOM */
      case 201:
        //e6y: check for overrun
        if (CheckForOverrun(header_p, demo_p, size, 1, failonerror))
          return NULL;

        if (!*demo_p++)
          compatibility_level = boom_201_compatibility;
        else
          compatibility_level = boom_compatibility_compatibility;
        break;
      case 202:
        //e6y: check for overrun
        if (CheckForOverrun(header_p, demo_p, size, 1, failonerror))
          return NULL;

        if (!*demo_p++)
          compatibility_level = boom_202_compatibility;
        else
          compatibility_level = boom_compatibility_compatibility;
        break;
      case 203:
        /* LxDoom or MBF - determine from signature
         * cph - load compatibility level */
        switch (*(header_p + 2)) {
        case 'B': /* LxDoom */
          /* cph - DEMOSYNC - LxDoom demos recorded in compatibility modes support dropped */
          compatibility_level = lxdoom_1_compatibility;
          break;
        case 'M':
          compatibility_level = mbf_compatibility;
          demo_p++;
          break;
        }
        break;
      case 210:
        compatibility_level = prboom_2_compatibility;
        demo_p++;
        break;
      case 211:
        compatibility_level = prboom_3_compatibility;
        demo_p++;
        break;
      case 212:
        compatibility_level = prboom_4_compatibility;
        demo_p++;
        break;
      case 213:
        compatibility_level = prboom_5_compatibility;
        demo_p++;
        break;
      case 214:
        compatibility_level = prboom_6_compatibility;
              longtics = 1;
        demo_p++;
        break;
      case 221:
        compatibility_level = mbf21_compatibility;
        longtics = 1;
        break;
    }
    //e6y: check for overrun
    if (CheckForOverrun(header_p, demo_p, size, 5, failonerror))
      return NULL;

    skill = *demo_p++;
    episode = *demo_p++;
    map = *demo_p++;
    deathmatch = *demo_p++;
    consoleplayer = *demo_p++;

    //e6y: check for overrun
    if (CheckForOverrun(header_p, demo_p, size, dsda_GameOptionSize(), failonerror))
      return NULL;

    demo_p = G_ReadOptions(demo_p);  // killough 3/1/98: Read game options

    if (demover == 200)              // killough 6/3/98: partially fix v2.00 demos
      demo_p += 256 - dsda_GameOptionSize();
  }

  if (sizeof(comp_lev_str)/sizeof(comp_lev_str[0]) != MAX_COMPATIBILITY_LEVEL)
    I_Error("G_ReadDemoHeader: compatibility level strings incomplete");
  lprintf(LO_INFO, "G_DoPlayDemo: playing demo with %s compatibility\n",
    comp_lev_str[compatibility_level]);

  for (i = 0; i < g_maxplayers; i++)
    playeringame[i] = 0;

  if (demo_compatibility || demover < 200) //e6y  // only 4 players can exist in old demos
  {
    if (hexen)
    {
      //e6y: check for overrun
      if (CheckForOverrun(header_p, demo_p, size, g_maxplayers, failonerror))
        return NULL;

      for (i = 0; i < g_maxplayers; i++)
      {
        playeringame[i] = (*demo_p++) != 0;
        PlayerClass[i] = *demo_p++ + 1;
      }
    }
    else
    {
      //e6y: check for overrun
      if (CheckForOverrun(header_p, demo_p, size, g_maxplayers, failonerror))
        return NULL;

      for (i = 0; i < g_maxplayers; i++)
        playeringame[i] = *demo_p++;
    }
  }
  else
  {
    //e6y: check for overrun
    if (CheckForOverrun(header_p, demo_p, size, g_maxplayers, failonerror))
      return NULL;

    for (i=0 ; i < g_maxplayers; i++)
      playeringame[i] = *demo_p++;
    demo_p += FUTURE_MAXPLAYERS - g_maxplayers;
  }

  {
    int p = M_CheckParm("-consoleplayer");

    if (p && (p + 1 < myargc)) {
      consoleplayer = atoi(myargv[p + 1]);

      if (consoleplayer < 0 || consoleplayer >= g_maxplayers || !playeringame[consoleplayer])
        consoleplayer = 0;
    }
  }

  if (playeringame[1])
  {
    netgame = true;
    netdemo = true;
  }

  if (!(params & RDH_SKIP_HEADER))
  {
    G_InitNew(skill, episode, map, true);
  }

  for (i = 0; i < g_maxplayers; i++)         // killough 4/24/98
    players[i].cheats = 0;

  // e6y
  // additional params
  {
    const byte *p = demo_p;

    dsda_EvaluateBytesPerTic();

    demo_playerscount = 0;
    demo_tics_count = 0;
    strcpy(demo_len_st, "-");

    for (i = 0; i < g_maxplayers; i++)
    {
      if (playeringame[i])
      {
        demo_playerscount++;
      }
    }

    if (demo_playerscount > 0 && demolength > 0)
    {
      demo_tics_count = dsda_DemoTicsCount(p, demobuffer, demolength);

      sprintf(demo_len_st, "\x1b\x35/%d:%02d",
        demo_tics_count / TICRATE / 60,
        (demo_tics_count % (60 * TICRATE)) / TICRATE);
    }
  }

  return demo_p;
}

void G_StartDemoPlayback(const byte *buffer, int length, int behaviour)
{
  const byte *demo_p;

  demo_p = G_ReadDemoHeaderEx(demobuffer, demolength, RDH_SAFE);
  dsda_AttachPlaybackStream(demo_p, demolength, behaviour);

  R_SmoothPlaying_Reset(NULL); // e6y
}

void G_DoPlayDemo(void)
{
  if (LoadDemo(defdemoname, &demobuffer, &demolength, &demolumpnum))
  {
    G_StartDemoPlayback(demobuffer, demolength, PLAYBACK_NORMAL);

    gameaction = ga_nothing;
  }
  else
  {
    // e6y
    // Do not exit if corresponding demo lump is not found.
    // It makes sense for Plutonia and TNT IWADs, which have no DEMO4 lump,
    // but DEMO4 should be in a demo cycle as real Plutonia and TNT have.
    //
    // Plutonia/Tnt executables exit with "W_GetNumForName: DEMO4 not found"
    // message after playing of DEMO3, because DEMO4 is not present
    // in the corresponding IWADs.
    D_StartTitle();                // Start the title screen
    gamestate = GS_DEMOSCREEN;     // And set the game state accordingly
  }
}

/* G_CheckDemoStatus
 *
 * Called after a death or level completion to allow demos to be cleaned up
 * Returns true if a new demo loop action will take place
 */
dboolean G_CheckDemoStatus (void)
{
  dsda_EvaluateSkipModeCheckDemoStatus();

  if (demorecording)
  {
    dsda_EndDemoRecording();

    return false;  // killough
  }

  if (timingdemo)
  {
    int endtime = dsda_GetTickRealTime();
    // killough -- added fps information and made it work for longer demos:
    unsigned realtics = endtime-starttime;

    M_SaveDefaults();

    lprintf(LO_INFO, "Timed %u gametics in %u realtics = %-.1f frames per second\n",
             (unsigned) gametic,realtics,
             (unsigned) gametic * (double) TICRATE / realtics);
    I_SafeExit(0);
  }

  if (demoplayback)
  {
    if (singledemo)
      I_SafeExit(0);  // killough

    if (demolumpnum != -1) {
      // cph - unlock the demo lump
      W_UnlockLumpNum(demolumpnum);
      demolumpnum = -1;
    }
    G_ReloadDefaults();    // killough 3/1/98
    netgame = false;       // killough 3/29/98
    deathmatch = false;
    D_AdvanceDemo ();
    return true;
  }
  return false;
}

// killough 1/22/98: this is a "Doom printf" for messages. I've gotten
// tired of using players->message=... and so I've added this dprintf.
//
// killough 3/6/98: Made limit static to allow z_zone functions to call
// this function, without calling realloc(), which seems to cause problems.

#define MAX_MESSAGE_SIZE 1024

// CPhipps - renamed to doom_printf to avoid name collision with glibc
void doom_printf(const char *s, ...)
{
  static char msg[MAX_MESSAGE_SIZE];
  va_list v;
  va_start(v,s);
  doom_vsnprintf(msg,sizeof(msg),s,v);   /* print message in buffer */
  va_end(v);
  players[consoleplayer].message = msg;  // set new message
}

//e6y
void P_WalkTicker()
{
  int strafe;
  int speed;
  int tspeed;
  int turnheld;
  int forward;
  int side;
  int angturn;

  if (!walkcamera.type || menuactive)
    return;

  G_SetSpeed(false);

  strafe = dsda_InputActive(dsda_input_strafe);
  speed = dsda_AutoRun() || dsda_InputActive(dsda_input_speed); // phares

  forward = side = 0;
  angturn = 0;
  turnheld = 0;

    // use two stage accelerative turning
    // on the keyboard and joystick
  if (joyxmove != 0 ||
      dsda_InputActive(dsda_input_turnright) ||
      dsda_InputActive(dsda_input_turnleft))
    ++turnheld;
  else
    turnheld = 0;

  if (turnheld < SLOWTURNTICS)
    tspeed = 0;             // slow turn
  else
    tspeed = speed;                                                             // phares

  // let movement keys cancel each other out

  if (strafe)
    {
      if (dsda_InputActive(dsda_input_turnright))
        side += sidemove[speed];
      if (dsda_InputActive(dsda_input_turnleft))
        side -= sidemove[speed];
      if (joyxmove > 0)
        side += sidemove[speed];
      if (joyxmove < 0)
        side -= sidemove[speed];
    }
  else
    {
      if (dsda_InputActive(dsda_input_turnright))
        angturn -= angleturn[tspeed];
      if (dsda_InputActive(dsda_input_turnleft))
        angturn += angleturn[tspeed];
      if (joyxmove > 0)
        angturn -= angleturn[tspeed];
      if (joyxmove < 0)
        angturn += angleturn[tspeed];
    }

  if (dsda_InputActive(dsda_input_forward))
    forward += forwardmove[speed];
  if (dsda_InputActive(dsda_input_backward))
    forward -= forwardmove[speed];
  if (joyymove < 0)
    forward += forwardmove[speed];
  if (joyymove > 0)
    forward -= forwardmove[speed];
  if (dsda_InputActive(dsda_input_straferight))
    side += sidemove[speed];
  if (dsda_InputActive(dsda_input_strafeleft))
    side -= sidemove[speed];

  forward += mousey;
  if (strafe)
    side += mousex / 4;       /* mead  Don't want to strafe as fast as turns.*/
  else
    angturn -= mousex; /* mead now have enough dynamic range 2-10-00 */

  walkcamera.angle += ((angturn / 8) << ANGLETOFINESHIFT);
  if (dsda_MouseLook())
  {
    walkcamera.pitch += ((mlooky / 8) << ANGLETOFINESHIFT);
    CheckPitch((signed int *) &walkcamera.pitch);
  }

  if (dsda_InputActive(dsda_input_fire))
  {
    walkcamera.x = players[0].mo->x;
    walkcamera.y = players[0].mo->y;
    walkcamera.angle = players[0].mo->angle;
    walkcamera.pitch = P_PlayerPitch(&players[0]);
  }

  if (forward > MAXPLMOVE)
    forward = MAXPLMOVE;
  else if (forward < -MAXPLMOVE)
    forward = -MAXPLMOVE;
  if (side > MAXPLMOVE)
    side = MAXPLMOVE;
  else if (side < -MAXPLMOVE)
    side = -MAXPLMOVE;

  // moving forward
  walkcamera.x += FixedMul ((ORIG_FRICTION / 4) * forward,
          finecosine[walkcamera.angle >> ANGLETOFINESHIFT]);
  walkcamera.y += FixedMul ((ORIG_FRICTION / 4) * forward,
          finesine[walkcamera.angle >> ANGLETOFINESHIFT]);

  // strafing
  walkcamera.x += FixedMul ((ORIG_FRICTION / 6) * side,
          finecosine[(walkcamera.angle -
          ANG90) >> ANGLETOFINESHIFT]);
  walkcamera.y += FixedMul ((ORIG_FRICTION / 6) * side,
        finesine[(walkcamera.angle - ANG90) >> ANGLETOFINESHIFT]);

  {
    subsector_t *subsec = R_PointInSubsector (walkcamera.x, walkcamera.y);
    walkcamera.z = subsec->sector->floorheight + 41 * FRACUNIT;
  }

  mousex = mousey = 0;
}

void P_ResetWalkcam(void)
{
  if (walkcamera.type)
  {
    walkcamera.PrevX = walkcamera.x;
    walkcamera.PrevY = walkcamera.y;
    walkcamera.PrevZ = walkcamera.z;
    walkcamera.PrevAngle = walkcamera.angle;
    walkcamera.PrevPitch = walkcamera.pitch;
  }
}

void P_SyncWalkcam(dboolean sync_coords, dboolean sync_sight)
{
  if (!walkcamera.type)
    return;

  if (players[displayplayer].mo)
  {
    if (sync_sight)
    {
      walkcamera.angle = players[displayplayer].mo->angle;
      walkcamera.pitch = P_PlayerPitch(&players[displayplayer]);
    }

    if(sync_coords)
    {
      walkcamera.x = players[displayplayer].mo->x;
      walkcamera.y = players[displayplayer].mo->y;
    }
  }
}

//e6y
void G_ContinueDemo(const char *playback_name)
{
  const byte *demo_p;

  if (LoadDemo(playback_name, &demobuffer, &demolength, &demolumpnum))
  {
    G_StartDemoPlayback(demobuffer, demolength, PLAYBACK_JOIN_ON_END);

    singledemo = true;
    autostart = true;
    dsda_InitDemoRecording();
    G_BeginRecording();
  }
}

// heretic

static dboolean InventoryMoveLeft(void)
{
    inventoryTics = 5 * 35;
    if (!inventory)
    {
        inventory = true;
        return false;
    }
    inv_ptr--;
    if (inv_ptr < 0)
    {
        inv_ptr = 0;
    }
    else
    {
        curpos--;
        if (curpos < 0)
        {
            curpos = 0;
        }
    }
    return true;
}

static dboolean InventoryMoveRight(void)
{
    player_t *plr;

    plr = &players[consoleplayer];
    inventoryTics = 5 * 35;
    if (!inventory)
    {
        inventory = true;
        return false;
    }
    inv_ptr++;
    if (inv_ptr >= plr->inventorySlotNum)
    {
        inv_ptr--;
        if (inv_ptr < 0)
            inv_ptr = 0;
    }
    else
    {
        curpos++;
        if (curpos > 6)
        {
            curpos = 6;
        }
    }
    return true;
}

// hexen

void G_Completed(int map, int position)
{
    if (gamemode == shareware && map > 4)
    {
        P_SetMessage(&players[consoleplayer], "ACCESS DENIED -- DEMO", true);
        S_StartSound(NULL, hexen_sfx_chat);
        return;
    }

    gameaction = ga_completed;
    LeaveMap = map;
    LeavePosition = position;
}

void G_TeleportNewMap(int map, int position)
{
    gameaction = ga_leavemap;
    LeaveMap = map;
    LeavePosition = position;
}

void G_DoTeleportNewMap(void)
{
    SV_MapTeleport(LeaveMap, LeavePosition);
    gamestate = GS_LEVEL;
    gameaction = ga_nothing;
    RebornPosition = LeavePosition;
    dsda_EvaluateSkipModeDoTeleportNewMap();
}

void G_PlayerExitMap(int playerNumber)
{
    int i;
    player_t *player;
    int flightPower;

    player = &players[playerNumber];

    // Strip all current powers (retain flight)
    flightPower = player->powers[pw_flight];
    memset(player->powers, 0, sizeof(player->powers));
    player->powers[pw_flight] = flightPower;

    if (deathmatch)
    {
        player->powers[pw_flight] = 0;
    }
    else
    {
        if (dsda_MapCluster(gamemap) != dsda_MapCluster(LeaveMap))
        {                       // Entering new cluster
            // Strip all keys
            for (i = 0; i < NUMCARDS; ++i)
              player->cards[i] = 0;

            // Strip flight artifact
            for (i = 0; i < 25; i++)
            {
                player->powers[pw_flight] = 0;
                P_PlayerUseArtifact(player, hexen_arti_fly);
            }
            player->powers[pw_flight] = 0;
        }
    }

    if (player->morphTics)
    {
        player->readyweapon = player->mo->special1.i;     // Restore weapon
        player->morphTics = 0;
    }
    player->lookdir = 0;
    player->mo->flags &= ~MF_SHADOW;    // Remove invisibility
    player->extralight = 0;     // Remove weapon flashes
    player->fixedcolormap = 0;  // Remove torch
    player->damagecount = 0;    // No palette changes
    player->bonuscount = 0;
    player->poisoncount = 0;
    if (player == &players[consoleplayer])
    {
        SB_Start();          // refresh the status bar
        viewangleoffset = 0;
    }
}

static void Hexen_G_DoCompleted(void)
{
    int i;

    totalleveltimes = players[consoleplayer].worldTimer;

    gameaction = ga_nothing;

    for (i = 0; i < g_maxplayers; i++)
    {
        if (playeringame[i])
        {
            G_PlayerExitMap(i);
        }
    }

    if (automapmode & am_active)
      AM_Stop();

    e6y_G_DoCompleted();
    dsda_WatchLevelCompletion();

    if (LeaveMap == -1 && LeavePosition == -1)
    {
        gameaction = ga_victory;
        return;
    }
    else
    {
        gamestate = GS_INTERMISSION;
        WI_Start(&wminfo);
    }
}

void Hexen_G_DoReborn(int playernum)
{
    int i;
    dboolean oldWeaponowned[HEXEN_NUMWEAPONS];
    dboolean oldKeys[NUMCARDS];
    int oldPieces;
    dboolean foundSpot;
    int bestWeapon;

    if (!netgame)
    {
        gameaction = ga_loadlevel;
    }
    else
    {                           // Net-game
        players[playernum].mo->player = NULL;   // Dissassociate the corpse

        if (deathmatch)
        {                       // Spawn at random spot if in death match
            G_DeathMatchSpawnPlayer(playernum);
            return;
        }

        // Cooperative net-play, retain keys and weapons
        for (i = 0; i < NUMCARDS; ++i)
          oldKeys[i] = players[playernum].cards[i];
        oldPieces = players[playernum].pieces;
        for (i = 0; i < HEXEN_NUMWEAPONS; i++)
        {
            oldWeaponowned[i] = players[playernum].weaponowned[i];
        }

        foundSpot = false;
        if (G_CheckSpot(playernum, &playerstarts[RebornPosition][playernum]))
        {                       // Appropriate player start spot is open
            P_SpawnPlayer(playernum, &playerstarts[RebornPosition][playernum]);
            foundSpot = true;
        }
        else
        {
            // Try to spawn at one of the other player start spots
            for (i = 0; i < g_maxplayers; i++)
            {
                if (G_CheckSpot(playernum, &playerstarts[RebornPosition][i]))
                {               // Found an open start spot

                    // Fake as other player
                    playerstarts[RebornPosition][i].type = playernum + 1;
                    P_SpawnPlayer(playernum, &playerstarts[RebornPosition][i]);

                    // Restore proper player type
                    playerstarts[RebornPosition][i].type = i + 1;

                    foundSpot = true;
                    break;
                }
            }
        }

        if (foundSpot == false)
        {                       // Player's going to be inside something
            P_SpawnPlayer(playernum, &playerstarts[RebornPosition][playernum]);
        }

        // Restore keys and weapons
        for (i = 0; i < NUMCARDS; ++i)
          players[playernum].cards[i] = oldKeys[i];
        players[playernum].pieces = oldPieces;
        for (bestWeapon = 0, i = 0; i < HEXEN_NUMWEAPONS; i++)
        {
            if (oldWeaponowned[i])
            {
                bestWeapon = i;
                players[playernum].weaponowned[i] = true;
            }
        }
        players[playernum].ammo[MANA_1] = 25;
        players[playernum].ammo[MANA_2] = 25;
        if (bestWeapon)
        {                       // Bring up the best weapon
            players[playernum].pendingweapon = bestWeapon;
        }
    }
}
