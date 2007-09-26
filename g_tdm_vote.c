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

//The voting file! Here is all the voting stuff, including web config
//download setup and parsing of configs, applying votes and all that.

#include "g_local.h"
#include "g_tdm.h"

#define VOTE_TIMELIMIT	0x1
#define VOTE_MAP		0x2
#define	VOTE_KICK		0x4
#define VOTE_WEAPONS	0x8
#define VOTE_POWERUPS	0x10
#define VOTE_GAMEMODE	0x20

#define VOTE_TELEMODE	0x80
#define VOTE_TIEMODE	0x100
#define VOTE_SWITCHMODE	0x200
#define VOTE_OVERTIME	0x400

//distinct so a local and remote '1v1' config are distinguishable
#define	VOTE_CONFIG		0x800
#define	VOTE_WEBCONFIG	0x1000

#define	VOTE_CHAT		0x2000

#define WEAPON_SHOTGUN			(1<<1)
#define	WEAPON_SSHOTGUN			(1<<2)
#define	WEAPON_MACHINEGUN		(1<<3)
#define	WEAPON_CHAINGUN			(1<<4)
#define WEAPON_GRENADES			(1<<5)
#define WEAPON_GRENADELAUNCHER	(1<<6)
#define	WEAPON_ROCKETLAUNCHER	(1<<7)
#define WEAPON_RAILGUN			(1<<8)
#define WEAPON_BFG10K			(1<<9)
#define	WEAPON_HYPERBLASTER		(1<<10)

#define POWERUP_QUAD			(1<<1)
#define POWERUP_INVULN			(1<<2)
#define POWERUP_POWERSHIELD		(1<<3)
#define POWERUP_POWERSCREEN		(1<<4)
#define POWERUP_SILENCER		(1<<5)
#define POWERUP_REBREATHER		(1<<6)
#define	POWERUP_ENVIROSUIT		(1<<7)

//the ordering of weapons must match ITEM_ defines too!
const weaponinfo_t	weaponvotes[WEAPON_MAX] = 
{
	{{"shot", "sg"}, WEAPON_SHOTGUN, ITEM_WEAPON_SHOTGUN},
	{{"sup", "ssg"}, WEAPON_SSHOTGUN, ITEM_WEAPON_SUPERSHOTGUN},
	{{"mac", "mg"}, WEAPON_MACHINEGUN, ITEM_WEAPON_MACHINEGUN},
	{{"cha", "cg"}, WEAPON_CHAINGUN, ITEM_WEAPON_CHAINGUN},
	{{"han", "hg"}, WEAPON_GRENADES, ITEM_AMMO_GRENADES},
	{{"gre", "gl"}, WEAPON_GRENADELAUNCHER, ITEM_WEAPON_GRENADELAUNCHER},
	{{"roc", "rl"}, WEAPON_ROCKETLAUNCHER, ITEM_WEAPON_ROCKETLAUNCHER},
	{{"hyper", "hb"}, WEAPON_HYPERBLASTER, ITEM_WEAPON_HYPERBLASTER},
	{{"rail", "rg"}, WEAPON_RAILGUN, ITEM_WEAPON_RAILGUN},
	{{"bfg", "10k"}, WEAPON_BFG10K, ITEM_WEAPON_BFG},
};

const powerupinfo_t	powerupvotes[POWERUP_MAX] = 
{
	{{"quad"}, POWERUP_QUAD, ITEM_ITEM_QUAD},
	{{"invul"}, POWERUP_INVULN, ITEM_ITEM_INVULNERABILITY,},
	{{"ps"}, POWERUP_POWERSHIELD, ITEM_ITEM_POWER_SHIELD},
	{{"powerscreen"}, POWERUP_POWERSCREEN, ITEM_ITEM_POWER_SCREEN},
	{{"silencer"}, POWERUP_SILENCER, ITEM_ITEM_SILENCER},
	{{"rebreather"}, POWERUP_REBREATHER, ITEM_ITEM_BREATHER},
	{{"envirosuit"}, POWERUP_ENVIROSUIT, ITEM_ITEM_ENVIRO},
};

//current vote
vote_t			vote;

//cached configs
tdm_config_t	tdm_configs;

/*
==============
TDM_ApplyVote
==============
Apply vote.
*/
void EndDMLevel (void);
static void TDM_ApplyVote (void)
{
	char value[16];

	if (vote.flags & VOTE_CONFIG)
		gi.bprintf (PRINT_CHAT, "New config: %s\n", vote.configname);
	else if (vote.flags & VOTE_WEBCONFIG)
		gi.bprintf (PRINT_CHAT, "New web config: %s\n", vote.configname);
	
	if (vote.flags & VOTE_TIMELIMIT)
	{
		sprintf (value, "%d", vote.newtimelimit * 60);
		g_match_time = gi.cvar_set ("g_match_time", value);

		if (!(vote.flags & (VOTE_CONFIG|VOTE_WEBCONFIG)))
			gi.bprintf (PRINT_CHAT, "New timelimit: %d minute%s\n", (int)vote.newtimelimit, vote.newtimelimit == 1 ? "" : "s");
	}

	if (vote.flags & VOTE_MAP)
	{
		strcpy (level.nextmap, vote.newmap);
		if (!(vote.flags & (VOTE_CONFIG|VOTE_WEBCONFIG)))
			gi.bprintf (PRINT_CHAT, "New map: %s\n", vote.newmap);
		EndDMLevel();
	}

	if (vote.flags & VOTE_KICK)
	{
		gi.AddCommandString (va ("kick %d\n", (int)(vote.victim - g_edicts - 1)));
	}

	if (vote.flags & VOTE_WEAPONS)
	{
		sprintf (value, "%d", vote.newweaponflags);
		g_itemflags = gi.cvar_set ("g_itemflags", value);
	}

	if (vote.flags & VOTE_POWERUPS)
	{
		sprintf (value, "%d", vote.newpowerupflags);
		g_powerupflags = gi.cvar_set ("g_powerupflags", value);
	}

	if (vote.flags & (VOTE_WEAPONS|VOTE_POWERUPS))
		TDM_ResetLevel ();

	if (vote.flags & VOTE_GAMEMODE)
	{
		if (vote.gamemode == GAMEMODE_ITDM)
			dmflags = gi.cvar_set ("dmflags", g_itdmflags->string);
		else if (vote.gamemode == GAMEMODE_TDM)
			dmflags = gi.cvar_set ("dmflags", g_tdmflags->string);
		else if (vote.gamemode == GAMEMODE_1V1)
			dmflags = gi.cvar_set ("dmflags", g_1v1flags->string);

		//we force it here since we're in warmup and we know what we're doing.
		//g_gamemode is latched otherwise to prevent server op from changing it
		//via rcon / console mid game and ruining things.
		gi.cvar_forceset ("g_gamemode", va ("%d", vote.gamemode));
		TDM_ResetGameState ();
		TDM_UpdateConfigStrings (true);
	}

	if (vote.flags & VOTE_TIEMODE)
	{
		sprintf (value, "%d", vote.tiemode);
		g_tie_mode = gi.cvar_set ("g_tie_mode", value);
	}

	if (vote.flags & VOTE_SWITCHMODE)
	{
		sprintf (value, "%d", vote.switchmode);
		g_fast_weap_switch = gi.cvar_set ("g_fast_weap_switch", value);
	}

	if (vote.flags & VOTE_TELEMODE)
	{
		sprintf (value, "%d", vote.telemode);
		g_teleporter_nofreeze = gi.cvar_set ("g_teleporter_nofreeze", value);
	}

	if (vote.flags & VOTE_CHAT)
	{
		sprintf (value, "%d", vote.newchatmode);
		g_chat_mode = gi.cvar_set ("g_chat_mode", value);
	}

	if (vote.flags & VOTE_OVERTIME)
	{
		sprintf (value, "%d", vote.overtimemins * 60);
		g_overtime = gi.cvar_set ("g_overtime", value);

		if (!(vote.flags & (VOTE_CONFIG|VOTE_WEBCONFIG)))
			gi.bprintf (PRINT_CHAT, "New overtime: %d minute%s\n", (int)vote.overtimemins, vote.overtimemins == 1 ? "" : "s");
	}
}

/*
==============
TDM_AnnounceVote
==============
Announce vote to other players.
should be on screen vote
*/
static void TDM_AnnounceVote (void)
{
	char	message[1024];
	char	what[1024];

	message[0] = 0;
	what[0] = 0;

	strcpy (message, vote.initiator->client->pers.netname);
	strcat (message, " started a vote: ");

	if (vote.flags & VOTE_CONFIG)
		sprintf (what, "config %s (", vote.configname);
	else if (vote.flags & VOTE_WEBCONFIG)
		sprintf (what, "webconfig %s (", vote.configname);

	if (vote.flags & VOTE_TIMELIMIT)
	{
		strcat (what, va("timelimit %d", vote.newtimelimit));
	}
	
	if (vote.flags & VOTE_MAP)
	{
		if (what[0])
			strcat (what, ", ");
		strcat (what, va ("map %s",vote.newmap));
	}
	
	if (vote.flags & VOTE_WEAPONS)
	{
		int			j;

		if (what[0])
			strcat (what, ", ");

		strcat (what, va("weapons"));
		for (j = 0; j < sizeof(weaponvotes) / sizeof(weaponvotes[1]); j++)
		{
			if (vote.newweaponflags & weaponvotes[j].value && !((int)g_itemflags->value & weaponvotes[j].value))
			{
				strcat (what, va(" -%s", weaponvotes[j].names[1]));
			}
			else if (!(vote.newweaponflags & weaponvotes[j].value) && (int)g_itemflags->value & weaponvotes[j].value)
			{
				strcat (what, va(" +%s", weaponvotes[j].names[1]));
			}
		}
	}

	//FIXME: should this really be able to be snuck in with all the other flags? :)
	if (vote.flags & VOTE_KICK)
	{
		if (what[0])
			strcat (what, ", ");
		strcat (what, va("kick %s", vote.victim->client->pers.netname));
	}
	
	if (vote.flags & VOTE_POWERUPS)
	{
		int			j;

		if (what[0])
			strcat (what, ", ");

		strcat (what, va("powerups"));
		for (j = 0; j < sizeof(powerupvotes) / sizeof(powerupvotes[1]); j++)
		{
			if (vote.newpowerupflags & powerupvotes[j].value && !((int)g_powerupflags->value & powerupvotes[j].value))
			{
				strcat (what, va(" -%s", powerupvotes[j].names[0]));
			}
			else if (!(vote.newpowerupflags & powerupvotes[j].value) && (int)g_powerupflags->value & powerupvotes[j].value)
			{
				strcat (what, va(" +%s", powerupvotes[j].names[0]));
			}
		}
	}
	
	if (vote.flags & VOTE_GAMEMODE)
	{
		if (what[0])
			strcat (what, ", ");

		if (vote.gamemode == GAMEMODE_TDM)
			strcat (what, "mode TDM");
		else if (vote.gamemode == GAMEMODE_ITDM)
			strcat (what, "mode ITDM");
		else if (vote.gamemode == GAMEMODE_1V1)
			strcat (what, "mode 1v1");
	}

	if (vote.flags & VOTE_TIEMODE)
	{
		if (what[0])
			strcat (what, ", ");

		if (vote.tiemode == 0)
			strcat (what, "no overtime");
		else if (vote.tiemode == 1)
			strcat (what, "overtime enabled");
		else if (vote.tiemode == 2)
			strcat (what, "sudden death enabled");
	}

	if (vote.flags & VOTE_SWITCHMODE)
	{
		if (what[0])
			strcat (what, ", ");

		if (vote.switchmode == 0)
			strcat (what, "normal weapon switch");
		else if (vote.switchmode == 1)
			strcat (what, "faster weapon switch");
		else if (vote.switchmode == 2)
			strcat (what, "instant weapon switch");
		else if (vote.switchmode == 3)
			strcat (what, "insane weapon switch");
	}

	if (vote.flags & VOTE_TELEMODE)
	{
		if (what[0])
			strcat (what, ", ");

		if (vote.telemode == 0)
			strcat (what, "normal teleporter mode");
		else if (vote.telemode == 1)
			strcat (what, "no freeze teleporter mode");
	}

	if (vote.flags & VOTE_OVERTIME)
	{
		if (what[0])
			strcat (what, ", ");

		strcat (what, va("overtime %d", vote.overtimemins));
	}

	if (vote.flags & VOTE_CHAT)
	{
		if (what[0])
			strcat (what, ", ");

		if (vote.newchatmode == 1)
			strcat (what, "no spectator chat");
		else
			strcat (what, "allow all chat");
	}

	if (vote.flags & (VOTE_CONFIG|VOTE_WEBCONFIG))
		strcat (what, ")");

	gi.bprintf (PRINT_CHAT, "%s%s\n", message, what);
}

/*
==============
TDM_RemoveVote
==============
Reset vote and all players' votes.
*/
void TDM_RemoveVote (void)
{
	edict_t	*ent;

	memset (&vote, 0, sizeof(vote));

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;
		
		ent->client->resp.vote = VOTE_HOLD;
	}
}

/*
==============
TDM_VoteTimeLimit
==============
Vote to change the timelimit.
*/
qboolean TDM_VoteTimeLimit (edict_t *ent)
{
	const char		*value;
	unsigned		limit;

	value = gi.argv(2);
	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote %s <mins>\n", gi.argv(0));
		return false;
	}

	limit = strtoul (value, NULL, 10);

	//one day should be sufficient.. :)
	if (limit < 1 || limit > 1440)
	{
		gi.cprintf (ent, PRINT_HIGH, "Invalid timelimit value.\n");
		return false;
	}

	//check current timelimit
	if (g_match_time->value == limit * 60)
	{
		gi.cprintf (ent, PRINT_HIGH, "Timelimit is already at %d minute%s.\n", limit, limit == 1 ? "" : "s");
		return false;
	}

	if (vote.active)
	{
		if (limit == vote.newtimelimit)
		{
			gi.cprintf (ent, PRINT_HIGH, "You've already started a vote for that timelimit.\n");
			return false;
		}

		if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
			return false;			
		
		gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
		TDM_RemoveVote ();
	}

	vote.flags |= VOTE_TIMELIMIT;
	vote.newtimelimit = limit;

	return true;
}

/*
==============
TDM_VoteMap
==============
Vote to change the map. Causes an intermission for reasons unknown :).
*/
qboolean TDM_VoteMap (edict_t *ent)
{
	unsigned	i;
	size_t		len;
	const char	*value;

	value = gi.argv(2);

	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote map <mapname>\n");
		return false;
	}

	//TODO: check map from maplist oO

	len = strlen (value);

	if (len >= MAX_QPATH - 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Invalid map name.\n");
		return false;
	}

	for (i = 0; i < len; i++)
	{
		if (!isalnum (value[i]) && value[i] != '_' && value[i] != '-')
		{
			gi.cprintf (ent, PRINT_HIGH, "Invalid map name.\n");
			return false;
		}
	}

	if (!strcmp (level.mapname, value))
	{
		gi.cprintf (ent, PRINT_HIGH, "You're already playing on %s!\n", value);
		return false;
	}

	if (vote.active)
	{
		if (!strcmp (vote.newmap , value))
		{
			gi.cprintf (ent, PRINT_HIGH, "You've already started a vote for %s.\n", value);
			return false;
		}

		if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
			return false;

		gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
		TDM_RemoveVote ();
	}

	strcpy (vote.newmap, value);
	vote.flags |= VOTE_MAP;

	return true;
}

/*
==============
TDM_VoteWeapons
==============
Vote to set which weapons are allowed.
*/
qboolean TDM_VoteWeapons (edict_t *ent)
{
	char		modifier;
	unsigned	flags;
	int			i, j;
	qboolean	found;
	const char	*value;

	value = gi.argv(2);

	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote weapons <+/-><rg,cg,rl,..>\n");
		return false;
	}

	//example weapons string, -ssg -bfg +rl
	//special string "all" sets all, eg -all +rg
	flags = (unsigned)g_itemflags->value;

	for (i = 2; i < gi.argc(); i++)
	{
		value = gi.argv (i);

		if (!value[0])
			break;

		modifier = value[0];
		if (modifier == '+' || modifier == '-')
			value++;
		else
			modifier = '+';

		found = false;

		for (j = 0; j < sizeof(weaponvotes) / sizeof(weaponvotes[0]); j++)
		{
			if (!Q_stricmp (value, weaponvotes[j].names[0]) ||
				!Q_stricmp (value, weaponvotes[j].names[1]))
			{
				if (modifier == '-')
					flags |= weaponvotes[j].value;
				else if (modifier == '+')
					flags &= ~weaponvotes[j].value;

				found = true;
				break;
			}
		}

		if (!found)
		{
			if (!Q_stricmp (value, "all"))
			{
				if (modifier == '-')
					flags = 0xFFFFFFFFU;
				else
					flags = 0;
			}
			else
			{
				gi.cprintf (ent, PRINT_HIGH, "Unknown weapon '%s'\n", value);
				return false;
			}
		}
	}

	if ((flags & WEAPON_SHOTGUN) && (flags & WEAPON_SSHOTGUN))
		flags |= AMMO_SHELLS;
	else
		flags &= ~AMMO_SHELLS;

	if ((flags & WEAPON_MACHINEGUN) && (flags & WEAPON_CHAINGUN))
		flags |= AMMO_BULLETS;
	else
		flags &= ~AMMO_BULLETS;

	if ((flags & WEAPON_GRENADES) && (flags & WEAPON_GRENADELAUNCHER))
		flags |= AMMO_GRENADES;
	else
		flags &= ~AMMO_GRENADES;

	if (flags & WEAPON_ROCKETLAUNCHER)
		flags |= AMMO_ROCKETS;
	else
		flags &= ~AMMO_ROCKETS;

	if (flags & WEAPON_RAILGUN)
		flags |= AMMO_SLUGS;
	else
		flags &= ~AMMO_SLUGS;

	if ((flags & WEAPON_BFG10K) && (flags & WEAPON_HYPERBLASTER) &&
		((unsigned)g_powerupflags->value & POWERUP_POWERSCREEN) && ((unsigned)g_powerupflags->value & POWERUP_POWERSHIELD))
		flags |= AMMO_CELLS;
	else
		flags &= ~AMMO_CELLS;

	if (flags == (unsigned)g_itemflags->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "That weapon config is already set!\n");
		return false;
	}

	if (vote.active)
	{
		if (flags == vote.newweaponflags)
		{
			gi.cprintf (ent, PRINT_HIGH, "You've already started a vote for that weapon config.\n");
			return false;
		}

		if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
			return false;

		gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
		TDM_RemoveVote ();
	}

	vote.newweaponflags = flags;
	vote.flags |= VOTE_WEAPONS;

	return true;
}

/*
==============
TDM_VoteKick
==============
Vote to kick someone from the server.
*/
qboolean TDM_VoteKick (edict_t *ent)
{
	edict_t		*victim;
	const char	*value;

	value = gi.argv(2);

	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote kick <name/id>\n");
		return false;
	}

	if (LookupPlayer (gi.argv(2), &victim, ent))
	{
		if (victim->client->pers.admin)
		{
			gi.cprintf (ent, PRINT_HIGH, "You can't kick an admin!\n");
			return false;
		}

		if (victim == ent)
		{
			gi.cprintf (ent, PRINT_HIGH, "You can't kick yourself!\n");
			return false;
		}

		if (vote.active)
		{
			if (victim == vote.victim)
			{
				gi.cprintf (ent, PRINT_HIGH, "You've already started a vote to kick %s.\n", vote.victim->client->pers.netname);
				return false;
			}

			if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
				return false;

			gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
			TDM_RemoveVote ();
		}

		vote.victim = victim;
		vote.flags |= VOTE_KICK;

		return true;
	}

	return false;
}

/*
==============
TDM_VotePowerups
==============
Vote to change the allowed powerups.
*/
qboolean TDM_VotePowerups (edict_t *ent)
{
	char		modifier;
	unsigned	flags;
	int			i, j;
	qboolean	found;
	const char	*value;

	value = gi.argv(2);

	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote powerups <+/-><quad,invul,ps,..>\n");
		return false;
	}

	flags = (unsigned)g_powerupflags->value;

	//example powerups string, -quad -invul +silencer
	//special string "all" sets all, eg -all +quad

	for (i = 2; i < gi.argc(); i++)
	{
		value = gi.argv (i);

		if (!value[0])
			break;

		modifier = value[0];
		if (modifier == '+' || modifier == '-')
			value++;
		else
			modifier = '+';

		found = false;

		for (j = 0; j < sizeof(powerupvotes) / sizeof(powerupvotes[0]); j++)
		{
			if (!Q_stricmp (value, powerupvotes[j].names[0]))
			{
				if (modifier == '-')
					flags |= powerupvotes[j].value;
				else if (modifier == '+')
					flags &= ~powerupvotes[j].value;

				found = true;
				break;
			}
		}

		if (!found)
		{
			if (!Q_stricmp (value, "all"))
			{
				if (modifier == '-')
					flags = 0xFFFFFFFFU;
				else
					flags = 0;
			}
			else
			{
				gi.cprintf (ent, PRINT_HIGH, "Unknown powerup '%s'\n", value);
				return false;
			}
		}
	}

	if ((unsigned)g_powerupflags->value == flags)
	{
		gi.cprintf (ent, PRINT_HIGH, "That powerup config is already set!\n");
		return false;
	}

	if (vote.active)
	{
		if (flags == vote.newpowerupflags)
		{
			gi.cprintf (ent, PRINT_HIGH, "You've already started a vote for that powerup config.\n");
			return false;
		}
		
		if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
			return false;
		
		gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
		TDM_RemoveVote ();
	}

	vote.newpowerupflags = flags;
	vote.flags |= VOTE_POWERUPS;

	return true;
}

/*
==============
TDM_VoteGameMode
==============
Vote to change the game mode. Game mode is overall 'style' of the mod and a change of this will
cause a dump of all clients back to spectator mode and menu.
*/
qboolean TDM_VoteGameMode (edict_t *ent)
{
	int			gamemode;
	const char	*value;

	value = gi.argv(2);

	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote %s <tdm/itdm/1v1>\n", gi.argv(1));
		return false;
	}

	if (!Q_stricmp (value, "tdm"))
		gamemode = GAMEMODE_TDM;
	else if(!Q_stricmp (value, "itdm"))
		gamemode = GAMEMODE_ITDM;
	else if (!Q_stricmp (value, "1v1") || !Q_stricmp (value, "duel"))
		gamemode = GAMEMODE_1V1;
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Unknown game mode: %s\n", value);
		return false;
	}

	if ((int)g_gamemode->value == gamemode)
	{
		gi.cprintf (ent, PRINT_HIGH, "That game mode is already set!\n");
		return false;
	}

	if (vote.active)
	{
		if (vote.gamemode == gamemode)
		{
			gi.cprintf (ent, PRINT_HIGH, "You've already started a vote for that game mode.\n");
			return false;
		}
		
		if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
			return false;	
		
		gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
		TDM_RemoveVote ();
	}

	vote.flags |= VOTE_GAMEMODE;
	vote.gamemode = gamemode;

	return true;
}

/*
==============
TDM_VoteTieMode
==============
Vote how ties are decided.
*/
qboolean TDM_VoteTieMode (edict_t *ent)
{
	int			tiemode;
	const char	*value;

	value = gi.argv(2);

	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote tiemode <none/ot/sd>\nnone: game ties after timelimit\not: overtime added until a winner\nsd: sudden death, first frag wins\n");
		return false;
	}

	if (!Q_stricmp (value, "ot") || !Q_stricmp (value, "overtime"))
		tiemode = 1;
	else if (!Q_stricmp (value, "sd") || !Q_stricmp (value, "sudden death"))
		tiemode = 2;
	else if (!Q_stricmp (value, "none") || !Q_stricmp (value, "tie") || !Q_stricmp (value, "normal"))
		tiemode = 0;
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Unknown mode: %s\n", value);
		return false;
	}

	if (g_tie_mode->value == tiemode)
	{
		gi.cprintf (ent, PRINT_HIGH, "That tie mode is already set!\n");
		return false;
	}

	if (vote.active)
	{
		if (tiemode == vote.tiemode)
		{
			gi.cprintf (ent, PRINT_HIGH, "You've already started a vote for that tie mode.\n");
			return false;
		}

		if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
			return false;

		gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
		TDM_RemoveVote ();
	}

	vote.flags |= VOTE_TIEMODE;
	vote.tiemode = tiemode;

	return true;
}

/*
==============
TDM_VoteTeleMode
==============
Vote how teleporter freezing is handled.
*/
qboolean TDM_VoteTeleMode (edict_t *ent)
{
	int			telemode;
	const char	*value;

	value = gi.argv(2);

	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote telemode <normal/nofreeze>\nnormal: teleporters act like regular Q2, you freeze briefly on exit\nnofreeze: teleporters act like Q3, your velocity is maintained on exit\n");
		return false;
	}

	if (!Q_stricmp (value, "normal") || !Q_stricmp (value, "freeze") || !Q_stricmp (value, "q2"))
		telemode = 0;
	else if (!Q_stricmp (value, "nofreeze") || !Q_stricmp (value, "q3"))
		telemode = 1;
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Unknown mode: %s\n", value);
		return false;
	}

	if (g_teleporter_nofreeze->value == telemode)
	{
		gi.cprintf (ent, PRINT_HIGH, "That teleporter mode is already set!\n");
		return false;
	}

	if (vote.active)
	{
		if (telemode == vote.telemode)
		{
			gi.cprintf (ent, PRINT_HIGH, "You've already started a vote for that teleporter mode.\n");
			return false;
		}

		if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
			return false;

		gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
		TDM_RemoveVote ();
	}

	vote.flags |= VOTE_TELEMODE;
	vote.telemode = telemode;

	return true;
}

/*
==============
TDM_VoteSwitchMode
==============
Vote how weapon switch is handled.
*/
qboolean TDM_VoteSwitchMode (edict_t *ent)
{
	int			switchmode;
	const char	*value;

	value = gi.argv(2);

	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote switchmode <normal/fast/instant/insane>\nnormal: regular Q2 weapon switch speed\nfast: weapon dropping animation is skipped\ninstant: weapon dropping / ready animations are skipped\ninsane: all non-firing animations are skipped\n");
		return false;
	}

	if (!Q_stricmp (value, "fast") || !Q_stricmp (value, "faster"))
		switchmode = 1;
	else if (!Q_stricmp (value, "instant"))
		switchmode = 2;
	else if (!Q_stricmp (value, "insane"))
		switchmode = 3;
	else if (!Q_stricmp (value, "normal") || !Q_stricmp (value, "slow") || !Q_stricmp (value, "default") || !Q_stricmp (value, "q2"))
		switchmode = 0;
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Unknown switch mode: %s\n", value);
		return false;
	}

	if (g_fast_weap_switch->value == switchmode)
	{
		gi.cprintf (ent, PRINT_HIGH, "That weapon switch mode is already set!\n");
		return false;
	}

	if (vote.active)
	{
		if (switchmode == vote.switchmode)
		{
			gi.cprintf (ent, PRINT_HIGH, "You've already started a vote for that weapon switch mode.\n");
			return false;
		}

		if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
			return false;

		gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
		TDM_RemoveVote ();
	}

	vote.switchmode = switchmode;
	vote.flags |= VOTE_SWITCHMODE;

	return true;
}

/*
==============
TDM_VoteOverTimeLimit
==============
Vote how many minutes overtime mode adds.
*/
qboolean TDM_VoteOverTimeLimit (edict_t *ent)
{
	unsigned	limit;
	const char	*value;

	value = gi.argv(2);
	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote %s <mins>\n", gi.argv(1));
		return false;
	}

	limit = atoi (value);
	if (limit < 1 || limit > 1440)
	{
		gi.cprintf (ent, PRINT_HIGH, "Invalid value.\n");
		return false;
	}

	//check current timelimit
	if (g_overtime->value == limit * 60)
	{
		gi.cprintf (ent, PRINT_HIGH, "Overtime is already at %d minute%s.\n", limit, limit == 1 ? "" : "s");
		return false;
	}

	if (vote.active)
	{
		if (limit == vote.overtimemins)
		{
			gi.cprintf (ent, PRINT_HIGH, "You've already started a vote for that overtime limit.\n");
			return false;
		}

		if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
			return false;

		gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
		TDM_RemoveVote ();
	}

	vote.flags |= VOTE_OVERTIME;
	vote.overtimemins = limit;

	return true;
}

/*
==============
TDM_VoteConfig
==============
Vote a config file. A config file is essentially a set of votables set to certain values that all
get applied at once.
*/
qboolean TDM_VoteConfig (edict_t *ent)
{
	const char		*value;
	char			*buff;
	char			path[MAX_QPATH];
	FILE			*confFile;
	size_t			len;
	size_t			i;
	tdm_config_t	config;

	value = gi.argv(2);
	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote config <configname>\n");
		return false;
	}

	len = strlen (value);

	if (len >= MAX_QPATH - 16)
	{
		gi.cprintf (ent, PRINT_HIGH, "Invalid config name.\n");
		return false;
	}

	for (i = 0; i < len; i++)
	{
		if (!isalnum (value[i]) && value[i] != '_' && value[i] != '-')
		{
			gi.cprintf (ent, PRINT_HIGH, "Invalid config name.\n");
			return false;
		}
	}

	Com_sprintf (path, sizeof(path), "./%s/configs/%s.cfg", game.gamedir, value);

	confFile = fopen (path, "rb");
	if (!confFile)
	{
		gi.cprintf (ent, PRINT_HIGH, "Config '%s' not found on server.\n", value);
		return false;
	}

	fseek (confFile, 0, SEEK_END);

	len = ftell (confFile);

	if (len > 8192)
	{
		fclose (confFile);
		gi.cprintf (ent, PRINT_HIGH, "Invalid config file.\n");
		return false;
	}

	rewind (confFile);

	buff = gi.TagMalloc (len + 1, TAG_GAME);

	fread (buff, len, 1, confFile);

	fclose (confFile);

	memset (&config, 0, sizeof(config));

	if (!TDM_ProcessText (buff, len, TDM_ParseVoteConfigLine, &config))
	{
		gi.TagFree (buff);
		gi.cprintf (ent, PRINT_HIGH, "Unable to parse config file.\n");
		return false;
	}

	gi.TagFree (buff);

	vote = config.settings;

	strcpy (vote.configname, config.name);
	vote.flags |= VOTE_CONFIG;

	return true;
}		

/*
==============
TDM_VoteWebConfig
==============
Start a web config download. Web configs are configs stored on opentdm.net that are downloaded
via libcurl, allowing a single player / tournament config to be used by every OpenTDM server.
*/
qboolean TDM_VoteWebConfig (edict_t *ent)
{
	const char		*value;
	size_t			len;
	size_t			i;
	tdm_config_t	*t, *last;
	unsigned		current_time;

	value = gi.argv(2);

	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote webconfig <configname>\n");
		return false;
	}

	len = strlen (value);

	if (len >= MAX_QPATH - 16)
	{
		gi.cprintf (ent, PRINT_HIGH, "Invalid config name.\n");
		return false;
	}

	for (i = 0; i < len; i++)
	{
		if (!isalnum (value[i]) && value[i] != '_' && value[i] != '-')
		{
			gi.cprintf (ent, PRINT_HIGH, "Invalid config name.\n");
			return false;
		}
	}

	//check config cache
	t = last = &tdm_configs;

	current_time = time(NULL);

	while (t->next)
	{
		t = t->next;

		//found it!
		if (!Q_stricmp (t->name, value))
		{
			//check freshness (6 hours max)
			if (current_time - t->last_downloaded > (3600 * 6))
			{
				//too old, have to redownload
				last->next = t->next;
				gi.TagFree (t);
				t = last;
			}
			else
			{				
				//it's fresh, use it immediately
				vote = t->settings;

				strcpy (vote.configname, t->name);
				vote.flags |= VOTE_WEBCONFIG;

				return true;
			}
		}

		last = t;
	}


	Com_sprintf (tdm_download.path , sizeof(tdm_download.path ), "configs/%s.cfg", value);
	tdm_download.initiator = ent;
	tdm_download.type = DL_CONFIG;
	strncpy (tdm_download.name, value, sizeof(tdm_download.name)-1);
	tdm_download.onFinish = TDM_VoteWebConfigResult;

	if (HTTP_QueueDownload (&tdm_download))
		gi.cprintf (ent, PRINT_HIGH, "Fetching web config '%s', please wait...\n", value);

	//we never legitimately start a vote yet, it's handled when the config is actually downloaded
	return false;
}

/*
==============
TDM_VoteWebConfigResult
==============
A web config finished downloading. Be very careful here as the game or player state may have changed
between when the config was requested and when we get called! Note, the download core checks if the
player disconnected, so we don't need to worry about things like that - only game state.
*/
void TDM_VoteWebConfigResult (edict_t *ent, int code, void *param)
{
	tdm_config_t	*config;

	config = (tdm_config_t *)param;

	if (code == 404)
	{
		gi.cprintf (ent, PRINT_HIGH, "Web config '%s' was not found. Visit www.opentdm.net for more information on web configs.\n", tdm_download.name);
		return;
	}
	else if (code == 600)
	{
		gi.cprintf (ent, PRINT_HIGH, "Unable to parse the config file. It may be corrupt.\n");
		return;
	}

	if (!config)
	{
		gi.cprintf (ent, PRINT_HIGH, "The OpenTDM web config service is unavailable at the moment.\n");
		return;
	}

	if (tdm_match_status != MM_WARMUP)
	{
		gi.cprintf (ent, PRINT_HIGH, "Web config '%s' was found, but it is too late to propose settings now.\n", tdm_download.name);
		return;
	}

	if (!ent->client->resp.team)
	{
		gi.cprintf (ent, PRINT_HIGH, "Web config '%s' was found, but you are no longer on a team!\n", tdm_download.name);
		return;
	}

	if (vote.active)
	{
		gi.cprintf (ent, PRINT_HIGH, "Web config '%s' was found, but another vote has already started.\n", tdm_download.name);
		return;
	}

	vote = config->settings;

	strcpy (vote.configname, config->name);
	vote.flags |= VOTE_WEBCONFIG;

	TDM_SetupVote (ent);
	TDM_AnnounceVote ();
	TDM_CheckVote ();
}

/*
==============
TDM_VoteChatMode
==============
Vote how chat is allowed.
*/
qboolean TDM_VoteChat (edict_t *ent)
{
	int			chatmode;
	const char	*value;

	value = gi.argv(2);

	if (!value[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote chat <all/players>\n");
		return false;
	}

	if (!Q_stricmp (value, "players") || !Q_stricmp (value, "team") || !Q_stricmp (value, "nospec") || !Q_stricmp (value, "1"))
		chatmode = 1;
	else if (!Q_stricmp (value, "all") || !Q_stricmp (value, "everyone") || !Q_stricmp (value, "0"))
		chatmode = 0;
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Unknown chat mode: %s\n", value);
		return false;
	}

	if (g_chat_mode->value == chatmode)
	{
		gi.cprintf (ent, PRINT_HIGH, "That chat mode is already set!\n");
		return false;
	}

	if (vote.active)
	{
		if (chatmode == vote.newchatmode)
		{
			gi.cprintf (ent, PRINT_HIGH, "You've already started a vote for that chat mode.\n");
			return false;
		}

		if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
			return false;

		gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
		TDM_RemoveVote ();
	}

	vote.newchatmode = chatmode;
	vote.flags |= VOTE_CHAT;

	return true;
}

/*
==============
TDM_Vote_X
==============
Vote yes or no.
*/
void TDM_Vote_X (edict_t *ent, player_vote_t x, const char *whatisit)
{
	if (!vote.active)
	{
		gi.cprintf (ent, PRINT_HIGH, "No vote in progress.\n");
		return;
	}

	if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
		return;
	
	if (ent->client->resp.vote == VOTE_HOLD)
	{
		ent->client->resp.vote = x;
		gi.bprintf (PRINT_HIGH, "%s voted %s.\n", ent->client->pers.netname, whatisit);
	}
	else if (ent->client->resp.vote == x)
	{
		gi.cprintf (ent, PRINT_HIGH, "You have already voted %s.\n", gi.argv(1));
	}
	else if (ent->client->resp.vote != x)
	{
		ent->client->resp.vote = x;
		gi.bprintf (PRINT_HIGH, "%s changed his vote to %s.\n", ent->client->pers.netname, whatisit);
	}
}

/*
==============
TDM_SetupVote
==============
A player started a vote, set up common vote stuff.
*/
void TDM_SetupVote (edict_t *ent)
{
	vote.initiator = ent;
	vote.end_frame = level.framenum + g_vote_time->value * (1 / FRAMETIME);
	vote.active = true;

	ent->client->resp.vote = VOTE_YES;
}

/*
==============
TDM_Vote_f
==============
Vote command handler. Create new vote.
*/
void TDM_Vote_f (edict_t *ent)
{
	const char	*cmd;
	qboolean	started_new_vote;

	if (!Q_stricmp (gi.argv(0), "yes") || !Q_stricmp (gi.argv(0), "no"))
	{
		cmd = gi.argv(0);
	}
	else
	{
		if (gi.argc() < 2)
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote <timelimit/map/kick/powerups/weapons/gamemode/tiemode/telemode/switchmode> <value>\n"
				"For weapon/powerup votes, specify multiple combinations of + or -, eg -bfg, +quad -invuln, etc\n");
			return;
		}

		cmd = gi.argv(1);
	}

	if (tdm_match_status != MM_WARMUP)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can only propose new settings during warmup.\n");
		return;
	}

	if (!ent->client->resp.team)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must be on a team to vote or propose new settings.\n");
		return;
	}

	//if initiator wants to change vote reset the vote and start again
	if (vote.active && vote.initiator != ent && Q_stricmp (cmd, "yes") && Q_stricmp (cmd, "no"))
	{
		gi.cprintf (ent, PRINT_HIGH, "Another vote is already in progress.\n");
		return;
	}

	started_new_vote = false;

	if (!Q_stricmp (cmd, "timelimit") || !Q_stricmp (cmd, "tl"))
		started_new_vote = TDM_VoteTimeLimit (ent);
	else if (!Q_stricmp (cmd, "map"))
		started_new_vote = TDM_VoteMap (ent);
	else if (!Q_stricmp (cmd, "weapons"))
		started_new_vote = TDM_VoteWeapons (ent);
	else if (!Q_stricmp (cmd, "kick"))
		started_new_vote = TDM_VoteKick (ent);
	else if (!Q_stricmp (cmd, "powerups"))
		started_new_vote = TDM_VotePowerups (ent);
	else if (!Q_stricmp (cmd, "gamemode") || !Q_stricmp (cmd, "mode"))
		started_new_vote = TDM_VoteGameMode (ent);
	else if (!Q_stricmp (cmd, "tiemode"))
		started_new_vote = TDM_VoteTieMode (ent);
	else if (!Q_stricmp (cmd, "telemode"))
		started_new_vote = TDM_VoteTeleMode (ent);
	else if (!Q_stricmp (cmd, "switchmode"))
		started_new_vote = TDM_VoteSwitchMode (ent);
	else if (!Q_stricmp (cmd, "overtime") || !Q_stricmp (cmd, "ot"))
		started_new_vote = TDM_VoteOverTimeLimit (ent);
	else if (!Q_stricmp (cmd, "config"))
		started_new_vote = TDM_VoteConfig (ent);
	else if (!Q_stricmp (cmd, "webconfig"))
		started_new_vote = TDM_VoteWebConfig (ent);
	else if (!Q_stricmp (cmd, "chat"))
		started_new_vote = TDM_VoteChat (ent);
	else if (!Q_stricmp (cmd, "yes"))
		TDM_Vote_X (ent, VOTE_YES, "YES");
	else if (!Q_stricmp (cmd, "no"))
		TDM_Vote_X (ent, VOTE_NO, "NO");
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Unknown vote action '%s'\n", cmd);
		return;
	}

	if (started_new_vote)
	{
		TDM_SetupVote (ent);
		TDM_AnnounceVote ();
	}

	TDM_CheckVote();
}

/*
==============
TDM_CheckVote
==============
Check vote for success or failure.
*/
void TDM_CheckVote (void)
{
	int		vote_hold = 0;
	int		vote_yes = 0;
	int		vote_no = 0;
	edict_t	*ent;

	if ((vote.flags & VOTE_KICK) && !vote.victim->inuse)
	{
		gi.bprintf (PRINT_HIGH, "Vote cancelled.\n");
		TDM_RemoveVote ();
		return;
	}

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		if (!ent->client->resp.team)
			continue;

		if (ent->client->resp.vote == VOTE_YES)
			vote_yes++;
		else if (ent->client->resp.vote == VOTE_NO)
 			vote_no++;
		else if (ent->client->resp.vote == VOTE_HOLD)
 			vote_hold++;
	}

	if (vote_yes > vote_hold + vote_no)
		vote.success = VOTE_SUCCESS;
	else if (vote_no > vote_hold + vote_yes)
		vote.success = VOTE_NOT_SUCCESS;
	else
		vote.success = VOTE_NOT_ENOUGH_VOTES;

	if (vote.success == VOTE_SUCCESS)
	{
		gi.bprintf (PRINT_HIGH, "Vote passed!\n");
		TDM_ApplyVote ();
		TDM_RemoveVote ();
	}
	else if (vote.success == VOTE_NOT_SUCCESS)
	{
		gi.bprintf (PRINT_HIGH, "Vote failed.\n");
		TDM_RemoveVote();
	}
}

/*
==============
TDM_ParseVoteConfigLine
==============
Parse a votable config.
*/
qboolean TDM_ParseVoteConfigLine (char *line, int line_number, void *param)
{
	tdm_config_t	*c;
	char			*p, *variable;

	c = (tdm_config_t *)param;

	p = strchr (line, '\t');
	if (!p)
	{
		gi.dprintf ("WARNING: Malformed line %d '%s'\n", line_number, line);
		return false;
	}

	p[0] = 0;
	p++;

	variable = line;

	if (!p[0])
	{
		gi.dprintf ("WARNING: Malformed line %d '%s'\n", line_number, line);
		return false;
	}

	//no validation is done here to keep things small - these should be validated serverside
	//already. yes, we're trusting the server not to lie to us :).
	if (!strcmp (variable, "timelimit"))
	{
		c->settings.newtimelimit = atoi (p);
		c->settings.flags |= VOTE_TIMELIMIT;
	}
	else if (!strcmp (variable, "gamemode"))
	{
		c->settings.gamemode = atoi(p);
		c->settings.flags |= VOTE_GAMEMODE;
	}
	else if (!strcmp (variable, "tiemode"))
	{
		c->settings.tiemode = atoi(p);
		c->settings.flags |= VOTE_TIEMODE;
	}
	else if (!strcmp (variable, "switchmode"))
	{
		c->settings.switchmode = atoi(p);
		c->settings.flags |= VOTE_SWITCHMODE;
	}
	else if (!strcmp (variable, "telemode"))
	{
		c->settings.telemode = atoi(p);
		c->settings.flags |= VOTE_TELEMODE;
	}
	else if (!strcmp (variable, "map"))
	{
		Q_strncpy (c->settings.newmap, p, sizeof(c->settings.newmap)-1);
		c->settings.flags |= VOTE_MAP;
	}
	else if (!strcmp (variable, "overtime"))
	{
		c->settings.overtimemins = atoi(p);
		c->settings.flags |= VOTE_OVERTIME;
	}
	else if (!strcmp (variable, "weapons"))
	{
		c->settings.newweaponflags = atoi(p);
		c->settings.flags |= VOTE_WEAPONS;
	}
	else if (!strcmp (variable, "powerups"))
	{
		c->settings.newpowerupflags = atoi(p);
		c->settings.flags |= VOTE_POWERUPS;
	}
	else if (!strcmp (variable, "chat"))
	{
		c->settings.newchatmode = atoi(p);
		c->settings.flags |= VOTE_CHAT;
	}
	else if (!strcmp (variable, "description"))
	{
		Q_strncpy (c->description, p, sizeof(c->description)-1);
	}
	else
	{
		gi.dprintf ("WARNING: Unknown variable  on line %d '%s'\n", line_number, variable);
		return false;
	}

	return true;
}

/*
==============
TDM_ConfigDownloaded
==============
A downloaded config finished, store it in memory temporarily
*/
void TDM_ConfigDownloaded (char *buff, int len, int code)
{
	tdm_config_t	*t, *last;

	if (buff)
	{
		t = &tdm_configs;

		while (t->next)
		{
			t = t->next;
		}

		last = t;
		t->next = gi.TagMalloc (sizeof(*t), TAG_GAME);

		t = t->next;

		strcpy (t->name, tdm_download.name);

		if (!TDM_ProcessText (buff, len, TDM_ParseVoteConfigLine, t))
		{
			gi.TagFree (t);
			last->next = NULL;

			gi.dprintf ("TDM_ConfigDownloaded: Parse failed.\n");
			tdm_download.onFinish (tdm_download.initiator, 600, NULL);
			return;
		}

		t->last_downloaded = time(NULL);

		tdm_download.onFinish (tdm_download.initiator, code, t);
	}
	else
	{
		tdm_download.onFinish (tdm_download.initiator, code, NULL);
	}
}
