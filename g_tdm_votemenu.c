/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

//OpenTDM vote menu.

#include "g_local.h"
#include "g_tdm.h"

static const pmenu_t votemenu[] =
{
	{ "*Vote menu",			PMENU_ALIGN_CENTER, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*Apply vote",		PMENU_ALIGN_CENTER, NULL, TDM_VoteMenuApply },
	{ "Use [ and ] to move cursor",	PMENU_ALIGN_CENTER, NULL, NULL },
	{ "ENTER select, ESC exit",	PMENU_ALIGN_CENTER, NULL, NULL },
};

/*
==============
VoteMenuUpdate
==============
Update selected lines in votemenu for client.
*/
void VoteMenuUpdate (edict_t *ent, unsigned flags)
{
	if (flags & VOTE_MENU_GAMEMODE)
	{
		static const char *gameString[] = {
			"TDM",
			"ITDM",
			"1v1"
		};

		sprintf (ent->client->votemenu_values.string_gamemode, "Gamemode: %s", gameString[ent->client->votemenu_values.gamemode]);
		ent->client->votemenu[2].text = ent->client->votemenu_values.string_gamemode;
		ent->client->votemenu[2].SelectFunc = VoteMenuGameMode;
	}

	if (flags & VOTE_MENU_MAP)
	{
		if (!ent->client->votemenu_values.map[0])
		{
			strcpy (ent->client->votemenu_values.string_map, "Map: maplist is disabled");
			ent->client->votemenu[3].SelectFunc = NULL;
		}
		else
		{
			sprintf (ent->client->votemenu_values.string_map, "Map:      %s", ent->client->votemenu_values.map);
			ent->client->votemenu[3].SelectFunc = VoteMenuMap;
		}

		ent->client->votemenu[3].text = ent->client->votemenu_values.string_map;
	}

	if (flags & VOTE_MENU_CONFIG)
	{
		if (!ent->client->votemenu_values.config[0] || !g_allow_vote_config->value)
		{
			strcpy (ent->client->votemenu_values.string_config, "Config:   disabled");
			ent->client->votemenu[4].SelectFunc = NULL;
		}
		else
		{
			sprintf (ent->client->votemenu_values.string_config, "Config:   %s", ent->client->votemenu_values.config);
			ent->client->votemenu[4].SelectFunc = VoteMenuConfig;
		}

		ent->client->votemenu[4].text = ent->client->votemenu_values.string_config;
	}

	if (flags & VOTE_MENU_TIMELIMIT)
	{
		sprintf (ent->client->votemenu_values.string_timelimit, "Timelimit: %d", ent->client->votemenu_values.timelimit);
		ent->client->votemenu[6].text = ent->client->votemenu_values.string_timelimit;
		ent->client->votemenu[6].SelectFunc = VoteMenuTimelimit;
	}

	if (flags & VOTE_MENU_OVERTIME)
	{
		if (ent->client->votemenu_values.overtime == -1)
			strcpy (ent->client->votemenu_values.string_overtime, "Overtime:  Sudden Death");
		else if (ent->client->votemenu_values.overtime == 0)
			strcpy (ent->client->votemenu_values.string_overtime, "Overtime:  Tie Mode");
		else
			sprintf (ent->client->votemenu_values.string_overtime, "Overtime:  %d", ent->client->votemenu_values.overtime);

		ent->client->votemenu[7].text = ent->client->votemenu_values.string_overtime;
		ent->client->votemenu[7].SelectFunc = VoteMenuOvertime;
	}

	if (flags & VOTE_MENU_POWERUPS)
	{
		sprintf (ent->client->votemenu_values.string_powerups, "Powerups:  %d", ent->client->votemenu_values.powerups);
		ent->client->votemenu[8].text = ent->client->votemenu_values.string_powerups;
		ent->client->votemenu[8].SelectFunc = VoteMenuPowerups;
	}

	if (flags & VOTE_MENU_BFG)
	{
		sprintf (ent->client->votemenu_values.string_bfg, "BFG:  %d", ent->client->votemenu_values.bfg);
		ent->client->votemenu[9].text = ent->client->votemenu_values.string_bfg;
		ent->client->votemenu[9].SelectFunc = VoteMenuBFG;
	}

	if (flags & VOTE_MENU_KICK)
	{
		edict_t	*victim;
		victim = ent->client->votemenu_values.kick;

		if (victim == NULL)
			strcpy (ent->client->votemenu_values.string_kick, "Kick: ---");
		else
			sprintf (ent->client->votemenu_values.string_kick, "Kick: %s", victim->client->pers.netname);

		ent->client->votemenu[11].text = ent->client->votemenu_values.string_kick;
		ent->client->votemenu[11].SelectFunc = VoteMenuKick;
	}

	if (flags & VOTE_MENU_CHAT)
	{
		static const char *chatString[] = {
			"speak",
			"whisper"
		};

		sprintf (ent->client->votemenu_values.string_chat, "Chat: %s", chatString[ent->client->votemenu_values.chat]);
		ent->client->votemenu[12].text = ent->client->votemenu_values.string_chat;
		ent->client->votemenu[12].SelectFunc = VoteMenuChat;
	}

	if (flags & VOTE_MENU_BUGS)
	{
		static const char *bugsString[] = {
			"all bugs fixed",
			"serious bugs fixed",
			"default q2 behavior"
		};

		sprintf (ent->client->votemenu_values.string_bugs, "Bugs: %s", bugsString[ent->client->votemenu_values.bugs]);
		ent->client->votemenu[13].text = ent->client->votemenu_values.string_bugs;
		ent->client->votemenu[13].SelectFunc = VoteMenuBugs;
	}
}

/*
==============
VoteMenuGameMode
==============
Vote menu gamemode change handler.
*/
void VoteMenuGameMode (edict_t *ent)
{
	ent->client->votemenu_values.gamemode = (ent->client->votemenu_values.gamemode + 1) % 3;

	VoteMenuUpdate (ent, VOTE_MENU_GAMEMODE);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

/*
==============
VoteMenuMap
==============
Vote menu map change handler.
*/
void VoteMenuMap (edict_t *ent)
{
	static char	maplist[2048];
	char		*listptr;
	int			i = 0;

	listptr = TDM_MaplistString ();

	if (!listptr || !listptr[0])
		return;

	if (!maplist[0])
		strcpy (maplist, listptr);

	if (ent->client->votemenu_values.mapptr == NULL || ent->client->votemenu_values.mapptr > maplist + strlen (maplist))
		ent->client->votemenu_values.mapptr = maplist;

	while (ent->client->votemenu_values.mapptr[i] && ent->client->votemenu_values.mapptr[i] != '\n')
		i++;

	//memset (ent->client->votemenu_values.map, '\0', strlen (ent->client->votemenu_values.map));

	// we reached the end of maplist.. show '---'
	if (!ent->client->votemenu_values.mapptr[0])
	{
		strcpy (ent->client->votemenu_values.map, "---");
		ent->client->votemenu_values.mapptr = NULL;
	}
	else
	{
		strncpy (ent->client->votemenu_values.map, ent->client->votemenu_values.mapptr, i);
		ent->client->votemenu_values.map[i] = '\0';

		ent->client->votemenu_values.mapptr += (i+1);
	}

	VoteMenuUpdate (ent, VOTE_MENU_MAP);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

/*
==============
VoteMenuConfig
==============
Vote menu config change handler.
*/
void VoteMenuConfig (edict_t *ent)
{
	static char	configlist[2048];
	char		*listptr;
	int			i = 0;

	listptr = TDM_ConfiglistString ();

	if (!listptr || !listptr[0])
		return;

	if (!configlist[0])
		strcpy (configlist, listptr);

	if (ent->client->votemenu_values.cfgptr == NULL || ent->client->votemenu_values.cfgptr > configlist + strlen (configlist))
		ent->client->votemenu_values.cfgptr = configlist;

	while (ent->client->votemenu_values.cfgptr[i] && ent->client->votemenu_values.cfgptr[i] != '\n')
		i++;

	//memset (ent->client->votemenu_values.config, '\0', strlen (ent->client->votemenu_values.config));

	// we reached the end of cfglist.. show '---'
	if (!ent->client->votemenu_values.cfgptr[0])
	{
		strcpy (ent->client->votemenu_values.config, "---");
		ent->client->votemenu_values.cfgptr = NULL;
	}
	else
	{
		strncpy (ent->client->votemenu_values.config, ent->client->votemenu_values.cfgptr, i);
		ent->client->votemenu_values.config[i] = '\0';
		ent->client->votemenu_values.cfgptr += (i+1);
	}

	VoteMenuUpdate (ent, VOTE_MENU_CONFIG);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

/*
==============
VoteMenuTimelimit
==============
Vote menu timelimit change handler.
*/
void VoteMenuTimelimit (edict_t *ent)
{
	ent->client->votemenu_values.timelimit = (ent->client->votemenu_values.timelimit % 30) + 5;

	VoteMenuUpdate (ent, VOTE_MENU_TIMELIMIT);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

/*
==============
VoteMenuOvertime
==============
Vote menu overtime change handler.
*/
void VoteMenuOvertime (edict_t *ent)
{
	// we need to cycle from -1 up to 5
	ent->client->votemenu_values.overtime = ((ent->client->votemenu_values.overtime + 2) % 7) - 1;

	VoteMenuUpdate (ent, VOTE_MENU_OVERTIME);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

/*
==============
VoteMenuPowerups
==============
Vote menu powerups change handler.
*/
void VoteMenuPowerups (edict_t *ent)
{
	ent->client->votemenu_values.powerups = (ent->client->votemenu_values.powerups + 1) % 2;

	VoteMenuUpdate (ent, VOTE_MENU_POWERUPS);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

/*
==============
VoteMenuBFG
==============
Vote menu BFG change handler.
*/
void VoteMenuBFG (edict_t *ent)
{
	ent->client->votemenu_values.bfg = (ent->client->votemenu_values.bfg + 1) % 2;

	VoteMenuUpdate (ent, VOTE_MENU_BFG);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

/*
==============
VoteMenuKick
==============
Vote menu kick change handler.
*/
void VoteMenuKick (edict_t *ent)
{
	edict_t	*victim;

	if (ent->client->votemenu_values.kick == NULL)
		victim = g_edicts + 1;
	else
		victim = ent->client->votemenu_values.kick + 1;

	while (victim <= g_edicts + game.maxclients && (!victim->inuse || victim == ent || victim->client->pers.admin))
		victim++;

	if (victim <= g_edicts + game.maxclients)
		ent->client->votemenu_values.kick = victim;
	else
		ent->client->votemenu_values.kick = NULL;

	VoteMenuUpdate (ent, VOTE_MENU_KICK);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

/*
==============
VoteMenuChat
==============
Vote menu chat change handler.
*/
void VoteMenuChat (edict_t *ent)
{
	ent->client->votemenu_values.chat = (ent->client->votemenu_values.chat + 1) % 2;

	VoteMenuUpdate (ent, VOTE_MENU_CHAT);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

/*
==============
VoteMenuBugs
==============
Vote menu bugs change handler.
*/
void VoteMenuBugs (edict_t *ent)
{
	ent->client->votemenu_values.bugs = (ent->client->votemenu_values.bugs + 1) % 3;

	VoteMenuUpdate (ent, VOTE_MENU_BUGS);
	PMenu_Update (ent);
	gi.unicast (ent, true);
}

/*
==============
OpenVoteMenu
==============
Update vote menu with current settings and open it.
*/
void OpenVoteMenu (edict_t *ent)
{
	if (!ent->client->pers.team && !ent->client->pers.admin)
	{
		gi.cprintf (ent, PRINT_HIGH, "Spectators cannot vote.\n");
		return;
	}

	// allow timelimit vote during the match
	if (tdm_match_status == MM_PLAYING)
	{
		ent->client->votemenu_values.timelimit = ((int)g_match_time->value / 60);
		memcpy (ent->client->votemenu, votemenu, sizeof(votemenu));
		VoteMenuUpdate (ent, VOTE_MENU_TIMELIMIT);

		PMenu_Close (ent);
		PMenu_Open (ent, ent->client->votemenu, 0, MENUSIZE_JOINMENU, false);
		return;
	}
	else if (tdm_match_status != MM_WARMUP)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can propose new settings only during warmup.\n");
		return;
	}

	// initialize all current settings
	ent->client->votemenu_values.gamemode = (int)g_gamemode->value;

	// if we don't use maplist we cannot tell what maps are allowed :(
	if (!g_maplistfile || !g_maplistfile->string[0])
		ent->client->votemenu_values.map[0] = '\0';
	else
	{
		strcpy (ent->client->votemenu_values.map, "---");
		ent->client->votemenu_values.mapptr = NULL;
	}

	if (!g_allow_vote_config->value)
		ent->client->votemenu_values.config[0] = '\0';
	else
	{
		strcpy (ent->client->votemenu_values.config, "---");
		ent->client->votemenu_values.cfgptr = NULL;
	}

	ent->client->votemenu_values.timelimit = ((int)g_match_time->value / 60);

	if (g_tie_mode->value == 1)
		ent->client->votemenu_values.overtime = ((int)g_overtime->value / 60);
	else if (g_tie_mode->value == 2)
		ent->client->votemenu_values.overtime = -1;
	else
		ent->client->votemenu_values.overtime = 0;

	if (g_powerupflags->value == 0)
		ent->client->votemenu_values.powerups = 1;
	else
		ent->client->votemenu_values.powerups = 0;

	if ((int)g_itemflags->value & WEAPON_BFG10K)
		ent->client->votemenu_values.bfg = 0;
	else
		ent->client->votemenu_values.bfg = 1;

	ent->client->votemenu_values.kick = NULL;
	ent->client->votemenu_values.chat = (int)g_chat_mode->value;
	ent->client->votemenu_values.bugs = (int)g_bugs->value;

	memcpy (ent->client->votemenu, votemenu, sizeof(votemenu));

	VoteMenuUpdate (ent, VOTE_MENU_ALL);

	// we are supposed to be here only from menu, so close it
	PMenu_Close (ent);
	PMenu_Open (ent, ent->client->votemenu, 0, MENUSIZE_JOINMENU, false);
}
