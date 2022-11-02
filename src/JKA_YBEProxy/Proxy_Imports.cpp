#include "Proxy_Header.hpp"

// ==================================================
// server engine
// ==================================================

// playerState_t* SV_GameClientNum(int clientNum)
playerState_t* Proxy_GetPlayerStateByClientNum(int clientNum)
{
	return (playerState_t*)((byte*)proxy.locatedGameData.g_clients + proxy.locatedGameData.g_clientSize * (clientNum));
}

/*
===========
SV_ClientCleanName

Gamecode to engine port (from OpenJK)
============
*/
// void SV_ClientCleanName(const char* in, char* out, int outSize)
void Proxy_ClientCleanName(const char* in, char* out, int outSize)
{
	int outpos = 0;
	int colorlessLen = 0;

	// discard leading spaces
	for (; *in == ' '; in++)
	{

	}

	// discard leading asterisk's (fail raven for using * as a skipnotify)
	// apparently .* causes the issue too so... derp
	for (; *in == '*'; in++)
	{

	}

	for (; *in && outpos < outSize - 1; in++)
	{
		out[outpos] = *in;

		if (*(in + 1) && *(in + 2))
		{
			if (*in == ' ' && *(in + 1) == ' ' && *(in + 2) == ' ') // don't allow more than 3 consecutive spaces
			{
				continue;
			}

			if (*in == '@' && *(in + 1) == '@' && *(in + 2) == '@') // don't allow too many consecutive @ signs
			{
				continue;
			}
		}

		if ((byte)*in < 0x20)
		{
			continue;
		}

		switch ((byte)* in)
		{
		default:
			break;
		case 0x81:
		case 0x8D:
		case 0x8F:
		case 0x90:
		case 0x9D:
		case 0xA0:
		case 0xAD:
			continue;
			break;
		}

		if (outpos > 0 && out[outpos - 1] == Q_COLOR_ESCAPE)
		{
			if (Q_IsColorStringExt(&out[outpos - 1]))
			{
				colorlessLen--;
			}
			else
			{
				//spaces = ats = 0;
				colorlessLen++;
			}
		}
		else
		{
			//spaces = ats = 0;
			colorlessLen++;
		}
		outpos++;
	}

	out[outpos] = '\0';

	// don't allow empty names
	if (*out == '\0' || colorlessLen == 0)
	{
		Q_strncpyz(out, "Padawan", outSize);
	}
}

// ==================================================
// q_shared
// ==================================================

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
#define	MAX_VA_STRING	32000
#define MAX_VA_BUFFERS	4

char *QDECL va(const char* format, ...)
{
	va_list		argptr;
	static char	string[MAX_VA_BUFFERS][MAX_VA_STRING];	// in case va is called by nested functions
	static size_t	index = 0;
	char* buf;

	va_start(argptr, format);
	buf = (char *) &string[index++ & 3];
	Q_vsnprintf(buf, sizeof(*string), format, argptr);
	va_end(argptr);

	return buf;
}

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
FIXME: overflow check?
===============
*/
char* Info_ValueForKey(const char* s, const char* key) {
	char	pkey[BIG_INFO_KEY];
	static	char value[2][BIG_INFO_VALUE];	// use two buffers so compares
											// work without stomping on each other
	static	size_t	valueindex = 0;

	if (!s || !key) {
		return "";
	}

	if (strlen(s) >= BIG_INFO_STRING) {
		Com_Error(ERR_DROP, "Info_ValueForKey: oversize infostring");
	}

	valueindex ^= 1;

	if (*s == '\\')
	{
		s++;
	}

	while (1)
	{
		char* o;

		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
			{
				return "";
			}
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			*o++ = *s++;
		}
		*o = 0;

		if (!Q_stricmp(key, pkey))
		{
			return value[valueindex];
		}

		if (!*s)
		{
			break;
		}
		s++;
	}

	return "";
}

/*
===================
Info_RemoveKey
===================
*/
void Info_RemoveKey(char* s, const char* key) {
	char* start = NULL;
	char* o = NULL;
	char	pkey[MAX_INFO_KEY] = { 0 };
	char	value[MAX_INFO_VALUE] = { 0 };

	if (strlen(s) >= MAX_INFO_STRING) {
		Com_Error(ERR_DROP, "Info_RemoveKey: oversize infostring");
		return;
	}

	if (strchr(key, '\\')) {
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
		{
			s++;
		}
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
			{
				return;
			}
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
			{
				return;
			}
			*o++ = *s++;
		}
		*o = 0;

		//OJKNOTE: static analysis pointed out pkey may not be null-terminated
		if (!strcmp(key, pkey))
		{
			memmove(start, s, strlen(s) + 1);	// remove this part
			{
				return;
			}
		}

		if (!*s)
		{
			return;
		}
	}
}

/*
==================
Info_SetValueForKey

Changes or adds a key/value pair
==================
*/
void Info_SetValueForKey(char* s, const char* key, const char* value) {
	char	newi[MAX_INFO_STRING];
	const char* blacklist = "\\;\"";

	if (strlen(s) >= MAX_INFO_STRING) {
		Com_Error(ERR_DROP, "Info_SetValueForKey: oversize infostring");
	}

	for (; *blacklist; ++blacklist)
	{
		if (strchr(key, *blacklist) || strchr(value, *blacklist))
		{
			Com_Printf(S_COLOR_YELLOW "Can't use keys or values with a '%c': %s = %s\n", *blacklist, key, value);
			return;
		}
	}

	Info_RemoveKey(s, key);
	if (!value || !strlen(value))
	{
		return;
	}

	Com_sprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) >= MAX_INFO_STRING)
	{
		Com_Printf("Info string length exceeded: %s\n", s);
		return;
	}

	strcat(newi, s);
	strcpy(s, newi);
}

int QDECL Com_sprintf(char* dest, int size, const char* fmt, ...) {
	int		len;
	va_list		argptr;

	va_start(argptr, fmt);
	len = Q_vsnprintf(dest, size, fmt, argptr);
	va_end(argptr);

	if (len >= size)
	{
		Com_Printf("Com_sprintf: Output length %d too short, require %d bytes.\n", size, len + 1);
	}

	return len;
}

// ==================================================
// q_string
// ==================================================

#if defined(_MSC_VER)
/*
=============
Q_vsnprintf

Special wrapper function for Microsoft's broken _vsnprintf() function.
MinGW comes with its own snprintf() which is not broken.
=============
*/
int Q_vsnprintf(char* str, size_t size, const char* format, va_list args)
{
	int retval;

	retval = _vsnprintf(str, size, format, args);

	if (retval < 0 || retval == size)
	{
		// Microsoft doesn't adhere to the C99 standard of vsnprintf,
		// which states that the return value must be the number of
		// bytes written if the output string had sufficient length.
		//
		// Obviously we cannot determine that value from Microsoft's
		// implementation, so we have no choice but to return size.

		str[size - 1] = '\0';
		return size;
	}

	return retval;
}
#endif

int Q_stricmpn(const char* s1, const char* s2, int n)
{
	int		c1;

	if (s1 == NULL)
	{
		if (s2 == NULL)
		{
			return 0;
		}

		return -1;
	}
	else if (s2 == NULL)
	{
		return 1;
	}

	do
	{
		c1 = *s1++;
		int c2 = *s2++;

		if (!n--)
		{
			return 0;		// strings are equal until end point
		}

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
			{
				c1 -= ('a' - 'A');
			}

			if (c2 >= 'a' && c2 <= 'z')
			{
				c2 -= ('a' - 'A');
			}

			if (c1 != c2) {
				return c1 < c2 ? -1 : 1;
			}
		}
	} while (c1);

	return 0;		// strings are equal
}

int Q_stricmp(const char* s1, const char* s2) {
	return (s1 && s2) ? Q_stricmpn(s1, s2, 99999) : -1;
}

int Q_strncmp(const char* s1, const char* s2, int n) {
	int		c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (!n--) {
			return 0;		// strings are equal until end point
		}

		if (c1 != c2) {
			return c1 < c2 ? -1 : 1;
		}
	} while (c1);

	return 0;		// strings are equal
}

/*
Q_strchrs

Description:	Find any characters in a string. Think of it as a shorthand strchr loop.
Mutates:		--
Return:			first instance of any characters found
otherwise NULL
*/

const char* Q_strchrs(const char* string, const char* search)
{
	const char* p = string;
	const char *s;

	while (*p != '\0')
	{
		for (s = search; *s; s++)
		{
			if (*p == *s)
			{
				return p;
			}
		}

		p++;
	}

	return NULL;
}

/*
=============
Q_strncpyz

Safe strncpy that ensures a trailing zero
=============
*/
void Q_strncpyz(char* dest, const char* src, size_t destsize) {
	assert(src);
	assert(dest);
	assert(destsize);

	strncpy(dest, src, destsize - 1);
	dest[destsize - 1] = 0;
}

// never goes past bounds or leaves without a terminating 0
void Q_strcat(char* dest, int size, const char* src)
{
	int		l1;

	l1 = strlen(dest);
	if (l1 >= size)
	{
		//Com_Error( ERR_FATAL, "Q_strcat: already overflowed" );
		return;
	}
	if (strlen(src) + 1 > (size_t)(size - l1))
	{	//do the error here instead of in Q_strncpyz to get a meaningful msg
		//Com_Error(ERR_FATAL,"Q_strcat: cannot append \"%s\" to \"%s\"", src, dest);
		return;
	}
	Q_strncpyz(dest + l1, src, size - l1);
}

char* Q_CleanStr(char* string)
{
	char* d;
	char* s;
	int		c;

	s = string;
	d = string;
	while ((c = *s) != 0)
	{
		if (Q_IsColorString(s))
		{
			s++;
		}
		else if (c >= 0x20 && c <= 0x7E)
		{
			*d++ = c;
		}
		s++;
	}
	*d = '\0';

	return string;
}

char* Q_CleanAsciiStr(char* string) {
	char* d;
	char* s;
	int	c;

	s = string;
	d = string;
	while ((c = *s) != 0) {
		if (c >= 0x20 && c <= 0x7E) {
			*d++ = c;
		}
		s++;
	}
	*d = '\0';

	return string;
}

qboolean Q_IsValidAsciiStr(char* string) {
	char* s;
	int	c;

	s = string;

	while ((c = *s) != 0) {
		if (c < 0x20 || c > 0x7E) {
			return qfalse;
		}
		s++;
	}

	return qtrue;
}

qboolean Q_isintegral(float f)
{
	return (qboolean)((int)f == f);
}

/*
==================
Q_StripColor

Strips coloured strings in-place using multiple passes: "fgs^^56fds" -> "fgs^6fds" -> "fgsfds"

This function modifies INPUT (is mutable)

(Also strips ^8 and ^9)
==================
*/
void Q_StripColor(char* text)
{
	qboolean doPass = qtrue;
	char* read;
	char* write;

	while (doPass)
	{
		doPass = qfalse;
		read = write = text;
		while (*read)
		{
			if (Q_IsColorStringExt(read))
			{
				doPass = qtrue;
				read += 2;
			}
			else
			{
				// Avoid writing the same data over itself
				if (write != read)
				{
					*write = *read;
				}
				write++;
				read++;
			}
		}
		if (write < read)
		{
			// Add trailing NUL byte if string has shortened
			*write = '\0';
		}
	}
}

// ==================================================
// other
// ==================================================

char* ConcatArgs(int start) {
	int		i;
	int		c;
	static char	line[MAX_STRING_CHARS];
	size_t	len;
	char	arg[MAX_STRING_CHARS];

	len = 0;
	c = proxy.trap->Argc();

	for (i = start; i < c; i++)
	{
		proxy.trap->Argv(i, arg, sizeof(arg));
		size_t tlen = strlen(arg);

		if (len + tlen >= MAX_STRING_CHARS - 1)
		{
			break;
		}

		memcpy(line + len, arg, tlen);
		len += tlen;

		if (i != c - 1)
		{
			line[len] = ' ';
			len++;
		}
	}

	line[len] = 0;

	return line;
}

// ==================================================
// cvar
// ==================================================

/*
============
Cvar_SetValue
============
*/
cvar_t* Cvar_SetValue(const char* var_name, float value)
{
	char	val[32];

	if (Q_isintegral(value))
		Com_sprintf(val, sizeof(val), "%i", (int)value);
	else
		Com_sprintf(val, sizeof(val), "%f", value);

	proxy.trap->Cvar_Set(var_name, val);

	return nullptr;
}

// ==================================================
// files
// ==================================================

/*
================
FS_CheckDirTraversal

Check whether the string contains stuff like "../" to prevent directory traversal bugs
and return qtrue if it does.
================
*/

qboolean FS_CheckDirTraversal(const char* checkdir)
{
	if (strstr(checkdir, "../") || strstr(checkdir, "..\\"))
		return qtrue;

	return qfalse;
}

/*
===========
FS_FilenameCompare

Ignore case and separator char distinctions
===========
*/
qboolean FS_FilenameCompare(const char* s1, const char* s2) {
	int		c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if (c1 >= 'a' && c1 <= 'z') {
			c1 -= ('a' - 'A');
		}
		if (c2 >= 'a' && c2 <= 'z') {
			c2 -= ('a' - 'A');
		}

		if (c1 == '\\' || c1 == ':') {
			c1 = '/';
		}
		if (c2 == '\\' || c2 == ':') {
			c2 = '/';
		}

		if (c1 != c2) {
			return qtrue;		// strings not equal
		}
	} while (c1);

	return qfalse;		// strings are equal
}

/*
================
FS_idPak
================
*/
qboolean FS_idPak(char* pak, char* base) {
	int i;

	for (i = 0; i < NUM_ID_PAKS; i++) {
		if (!FS_FilenameCompare(pak, va("%s/assets%d", base, i))) {
			break;
		}
	}
	if (i < NUM_ID_PAKS) {
		return qtrue;
	}
	return qfalse;
}