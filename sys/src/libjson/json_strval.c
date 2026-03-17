#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include "json.h"

static char *valarray(Jarray *a, char delim, char *path);
static char *valmap(Jmap *map, char delim, char *path);

static char *
value(Jvalue *v, char delim, char *path)
{
	static char buf[16];

	switch(v->type){
	case Tnull:
		return nil;
	case Ttrue:
		return "true";
	case Tfalse:
		return "false";
	case Tnumber:
		if(floor(v->num) == v->num)
			snprint(buf, sizeof(buf), "%.0f", v->num);
		else
			snprint(buf, sizeof(buf), "%g", v->num);
		return buf;
	case Tstring:
		return v->str;
	case Tmap:
		return valmap(v->map, delim, path);
	case Tarray:
		return valarray(v->array, delim, path);
	default:
		break;
	}
	abort();
	return nil;
}

static char *
pathnext(char *s, char *t, char delim, int isarray)
{
	while(*t && *s == *t){
		s++;
		t++;
	}
	
	if(*t)
		return nil;		/* no match */
	if(!isarray && *s == 0)
		return s;		/* match last map name of path */
	if(isarray && *s == '[')
		return s;		/* match an array */
	if(! isarray && *s == delim)
		return s+1;		/* match map name */
	return nil;			/* partial match */
}

static char *
valarray(Jarray *a, char delim, char *path)
{
	int i, idx;
	char *p;

	for(p = path+1; *p && *p != delim && *p != '['; p++)
		continue;
	if(*p == delim)
		p++;

	if(*path != '['){
		fprint(2, "valarray: expected '[' found '%.*s'", (int)(p-path), path);
		return nil;
	}
	idx = strtol(path+1, &path, 0);
	if(*path != ']'){
		fprint(2, "valarray: expected ']' found '%.*s'", (int)(p-path), path);
		return nil;
	}

	for(i = 0; a && i < a->used; i++)
		if(i == idx)
			return value(a->base[i], delim, p);
	return nil;
}

static char *
valmap(Jmap *map, char delim, char *path)
{
	Jmap *m;
	char *p;
	int isarray;

	for(m = map; m; m = m->next){
		isarray = 0;
		if(m->value && m->value->type == Tarray)
			isarray = 1;

		if((p = pathnext(path, m->name, delim, isarray)) != nil)
			return value(m->value, delim, p);
	}
	return nil;
}

char *
json_strval(Jvalue *v, char delim, char *fmt, ...)
{
	va_list ap;
	char *rc, *path;

	va_start(ap, fmt);
	path = vsmprint(fmt, ap);
	va_end(ap);

	rc = nil;
	switch(v->type){
	case Tmap:
		rc = valmap(v->map, delim, path);
		break;
	case Tarray:
		rc = valarray(v->array, delim, path);
		break;
	default:
		break;
	}

	free(path);
	return rc;
}

