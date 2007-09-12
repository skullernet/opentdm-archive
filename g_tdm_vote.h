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

//define for variable frametime support
#define SECS_TO_FRAMES(seconds)	(int)((seconds)/FRAMETIME)
#define FRAMES_TO_SECS(frames)	(int)((frames)*FRAMETIME)

int LookupPlayer (const char *match, edict_t **out, edict_t *ent);

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
