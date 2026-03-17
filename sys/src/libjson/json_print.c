#include <u.h>
#include <libc.h>
#include <bio.h>
#include "json.h"

enum { Indent = 4 };

static void printmap(Biobuf *bp, int indent, Jmap *m);
static void printarray(Biobuf *bp, int indent, Jarray *a);

static void
printstr(Biobuf *bp, char *s)
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
printvalue(Biobuf *bp, int indent, Jvalue *v)
{
	if(v == nil){
		Bprint(bp, "### value==nil ###");
		return;
	}

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
		printstr(bp, v->str);
		break;
	case Tmap:
		printmap(bp, indent, v->map);
		break;
	case Tarray:
		printarray(bp, indent, v->array);
		break;
	case Tempty:
		break;
	default:
		sysfatal("printvalue: %d unknown value type", v->type);
		break;
	}
}

static void
printarray(Biobuf *bp, int indent, Jarray *a)
{
	int i;

	Bprint(bp, "[");
	for(i = 0; a && i < a->used; i++){
		if(i == 0)
			Bprint(bp, "\n%*.s", (indent+1)*Indent, "");
		else
			Bprint(bp, ",\n%*.s", (indent+1)*Indent, "");
		printvalue(bp, indent, a->base[i]);
	}
	Bprint(bp, "\n%*.s]", indent*Indent, "");
}

static void
printmap(Biobuf *bp, int indent, Jmap *map)
{
	Jmap *m;

	Bprint(bp, "{");
	for(m = map; m; m = m->next){
		if(m == map)
			Bprint(bp, "\n%*.s\"%s\" : ", (indent+1)*Indent, "", m->name);
		else
			Bprint(bp, ",\n%*.s\"%s\" : ", (indent+1)*Indent, "", m->name);
		printvalue(bp, indent+1, m->value);
	}
	Bprint(bp, "\n%*.s}", indent*Indent, "");
}

void
json_Bprint(Biobuf *bp, Jvalue *v)
{
	switch(v->type){
	case Tmap:
		printmap(bp, 0, v->map);
		Bprint(bp, "\n");
		break;
	case Tarray:
		printarray(bp, 0, v->array);
		Bprint(bp, "\n");
		break;
	default:
		sysfatal("json_print: %d unknown object type\n", v->type);
		break;
	}
}

void
json_print(int fd, Jvalue *v)
{
	Biobuf out;

	Binit(&out, fd, OWRITE);
	json_Bprint(&out, v);
	Bterm(&out);
}

