#include "g_local.h"

teaminfo_t	teaminfo[MAX_TEAMS];

void JoinTeam1 (edict_t *ent)
{
	ent->client->resp.team = TEAM_A;
	TDM_TeamsChanged ();
	PutClientInServer (ent);
}

void JoinTeam2 (edict_t *ent)
{
	ent->client->resp.team = TEAM_B;
	TDM_TeamsChanged ();
	PutClientInServer (ent);
}

void ToggleChaseCam (edict_t *ent)
{
	if (ent->client->chase_target)
	{
		ent->client->chase_target = NULL;
		ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
	}
	else
		GetChaseTarget(ent);

	PMenu_Close (ent);
}

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

void CountPlayers (void)
{
	edict_t	*ent;
	int		i;

	for (i = 0; i < MAX_TEAMS; i++)
		teaminfo[i].players = 0;

	for (ent = g_edicts + 1; ent <= g_edicts + (int)maxclients->value; ent++)
	{
		if (ent->inuse)
			teaminfo[ent->client->resp.team].players++;
	}
}

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

	for (ent = g_edicts + 1; ent <= g_edicts + (int)maxclients->value; ent++)
	{
		if (ent->inuse && ent->client->menu.active && ent->client->menu.entries == joinmenu)
		{
			PMenu_Update (ent);
			gi.unicast (ent, true);
		}
	}
}

void TDM_TeamsChanged (void)
{
	CountPlayers ();
	UpdateTeamMenu ();
}

void TDM_ShowTeamMenu (edict_t *ent)
{
	PMenu_Open (ent, joinmenu, teamJoinEntries[ent->client->resp.team], MENUSIZE_JOINMENU, false);
}

void TDM_SetupClient (edict_t *ent)
{
	ent->client->resp.team = TEAM_SPEC;
	TDM_TeamsChanged ();
	TDM_ShowTeamMenu (ent);
}

void TDM_ResetGameState (void)
{
	
}

void TDM_Init (void)
{
	strcpy (teaminfo[0].name, "Observers");

	strcpy (teaminfo[1].name, "Hometeam");
	strcpy (teaminfo[2].name, "Visitors");

	sprintf (teaminfo[1].statname, "%32s", teaminfo[1].name);
	sprintf (teaminfo[2].statname, "%32s", teaminfo[2].name);

	strcpy (teaminfo[1].skin, "male/grunt");
	strcpy (teaminfo[2].skin, "female/athena");

	gi.configstring (CS_GENERAL + 0, teaminfo[1].statname);
	gi.configstring (CS_GENERAL + 1, teaminfo[2].statname);
}
