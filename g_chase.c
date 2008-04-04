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

void ChaseEyeHack (edict_t *ent, edict_t *newplayer, edict_t *oldplayer)
{
	//yes this is sent twice on purpose - sending unreliable will ensure model is hidden
	//at the same time the new packetentities arrives, otherwise there will be a brief
	//duration if there is pending reliable where the model blocks the view.

	if (newplayer)
	{
		gi.WriteByte (svc_configstring);
		gi.WriteShort (CS_PLAYERSKINS + (newplayer - g_edicts) - 1);
		gi.WriteString (va ("%s\\opentdm/null", newplayer->client->pers.netname));
		gi.unicast (ent, true);

		gi.WriteByte (svc_configstring);
		gi.WriteShort (CS_PLAYERSKINS + (newplayer - g_edicts) - 1);
		gi.WriteString (va ("%s\\opentdm/null", newplayer->client->pers.netname));
		gi.unicast (ent, false);
	}

	if (oldplayer)
	{
		gi.WriteByte (svc_configstring);
		gi.WriteShort (CS_PLAYERSKINS + (oldplayer - g_edicts) - 1);
		gi.WriteString (va ("%s\\%s", oldplayer->client->pers.netname, teaminfo[oldplayer->client->pers.team].skin));
		gi.unicast (ent, true);

		gi.WriteByte (svc_configstring);
		gi.WriteShort (CS_PLAYERSKINS + (oldplayer - g_edicts) - 1);
		gi.WriteString (va ("%s\\%s", oldplayer->client->pers.netname, teaminfo[oldplayer->client->pers.team].skin));
		gi.unicast (ent, false);
	}
}

void DisableChaseCam (edict_t *ent)
{
	ChaseEyeHack (ent, NULL, ent->client->chase_target);

	//remove a gun model if we were using one for in-eyes
	ent->client->ps.gunframe = ent->client->ps.gunindex = 0;
	VectorClear (ent->client->ps.gunoffset);
	VectorClear (ent->client->ps.kick_angles);

	ent->client->ps.viewangles[ROLL] = 0;
	ent->client->chase_target = NULL;
	ent->client->ps.pmove.pm_flags &= ~PMF_NO_PREDICTION;
}

void NextChaseMode (edict_t *ent)
{
	ent->client->chase_mode = (ent->client->chase_mode + 1) % CHASE_MAX;

	if (ent->client->chase_mode == CHASE_EYES)
	{
		ChaseEyeHack (ent, ent->client->chase_target, NULL);
	}
	else if (ent->client->chase_mode == CHASE_THIRDPERSON)
	{
		//going 3rd person, remove gun and invisible player hack
		ChaseEyeHack (ent,  NULL, ent->client->chase_target);
		ent->client->ps.gunindex = ent->client->ps.gunframe = 0;
	}
	else if (ent->client->chase_mode == CHASE_FREE)
	{
		DisableChaseCam (ent);
	}
}

void UpdateChaseCam(edict_t *ent)
{
	vec3_t o, ownerv, goal;
	edict_t *targ;
	vec3_t forward, right;
	trace_t trace;
	int i;
	vec3_t oldgoal;
	vec3_t angles;

	// is our chase target gone?
	if (!ent->client->chase_target->inuse || !ent->client->chase_target->client->pers.team)
	{
		edict_t *old = ent->client->chase_target;
		ChaseNext(ent);
		if (ent->client->chase_target == old)
		{
			DisableChaseCam (ent);
			return;
		}
	}

	targ = ent->client->chase_target;

	if (ent->client->chase_mode == CHASE_EYES)
	{
		VectorCopy (targ->s.origin, goal);
		goal[2] += targ->viewheight;
	}
	else if (ent->client->chase_mode == CHASE_THIRDPERSON)
	{
		VectorCopy(targ->s.origin, ownerv);
		VectorCopy(ent->s.origin, oldgoal);

		ownerv[2] += targ->viewheight;

		VectorCopy(targ->client->v_angle, angles);
		if (angles[PITCH] > 56)
			angles[PITCH] = 56;
		AngleVectors (angles, forward, right, NULL);
		VectorNormalize(forward);
		VectorMA(ownerv, -50, forward, o);

		if (o[2] < targ->s.origin[2] + 20)
			o[2] = targ->s.origin[2] + 20;

		// jump animation lifts
		if (!targ->groundentity)
			o[2] += 16;

		trace = gi.trace(ownerv, vec3_origin, vec3_origin, o, targ, MASK_SOLID);

		VectorCopy(trace.endpos, goal);

		VectorMA(goal, 2, forward, goal);

		// pad for floors and ceilings
		VectorCopy(goal, o);
		o[2] += 6;
		trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
		if (trace.fraction < 1) {
			VectorCopy(trace.endpos, goal);
			goal[2] -= 6;
		}

		VectorCopy(goal, o);
		o[2] -= 6;
		trace = gi.trace(goal, vec3_origin, vec3_origin, o, targ, MASK_SOLID);
		if (trace.fraction < 1) {
			VectorCopy(trace.endpos, goal);
			goal[2] += 6;
		}
	}

	VectorCopy(goal, ent->s.origin);
	
	for (i=0 ; i<3 ; i++)
		ent->client->ps.pmove.delta_angles[i] = ANGLE2SHORT(targ->client->v_angle[i] - ent->client->resp.cmd_angles[i]);

	if (targ->deadflag)
	{
		ent->client->ps.viewangles[ROLL] = 40;
		ent->client->ps.viewangles[PITCH] = -15;
		ent->client->ps.viewangles[YAW] = targ->client->killer_yaw;
		ent->client->ps.pmove.pm_type = PM_DEAD;
	}
	else
	{
		VectorCopy(targ->client->v_angle, ent->client->ps.viewangles);
		VectorCopy(targ->client->v_angle, ent->client->v_angle);
		ent->client->ps.pmove.pm_type = PM_FREEZE;
	}

	ent->viewheight = 0;
	ent->client->ps.pmove.pm_flags |= PMF_NO_PREDICTION;
	gi.linkentity (ent);
}

void ChaseNext(edict_t *ent)
{
	int i;
	edict_t *e, *old;

	if (!ent->client->chase_target)
		return;

	old = ent->client->chase_target;
	i = ent->client->chase_target - g_edicts;
	do
	{
		i++;
		if (i > game.maxclients)
			i = 1;

		e = g_edicts + i;

		if (!e->inuse)
			continue;

		if (e->client->pers.team)
			break;
	} while (e != ent->client->chase_target);

	if (e != old && ent->client->chase_mode == CHASE_EYES)
		ChaseEyeHack (ent, e, old);

	ent->client->chase_target = e;
	ent->client->update_chase = true;
}

void ChasePrev(edict_t *ent)
{
	int i;
	edict_t *e, *old;

	if (!ent->client->chase_target)
		return;

	old = ent->client->chase_target;

	i = ent->client->chase_target - g_edicts;
	do
	{
		i--;
		if (i < 1)
			i = game.maxclients;
		e = g_edicts + i;

		if (!e->inuse)
			continue;

		if (e->client->pers.team)
			break;
	} while (e != ent->client->chase_target);

	if (e != old && ent->client->chase_mode == CHASE_EYES)
		ChaseEyeHack (ent, e, old);

	ent->client->chase_target = e;
	ent->client->update_chase = true;
}

void GetChaseTarget(edict_t *ent)
{
	int i;
	edict_t *other;

	

	for (i = 1; i <= game.maxclients; i++)
	{
		other = g_edicts + i;
		if (other->inuse && other->client->pers.team)
		{
			ent->client->chase_mode = CHASE_EYES;
			ChaseEyeHack (ent, other, ent->client->chase_target);
			ent->client->chase_target = other;
			ent->client->update_chase = true;
			UpdateChaseCam(ent);
			return;
		}
	}

	gi.centerprintf(ent, "No other players to chase.");
}

