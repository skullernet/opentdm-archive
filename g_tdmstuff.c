#include "g_local.h"

teaminfo_t	teaminfo[MAX_TEAMS];
matchmode_t	tdm_match_status;

#define VOTE_TIMELIMIT	0x1
#define VOTE_MAP		0x2
#define	VOTE_KICK		0x4
#define VOTE_WEAPONS	0x8
#define VOTE_POWERUPS	0x10

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

typedef struct weaponvote_s
{
	const char		*names[2];
	unsigned		value;
	int				itemindex;
} weaponinfo_t;

const weaponinfo_t	weaponvotes[] = 
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

typedef struct vote_s
{
	unsigned		flags;
	unsigned		newtimelimit;
	unsigned		newweaponflags;
	unsigned		newpowerupflags;
	char			newmap[MAX_QPATH];
	edict_t			*victim;
	edict_t			*initiator;
	unsigned		end_frame;
	qboolean		active;
} vote_t;

vote_t	vote;

void droptofloor (edict_t *ent);

void JoinTeam1 (edict_t *ent);
void JoinTeam2 (edict_t *ent);
void ToggleChaseCam (edict_t *ent);
void SelectNextHelpPage (edict_t *ent);

static char teamStatus[MAX_TEAMS][32];
static char teamJoinText[MAX_TEAMS][32];
const int	teamJoinEntries[MAX_TEAMS] = {9, 3, 6};

pmenu_t joinmenu[] =
{
	{ "*Quake II - OpenTDM",PMENU_ALIGN_CENTER, NULL, NULL },
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
	{ "v" OPENTDM_VERSION,	PMENU_ALIGN_RIGHT, NULL, NULL },
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
TDM_ResetLevel
==============
Resets the items and triggers / funcs / etc in the level
in preparation for a match.
*/
void TDM_ResetLevel (void)
{
	edict_t	*ent;

	//free up any stray ents
	for (ent = g_edicts + 1 + game.maxclients; ent < g_edicts + globals.num_edicts; ent++)
	{
		if (!ent->inuse)
			continue;

		if (
			(ent->enttype == ENT_DOOR_TRIGGER || ent->enttype == ENT_PLAT_TRIGGER)
			||
			(ent->owner >= (g_edicts + 1) && ent->owner <= (g_edicts + game.maxclients))
			)
			G_FreeEdict (ent);
	}

	///rerun the entity level string
	ParseEntityString (true);

	//immediately droptofloor
	for (ent = g_edicts + 1 + game.maxclients; ent < g_edicts + globals.num_edicts; ent++)
	{
		if (!ent->inuse)
			continue;

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

	level.match_start_framenum = 0;
	level.match_end_framenum = level.framenum + (int)(g_match_time->value / FRAMETIME);
	tdm_match_status = MM_PLAYING;
	TDM_ResetLevel ();

	teaminfo[TEAM_A].score = teaminfo[TEAM_B].score = 0;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		if (ent->client->resp.team)
			respawn (ent);
	}

	TDM_UpdateConfigStrings (false);
}

/*
==============
TDM_ScoreBoardMessage
==============
Display TDM scoreboard. Must be unicast or multicast after calling this
function.
*/
void TDM_ScoreBoardMessage (edict_t *ent)
{
	//TODO: implement scoreboard
}

/*
==============
TDM_BeginCountdown
==============
All players are ready so start the countdown
*/
void TDM_BeginCountdown (void)
{
	gi.bprintf (PRINT_CHAT, "All players ready! Starting countdown (%g secs)...\n", g_match_countdown->value);

	tdm_match_status = MM_COUNTDOWN;
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

	level.intermissionframe = level.framenum;

	// find an intermission spot
	ent = G_Find (NULL, FOFS(classname), "info_player_intermission");
	if (!ent)
	{	// the map creator forgot to put in an intermission point...
		ent = G_Find (NULL, FOFS(classname), "info_player_start");
		if (!ent)
			ent = G_Find (NULL, FOFS(classname), "info_player_deathmatch");
	}
	else
	{	// chose one of four spots
		i = rand() & 3;
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
		MoveClientToIntermission (client);
	}
}


/*
==============
TDM_EndMatch
==============
A match has ended through some means.
*/
void TDM_EndMatch (void)
{
	int		winner, loser;

	winner = 0;
	loser = 0;

	if (teaminfo[TEAM_A].score > teaminfo[TEAM_B].score)
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
		gi.bprintf (PRINT_HIGH, "Tie game, %d to %d.\n", teaminfo[TEAM_A].score, teaminfo[TEAM_B].score);
	}

	if (winner)
	{
		gi.bprintf (PRINT_HIGH, "%s wins, %d to %d.\n", teaminfo[winner].name, teaminfo[winner].score, teaminfo[loser].score);
	}

	level.match_score_end_framenum = level.framenum + (10.0f / FRAMETIME);
	TDM_BeginIntermission ();
}

/*
==============
TDM_CheckTimes
==============
Check miscellaneous timers, eg match start countdown
*/
void TDM_CheckTimes (void)
{
	if (level.match_start_framenum)
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

	if (level.match_end_framenum)
	{
		int		remaining;

		remaining = level.match_end_framenum - level.framenum;

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
			TDM_EndMatch ();
		}
	}

	if (level.match_score_end_framenum)
	{
		int	remaining;

		remaining = level.match_score_end_framenum - level.framenum;

		if (remaining == 0)
			TDM_EndIntermission ();
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

	ready[TEAM_A] = ready[TEAM_B] = 0;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		if (ent->client->resp.team == TEAM_SPEC)
			continue;

		if (ent->client->resp.ready)
			ready[ent->client->resp.team]++;
	}

	if (teaminfo[TEAM_A].players && ready[TEAM_A] == teaminfo[TEAM_A].players &&
		teaminfo[TEAM_B].players && ready[TEAM_B] == teaminfo[TEAM_B].players)
	{
		TDM_BeginCountdown ();
	}
	else
	{
		if (level.match_start_framenum)
		{
			gi.bprintf (PRINT_CHAT, "Countdown aborted!\n");
			level.match_start_framenum = 0;
			tdm_match_status = MM_WARMUP;
		}
	}
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

	ent->client->resp.ready = !ent->client->resp.ready;

	gi.bprintf (PRINT_HIGH, "%s is %sready!\n", ent->client->pers.netname, ent->client->resp.ready ? "" : "NOT ");

	TDM_CheckMatchStart ();
}

/*
==============
LookupPlayer
==============
Look up a player by partial subnamem, full name or client id. If multiple
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
			gi.cprintf (ent, PRINT_HIGH, "'%s' matches multiple players.\n", match);
			return 0;
		}
	}
	else
	{
		p = g_edicts + 1 + numericMatch;

		if (!p->inuse)
		{
			gi.cprintf (ent, PRINT_HIGH, "Client %d is not active.\n", numericMatch);
			return 0;
		}

		*out = p;
		return 1;
	}

	gi.cprintf (ent, PRINT_HIGH, "No player match found for '%s'\n", match);
	return 0;
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
		gi.cprintf (ent, PRINT_HIGH, "Usage: kickplayer <name/id>");
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

		gi.cprintf (victim, PRINT_HIGH, "You were removed from the %s team by %s.\n", teaminfo[victim->client->resp.team].name, ent->client->pers.netname);
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

static void TDM_AnnounceVote (void)
{
	char	message[1400];
	char	what[1400];

	message[0] = 0;
	what[0] = 0;

	strcpy (message, vote.initiator->client->pers.netname);
	strcat (message, " started a vote: ");

	if (vote.flags & VOTE_TIMELIMIT)
		strcat (what, va("timelimit %d", vote.newtimelimit));

	if (vote.flags & VOTE_MAP)
		strcat (what, va ("%smap %s", what[0] ? ", " : "", vote.newmap));

	gi.bprintf (PRINT_CHAT, "%s%s\n", message, what);
}

static void TDM_Vote_f (edict_t *ent)
{
	const char *cmd;
	const char *value;

	if (gi.argc() < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote <timelimit/map/kick/powerups/weapons> <value>\n"
			"For weapon/powerup votes, specify multiple combinations of + or -, eg -bfg, +quad -invuln, etc\n");
		return;
	}

	if (tdm_match_status != MM_WARMUP)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can only propose new settings during warmup.\n");
		return;
	}

	cmd = gi.argv(1);

	if (!Q_stricmp (cmd, "timelimit"))
	{
		int		limit;

		value = gi.argv(2);
		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote timelimit <mins>\n");
			return;
		}

		limit = atoi (value);
		if (!limit)
		{
			gi.cprintf (ent, PRINT_HIGH, "Invalid timelimit value.\n");
			return;
		}

		vote.flags |= VOTE_TIMELIMIT;
		vote.newtimelimit = limit;
		vote.initiator = ent;
		vote.end_frame = level.framenum + g_vote_time->value * (FRAMETIME / 10);
		vote.active = true;
		TDM_AnnounceVote ();
	}
	else if (!Q_stricmp (cmd, "map"))
	{
		value = gi.argv(2);

		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote map <mapname>\n");
			return;
		}

		if (strlen (value) >= MAX_QPATH - 1)
		{
			gi.cprintf (ent, PRINT_HIGH, "Invalid map name.\n");
			return;
		}

		strcpy (vote.newmap, value);

		vote.flags |= VOTE_MAP;
		vote.end_frame = level.framenum + g_vote_time->value * (FRAMETIME / 10);
		vote.initiator = ent;
		vote.active = true;
		TDM_AnnounceVote ();
	}
	else if (!Q_stricmp (cmd, "weapons"))
	{
		char		modifier;
		unsigned	flags;
		int			i, j;
		qboolean	found;

		//example weapons string, -ssg -bfg +rl
		//special string "all" sets all, eg -all +rg

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
					if (modifier == '+')
						flags |= weaponvotes[j].value;
					else if (modifier == '-')
						flags &= ~weaponvotes[j].value;

					found = true;
					break;
				}
			}

			if (!found)
			{
				if (!Q_stricmp (value, "all"))
				{
					if (modifier == '+')
						flags = 0xFFFFFFFFU;
					else
						flags = 0;
				}
				else
				{
					gi.cprintf (ent, PRINT_HIGH, "Unknown weapon '%s'\n", value);
					return;
				}
			}
		}
	}
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Unknown vote action '%s'\n", cmd);
	}
}

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

	if (!Q_stricmp (cmd, "ready"))
		TDM_Ready_f (ent);
	else if (!Q_stricmp (cmd, "kickplayer"))
		TDM_KickPlayer_f (ent);
	else if (!Q_stricmp (cmd, "admin"))
		TDM_Admin_f (ent);
	else if (!Q_stricmp (cmd, "vote"))
		TDM_Vote_f (ent);
	else if (!Q_stricmp (cmd, "menu") || !Q_stricmp (cmd, "ctfmenu"))
		TDM_ShowTeamMenu (ent);
	else
		return false;

	return true;
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
			for (i = 1; i < game.num_items; i++)
			{
				item = GETITEM (i);
				if (item->flags & IT_WEAPON)
				{
					client->inventory[i] = 1;
					if (item->ammoindex)
						Add_Ammo (ent, GETITEM(item->ammoindex), 1000);
					client->selected_item = i;
					client->weapon = item;
				}
			}
			client->inventory[ITEM_ITEM_ARMOR_BODY] = 200;
			break;

		default:
			item = GETITEM (ITEM_WEAPON_BLASTER);
			client->weapon = item;
			client->selected_item = ITEM_INDEX(item);
			client->inventory[client->selected_item] = 1;
			break;
	}
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
	if (ent)
	{
		gi.bprintf (PRINT_HIGH, "%s became captain of '%s'\n", ent->client->pers.netname, teaminfo[team].name);
		gi.cprintf (ent, PRINT_CHAT, "You are the captain of '%s'\n", teaminfo[team].name);
	}
}

/*
==============
JoinedTeam
==============
A player just joined a team, so do things.
*/
void JoinedTeam (edict_t *ent)
{
	gi.bprintf (PRINT_HIGH, "%s joined team '%s'\n", ent->client->pers.netname, teaminfo[ent->client->resp.team].name);

	ent->client->resp.ready = false;

	if (!teaminfo[ent->client->resp.team].captain)
		TDM_SetCaptain (ent->client->resp.team, ent);

	TDM_TeamsChanged ();
	respawn (ent);
}

/*
==============
TDM_LeftTeam
==============
A player just left a team, so do things.
*/
void TDM_LeftTeam (edict_t *ent)
{
	gi.bprintf (PRINT_HIGH, "%s left team '%s'\n", ent->client->pers.netname, teaminfo[ent->client->resp.team].name);

	if (teaminfo[ent->client->resp.team].captain == ent)
		TDM_SetCaptain (ent->client->resp.team, NULL);

	TDM_TeamsChanged ();
}

qboolean CanJoin (edict_t *ent)
{
	if (level.match_start_framenum)
	{
		gi.cprintf (ent, PRINT_HIGH, "Teams are locked during countdown.\n");
		return false;
	}

	if (tdm_match_status != MM_WARMUP)
	{
		gi.cprintf (ent, PRINT_HIGH, "Match already in progress.\n");
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
	if (!CanJoin (ent))
		return;

	if (ent->client->resp.team)
		TDM_LeftTeam (ent);

	ent->client->resp.team = TEAM_A;
	JoinedTeam (ent);
}

/*
==============
JoinTeam2
==============
Player joined Team B via menu
*/
void JoinTeam2 (edict_t *ent)
{
	if (!CanJoin (ent))
		return;

	if (ent->client->resp.team)
		TDM_LeftTeam (ent);

	ent->client->resp.team = TEAM_B;
	JoinedTeam (ent);
}

/*
==============
ToggleChaseCam
==============
Player hit Spectator menu option
*/
void ToggleChaseCam (edict_t *ent)
{
	if (ent->client->resp.team)
	{
		TDM_LeftTeam (ent);
		ent->client->resp.team = TEAM_SPEC;
		respawn (ent);
		TDM_TeamsChanged ();
	}

	if (ent->client->chase_target)
	{
		ent->client->chase_target = NULL;
		ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
	}
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
CountPlayers
==============
Count how many players each team has
*/
void CountPlayers (void)
{
	edict_t	*ent;
	int		i;

	for (i = 0; i < MAX_TEAMS; i++)
		teaminfo[i].players = 0;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (ent->inuse)
			teaminfo[ent->client->resp.team].players++;
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

	for (i = 0; i < MAX_TEAMS; i++)
	{
		sprintf (teamStatus[i], "  (%d player%s)", teaminfo[i].players, teaminfo[i].players == 1 ? "" : "s");
		sprintf (teamJoinText[i], "*Join %s", teaminfo[i].name);
	}

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
	UpdateTeamMenu ();
	TDM_CheckMatchStart ();
}

/*
==============
TDM_ShowTeamMenu
==============
Show the join team menu
*/
void TDM_ShowTeamMenu (edict_t *ent)
{
	PMenu_Open (ent, joinmenu, teamJoinEntries[ent->client->resp.team], MENUSIZE_JOINMENU, false);
}

/*
==============
TDM_SetupClient
==============
Setup the client after an initial connection. Called on first spawn
on every map load.
*/
void TDM_SetupClient (edict_t *ent)
{
	ent->client->resp.team = TEAM_SPEC;
	TDM_TeamsChanged ();
	TDM_ShowTeamMenu (ent);
}

/*
==============
TDM_MapChanged
==============
Called when a new level is about to begin.
*/
void TDM_MapChanged (void)
{
	TDM_UpdateConfigStrings (true);
}

/*
==============
TDM_ResetGameState
==============
Reset the game state after a match has completed.
*/
void TDM_ResetGameState (void)
{
	edict_t		*ent;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (ent->inuse && ent->client->resp.team != TEAM_SPEC)
		{
			ent->client->resp.team = TEAM_SPEC;
			PutClientInServer (ent);
		}
	}
	TDM_ResetLevel ();
	tdm_match_status = MM_WARMUP;
	level.match_start_framenum = 0;
	teaminfo[TEAM_A].score = teaminfo[TEAM_B].score = 0;
	UpdateTeamMenu ();
}

/*
==============
TDM_Init
==============
Single time initialization stuff.
*/
void TDM_Init (void)
{
	strcpy (teaminfo[0].name, "Observers");

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
	int			i;

	for (i = TEAM_A; i <= TEAM_B; i++)
	{
		oldskin = teaminfo[i].skin;
		if (i == TEAM_A)
			newskin = g_team_a_skin->string;
		else
			newskin = g_team_b_skin->string;

		if (!strcmp (oldskin, newskin))
			continue;

		strncpy (teaminfo[i].skin, newskin, sizeof(teaminfo[i].skin)-1);

		for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
		{
			if (!ent->inuse)
				continue;

			if (ent->client->resp.team == i)
				gi.configstring (CS_PLAYERSKINS + (ent - g_edicts) - 1, va("%s\\%s", ent->client->pers.netname, teaminfo[i].skin));
		}
	}
}

/*
==============
TDM_UpdateConfigStrings
==============
Check any cvar and other changes and update relevant configstrings
*/
void TDM_UpdateConfigStrings (qboolean forceUpdate)
{
	static int			last_time_remaining = -1;
	static int			last_scores[MAX_TEAMS] = {-1, -1};
	static matchmode_t	last_mode = MM_INVALID;
	int					time_remaining;

	if (g_team_a_name->modified || forceUpdate)
	{
		g_team_a_name->modified = false;
		strncpy (teaminfo[TEAM_A].name, g_team_a_name->string, sizeof(teaminfo[TEAM_A].name)-1);
		sprintf (teaminfo[TEAM_A].statname, "%31s", teaminfo[TEAM_A].name);
		gi.configstring (CS_GENERAL + 0, teaminfo[TEAM_A].statname);
	}

	if (g_team_b_name->modified || forceUpdate)
	{
		g_team_b_name->modified = false;
		strncpy (teaminfo[TEAM_B].name, g_team_b_name->string, sizeof(teaminfo[TEAM_B].name)-1);
		sprintf (teaminfo[TEAM_B].statname, "%31s", teaminfo[TEAM_B].name);
		gi.configstring (CS_GENERAL + 1, teaminfo[TEAM_B].statname);
	}

	if (g_team_a_skin->modified || g_team_b_skin->modified || forceUpdate)
	{
		g_team_a_skin->modified = g_team_b_skin->modified = false;
		TDM_SetSkins ();
	}

	if (tdm_match_status != last_mode || forceUpdate)
	{
		last_mode = tdm_match_status;

		switch (tdm_match_status)
		{
			case MM_COUNTDOWN:
			case MM_WARMUP:
				sprintf (teaminfo[TEAM_A].statstatus, "%15s", "WARMUP");
				sprintf (teaminfo[TEAM_B].statstatus, "%15s", "WARMUP");
				break;
			default:
				sprintf (teaminfo[TEAM_A].statstatus, "%15d", teaminfo[TEAM_A].score);
				sprintf (teaminfo[TEAM_B].statstatus, "%15d", teaminfo[TEAM_B].score);
				last_scores[TEAM_A] = teaminfo[TEAM_A].score;
				last_scores[TEAM_B] = teaminfo[TEAM_B].score;
				break;
		}
		gi.configstring (CS_GENERAL + 2, teaminfo[TEAM_A].statstatus);
		gi.configstring (CS_GENERAL + 3, teaminfo[TEAM_B].statstatus);
	}

	if (tdm_match_status >= MM_PLAYING)
	{
		int		i;

		for (i = TEAM_A; i < TEAM_B; i++)
		{
			if (last_scores[i] != teaminfo[i].score || forceUpdate)
			{
				last_scores[i] = teaminfo[i].score;
				sprintf (teaminfo[i].statstatus, "%15d", teaminfo[i].score);
				gi.configstring (CS_GENERAL + 2 + (i - TEAM_A), teaminfo[i].statstatus);
			}
		}
	}

	if (tdm_match_status == MM_COUNTDOWN)
		time_remaining = level.match_start_framenum - level.framenum;
	else if (tdm_match_status == MM_WARMUP)
		time_remaining = g_match_time->value * 10 - 1;
	else
		time_remaining = level.match_end_framenum - level.framenum;

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
			gi.configstring (CS_GENERAL + 4, time_buffer);
		}
	}
}
