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

//This file handles macro expansion for say / say_team. That's it really, but it's
//a separate file since there is a potential for a lot of small functions.

#include "g_local.h"
#include "g_tdm.h"

const char *TDM_Macro_Health (edict_t *ent, size_t *length);
const char *TDM_Macro_LongArmor (edict_t *ent, size_t *length);
const char *TDM_Macro_ShortArmor (edict_t *ent, size_t *length);
const char *TDM_Macro_LongWeapon (edict_t *ent, size_t *length);
const char *TDM_Macro_ShortWeapon (edict_t *ent, size_t *length);
const char *TDM_Macro_Location (edict_t *ent, size_t *length);

typedef struct
{
	const char		*symbol;
	const size_t	symbol_length;	//to avoid mass strlen calls
	const char		*(*replacement) (edict_t *ent, size_t *length);
} tdm_macro_t;

//these are run in order, if you define %health after %h for example, %health would never
//work since it would become 100ealth or whatever.
static const tdm_macro_t tdm_macros[] = 
{
	{"%h", 2, TDM_Macro_Health},
	{"%A", 2, TDM_Macro_LongArmor},
	{"%a", 2, TDM_Macro_ShortArmor},
	{"%W", 2, TDM_Macro_LongWeapon},
	{"%w", 2, TDM_Macro_ShortWeapon},
	{"%l", 2, TDM_Macro_Location},
};

/*
==========
Health
==========
*/
const char *TDM_Macro_Health (edict_t *ent, size_t *length)
{
	static char	buff[8];

	*length = sprintf (buff, "%d", ent->health);
	return buff;
}

/*
==========
Armor (short)
==========
*/
const char *TDM_Macro_ShortArmor (edict_t *ent, size_t *length)
{
	static char	buff[8];
	int			index;

	index = ArmorIndex (ent);

	if (index == 0)
	{
		*length = 1;
		return "0";
	}

	*length = sprintf (buff, "%d", ent->client->inventory[index]);
	return buff;
}

/*
==========
Armor (long)
==========
*/
const char *TDM_Macro_LongArmor (edict_t *ent, size_t *length)
{
	static char	buff[32];
	int			index;

	index = ArmorIndex (ent);

	if (index == 0)
	{
		*length = 1;
		return "0";
	}

	*length = sprintf (buff, "%d %s", ent->client->inventory[index], GETITEM(index)->pickup_name);
	return buff;
}

/*
==========
Weapon (long)
==========
*/
const char *TDM_Macro_LongWeapon (edict_t *ent, size_t *length)
{
	static char	buff[32];

	if (!ent->client->weapon)
		return NULL;

	*length = sprintf (buff, "%s", ent->client->weapon->pickup_name);
	return buff;
}

/*
==========
Weapon (short)
==========
*/
const char *TDM_Macro_ShortWeapon (edict_t *ent, size_t *length)
{
	static char	buff[8];

	if (!ent->client->weapon)
		return NULL;

	*length = sprintf (buff, "%s", ent->client->weapon->shortname);
	return buff;
}

/*
==========
Location
==========
*/
const char *TDM_Macro_Location (edict_t *ent, size_t *length)
{
	edict_t		*e, *best;
	static char	buff[64];
	float		bestdist;
	float		height;
	int			bestindex;
	const char	*modifier;

	bestdist = 99999;
	modifier = NULL;
	best = NULL;

	for (e = g_edicts + game.maxclients + 1; e < g_edicts + globals.num_edicts; e++)
	{
		if (e->item && (((e->item->flags & (IT_WEAPON|IT_POWERUP)) && !(e->item->flags & IT_AMMO)) || (ITEM_INDEX(e->item) == ITEM_ITEM_HEALTH && e->count == 100)))
		{
			float	distance;
			vec3_t	diff;

			VectorSubtract (ent->s.origin, e->s.origin, diff);
			distance = VectorLength (diff);

			if (distance < bestdist && gi.inPVS (ent->s.origin, e->s.origin))
			{
				bestdist = distance;
				best = e;
				height = e->s.origin[2];
				bestindex = ITEM_INDEX (e->item);
			}
		}
	}

	//check for other items, apply modifier if possible
	for (e = g_edicts + game.maxclients + 1; e < g_edicts + globals.num_edicts; e++)
	{
		if (e != best && e->item && (((e->item->flags & (IT_WEAPON|IT_POWERUP)) && !(e->item->flags & IT_AMMO)) || (ITEM_INDEX(e->item) == ITEM_ITEM_HEALTH && e->count == 100)))
		{
			if (ITEM_INDEX (e->item) == bestindex && !modifier)
			{
				if (e->s.origin[2] < height)
					modifier = "upper ";
				else
					modifier = "lower ";

				break;
			}
		}
	}

	//FIXME: serverside .loc support?

	if (!best)
	{
		if (ent->waterlevel)
			*length = sprintf (buff, "water");
		else
			*length = sprintf (buff, "%s", level.mapname);
	}
	else
		*length = sprintf (buff, "%s%s", modifier ? modifier : "", best->item->shortname);
	return buff;
}

void TDM_MacroExpand (edict_t *ent, char *text, int maxlength)
{
	int			i;
	char		*position;
	const char	*replacement_text;
	size_t		len;
	size_t		replacement_length;

	len = strlen (text);

	//this could be more efficient i know, but the current way makes it easy to add/remove macros as well as
	//support different symbols for whatever.
	for (i = 0; i < sizeof(tdm_macros) / sizeof(tdm_macros[0]); i++)
	{
		position = text;

		while ((position = strstr (position, tdm_macros[i].symbol)))
		{
			replacement_text = tdm_macros[i].replacement (ent, &replacement_length);

			if (!replacement_text)
			{
				//no replacement text, shuffle existing text down
				memmove (position, position + tdm_macros[i].symbol_length, len - (position - text) - tdm_macros[i].symbol_length + 1);
				len -= tdm_macros[i].symbol_length;
			}
			else
			{
				//check for malicious macros
				if (len + replacement_length >= maxlength)
				{
					gi.dprintf ("TDM_MacroExpand: Overflowed while expanding macro from %s.\n", ent->client->pers.netname);
					return;
				}

				//if its the same size we got lucky, no need to memmove!
				if (replacement_length != tdm_macros[i].symbol_length)
					memmove (position + replacement_length, position + tdm_macros[i].symbol_length, len - (position - text) - tdm_macros[i].symbol_length + 1);
				memcpy (position, replacement_text, replacement_length);

				//skip over replacement to prevent infinite expansion
				position += replacement_length;
				len += replacement_length - tdm_macros[i].symbol_length;
			}
		}
	}
}