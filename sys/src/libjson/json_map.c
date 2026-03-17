#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include "json.h"

static Jmap *valarray(Jarray *a, char *path);
static Jmap *valmap(Jmap *map, char *path);

static Jmap *
value(Jvalue *v, char *path)
{
	static char buf[16];

	switch(v->type){
	case Tnull:
		return nil;
	case Ttrue:
		return nil;
	case Tfalse:
		return nil;
	case Tnumber:
		return nil;
	case Tstring:
		return nil;
	case Tmap:
		return valmap(v->map, path);
	case Tarray:
		return valarray(v->array, path);
	default:
		break;
	}
	abort();
	return nil;
}

static char *
pathnext(char *s, char *t, int isarray)
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
	if(! isarray && *s == '.')
		return s+1;		/* match map name */
	return nil;			/* partial match */
}

static Jmap *
valarray(Jarray *a, char *path)
{
	int i, idx;
	char *p;

	for(p = path+1; *p && *p != '.' && *p != '['; p++)
		continue;
	if(*p == '.')
		p++;

	if(*path != '[')
		sysfatal("valarray: %.*s - bad array index 1", (int)(p-path), path);
	idx = strtol(path+1, &path, 0);
	if(*path != ']')
		sysfatal("valarray: %.*s - bad array index 2", (int)(p-path), path);

	for(i = 0; a && i < a->used; i++){
		if(i == idx)
			return value(a->base[i], p);
	}
	return nil;
}

static Jmap *
valmap(Jmap *map, char *path)
{
	Jmap *m;
	char *p;
	int isarray;

	if(*path == 0)
		return map;

	for(m = map; m; m = m->next){
		isarray = 0;
		if(m->value && m->value->type == Tarray)
			isarray = 1;

		if((p = pathnext(path, m->name, isarray)) != nil){
			m = value(m->value, p);
			return m;
		}
	}
	return nil;
}

Jmap *
Jmapfirst(Jvalue *v, char *fmt, ...)
{
	Jmap *m;
	va_list ap;
	char path[1024];

	va_start(ap, fmt);
	vsnprint(path, sizeof(path), fmt, ap);
	va_end(ap);

	m = nil;
	switch(v->type){
	case Tmap:
		m = valmap(v->map, path);
		break;
	case Tarray:
		m = valarray(v->array, path);
		break;
	default:
		break;
	}

	return m;
}

char *
Jmapstr(Jmap *m)
{
	return m->name;
}


Jmap *
Jmapnext(Jmap *m)
{
	return m->next;
}
