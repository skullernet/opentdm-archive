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

#include "g_local.h"
#include "g_tdm.h"

/*
==============
TDM_RateLimited
==============
Return true if the client has used a command that prints global info or similar recently.
*/
qboolean TDM_RateLimited (edict_t *ent, int penalty)
{
	if (level.framenum < ent->client->resp.last_command_frame)
	{
		gi.cprintf (ent, PRINT_HIGH, "Command ignored due to rate limiting, wait a little longer.\n");
		return true;
	}

	ent->client->resp.last_command_frame = level.framenum + penalty;
	return false;
}

/*
==============
TDM_ForceReady_f
==============
Force everyone to be ready, admin command.
*/
static void TDM_ForceReady_f (void)
{
	edict_t	*ent;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		if (!ent->client->resp.team)
			continue;

		ent->client->resp.ready = true;
	}

	TDM_CheckMatchStart ();
}

/*
==============
TDM_Commands_f
==============
Show brief help on all commands
*/
void TDM_Commands_f (edict_t *ent)
{
	gi.cprintf (ent, PRINT_HIGH,
		"General\n"
		"-------\n"
		"menu         Show OpenTDM menu\n"
		"join         Join a team\n"
		"vote         Propose new settings\n"
		"accept       Accept a team invite\n"
		"captain      Show / become / set captain\n"
		"settings     Show match settings\n"
		"ready        Set ready\n"
		"notready     Set not ready\n"
		"time         Call a time out\n"
		"chase        Enter chasecam mode\n"
		"\n"
		"Team Captains\n"
		"-------------\n"
		"teamname     Change team name\n"
		"teamskin     Change team skin\n"
		"invite       Invite a player\n"
		"pickplayer   Pick a player\n"
		"lockteam     Lock team\n"
		"unlockteam   Unlock team\n"
		"teamready    Force team ready\n"
		"teamnotready Force team not ready\n"
		"kickplayer   Remove a player from team\n"
		);
}

/*
==============
TDM_Team_f
==============
Show team status / join a team
*/
void TDM_Team_f (edict_t *ent)
{
	int			team;

	if (gi.argc () < 2)
	{
		if (!ent->client->resp.team)
		{
			gi.cprintf (ent, PRINT_HIGH, "You are not on a team. Use %s 1 or %s 2 to join a team.\n", gi.argv(0), gi.argv(0));
			return;
		}

		gi.cprintf (ent, PRINT_HIGH, "You are on the '%s' team.\n", teaminfo[ent->client->resp.team].name);
		return;
	}

	if (tdm_match_status != MM_WARMUP)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can't change team in the middle of a match.\n");
		return;
	}

	team = TDM_GetTeamFromArg (ent, gi.args());
	if (team == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Unknown team: %s\n", gi.args());
		return;
	}

	if ((int)ent->client->resp.team == team)
	{
		gi.cprintf (ent, PRINT_HIGH, "You're already on that team!\n");
		return;
	}

	if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
		return;

	if (team == 1)
		JoinTeam1 (ent);
	else if (team == 2)
		JoinTeam2 (ent);
	else
		ToggleChaseCam (ent);
}

/*
==============
TDM_Settings_f
==============
Shows current match settings.
*/
void TDM_Settings_f (edict_t *ent)
{
	gi.cprintf (ent, PRINT_HIGH, "%s", TDM_SettingsString ());
}

/*
==============
TDM_Timeout_f
==============
Call a timeout.
*/
void TDM_Timeout_f (edict_t *ent)
{
	if (!ent->client->resp.team)
	{
		gi.cprintf (ent, PRINT_HIGH, "Only players in the match may call a time out.\n");
		return;
	}

	if (tdm_match_status == MM_TIMEOUT)
	{
		if (level.match_resume_framenum)
		{
			if (level.tdm_timeout_caller->client == ent)
			{
				gi.cprintf (ent, PRINT_HIGH, "You may not call another time out.\n");
				return;
			}

			//someone called another timeout during the game restart countdown.
			G_StuffCmd (NULL, "stopsound\n");
			level.match_resume_framenum = 0;
		}
		else
		{
			//someone tried to resume a time out
			if (level.tdm_timeout_caller->client == NULL)
			{
				gi.cprintf (ent, PRINT_HIGH, "Match will automatically resume once %s reconnects.\n", level.tdm_timeout_caller->name);
				return;
			}
			else if (level.tdm_timeout_caller->client != ent && !ent->client->pers.admin)
			{
				gi.cprintf (ent, PRINT_HIGH, "Only %s or an admin can resume play.\n", level.tdm_timeout_caller->name);
				return;
			}

			TDM_ResumeGame ();
			return;
		}
	}

	if (tdm_match_status < MM_PLAYING || tdm_match_status == MM_SCOREBOARD)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can only call a time out during a match.\n");
		return;
	}

	if (g_max_timeout->value == 0)
	{
		gi.cprintf (ent, PRINT_HIGH, "Time out is disabled on this server.\n");
		return;
	}

	level.timeout_end_framenum = level.realframenum + SECS_TO_FRAMES(g_max_timeout->value);
	level.tdm_timeout_caller = ent->client->resp.teamplayerinfo;
	level.last_tdm_match_status = tdm_match_status;
	tdm_match_status = MM_TIMEOUT;

	if (TDM_Is1V1 ())
		gi.bprintf (PRINT_CHAT, "%s called a time out. Match will resume automatically in %s.\n", ent->client->pers.netname, TDM_SecsToString (g_max_timeout->value));
	else
		gi.bprintf (PRINT_CHAT, "%s (%s) called a time out. Match will resume automatically in %s.\n", ent->client->pers.netname, teaminfo[ent->client->resp.team].name, TDM_SecsToString (g_max_timeout->value));

	gi.cprintf (ent, PRINT_HIGH, "Match paused. Use '%s' again to resume play.\n", gi.argv(0));
}

/*
==============
TDM_Win_f
==============
In 1v1 games, causes player who issues the command to win by forfeit. Only used when opponent
disconnects mid-match and this player doesn't want to wait for them to reconnect.
*/
void TDM_Win_f (edict_t *ent)
{
	if (!TDM_Is1V1())
	{
		gi.cprintf (ent, PRINT_HIGH, "This command may only be used in 1v1 mode.\n");
		return;
	}

	if (tdm_match_status != MM_TIMEOUT || level.tdm_timeout_caller->client)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can only cause a forfeit during a time out caused by your opponent disconnecting.\n");
		return;
	}

	level.timeout_end_framenum = 0;
	level.match_resume_framenum = level.realframenum + 1;
}

/*
==============
TDM_Settings_f
==============
A horrible glob of settings text.
*/
char *TDM_SettingsString (void)
{
	static char	settings[1400];
	int			i;

	static const char *gamemode_text[] = {"Team Deathmatch", "Instagib Team Deathmatch", "1 vs 1 duel"};
	static const char *switchmode_text[] = {"normal", "faster", "instant"};
	static const char *telemode_text[] = {"normal", "no freeze"};

	settings[0] = 0;

	strcat (settings, "Game mode: ");
	strcat (settings, TDM_SetColorText(va("%s\n", gamemode_text[(int)g_gamemode->value])));

	strcat (settings, "Timelimit: ");
	strcat (settings, TDM_SetColorText(va("%g minute%s\n", g_match_time->value / 60, g_match_time->value / 60 == 1 ? "" : "s")));

	strcat (settings, "Overtime: ");
	switch ((int)g_tie_mode->value)
	{
		case 0:
			strcat (settings, TDM_SetColorText(va ("disabled")));
			break;
		case 1:
			strcat (settings, TDM_SetColorText(va ("%g minute%s\n", g_overtime->value / 60, g_overtime->value / 60 == 1 ? "" : "s")));
			break;
		case 2:
			strcat (settings, TDM_SetColorText(va ("Sudden death\n")));
			break;
	}

	strcat (settings, "\n");

	strcat (settings, "Removed weapons: ");
	for (i = 0; i < sizeof(weaponvotes) / sizeof(weaponvotes[0]); i++)
	{
		if ((int)g_itemflags->value & weaponvotes[i].value)
		{
			strcat (settings, TDM_SetColorText (va ("%s", weaponvotes[i].names[1])));
			strcat (settings, " ");
		}
	}
	strcat (settings, "\n");
	
	strcat (settings, "Removed powerups: ");
	for (i = 0; i < sizeof(powerupvotes) / sizeof(powerupvotes[0]); i++)
	{
		if ((int)g_powerupflags->value & powerupvotes[i].value)
		{
			strcat (settings, TDM_SetColorText (va ("%s", powerupvotes[i].names[0])));
			strcat (settings, " ");
		}
	}
	strcat (settings, "\n");
	
	strcat (settings, "\n");


	strcat (settings, va("'%s' skin: ", teaminfo[TEAM_A].name));
	strcat (settings, TDM_SetColorText(va("%s\n", teaminfo[TEAM_A].skin)));

	strcat (settings, va("'%s' skin: ", teaminfo[TEAM_B].name));
	strcat (settings, TDM_SetColorText(va("%s\n", teaminfo[TEAM_B].skin)));

	strcat (settings, "\n");

	strcat (settings, "Weapon switch: ");
	strcat (settings, TDM_SetColorText(va("%s\n", switchmode_text[(int)g_fast_weap_switch->value])));

	strcat (settings, "Teleporter mode: ");
	strcat (settings, TDM_SetColorText(va("%s\n", telemode_text[(int)g_teleporter_nofreeze->value])));

	return settings;
}

/*
==============
TDM_SV_Settings_f
==============
Server admin wants to see current settings.
*/
void TDM_SV_Settings_f (void)
{
	char	*settings;
	size_t	len;
	int		i;

	settings = TDM_SettingsString ();

	//no colortext for the console
	len = strlen (settings);
	for (i = 0; i < len; i++)
		settings[i] &= ~0x80;

	gi.dprintf ("%s", settings);
}

/*
==============
TDM_SV_SaveDefaults_f
==============
Server admin changed cvars and wants the new ones to be default.
*/
void TDM_SV_SaveDefaults_f (void)
{
	TDM_SaveDefaultCvars ();
	gi.dprintf ("Default cvars saved.\n");
}

/*
==============
TDM_ServerCommand
==============
Handle a server sv console command.
*/
qboolean TDM_ServerCommand (const char *cmd)
{
	if (!Q_stricmp (cmd, "settings"))
		TDM_SV_Settings_f ();
	else if (!Q_stricmp (cmd, "savedefaults"))
		TDM_SV_SaveDefaults_f ();
	else
		return false;

	return true;
}

/*
==============
TDM_Teamname_f
==============
Set teamname (captain/admin only).
*/
void TDM_Teamname_f (edict_t *ent)
{
	char		*value;
	unsigned	i;

	if (g_locked_names->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "Teamnames are locked.\n");
		return;
	}

	if (gi.argc() < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: teamname <name>\n");
		return;
	}

	if (g_gamemode->value == GAMEMODE_1V1)
	{
		gi.cprintf (ent, PRINT_HIGH, "This command is unavailable in 1v1 mode.\n");
		return;
	}

	if (teaminfo[ent->client->resp.team].captain != ent && !ent->client->pers.admin)
	{
		gi.cprintf (ent, PRINT_HIGH, "Only team captains or admins can change teamname.\n");
		return;
	}

	if (tdm_match_status != MM_WARMUP && !ent->client->pers.admin)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can only change team name during warmup.\n");
		return;
	}

	value = gi.args ();

	//max however many characters
	value[sizeof(teaminfo[TEAM_SPEC].name)-1] = '\0';

	//validate teamname in the most convuluted way possible
	i = 0;
	do
	{
		if (value[i] < 32)
		{
			gi.cprintf (ent, PRINT_HIGH, "Invalid team name.\n");
			return;
		}
		i++;
	} while (value[i]);

	if (ent->client->resp.team == TEAM_A)
	{
		if (!strcmp (teaminfo[TEAM_A].name, value))
			return;

		g_team_a_name = gi.cvar_set ("g_team_a_name", value);
		g_team_a_name->modified = true;
	}
	else if (ent->client->resp.team == TEAM_B)
	{
		if (!strcmp (teaminfo[TEAM_B].name, value))
			return;

		g_team_b_name = gi.cvar_set ("g_team_b_name", value);
		g_team_b_name->modified = true;
	}

	gi.bprintf (PRINT_HIGH, "Team '%s' renamed to '%s'.\n", teaminfo[ent->client->resp.team].name, value);

	TDM_UpdateTeamNames ();

	TDM_UpdateConfigStrings(false);
}

/*
==============
TDM_Lockteam_f
==============
Locks / unlocks team (captain/admin only).
*/
void TDM_Lockteam_f (edict_t *ent, qboolean lock)
{
	if (g_gamemode->value == GAMEMODE_1V1)
	{
		gi.cprintf (ent, PRINT_HIGH, "This command is unavailable in 1v1 mode.\n");
		return;
	}

	if (teaminfo[ent->client->resp.team].captain != ent && !ent->client->pers.admin)
	{
		gi.cprintf (ent, PRINT_HIGH, "Only team captains or admins can lock/unlock team.\n");
		return;
	}

	teaminfo[ent->client->resp.team].locked = lock;
	gi.cprintf (ent, PRINT_HIGH, "Team '%s' is %slocked.\n", teaminfo[ent->client->resp.team].name, (lock ? "" : "un"));
}

/*
==============
TDM_Ban_f
==============
Kick and ban a player from the server. Defaults to one hour.
*/
void TDM_Ban_f (edict_t *ent)
{
	//TODO: Implement banning functions
}

void TDM_Forceteam_f (edict_t *ent)
{
	//TODO: Force team
}

/*
==============
TDM_PickPlayer_f
==============
Pick a player
//TODO: invite instead of direct picking
*/
void TDM_PickPlayer_f (edict_t *ent)
{
	edict_t	*victim;

	//this could be abused by some captain to make server unplayable by constantly picking
	if (!g_tdm_allow_pick->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "Player picking is disabled by the server administrator. Try using invite instead.\n");
		return;
	}

	if (gi.argc() < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: %s <name/id>\n", gi.argv(0));
		return;
	}

	if (g_gamemode->value == GAMEMODE_1V1)
	{
		gi.cprintf (ent, PRINT_HIGH, "This command is unavailable in 1v1 mode.\n");
		return;
	}

	if (teaminfo[ent->client->resp.team].captain != ent && !ent->client->pers.admin)
	{
		gi.cprintf (ent, PRINT_HIGH, "Only team captains or admins can pick players.\n");
		return;
	}

	if (tdm_match_status != MM_WARMUP)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can only pick players during warmup.\n");
		return;
	}

	if (TDM_RateLimited (ent, SECS_TO_FRAMES(1)))
		return;

	if (LookupPlayer (gi.args(), &victim, ent))
	{
		if (ent == victim)
		{
			gi.cprintf (ent, PRINT_HIGH, "You can't pick yourself!\n");
			return;
		}

		if (victim->client->resp.team == ent->client->resp.team)
		{
			gi.cprintf (ent, PRINT_HIGH, "%s is already in your team.\n", victim->client->pers.netname);
			return;
		}

		gi.bprintf (PRINT_CHAT, "%s picked %s for team '%s'.\n", ent->client->pers.netname, victim->client->pers.netname, teaminfo[ent->client->resp.team].name);

		if (victim->client->resp.team)
			TDM_LeftTeam (victim);

		victim->client->resp.team = ent->client->resp.team;
		JoinedTeam (victim);
	}
}

/*
==============
TDM_Invite_f
==============
Invite a player to your team.
*/
void TDM_Invite_f (edict_t *ent)
{
	edict_t	*victim;

	if (gi.argc() < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: invite <name/id>\n");
		return;
	}

	if (teaminfo[ent->client->resp.team].captain != ent && !ent->client->pers.admin)
	{
		gi.cprintf (ent, PRINT_HIGH, "Only team captains or admins can invite players.\n");
		return;
	}

	if (g_gamemode->value == GAMEMODE_1V1)
	{
		gi.cprintf (ent, PRINT_HIGH, "This command is unavailable in 1v1 mode.\n");
		return;
	}

	if (tdm_match_status != MM_WARMUP)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can only invite players during warmup.\n");
		return;
	}

	if (LookupPlayer (gi.args(), &victim, ent))
	{
		if (ent == victim)
		{
			gi.cprintf (ent, PRINT_HIGH, "You can't invite yourself!\n");
			return;
		}

		if (TDM_RateLimited (ent, SECS_TO_FRAMES(2)))
			return;

		if (victim->client->resp.team == ent->client->resp.team)
		{
			gi.cprintf (ent, PRINT_HIGH, "%s is already on your team.\n", victim->client->pers.netname);
			return;
		}

		victim->client->resp.last_invited_by = ent;
		gi.centerprintf (victim, "You are invited to '%s'\nby %s. Type ACCEPT in\nthe console to accept.\n", teaminfo[ent->client->resp.team].name, ent->client->pers.netname);
		gi.cprintf (ent, PRINT_HIGH, "%s was invited to join your team.\n", victim->client->pers.netname);
	}
}

/*
==============
TDM_Accept_f
==============
Accept an invite.
*/
void TDM_Accept_f (edict_t *ent)
{
	if (!ent->client->resp.last_invited_by)
	{
		gi.cprintf (ent, PRINT_HIGH, "No invite to accept!\n");
		return;
	}

	if (tdm_match_status != MM_WARMUP)
	{
		gi.cprintf (ent, PRINT_HIGH, "The match has started, too late buddy!\n");
		ent->client->resp.last_invited_by = NULL;
		return;
	}

	//holy dereference batman
	if (!ent->client->resp.last_invited_by->inuse ||
		teaminfo[ent->client->resp.last_invited_by->client->resp.team].captain != ent->client->resp.last_invited_by)
	{
		gi.cprintf (ent, PRINT_HIGH, "The invite is no longer valid.\n");
		ent->client->resp.last_invited_by = NULL;
		return;
	}

	if (ent->client->resp.team)
		TDM_LeftTeam (ent);

	ent->client->resp.team = ent->client->resp.last_invited_by->client->resp.team;

	JoinedTeam (ent);
}

/*
==============
TDM_KickPlayer_f
==============
Kick a player from a team
*/
void TDM_KickPlayer_f (edict_t *ent)
{
	edict_t	*victim;

	if (gi.argc() < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: %s <name/id>\n", gi.argv(0));
		return;
	}

	if (g_gamemode->value == GAMEMODE_1V1)
	{
		gi.cprintf (ent, PRINT_HIGH, "This command is unavailable in 1v1 mode.\n");
		return;
	}

	if (teaminfo[ent->client->resp.team].captain != ent && !ent->client->pers.admin)
	{
		gi.cprintf (ent, PRINT_HIGH, "Only team captains or admins can kick players.\n");
		return;
	}

	if (LookupPlayer (gi.args(), &victim, ent))
	{
		if (!victim->client->resp.team)
		{
			gi.cprintf (ent, PRINT_HIGH, "%s is not on a team.\n", victim->client->pers.netname);
			return;
		}

		if (victim->client->resp.team != ent->client->resp.team)
		{
			gi.cprintf (ent, PRINT_HIGH, "%s is not on your team.\n", victim->client->pers.netname);
			return;
		}

		if (victim->client->pers.admin)
		{
			gi.cprintf (ent, PRINT_HIGH, "You can't kick an admin!\n");
			return;
		}

		if (victim == ent)
		{
			gi.cprintf (ent, PRINT_HIGH, "You can't kick yourself!\n");
			return;
		}

		//maybe this should broadcast?
		gi.cprintf (victim, PRINT_HIGH, "You were removed from team '%s' by %s.\n", teaminfo[victim->client->resp.team].name, ent->client->pers.netname);
		ToggleChaseCam (victim);
	}
}

/*
==============
TDM_Admin_f
==============
Become an admin
*/
void TDM_Admin_f (edict_t *ent)
{
	if (ent->client->pers.admin)
	{
		gi.cprintf (ent, PRINT_HIGH, "You are already an admin!\n");
		return;
	}

	if (!g_admin_password->string[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "Admin is disabled on this server.\n");
		return;
	}

	if (!strcmp (gi.argv(1), g_admin_password->string))
	{
		gi.bprintf (PRINT_CHAT, "%s became an admin.\n", ent->client->pers.netname);
		ent->client->pers.admin = true;
	}
}

/*
==============
TDM_Captain_f
==============
Print / set captain
*/
void TDM_Captain_f (edict_t *ent)
{
	if (g_gamemode->value == GAMEMODE_1V1)
	{
		gi.cprintf (ent, PRINT_HIGH, "This command is unavailable in 1v1 mode.\n");
		return;
	}

	if (gi.argc() < 2)
	{
		//checking captain status or assigning from NULL captain
		if (ent->client->resp.team == TEAM_SPEC)
		{
			gi.cprintf (ent, PRINT_HIGH, "You must join a team to set or become captain.\n");
			return;
		}

		if (teaminfo[ent->client->resp.team].captain == ent)
		{
			gi.cprintf (ent, PRINT_HIGH, "You are the captain of team '%s'\n", teaminfo[ent->client->resp.team].name);
		}
		else if (teaminfo[ent->client->resp.team].captain)
		{
			gi.cprintf (ent, PRINT_HIGH, "%s is the captain of team '%s'\n",
			(teaminfo[ent->client->resp.team].captain)->client->pers.netname, teaminfo[ent->client->resp.team].name);
		}
		else
		{
			TDM_SetCaptain (ent->client->resp.team, ent);
		}

	}
	else
	{
		//transferring captain to another player
		edict_t	*victim;

		if (teaminfo[ent->client->resp.team].captain != ent)
		{
			gi.cprintf (ent, PRINT_HIGH, "You must be captain to transfer it to another player!\n");
			return;
		}

		if (LookupPlayer (gi.argv(1), &victim, ent))
		{
			if (victim == ent)
			{
				gi.cprintf (ent, PRINT_HIGH, "You can't transfer captain to yourself!\n");
				return;
			}

			if (victim->client->resp.team != ent->client->resp.team)
			{
				gi.cprintf (ent, PRINT_HIGH, "%s is not on your team.\n", victim->client->pers.netname);
				return;
			}

			//so they don't wonder wtf just happened...
			gi.cprintf (victim, PRINT_HIGH, "%s transferred captain status to you.\n", ent->client->pers.netname);
			TDM_SetCaptain (victim->client->resp.team, victim);
		}
	}
}

/*
==============
TDM_Kick_f
==============
Kick a player from the server (admin only)
*/
void TDM_Kick_f (edict_t *ent)
{
	edict_t	*victim;

	if (LookupPlayer (gi.args(), &victim, ent))
	{
		if (victim->client->pers.admin)
		{
			gi.cprintf (ent, PRINT_HIGH, "You cannot kick an admin!\n");
			return;
		}

		gi.AddCommandString (va ("kick %d\n", (int)(victim - g_edicts - 1)));
	}
}

/*
==============
TDM_Teamskin_f
==============
Set teamskin (captain/admin only).
*/
void TDM_Teamskin_f (edict_t *ent)
{
	const char *value;

	char	*model;
	char	*skin;

	unsigned	i;
	size_t		len;

	if (g_locked_skins->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "Teamskins are locked.\n");
		return;
	}

	if (gi.argc() < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: teamskin <model/skin>\n");
		return;
	}

	if (teaminfo[ent->client->resp.team].captain != ent && !ent->client->pers.admin)
	{
		gi.cprintf (ent, PRINT_HIGH, "Only team captains or admins can change teamskin.\n");
		return;
	}

	value = gi.argv(1);

	if (!value[0] || strlen (value) >= sizeof(teaminfo[TEAM_SPEC].skin) - 1)
	{
		gi.cprintf (ent, PRINT_HIGH, "Invalid model/skin name.\n");
		return;
	}

	model = G_CopyString (value);
	skin = strchr (model, '/');
	if (!skin)
	{
		gi.TagFree (model);
		gi.cprintf (ent, PRINT_HIGH, "Skin must be in the format model/skin.\n");
		return;
	}

	skin[0] = 0;
	skin++;

	if (!skin[0])
	{
		gi.TagFree (model);
		gi.cprintf (ent, PRINT_HIGH, "Skin must be in the format model/skin.\n");
		return;
	}

	len = strlen (model);
	for (i = 0; i < len; i++)
	{
		if (!isalnum (model[i]) && model[i] != '_' && model[i] != '-')
		{
			gi.TagFree (model);
			gi.cprintf (ent, PRINT_HIGH, "Invalid model/skin name.\n");
			return;
		}
	}
	
	len = strlen (skin);
	for (i = 0; i < len; i++)
	{
		if (!isalnum (skin[i]) && skin[i] != '_' && skin[i] != '-')
		{
			gi.TagFree (model);
			gi.cprintf (ent, PRINT_HIGH, "Invalid model/skin name.\n");
			return;
		}
	}

	gi.TagFree (model);

	if (tdm_match_status != MM_WARMUP && !ent->client->pers.admin)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can only change team skin during warmup.\n");
		return;
	}

	//TODO: some check model/skin name, force only female/male models

	if (ent->client->resp.team == TEAM_A)
	{
		if (!strcmp (teaminfo[TEAM_A].skin, value))
			return;
		g_team_a_skin = gi.cvar_set ("g_team_a_skin", value);
		g_team_a_skin->modified = true;
	}
	else if (ent->client->resp.team == TEAM_B)
	{
		if (!strcmp (teaminfo[TEAM_B].skin, value))
			return;
		g_team_b_skin = gi.cvar_set ("g_team_b_skin", value);
		g_team_b_skin->modified = true;
	}

	TDM_UpdateConfigStrings (false);
}

/*
==============
TDM_Ready_f
==============
Toggle ready status
*/
void TDM_Ready_f (edict_t *ent)
{
	if (!ent->client->resp.team)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must be on a team to be ready.\n");
		return;
	}

	if (tdm_match_status >= MM_PLAYING)
		return;

	if (ent->client->resp.ready)
		return;

	if (TDM_RateLimited (ent, SECS_TO_FRAMES(1)))
		return;

	ent->client->resp.ready = true;

	gi.bprintf (PRINT_HIGH, "%s is ready!\n", ent->client->pers.netname);

	TDM_CheckMatchStart ();
}

/*
==============
TDM_NotReady_f
==============
Set notready status
wision: some ppl actually use this
*/
void TDM_NotReady_f (edict_t *ent)
{
	if (!ent->client->resp.team)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must be on a team to be NOT ready.\n");
		return;
	}

	if (tdm_match_status >= MM_PLAYING)
		return;

	if (!ent->client->resp.ready)
		return;

	if (TDM_RateLimited (ent, SECS_TO_FRAMES(1)))
		return;

	ent->client->resp.ready = false;

	gi.bprintf (PRINT_HIGH, "%s is not ready!\n", ent->client->pers.netname);

	TDM_CheckMatchStart ();
}

/*
==============
TDM_Changeteamstatus_f
==============
Set ready/notready status for whole team
*/
void TDM_Changeteamstatus_f (edict_t *ent, qboolean ready)
{
	edict_t *ent2;

	if (ent->client->resp.team == TEAM_SPEC)
		return;

	if (teaminfo[ent->client->resp.team].captain != ent && !ent->client->pers.admin)
	{
		gi.cprintf (ent, PRINT_HIGH, "Only team captains or admins can use teamready/teamnotready.\n");
		return;
	}

	for (ent2 = g_edicts + 1; ent2 <= g_edicts + game.maxclients; ent2++)
	{
		if (!ent2->inuse)
			continue;

		if (ent->client->resp.team != ent2->client->resp.team)
			continue;

		if (ready && ent2->client->resp.ready)
			continue;

		if (!ready && !ent2->client->resp.ready)
			continue;

		if (ent2 != ent)
			gi.cprintf (ent2, PRINT_HIGH, "You were forced %sready by team captain %s!\n", ready ? "" : "not ", ent->client->pers.netname);

		TDM_Ready_f (ent2);
	}
}

/*
==============
TDM_OldScores_f
==============
Show the scoreboard from the last match. I hope I got that right :).
*/
void TDM_OldScores_f (edict_t *ent)
{
	if (!old_matchinfo.scoreboard_string[0])
	{
		gi.cprintf (ent, PRINT_HIGH, "No old scores to show yet.\n");
		return;
	}

	if (ent->client->showoldscores)
	{
		ent->client->showoldscores = false;
		return;
	}

	PMenu_Close (ent);

	ent->client->showscores = false;
	ent->client->showoldscores = true;

	gi.WriteByte (svc_layout);
	gi.WriteString (old_matchinfo.scoreboard_string);
	gi.unicast (ent, true);
}

/*
==============
TDM_Ghost_f
==============
Manual recovery of saved client info via join code
*/
void TDM_Ghost_f (edict_t *ent)
{
	unsigned code;

	if (gi.argc() < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: %s <code>\n", gi.argv(0));
		return;
	}

	if (tdm_match_status < MM_PLAYING || tdm_match_status == MM_SCOREBOARD)
	{
		gi.cprintf (ent, PRINT_HIGH, "No match in progress.\n");
		return;
	}

	if (ent->client->resp.team)
	{
		gi.cprintf (ent, PRINT_HIGH, "%s can only be used from spectator mode.\n", gi.argv(0));
		return;
	}

	code = strtoul (gi.args(), NULL, 0);

	//to prevent brute forcing :)
	if (TDM_RateLimited (ent, 2))
		return;

	if (!TDM_ProcessJoinCode (ent, code))
	{
		gi.cprintf (ent, PRINT_HIGH, "No client information found for code %u.\n", code);
		return;
	}
}

/*
==============
TDM_Command
==============
Process TDM commands (from ClientCommand)
*/
qboolean TDM_Command (const char *cmd, edict_t *ent)
{
	if (ent->client->pers.admin)
	{
		if (!Q_stricmp (cmd, "!forceready"))
		{
			TDM_ForceReady_f ();
			return true;
		}
		else if (!Q_stricmp (cmd, "!kickplayer"))
		{
			TDM_KickPlayer_f (ent);
			return true;
		}
		else if (!Q_stricmp (cmd, "!kick"))
		{
			TDM_Kick_f (ent);
			return true;
		}
		else if (!Q_stricmp (cmd, "!ban"))
		{
			TDM_Ban_f (ent);
			return true;
		}
		else if (!Q_stricmp (cmd, "!forceteam"))
		{
			TDM_Forceteam_f (ent);
			return true;
		}
	}

	//only a few commands work in time out mode or intermission
	if (tdm_match_status == MM_TIMEOUT || tdm_match_status == MM_SCOREBOARD)
	{
		if (!Q_stricmp (cmd, "commands"))
			TDM_Commands_f (ent);
		else if (!Q_stricmp (cmd, "settings") || !Q_stricmp (cmd, "matchinfo"))
			TDM_Settings_f (ent);
		else if (!Q_stricmp (cmd, "calltime") | !Q_stricmp (cmd, "pause") || !Q_stricmp (cmd, "ctime") || !Q_stricmp (cmd, "time"))
			TDM_Timeout_f (ent);
		else if (!Q_stricmp (cmd, "stats") || !Q_stricmp (cmd, "kills"))
			TDM_Stats_f (ent, &current_matchinfo);
		else if (!Q_stricmp (cmd, "accuracy"))
			TDM_Accuracy_f (ent, &current_matchinfo);
		else if (!Q_stricmp (cmd, "damage"))
			TDM_Damage_f (ent, &current_matchinfo);
		else if (!Q_stricmp (cmd, "items"))
			TDM_Items_f (ent, &current_matchinfo);
		else if (!Q_stricmp (cmd, "ghost") || !Q_stricmp (cmd, "restore") || !Q_stricmp (cmd, "recover") | !Q_stricmp (cmd, "rejoin"))
			TDM_Ghost_f (ent);
		else if (!Q_stricmp (cmd, "win"))
			TDM_Win_f (ent);
		else if (!Q_stricmp (cmd, "stopsound"))
			return true;	//prevent chat from our stuffcmds on people who have no sound

		//return true to cover all other commands (this prevents using console for chat but meh :/)
		return true;
	}
	else
	{
		if (!Q_stricmp (cmd, "ready"))
			TDM_Ready_f (ent);
		else if (!Q_stricmp (cmd, "notready") || !Q_stricmp (cmd, "unready") || !Q_stricmp (cmd, "noready"))
			TDM_NotReady_f (ent);
		else if (!Q_stricmp (cmd, "kickplayer") || !Q_stricmp (cmd, "removeplayer") || !Q_stricmp (cmd, "remove"))
			TDM_KickPlayer_f (ent);
		else if (!Q_stricmp (cmd, "admin"))
			TDM_Admin_f (ent);
		else if (!Q_stricmp (cmd, "captain"))
			TDM_Captain_f (ent);
		else if (!Q_stricmp (cmd, "vote"))
			TDM_Vote_f (ent);
		else if (!Q_stricmp (cmd, "yes") || !Q_stricmp (cmd, "no"))
			TDM_Vote_f (ent);
		else if (!Q_stricmp (cmd, "lockteam") || !Q_stricmp (cmd, "lock"))
			TDM_Lockteam_f (ent, true);
		else if (!Q_stricmp (cmd, "unlockteam") || !Q_stricmp (cmd, "unlock"))
			TDM_Lockteam_f (ent, false);
		else if (!Q_stricmp (cmd, "pickplayer") || !Q_stricmp (cmd, "pick"))
			TDM_PickPlayer_f (ent);
		else if (!Q_stricmp (cmd, "invite"))
			TDM_Invite_f (ent);
		else if (!Q_stricmp (cmd, "accept"))
			TDM_Accept_f (ent);
		else if (!Q_stricmp (cmd, "teamskin"))
			TDM_Teamskin_f (ent);
		else if (!Q_stricmp (cmd, "teamname"))
			TDM_Teamname_f (ent);
		else if (!Q_stricmp (cmd, "teamready") || !Q_stricmp (cmd, "readyteam"))
			TDM_Changeteamstatus_f (ent, true);
		else if (!Q_stricmp (cmd, "teamnotready") || !Q_stricmp (cmd, "notreadyteam"))
			TDM_Changeteamstatus_f (ent, false);
		else if (!Q_stricmp (cmd, "menu") || !Q_stricmp (cmd, "ctfmenu") || !Q_stricmp (cmd, "inven"))
			TDM_ShowTeamMenu (ent);
		else if (!Q_stricmp (cmd, "commands"))
			TDM_Commands_f (ent);
		else if (!Q_stricmp (cmd, "join") || !Q_stricmp (cmd, "team"))
			TDM_Team_f (ent);
		else if (!Q_stricmp (cmd, "settings") || !Q_stricmp (cmd, "matchinfo"))
			TDM_Settings_f (ent);
		else if (!Q_stricmp (cmd, "observer") || !Q_stricmp (cmd, "spectate") || !Q_stricmp (cmd, "chase"))
			ToggleChaseCam (ent);
		else if (!Q_stricmp (cmd, "calltime") | !Q_stricmp (cmd, "pause") || !Q_stricmp (cmd, "ctime") || !Q_stricmp (cmd, "time"))
			TDM_Timeout_f (ent);
		else if (!Q_stricmp (cmd, "stats") || !Q_stricmp (cmd, "kills"))
			TDM_Stats_f (ent, &current_matchinfo);
		else if (!Q_stricmp (cmd, "accuracy"))
			TDM_Accuracy_f (ent, &current_matchinfo);
		else if (!Q_stricmp (cmd, "teamaccuracy"))
			TDM_TeamAccuracy_f (ent, &current_matchinfo);
		else if (!Q_stricmp (cmd, "damage"))
			TDM_Damage_f (ent, &current_matchinfo);
		else if (!Q_stricmp (cmd, "teamdamage"))
			TDM_TeamDamage_f (ent, &current_matchinfo);
		else if (!Q_stricmp (cmd, "items"))
			TDM_Items_f (ent, &current_matchinfo);
		else if (!Q_stricmp (cmd, "teamitems"))
			TDM_TeamItems_f (ent, &current_matchinfo);
		else if (!Q_stricmp (cmd, "oldstats") || !Q_stricmp (cmd, "oldkills") || (!Q_stricmp (cmd, "laststats") || !Q_stricmp (cmd, "lastkills") ))
			TDM_Stats_f (ent, &old_matchinfo);
		else if (!Q_stricmp (cmd, "oldaccuracy") || !Q_stricmp (cmd, "lastaccuracy"))
			TDM_Accuracy_f (ent, &old_matchinfo);
		else if (!Q_stricmp (cmd, "oldteamaccuracy") || !Q_stricmp (cmd, "lastteamaccuracy"))
			TDM_TeamAccuracy_f (ent, &old_matchinfo);
		else if (!Q_stricmp (cmd, "olddamage") || !Q_stricmp (cmd, "lastdamage"))
			TDM_Damage_f (ent, &old_matchinfo);
		else if (!Q_stricmp (cmd, "oldteamdamage"))
			TDM_TeamDamage_f (ent, &old_matchinfo);
		else if (!Q_stricmp (cmd, "olditems") || !Q_stricmp (cmd, "lasitems"))
			TDM_Items_f (ent, &old_matchinfo);
		else if (!Q_stricmp (cmd, "oldteamitems"))
			TDM_TeamItems_f (ent, &old_matchinfo);
		else if (!Q_stricmp (cmd, "oldscores") || !Q_stricmp (cmd, "oldscore") || !Q_stricmp (cmd, "lastscores") || !Q_stricmp (cmd, "lastscore"))
			TDM_OldScores_f (ent);
		else if (!Q_stricmp (cmd, "ghost") || !Q_stricmp (cmd, "restore") || !Q_stricmp (cmd, "recover") | !Q_stricmp (cmd, "rejoin"))
			TDM_Ghost_f (ent);
		else if (!Q_stricmp (cmd, "stopsound"))
			return true;	//prevent chat from our stuffcmds on people who have no sound
		else
			return false;
	}

	return true;
}
