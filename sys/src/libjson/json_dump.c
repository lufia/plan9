#include <u.h>
#include <libc.h>
#include <bio.h>
#include "json.h"

enum { Indent = 4 };

static void dumpmap(Biobuf *bp, int indent, Jmap *m);
static void dumparray(Biobuf *bp, int indent, Jarray *a);

void
dumpstr(Biobuf *bp, char *s)
{
	Rune r;
	char *p;

	Bprint(bp, "\"");
	for(p = s; *p;){
		p += chartorune(&r, p);

		switch(r){
		case Runeerror:
			sysfatal("%s contains a bad rune\n", s);
		case L'\r': Bprint(bp, "\\r"); continue;
		case L'\n': Bprint(bp, "\\n"); continue;
		case L'\b': Bprint(bp, "\\b"); continue;
		case L'\f': Bprint(bp, "\\f"); continue;
		case L'\t': Bprint(bp, "\\t"); continue;
		case L'\\': Bprint(bp, "\\\\"); continue;
		case L'/':  Bprint(bp, "\\/"); continue;	/* some browsers see // as a comment */
		case L'"':  Bprint(bp, "\\\""); continue;

		/* These are not in the spec but cause some parsers grief */
		case L'<':  Bprint(bp, "\\u003C"); ; break;
		case L'>':  Bprint(bp, "\\u003E");  break;
		case L'&':  Bprint(bp, "\\u0026");  break;
		}

		if(r < ' ' || r > '~')
			Bprint(bp, "\\u%04x", r);
		else
			Bprint(bp, "%C", r);
	}
	Bprint(bp, "\"");
}



static void
dumpvalue(Biobuf *bp, int indent, Jvalue *v)
{
	switch(v->type){
	case Tnull:
		Bprint(bp, "null");
		break;
	case Ttrue:
		Bprint(bp, "true");
		break;
	case Tfalse:
		Bprint(bp, "false");
		break;
	case Tnumber:
		if(floor(v->num) == v->num)
			Bprint(bp, "%.0f", v->num);
		else
			Bprint(bp, "%g", v->num);
		break;
	case Tstring:
		dumpstr(bp, v->str);
		break;
	case Tmap:
		dumpmap(bp, indent, v->map);
		break;
	case Tarray:
		dumparray(bp, indent, v->array);
		break;
	default:
		sysfatal("dumpvalue: %d unknown value type", v->type);
		break;
	}
}

static void
dumparray(Biobuf *bp, int indent, Jarray *a)
{
	int i;

	Bprint(bp, "[");
	for(i = 0; a && i < a->used; i++){
		if(i == 0)
			Bprint(bp, "\n%*.s", (indent+1)*Indent, "");
		else
			Bprint(bp, ",\n%*.s", (indent+1)*Indent, "");
		dumpvalue(bp, indent, a->base[i]);
	}
	Bprint(bp, "\n%*.s]", indent*Indent, "");
}

static void
dumpmap(Biobuf *bp, int indent, Jmap *map)
{
	Jmap *m;

	Bprint(bp, "{");
	for(m = map; m; m = m->next){
		if(m == map)
			Bprint(bp, "\n%*.s\"%s\" : ", (indent+1)*Indent, "", m->name);
		else
			Bprint(bp, ",\n%*.s\"%s\" : ", (indent+1)*Indent, "", m->name);
		dumpvalue(bp, indent+1, m->value);
	}
	Bprint(bp, "\n%*.s}", indent*Indent, "");
}

void
json_dump(int fd, Jvalue *v)
{
	Biobuf out;

	Binit(&out, fd, OWRITE);

	switch(v->type){
	case Tmap:
		dumpmap(&out, 0, v->map);
		Bprint(&out, "\n");
		break;
	case Tarray:
		dumparray(&out, 0, v->array);
		Bprint(&out, "\n");
		break;
	default:
		sysfatal("json_dump: %d unknown object type\n", v->type);
		break;
	}
	Bterm(&out);
}
