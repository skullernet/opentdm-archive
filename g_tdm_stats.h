/*===============
Stats Stuff
===============*/
//extern teamplayer_t	*current_teamplayers ;
//extern int			num_current_teamplayers;

//extern teamplayer_t	*old_teamplayers ;
//extern int			num_old_teamplayers;

void TDM_Stats_f (edict_t *ent, matchinfo_t *info);
void TDM_Accuracy_f (edict_t *ent, matchinfo_t *info);
void TDM_Damage_f (edict_t *ent, matchinfo_t *info);
void TDM_Items_f (edict_t *ent, matchinfo_t *info);
void TDM_Weapons_f (edict_t *ent, matchinfo_t *info);

void TDM_TeamStats_f (edict_t *ent, matchinfo_t *info);
void TDM_TeamAccuracy_f (edict_t *ent, matchinfo_t *info);
void TDM_TeamDamage_f (edict_t *ent, matchinfo_t *info);
void TDM_TeamItems_f (edict_t *ent, matchinfo_t *info);
void TDM_TeamWeapons_f (edict_t *ent, matchinfo_t *info);

void TDM_RemoveStatsLink (edict_t *ent);
void TDM_SetupMatchInfoAndTeamPlayers (void);
void TDM_WriteStatsString (edict_t *ent, teamplayer_t *info);
teamplayer_t *TDM_FindTeamplayerForJoinCode (unsigned code);
void TDM_SetupTeamInfoForPlayer (edict_t *ent, teamplayer_t *info);
void TDM_Killed (edict_t *attacker, edict_t *victim, int mod);
