/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2002 by
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
 * DESCRIPTION:
 *      Cheat sequence checking.
 *
 *-----------------------------------------------------------------------------*/

#include "doomstat.h"
#include "am_map.h"
#include "g_game.h"
#include "r_data.h"
#include "p_inter.h"
#include "p_tick.h"
#include "m_cheat.h"
#include "m_argv.h"
#include "s_sound.h"
#include "sounds.h"
#include "dstrings.h"
#include "r_main.h"
#include "p_map.h"
#include "d_deh.h"  // Ty 03/27/98 - externalized strings
/* cph 2006/07/23 - needs direct access to thinkercap */
#include "p_tick.h"
#include "w_wad.h"
#include "p_setup.h"
#include "lprintf.h"

#include "heretic/def.h"
#include "heretic/sb_bar.h"

#include "dsda/excmd.h"
#include "dsda/input.h"
#include "dsda/map_format.h"
#include "dsda/mapinfo.h"
#include "dsda/settings.h"

#define plyr (players+consoleplayer)     /* the console player */

//e6y: for speedup
static int boom_cheat_route[MAX_COMPATIBILITY_LEVEL];

//-----------------------------------------------------------------------------
//
// CHEAT SEQUENCE PACKAGE
//
//-----------------------------------------------------------------------------

static void cheat_mus();
static void cheat_choppers();
static void cheat_god();
static void cheat_fa();
static void cheat_k();
static void cheat_kfa();
static void cheat_noclip();
static void cheat_pw();
static void cheat_behold();
static void cheat_clev();
static void cheat_mypos();
static void cheat_rate();
static void cheat_comp();
static void cheat_friction();
static void cheat_pushers();
static void cheat_massacre();
static void cheat_ddt();
static void cheat_reveal_secret();
static void cheat_reveal_kill();
static void cheat_reveal_item();
static void cheat_hom();
static void cheat_fast();
static void cheat_tntkey();
static void cheat_tntkeyx();
static void cheat_tntkeyxx();
static void cheat_tntweap();
static void cheat_tntweapx();
static void cheat_tntammo();
static void cheat_tntammox();
static void cheat_smart();
static void cheat_pitch();
static void cheat_megaarmour();
static void cheat_health();
static void cheat_notarget();
static void cheat_fly();

// heretic
static void cheat_reset_health();
static void cheat_tome();
static void cheat_chicken();
static void cheat_artifact();

// hexen
static void cheat_inventory();
static void cheat_puzzle();
static void cheat_class();
static void cheat_init();
static void cheat_script();

//-----------------------------------------------------------------------------
//
// List of cheat codes, functions, and special argument indicators.
//
// The first argument is the cheat code.
//
// The second argument is its DEH name, or NULL if it's not supported by -deh.
//
// The third argument is a combination of the bitmasks,
// which excludes the cheat during certain modes of play.
//
// The fourth argument is the handler function.
//
// The fifth argument is passed to the handler function if it's non-negative;
// if negative, then its negative indicates the number of extra characters
// expected after the cheat code, which are passed to the handler function
// via a pointer to a buffer (after folding any letters to lowercase).
//
//-----------------------------------------------------------------------------

cheatseq_t cheat[] = {
  CHEAT("idmus",      "Change music",     always, cheat_mus, -2, false),
  CHEAT("idchoppers", "Chainsaw",         cht_never, cheat_choppers, 0, false),
  CHEAT("iddqd",      "God mode",         cht_dsda,  cheat_god, 0, false),
  CHEAT("idkfa",      "Ammo & Keys",      cht_never, cheat_kfa, 0, false),
  CHEAT("idfa",       "Ammo",             cht_never, cheat_fa, 0, false),
  CHEAT("idspispopd", "No Clipping 1",    cht_dsda,  cheat_noclip, 0, false),
  CHEAT("idclip",     "No Clipping 2",    cht_dsda,  cheat_noclip, 0, false),
  CHEAT("idbeholdh",  "Invincibility",    cht_never, cheat_health, 0, false),
  CHEAT("idbeholdm",  "Invincibility",    cht_never, cheat_megaarmour, 0, false),
  CHEAT("idbeholdv",  "Invincibility",    cht_never, cheat_pw, pw_invulnerability, false),
  CHEAT("idbeholds",  "Berserk",          cht_never, cheat_pw, pw_strength, false),
  CHEAT("idbeholdi",  "Invisibility",     cht_never, cheat_pw, pw_invisibility, false),
  CHEAT("idbeholdr",  "Radiation Suit",   cht_never, cheat_pw, pw_ironfeet, false),
  CHEAT("idbeholda",  "Auto-map",         not_dm, cheat_pw, pw_allmap, false),
  CHEAT("idbeholdl",  "Lite-Amp Goggles", not_dm, cheat_pw, pw_infrared, false),
  CHEAT("idbehold",   "BEHOLD menu",      not_dm, cheat_behold, 0, false),
  CHEAT("idclev",     "Level Warp",       cht_never | not_menu, cheat_clev, -2, false),
  CHEAT("idmypos",    "Player Position",  not_dm, cheat_mypos, 0, false),
  CHEAT("idrate",     "Frame rate",       always, cheat_rate, 0, false),
  // phares
  CHEAT("tntcomp",    NULL,               cht_never, cheat_comp, 0, false),
  // jff 2/01/98 kill all monsters
  CHEAT("tntem",      NULL,               cht_never, cheat_massacre, 0, false),
  // killough 2/07/98: moved from am_map.c
  CHEAT("iddt",       "Map cheat",        not_dm, cheat_ddt, 0, true),
  CHEAT("iddst",      NULL,               not_dm, cheat_reveal_secret, 0, true),
  CHEAT("iddkt",      NULL,               not_dm, cheat_reveal_kill, 0, true),
  CHEAT("iddit",      NULL,               not_dm, cheat_reveal_item, 0, true),
  // killough 2/07/98: HOM autodetector
  CHEAT("tnthom",     NULL,               always, cheat_hom, 0, false),
  // killough 2/16/98: generalized key cheats
  CHEAT("tntkey",     NULL,               cht_never, cheat_tntkey, 0, false),
  CHEAT("tntkeyr",    NULL,               cht_never, cheat_tntkeyx, 0, false),
  CHEAT("tntkeyy",    NULL,               cht_never, cheat_tntkeyx, 0, false),
  CHEAT("tntkeyb",    NULL,               cht_never, cheat_tntkeyx, 0, false),
  CHEAT("tntkeyrc",   NULL,               cht_never, cheat_tntkeyxx, it_redcard, false),
  CHEAT("tntkeyyc",   NULL,               cht_never, cheat_tntkeyxx, it_yellowcard, false),
  CHEAT("tntkeybc",   NULL,               cht_never, cheat_tntkeyxx, it_bluecard, false),
  CHEAT("tntkeyrs",   NULL,               cht_never, cheat_tntkeyxx, it_redskull, false),
  CHEAT("tntkeyys",   NULL,               cht_never, cheat_tntkeyxx, it_yellowskull, false),
  // killough 2/16/98: end generalized keys
  CHEAT("tntkeybs",   NULL,               cht_never, cheat_tntkeyxx, it_blueskull, false),
  // Ty 04/11/98 - Added TNTKA
  CHEAT("tntka",      NULL,               cht_never, cheat_k, 0, false),
  // killough 2/16/98: generalized weapon cheats
  CHEAT("tntweap",    NULL,               cht_never, cheat_tntweap, 0, false),
  CHEAT("tntweap",    NULL,               cht_never, cheat_tntweapx, -1, false),
  CHEAT("tntammo",    NULL,               cht_never, cheat_tntammo, 0, false),
  // killough 2/16/98: end generalized weapons
  CHEAT("tntammo",    NULL,               cht_never, cheat_tntammox, -1, false),
  // killough 2/21/98: smart monster toggle
  CHEAT("tntsmart",   NULL,               cht_never, cheat_smart, 0, false),
  // killough 2/21/98: pitched sound toggle
  CHEAT("tntpitch",   NULL,               always, cheat_pitch, 0, false),
  // killough 2/21/98: reduce RSI injury by adding simpler alias sequences:
  // killough 2/21/98: same as tntammo
  CHEAT("tntamo",     NULL,               cht_never, cheat_tntammo, 0, false),
  // killough 2/21/98: same as tntammo
  CHEAT("tntamo",     NULL,               cht_never, cheat_tntammox, -1, false),
  // killough 3/6/98: -fast toggle
  CHEAT("tntfast",    NULL,               cht_never, cheat_fast, 0, false),
  // phares 3/10/98: toggle variable friction effects
  CHEAT("tntice",     NULL,               cht_never, cheat_friction, 0, false),
  // phares 3/10/98: toggle pushers
  CHEAT("tntpush",    NULL,               cht_never, cheat_pushers, 0, false),

  // [RH] Monsters don't target
  CHEAT("notarget",   NULL,               cht_never, cheat_notarget, 0, false),
  // fly mode is active
  CHEAT("fly",        NULL,               cht_never, cheat_fly, 0, false),

  // heretic
  CHEAT("quicken", NULL, cht_dsda, cheat_god, 0, false),
  CHEAT("ponce", NULL, cht_never, cheat_reset_health, 0, false),
  CHEAT("kitty", NULL, cht_dsda, cheat_noclip, 0, false),
  CHEAT("massacre", NULL, cht_never, cheat_massacre, 0, false),
  CHEAT("rambo", NULL, cht_never, cheat_fa, 0, false),
  CHEAT("skel", NULL, cht_never, cheat_k, 0, false),
  CHEAT("gimme", NULL, cht_never, cheat_artifact, -2, false),
  CHEAT("shazam", NULL, cht_never, cheat_tome, 0, false),
  CHEAT("engage", NULL, cht_never | not_menu, cheat_clev, -2, false),
  CHEAT("ravmap", NULL, not_dm, cheat_ddt, 0, true),
  CHEAT("cockadoodledoo", NULL, cht_never, cheat_chicken, 0, false),

  // hexen
  CHEAT("satan", NULL, cht_dsda, cheat_god, 0, false),
  CHEAT("clubmed", NULL, cht_never, cheat_reset_health, 0, false),
  CHEAT("butcher", NULL, cht_never, cheat_massacre, 0, false),
  CHEAT("nra", NULL, cht_never, cheat_fa, 0, false),
  CHEAT("indiana", NULL, cht_never, cheat_inventory, 0, false),
  CHEAT("locksmith", NULL, cht_never, cheat_k, 0, false),
  CHEAT("sherlock", NULL, cht_never, cheat_puzzle, 0, false),
  CHEAT("casper", NULL, cht_dsda, cheat_noclip, 0, false),
  CHEAT("shadowcaster", NULL, cht_never, cheat_class, -1, false),
  CHEAT("visit", NULL, cht_never | not_menu, cheat_clev, -2, false),
  CHEAT("init", NULL, cht_never, cheat_init, 0, false),
  CHEAT("puke", NULL, cht_never, cheat_script, -2, false),
  CHEAT("mapsco", NULL, not_dm, cheat_ddt, 0, true),
  CHEAT("deliverance", NULL, cht_never, cheat_chicken, 0, false),

  // end-of-list marker
  {NULL}
};

//-----------------------------------------------------------------------------

static void cheat_mus(buf)
char buf[3];
{
  int musnum;

  //jff 3/20/98 note: this cheat allowed in netgame/demorecord

  //jff 3/17/98 avoid musnum being negative and crashing
  if (!isdigit(buf[0]) || !isdigit(buf[1]))
    return;

  plyr->message = s_STSTR_MUS; // Ty 03/27/98 - externalized

  if (gamemode == commercial)
    {
      musnum = mus_runnin + (buf[0]-'0')*10 + buf[1]-'0' - 1;

      //jff 4/11/98 prevent IDMUS00 in DOOMII and IDMUS36 or greater
      if (musnum < mus_runnin ||  ((buf[0]-'0')*10 + buf[1]-'0') > 35)
        plyr->message = s_STSTR_NOMUS; // Ty 03/27/98 - externalized
      else
        {
          S_ChangeMusic(musnum, 1);
          idmusnum = musnum; //jff 3/17/98 remember idmus number for restore
        }
    }
  else
    {
      musnum = mus_e1m1 + (buf[0]-'1')*9 + (buf[1]-'1');

      //jff 4/11/98 prevent IDMUS0x IDMUSx0 in DOOMI and greater than introa
      if (buf[0] < '1' || buf[1] < '1' || ((buf[0]-'1')*9 + buf[1]-'1') > 31)
        plyr->message = s_STSTR_NOMUS; // Ty 03/27/98 - externalized
      else
        {
          S_ChangeMusic(musnum, 1);
          idmusnum = musnum; //jff 3/17/98 remember idmus number for restore
        }
    }
}

// 'choppers' invulnerability & chainsaw
static void cheat_choppers()
{
  plyr->weaponowned[wp_chainsaw] = true;
  plyr->powers[pw_invulnerability] = true;
  plyr->message = s_STSTR_CHOPPERS; // Ty 03/27/98 - externalized
}

void M_CheatGod(void)
{
  // dead players are first respawned at the current position
  if (plyr->playerstate == PST_DEAD)
  {
    signed int an;
    mapthing_t mt = {0};

    P_MapStart();
    mt.x = plyr->mo->x >> FRACBITS;
    mt.y = plyr->mo->y >> FRACBITS;
    mt.angle = (plyr->mo->angle + ANG45/2)*(uint_64_t)45/ANG45;
    mt.type = consoleplayer + 1;
    mt.options = 1; // arbitrary non-zero value
    P_SpawnPlayer(consoleplayer, &mt);

    // spawn a teleport fog
    an = plyr->mo->angle >> ANGLETOFINESHIFT;
    P_SpawnMobj(plyr->mo->x + 20*finecosine[an],
                plyr->mo->y + 20*finesine[an],
                plyr->mo->z + g_telefog_height,
                g_mt_tfog);
    S_StartSound(plyr, g_sfx_revive);
    P_MapEnd();
  }

  plyr->cheats ^= CF_GODMODE;
  if (plyr->cheats & CF_GODMODE)
  {
    if (plyr->mo)
      plyr->mo->health = god_health;  // Ty 03/09/98 - deh

    plyr->health = god_health;
    plyr->message = s_STSTR_DQDON; // Ty 03/27/98 - externalized
  }
  else
    plyr->message = s_STSTR_DQDOFF; // Ty 03/27/98 - externalized

  if (raven) SB_Start();
}

static void cheat_god()
{                                    // 'dqd' cheat for toggleable god mode
  if (demorecording)
  {
    dsda_QueueExCmdGod();
    return;
  }

  M_CheatGod();
}

// CPhipps - new health and armour cheat codes
static void cheat_health()
{
  if (!(plyr->cheats & CF_GODMODE)) {
    if (plyr->mo)
      plyr->mo->health = mega_health;
    plyr->health = mega_health;
    plyr->message = s_STSTR_BEHOLDX; // Ty 03/27/98 - externalized
  }
}

static void cheat_megaarmour()
{
  plyr->armorpoints[ARMOR_ARMOR] = idfa_armor;      // Ty 03/09/98 - deh
  plyr->armortype = idfa_armor_class;  // Ty 03/09/98 - deh
  plyr->message = s_STSTR_BEHOLDX; // Ty 03/27/98 - externalized
}

static void cheat_fa()
{
  int i;

  if (hexen)
  {
    for (i = 0; i < NUMARMOR; i++)
    {
        plyr->armorpoints[i] = pclass[plyr->pclass].armor_increment[i];
    }
    for (i = 0; i < HEXEN_NUMWEAPONS; i++)
    {
        plyr->weaponowned[i] = true;
    }
    for (i = 0; i < NUMMANA; i++)
    {
        plyr->ammo[i] = MAX_MANA;
    }
  }
  else
  {
    if (!plyr->backpack)
    {
      for (i=0 ; i<NUMAMMO ; i++)
        plyr->maxammo[i] *= 2;
      plyr->backpack = true;
    }

    plyr->armorpoints[ARMOR_ARMOR] = idfa_armor;      // Ty 03/09/98 - deh
    plyr->armortype = idfa_armor_class;  // Ty 03/09/98 - deh

    // You can't own weapons that aren't in the game // phares 02/27/98
    for (i=0;i<NUMWEAPONS;i++)
      if (!(((i == wp_plasma || i == wp_bfg) && gamemode == shareware) ||
            (i == wp_supershotgun && gamemode != commercial)))
        plyr->weaponowned[i] = true;

    for (i=0;i<NUMAMMO;i++)
      if (i!=am_cell || gamemode!=shareware)
        plyr->ammo[i] = plyr->maxammo[i];

    plyr->message = s_STSTR_FAADDED;
  }
}

static void cheat_k()
{
  int i;
  for (i=0;i<NUMCARDS;i++)
    if (!plyr->cards[i])     // only print message if at least one key added
      {                      // however, caller may overwrite message anyway
        plyr->cards[i] = true;
        plyr->message = "Keys Added";
      }

  // heretic - reset status bar
  SB_Start();
}

static void cheat_kfa()
{
  cheat_k();
  cheat_fa();
  plyr->message = s_STSTR_KFAADDED;
}

void M_CheatNoClip(void)
{
  plyr->message = (plyr->cheats ^= CF_NOCLIP) & CF_NOCLIP ?
    s_STSTR_NCON : s_STSTR_NCOFF; // Ty 03/27/98 - externalized
}

static void cheat_noclip()
{
  if (demorecording)
  {
    dsda_QueueExCmdNoClip();
    return;
  }

  M_CheatNoClip();
}

// 'behold?' power-up cheats (modified for infinite duration -- killough)
static void cheat_pw(int pw)
{
  if (plyr->powers[pw])
    plyr->powers[pw] = pw!=pw_strength && pw!=pw_allmap;  // killough
  else
    {
      P_GivePower(plyr, pw);
      if (pw != pw_strength)
        plyr->powers[pw] = -1;      // infinite duration -- killough
    }
  plyr->message = s_STSTR_BEHOLDX; // Ty 03/27/98 - externalized
}

// 'behold' power-up menu
static void cheat_behold()
{
  plyr->message = s_STSTR_BEHOLD; // Ty 03/27/98 - externalized
}

extern int EpiCustom;

// 'clev' change-level cheat
static void cheat_clev(char buf[3])
{
  int epsd, map;
  struct MapEntry* entry;

  if (gamemode == commercial)
  {
    epsd = 1; //jff was 0, but espd is 1-based
    map = (buf[0] - '0') * 10 + buf[1] - '0';
  }
  else
  {
    epsd = buf[0] - '0';
    map = buf[1] - '0';
  }

  if (dsda_ResolveCLEV(&epsd, &map))
  {
    plyr->message = s_STSTR_CLEV; // Ty 03/27/98 - externalized

    G_DeferedInitNew(gameskill, epsd, map);
  }
}

// 'mypos' for player position
// killough 2/7/98: simplified using dprintf and made output more user-friendly
static void cheat_mypos()
{
  doom_printf("Position (%d,%d,%d)\tAngle %-.0f",
          players[consoleplayer].mo->x >> FRACBITS,
          players[consoleplayer].mo->y >> FRACBITS,
          players[consoleplayer].mo->z >> FRACBITS,
          players[consoleplayer].mo->angle * (90.0/ANG90));
}

// cph - cheat to toggle frame rate/rendering stats display
static void cheat_rate()
{
  rendering_stats ^= 1;
}

// compatibility cheat

static void cheat_comp()
{
  // CPhipps - modified for new compatibility system
  compatibility_level++; compatibility_level %= MAX_COMPATIBILITY_LEVEL;
  // must call G_Compatibility after changing compatibility_level
  // (fixes sf bug number 1558738)
  G_Compatibility();
  doom_printf("New compatibility level:\n%s",
        comp_lev_str[compatibility_level]);
}

// variable friction cheat
static void cheat_friction()
{
  plyr->message =                       // Ty 03/27/98 - *not* externalized
    (variable_friction = !variable_friction) ? "Variable Friction enabled" :
                                               "Variable Friction disabled";
}


// Pusher cheat
// phares 3/10/98
static void cheat_pushers()
{
  plyr->message =                      // Ty 03/27/98 - *not* externalized
    (allow_pushers = !allow_pushers) ? "Pushers enabled" : "Pushers disabled";
}

static void cheat_massacre()    // jff 2/01/98 kill all monsters
{
  // jff 02/01/98 'em' cheat - kill all monsters
  // partially taken from Chi's .46 port
  //
  // killough 2/7/98: cleaned up code and changed to use dprintf;
  // fixed lost soul bug (LSs left behind when PEs are killed)

  int killcount=0;
  thinker_t *currentthinker = NULL;
  extern void A_PainDie(mobj_t *);

  // killough 7/20/98: kill friendly monsters only if no others to kill
  uint_64_t mask = MF_FRIEND;
  P_MapStart();
  do
    while ((currentthinker = P_NextThinker(currentthinker,th_all)) != NULL)
    if (currentthinker->function == P_MobjThinker &&
  !(((mobj_t *) currentthinker)->flags & mask) && // killough 7/20/98
        (((mobj_t *) currentthinker)->flags & MF_COUNTKILL ||
         ((mobj_t *) currentthinker)->type == MT_SKULL))
      { // killough 3/6/98: kill even if PE is dead
        if (((mobj_t *) currentthinker)->health > 0)
          {
            killcount++;
            P_DamageMobj((mobj_t *)currentthinker, NULL, NULL, 10000);
          }
        if (((mobj_t *) currentthinker)->type == MT_PAIN)
          {
            A_PainDie((mobj_t *) currentthinker);    // killough 2/8/98
            P_SetMobjState ((mobj_t *) currentthinker, S_PAIN_DIE6);
          }
      }
  while (!killcount && mask ? mask=0, 1 : 0); // killough 7/20/98
  P_MapEnd();
  // killough 3/22/98: make more intelligent about plural
  // Ty 03/27/98 - string(s) *not* externalized
  doom_printf("%d Monster%s Killed", killcount, killcount==1 ? "" : "s");
}

// killough 2/7/98: move iddt cheat from am_map.c to here
// killough 3/26/98: emulate Doom better
static void cheat_ddt()
{
  extern int dsda_reveal_map;
  if (automapmode & am_active)
    dsda_reveal_map = (dsda_reveal_map+1) % 3;
}

static void cheat_reveal_secret()
{
  static int last_secret = -1;

  if (automapmode & am_active)
  {
    int i, start_i;

    i = last_secret + 1;
    if (i >= numsectors)
      i = 0;
    start_i = i;

    do
    {
      sector_t *sec = &sectors[i];

      if (P_IsSecret(sec))
      {
        automapmode &= ~am_follow;

        // This is probably not necessary
        if (sec->lines && sec->lines[0] && sec->lines[0]->v1)
        {
          AM_SetMapCenter(sec->lines[0]->v1->x, sec->lines[0]->v1->y);
          last_secret = i;
          break;
        }
      }

      i++;
      if (i >= numsectors)
        i = 0;
    } while (i != start_i);
  }
}

static void cheat_cycle_mobj(mobj_t **last_mobj, int *last_count, int flags, int alive)
{
  extern int init_thinkers_count;
  thinker_t *th, *start_th;

  // If the thinkers have been wiped, addresses are invalid
  if (*last_count != init_thinkers_count)
  {
    *last_count = init_thinkers_count;
    *last_mobj = NULL;
  }

  if (*last_mobj)
    th = &(*last_mobj)->thinker;
  else
    th = &thinkercap;

  start_th = th;

  for (th = th->next; th != start_th; th = th->next)
  {
    if (th->function == P_MobjThinker)
    {
      mobj_t *mobj;

      automapmode &= ~am_follow;

      mobj = (mobj_t *) th;

      if ((!alive || mobj->health > 0) && mobj->flags & flags)
      {
        AM_SetMapCenter(mobj->x, mobj->y);
        P_SetTarget(last_mobj, mobj);
        break;
      }
    }
  }
}

static void cheat_reveal_kill()
{
  if (automapmode & am_active)
  {
    static int last_count;
    static mobj_t *last_mobj;

    cheat_cycle_mobj(&last_mobj, &last_count, MF_COUNTKILL, true);
  }
}

static void cheat_reveal_item()
{
  if (automapmode & am_active)
  {
    static int last_count;
    static mobj_t *last_mobj;

    cheat_cycle_mobj(&last_mobj, &last_count, MF_COUNTITEM, false);
  }
}

// killough 2/7/98: HOM autodetection
static void cheat_hom()
{
  plyr->message = (flashing_hom = !flashing_hom) ? "HOM Detection On" :
    "HOM Detection Off";
}

// killough 3/6/98: -fast parameter toggle
static void cheat_fast()
{
  plyr->message = (fastparm = !fastparm) ? "Fast Monsters On" :
    "Fast Monsters Off";  // Ty 03/27/98 - *not* externalized
  G_SetFastParms(fastparm); // killough 4/10/98: set -fast parameter correctly
}

// killough 2/16/98: keycard/skullkey cheat functions
static void cheat_tntkey()
{
  plyr->message = "Red, Yellow, Blue";  // Ty 03/27/98 - *not* externalized
}

static void cheat_tntkeyx()
{
  plyr->message = "Card, Skull";        // Ty 03/27/98 - *not* externalized
}

static void cheat_tntkeyxx(int key)
{
  plyr->message = (plyr->cards[key] = !plyr->cards[key]) ?
    "Key Added" : "Key Removed";  // Ty 03/27/98 - *not* externalized
}

// killough 2/16/98: generalized weapon cheats

static void cheat_tntweap()
{                                   // Ty 03/27/98 - *not* externalized
  plyr->message = gamemode==commercial ?           // killough 2/28/98
    "Weapon number 1-9" : "Weapon number 1-8";
}

static void cheat_tntweapx(buf)
char buf[3];
{
  int w = *buf - '1';

  if ((w==wp_supershotgun && gamemode!=commercial) ||      // killough 2/28/98
      ((w==wp_bfg || w==wp_plasma) && gamemode==shareware))
    return;

  if (w==wp_fist)           // make '1' apply beserker strength toggle
    cheat_pw(pw_strength);
  else
    if (w >= 0 && w < NUMWEAPONS) {
      if ((plyr->weaponowned[w] = !plyr->weaponowned[w]))
        plyr->message = "Weapon Added";  // Ty 03/27/98 - *not* externalized
      else
        {
          plyr->message = "Weapon Removed"; // Ty 03/27/98 - *not* externalized
          if (w==plyr->readyweapon)         // maybe switch if weapon removed
            plyr->pendingweapon = P_SwitchWeapon(plyr);
        }
    }
}

// killough 2/16/98: generalized ammo cheats
static void cheat_tntammo()
{
  plyr->message = "Ammo 1-4, Backpack";  // Ty 03/27/98 - *not* externalized
}

static void cheat_tntammox(buf)
char buf[1];
{
  int a = *buf - '1';
  if (*buf == 'b')  // Ty 03/27/98 - strings *not* externalized
    if ((plyr->backpack = !plyr->backpack))
      for (plyr->message = "Backpack Added",   a=0 ; a<NUMAMMO ; a++)
        plyr->maxammo[a] <<= 1;
    else
      for (plyr->message = "Backpack Removed", a=0 ; a<NUMAMMO ; a++)
        {
          if (plyr->ammo[a] > (plyr->maxammo[a] >>= 1))
            plyr->ammo[a] = plyr->maxammo[a];
        }
  else
    if (a>=0 && a<NUMAMMO)  // Ty 03/27/98 - *not* externalized
      { // killough 5/5/98: switch plasma and rockets for now -- KLUDGE
        a = a==am_cell ? am_misl : a==am_misl ? am_cell : a;  // HACK
        plyr->message = (plyr->ammo[a] = !plyr->ammo[a]) ?
          plyr->ammo[a] = plyr->maxammo[a], "Ammo Added" : "Ammo Removed";
      }
}

static void cheat_smart()
{
  plyr->message = (monsters_remember = !monsters_remember) ?
    "Smart Monsters Enabled" : "Smart Monsters Disabled";
}

static void cheat_pitch()
{
  plyr->message=(pitched_sounds = !pitched_sounds) ? "Pitch Effects Enabled" :
    "Pitch Effects Disabled";
}

static void cheat_notarget()
{
  plyr->cheats ^= CF_NOTARGET;
  if (plyr->cheats & CF_NOTARGET)
    plyr->message = "Notarget Mode ON";
  else
    plyr->message = "Notarget Mode OFF";
}

static void cheat_fly()
{
  if (plyr->mo != NULL)
  {
    plyr->cheats ^= CF_FLY;
    if (plyr->cheats & CF_FLY)
    {
      plyr->mo->flags |= MF_NOGRAVITY;
      plyr->mo->flags |= MF_FLY;
      plyr->message = "Fly mode ON";
    }
    else
    {
      plyr->mo->flags &= ~MF_NOGRAVITY;
      plyr->mo->flags &= ~MF_FLY;
      plyr->message = "Fly mode OFF";
    }
  }
}

static dboolean M_ClassicDemo(void)
{
  return (demorecording || demoplayback) && !dsda_AllowCasualExCmdFeatures();
}

static dboolean M_CheatAllowed(int when)
{
  return !(when                    && dsda_StrictMode()) &&
         !(when & not_dm           && deathmatch) &&
         !(when & not_coop         && netgame && !deathmatch) &&
         !(when & not_demo         && (demorecording || demoplayback)) &&
         !(when & not_classic_demo && M_ClassicDemo()) &&
         !(when & not_menu         && menuactive);
}

static void cht_InitCheats(void)
{
  static int init = false;

  if (!init)
  {
    cheatseq_t* cht;

    init = true;

    for (cht = cheat; cht->cheat; cht++)
    {
      cht->sequence_len = strlen(cht->cheat);
    }
  }
}

//
// CHEAT SEQUENCE PACKAGE
//

//
// Called in st_stuff module, which handles the input.
// Returns a 1 if the cheat was successful, 0 if failed.
//
static int M_FindCheats(int key)
{
  int rc = 0;
  cheatseq_t* cht;
  char char_key;

  cht_InitCheats();

  char_key = (char)key;

  for (cht = cheat; cht->cheat; cht++)
  {
    if (M_CheatAllowed(cht->when))
    {
      if (cht->chars_read < cht->sequence_len)
      {
        // still reading characters from the cheat code
        // and verifying.  reset back to the beginning
        // if a key is wrong

        if (char_key == cht->cheat[cht->chars_read])
          ++cht->chars_read;
        else if (char_key == cht->cheat[0])
          cht->chars_read = 1;
        else
          cht->chars_read = 0;

        cht->param_chars_read = 0;
      }
      else if (cht->param_chars_read < -cht->arg)
      {
        // we have passed the end of the cheat sequence and are
        // entering parameters now

        cht->parameter_buf[cht->param_chars_read] = char_key;

        ++cht->param_chars_read;

        // affirmative response
        rc = 1;
      }

      if (cht->chars_read >= cht->sequence_len &&
          cht->param_chars_read >= -cht->arg)
      {
        if (cht->param_chars_read)
        {
          static char argbuf[CHEAT_ARGS_MAX + 1];

          // process the arg buffer
          memcpy(argbuf, cht->parameter_buf, -cht->arg);

          cht->func(argbuf);
        }
        else
        {
          // call cheat handler
          cht->func(cht->arg);

          if (cht->repeatable)
          {
            --cht->chars_read;
          }
        }

        if (!cht->repeatable)
          cht->chars_read = cht->param_chars_read = 0;
        rc = 1;
      }
    }
  }

  return rc;
}

typedef struct cheat_input_s {
  int input;
  const cheat_when_t when;
  void (*const func)();
  const int arg;
} cheat_input_t;

static cheat_input_t cheat_input[] = {
  { dsda_input_iddqd, cht_dsda, cheat_god, 0 },
  { dsda_input_idkfa, cht_never, cheat_kfa, 0 },
  { dsda_input_idfa, cht_never, cheat_fa, 0 },
  { dsda_input_idclip, cht_dsda, cheat_noclip, 0 },
  { dsda_input_idbeholdh, cht_never, cheat_health, 0 },
  { dsda_input_idbeholdm, cht_never, cheat_megaarmour, 0 },
  { dsda_input_idbeholdv, cht_never, cheat_pw, pw_invulnerability },
  { dsda_input_idbeholds, cht_never, cheat_pw, pw_strength },
  { dsda_input_idbeholdi, cht_never, cheat_pw, pw_invisibility },
  { dsda_input_idbeholdr, cht_never, cheat_pw, pw_ironfeet },
  { dsda_input_idbeholda, not_dm, cheat_pw, pw_allmap },
  { dsda_input_idbeholdl, not_dm, cheat_pw, pw_infrared },
  { dsda_input_idmypos, not_dm, cheat_mypos, 0 },
  { dsda_input_idrate, always, cheat_rate, 0 },
  { dsda_input_iddt, not_dm, cheat_ddt, 0 },
  { dsda_input_ponce, cht_never, cheat_reset_health, 0 },
  { dsda_input_shazam, cht_never, cheat_tome, 0 },
  { dsda_input_chicken, cht_never, cheat_chicken, 0 },
  { dsda_input_notarget, cht_never, cheat_notarget, 0 },
  { 0 }
};

dboolean M_CheatResponder(event_t *ev)
{
  cheat_input_t* cheat_i;

  if (dsda_ProcessCheatCodes() &&
      ev->type == ev_keydown &&
      M_FindCheats(ev->data1))
    return true;

  for (cheat_i = cheat_input; cheat_i->input; cheat_i++)
  {
    if (dsda_InputActivated(cheat_i->input))
    {
      if (M_CheatAllowed(cheat_i->when))
        cheat_i->func(cheat_i->arg);

      return true;
    }
  }

  if (M_CheatAllowed(cht_never) && dsda_InputActivated(dsda_input_avj))
  {
    plyr->mo->momz = 1000 * FRACUNIT / plyr->mo->info->mass;

    return true;
  }

  return false;
}

dboolean M_CheatEntered(const char* element, const char* value)
{
  cheatseq_t* cheat_i;

  for (cheat_i = cheat; cheat_i->cheat; cheat_i++)
  {
    if (!strcmp(cheat_i->cheat, element) && M_CheatAllowed(cheat_i->when))
    {
      if (cheat_i->arg >= 0)
        cheat_i->func(cheat_i->arg);
      else
        cheat_i->func(value);
      return true;
    }
  }
  return false;
}

// heretic

#include "p_user.h"

static void cheat_reset_health(void)
{
  if (heretic && plyr->chickenTics)
  {
    plyr->health = plyr->mo->health = MAXCHICKENHEALTH;
  }
  else if (hexen && plyr->morphTics)
  {
    plyr->health = plyr->mo->health = MAXMORPHHEALTH;
  }
  else
  {
    plyr->health = plyr->mo->health = MAXHEALTH;
  }
  plyr->message = "FULL HEALTH";
}

static void cheat_artifact(char buf[3])
{
  int i;
  int j;
  int type;
  int count;

  if (!heretic) return;

  type = buf[0] - 'a' + 1;
  count = buf[1] - '0';
  if (type == 26 && count == 0)
  { // All artifacts
    for (i = arti_none + 1; i < NUMARTIFACTS; i++)
    {
      if (gamemode == shareware && (i == arti_superhealth || i == arti_teleport))
      {
        continue;
      }
      for (j = 0; j < 16; j++)
      {
        P_GiveArtifact(plyr, i, NULL);
      }
    }
    plyr->message = "YOU GOT IT";
  }
  else if (type > arti_none && type < NUMARTIFACTS && count > 0 && count < 10)
  {
    if (gamemode == shareware && (type == arti_superhealth || type == arti_teleport))
    {
      plyr->message = "BAD INPUT";
      return;
    }
    for (i = 0; i < count; i++)
    {
      P_GiveArtifact(plyr, type, NULL);
    }
    plyr->message = "YOU GOT IT";
  }
  else
  { // Bad input
    plyr->message = "BAD INPUT";
  }
}

static void cheat_tome(void)
{
  if (!heretic) return;

  if (plyr->powers[pw_weaponlevel2])
  {
    plyr->powers[pw_weaponlevel2] = 0;
    plyr->message = "POWER OFF";
  }
  else
  {
    P_UseArtifact(plyr, arti_tomeofpower);
    plyr->message = "POWER ON";
  }
}

static void cheat_chicken(void)
{
  if (!raven) return;

  P_MapStart();
  if (heretic)
  {
    if (plyr->chickenTics)
    {
      if (P_UndoPlayerChicken(plyr))
      {
          plyr->message = "CHICKEN OFF";
      }
    }
    else if (P_ChickenMorphPlayer(plyr))
    {
      plyr->message = "CHICKEN ON";
    }
  }
  else
  {
    if (plyr->morphTics)
    {
      P_UndoPlayerMorph(plyr);
    }
    else
    {
      P_MorphPlayer(plyr);
    }
    plyr->message = "SQUEAL!!";
  }
  P_MapEnd();
}

// hexen

#include "hexen/p_acs.h"

static void cheat_init(void)
{
  if (dsda_ResolveINIT())
  {
    P_SetMessage(plyr, "LEVEL WARP", true);
  }
}

static void cheat_inventory(void)
{
  int i, j;
  int start, end;

  if (!raven) return;

  if (heretic)
  {
    start = arti_none + 1;
    end = NUMARTIFACTS;
  }
  else
  {
    start = hexen_arti_none + 1;
    end = hexen_arti_firstpuzzitem;
  }

  for (i = start; i < end; i++)
  {
    for (j = 0; j < g_arti_limit; j++)
    {
      P_GiveArtifact(plyr, i, NULL);
    }
  }
  P_SetMessage(plyr, "ALL ARTIFACTS", true);
}

static void cheat_puzzle(void)
{
  int i, j;

  if (!hexen) return;

  for (i = hexen_arti_firstpuzzitem; i < HEXEN_NUMARTIFACTS; i++)
  {
    P_GiveArtifact(plyr, i, NULL);
  }
  P_SetMessage(plyr, "ALL PUZZLE ITEMS", true);
}

static void cheat_class(char buf[2])
{
  int i;
  int new_class;

  if (!hexen) return;

  if (plyr->morphTics)
  {                           // don't change class if the player is morphed
      return;
  }

  new_class = 1 + (buf[0] - '0');
  if (new_class > PCLASS_MAGE || new_class < PCLASS_FIGHTER)
  {
    P_SetMessage(plyr, "INVALID PLAYER CLASS", true);
    return;
  }
  plyr->pclass = new_class;
  for (i = 0; i < NUMARMOR; i++)
  {
    plyr->armorpoints[i] = 0;
  }
  PlayerClass[consoleplayer] = new_class;
  P_PostMorphWeapon(plyr, wp_first);
  SB_SetClassData();
  SB_Start();
  P_SetMessage(plyr, "CLASS CHANGED", true);
}

static void cheat_script(char buf[3])
{
  int script;
  byte script_args[3];
  int tens, ones;
  static char textBuffer[40];

  if (!map_format.acs) return;

  tens = buf[0] - '0';
  ones = buf[1] - '0';
  script = tens * 10 + ones;
  if (script < 1)
      return;
  if (script > 99)
      return;
  script_args[0] = script_args[1] = script_args[2] = 0;

  if (P_StartACS(script, 0, script_args, plyr->mo, NULL, 0))
  {
    doom_snprintf(textBuffer, sizeof(textBuffer),
                  "RUNNING SCRIPT %.2d", script);
    P_SetMessage(plyr, textBuffer, true);
  }
}
