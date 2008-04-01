#ifndef _WIN32
#include "g_local.h"
#include <dirent.h>

static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH];
static	char	findpattern[MAX_OSPATH];
static	DIR		*fdir;

const char *Sys_FindFirst (char *filespec)
{
	struct dirent *d;
	char *p;

	if (fdir)
		TDM_Error ("Sys_FindFirst without close");

	strcpy (findbase, filespec);

	if ((p = strrchr(findbase, '/')) != NULL)
	{
		*p = 0;
		strcpy (findpattern, p + 1);
	}
	else
		strcpy (findpattern, "*");

	if (!strcmp(findpattern, "*.*"))
		strcpy(findpattern, "*");
	
	if ((fdir = opendir(findbase)) == NULL)
		return NULL;

	while ((d = readdir(fdir)) != NULL)
	{
		if (!*findpattern || glob_match(findpattern, d->d_name))
		{
			snprintf (findpath, sizeof(findpath)-1, "%s/%s", findbase, d->d_name);
			return findpath;
		}
	}

	return NULL;
}

const char *Sys_FindNext (void)
{
	struct dirent *d;

	if (fdir == NULL)
		return NULL;

	while ((d = readdir(fdir)) != NULL)
	{
		if (!*findpattern || glob_match(findpattern, d->d_name))
		{
			snprintf (findpath, sizeof(findpath), "%s/%s", findbase, d->d_name);
			return findpath;
		}
	}

	return NULL;
}

void Sys_FindClose (void)
{
	if (fdir != NULL)
		closedir(fdir);

	fdir = NULL;
}

void Sys_DebugBreak (void)
{
	asm ("int $3");
}

#endif
