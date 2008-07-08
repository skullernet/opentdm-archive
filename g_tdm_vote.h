/*===============
Voting Stuff
===============*/
void TDM_CheckVote (void);
void TDM_ResetLevel (void);
void TDM_RemoveVote (void);
void TDM_Vote_f (edict_t *ent);
qboolean TDM_ParseVoteConfigLine (char *line, int line_number, void *param);
qboolean TDM_RateLimited (edict_t *ent, int penalty);
void TDM_VoteWebConfigResult (edict_t *ent, int code, void *param);
void TDM_SetupVote (edict_t *ent);
void TDM_ConfigDownloaded (char *buff, int len, int code);
void TDM_CreateConfiglist (void);
void TDM_VoteMenuApply (edict_t *ent);
void TDM_UpdateVoteConfigString (void);

int LookupPlayer (const char *match, edict_t **out, edict_t *ent);


//votemenu.c
void OpenVoteMenu (edict_t *ent);
void VoteMenuGameMode (edict_t *ent);
void VoteMenuMap (edict_t *ent);
void VoteMenuConfig (edict_t *ent);
void VoteMenuMatchRestart (edict_t *ent);
void VoteMenuTimelimit (edict_t *ent);
void VoteMenuOvertime (edict_t *ent);
void VoteMenuPowerups (edict_t *ent);
void VoteMenuBFG (edict_t *ent);
void VoteMenuKick (edict_t *ent);
void VoteMenuChat (edict_t *ent);
void VoteMenuBugs (edict_t *ent);

#define VOTE_MENU_GAMEMODE	0x1
#define VOTE_MENU_MAP		0x2
#define VOTE_MENU_CONFIG	0x4
#define VOTE_MENU_TIMELIMIT	0x8
#define VOTE_MENU_OVERTIME	0x10
#define VOTE_MENU_POWERUPS	0x20
#define VOTE_MENU_BFG		0x40
#define VOTE_MENU_KICK		0x80
#define VOTE_MENU_CHAT		0x100
#define VOTE_MENU_BUGS		0x200

#define VOTE_MENU_ALL		0xFFFFFFFFU 

typedef struct weaponvote_s
{
	const char		*names[2];
	unsigned		value;
	int				itemindex;
} weaponinfo_t;

typedef struct powerupvote_s
{
	const char		*names[1];
	unsigned		value;
	int				itemindex;
} powerupinfo_t;

#define WEAPON_MAX		10
#define POWERUP_MAX		7

extern const weaponinfo_t	weaponvotes[WEAPON_MAX];
extern const powerupinfo_t	powerupvotes[POWERUP_MAX];

extern vote_t	vote;

extern qboolean	tdm_settings_not_default;

extern char			**tdm_configlist;

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
