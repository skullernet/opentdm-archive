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

//OpenTDM core. The functions here are mainly 'behind the scenes' things - match
//setup, timer checks, managing state, configstrings, etc...

#include "g_local.h"
#include "g_tdm.h"

teaminfo_t	teaminfo[MAX_TEAMS];
matchmode_t	tdm_match_status;

int soundcache[MAX_SOUNDS];

typedef struct
{
	const char	*variable_name;
	char		*default_string;
} cvarsave_t;

//anything in this list is modified by voting
//so the default values are saved on startup
//and reset when everyone leaves the server.
cvarsave_t preserved_vars[] = 
{
	{"g_team_a_name", NULL},
	{"g_team_b_name", NULL},
	{"g_team_a_skin", NULL},
	{"g_team_b_skin", NULL},
	{"g_match_time", NULL},
	{"g_itemflags", NULL},
	{"g_powerupflags", NULL},
	{"g_gamemode", NULL},
	{"g_tie_mode", NULL},
	{"g_teleporter_nofreeze", NULL},
	{"g_fast_weap_switch", NULL},
	{"g_overtime", NULL},
	{NULL, NULL},
};

const char *soundnames[17] = {
	"death1.wav",
	"death2.wav",
	"death3.wav",
	"death4.wav",
	"fall1.wav",
	"fall2.wav",
	"gurp1.wav",
	"gurp2.wav",
	"jump1.wav",
	"pain25_1.wav",
	"pain25_2.wav",
	"pain50_1.wav",
	"pain50_2.wav",
	"pain75_1.wav",
	"pain75_2.wav",
	"pain100_1.wav",
	"pain100_2.wav",
};

tdm_download_t	tdm_download;

//a note regarding matchinfo and teamplayers.
//matchinfo is a structure containing information about a single match - from "Fight!" to a team winning.
//it is not used for warmup or anything else like that, only initialized when the match actually begins.
//current_matchinfo describes the current match - game mode, timelimit, map, etc. in the matchinfo_t
//structure is a link to a teamplayer_t * - this is a dynamically allocated array that contains info about
//every player in the match for both teams. teamplayer_t also contains a link back to the matchinfo_t for
//time saving purposes.

//at the end of a match, old_matchinfo becomes current_matchinfo and current_matchinfo teamplayers is NULLed
//to prevent certain things from happening. a few extra pieces of glue are needed here - since matchinfo
//structures are static, pointers in the teamplayer_t need updating to point to the old_matchinfo instead
//of current_matchinfo. if old_matchinfo.teamplayers already exists, it is freed and data about that match
//is gone.

matchinfo_t	current_matchinfo;
matchinfo_t	old_matchinfo;

static char teamStatus[MAX_TEAMS][MAX_TEAMNAME_LENGTH];
static char teamJoinText[MAX_TEAMS][32];

//copy of last game scoreboard for oldscores command
char		old_scoreboard_string[1400];

//static char	last_player_model[MAX_TEAMS][32];

pmenu_t joinmenu[] =
{
	{ NULL,					PMENU_ALIGN_CENTER, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, JoinTeam1 },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, JoinTeam2 },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*Spectate",			PMENU_ALIGN_LEFT, NULL, ToggleChaseCam },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Use [ and ] to move cursor",	PMENU_ALIGN_LEFT, NULL, NULL },
	{ "ENTER to select",	PMENU_ALIGN_LEFT, NULL, NULL },
	{ "ESC to Exit Menu",	PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*" OPENTDM_VERSION,	PMENU_ALIGN_RIGHT, NULL, NULL },
};

/*const pmenu_t helpmenu[][] =
{
	{ "*Quake II - OpenTDM",PMENU_ALIGN_CENTER, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Console Commands",	PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*menu",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Open the OpenTDM menu",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*team <name>",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Join the team <name>",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*vote <vote command>",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Start a vote to change the",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "server settings (see p.2)",			PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*ready",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Toggle your ready status",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*forceready",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Force all your team ready",	PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*timeout",	PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Request a time out",	PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*(Continued...)",	PMENU_ALIGN_RIGHT, NULL, SelectNextHelpPage },
},
{
	{ "*Quake II - OpenTDM",PMENU_ALIGN_CENTER, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Voting Commands",	PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*vote kick <player>",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Kick <player>",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*vote map <map>",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Change to <map>",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*vote timelimit <x>",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Set timelimit to x",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*vote overtime <mode>",			PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Set overtime mode, one of",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "sd or extend",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*vote weapons",					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Show weapon vote menu",	PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*vote powerups",	PMENU_ALIGN_LEFT, NULL, NULL },
	{ "Show powerups vote menu",	PMENU_ALIGN_LEFT, NULL, NULL },
	{ NULL,					PMENU_ALIGN_LEFT, NULL, NULL },
	{ "*(Continued...)",	PMENU_ALIGN_RIGHT, NULL, SelectNextHelpPage },
},
{
	{0}
};*/

/*
==============
TDM_SetColorText
==============
Converts a string to color text (high ASCII)
*/
char *TDM_SetColorText (char *buffer)
{
	size_t	len;
	int		i;

	len = strlen (buffer);
	for (i = 0; i < len; i++)
	{
		if (buffer[i] != '\n')
			buffer[i] |= 0x80;
	}

	return buffer;
}

/*
==============
TDM_SaveDefaultCvars
==============
Save whatever the server admin set so we can restore later.
*/
void TDM_SaveDefaultCvars (void)
{
	cvarsave_t	*preserved;
	cvar_t		*var;

	preserved = preserved_vars;

	while (preserved->variable_name)
	{
		var = gi.cvar (preserved->variable_name, NULL, 0);
		if (!var)
			TDM_Error ("TDM_SaveDefaultCvars: Couldn't preserve %s", preserved->variable_name);

		//note, G_CopyString resets on map change - have to do this one ourselves
		preserved->default_string = gi.TagMalloc (strlen(var->string)+1, TAG_GAME);
		strcpy (preserved->default_string, var->string);

		preserved++;
	}
}

/*
==============
TDM_ResetLevel
==============
Resets the items and triggers / funcs / etc in the level
in preparation for a match.
*/
void TDM_ResetLevel (void)
{
	int i;
	edict_t	*ent;

	//map hasn't loaded yet!
	if (!level.entity_string)
		return;

	//free up any stray ents
	for (ent = g_edicts + 1 + game.maxclients; ent < g_edicts + globals.num_edicts; ent++)
	{
		if (!ent->inuse)
			continue;

		//handle body que specially, just remove effects and unlink it
		if (ent->enttype == ENT_BODYQUE)
		{
			ent->s.modelindex = ent->s.effects = ent->s.sound = ent->s.renderfx = 0;
			gi.unlinkentity (ent);
			continue;
		}

		if (
			(ent->enttype == ENT_DOOR_TRIGGER || ent->enttype == ENT_PLAT_TRIGGER || ent->enttype == ENT_GIB)
			||
			(ent->owner >= (g_edicts + 1) && ent->owner <= (g_edicts + game.maxclients))
			)
			G_FreeEdict (ent);
	}

	///rerun the level entity string
	ParseEntityString (true);

	//immediately droptofloor and setup removed items
	for (ent = g_edicts + 1 + game.maxclients; ent < g_edicts + globals.num_edicts; ent++)
	{
		if (!ent->inuse)
			continue;

		//wision: add/remove items
		if (ent->item)
		{
			for (i = 0; i < sizeof(weaponvotes) / sizeof(weaponinfo_t); i++)
			{
				//this item isn't removed
				if (!((int)g_itemflags->value & weaponvotes[i].value))
					continue;

				//this is a weapon that should be removed
				if (ITEM_INDEX (ent->item) == weaponvotes[i].itemindex)
				{
					G_FreeEdict (ent);
					break;
				}

				//this is ammo for a weapon that should be removed
				if (ITEM_INDEX (ent->item) == GETITEM (weaponvotes[i].itemindex)->ammoindex)
				{
					//special case: cells, grenades, shells
					if ((int)g_itemflags->value & GETITEM(GETITEM(weaponvotes[i].itemindex)->ammoindex)->tag)
					{
						G_FreeEdict (ent);
						break;
					}
				}
			}

			//was removed
			if (!ent->inuse)
				continue;

			//wision: add/remove powerups
			for (i = 0; i < sizeof(powerupvotes) / sizeof(powerupinfo_t); i++)
			{
				//this powerup isn't removed
				if (!((int)g_powerupflags->value & powerupvotes[i].value))
					continue;

				if (ITEM_INDEX (ent->item) == powerupvotes[i].itemindex)
				{
					G_FreeEdict (ent);
					break;
				}
			}

			//was removed
			if (!ent->inuse)
				continue;

			//track how many items have spawned for stats
			TDM_ItemSpawned (ent);
		}

		if (ent->think == droptofloor)
		{
			droptofloor (ent);
			ent->nextthink = 0;
		}
	}
}

/*
==============
TDM_BeginMatch
==============
A match has just started (end of countdown)
*/
void TDM_BeginMatch (void)
{
	edict_t		*ent;

	//level.match_start_framenum = 0;
	level.match_end_framenum = level.framenum + (int)(g_match_time->value / FRAMETIME);
	tdm_match_status = MM_PLAYING;

	//must setup teamplayers before level, or we lose item spawn stats
	TDM_SetupMatchInfoAndTeamPlayers ();

	//respawn the map
	TDM_ResetLevel ();

	gi.bprintf (PRINT_HIGH, "Fight!\n");

	//should already be 0, check this is needed
	teaminfo[TEAM_A].score = teaminfo[TEAM_B].score = 0;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		if (ent->client->resp.team)
		{
			respawn (ent);
		}
	}

	TDM_UpdateConfigStrings (false);
}

/*
==============
TDM_ScoreBoardString
==============
Display TDM scoreboard. Must be unicast or multicast after calling this
function.
*/
char *TDM_ScoreBoardString (edict_t *ent)
{
	//TODO: implement proper scoreboard
	char		entry[1024];
	static char	string[1400];
	int			len;
	int			i, j, k, n;
	int			sorted[2][MAX_CLIENTS];
	int			sortedscores[2][MAX_CLIENTS];
	int			score, total[2], totalscore[2];
	int			last[2];
	int			width[2];
	int			maxplayers;
	int			offset;

	gclient_t	*cl;
	edict_t		*cl_ent;
	int team;
	int maxsize = 1300;
	
	// sort the clients by team and score
	total[0] = total[1] = 0;
	last[0] = last[1] = 0;
	totalscore[0] = totalscore[1] = 0;
	for (i=0 ; i < game.maxclients ; i++)
	{
		cl_ent = g_edicts + 1 + i;
		if (!cl_ent->inuse)
			continue;
		if (game.clients[i].resp.team == TEAM_A)
			team = 0;
		else if (game.clients[i].resp.team == TEAM_B)
			team = 1;
		else
			continue; // unknown team?

		score = game.clients[i].resp.score;
		for (j=0 ; j<total[team] ; j++)
		{
			if (score > sortedscores[team][j])
				break;
		}

		for (k=total[team] ; k>j ; k--)
		{
			sorted[team][k] = sorted[team][k-1];
			sortedscores[team][k] = sortedscores[team][k-1];
		}
		sorted[team][j] = i;
		sortedscores[team][j] = score;
		totalscore[team] += score;
		total[team]++;
	}

	if (total[0] > total[1])
		maxplayers = total[0];
	else
		maxplayers = total[1];

	len = abs(teaminfo[TEAM_A].score);

	if (len > 9)
		width[0] = 2;
	else if (len > 99)
		width[0] = 3;
	else
		width[0] = 1;

	//for negative sign
	if (teaminfo[TEAM_A].score < 0)
		width[0]++;

	len = abs(teaminfo[TEAM_B].score);

	if (len > 9)
		width[1] = 2;
	else if (len > 99)
		width[1] = 3;
	else
		width[1] = 1;

	//for negative sign
	if (teaminfo[TEAM_B].score < 0)
		width[1]++;

	// print level name and exit rules
	// add the clients in sorted order
	*string = 0;
	len = 0;

	offset = maxplayers * 8 + 64;

	// team one
	sprintf(string,
		
		//Team A
		"xv 0 yv 0 picn /players/%s_i.pcx "
		"xv 34 yv 0 string \"%s\" "
		"xv 32 yv 8 num %d 23 "

		//Team B
		"xv 0 yv %d picn /players/%s_i.pcx "
		"xv 34 yv %d string \"%s\" "
		"xv 32 yv %d num %d 24 ",
		teaminfo[TEAM_A].skin, va ("%s (%d player%s)", teaminfo[TEAM_A].name, total[0], total[0] == 1 ? "" : "s"), width[0],
		offset + 0, teaminfo[TEAM_B].skin,
		offset + 0, va ("%s (%d player%s)", teaminfo[TEAM_B].name, total[1], total[1] == 1 ? "" : "s"),
		offset + 8, width[1]);

	len = strlen(string);

	for (i=0 ; i<16 ; i++)
	{
		if (i >= total[0] && i >= total[1])
			break; // we're done

#if 1
		// set up y
		sprintf(entry, "yv %d ", 42 + i * 8);
		if (maxsize - len > strlen(entry)) {
			strcat(string, entry);
			len = strlen(string);
		}
#else
		*entry = 0;
#endif

		// top
		if (i < total[0]) {
			cl = &game.clients[sorted[0][i]];
			cl_ent = g_edicts + 1 + sorted[0][i];

#if 1
			if (current_matchinfo.teamplayers)
			{
				sprintf(entry+strlen(entry),
				"xv 0 yv %d %s \"%2d %-12.12s %2dk %2dd %2ds %2dt %3dp\" ",
				i * 8 + 40, (cl_ent == ent) ? "string2" : "string",
				cl->resp.score, 
				cl->pers.netname,
				cl->resp.teamplayerinfo->enemy_kills,
				cl->resp.teamplayerinfo->deaths,
				cl->resp.teamplayerinfo->suicides,
				cl->resp.teamplayerinfo->team_kills,
				(cl->ping > 999) ? 999 : cl->ping);
			}
			else
			{
				sprintf(entry+strlen(entry),
				"xv 0 yv %d %s \"%s %3d %-12.12s\" ",
				i * 8 + 40, (cl_ent == ent) ? "string2" : "string",
				cl->resp.ready ? "READY" : "     ",
				(cl->ping > 999) ? 999 : cl->ping, 
				cl->pers.netname);
			}

#else
			sprintf(entry+strlen(entry),
				"ctf 0 %d %d %d %d ",
				42 + i * 8,
				sorted[0][i],
				cl->resp.score,
				cl->ping > 999 ? 999 : cl->ping);
#endif

			if (maxsize - len > strlen(entry)) {
				strcat(string, entry);
				len = strlen(string);
				last[0] = i;
			}
		}

		// bottom
		if (i < total[1]) {
			cl = &game.clients[sorted[1][i]];
			cl_ent = g_edicts + 1 + sorted[1][i];

#if 1
			if (current_matchinfo.teamplayers)
			{
				sprintf(entry+strlen(entry),
				"xv 0 yv %d %s \"%2d %-12.12s %2dk %2dd %2ds %2dt %3dp\" ",
				i * 8 + 40 + offset, (cl_ent == ent) ? "string2" : "string",
				cl->resp.score, 
				cl->pers.netname,
				cl->resp.teamplayerinfo->enemy_kills,
				cl->resp.teamplayerinfo->deaths,
				cl->resp.teamplayerinfo->suicides,
				cl->resp.teamplayerinfo->team_kills,
				(cl->ping > 999) ? 999 : cl->ping);
			}
			else
			{
				sprintf(entry+strlen(entry),
				"xv 0 yv %d %s \"%s %3d %-12.12s\" ",
				i * 8 + 40 + offset, (cl_ent == ent) ? "string2" : "string",
				cl->resp.ready ? "READY" : "     ",
				(cl->ping > 999) ? 999 : cl->ping, 
				cl->pers.netname);
			}
#else

			sprintf(entry+strlen(entry),
				"ctf 0 %d %d %d %d ",
				42 + i * 8 + offset,
				sorted[1][i],
				cl->resp.score,
				cl->ping > 999 ? 999 : cl->ping);

#endif
			if (maxsize - len > strlen(entry)) {
				strcat(string, entry);
				len = strlen(string);
				last[1] = i;
			}
		}
	}

	// put in spectators if we have enough room
	if (last[0] > last[1])
		j = last[0];
	else
		j = last[1];

	j = (j + 2) * 8 + 42 + offset;

	k = n = 0;
	if (maxsize - len > 50)
	{
		for (i = 0; i < game.maxclients; i++)
		{
			cl_ent = g_edicts + 1 + i;
			cl = &game.clients[i];
			if (!cl_ent->inuse ||
				cl_ent->solid != SOLID_NOT ||
				cl_ent->client->resp.team != TEAM_SPEC)
				continue;

			if (!k)
			{
				k = 1;
				sprintf(entry, "xv 0 yv %d string2 \"Spectators\" ", j);
				strcat(string, entry);
				len = strlen(string);
				j += 8;
			}

			sprintf(entry+strlen(entry),
				"xv 0 yv %d string \"%s%s %3dp\" ",
				//0, // x
				j, // y
				cl->pers.netname,
				cl->chase_target ? va(" -> %-12.12s", cl->chase_target->client->pers.netname) : "",
				cl->ping > 999 ? 999 : cl->ping);

			if (maxsize - len > strlen(entry)) {
				strcat(string, entry);
				len = strlen(string);
			}
			
			//if (n & 1)
			j += 8;
			n++;
		}
	}

	if (total[0] - last[0] > 1) // couldn't fit everyone
		sprintf(string + strlen(string), "xv 8 yv %d string \"..and %d more\" ",
			offset + 42 + (last[0]+1)*8, total[0] - last[0] - 1);
	if (total[1] - last[1] > 1) // couldn't fit everyone
		sprintf(string + strlen(string), "xv 168 yv %d string \"..and %d more\" ",
			offset + 42 + (last[1]+1)*8, total[1] - last[1] - 1);

	return string;
}

/*
==============
TDM_Is1V1
==============
Return true if it's 1v1, regardless of game mode. Used to avoid printing
team name when teamname = playername due to the 'pseudo 1v1' mode.
*/
qboolean TDM_Is1V1 (void)
{
	if (level.tdm_pseudo_1v1mode || g_gamemode->value == GAMEMODE_1V1)
		return true;

	return false;
}

/*
==============
TDM_BeginCountdown
==============
All players are ready so start the countdown
*/
void TDM_BeginCountdown (void)
{
	gi.bprintf (PRINT_HIGH, "Match Settings:\n%s", TDM_SettingsString ());

	gi.bprintf (PRINT_CHAT, "All players ready! Starting countdown (%g secs)...\n", g_match_countdown->value);

	//remove any vote so it doesn't change settings mid-match :D
	if (vote.active)
		TDM_RemoveVote ();

	tdm_match_status = MM_COUNTDOWN;

	if (teaminfo[TEAM_A].players == 1 && teaminfo[TEAM_B].players == 1)
		level.tdm_pseudo_1v1mode = true;
	else
		level.tdm_pseudo_1v1mode = false;

	//called to apply a temporary hack for people who do 1v1 on tdm mode
	TDM_UpdateTeamNames ();

	level.match_start_framenum = level.framenum + (int)(g_match_countdown->value / FRAMETIME);
}

/*
==============
TDM_EndIntermission
==============
Intermission timer expired or all clients are ready. Reset for another game.
*/
void TDM_EndIntermission (void)
{
	int		i;

	//for test server
	gi.bprintf (PRINT_CHAT, "Please report any bugs at www.opentdm.net.\n");

	//shuffle current stats to old and cleanup any players who never reconnected
	if (current_matchinfo.teamplayers)
	{
		if (old_matchinfo.teamplayers)
			gi.TagFree (old_matchinfo.teamplayers);

		old_matchinfo = current_matchinfo;

		current_matchinfo.teamplayers = NULL;
		current_matchinfo.num_teamplayers = 0;

		teaminfo[TEAM_A].oldscore = teaminfo[TEAM_A].score;
		teaminfo[TEAM_B].oldscore = teaminfo[TEAM_B].score;

		strcpy (old_matchinfo.scoreboard_string, TDM_ScoreBoardString(NULL));

		for (i = 0; i < old_matchinfo.num_teamplayers; i++)
		{
			if (old_matchinfo.teamplayers[i].saved_client)
			{
				gi.TagFree (old_matchinfo.teamplayers[i].saved_client);
				gi.TagFree (old_matchinfo.teamplayers[i].saved_entity);

				old_matchinfo.teamplayers[i].saved_entity = NULL;
				old_matchinfo.teamplayers[i].saved_client = NULL;
			}

			old_matchinfo.teamplayers[i].matchinfo = &old_matchinfo;
		}
	}
	else
		TDM_Error ("TDM_EndIntermission: We should have current_teamplayers but we don't!");

	level.match_score_end_framenum = 0;
	TDM_ResetGameState ();
}

/*
==============
TDM_BeginIntermission
==============
Match has ended, move all clients to spectator mode and set origin, note this
is not the same as EndDMLevel since we aren't changing maps.
*/
void TDM_BeginIntermission (void)
{
	int		i;
	edict_t	*ent, *client;

	level.match_score_end_framenum = level.framenum + (10.0f / FRAMETIME);

	//remove any weapons or similar stuff still in flight
	for (ent = g_edicts + game.maxclients + 1; ent < g_edicts + globals.num_edicts; ent++)
	{
		if ((ent->owner && ent->owner->client) || ent->enttype == ENT_GHOST)
			G_FreeEdict (ent);
	}

	// find an intermission spot
	ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	if (!ent)
	{	
		// the map creator forgot to put in an intermission point...
		ent = G_Find (NULL, FOFS(classname), "info_player_start");
		if (!ent)
			ent = G_Find (NULL, FOFS(classname), "info_player_deathmatch");
	}
	else
	{	
		// chose one of four spots
		i = genrand_int32() & 3;
		while (i--)
		{
			ent = G_Find (ent, FOFS(classname), "info_player_intermission");
			if (!ent)	// wrap around the list
				ent = G_Find (ent, FOFS(classname), "info_player_intermission");
		}
	}

	VectorCopy (ent->s.origin, level.intermission_origin);
	VectorCopy (ent->s.angles, level.intermission_angle);

	// move all clients to the intermission point
	for (i=0 ; i < game.maxclients; i++)
	{
		client = g_edicts + 1 + i;

		if (!client->inuse)
			continue;

		//reset any invites
		client->client->resp.last_invited_by = NULL;

		MoveClientToIntermission (client);
	}
}

/*
==============
TDM_EndMatch
==============
A match has ended through some means.
Overtime / SD is handled in CheckTimes.
*/
void TDM_EndMatch (void)
{
	qboolean	forfeit;
	int			winner, loser;
	edict_t		*ent;

	//have to set this here so the stats system doesn't think we're still playing
	tdm_match_status = MM_SCOREBOARD;

	//show stats
	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (ent->inuse && ent->client->resp.team)
		{
			if (!ent->client->resp.teamplayerinfo)
				TDM_Error ("TDM_EndMatch: Missing teamplayerinfo for client %d", ent - g_edicts);

			TDM_WriteStatsString (ent, ent->client->resp.teamplayerinfo);
		}
	}

	winner = 0;
	loser = 0;
	forfeit = false;

	if (teaminfo[TEAM_A].players == 0)
	{
		winner = TEAM_B;
		loser = TEAM_A;
		forfeit = true;
	}
	else if (teaminfo[TEAM_B].players == 0)
	{
		winner = TEAM_A;
		loser = TEAM_B;
		forfeit = true;
	}
	else if (teaminfo[TEAM_A].score > teaminfo[TEAM_B].score)
	{
		winner = TEAM_A;
		loser = TEAM_B;
	}
	else if (teaminfo[TEAM_B].score > teaminfo[TEAM_A].score)
	{
		winner = TEAM_B;
		loser = TEAM_A;
	}
	else
	{
		gi.bprintf (PRINT_HIGH, "Tie game, %d to %d.\n", teaminfo[TEAM_A].score, teaminfo[TEAM_A].score);
	}

	if (winner)
	{
		if (forfeit)
			gi.bprintf (PRINT_HIGH, "%s wins by forfeit!\n", teaminfo[winner].name);
		else
			gi.bprintf (PRINT_HIGH, "%s wins, %d to %d.\n", teaminfo[winner].name, teaminfo[winner].score, teaminfo[loser].score);
	}

	level.match_resume_framenum = 0;
	level.match_end_framenum = 0;

	TDM_BeginIntermission ();
}

void TDM_Overtime (void)
{
	level.match_end_framenum = level.framenum + (int)(g_overtime->value / FRAMETIME);

	gi.bprintf (PRINT_HIGH, "Scores are tied %d - %d, adding %g minute%s overtime.\n",
		teaminfo[TEAM_A].score, teaminfo[TEAM_A].score, g_overtime->value / 60, g_overtime->value / 60 == 1 ? "" : "s");

	tdm_match_status = MM_OVERTIME;
}

void TDM_SuddenDeath (void)
{
	gi.bprintf (PRINT_HIGH, "Scores are tied %d - %d, entering Sudden Death!\n", teaminfo[TEAM_A].score, teaminfo[TEAM_A].score);

	tdm_match_status = MM_SUDDEN_DEATH;
}

/*
==============
TDM_NagUnreadyPlayers
==============
Show who isn't ready, only called if >= 50% of other players are ready.
*/
void TDM_NagUnreadyPlayers (void)
{
	char	message[1000];
	edict_t	*ent;
	int		len;

	len = 0;

	message[0] = '\0';

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		if (ent->client->resp.team == TEAM_SPEC)
			continue;

		if (ent->client->resp.ready)
			continue;

		if (message[0])
		{
			len += 2;
			strcat (message, ", ");
		}

		len += strlen (ent->client->pers.netname);
		strcat (message, ent->client->pers.netname);

		if (len >= sizeof(message)-20)
			break;
	}

	gi.bprintf (PRINT_CHAT, "Waiting on %s\n", message);
}

/*
==============
TDM_FixDeltaAngles
==============
Set angles after a restart of a match otherwise movement done during frozen
will screw things up.
*/
void TDM_FixDeltaAngles (void)
{
	int		i;
	edict_t	*ent;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		//set the delta angle
		for (i=0 ; i<3 ; i++)
			ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(ent->client->v_angle[i] - ent->client->resp.cmd_angles[i]);
	}
}

/*
==============
TDM_UpdateServerInfo
==============
Update server info visible to browsers.
*/
void TDM_UpdateServerInfo (void)
{

}

/*
==============
TDM_CheckTimes
==============
Check miscellaneous timers, eg match start countdown
*/
void TDM_CheckTimes (void)
{
	if (tdm_match_status < MM_PLAYING && level.match_start_framenum)
	{
		int		remaining;

		remaining = level.match_start_framenum - level.framenum;

		if (remaining == (int)(10.4f / FRAMETIME))
		{
			gi.sound (world, 0, gi.soundindex ("world/10_0.wav"), 1, ATTN_NONE, 0);
		}
		else if (remaining > 0 && remaining <= (int)(5.0f / FRAMETIME) && remaining % (int)(1.0f / FRAMETIME) == 0)
		{
			gi.bprintf (PRINT_HIGH, "%d\n", (int)(remaining * FRAMETIME));
		}
		else if (remaining == 0)
		{
			TDM_BeginMatch ();
		}
	}

	if (level.match_resume_framenum)
	{
		int		remaining;

		remaining = level.match_resume_framenum - level.realframenum;		
		
		if (remaining > 0 && remaining <= (int)(5.0f / FRAMETIME) && remaining % (int)(1.0f / FRAMETIME) == 0)
		{
			gi.bprintf (PRINT_HIGH, "%d\n", (int)(remaining * FRAMETIME));
		}
		else if (remaining == 0)
		{
			TDM_FixDeltaAngles ();
			gi.bprintf (PRINT_HIGH, "Fight!\n");

			level.tdm_timeout_caller = 0;
			level.match_resume_framenum = 0;
			tdm_match_status = level.last_tdm_match_status;

			UpdateMatchStatus ();
		}
	}

	if (level.match_end_framenum)
	{
		int		remaining;

		if (tdm_match_status == MM_SUDDEN_DEATH)
		{
			if (teaminfo[TEAM_A].score != teaminfo[TEAM_B].score)
				TDM_EndMatch ();
		}
		else
		{
			remaining = level.match_end_framenum - level.framenum;

			if (remaining == (int)(10.4f / FRAMETIME))
			{
				gi.sound (world, 0, gi.soundindex ("world/10_0.wav"), 1, ATTN_NONE, 0);
			}
			/*else if (remaining > 0 && remaining <= (int)(5.0f / FRAMETIME) && remaining % (int)(1.0f / FRAMETIME) == 0)
			{
				gi.bprintf (PRINT_HIGH, "%d\n", (int)(remaining * FRAMETIME));
			}*/
			else if (remaining == 0)
			{
				if (teaminfo[TEAM_A].score == teaminfo[TEAM_B].score)
				{
					if (g_tie_mode->value == 1)
						TDM_Overtime ();
					else if (g_tie_mode->value == 2)
						TDM_SuddenDeath ();
					else
						TDM_EndMatch ();
				}
				else
					TDM_EndMatch ();
			}
		}
	}

	//end timeout, regardless of what's happening. prevents single player from
	//destroying server should they disconnect eg in 1v1 and never reconnect
	if (level.timeout_end_framenum)
	{
		int		remaining;

		remaining = level.timeout_end_framenum - level.realframenum;

		if (remaining == SECS_TO_FRAMES(60))
		{
			gi.bprintf (PRINT_HIGH, "Automatically resuming in 1 minute.\n");
		}

		if (remaining == 0)
			TDM_ResumeGame ();
	}

	//end of map intermission - same as regular dm style
	if (level.intermissionframe && level.framenum - level.intermissionframe == SECS_TO_FRAMES(g_intermission_time->value))
	{
		level.exitintermission = 1;
	}

	if (level.next_ready_nag_framenum && level.next_ready_nag_framenum == level.framenum)
	{
		TDM_NagUnreadyPlayers ();
		level.next_ready_nag_framenum = level.framenum + SECS_TO_FRAMES(20);
	}

	//between match intermission - scores only
	if (level.match_score_end_framenum)
	{
		int	remaining;

		remaining = level.match_score_end_framenum - level.framenum;

		if (remaining <= 0)
			TDM_EndIntermission ();
	}

	if (vote.active && level.framenum == vote.end_frame)
	{
		gi.bprintf (PRINT_HIGH, "Vote failed.\n");
		TDM_RemoveVote ();
	}
}

/*
==============
TDM_CheckMatchStart
==============
See if everyone is ready and there are enough players to start a countdown
*/
void TDM_CheckMatchStart (void)
{
	edict_t	*ent;
	int		ready[MAX_TEAMS];
	int		total_players, total_ready;

	if (tdm_match_status >= MM_PLAYING)
		return;

	ready[TEAM_A] = ready[TEAM_B] = 0;
	total_ready = total_players = 0;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		if (ent->client->resp.team == TEAM_SPEC)
			continue;

		if (ent->client->resp.ready)
		{
			total_ready++;
			ready[ent->client->resp.team]++;
		}

		total_players++;
	}

	if (teaminfo[TEAM_A].players && ready[TEAM_A] == teaminfo[TEAM_A].players)
		teaminfo[TEAM_A].ready = true;
	else
		teaminfo[TEAM_A].ready = false;

	if (teaminfo[TEAM_B].players && ready[TEAM_B] == teaminfo[TEAM_B].players)
		teaminfo[TEAM_B].ready = true;
	else
		teaminfo[TEAM_B].ready = false;

	if (teaminfo[TEAM_A].ready && teaminfo[TEAM_B].ready)
	{
		level.next_ready_nag_framenum = 0;

		//wision: do NOT restart match during the match
		//r1: under what conditions can/did this happen? late joining shouldn't be possible?
		if (tdm_match_status < MM_COUNTDOWN)
			TDM_BeginCountdown ();
	}
	else
	{
		if (total_players >= 2 && total_ready >= total_players / 2)
		{
			if (!level.next_ready_nag_framenum)
				level.next_ready_nag_framenum = level.framenum + SECS_TO_FRAMES(20);
		}
		else
			level.next_ready_nag_framenum = 0;

		if (level.match_start_framenum)
		{
			//stop the .wav file, icky.
			G_StuffCmd (NULL, "stopsound\n");
			gi.bprintf (PRINT_CHAT, "Countdown aborted!\n");
			level.match_start_framenum = 0;
			level.tdm_pseudo_1v1mode = false;
			tdm_match_status = MM_WARMUP;
		}
	}
}

/*
==============
LookupPlayer
==============
Look up a player by partial subname, full name or client id. If multiple
matches, show a list. Return 0 on failure. Case insensitive.
*/
int LookupPlayer (const char *match, edict_t **out, edict_t *ent)
{
	int			matchcount;
	int			numericMatch;
	edict_t		*p;
	char		lowered[32];
	char		lowermatch[32];
	const char	*m;

	if (!match[0])
		return 0;

	matchcount = 0;
	numericMatch = 0;

	m = match;

	while (m[0])
	{
		if (!isdigit (m[0]))
		{
			numericMatch = -1;
			break;
		}
		m++;
	}

	if (numericMatch == 0)
	{
		numericMatch = strtoul (match, NULL, 10);

		if (numericMatch < 0 || numericMatch >= game.maxclients)
		{
			if (ent)
				gi.cprintf (ent, PRINT_HIGH, "Invalid client id %d.\n", numericMatch);
			return 0;
		}
	}

	if (numericMatch == -1)
	{
		Q_strncpy (lowermatch, match, sizeof(lowermatch)-1);
		Q_strlwr (lowermatch);

		for (p = g_edicts + 1; p <= g_edicts + game.maxclients; p++)
		{
			if (!p->inuse)
				continue;

			Q_strncpy (lowered, p->client->pers.netname, sizeof(lowered)-1);
			Q_strlwr (lowered);

			if (!strcmp (lowered, lowermatch))
			{
				*out = p;
				return 1;
			}

			if (strstr (lowered, lowermatch))
			{
				matchcount++;
				*out = p;
				continue;
			}
		}

		if (matchcount == 1)
		{
			return 1;
		}
		else if (matchcount > 1)
		{
			if (ent)
				gi.cprintf (ent, PRINT_HIGH, "'%s' matches multiple players.\n", match);
			return 0;
		}
	}
	else
	{
		p = g_edicts + 1 + numericMatch;

		if (!p->inuse)
		{
			if (ent)
				gi.cprintf (ent, PRINT_HIGH, "Client %d is not active.\n", numericMatch);
			return 0;
		}

		*out = p;
		return 1;
	}

	if (ent)
		gi.cprintf (ent, PRINT_HIGH, "No player match found for '%s'\n", match);
	return 0;
}

/*
==============
TDM_UpdateTeamNames
==============
A rather messy function to handle team names in TDM and 1v1. Assigns "dynamic" team names
if two players start a 1v1 game while in game mode TDM.
*/
void TDM_UpdateTeamNames (void)
{
	edict_t	*ent;

	if (g_gamemode->value == GAMEMODE_1V1)
	{
		if (teaminfo[TEAM_A].captain)
		{
			if (strcmp (teaminfo[TEAM_A].name, teaminfo[TEAM_A].captain->client->pers.netname))
			{
				strncpy (teaminfo[TEAM_A].name, teaminfo[TEAM_A].captain->client->pers.netname, sizeof(teaminfo[TEAM_A].name)-1);
				g_team_a_name->modified = true;
			}
		}
		else
		{
			if (strcmp (teaminfo[TEAM_A].name, "Player 1"))
			{
				strcpy (teaminfo[TEAM_A].name, "Player 1");
				g_team_a_name->modified = true;
			}
		}	

		if (teaminfo[TEAM_B].captain)
		{
			if (strcmp (teaminfo[TEAM_B].name, teaminfo[TEAM_B].captain->client->pers.netname))
			{
				strncpy (teaminfo[TEAM_B].name, teaminfo[TEAM_B].captain->client->pers.netname, sizeof(teaminfo[TEAM_B].name)-1);
				g_team_b_name->modified = true;
			}
		}
		else
		{
			if (strcmp (teaminfo[TEAM_B].name, "Player 2"))
			{
				strcpy (teaminfo[TEAM_B].name, "Player 2");
				g_team_b_name->modified = true;
			}
		}	
	}
	else
	{
		if (TDM_Is1V1())
		{
			edict_t	*p1, *p2;

			p1 = TDM_FindPlayerForTeam (TEAM_A);
			p2 = TDM_FindPlayerForTeam (TEAM_B);

			//one of the players might have disappeared due to a disconnect
			if (p1 && p2)
			{
				if (strcmp (teaminfo[TEAM_A].name, p1->client->pers.netname))
				{
					strncpy (teaminfo[TEAM_A].name, p1->client->pers.netname, sizeof(teaminfo[TEAM_A].name)-1);
					g_team_a_name->modified = true;
				}

				if (strcmp (teaminfo[TEAM_B].name, p2->client->pers.netname))
				{
					strncpy (teaminfo[TEAM_B].name, p2->client->pers.netname, sizeof(teaminfo[TEAM_B].name)-1);
					g_team_b_name->modified = true;
				}
			}
		}
		else
		{
			if (strcmp (teaminfo[TEAM_A].name, g_team_a_name->string))
			{
				strncpy (teaminfo[TEAM_A].name, g_team_a_name->string, sizeof(teaminfo[TEAM_A].name)-1);
				g_team_a_name->modified = true;
			}

			if (strcmp (teaminfo[TEAM_B].name, g_team_b_name->string))
			{
				strncpy (teaminfo[TEAM_B].name, g_team_b_name->string, sizeof(teaminfo[TEAM_B].name)-1);
				g_team_b_name->modified = true;
			}
		}
	}

	if (!g_team_a_name->modified && !g_team_b_name->modified)
		return;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		if (ent->client->resp.team == TEAM_A && g_team_a_name->modified)
		{
			if (TDM_Is1V1())
				gi.configstring (CS_TDM_SPECTATOR_STRINGS + (ent - g_edicts) - 1, ent->client->pers.netname);
			else
				gi.configstring (CS_TDM_SPECTATOR_STRINGS + (ent - g_edicts) - 1, va("%s (%s)", ent->client->pers.netname, teaminfo[TEAM_A].name));
		}
		else if (ent->client->resp.team == TEAM_B && g_team_b_name->modified)
		{
			if (TDM_Is1V1())
				gi.configstring (CS_TDM_SPECTATOR_STRINGS + (ent - g_edicts) - 1, ent->client->pers.netname);
			else
				gi.configstring (CS_TDM_SPECTATOR_STRINGS + (ent - g_edicts) - 1, va("%s (%s)", ent->client->pers.netname, teaminfo[TEAM_B].name));
		}
	}
}

/*
==============
TDM_ResetVotableVariables
==============
Everyone has left the server, so reset anything they voted back to defaults
*/
void TDM_ResetVotableVariables (void)
{
	cvarsave_t	*var;

	gi.dprintf ("Resetting votable variables to defaults.\n");

	var = preserved_vars;

	while (var->variable_name)
	{
		if (!var->default_string)
			TDM_Error ("TDM_ResetVotableVariables: Preserved variable %s with no default", var->variable_name);
		gi.cvar_forceset (var->variable_name, var->default_string);
		var++;
	}

	TDM_UpdateConfigStrings (true);
}	

void TDM_ResumeGame (void)
{
	if (tdm_match_status != MM_TIMEOUT)
		TDM_Error ("TDM_ResumeGame caled with match state %d", tdm_match_status);

	gi.sound (world, 0, gi.soundindex ("world/10_0.wav"), 1, ATTN_NONE, 0);
	gi.bprintf (PRINT_CHAT, "Game resuming in 10 seconds. Match time remaining: %s\n", TDM_SecsToString(FRAMES_TO_SECS(level.match_end_framenum - level.framenum)));

	level.timeout_end_framenum = 0;
	level.match_resume_framenum = level.realframenum + SECS_TO_FRAMES(10.4);
}

/*
==============
TDM_SetCaptain
==============
Set ent to be a captain of team, ent can be NULL to remove captain
*/
void TDM_SetCaptain (int team, edict_t *ent)
{
	teaminfo[team].captain = ent;

	//no announce in 1v1, but captain is still silently used.
	if (ent && g_gamemode->value != GAMEMODE_1V1)
	{
		gi.bprintf (PRINT_HIGH, "%s became captain of '%s'\n", ent->client->pers.netname, teaminfo[team].name);
		gi.cprintf (ent, PRINT_CHAT, "You are the captain of '%s'\n", teaminfo[team].name);
	}
}

/*
==============
TDM_SetupSounds
==============
First time setup of sound paths, caches commonly used client sounds indexes.
*/
void TDM_SetupSounds (void)
{
	int		i;
	char	path[MAX_QPATH];

	strcpy (path, "*");

	for (i = 0; i < SND_MAX; i++)
	{
		strcpy (path + 1, soundnames[i]);
		soundcache[i] = gi.soundindex (path);
	}
}

/*
==============
CountPlayers
==============
Count how many players each team has
*/
void CountPlayers (void)
{
	edict_t	*ent;
	int		i;
	int		total;

	for (i = 0; i < MAX_TEAMS; i++)
		teaminfo[i].players = 0;

	total = 0;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (ent->inuse)
		{
			teaminfo[ent->client->resp.team].players++;
			total++;
		}
	}
}

/*
==============
UpdateMatchStatus
==============
Update match status (end match when whole team leaves i.e.)
*/
void UpdateMatchStatus (void)
{
	int team;

	if (tdm_match_status < MM_PLAYING || tdm_match_status == MM_SCOREBOARD || tdm_match_status == MM_TIMEOUT)
		return;

	for (team = 1; team < MAX_TEAMS; team++)
	{
		if (teaminfo[team].players < 1)
		{
			TDM_EndMatch ();
			break;
		}
	}
}

/*
==============
UpdateTeamMenu
==============
Update the join menu to reflect team names / player counts
*/
void UpdateTeamMenu (void)
{
	edict_t		*ent;
	int			i;
	static char	openTDMBanner[32];
	static char *gameString[] = {
		"TDM",
		"ITDM",
		"1v1"
	};

	for (i = 0; i < MAX_TEAMS; i++)
	{
		sprintf (teamStatus[i], "  (%d player%s)", teaminfo[i].players, teaminfo[i].players == 1 ? "" : "s");
		sprintf (teamJoinText[i], "*Join %.20s", teaminfo[i].name);
	}

	sprintf (openTDMBanner, "*Quake II - OpenTDM (%s)", gameString[(int)g_gamemode->value]);

	joinmenu[0].text = openTDMBanner;

	joinmenu[3].text = teamJoinText[1];
	joinmenu[4].text = teamStatus[1];

	joinmenu[6].text = teamJoinText[2];
	joinmenu[7].text = teamStatus[2];

	//joinmenu[7].text = teamJoinText[0];
	joinmenu[10].text = teamStatus[0];

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (ent->inuse && ent->client->menu.active && ent->client->menu.entries == joinmenu)
		{
			PMenu_Update (ent);
			gi.unicast (ent, true);
		}
	}
}

/*
==============
TDM_TeamsChanged
==============
The teams have changed in some way, so check everything out
*/
void TDM_TeamsChanged (void)
{
	CountPlayers ();
	TDM_UpdateTeamNames ();
	UpdateTeamMenu ();
	UpdateMatchStatus ();
	TDM_CheckMatchStart ();
}



/*
==============
TDM_SetupSpawns
==============
Keeps track of spawn points rather than counting/looping each time.
*/
void TDM_SetupSpawns (void)
{
	int		count;
	edict_t	*spot;

	count = 0;
	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL)
	{
		level.spawns[count] = spot;
		count++;

		if (count > 32)
			break;
	}

	level.numspawns = count;
}

/*
==============
TDM_MapChanged
==============
Called when a new level is about to begin.
*/
void TDM_MapChanged (void)
{
	//check console didn't try to set gamemode to invalid value
	if (g_gamemode->value > 2 || g_gamemode->value < 0)
		gi.error ("g_gamemode: invalid value");

	TDM_ResetGameState ();
	TDM_SetSkins ();
	TDM_SetupSounds ();
	TDM_UpdateConfigStrings (true);
	TDM_SetupSpawns ();
}

/*
==============
TDM_ResetGameState
==============
Reset the game state after a match has completed or a map / mode change.
*/
void TDM_ResetGameState (void)
{
	edict_t		*ent;

	level.tdm_pseudo_1v1mode = false;
	level.match_start_framenum = 0;
	tdm_match_status = MM_WARMUP;
	TDM_ResetLevel ();

	//don't memset, since we have info we do actually want to preserve
	teaminfo[TEAM_A].score = teaminfo[TEAM_B].score = 0;
	teaminfo[TEAM_A].players = teaminfo[TEAM_B].players = 0;
	teaminfo[TEAM_A].captain = teaminfo[TEAM_B].captain = NULL;
	teaminfo[TEAM_A].locked = teaminfo[TEAM_B].locked = false;
	teaminfo[TEAM_A].ready = teaminfo[TEAM_B].ready = false;

	TDM_UpdateTeamNames ();

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (ent->inuse)
		{
			ent->client->resp.last_command_frame = 0;
			ent->client->resp.last_invited_by = NULL;
			ent->client->resp.score = 0;
			ent->client->resp.teamplayerinfo = NULL;

			ent->client->showscores = false;
			ent->client->showoldscores = false;

			if (ent->client->resp.team != TEAM_SPEC)
			{
				ent->client->resp.team = TEAM_SPEC;
				PutClientInServer (ent);
			}
		}
	}

	//run these functions only after we've moved everyone to spec mode
	CountPlayers ();
	UpdateTeamMenu ();

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (ent->inuse)
			TDM_ShowTeamMenu (ent);
	}
}

/*
==============
TDM_Init
==============
Single time initialization stuff.
*/
void TDM_Init (void)
{
	cvar_t	*var;

	HTTP_Init ();

	var = gi.cvar ("game", NULL, 0);
	if (!var)
		gi.error ("Couldn't determine game directory");

	strncpy (game.gamedir, var->string, sizeof(game.gamedir)-1);

	//ensure R1Q2 entflags are available since we use them for all projectiles
	var = gi.cvar ("sv_new_entflags", NULL, 0);
	if (!var)
	{
		//super cheesy notice!
		gi.dprintf ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		gi.dprintf ("               W A R N I N G !\n");
		gi.dprintf ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
		gi.dprintf ("\n");
		gi.dprintf ("  OpenTDM is designed to use some of R1Q2s\n");
		gi.dprintf ("  new entity flags. Your server does not\n");
		gi.dprintf ("  appear to be running R1Q2, or is out of\n");
		gi.dprintf ("  date. Some features may not work correctly.\n");
		gi.dprintf ("\n");
		gi.dprintf ("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
	}
	else
	{
		gi.cvar_set ("sv_new_entflags", "1");
	}

	strcpy (teaminfo[0].name, "Spectators");
	strcpy (teaminfo[0].skin, "male/grunt");

	//this is used as index in some arrays, must be valid!
	if (g_gamemode->value > 2 || g_gamemode->value < 0)
		gi.error ("g_gamemode: invalid value");

	TDM_SaveDefaultCvars ();

	if (g_gamemode->value == GAMEMODE_ITDM)
		dmflags = gi.cvar_set ("dmflags", g_itdmflags->string);
	else if (g_gamemode->value == GAMEMODE_TDM)
		dmflags = gi.cvar_set ("dmflags", g_tdmflags->string);
	else if (g_gamemode->value == GAMEMODE_1V1)
		dmflags = gi.cvar_set ("dmflags", g_1v1flags->string);

	TDM_ResetGameState ();
}

/*
==============
TDM_SetSkins
==============
Setup skin configstrings.
*/
void TDM_SetSkins (void)
{
	edict_t		*ent;
	const char	*newskin, *oldskin;
	unsigned 	i;
	//int			index;

	for (i = TEAM_A; i <= TEAM_B; i++)
	{
		oldskin = teaminfo[i].skin;

		if (i == TEAM_A)
		{
			//index = CS_TDM_TEAM_A_PIC;
			newskin = g_team_a_skin->string;
		}
		else
		{
			//index = CS_TDM_TEAM_B_PIC;
			newskin = g_team_b_skin->string;
		}

		if (!strcmp (oldskin, newskin))
			continue;

		//gi.configstring (index, va("/players/%s_i.pcx", newskin));

		strncpy (teaminfo[i].skin, newskin, sizeof(teaminfo[i].skin)-1);

		for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
		{
			if (!ent->inuse)
				continue;

			if (ent->client->resp.team == i)
			{
				gi.configstring (CS_PLAYERSKINS + (ent - g_edicts) - 1, va("%s\\%s", ent->client->pers.netname, teaminfo[i].skin));
			}	
		}

		//fix skin hack for spectators
		for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
		{
			if (!ent->inuse)
				continue;

			if (ent->client->chase_target && ent->client->chase_mode == CHASE_EYES)
				ChaseEyeHack (ent, ent->client->chase_target, NULL);
		}
	}

	//TDM_UpdateSounds ();
}

/*
==============
TDM_SecsToString
==============
Convert secs to mm:ss string.
*/
const char *TDM_SecsToString (int seconds)
{
	static char	time_buffer[32];
	int			mins;

	mins = seconds / 60;
	seconds -= (mins * 60);

	sprintf (time_buffer, "%d:%.2d", mins, seconds);

	return time_buffer;
}

/*
==============
TDM_UpdateConfigStrings
==============
Check any cvar and other changes and update relevant configstrings
*/
void TDM_UpdateConfigStrings (qboolean forceUpdate)
{
	static int			last_timeout_remaining = -1;
	static int			last_time_remaining = -1;
	static int			last_scores[MAX_TEAMS] = {-9999, -9999, -9999};
	static qboolean		last_ready_status[MAX_TEAMS] = {false, false, false};
	static matchmode_t	last_mode = MM_INVALID;
	int					time_remaining;
	int					timeout_remaining;

	if (g_team_a_name->modified || forceUpdate)
	{
		g_team_a_name->modified = false;
		sprintf (teaminfo[TEAM_A].statname, "%31s", teaminfo[TEAM_A].name);
		gi.configstring (CS_TDM_TEAM_A_NAME, teaminfo[TEAM_A].statname);
	}

	if (g_team_b_name->modified || forceUpdate)
	{
		g_team_b_name->modified = false;
		sprintf (teaminfo[TEAM_B].statname, "%31s", teaminfo[TEAM_B].name);
		gi.configstring (CS_TDM_TEAM_B_NAME, teaminfo[TEAM_B].statname);
	}

	if (g_team_a_skin->modified || g_team_b_skin->modified || forceUpdate)
	{
		g_team_a_skin->modified = g_team_b_skin->modified = false;
		TDM_SetSkins ();
	}

	if (tdm_match_status != last_mode || forceUpdate ||
		last_ready_status[TEAM_A] != teaminfo[TEAM_A].ready ||
		last_ready_status[TEAM_B] != teaminfo[TEAM_B].ready)
	{
		last_mode = tdm_match_status;

		switch (tdm_match_status)
		{
			//force scores to update
			case MM_PLAYING:
				last_scores[TEAM_A] = last_scores[TEAM_B] = -9999;
				break;

			//note, we shouldn't need to do anything when we are mm_countdown, but fall through just to be safe
			case MM_COUNTDOWN:
			case MM_WARMUP:
				if (teaminfo[TEAM_A].ready != last_ready_status[TEAM_A] || forceUpdate)
				{
					last_ready_status[TEAM_A] = teaminfo[TEAM_A].ready;

					if (teaminfo[TEAM_A].ready)
						sprintf (teaminfo[TEAM_A].statstatus, "%15s", "READY");
					else
						sprintf (teaminfo[TEAM_A].statstatus, "%15s", "WARMUP");

					gi.configstring (CS_TDM_TEAM_A_STATUS, teaminfo[TEAM_A].statstatus);
				}

				if (teaminfo[TEAM_B].ready != last_ready_status[TEAM_B] || forceUpdate)
				{
					last_ready_status[TEAM_B] = teaminfo[TEAM_B].ready;

					if (teaminfo[TEAM_B].ready)
						sprintf (teaminfo[TEAM_B].statstatus, "%15s", "READY");
					else
						sprintf (teaminfo[TEAM_B].statstatus, "%15s", "WARMUP");

					gi.configstring (CS_TDM_TEAM_B_STATUS, teaminfo[TEAM_B].statstatus);
				}
				break;

			case MM_SUDDEN_DEATH:
				gi.configstring (CS_TDM_TIMELIMIT_STRING, "Sudden Death");
				break;

			default:
				//nothing to do!
				break;
		}
	}

	if (tdm_match_status >= MM_PLAYING)
	{
		int		i;

		for (i = TEAM_A; i <= TEAM_B; i++)
		{
			if (last_scores[i] != teaminfo[i].score || forceUpdate)
			{
				last_scores[i] = teaminfo[i].score;
				sprintf (teaminfo[i].statstatus, "%15d", teaminfo[i].score);
				gi.configstring (CS_TDM_TEAM_A_STATUS + (i - TEAM_A), teaminfo[i].statstatus);
			}
		}
	}

	switch (tdm_match_status)
	{
		case MM_COUNTDOWN:
			timeout_remaining = 0;
			time_remaining = level.match_start_framenum - level.framenum;
			break;
		case MM_TIMEOUT:
			if (level.match_resume_framenum)
				timeout_remaining = level.match_resume_framenum - level.realframenum;
			else
				timeout_remaining = level.timeout_end_framenum - level.realframenum;
			time_remaining = level.match_end_framenum - level.framenum;
			break;
		case MM_WARMUP:
			timeout_remaining = 0;
			time_remaining = g_match_time->value * 10 - 1;
			break;
		case MM_SUDDEN_DEATH:
			timeout_remaining = 0;
			time_remaining = 0;
			break;
		default:
			timeout_remaining = 0;
			time_remaining = level.match_end_framenum - level.framenum;
			break;
	}

	if (time_remaining != last_time_remaining || forceUpdate)
	{
		static int	last_secs = -1;
		char		time_buffer[8];
		int			mins, secs;

		last_time_remaining = time_remaining;

		secs = ceil((float)time_remaining * FRAMETIME);

		if (secs < 0)
			secs = 0;

		if (secs != last_secs || forceUpdate)
		{
			last_secs = secs;

			mins = secs / 60;
			secs -= (mins * 60);

			sprintf (time_buffer, "%2d:%.2d", mins, secs);

			if (last_secs < 60)
				TDM_SetColorText (time_buffer);
			else if (last_secs == 60 && tdm_match_status >= MM_PLAYING)
				gi.bprintf (PRINT_HIGH, "1 minute remaining.\n");

			gi.configstring (CS_TDM_TIMELIMIT_STRING, time_buffer);
		}
	}

	if (timeout_remaining != last_timeout_remaining)
	{
		static int	last_secs = -1;
		char		time_buffer[8];
		int			mins, secs;

		last_timeout_remaining = timeout_remaining;

		secs = ceil((float)timeout_remaining * FRAMETIME);

		if (secs < 0)
			secs = 0;

		if (secs != last_secs || forceUpdate)
		{
			last_secs = secs;

			mins = secs / 60;
			secs -= (mins * 60);

			sprintf (time_buffer, "%2d:%.2d", mins, secs);

			if (last_secs < 60)
				TDM_SetColorText (time_buffer);

			gi.configstring (CS_TDM_TIMEOUT_STIRNG, time_buffer);
		}
	}
}

/*
==============
TDM_Error
==============
Die horribly.
*/
void TDM_Error (const char *fmt, ...)
{
	va_list		argptr;
	char		text[512];
	edict_t		*ent;

	va_start (argptr,fmt);
	Q_vsnprintf (text, sizeof(text), fmt, argptr);
	va_end (argptr);
	text[sizeof(text)-1] = 0;

	gi.dprintf ("An internal OpenTDM error has occured. Please save the following information and post it on the forums at www.opentdm.net.\n");
	gi.dprintf ("ERROR: %s\n", text);
	gi.dprintf ("Match state: %d\n", tdm_match_status);
	gi.dprintf ("Team status: %d - %d - %d\n", teaminfo[TEAM_SPEC].players, teaminfo[TEAM_A].players, teaminfo[TEAM_B].players);
	gi.dprintf ("Level times: frame: %d, start: %d, end: %d, resume: %d, scores: %d\n", level.framenum, level.match_start_framenum, level.match_end_framenum,
		level.match_resume_framenum, level.match_score_end_framenum);
	gi.dprintf ("Vote status: Active: %d, type: %d, end: %d, initiator %d\n", vote.active, vote.flags, vote.end_frame, vote.initiator ? vote.initiator - g_edicts : 0);
	
	gi.dprintf ("Client dump:\n");
	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		gi.dprintf ("%d: %s, connected %d, team %d, info %p\n", ent - g_edicts, ent->client->pers.netname, ent->client->pers.connected, ent->client->resp.team, ent->client->resp.teamplayerinfo);
	}

	gi.error (text);
}

/*
==============
TDM_ProcessText
==============
Generic function for processing a bunch of text a line at a time. Destroys input!
*/
qboolean TDM_ProcessText (char *buff, int len, qboolean (*func)(char *, int, void *), void *param)
{
	char		line[256];
	char		*q;
	char		*ptr;
	int			line_number;

	ptr = buff;
	q = buff;

	line_number = 1;

	while (len)
	{
		switch (buff[0])
		{
			case '\n':
			case '\r':
				buff[0] = 0;
				if (q)
				{
					qboolean	parse;
					char		*p;

					Q_strncpy (line, q, sizeof(line)-1);
					parse = true;

					p = strchr (line, '\n');
					if (p)
						p[0] = 0;

					p = strchr (line, '\r');
					if (p)
						p[0] = 0;

					if (line[0] == '#' || line[0] == '/' || line[0] == '\0')
						parse = false;
					else if (line[0] == '\\')
					{
						/*if (!strncmp (line + 1, "include ", 8))
						{
							char	*path;

							path = line + 9;

							if (!TDM_ProcessFile (path, func))
								gi.dprintf ("WARNING: Unable to read included file '%s' on line %d\n", path, line_number);
						}
						else*/
						{
							gi.dprintf ("WARNING: Unknown directive '%s' on line %d\n", line + 1, line_number);
						}

						parse = false;
					}

					if (parse)
					{
						if (!func (line, line_number, param))
							return false;
					}
					q = NULL;
					line_number++;
				}
				buff++;
				break;
			case '\0':
				buff++;
				break;
			default:
				if (!q)
					q = buff;
				buff++;
				break;
		}
		len--;
	}

	return true;
}

/*
==============
TDM_HandleDownload
==============
A download we requested for something has finished. Do stuff.
*/
void TDM_HandleDownload (char *buff, int len, int code)
{
	//player left before download finished, lame!
	//note on an extremely poor connection it's possible another player since occupied their slot, but
	//for that to happen, the download must take 3+ seconds which should be unrealistic, so i don't
	//deal with it.
	if (!tdm_download.initiator->inuse)
		return;

	//oops, i'm doing something to the site
	if (code == 500)
	{
		gi.cprintf (tdm_download.initiator, PRINT_HIGH, "The OpenTDM web service is currently unavailable. Please try again later.\n");
		return;
	}

	switch (tdm_download.type)
	{
		case DL_CONFIG:
			TDM_ConfigDownloaded (buff, len, code);
			break;

		case DL_POST_STATS:
			break;
		
		default:
			TDM_Error ("TDM_HandleDownload: Unrequested/unknown type");
			break;
	}
}

/*
==============
TDM_GetTeamFromArgs
==============
Generic function to return a team index from gi.arg*, used to lookup
team based on user input. -1 on error.
*/
int TDM_GetTeamFromArg (edict_t *ent, const char *value)
{
	if (!Q_stricmp (value, "1") || !Q_stricmp (value, "A") || !Q_stricmp (value, teaminfo[TEAM_A].name))
	{
		return TEAM_A;
	}
	else if (!Q_stricmp (value, "2") || !Q_stricmp (value, "B") || !Q_stricmp (value, teaminfo[TEAM_B].name))
	{
		return TEAM_B;
	}
	else if (!Q_stricmp (value, "0") || !Q_stricmp (value, "S") || !Q_stricmp (value, "O") ||
		!Q_stricmp (value, "spec") || !Q_stricmp (value, "obs"))
	{
		return TEAM_SPEC;
	}

	return -1;
}
