#include <u.h>
#include <libc.h>
#include <bio.h>
#include "json.h"

static void freemap(Jmap *m);
static void freearray(Jarray *a);


static void
freevalue(Jvalue *v)
{
	if(v == nil)
		return;

	switch(v->type){
	case Tnull:
	case Ttrue:
	case Tfalse:
	case Tempty:
		break;
	case Tnumber:
		break;
	case Tstring:
		free(v->str);
		v->str = nil;
		break;
	case Tmap:
		freemap(v->map);
		v->map = nil;
		break;
	case Tarray:
		freearray(v->array);
		v->array = nil;
		break;
	default:
		sysfatal("json_free: %d unknown value type", v->type);
		break;
	}
	free(v);
}

static void
freearray(Jarray *a)
{
	int i;

	if(a){
		for(i = 0; i < a->used; i++){
			freevalue(a->base[i]);
			a->base[i] = nil;
		}
		free(a->base);
		a->base = nil;
		free(a);
	}
}

static void
freemap(Jmap *map)
{
	Jmap *m, *t;

	for(m = map; m; m = t){
		t = m->next;
		freevalue(m->value);
		m->value = nil;
		free(m->name);
		m->name = nil;
		free(m);
	}
}

void
json_free(Jvalue *v)
{
	switch(v->type){
	case Tmap:
		freemap(v->map);
		v->map = nil;
		break;
	case Tarray:
		freearray(v->array);
		v->array = nil;
		break;
	case Tempty:
		break;
	default:
		sysfatal("json_free: %d unknown object type\n", v->type);
		break;
	}
	free(v);
}
