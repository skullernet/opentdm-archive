/*===============
Core Stuff
===============*/
void JoinedTeam (edict_t *ent);
void droptofloor (edict_t *ent);
void JoinTeam1 (edict_t *ent);
void JoinTeam2 (edict_t *ent);
void ToggleChaseCam (edict_t *ent);
void SelectNextHelpPage (edict_t *ent);
void TDM_UpdateTeamNames (void);
void TDM_SetSkins (void);
void TDM_SaveDefaultCvars (void);
qboolean TDM_Is1V1 (void);
void TDM_ResumeGame (void);
qboolean TDM_ProcessJoinCode (edict_t *ent, unsigned value);
const char *TDM_SecsToString (int seconds);
void UpdateMatchStatus (void);
void TDM_HandleDownload (char *buff, int len, int code);
qboolean TDM_ProcessText (char *buff, int len, qboolean (*func)(char *, int, void *), void *param);
void TDM_ResetVotableVariables (void);
int TDM_GetTeamFromArg (edict_t *ent, const char *value);
void TDM_FixDeltaAngles (void);
void TDM_EndMatch (void);

typedef enum
{
	DL_NONE,
	DL_CONFIG,
	DL_POST_STATS,
} dltype_t;

//web config cache
typedef struct tdm_config_s
{
	struct tdm_config_s	*next;
	char				name[32];
	char				description[128];
	vote_t				settings;
	unsigned int		last_downloaded;
} tdm_config_t;

typedef struct tdm_download_s
{
	edict_t		*initiator;
	dltype_t	type;
	char		name[32];
	char		path[1024];
	void		(*onFinish)(edict_t *, int, void *);
} tdm_download_t;

extern tdm_download_t	tdm_download;
