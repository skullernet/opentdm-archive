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

//TDM client stuff. Commands that are issued by or somehow affect a client are mostly
//here.

#include "g_local.h"
#include "g_tdm.h"

const int	teamJoinEntries[MAX_TEAMS] = {9, 3, 6};

/*
==============
TDM_AddPlayerToMatchinfo
==============
A new player joined mid-match (invite / etc), update teamplayerinfo to reflect this.
*/
void TDM_AddPlayerToMatchinfo (edict_t *ent)
{
	int				i;
	teamplayer_t	*new_teamplayers;

	if (!current_matchinfo.teamplayers)
		TDM_Error ("TDM_AddPlayerToMatchinfo: no teamplayers");

	//allocate a new teamplayers array
	new_teamplayers = gi.TagMalloc (sizeof(teamplayer_t) * (current_matchinfo.num_teamplayers + 1), TAG_GAME);
	memcpy (new_teamplayers, current_matchinfo.teamplayers, sizeof(teamplayer_t) * current_matchinfo.num_teamplayers);

	//setup this client
	TDM_SetupTeamInfoForPlayer (ent, new_teamplayers + current_matchinfo.num_teamplayers);

	//fix up pointers on existing clients and other things that reference teamplayers. NASTY!
	for (i = 0; i < current_matchinfo.num_teamplayers; i++)
	{
		if (current_matchinfo.captains[TEAM_A] == current_matchinfo.teamplayers + i)
			current_matchinfo.captains[TEAM_A] = new_teamplayers + i;
		else if (current_matchinfo.captains[TEAM_B] == current_matchinfo.teamplayers + i)
			current_matchinfo.captains[TEAM_B] = new_teamplayers + i;

		if (level.tdm_timeout_caller == current_matchinfo.teamplayers + i)
			level.tdm_timeout_caller = new_teamplayers + i;

		//might not have a current client (disconnected)
		if (current_matchinfo.teamplayers[i].client)
			current_matchinfo.teamplayers[i].client->client->resp.teamplayerinfo = new_teamplayers + i;
	}

	//remove old memory
	gi.TagFree (current_matchinfo.teamplayers);

	//move everything to new
	current_matchinfo.teamplayers = new_teamplayers;
	current_matchinfo.num_teamplayers++;
}

/*
==============
JoinedTeam
==============
A player just joined a team, so do things.
*/
void JoinedTeam (edict_t *ent, qboolean reconnected)
{
	if (g_gamemode->value != GAMEMODE_1V1)
		gi.bprintf (PRINT_HIGH, "%s %sjoined team '%s'\n", ent->client->pers.netname, reconnected ? "re" : "", teaminfo[ent->client->resp.team].name);
	else
		gi.bprintf (PRINT_HIGH, "%s %sjoined the game.\n", ent->client->pers.netname, reconnected ? "re" : "");

	ent->client->resp.ready = false;

	//joining a team with no captain by default assigns.
	//FIXME: should this still assign even if the team has existing players?
	if (!teaminfo[ent->client->resp.team].captain)
		TDM_SetCaptain (ent->client->resp.team, ent);

	//nasty hack for setting team names for 1v1 mode
	TDM_UpdateTeamNames ();

	//if we were invited mid-game, reallocate and insert into teamplayers
	// wision: do not add if a player reconnected and used his joincode
	if (tdm_match_status != MM_WARMUP && !reconnected)
		TDM_AddPlayerToMatchinfo (ent);

	//wision: set skin for new player
	gi.configstring (CS_PLAYERSKINS + (ent - g_edicts) - 1, va("%s\\%s", ent->client->pers.netname, teaminfo[ent->client->resp.team].skin));

	if (g_gamemode->value != GAMEMODE_1V1)
		gi.configstring (CS_TDM_SPECTATOR_STRINGS + (ent - g_edicts) - 1, va ("%s (%s)", ent->client->pers.netname, teaminfo[ent->client->resp.team].name));
	else
		gi.configstring (CS_TDM_SPECTATOR_STRINGS + (ent - g_edicts) - 1, ent->client->pers.netname);

	TDM_TeamsChanged ();
	respawn (ent);
}

/*
==============
TDM_LeftTeam
==============
A player just left a team, so do things. Remember to call TeamsChanged afterwards!
*/
void TDM_LeftTeam (edict_t *ent)
{
	int	oldteam;

	gi.bprintf (PRINT_HIGH, "%s left team '%s'\n", ent->client->pers.netname, teaminfo[ent->client->resp.team].name);

	oldteam = ent->client->resp.team;

	//wision: remove player from the team!
	ent->client->resp.team = TEAM_SPEC;

	//assign a new captain
	if (teaminfo[oldteam].captain == ent)
		TDM_SetCaptain (oldteam, TDM_FindPlayerForTeam (oldteam));

	//resume play if this guy called time?
	if (tdm_match_status == MM_TIMEOUT && level.tdm_timeout_caller->client == ent)
		TDM_ResumeGame ();
}

qboolean CanJoin (edict_t *ent, unsigned team)
{
	if (tdm_match_status == MM_COUNTDOWN)
	{
		gi.cprintf (ent, PRINT_HIGH, "Teams are locked during countdown.\n");
		return false;
	}

	if (tdm_match_status != MM_WARMUP)
	{
		gi.cprintf (ent, PRINT_HIGH, "Match already in progress.\n");
		return false;
	}

	if (g_gamemode->value == GAMEMODE_1V1 && teaminfo[team].captain)
	{
		gi.cprintf (ent, PRINT_HIGH, "Only one player is allowed on each team in 1v1 mode.\n");
		return false;
	}

	//wision: forbid rejoining the team
	if (ent->client->resp.team == team)
	{
		gi.cprintf (ent, PRINT_HIGH, "You are already on team '%s'.\n", teaminfo[ent->client->resp.team].name);
		return false;
	}

	//wision: forbid joining locked team
	if (teaminfo[team].locked)
	{
		//being invited to a team bypasses a lock
		if (!(ent->client->resp.last_invited_by && ent->client->resp.last_invited_by->inuse &&
			teaminfo[ent->client->resp.last_invited_by->client->resp.team].captain == ent->client->resp.last_invited_by))
		{
			gi.cprintf (ent, PRINT_HIGH, "Team '%s' is locked.\n", teaminfo[team].name);
			return false;
		}
	}

	//r1: cap at max allowed per-team (default 4)
	if (g_max_players_per_team->value && teaminfo[team].players >= g_max_players_per_team->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "Team '%s' is full.\n", teaminfo[team].name);
		return false;
	}

	return true;
}


/*
==============
JoinTeam1
==============
Player joined Team A via menu
*/
void JoinTeam1 (edict_t *ent)
{
	if (!CanJoin (ent, TEAM_A))
		return;

	if (ent->client->resp.team)
		TDM_LeftTeam (ent);

	ent->client->resp.team = TEAM_A;
	JoinedTeam (ent, false);
}
//merge those together?
/*
==============
JoinTeam2
==============
Player joined Team B via menu
*/
void JoinTeam2 (edict_t *ent)
{
	if (!CanJoin (ent, TEAM_B))
		return;

	if (ent->client->resp.team)
		TDM_LeftTeam (ent);

	ent->client->resp.team = TEAM_B;
	JoinedTeam (ent, false);
}

/*
==============
TDM_GlobalClientSound
==============
Gross, ugly, disgustingly nasty hack for in-eyes mode. We run all the sound
processing code ourselves, and we temporarily throw configstrings at in-eyes
mode players to revert the sexed sound to the correct model, play the sound
and then revert to invisible. Since no svc_frame comes in between this, client
never notices except for sound purposes. Hah.
*/
void TDM_GlobalClientSound (edict_t *ent, int channel, int soundindex, float volume, float attenuation, float timeofs)
{
	int			entnum;
	int			sendchan;
    int			flags;
	vec3_t		origin_v;
	qboolean	use_phs;
	qboolean	force_pos;
	edict_t		*client;

	entnum = ent - g_edicts;

	if (channel & CHAN_NO_PHS_ADD)	// no PHS flag
	{
		use_phs = false;
		channel &= 7;
	}
	else
		use_phs = true;

	sendchan = (entnum<<3) | (channel&7);

	force_pos = false;
	flags = 0;

	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		flags |= SND_VOLUME;

	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		flags |= SND_ATTENUATION;

	if (attenuation == ATTN_NONE)
	{
		use_phs = false;
	}
	else
	{
		// the client doesn't know that bmodels have weird origins
		// the origin can also be explicitly set
		if ( (ent->svflags & SVF_NOCLIENT)
			|| (ent->s.solid == SOLID_BSP)) {
			flags |= SND_POS;
			force_pos = true;
			}

		// use the entity origin unless it is a bmodel or explicitly specified
		if (ent->s.solid == SOLID_BSP)
		{
			origin_v[0] = ent->s.origin[0]+0.5f*(ent->mins[0]+ent->maxs[0]);
			origin_v[1] = ent->s.origin[1]+0.5f*(ent->mins[1]+ent->maxs[1]);
			origin_v[2] = ent->s.origin[2]+0.5f*(ent->mins[2]+ent->maxs[2]);
		}
		else
		{
			VectorCopy (ent->s.origin, origin_v);
		}
	}

	// always send the entity number for channel overrides
	flags |= SND_ENT;

	if (timeofs)
		flags |= SND_OFFSET;

	for (client = g_edicts + 1; client < g_edicts + game.maxclients; client++)
	{
		if (!client->inuse)
			continue;

		if (use_phs)
		{
			if (force_pos)
			{
				flags |= SND_POS;
			}
			else
			{
				if (!gi.inPHS (client->s.origin, origin_v))
					continue;

				if (!gi.inPVS (client->s.origin, origin_v))
					flags |= SND_POS;
				else
					flags &= ~SND_POS;
			}
		}

		if (client->client->chase_target && client->client->chase_mode == CHASE_EYES)
		{
			gi.WriteByte (svc_configstring);
			gi.WriteShort (CS_PLAYERSKINS + (client->client->chase_target - g_edicts) -1);
			gi.WriteString (va ("%s\\%s", client->client->chase_target->client->pers.netname, teaminfo[client->client->chase_target->client->resp.team].skin));
			//gi.unicast (client, false);
		}

		gi.WriteByte (svc_sound);
		gi.WriteByte (flags);
		gi.WriteByte (soundindex);

		if (flags & SND_VOLUME)
			gi.WriteByte ((int)(volume*255));

		if (flags & SND_ATTENUATION)
			gi.WriteByte ((int)(attenuation*64));

		if (flags & SND_OFFSET)
			gi.WriteByte ((int)(timeofs*1000));

		if (flags & SND_ENT)
			gi.WriteShort (sendchan);

		if (flags & SND_POS)
			gi.WritePosition (origin_v);

		if (client->client->chase_target && client->client->chase_mode == CHASE_EYES)
		{
			gi.WriteByte (svc_configstring);
			gi.WriteShort (CS_PLAYERSKINS + (client->client->chase_target - g_edicts) -1);
			gi.WriteString (va ("%s\\opentdm/null", client->client->chase_target->client->pers.netname));
			//gi.unicast (client, false);
		}

		gi.unicast (client, false);
	}
}

/*
==============
ToggleChaseCam
==============
Player hit Spectator menu option or used
chase command.
*/
void ToggleChaseCam (edict_t *ent)
{
	if (ent->client->resp.team)
	{
		TDM_LeftTeam (ent);
		TDM_TeamsChanged ();
		if (tdm_match_status == MM_TIMEOUT && teaminfo[TEAM_A].players == 0 && teaminfo[TEAM_B].players == 0)
			TDM_ResumeGame ();
		respawn (ent);
	}

	if (ent->client->chase_target)
		DisableChaseCam (ent);
	else
		GetChaseTarget(ent);

	PMenu_Close (ent);
}

/*
==============
SelectNextHelpPage
==============
Select the next page of help from the help menu. If help
menu is not open, opens on the first page of help.
*/
void SelectNextHelpPage (edict_t *ent)
{
/*	int		i;

	if (!ent->client->menu.active)
	{
		PMenu_Open (ent, helpmenu[0], 0, 0, false);
		return;
	}

	for (i = 0; i < 3; i++)
	{
		if (ent->client->menu.entries = helpmenu[i])
		{
			PMenu_Close (ent);
			PMenu_Open (ent, helpmenu[i+1], 0, 0, false);
			return;
		}
	}

	PMenu_Close (ent);
	*/
}

/*
==============
TDM_ShowTeamMenu
==============
Show/hide the join team menu
*/
void TDM_ShowTeamMenu (edict_t *ent)
{
	//wision: toggle show/hide the menu
	if (ent->client->menu.active)
		PMenu_Close (ent);
	else
		PMenu_Open (ent, joinmenu, teamJoinEntries[ent->client->resp.team], MENUSIZE_JOINMENU, false);
}

/*
==============
TDM_ProcessJoinCode
==============
Returns true if a player was set back on a team as a result of rejoining.
*/
qboolean TDM_ProcessJoinCode (edict_t *ent, unsigned value)
{
	int				i;
	const char		*code;
	teamplayer_t	*t;
	edict_t			*ghost;

	edict_t			new_entity;
	gclient_t		new_client;

	gclient_t		*saved_client;

	if (tdm_match_status < MM_PLAYING || tdm_match_status == MM_SCOREBOARD)
		return false;

	//if no value was passed, look up userinfo
	if (!value)
	{
		code = Info_ValueForKey (ent->client->pers.userinfo, "joincode");
		if (!code[0])
			return false;

		value = strtoul (code, NULL, 10);
	}

	//must be a disconnected client for this to work
	t = TDM_FindTeamplayerForJoinCode (value);
	if (!t || t->client || !t->saved_client || !t->saved_entity)
		return false;

	//could be using a joincode while chasing, need to fix it here before we possibly overwrite entity
	if (ent->client->chase_target)
		DisableChaseCam (ent);

	gi.unlinkentity (ent);

	ent->client->resp.team = t->team;
	JoinedTeam (ent, true);

	//we only preserve the whole client state in 1v1 mode, in TDM we simply respawn the player
	if (TDM_Is1V1())
	{
		//remove ghost model
		for (ghost = g_edicts + game.maxclients + 1; ghost < g_edicts + globals.num_edicts; ghost++)
		{
			if (!ghost->inuse)
				continue;

			if (ghost->enttype != ENT_GHOST)
				continue;

			if (ghost->count == t->saved_entity->s.number)
			{
				G_FreeEdict (ghost);
				break;
			}
		}

		//take copies of the current (new) structures
		new_entity = *ent;
		new_client = *ent->client;

		//preserve new client pointer
		saved_client = ent->client;

		//restore all edict fields and preserve new client pointer
		*ent = *t->saved_entity;
		ent->client = saved_client;

		//restore all client fields
		*ent->client = *t->saved_client;

		//restore stuff that shouldn't be overwritten
		ent->inuse = true;
		ent->s.number = new_entity.s.number;
		ent->client->resp.vote = VOTE_HOLD;
		VectorCopy (new_client.resp.cmd_angles, ent->client->resp.cmd_angles);

		strcpy (ent->client->pers.userinfo, new_client.pers.userinfo);

		//aiee :E :E
		ent->linkcount = new_entity.linkcount;
		ent->area = new_entity.area;
		ent->num_clusters = new_entity.num_clusters;
		memcpy (ent->clusternums, new_entity.clusternums, sizeof(ent->clusternums));
		ent->headnode = new_entity.headnode;
		ent->areanum = new_entity.areanum;
		ent->areanum2 = new_entity.areanum2;

		//set the delta angle
		for (i=0 ; i<3 ; i++)
			ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(ent->client->v_angle[i] - ent->client->resp.cmd_angles[i]);
	}
	else
	{
		ent->client->resp.score = t->saved_client->resp.score;
		ent->client->resp.enterframe = t->saved_client->resp.enterframe;
		ent->client->resp.joinstate = t->saved_client->resp.joinstate;
		ent->client->resp.ready = t->saved_client->resp.ready;

		ent->client->pers.admin = t->saved_client->pers.admin;
	}

	//free the teamplayer saved info
	gi.TagFree (t->saved_entity);
	gi.TagFree (t->saved_client);

	t->saved_entity = NULL;
	t->saved_client = NULL;

	// wision: restore player's name
	G_StuffCmd (ent, "set name \"%s\"\n", t->name);
	//restore teamplayer links
	ent->client->resp.teamplayerinfo = t;
	t->client = ent;

	//no linkentity, we are called as part of PutClientInServer, linking here would telefrag ourselves!
	gi.unlinkentity (ent);

	return true;
}


/*
==============
TDM_Disconnected
==============
A player disconnected, do things.
*/
void TDM_Disconnected (edict_t *ent)
{
	qboolean	removeTimeout;

	removeTimeout = false;

	//have to check this right up here since we nuke the teamplayer just below!
	if (tdm_match_status == MM_TIMEOUT && level.tdm_timeout_caller->client == ent)
		removeTimeout = true;

	//we remove this up here so TDM_LeftTeam doesn't try to resume if we become implicit timeout caller
	TDM_RemoveStatsLink (ent);

	if (ent->client->resp.team)
	{
		if (tdm_match_status >= MM_PLAYING && tdm_match_status != MM_SCOREBOARD)
		{
			//do joincode stuff if a team player disconnects - save all their client info
			ent->client->resp.teamplayerinfo->saved_client = gi.TagMalloc (sizeof(gclient_t), TAG_GAME);
			*ent->client->resp.teamplayerinfo->saved_client = *ent->client;

			ent->client->resp.teamplayerinfo->saved_entity = gi.TagMalloc (sizeof(edict_t), TAG_GAME);
			*ent->client->resp.teamplayerinfo->saved_entity = *ent;

			if (TDM_Is1V1() && g_1v1_timeout->value > 0)
			{
				edict_t		*ghost;

				//may have already been set by a player or previous client disconnect
				if (tdm_match_status != MM_TIMEOUT)
				{
					
					edict_t		*opponent;

					//timeout is called implicitly in 1v1 games or the other player would auto win
					level.timeout_end_framenum = level.realframenum + SECS_TO_FRAMES(g_1v1_timeout->value);
					level.last_tdm_match_status = tdm_match_status;
					tdm_match_status = MM_TIMEOUT;

					level.tdm_timeout_caller = ent->client->resp.teamplayerinfo;
					gi.bprintf (PRINT_CHAT, "%s disconnected and has %s to reconnect.\n", level.tdm_timeout_caller->name, TDM_SecsToString (g_1v1_timeout->value));

					//show the opponent their options
					opponent = TDM_FindPlayerForTeam (TEAM_A);
					if (opponent)
						gi.cprintf (opponent, PRINT_HIGH, "Your opponent has disconnected. You can allow them %s to reconnect, or you can force a forfeit by typing 'win' in the console.\n", TDM_SecsToString (g_1v1_timeout->value));

					opponent = TDM_FindPlayerForTeam (TEAM_B);
					if (opponent)
						gi.cprintf (opponent, PRINT_HIGH, "Your opponent has disconnected. You can allow them %s to reconnect, or you can force a forfeit by typing 'win' in the console.\n", TDM_SecsToString (g_1v1_timeout->value));

				}

				//show a "ghost" player where the player was
				ghost = G_Spawn ();
				VectorCopy (ent->s.origin, ghost->s.origin);
				VectorCopy (ent->s.angles, ghost->s.angles);
				ghost->s.effects = EF_SPHERETRANS;
				ghost->s.modelindex = 255;
				ghost->s.modelindex2 = 255;
				ghost->s.skinnum = ent - g_edicts - 1;
				ghost->s.frame = ent->s.frame;
				ghost->count = ent->s.number;
				ghost->classname = "ghost";
				ghost->target_ent = ent;
				ghost->enttype = ENT_GHOST;
				gi.linkentity (ghost);
			}
		}

		if (removeTimeout)
			TDM_ResumeGame ();

		TDM_LeftTeam (ent);
	}

	TDM_TeamsChanged ();

	if (tdm_match_status == MM_WARMUP && teaminfo[TEAM_SPEC].players == 0 && teaminfo[TEAM_A].players == 0 && teaminfo[TEAM_B].players == 0)
	{
		if (vote.active)
			TDM_RemoveVote ();
	}
	else
		TDM_CheckVote ();

	//zero for connecting clients on server browsers
	ent->client->ps.stats[STAT_FRAGS] = 0;
}

/*
==============
TDM_SetupClient
==============
Setup the client after an initial connection. Called on first spawn
on every map load. Returns true if a join code was used to respawn
a full player, so that PutClientInServer knows about it.
*/
qboolean TDM_SetupClient (edict_t *ent)
{
	ent->client->resp.team = TEAM_SPEC;
	TDM_TeamsChanged ();

	if (!TDM_ProcessJoinCode (ent, 0))
	{
		if (tdm_match_status == MM_TIMEOUT)
		{
			gi.cprintf (ent, PRINT_CHAT, "\nMatch is currently paused and will auto resume in %s.\n", TDM_SecsToString(FRAMES_TO_SECS(level.timeout_end_framenum - level.realframenum)));
		}
		else
		{
			TDM_ShowTeamMenu (ent);
			gi.cprintf (ent, PRINT_CHAT, "\nWelcome to OpenTDM, an open source OSP/Battle replacement. Please report any bugs at www.opentdm.net. Type 'commands' in the console for a brief command guide.\n\n");
		}
	}
	else
	{
		if (tdm_match_status == MM_TIMEOUT)
		{
			if (TDM_Is1V1())
			{
				//only resume if we have 2 players - it's possible both players dropped, and that is a situation
				//we need to be able to handle.
				if (teaminfo[TEAM_A].players == 1 && teaminfo[TEAM_B].players == 1)
					TDM_ResumeGame ();
				return true;
			}
		}
	}

	return false;
}

/*
==============
TDM_FindPlayerForTeam
==============
Returns first matching player of team. Used in situations where
team is without captain so we can't use teaminfo.captain.
*/
edict_t *TDM_FindPlayerForTeam (unsigned team)
{
	edict_t	*ent;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		if (ent->client->resp.team == team)
			return ent;
	}

	return NULL;
}


/*
==============
TDM_PlayerNameChanged
==============
Someone changed their name, update configstrings for spectators and such.
*/
void TDM_PlayerNameChanged (edict_t *ent)
{
	if (ent->client->resp.team)
	{
		if (TDM_Is1V1())
			gi.configstring (CS_TDM_SPECTATOR_STRINGS + (ent - g_edicts) - 1, ent->client->pers.netname);
		else
			gi.configstring (CS_TDM_SPECTATOR_STRINGS + (ent - g_edicts) - 1, va("%s (%s)", ent->client->pers.netname, teaminfo[ent->client->resp.team].name));

		TDM_UpdateTeamNames ();
	}
}

/*
==============
TDM_SetInitialItems
==============
Give a client an initial weapon/item loadout depending on match mode
*/
void TDM_SetInitialItems (edict_t *ent)
{
	gclient_t		*client;
	const gitem_t	*item;
	int				i;

	client = ent->client;


	client->max_bullets		= 200;
	client->max_shells		= 100;
	client->max_rockets		= 50;
	client->max_grenades	= 50;
	client->max_cells		= 200;
	client->max_slugs		= 50;

	switch (tdm_match_status)
	{
		case MM_WARMUP:
			//wision: spawn with rail in insta
			if (g_gamemode->value == GAMEMODE_ITDM)
			{
				item = GETITEM (ITEM_WEAPON_RAILGUN);
				Add_Ammo (ent, GETITEM(item->ammoindex), 1000);
				client->weapon = item;
				client->selected_item = ITEM_INDEX(item);
				client->inventory[client->selected_item] = 1;
			}
			else
			{
				for (i = 1; i < game.num_items; i++)
				{
					item = GETITEM (i);
					//wision: BFG sucks in warmup :X
					if ((item->flags & IT_WEAPON) && i != ITEM_WEAPON_BFG)
					{
						client->inventory[i] = 1;
						if (item->ammoindex)
							Add_Ammo (ent, GETITEM(item->ammoindex), 1000);
					}
				}

				//spawn with RL up
				if (!client->resp.last_weapon || client->inventory[ITEM_INDEX (client->resp.last_weapon)] == 0)
				{
					client->selected_item = ITEM_WEAPON_ROCKETLAUNCHER;
					client->weapon = GETITEM (ITEM_WEAPON_ROCKETLAUNCHER);
				}
				else
				{
					client->weapon = client->resp.last_weapon;
					client->selected_item = ITEM_INDEX (client->resp.last_weapon);
				}
			}
			client->inventory[ITEM_ITEM_ARMOR_BODY] = 100;
			break;

		default:
			//wision: spawn with rail in insta
			if (g_gamemode->value == GAMEMODE_ITDM)
			{
				item = GETITEM (ITEM_WEAPON_RAILGUN);
				Add_Ammo (ent, GETITEM(item->ammoindex), 1000);
			}
			else
				item = GETITEM (ITEM_WEAPON_BLASTER);
			client->weapon = item;
			client->selected_item = ITEM_INDEX(item);
			client->inventory[client->selected_item] = 1;
			break;
	}
}
