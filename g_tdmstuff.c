#include "g_local.h"

teaminfo_t	teaminfo[MAX_TEAMS];
matchmode_t	tdm_match_status;

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

typedef struct powerupvote_s
{
	const char		*names[1];
	unsigned		value;
	int				itemindex;
} powerupinfo_t;

powerupinfo_t	powerupvotes[] = 
{
	{{"quad"}, POWERUP_QUAD, ITEM_ITEM_QUAD},
	{{"invul"}, POWERUP_INVULN, ITEM_ITEM_INVULNERABILITY,},
	{{"ps"}, POWERUP_POWERSHIELD, ITEM_ITEM_POWER_SHIELD},
	{{"powerscreen"}, POWERUP_POWERSCREEN, ITEM_ITEM_POWER_SCREEN},
	{{"silencer"}, POWERUP_SILENCER, ITEM_ITEM_SILENCER},
	{{"rebreather"}, POWERUP_REBREATHER, ITEM_ITEM_BREATHER},
	{{"envirosuit"}, POWERUP_ENVIROSUIT, ITEM_ITEM_ENVIRO},
};

typedef enum
{
	VOTE_SUCCESS,
	VOTE_NOT_SUCCESS,
	VOTE_NOT_ENOUGH_VOTES
} vote_success_t;

typedef struct vote_s
{
	unsigned		flags;
	unsigned		newtimelimit;
	unsigned		newweaponflags;
	unsigned		newpowerupflags;
	unsigned		newmodeflags;
	char			newmap[MAX_QPATH];
	edict_t			*victim;
	edict_t			*initiator;
	int				end_frame;
	qboolean		active;
	vote_success_t	success;
	int				telemode;
	int				switchmode;
	int				tiemode;
	int				gamemode;
	unsigned		overtimemins;
} vote_t;

vote_t	vote;

//ugly define for variable frametime support
#define SECS(x) (x*(1/FRAMETIME))

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
			gi.error ("TDM_SaveDefaultCvars: Couldn't preserve %s", preserved->variable_name);

		preserved->default_string = G_CopyString (var->string);

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

	//immediately droptofloor and setup removed items
	for (ent = g_edicts + 1 + game.maxclients; ent < g_edicts + globals.num_edicts; ent++)
	{
		if (!ent->inuse)
			continue;

		//wision: add/remove items
		if (ent->item)
		{
			for (i = 0; i < sizeof(weaponvotes) / sizeof(weaponvotes[1]); i++)
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

			//wision: add/remove powerups
			for (i = 0; i < sizeof(powerupvotes) / sizeof(powerupvotes[1]); i++)
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
	strcat (settings, TDM_SetColorText(va("%g minutes\n", g_match_time->value / 60)));

	strcat (settings, "Overtime: ");
	switch ((int)g_tie_mode->value)
	{
		case 0:
			strcat (settings, TDM_SetColorText(va ("disabled")));
			break;
		case 1:
			strcat (settings, TDM_SetColorText(va ("%g minutes\n", g_overtime->value / 60)));
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
TDM_BeginCountdown
==============
All players are ready so start the countdown
*/
void TDM_BeginCountdown (void)
{
	gi.bprintf (PRINT_HIGH, "Match Settings:\n%s", TDM_SettingsString ());

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
	//for test server
	gi.bprintf (PRINT_CHAT, "Please report any bugs at www.opentdm.net.\n");

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

	level.match_score_end_framenum = level.framenum + (5.0f / FRAMETIME);
	TDM_BeginIntermission ();
}

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
TDM_RemoveVote
==============
Reset vote and all players' votes.
*/
static void TDM_RemoveVote (void)
{
	edict_t	*ent;

	memset (&vote, 0, sizeof(vote));
	/*vote.flags = 0;
	vote.active = false;
	vote.victim = NULL;
	vote.initiator = NULL;
	vote.success = VOTE_NOT_ENOUGH_VOTES;*/

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;
		
		ent->client->resp.vote = VOTE_HOLD;
	}
}

void TDM_Overtime (void)
{
	level.match_end_framenum = level.framenum + (int)(g_overtime->value / FRAMETIME);

	gi.bprintf (PRINT_HIGH, "Scores are tied %d - %d, adding %g minutes overtime.\n", teaminfo[TEAM_A].score, teaminfo[TEAM_A].score, g_overtime->value / 60);

	tdm_match_status = MM_OVERTIME;
}

void TDM_SuddenDeath (void)
{
	gi.bprintf (PRINT_HIGH, "Scores are tied %d - %d, entering Sudden Death!\n", teaminfo[TEAM_A].score, teaminfo[TEAM_A].score);

	tdm_match_status = MM_SUDDEN_DEATH;
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
			else if (remaining > 0 && remaining <= (int)(5.0f / FRAMETIME) && remaining % (int)(1.0f / FRAMETIME) == 0)
			{
				gi.bprintf (PRINT_HIGH, "%d\n", (int)(remaining * FRAMETIME));
			}
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
		//wision: do NOT restart match during the match
		//r1: under what conditions can/did this happen? late joining shouldn't be possible?
		if (tdm_match_status < MM_COUNTDOWN)
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

	if (TDM_RateLimited (ent, SECS(1)))
		return;

	ent->client->resp.ready = !ent->client->resp.ready;

	gi.bprintf (PRINT_HIGH, "%s is %sready!\n", ent->client->pers.netname, ent->client->resp.ready ? "" : "NOT ");

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
	{
		gi.cprintf (ent, PRINT_HIGH, "You are already marked as not ready.\n");
		return;
	}

	ent->client->resp.ready = false;

	gi.bprintf (PRINT_HIGH, "%s is %sready!\n", ent->client->pers.netname, ent->client->resp.ready ? "" : "NOT ");

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

		TDM_Ready_f (ent2);
	}
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

void JoinedTeam (edict_t *ent);

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

	if (TDM_RateLimited (ent, SECS(1)))
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

	if (TDM_RateLimited (ent, SECS(2)))
		return;

	if (LookupPlayer (gi.args(), &victim, ent))
	{
		if (ent == victim)
		{
			gi.cprintf (ent, PRINT_HIGH, "You can't invite yourself!\n");
			return;
		}

		if (victim->client->resp.team == ent->client->resp.team)
		{
			gi.cprintf (ent, PRINT_HIGH, "%s is already in your team.\n", victim->client->pers.netname);
			return;
		}

		victim->client->resp.last_invited_by = ent;
		gi.centerprintf (victim, "You are invited to '%s\nby %s. Type ACCEPT in\nthe console to accept.\n", teaminfo[ent->client->resp.team].name, ent->client->pers.netname);
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
TDM_CheckVote
==============
Check vote for success or failure.
*/
static void TDM_CheckVote (void)
{
	int vote_hold = 0;
	int vote_yes = 0;
	int vote_no = 0;
	edict_t	*ent;

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (!ent->inuse)
			continue;

		else if (ent->client->resp.vote == VOTE_YES)
			vote_yes++;
		else if (ent->client->resp.vote == VOTE_NO)
 			vote_no++;
		else if (ent->client->resp.vote == VOTE_HOLD)
 			vote_hold++;
	}

	if (vote_yes > vote_hold + vote_no)
		vote.success = VOTE_SUCCESS;
	else if(vote_no > vote_hold + vote_yes)
		vote.success = VOTE_NOT_SUCCESS;
	else
		vote.success = VOTE_NOT_ENOUGH_VOTES;
}

/*
==============
TDM_ResetVotableVariables
==============
Everyone has left the server, so reset anything they voted back to defaults
*/
static void TDM_ResetVotableVariables (void)
{
	cvarsave_t	*var;

	gi.dprintf ("Resetting votable variables to defaults.\n");

	var = preserved_vars;

	while (var->variable_name)
	{
		gi.cvar_set (var->variable_name, var->default_string);
		var++;
	}

	TDM_UpdateConfigStrings (true);
}	

/*
==============
TDM_Disconnected
==============
A player disconnected, do things.
*/
void TDM_Disconnected (edict_t *ent)
{
	if (ent->client->resp.team)
		TDM_LeftTeam (ent);

	TDM_TeamsChanged ();

	if (tdm_match_status == MM_WARMUP && teaminfo[TEAM_SPEC].players == 0 && teaminfo[TEAM_A].players == 0 && teaminfo[TEAM_B].players == 0)
	{
		if (vote.active)
			TDM_RemoveVote ();
			
		TDM_ResetVotableVariables ();
	}
	else
		TDM_CheckVote ();

	//zero for connecting clients on server browsers
	ent->client->ps.stats[STAT_FRAGS] = 0;
}

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

	if (vote.flags & VOTE_TIMELIMIT)
	{
		sprintf (value, "%d", vote.newtimelimit * 60);
		g_match_time = gi.cvar_set ("g_match_time", value);
		gi.bprintf (PRINT_CHAT, "New timelimit: %d minutes\n", (int)vote.newtimelimit);
	}

	if (vote.flags & VOTE_MAP)
	{
		strcpy (level.nextmap, vote.newmap);
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
		TDM_ResetLevel();
	}

	if (vote.flags & VOTE_POWERUPS)
	{
		sprintf (value, "%d", vote.newpowerupflags);
		g_powerupflags = gi.cvar_set ("g_powerupflags", value);
		TDM_ResetLevel();
	}

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

	if (vote.flags & VOTE_OVERTIME)
	{
		sprintf (value, "%d", vote.overtimemins * 60);
		g_overtime = gi.cvar_set ("g_overtime", value);
		gi.bprintf (PRINT_CHAT, "New overtime: %d minutes\n", (int)vote.overtimemins);
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
	char	message[1400];
	char	what[1400];

	message[0] = 0;
	what[0] = 0;

	strcpy (message, vote.initiator->client->pers.netname);
	strcat (message, " started a vote: ");

	if (vote.flags & VOTE_TIMELIMIT)
	{
		strcat (what, va("timelimit %d", vote.newtimelimit));
	}
	
	if (vote.flags & VOTE_MAP)
	{
		if (what[0])
			strcat (what, ", ");
		strcat (what, va ("%smap %s", what[0] ? ", " : "", vote.newmap));
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

	if (vote.flags & VOTE_TIMELIMIT)
	{
		if (what[0])
			strcat (what, ", ");

		strcat (what, va("overtime %d", vote.overtimemins));
	}

	gi.bprintf (PRINT_CHAT, "%s%s\n", message, what);
}

/*
==============
TDM_Vote_f
==============
Vote command handler. Create new vote.
*/
static void TDM_Vote_f (edict_t *ent)
{
	const char *cmd;
	const char *value;

	if (gi.argc() < 2)
	{
		gi.cprintf (ent, PRINT_HIGH, "Usage: vote <timelimit/map/kick/powerups/weapons/gamemode/tiemode/telemode/switchmode> <value>\n"
			"For weapon/powerup votes, specify multiple combinations of + or -, eg -bfg, +quad -invuln, etc\n");
		return;
	}

	if (tdm_match_status != MM_WARMUP)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can only propose new settings during warmup.\n");
		return;
	}

	cmd = gi.argv(1);

	//if initiator wants to change vote reset the vote and start again
	if (vote.active && vote.initiator != ent && Q_stricmp (cmd, "yes") && Q_stricmp (cmd, "no"))
	{
		gi.cprintf (ent, PRINT_HIGH, "Another vote is already in progress.\n");
		return;
	}

	if (!Q_stricmp (cmd, "timelimit") || !Q_stricmp (cmd, "tl"))
	{
		int		limit;

		value = gi.argv(2);
		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote %s <mins>\n", cmd);
			return;
		}

		limit = atoi (value);
		if (limit < 1)
		{
			gi.cprintf (ent, PRINT_HIGH, "Invalid timelimit value.\n");
			return;
		}

		//check current timelimit
		if (g_match_time->value == limit * 60)
		{
			gi.cprintf (ent, PRINT_HIGH, "Timelimit is already at %d minutes.\n", limit);
			return;
		}

		if (vote.active)
		{
			gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
			TDM_RemoveVote ();
		}

		vote.flags |= VOTE_TIMELIMIT;
		vote.newtimelimit = limit;
		vote.initiator = ent;
		vote.end_frame = level.framenum + g_vote_time->value * (1 / FRAMETIME);
		vote.active = true;
		TDM_AnnounceVote ();
		ent->client->resp.vote = VOTE_YES;
	}
	else if (!Q_stricmp (cmd, "map"))
	{
		unsigned	i;
		size_t		len;

		value = gi.argv(2);

		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote map <mapname>\n");
			return;
		}

		//TODO: check map from maplist oO

		len = strlen (value);

		if (len >= MAX_QPATH - 1)
		{
			gi.cprintf (ent, PRINT_HIGH, "Invalid map name.\n");
			return;
		}

		for (i = 0; i < len; i++)
		{
			if (!isalnum (value[i]) && value[i] != '_' && value[i] != '-')
			{
				gi.cprintf (ent, PRINT_HIGH, "Invalid map name.\n");
				return;
			}
		}

		if (vote.active)
		{
			gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
			TDM_RemoveVote ();
		}

		strcpy (vote.newmap, value);

		vote.flags |= VOTE_MAP;
		vote.end_frame = level.framenum + g_vote_time->value * (1 / FRAMETIME);
		vote.initiator = ent;
		vote.active = true;
		TDM_AnnounceVote ();
		ent->client->resp.vote = VOTE_YES;
	}
	else if (!Q_stricmp (cmd, "weapons"))
	{
		char		modifier;
		unsigned	flags;
		int			i, j;
		qboolean	found;

		value = gi.argv(2);

		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote weapons <+/-><rg,cg,rl,..>\n");
			return;
		}

		//example weapons string, -ssg -bfg +rl
		//special string "all" sets all, eg -all +rg

//		for (i = 0; i < sizeof(weaponvotes) / sizeof(weaponvotes[0]); i++)
//			flags |= weaponvotes[i].ammovalue;
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
					return;
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
			return;
		}

		if (vote.active)
		{
			gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
			TDM_RemoveVote ();
		}

		vote.newweaponflags = flags;
		vote.flags |= VOTE_WEAPONS;
		vote.end_frame = level.framenum + g_vote_time->value * (1 / FRAMETIME);
		vote.initiator = ent;
		vote.active = true;
		TDM_AnnounceVote ();
		ent->client->resp.vote = VOTE_YES;
	}
	else if (!Q_stricmp (cmd, "kick"))
	{
		edict_t	*victim;

		value = gi.argv(2);

		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote kick <name/id>\n");
			return;
		}

		if (LookupPlayer (gi.argv(2), &victim, ent))
		{
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

			if (vote.active)
			{
				gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
				TDM_RemoveVote ();
			}

			vote.victim = victim;
			vote.flags |= VOTE_KICK;
			vote.end_frame = level.framenum + g_vote_time->value * (1 / FRAMETIME);
			vote.initiator = ent;
			vote.active = true;
			TDM_AnnounceVote ();
			ent->client->resp.vote = VOTE_YES;
		}
	}
	else if (!Q_stricmp (cmd, "powerups"))
	{
		char		modifier;
		unsigned	flags;
		int			i, j;
		qboolean	found;

		value = gi.argv(2);

		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote powerups <+/-><quad,invul,ps,..>\n");
			return;
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
					return;
				}
			}
		}

		if ((unsigned)g_powerupflags->value == flags)
		{
			gi.cprintf (ent, PRINT_HIGH, "That powerup config is already set!\n");
			return;
		}

		if (vote.active)
		{
			gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
			TDM_RemoveVote ();
		}

		vote.newpowerupflags = flags;
		vote.flags |= VOTE_POWERUPS;
		vote.end_frame = level.framenum + g_vote_time->value * (1 / FRAMETIME);
		vote.initiator = ent;
		vote.active = true;
		TDM_AnnounceVote ();
		ent->client->resp.vote = VOTE_YES;
	}
	else if (!Q_stricmp (cmd, "gamemode") || !Q_stricmp (cmd, "mode"))
	{
		int	gamemode;

		value = gi.argv(2);

		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote %s <tdm/itdm/1v1>\n", cmd);
			return;
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
			return;
		}

		if ((int)g_gamemode->value == gamemode)
		{
			gi.cprintf (ent, PRINT_HIGH, "That game mode is already set!\n");
			return;
		}

		if (vote.active)
		{
			gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
			TDM_RemoveVote ();
		}

		vote.flags |= VOTE_GAMEMODE;
		vote.gamemode = gamemode;
		vote.end_frame = level.framenum + g_vote_time->value * (1 / FRAMETIME);
		vote.initiator = ent;
		vote.active = true;
		TDM_AnnounceVote ();
		ent->client->resp.vote = VOTE_YES;
	}
	else if (!Q_stricmp (cmd, "tiemode"))
	{
		int	tiemode;

		value = gi.argv(2);

		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote tiemode <none/ot/sd>\nnone: game ties after timelimit\not: overtime added until a winner\nsd: sudden death, first frag wins\n");
			return;
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
			return;
		}

		if (g_tie_mode->value == tiemode)
		{
			gi.cprintf (ent, PRINT_HIGH, "That tie mode is already set!\n");
			return;
		}

		if (vote.active)
		{
			gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
			TDM_RemoveVote ();
		}

		vote.flags |= VOTE_TIEMODE;
		vote.tiemode = tiemode;

		vote.end_frame = level.framenum + g_vote_time->value * (1 / FRAMETIME);
		vote.initiator = ent;
		vote.active = true;
		TDM_AnnounceVote ();
		ent->client->resp.vote = VOTE_YES;
	}
	else if (!Q_stricmp (cmd, "telemode"))
	{
		int	telemode;

		value = gi.argv(2);

		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote telemode <normal/nofreeze>\nnormal: teleporters act like regular Q2, you freeze briefly on exit\nnofreeze: teleporters act like Q3, your velocity is maintained on exit\n");
			return;
		}

		if (!Q_stricmp (value, "normal") || !Q_stricmp (value, "freeze") || !Q_stricmp (value, "q2"))
			telemode = 0;
		else if (!Q_stricmp (value, "nofreeze") || !Q_stricmp (value, "q3"))
			telemode = 1;
		else
		{
			gi.cprintf (ent, PRINT_HIGH, "Unknown mode: %s\n", value);
			return;
		}

		if (g_teleporter_nofreeze->value == telemode)
		{
			gi.cprintf (ent, PRINT_HIGH, "That teleporter mode is already set!\n");
			return;
		}

		if (vote.active)
		{
			gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
			TDM_RemoveVote ();
		}

		vote.flags |= VOTE_TELEMODE;
		vote.telemode = telemode;

		vote.end_frame = level.framenum + g_vote_time->value * (1 / FRAMETIME);
		vote.initiator = ent;
		vote.active = true;
		TDM_AnnounceVote ();
		ent->client->resp.vote = VOTE_YES;
	}
	else if (!Q_stricmp (cmd, "switchmode"))
	{
		int	switchmode;

		value = gi.argv(2);

		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote switchmode <normal/fast/instant>\nnormal: regular Q2 weapon switch speed\nfast: weapon dropping animation is skipped\ninstant: all animations are skipped\n");
			return;
		}

		if (!Q_stricmp (value, "fast") || !Q_stricmp (value, "faster"))
			switchmode = 1;
		else if (!Q_stricmp (value, "instant"))
			switchmode = 2;
		else if (!Q_stricmp (value, "normal") || !Q_stricmp (value, "slow"))
			switchmode = 0;
		else
		{
			gi.cprintf (ent, PRINT_HIGH, "Unknown switch mode: %s\n", value);
			return;
		}

		if (g_fast_weap_switch->value == switchmode)
		{
			gi.cprintf (ent, PRINT_HIGH, "That weapon switch mode is already set!\n");
			return;
		}

		if (vote.active)
		{
			gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
			TDM_RemoveVote ();
		}

		vote.switchmode = switchmode;
		vote.flags |= VOTE_SWITCHMODE;

		vote.end_frame = level.framenum + g_vote_time->value * (1 / FRAMETIME);
		vote.initiator = ent;
		vote.active = true;
		TDM_AnnounceVote ();
		ent->client->resp.vote = VOTE_YES;
	}
	else if (!Q_stricmp (cmd, "overtime") || !Q_stricmp (cmd, "ot"))
	{
		int		limit;

		value = gi.argv(2);
		if (!value[0])
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: vote %s <mins>\n", cmd);
			return;
		}

		limit = atoi (value);
		if (limit < 1)
		{
			gi.cprintf (ent, PRINT_HIGH, "Invalid value.\n");
			return;
		}

		//check current timelimit
		if (g_overtime->value == limit * 60)
		{
			gi.cprintf (ent, PRINT_HIGH, "Overtime is already at %d minutes.\n", limit);
			return;
		}

		if (vote.active)
		{
			gi.bprintf (PRINT_HIGH, "Vote cancelled!\n");
			TDM_RemoveVote ();
		}

		vote.flags |= VOTE_OVERTIME;
		vote.overtimemins = limit;
		vote.initiator = ent;
		vote.end_frame = level.framenum + g_vote_time->value * (1 / FRAMETIME);
		vote.active = true;
		TDM_AnnounceVote ();
		ent->client->resp.vote = VOTE_YES;
	}
	else if (!Q_stricmp (cmd, "yes"))
	{
		if (!vote.active)
		{
			gi.cprintf (ent, PRINT_HIGH, "No vote in progress.\n");
		}

		if (TDM_RateLimited (ent, SECS(2)))
			return;
		
		if (ent->client->resp.vote == VOTE_HOLD)
		{
			ent->client->resp.vote = VOTE_YES;
			gi.bprintf (PRINT_HIGH, "%s voted YES.\n", ent->client->pers.netname);
		}
		else if (ent->client->resp.vote == VOTE_YES)
		{
			gi.cprintf (ent, PRINT_HIGH, "You have already voted yes.\n");
		}
		else if (ent->client->resp.vote == VOTE_NO)
		{
			ent->client->resp.vote = VOTE_YES;
			gi.bprintf (PRINT_HIGH, "%s changed his vote to YES.\n", ent->client->pers.netname);
		}
	}
	else if (!Q_stricmp (cmd, "no"))
	{
		if (!vote.active)
		{
			gi.cprintf (ent, PRINT_HIGH, "No vote in progress.\n");
		}

		if (TDM_RateLimited (ent, SECS(2)))
			return;
		
		if (ent->client->resp.vote == VOTE_HOLD)
		{
			ent->client->resp.vote = VOTE_NO;
			gi.bprintf (PRINT_HIGH, "%s voted NO.\n", ent->client->pers.netname);
		}
		else if (ent->client->resp.vote == VOTE_NO)
		{
			gi.cprintf (ent, PRINT_HIGH, "You have already voted no.\n");
		}
		else if (ent->client->resp.vote == VOTE_YES)
		{
			ent->client->resp.vote = VOTE_NO;
			gi.bprintf (PRINT_HIGH, "%s changed his vote to NO.\n", ent->client->pers.netname);
		}
	}
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Unknown vote action '%s'\n", cmd);
		return;
	}

	TDM_CheckVote();

	if (vote.success == VOTE_SUCCESS)
	{
		gi.bprintf (PRINT_HIGH, "Vote passed!\n");
		TDM_ApplyVote();
		TDM_RemoveVote();
	}
	else if (vote.success == VOTE_NOT_SUCCESS)
	{
		gi.bprintf (PRINT_HIGH, "Vote failed.\n");
		TDM_RemoveVote();
	}
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
		"ready        Toggle ready status\n"
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
	const char	*value;

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

	value = gi.argv (1);

	if (!Q_stricmp (value, "1") || !Q_stricmp (value, "A") || !Q_stricmp (value, teaminfo[TEAM_A].name))
	{
		if (ent->client->resp.team == TEAM_A)
		{
			gi.cprintf (ent, PRINT_HIGH, "You're already on that team!\n");
			return;
		}

		if (TDM_RateLimited (ent, SECS(2)))
			return;

		JoinTeam1 (ent);
	}
	else if (!Q_stricmp (value, "2") || !Q_stricmp (value, "B") || !Q_stricmp (value, teaminfo[TEAM_B].name))
	{
		if (ent->client->resp.team == TEAM_B)
		{
			gi.cprintf (ent, PRINT_HIGH, "You're already on that team!\n");
			return;
		}

		if (TDM_RateLimited (ent, SECS(2)))
			return;

		JoinTeam2 (ent);
	}
	else
	{
		gi.cprintf (ent, PRINT_HIGH, "Unknown team: %s\n", value);
	}
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
	else if (!Q_stricmp (cmd, "notready") || !Q_stricmp (cmd, "unready"))
		TDM_NotReady_f (ent);
	else if (!Q_stricmp (cmd, "kickplayer") || !Q_stricmp (cmd, "removeplayer") || !Q_stricmp (cmd, "remove"))
		TDM_KickPlayer_f (ent);
	else if (!Q_stricmp (cmd, "admin"))
		TDM_Admin_f (ent);
	else if (!Q_stricmp (cmd, "captain"))
		TDM_Captain_f (ent);
	else if (!Q_stricmp (cmd, "vote"))
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
	else if (!Q_stricmp (cmd, "menu") || !Q_stricmp (cmd, "ctfmenu"))
		TDM_ShowTeamMenu (ent);
	else if (!Q_stricmp (cmd, "commands"))
		TDM_Commands_f (ent);
	else if (!Q_stricmp (cmd, "join") || !Q_stricmp (cmd, "team"))
		TDM_Team_f (ent);
	else if (!Q_stricmp (cmd, "settings"))
		TDM_Settings_f (ent);
	else
		return false;

	return true;
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
				client->selected_item = ITEM_WEAPON_ROCKETLAUNCHER;
				client->weapon = GETITEM (ITEM_WEAPON_ROCKETLAUNCHER);
			}
			client->inventory[ITEM_ITEM_ARMOR_BODY] = 200;
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

/*
==============
TDM_UpdateTeamNames
==============
A rather messy function to handle team names in TDM and 1v1.
*/
void TDM_UpdateTeamNames (void)
{
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
		strncpy (teaminfo[TEAM_A].name, g_team_a_name->string, sizeof(teaminfo[TEAM_A].name)-1);
		strncpy (teaminfo[TEAM_B].name, g_team_b_name->string, sizeof(teaminfo[TEAM_B].name)-1);
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

	//no announce in 1v1, but captain is still silently used.
	if (ent && g_gamemode->value != GAMEMODE_1V1)
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
	if (g_gamemode->value != GAMEMODE_1V1)
		gi.bprintf (PRINT_HIGH, "%s joined team '%s'\n", ent->client->pers.netname, teaminfo[ent->client->resp.team].name);

	ent->client->resp.ready = false;

	//joining a team with no captain by default assigns.
	//FIXME: should this still assign even if the team has existing players?
	if (!teaminfo[ent->client->resp.team].captain)
		TDM_SetCaptain (ent->client->resp.team, ent);

	//nasty hack for setting team names for 1v1 mode
	TDM_UpdateTeamNames ();

	//wision: set skin for new player
	gi.configstring (CS_PLAYERSKINS + (ent - g_edicts) - 1, va("%s\\%s", ent->client->pers.netname, teaminfo[ent->client->resp.team].skin));
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
	int	oldteam;

	gi.bprintf (PRINT_HIGH, "%s left team '%s'\n", ent->client->pers.netname, teaminfo[ent->client->resp.team].name);

	//FIXME: do we want to pick a new captain or let players contend for it with 'captain' cmd?
	if (teaminfo[ent->client->resp.team].captain == ent)
		TDM_SetCaptain (ent->client->resp.team, NULL);

	//r1: messy team name fix for 1v1
	TDM_UpdateTeamNames ();

	oldteam = ent->client->resp.team;

	//wision: remove player from the team!
	ent->client->resp.team = TEAM_SPEC;
	TDM_TeamsChanged ();

	//unlock here if team emptied
	if (teaminfo[oldteam].players == 0)
		teaminfo[oldteam].locked = false;
}

qboolean CanJoin (edict_t *ent, unsigned team)
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
		gi.cprintf (ent, PRINT_HIGH, "Team '%s' is locked.\n", teaminfo[team].name);
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
	JoinedTeam (ent);
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

	if (tdm_match_status < MM_PLAYING)
		return;

	//duplicated - eg TDM_LeftTeam + ClientDisconnect
	if (level.match_end_framenum == level.framenum)
		return;

	for (team = 1; team < MAX_TEAMS; team++)
	{
		if (teaminfo[team].players < 1)
		{
			level.match_end_framenum = level.framenum;
			TDM_CheckTimes();
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
		sprintf (teamJoinText[i], "*Join %s", teaminfo[i].name);
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

	gi.cprintf (ent, PRINT_CHAT, "\nWelcome to OpenTDM, an open source OSP/Battle replacement. Please report any bugs at www.opentdm.net. Type 'commands' in the console for a brief command guide.\n\n");
}

/*
==============
TDM_MapChanged
==============
Called when a new level is about to begin.
*/
void TDM_MapChanged (void)
{
	//wision: apply item settings
	//r1: not needed, this is handled automatically in SpawnEntities
	//TDM_ResetLevel ();
	TDM_ResetGameState ();
	TDM_UpdateConfigStrings (true);
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

	level.match_start_framenum = 0;
	tdm_match_status = MM_WARMUP;
	TDM_ResetLevel ();

	teaminfo[TEAM_A].score = teaminfo[TEAM_B].score = 0;
	teaminfo[TEAM_A].players = teaminfo[TEAM_B].players = 0;
	teaminfo[TEAM_A].captain = teaminfo[TEAM_B].captain = NULL;
	teaminfo[TEAM_A].locked = teaminfo[TEAM_B].locked = false;

	TDM_UpdateTeamNames ();

	for (ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (ent->inuse)
		{
			ent->client->resp.last_command_frame = 0;
			ent->client->resp.last_invited_by = NULL;
			if (ent->client->resp.team != TEAM_SPEC)
			{
				ent->client->resp.team = TEAM_SPEC;
				PutClientInServer (ent);
			}
		}
	}

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
	strcpy (teaminfo[0].name, "Spectators");
	strcpy (teaminfo[0].skin, "male/grunt");

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
	static int			last_scores[MAX_TEAMS] = {-1, -1, -1};
	static matchmode_t	last_mode = MM_INVALID;
	int					time_remaining;

	if (g_team_a_name->modified || forceUpdate)
	{
		g_team_a_name->modified = false;
		sprintf (teaminfo[TEAM_A].statname, "%31s", teaminfo[TEAM_A].name);
		gi.configstring (CS_GENERAL + 0, teaminfo[TEAM_A].statname);
	}

	if (g_team_b_name->modified || forceUpdate)
	{
		g_team_b_name->modified = false;
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
				sprintf (teaminfo[TEAM_A].statstatus, "%15s", "READY");
				sprintf (teaminfo[TEAM_B].statstatus, "%15s", "READY");
				break;
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

		for (i = TEAM_A; i <= TEAM_B; i++)
		{
			if (last_scores[i] != teaminfo[i].score || forceUpdate)
			{
				last_scores[i] = teaminfo[i].score;
				sprintf (teaminfo[i].statstatus, "%15d", teaminfo[i].score);
				gi.configstring (CS_GENERAL + 2 + (i - TEAM_A), teaminfo[i].statstatus);
			}
		}
	}

	switch (tdm_match_status)
	{
		case MM_COUNTDOWN:
			time_remaining = level.match_start_framenum - level.framenum;
			break;
		case MM_WARMUP:
			time_remaining = g_match_time->value * 10 - 1;
			break;
		case MM_SUDDEN_DEATH:
			time_remaining = 0;
			break;
		default:
			time_remaining = level.match_end_framenum - level.framenum;
			break;
	}

	if (time_remaining != last_time_remaining || forceUpdate)
	{
		static int	last_secs = -1;
		char		time_buffer[8];
		int			mins, secs;

		last_time_remaining = time_remaining;

		if (tdm_match_status == MM_SUDDEN_DEATH)
		{
			gi.configstring (CS_GENERAL + 4, "Sudden Death");
		}
		else
		{
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

				gi.configstring (CS_GENERAL + 4, time_buffer);
			}
		}
	}
}
