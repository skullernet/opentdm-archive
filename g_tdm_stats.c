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

//This file handles most stats tracking and some of the teamplayer stuff. Teamplayer
//is a structure used to record who is playing in a match at the match start, so in
//the event of a disconnection or similar, we can still have full info available.

#include "g_local.h"
#include "g_tdm.h"

static const int tdmg_weapons[] =
{
	0,
	0,
	ITEM_WEAPON_BLASTER,
	ITEM_WEAPON_SHOTGUN,
	ITEM_WEAPON_SUPERSHOTGUN,
	ITEM_WEAPON_MACHINEGUN,
	ITEM_WEAPON_CHAINGUN,
	ITEM_AMMO_GRENADES,
	ITEM_WEAPON_GRENADELAUNCHER,
	ITEM_WEAPON_ROCKETLAUNCHER,
	ITEM_WEAPON_HYPERBLASTER,
	ITEM_WEAPON_RAILGUN,
	ITEM_WEAPON_BFG,
};

//Looukp table  for meansOfDeath. MUST BE KEPT IN
//SYNC WITH MOD_*!!
static const int meansOfDeathToTDMG[] =
{
	0,
	TDMG_BLASTER,
	TDMG_SHOTGUN,
	TDMG_SSHOTGUN,
	TDMG_MACHINEGUN,
	TDMG_CHAINGUN,
	TDMG_GRENADELAUNCHER,
	TDMG_GRENADELAUNCHER,
	TDMG_ROCKETLAUNCHER,
	TDMG_ROCKETLAUNCHER,
	TDMG_HYPERBLASTER,
	TDMG_RAILGUN,
	TDMG_BFG10K,
	TDMG_BFG10K,
	TDMG_BFG10K,
	TDMG_HANDGRENADE,
	TDMG_HANDGRENADE,
	TDMG_WORLD,
	TDMG_WORLD,
	TDMG_WORLD,
	TDMG_WORLD,
	0,
	TDMG_WORLD,
	0,
	TDMG_HANDGRENADE,
	TDMG_WORLD,
	TDMG_WORLD,
	TDMG_WORLD,
	0,
	TDMG_WORLD,
	TDMG_WORLD,
	TDMG_WORLD,
	TDMG_WORLD,
	TDMG_WORLD,
};

typedef struct
{
	float	percentage;
	int		index;
} statsort_t;

int TDM_PercentageSort (void const *a, void const *b)
{
	statsort_t	*a1 = (statsort_t *)a;
	statsort_t	*b1 = (statsort_t *)b;

	if (a1->percentage > b1->percentage)
		return -1;
	else if (a1->percentage < b1->percentage)
		return 1;

	return 0;
}

typedef struct
{
	int		amount;
	int		index;
} intsort_t;

int TDM_DamageSort (void const *a, void const *b)
{
	intsort_t	*a1 = (intsort_t *)a;
	intsort_t	*b1 = (intsort_t *)b;

	if (a1->amount > b1->amount)
		return -1;
	else if (a1->amount < b1->amount)
		return 1;

	return 0;
}

/*
==============
TDM_WeaponFired
==============
Client fired a weapon, 
*/
void TDM_WeaponFired (edict_t *ent)
{
	int	i;

	//ignore warmup
	if (tdm_match_status < MM_PLAYING || tdm_match_status == MM_SCOREBOARD)
		return;

	if (!ent->client->resp.teamplayerinfo)
		TDM_Error ("TDM_WeaponFired: Trying to track stats but no teamplayerinfo for client %s", ent->client->pers.netname);

	//FIXME optimize this, chaingun can call several times per frame.
	for (i = 0; i < sizeof(tdmg_weapons) / sizeof(tdmg_weapons[0]); i++)
	{
		if (tdmg_weapons[i] == ITEM_INDEX(ent->client->weapon))
		{
			ent->client->resp.teamplayerinfo->shots_fired[i]++;
			break;
		}
	}
}

/*
==============
TDM_BeginDamage
==============
Begin a sequence of possibly multiple damage, we only want to count one though for the weapon.
We are called like so - TDM_BeginDamage -> TDM_Damage(multiple) -> TDM_EndDamage (stop tracking)
*/
static qboolean	weapon_hit;

void TDM_BeginDamage (void)
{
	weapon_hit = false;
}

void TDM_Damage (edict_t *ent, edict_t *victim, edict_t *inflictor, int damage)
{
	int		weapon;

	//Ignore warmup
	if (tdm_match_status < MM_PLAYING || tdm_match_status == MM_SCOREBOARD)
		return;

	//Determine what weapon was used
	weapon = meansOfDeathToTDMG[meansOfDeath &~ MOD_FRIENDLY_FIRE];

	//Not something we care about
	if (!weapon)
		return;

	//No credit for destroying corpses in progress
	if (victim->deadflag)
		return;

	//For damage tracking we DO want to count every shot
	if (!victim->client->resp.teamplayerinfo)
		TDM_Error ("TDM_Damage: Trying to track stats but no teamplayerinfo for victim %s", victim->client->pers.netname);

	if (!ent->client->resp.teamplayerinfo)
		TDM_Error ("TDM_Damage: Trying to track stats but no teamplayerinfo for attacker %s", ent->client->pers.netname);

	ent->client->resp.teamplayerinfo->damage_dealt[weapon] += damage;
	victim->client->resp.teamplayerinfo->damage_received[weapon] += damage;

	//Hitting yourself doesn't count for accuracy, does for damage
	if (ent == victim)
		return;

	//Shooting teammates is not accurate!
	if (ent->client->resp.team == victim->client->resp.team)
		return;

	//Only count one hit, eg shotgun or rocket radius damage
	if (weapon_hit)
		return;

	//Add stats
	ent->client->resp.teamplayerinfo->shots_hit[weapon]++;

	//Hitsound
	//G_UnicastSound (ent, gi.soundindex ("parasite/paridle1.wav"), false);

	weapon_hit = true;
}

void TDM_EndDamage (void)
{
	//so future TDM_Damages from without a begindamage are ignored
	weapon_hit = true;
}

/*
==============
TDM_BuildAccuracyString
==============
Builds accuracy string - hits / misses per weapon.
*/
char *TDM_BuildAccuracyString (edict_t *ent, teamplayer_t *info)
{
	static char stats[1400];
	unsigned	i;
	unsigned	total_shot, total_hit;
	float		accuracy;
	statsort_t	float_indexes[TDMG_MAX];

	stats[0] = 0;

	strcat (stats, TDM_SetColorText (va ("ACCURACY\n")));

	//0 is invalid, 1 is world, not a weapon :)
	for (i = TDMG_BLASTER; i < TDMG_MAX; i++)
	{
		if (!info->shots_fired[i])
			accuracy = 0;
		else
			accuracy = ((float)info->shots_hit[i] / (float)info->shots_fired[i]) * 100;

		float_indexes[i].percentage = accuracy;
		float_indexes[i].index = i;
	}

	qsort (float_indexes + TDMG_BLASTER, TDMG_MAX - TDMG_BLASTER -1, sizeof(float_indexes[0]), TDM_PercentageSort);

	total_shot = total_hit = 0;

	for (i = TDMG_BLASTER; i < TDMG_MAX; i++)
	{
		if (info->shots_fired[float_indexes[i].index])
		{
			strcat (stats, va ("%-16.16s:%4d / %-5d (%.0f%%)\n", GETITEM(tdmg_weapons[float_indexes[i].index])->pickup_name,
				info->shots_hit[float_indexes[i].index], info->shots_fired[float_indexes[i].index], ceil (float_indexes[i].percentage)));

			total_shot += info->shots_fired[float_indexes[i].index];
			total_hit += info->shots_hit[float_indexes[i].index];
		}
	}

	strcat (stats, va("Overall accuracy: %.1f%%\n", ((float)total_hit / (float)total_shot) * 100));

	return stats;
}

/*
==============
TDM_BuildTeamAccuracyString
==============
Builds team accuracy string - hits / misses per weapon.
*/
char *TDM_BuildTeamAccuracyString (edict_t *ent, matchinfo_t *info, unsigned team)
{
	static char stats[1400];
	unsigned	i, j;
	unsigned	team_shot[TDMG_MAX];
	unsigned	team_hit[TDMG_MAX];
	float		accuracy;
	statsort_t	float_indexes[TDMG_MAX];
	unsigned	total_shot, total_hit;

	stats[0] = 0;
	memset (team_shot, 0, sizeof(team_shot));
	memset (team_hit, 0, sizeof(team_hit));

	strcat (stats, TDM_SetColorText (va ("TEAM ACCURACY\n")));

	for (j = 0; j < info->num_teamplayers; j++)
	{
		if (info->teamplayers[j].team == team)
		{
			for (i = TDMG_BLASTER; i < TDMG_MAX; i++)
			{
				team_shot[i] += info->teamplayers[j].shots_fired[i];
				team_hit[i] += info->teamplayers[j].shots_hit[i];
			}
		}
	}

	//0 is invalid, 1 is world, not a weapon :)
	for (i = TDMG_BLASTER; i < TDMG_MAX; i++)
	{
		if (!team_shot[i])
			accuracy = 0;
		else
			accuracy = ((float)team_hit[i] / (float)team_shot[i]) * 100;

		float_indexes[i].percentage = accuracy;
		float_indexes[i].index = i;
	}

	qsort (float_indexes + TDMG_BLASTER, TDMG_MAX - TDMG_BLASTER -1, sizeof(float_indexes[0]), TDM_PercentageSort);

	total_shot = total_hit = 0;

	for (i = TDMG_BLASTER; i < TDMG_MAX; i++)
	{
		int	index;

		index = float_indexes[i].index;

		if (team_shot[index])
		{
			strcat (stats, va ("%-16.16s:%4d / %-5d (%.0f%%)\n", GETITEM(tdmg_weapons[index])->pickup_name,
				team_hit[index], team_shot[index], ceil (float_indexes[i].percentage)));

			total_shot += team_shot[index];
			total_hit += team_hit[index];
		}
	}

	strcat (stats, va("Overall accuracy: %.1f%%\n", ((float)total_hit / (float)total_shot) * 100));

	return stats;
}

/*
==============
TDM_BuildDamageString
==============
Builds damage string - damage given/taken per weapon + total.
*/
char *TDM_BuildDamageString (edict_t *ent, teamplayer_t *info)
{
	static char stats[1400];
	unsigned	i;
	int			damage;
	int			total_damage[2];
	intsort_t	int_indexes[TDMG_MAX];

	stats[0] = 0;

	strcat (stats, TDM_SetColorText (va ("DAMAGE (dealt / received)\n")));

	//0 is invalid, 1 is world, not a weapon :)
	for (i = TDMG_BLASTER; i < TDMG_MAX; i++)
	{
		damage = info->damage_dealt[i];

		int_indexes[i].amount = damage;
		int_indexes[i].index = i;
	}

	total_damage[0] = total_damage[1] = 0;

	qsort (int_indexes + TDMG_BLASTER, TDMG_MAX - TDMG_BLASTER - 1, sizeof(int_indexes[0]), TDM_DamageSort);

	for (i = TDMG_BLASTER; i < TDMG_MAX; i++)
	{
		int	index = int_indexes[i].index;
		if (info->damage_dealt[index] || info->damage_received[index])
		{
			total_damage[0] += info->damage_dealt[index];
			total_damage[1] += info->damage_received[index];

			strcat (stats, va ("%-16.16s:%5d / %-d\n", GETITEM(tdmg_weapons[index])->pickup_name,
				info->damage_dealt[index], info->damage_received[index]));
		}
	}

	strcat (stats, va ("%-16.16s:%5d / %-d\n", "Total Damage", total_damage[0], total_damage[1]));

	return stats;
}

/*
==============
TDM_BuildTeamDamageString
==============
Builds team damage string - damage given/taken per weapon + total.
*/
char *TDM_BuildTeamDamageString (edict_t *ent, matchinfo_t *info, unsigned team)
{
	static char stats[1400];
	unsigned	i, j;
	int			total_damage[2];
	intsort_t	int_indexes[TDMG_MAX];
	unsigned	team_damage_dealt[TDMG_MAX];
	unsigned	team_damage_taken[TDMG_MAX];

	stats[0] = 0;

	strcat (stats, TDM_SetColorText (va ("TEAM DAMAGE (dealt / received)\n")));

	memset (team_damage_dealt, 0, sizeof(team_damage_dealt));
	memset (team_damage_taken, 0, sizeof(team_damage_taken));

	for (j = 0; j < info->num_teamplayers; j++)
	{
		if (info->teamplayers[j].team == team)
		{
			for (i = TDMG_BLASTER; i < TDMG_MAX; i++)
			{
				team_damage_dealt[i] += info->teamplayers[j].damage_dealt[i];
				team_damage_taken[i] += info->teamplayers[j].damage_received[i];
			}
		}
	}

	//0 is invalid, 1 is world, not a weapon :)
	for (i = TDMG_BLASTER; i < TDMG_MAX; i++)
	{
		int_indexes[i].amount = team_damage_dealt[i];
		int_indexes[i].index = i;
	}

	total_damage[0] = total_damage[1] = 0;

	qsort (int_indexes + TDMG_BLASTER, TDMG_MAX - TDMG_BLASTER - 1, sizeof(int_indexes[0]), TDM_DamageSort);

	for (i = TDMG_BLASTER; i < TDMG_MAX; i++)
	{
		int	index = int_indexes[i].index;
		if (team_damage_dealt[index] || team_damage_taken[index])
		{
			total_damage[0] += team_damage_dealt[index];
			total_damage[1] += team_damage_taken[index];

			strcat (stats, va ("%-16.16s:%6d / %-d\n", GETITEM(tdmg_weapons[index])->pickup_name,
				team_damage_dealt[index], team_damage_taken[index]));
		}
	}

	strcat (stats, va ("%-16.16s:%6d / %-d\n", "Total Damage", total_damage[0], total_damage[1]));

	return stats;
}

/*
==============
TDM_BuildItemsString
==============
Builds item string - grabbed / missed.
*/
char *TDM_BuildItemsString (edict_t *ent, teamplayer_t *info)
{
	static char stats[1400];
	unsigned	i;
	float		percent;
	statsort_t	float_indexes[MAX_ITEMS];
	qboolean	basic;

	stats[0] = 0;
	basic = (tdm_match_status >= MM_PLAYING && tdm_match_status != MM_SCOREBOARD) && ent->client->resp.team;

	if (basic)
		strcat (stats, TDM_SetColorText (va ("ITEMS\n")));
	else
		strcat (stats, TDM_SetColorText (va ("ITEMS (grabbed / missed)\n")));

	for (i = 1; i < game.num_items; i++)
	{
		if (!info->items_collected[i])
			percent = 0;
		else
		{
			//even guessing the percentage could give away too much 
			if (basic)
				percent = (float)info->items_collected[i];
			else
				percent = ((float)info->items_collected[i] / (float)info->matchinfo->item_spawn_count[i] * 100);
		}

		float_indexes[i].percentage = percent;
		float_indexes[i].index = i;
	}

	qsort (float_indexes+1, game.num_items-1, sizeof(float_indexes[0]), TDM_PercentageSort);

	for (i = 1; i < game.num_items; i++)
	{
		const gitem_t	*item;
		int				index = float_indexes[i].index;

		item = GETITEM (index);

		if (info->items_collected[index] || info->matchinfo->item_spawn_count[index])
		{
			if (basic)
			{
				if (!info->items_collected[index])
					continue;

				//if player is requesting his own stats, don't show total spawned yet as it could
				//possibly be used to determine things - eg when mega has respawned
				strcat (stats, va ("%-16.16s:%3d\n", item->pickup_name, info->items_collected[index]));
			}
			else
			{
				strcat (stats, va ("%-16.16s:%3d / %-3d (%.f%%)\n", item->pickup_name,
					info->items_collected[index], info->matchinfo->item_spawn_count[index] - info->items_collected[index], float_indexes[i].percentage));
			}
		}
	}

	return stats;
}

/*
==============
TDM_BuildTeamItemsString
==============
Builds item string for whole team.
*/
char *TDM_BuildTeamItemsString (edict_t *ent, matchinfo_t *info, unsigned team)
{
	static char stats[1400];
	unsigned	i, j, team_collected[MAX_ITEMS];
	float		percent;
	statsort_t	float_indexes[MAX_ITEMS];
	qboolean	basic;

	stats[0] = 0;
	basic = (tdm_match_status >= MM_PLAYING && tdm_match_status != MM_SCOREBOARD) && ent->client->resp.team;

	if (basic)
		strcat (stats, TDM_SetColorText (va ("TEAM ITEMS\n")));
	else
		strcat (stats, TDM_SetColorText (va ("TEAM ITEMS (grabbed / missed)\n")));

	memset (team_collected, 0, sizeof(team_collected));

	for (j = 0; j < info->num_teamplayers; j++)
	{
		if (info->teamplayers[j].team == team)
		{
			for (i = 1; i < game.num_items; i++)
			{
				team_collected[i] += info->teamplayers[j].items_collected[i];
			}
		}
	}

	for (i = 1; i < game.num_items; i++)
	{
		if (!team_collected[i])
			percent = 0;
		else
		{
			//even guessing the percentage could give away too much 
			if (basic)
				percent = (float)team_collected[i];
			else
				percent = ((float)team_collected[i] / (float)info->item_spawn_count[i] * 100);
		}

		float_indexes[i].percentage = percent;
		float_indexes[i].index = i;
	}

	qsort (float_indexes+1, game.num_items+1, sizeof(float_indexes[0]), TDM_PercentageSort);

	for (i = 1; i < game.num_items; i++)
	{
		const gitem_t	*item;
		int				index = float_indexes[i].index;

		item = GETITEM (index);

		if (team_collected[index] || info->item_spawn_count[index])
		{
			if (basic)
			{
				if (!team_collected[index])
					continue;

				//if player is requesting his own stats, don't show total spawned yet as it could
				//possibly be used to determine things - eg when mega has respawned
				strcat (stats, va ("%-16.16s:%3d\n", GETITEM(index)->pickup_name, team_collected[index]));
			}
			else
			{
				strcat (stats, va ("%-16.16s:%3d / %-3d (%.f%%)\n", GETITEM(index)->pickup_name,
					team_collected[index], info->item_spawn_count[index] - team_collected[index], float_indexes[i].percentage));
			}
		}
	}

	return stats;
}

/*
==============
TDM_WriteStatsString
==============
Shows shitloads of useless information. Note it isn't hard to
overflow the 1024 byte cprintf limit, which is why this is
split into multiple cprints. On non-R1Q2 servers, this will
probably cause overflows!
*/
void TDM_WriteStatsString (edict_t *ent, teamplayer_t *info)
{
	char		*extra;
	static char	stats[1024];

	stats[0] = 0;

	//======= GENERAL ========
	strcat (stats, TDM_SetColorText (va ("GENERAL\n")));
	strcat (stats, va ("Kills    : %d\n", info->enemy_kills));
	strcat (stats, va ("Deaths   : %d\n", info->deaths));
	strcat (stats, va ("Suicides : %d\n", info->suicides));
	strcat (stats, va ("Teamkills: %d\n", info->team_kills));

	//======= DAMAGE ========
	extra = TDM_BuildDamageString (ent, info);

	//need to flush?
	if (strlen(stats) + strlen(extra) >= sizeof(stats)-1)
	{
		gi.cprintf (ent, PRINT_HIGH, "%s", stats);
		stats[0] = 0;
	}

	strcat (stats, "\n");
	strcat (stats, extra);

	//======= ITEMS ========
	//no item stats in itdm
	if (info->matchinfo->game_mode != GAMEMODE_ITDM)
	{
		extra = TDM_BuildItemsString (ent, info);

		//need to flush?
		if (strlen(stats) + strlen(extra) >= sizeof(stats)-1)
		{
			gi.cprintf (ent, PRINT_HIGH, "%s", stats);
			stats[0] = 0;
		}

		strcat (stats, "\n");
		strcat (stats, extra);
	}

	//======= ACCURACY ========
	extra = TDM_BuildAccuracyString (ent, info);

	//need to flush?
	if (strlen(stats) + strlen(extra) >= sizeof(stats)-1)
	{
		gi.cprintf (ent, PRINT_HIGH, "%s", stats);
		stats[0] = 0;
	}

	strcat (stats, "\n");
	strcat (stats, extra);

	if (strlen (stats) < 800)
		strcat (stats, "\nThis is still a work in progress... if you have any interesting ideas for stats, post on www.opentdm.net!\n");

	gi.cprintf (ent, PRINT_HIGH, "%s", stats);
}

void TDM_WriteTeamStatsString (edict_t *ent, matchinfo_t *info, unsigned team)
{
	char		*extra;
	static char	stats[1024];

	stats[0] = 0;

	//======= GENERAL ========
	//FIXME
	/*strcat (stats, TDM_SetColorText (va ("GENERAL\n")));
	strcat (stats, va ("Kills    : %d\n", info->enemy_kills));
	strcat (stats, va ("Deaths   : %d\n", info->deaths));
	strcat (stats, va ("Suicides : %d\n", info->suicides));
	strcat (stats, va ("Teamkills: %d\n", info->team_kills));*/

	//======= DAMAGE ========
	extra = TDM_BuildTeamDamageString (ent, info, team);

	//need to flush?
	if (strlen(stats) + strlen(extra) >= sizeof(stats)-1)
	{
		gi.cprintf (ent, PRINT_HIGH, "%s", stats);
		stats[0] = 0;
	}

	strcat (stats, "\n");
	strcat (stats, extra);

	//======= ITEMS ========
	//no item stats in itdm
	if (info->game_mode != GAMEMODE_ITDM)
	{
		extra = TDM_BuildTeamItemsString (ent, info, team);

		//need to flush?
		if (strlen(stats) + strlen(extra) >= sizeof(stats)-1)
		{
			gi.cprintf (ent, PRINT_HIGH, "%s", stats);
			stats[0] = 0;
		}

		strcat (stats, "\n");
		strcat (stats, extra);
	}

	//======= ACCURACY ========
	extra = TDM_BuildTeamAccuracyString (ent, info, team);

	//need to flush?
	if (strlen(stats) + strlen(extra) >= sizeof(stats)-1)
	{
		gi.cprintf (ent, PRINT_HIGH, "%s", stats);
		stats[0] = 0;
	}

	strcat (stats, "\n");
	strcat (stats, extra);

	if (strlen (stats) < 800)
		strcat (stats, "\nThis is still a work in progress... if you have any interesting ideas for stats, post on www.opentdm.net!\n");

	gi.cprintf (ent, PRINT_HIGH, "%s", stats);
}

/*
==============
TDM_RemoveStatsLink
==============
A player just disconnected, so remove the pointer to their edict
from the stats info to avoid new clients taking the same ent and
messing things up.
*/
void TDM_RemoveStatsLink (edict_t *ent)
{
	int		i;

	if (current_matchinfo.teamplayers)
		for (i = 0; i < current_matchinfo.num_teamplayers; i++)
		{
			if (current_matchinfo.teamplayers[i].client == ent)
				current_matchinfo.teamplayers[i].client = NULL;
		}

	if (old_matchinfo.teamplayers)
		for (i = 0; i < old_matchinfo.num_teamplayers; i++)
		{
			if (old_matchinfo.teamplayers[i].client == ent)
				old_matchinfo.teamplayers[i].client = NULL;
		}
}

/*
==============
TDM_GetInfoForPlayer
==============
Searches matchinfo for a player matching gi.args.
*/
teamplayer_t *TDM_GetInfoForPlayer (edict_t *ent, matchinfo_t *matchinfo)
{
	teamplayer_t	*victim, *info;
	int				count;

	if (!matchinfo->teamplayers)
	{
		gi.cprintf (ent, PRINT_HIGH, "That information is not available yet.\n");
		return NULL;
	}
	
	victim = NULL;

	count = matchinfo->num_teamplayers;
	info = matchinfo->teamplayers;

	if (gi.argc() < 2)
	{
		int		i;

		//viewing own stats mid-game
		if (ent->client->resp.teamplayerinfo)
		{
			victim = ent->client->resp.teamplayerinfo;
			return victim;
		}

		//viewing chasee stats
		if (ent->client->chase_target && ent->client->chase_target->client->resp.teamplayerinfo)
		{
			victim = ent->client->chase_target->client->resp.teamplayerinfo;
			return victim;
		}

		//finished game, checking stats
		for (i = 0; i < count; i++)
		{
			if (info[i].client == ent)
			{
				victim = info + i;
				return victim;
			}
		}

		if (!victim)
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: %s <name/id>\n", gi.argv(0));
			return victim;
		}
	}
	else
	{
		edict_t	*client;
		int		i;

		//match on existing player who still has a link to the stats
		if (LookupPlayer (gi.args(), &client, NULL))
		{
			for (i = 0; i < count; i++)
			{
				if (info[i].client == client)
				{
					victim = info + i;
					return victim;
				}
			}
		}

		//match on saved names
		if (!victim)
		{
			for (i = 0; i < count; i++)
			{
				if (!Q_stricmp (info[i].name, gi.args()))
				{
					victim = info + i;
					return victim;
				}
			}				
		}

		//match on partial lowered saved names
		if (!victim)
		{
			char	lowered_player[16];
			char	lowered_args[16];

			Q_strncpy (lowered_args, gi.args(), sizeof(lowered_args)-1);
			Q_strlwr (lowered_args);

			for (i = 0; i < count; i++)
			{
				strcpy (lowered_player, info[i].name);
				Q_strlwr (lowered_player);

				if (strstr (lowered_player, lowered_args))
				{
					victim = info + i;
					return victim;
				}
			}				
		}

		//well, we tried our best.
		if (!victim)
		{
			char	participants[1024];

			participants[0] = '\0';

			for (i = 0; i < count; i++)
			{
				if (participants[0])
					strcat (participants, ", ");

				strcat (participants, info[i].name);
			}

			gi.cprintf (ent, PRINT_HIGH, "No match for '%s'. Players in the match: %s\n", gi.args(), participants);
			return victim;
		}
	}

	return victim;
}

/*
==============
TDM_GetTeamFromMatchInfo
==============
Searches matchinfo for team of a player, or searches for a team based on gi.args.
*/
int TDM_GetTeamFromMatchInfo (edict_t *ent, matchinfo_t *matchinfo)
{
	teamplayer_t	*victim, *info;
	int				count;

	if (!matchinfo->teamplayers)
	{
		gi.cprintf (ent, PRINT_HIGH, "That information is not available yet.\n");
		return -1;
	}
	
	victim = NULL;

	count = matchinfo->num_teamplayers;
	info = matchinfo->teamplayers;

	if (gi.argc() < 2)
	{
		int		i;

		//viewing own stats mid-game
		if (ent->client->resp.teamplayerinfo)
			return ent->client->resp.teamplayerinfo->team;

		//viewing chasee stats - mmmmmonster deref!
		if (ent->client->chase_target && ent->client->chase_target->client->resp.teamplayerinfo)
			return ent->client->chase_target->client->resp.teamplayerinfo->team;

		//finished game, checking own stats
		for (i = 0; i < count; i++)
		{
			if (info[i].client == ent)
			{
				victim = info + i;
				return victim->team;
			}
		}

		if (!victim)
		{
			gi.cprintf (ent, PRINT_HIGH, "Usage: %s <teamname/id>\n", gi.argv(0));
			return -1;
		}
	}
	else
	{
		int		i;

		//match on a direct numeric team id or name to avoid conflicting with partial player name match below
		i = TDM_GetTeamFromArg (ent, gi.args());

		//ignore spec/invalid response
		if (i == TEAM_A || i == TEAM_B)
			return i;

		//match on saved names
		if (!victim)
		{
			for (i = 0; i < count; i++)
			{
				if (!Q_stricmp (info[i].name, gi.args()))
				{
					victim = info + i;
					return victim->team;
				}
			}				
		}

		//match on partial lowered saved names
		if (!victim)
		{
			char	lowered_player[16];
			char	lowered_args[16];

			Q_strncpy (lowered_args, gi.args(), sizeof(lowered_args)-1);
			Q_strlwr (lowered_args);

			for (i = 0; i < count; i++)
			{
				strcpy (lowered_player, info[i].name);
				Q_strlwr (lowered_player);

				if (strstr (lowered_player, lowered_args))
				{
					victim = info + i;
					return victim->team;
				}
			}				
		}

		gi.cprintf (ent, PRINT_HIGH, "Team '%s' not found. Use either the name of a player to look up their team, or use team 1 or 2.\n", gi.args());
	}

	return -1;
}

/*
==============
TDM_StatCheatCheck
==============
Returns true if someone in a match is trying to view stats of the other team. Prints
a message to ent if they are doing such.
*/
qboolean TDM_StatCheatCheck (edict_t *ent, unsigned team)
{
	if (tdm_match_status >= MM_PLAYING && tdm_match_status != MM_SCOREBOARD &&
		ent->client->resp.team && ent->client->resp.team != team)
	{
		gi.cprintf (ent, PRINT_HIGH, "You can only see stats for the other team after the match has finished.\n");
		return true;
	}

	return false;
}

/*
==============
TDM_Stats_f
==============
Show shitloads of useless information.
*/
void TDM_Stats_f (edict_t *ent, matchinfo_t *info)
{
	teamplayer_t	*victim;

	victim = TDM_GetInfoForPlayer (ent, info);
	if (!victim)
		return;

	if (TDM_StatCheatCheck (ent, victim->team))
		return;

	if (ent != victim->client)
		gi.cprintf (ent, PRINT_HIGH, "All statistics for %s\n", victim->name);

	TDM_WriteStatsString (ent, victim);
}

/*
==============
TDM_TeamStats_f
==============
Show shitloads of useless information.
*/
void TDM_TeamStats_f (edict_t *ent, matchinfo_t *info)
{
	int				team;

	team = TDM_GetTeamFromMatchInfo (ent, info);
	if (team == -1)
		return;

	if (TDM_StatCheatCheck (ent, team))
		return;

	if (team != (int)ent->client->resp.team)
		gi.cprintf (ent, PRINT_HIGH, "Team '%s':\n", info->teamnames[team]);

	if (TDM_Is1V1())
		TDM_WriteStatsString (ent, teaminfo[team].captain->client->resp.teamplayerinfo);
	else
		TDM_WriteTeamStatsString (ent, info, team);
		
}

/*
==============
TDM_Accuracy_f
==============
Show accuracy info.
*/
void TDM_Accuracy_f (edict_t *ent, matchinfo_t *info)
{
	const char		*stats;
	teamplayer_t	*victim;

	victim = TDM_GetInfoForPlayer (ent, info);
	if (!victim)
		return;

	if (TDM_StatCheatCheck (ent, victim->team))
		return;

	if (ent != victim->client)
		gi.cprintf (ent, PRINT_HIGH, "Accuracy for %s\n", victim->name);

	stats = TDM_BuildAccuracyString (ent, victim);

	gi.cprintf (ent, PRINT_HIGH, "%s", stats);
}

/*
==============
TDM_TeamAccuracy_f
==============
Show team accuracy info.
*/
void TDM_TeamAccuracy_f (edict_t *ent, matchinfo_t *info)
{
	const char		*stats;
	int				team;

	team = TDM_GetTeamFromMatchInfo (ent, info);
	if (team == -1)
		return;

	if (TDM_StatCheatCheck (ent, team))
		return;

	if (team != (int)ent->client->resp.team)
		gi.cprintf (ent, PRINT_HIGH, "Team '%s':\n", info->teamnames[team]);

	if (TDM_Is1V1())
		stats = TDM_BuildAccuracyString (ent, teaminfo[team].captain->client->resp.teamplayerinfo);
	else
		stats = TDM_BuildTeamAccuracyString (ent, info, team);

	gi.cprintf (ent, PRINT_HIGH, "%s", stats);
}

/*
==============
TDM_Damage_f
==============
Show damage info.
*/
void TDM_Damage_f (edict_t *ent, matchinfo_t *info)
{
	const char		*stats;
	teamplayer_t	*victim;

	victim = TDM_GetInfoForPlayer (ent, info);
	if (!victim)
		return;

	if (TDM_StatCheatCheck (ent, victim->team))
		return;

	if (ent != victim->client)
		gi.cprintf (ent, PRINT_HIGH, "Damage stats for %s\n", victim->name);

	stats = TDM_BuildDamageString (ent, victim);

	gi.cprintf (ent, PRINT_HIGH, "%s", stats);
}

/*
==============
TDM_TeamDamage_f
==============
Show team damage info.
*/
void TDM_TeamDamage_f (edict_t *ent, matchinfo_t *info)
{
	const char		*stats;
	int				team;

	team = TDM_GetTeamFromMatchInfo (ent, info);
	if (team == -1)
		return;

	if (TDM_StatCheatCheck (ent, team))
		return;

	if (team != (int)ent->client->resp.team)
		gi.cprintf (ent, PRINT_HIGH, "Team '%s':\n", info->teamnames[team]);

	if (TDM_Is1V1())
		stats = TDM_BuildDamageString (ent, teaminfo[team].captain->client->resp.teamplayerinfo);
	else
		stats = TDM_BuildTeamDamageString (ent, info, team);

	gi.cprintf (ent, PRINT_HIGH, "%s", stats);
}

/*
==============
TDM_Items_f
==============
Show items info.
*/
void TDM_Items_f (edict_t *ent, matchinfo_t *info)
{
	const char		*stats;
	teamplayer_t	*victim;

	victim = TDM_GetInfoForPlayer (ent, info);
	if (!victim)
		return;

	if (info->game_mode == GAMEMODE_ITDM)
	{
		gi.cprintf (ent, PRINT_HIGH, "Items stats are not available for matches played under ITDM rules.\n");
		return;
	}

	if (TDM_StatCheatCheck (ent, victim->team))
		return;

	if (ent != victim->client)
		gi.cprintf (ent, PRINT_HIGH, "Item stats for %s\n", victim->name);

	stats = TDM_BuildItemsString (ent, victim);

	gi.cprintf (ent, PRINT_HIGH, "%s", stats);
}

/*
==============
TDM_TeamItems_f
==============
Show items info.
*/
void TDM_TeamItems_f (edict_t *ent, matchinfo_t *info)
{
	const char		*stats;
	int				team;

	team = TDM_GetTeamFromMatchInfo (ent, info);
	if (team == -1)
		return;

	if (info->game_mode == GAMEMODE_ITDM)
	{
		gi.cprintf (ent, PRINT_HIGH, "Items stats are not available for matches played under ITDM rules.\n");
		return;
	}

	if (TDM_StatCheatCheck (ent, team))
		return;

	if (team != (int)ent->client->resp.team)
		gi.cprintf (ent, PRINT_HIGH, "Team '%s':\n", info->teamnames[team]);

	if (TDM_Is1V1())
		stats = TDM_BuildItemsString (ent, teaminfo[team].captain->client->resp.teamplayerinfo);
	else
		stats = TDM_BuildTeamItemsString (ent, info, team);

	gi.cprintf (ent, PRINT_HIGH, "%s", stats);
}

/*
==============
TDM_FindTeamplayerForJoinCode
==============
Get teamplayer info for a join code.
*/
teamplayer_t *TDM_FindTeamplayerForJoinCode (unsigned code)
{
	int	i;

	for (i = 0; i < current_matchinfo.num_teamplayers; i++)
	{
		if (current_matchinfo.teamplayers[i].joincode == code)
			return current_matchinfo.teamplayers + i;
	}

	return NULL;
}

/*
==============
TDM_SetupTeamPlayers
==============
Record who is in this match for stats / scores tracking in the event a player
disconnects mid-match or something.
*/
void TDM_SetupMatchInfoAndTeamPlayers (void)
{
	int		i;
	edict_t	*ent;

	current_matchinfo.game_mode = g_gamemode->value;
	current_matchinfo.timelimit = g_match_time->value / 60;

	strcpy (current_matchinfo.mapname, level.mapname);

	strcpy (current_matchinfo.teamnames[TEAM_A], teaminfo[TEAM_A].name);
	strcpy (current_matchinfo.teamnames[TEAM_B], teaminfo[TEAM_B].name);

	current_matchinfo.num_teamplayers = teaminfo[TEAM_A].players + teaminfo[TEAM_B].players;
	current_matchinfo.teamplayers = gi.TagMalloc (current_matchinfo.num_teamplayers * sizeof(teamplayer_t), TAG_GAME);

	for (i = 0, ent = g_edicts + 1; ent <= g_edicts + game.maxclients; ent++)
	{
		if (ent->client->resp.team)
		{
			const char *code;

			strcpy (current_matchinfo.teamplayers[i].name, ent->client->pers.netname);
			
			current_matchinfo.teamplayers[i].client = ent;
			current_matchinfo.teamplayers[i].ping = ent->client->ping;
			current_matchinfo.teamplayers[i].team = ent->client->resp.team;
			current_matchinfo.teamplayers[i].matchinfo = &current_matchinfo;

			//user has a preferred joincode they want to always use
			code = Info_ValueForKey (ent->client->pers.userinfo, "joincode");
			if (code[0])
			{
				unsigned	value;

				value = strtoul (code, NULL, 10);
				if (!value || TDM_FindTeamplayerForJoinCode (value))
					gi.cprintf (ent, PRINT_HIGH, "Your preferred join code could not be set.\n");
				else
					current_matchinfo.teamplayers[i].joincode = value;
			}

			if (!current_matchinfo.teamplayers[i].joincode)
			{
				//no prefered code, they get random
				do
				{
					current_matchinfo.teamplayers[i].joincode = genrand_int31 () % 9999;
				} while (!TDM_FindTeamplayerForJoinCode (current_matchinfo.teamplayers[i].joincode));
			}

			G_StuffCmd (ent, "set joincode \"%u\" u\n", current_matchinfo.teamplayers[i].joincode);
			gi.cprintf (ent, PRINT_HIGH, "Your join code for this match is %s\n", TDM_SetColorText(va("%u", current_matchinfo.teamplayers[i].joincode)));

			ent->client->resp.teamplayerinfo = current_matchinfo.teamplayers + i;

			i++;
		}
	}
}

/*
==============
TDM_IsTrackableItem
==============
Returns true for items we want to keep track of for stats.
*/
qboolean TDM_IsTrackableItem (edict_t *ent)
{
	if (!ent->item)
		TDM_Error ("TDM_IsTrackableItem: Got entity %d with no item", ent - g_edicts);

	//we don't track stuff during warmup
	if (tdm_match_status < MM_PLAYING || tdm_match_status == MM_SCOREBOARD)
		return false;

	//its health, but not megahealth
	if (ent->item == GETITEM(ITEM_ITEM_HEALTH) && !(ent->style & HEALTH_TIMED))
		return false;

	//useless counting this
	if (ent->item->flags & IT_AMMO)
		return false;

	//ignore tossed / dropped weapons
	if (ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM))
		return false;

	//ignore weapons if weapon stay is enabled
	if ((ent->item->flags & IT_WEAPON) && ((int)dmflags->value & DF_WEAPONS_STAY))
		return false;

	//armor shards aren't worth tracking
	if (ent->item->tag == ARMOR_SHARD)
		return false;

	//by now we should have everything else - armor, weapons, powerups and mh
	return true;
}

/*
==============
TDM_ItemSpawned
==============
An item (re)spawned, track count for stats.
*/
void TDM_ItemSpawned (edict_t *ent)
{
	if (!TDM_IsTrackableItem (ent))
		return;
	
	current_matchinfo.item_spawn_count[ITEM_INDEX(ent->item)]++;
}

/*
==============
TDM_ItemGrabbed
==============
Someone grabbed an item, do stuff for stats if needed.
*/
void TDM_ItemGrabbed (edict_t *ent, edict_t *player)
{
	//do we want to track it?
	if (!TDM_IsTrackableItem (ent))
		return;

	//something bad happened if this is hit!
	if (!player->client->resp.teamplayerinfo)
		TDM_Error ("TDM_ItemGrabbed: No teamplayerinfo for client %d", player - g_edicts - 1);

	//add it
	player->client->resp.teamplayerinfo->items_collected[ITEM_INDEX(ent->item)]++;
}
