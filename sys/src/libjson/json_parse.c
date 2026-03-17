/* ©  Copyright Steve Simon, 2013 */
/*
 * JSON parser - see http://www.json.org
 */
#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include "json.h"

#pragma varargck type "T" int

typedef struct State State;
struct State {
	Biobuf *bp;

	int line;		/* for error messages */
	char *file;
	int errs;		/* number of errors found */
	char *genbuf;	/* lookaside buffer for parse */
	int bufsiz;
	char *str;
	double num;
};

static Jmap *map(State *s, int tok);
static Jarray *array(State *s, int tok);

static int
isxdigitrune(Rune r)
{
	if(isdigitrune(r))
		return 1;
	if((r >= 'A' && r <= 'F') || (r >= 'a' && r <= 'f'))
		return 1;
	return 0;
}

static int
tokfmt(Fmt *f)
{
	int tok;
	static char *names[] = {
		"string", "number", "true", "false", "null", "map", "array"
	};

	tok = va_arg(f->args, int);
	if(tok < 0 || tok >= sizeof(names)){
		if(isprint(tok))
			return fmtprint(f,"'%c'", tok);
		else
		if(tok == -1)
			return fmtprint(f,"<unexpected EOF>");
		else
			return fmtprint(f,"id=%d", tok);
	}
	return fmtprint(f,"%s", names[tok]);
	
}

static void
syntax(State *s, char *fmt, ...)
{
	va_list ap;

	s->errs++;
	fprint(2, "%s %s:%d ", argv0, s->file, s->line);

	va_start(ap, fmt);
	vfprint(2, fmt, ap);
	va_end(ap);
}


static Rune
utf(State *s)
{
	Rune r;
	int i, n;
	char *p;

	p = s->genbuf;
	do{
		if((i = Bgetrune(s->bp)) < 0)
			return -1;

		r = i;
		if(! isxdigitrune(r))
			break;

		if(s->bufsiz - (p - s->genbuf) >= UTFmax){
			n = runetochar(p, &r);
			p += n;
		}
	}while((p - s->genbuf) < 4);
	*p = 0;

	if(p - s->genbuf != 4){
		syntax(s, "got %d hex digits expecting 4\n", p - s->genbuf);
		return L'?';
	}

	return strtol(s->genbuf, nil, 16);
}

/* NB: an empty string is still valid */
static int
string(State *s)
{
	int n, i;
	Rune qt, r;
	char *p;

	p = s->genbuf;
	qt = Bgetrune(s->bp);
	while(1){
		if((i = Bgetrune(s->bp)) < 0)
			return -1;
		r = i;

		if(r == '\n')
			s->line++;

		if(r == qt)		/* normal exit from loop */
			break;

		switch(r){
		case L'\\':
			if((i = Bgetrune(s->bp)) < 0)
				return -1;
			r = i;

			switch(r){
			case L'\\': r = L'\\'; break;
			case L'n': r = L'\n'; break;
			case L'r': r = L'\r'; break;
			case L'b': r = L'\b'; break;
			case L'f': r = L'\f'; break;
			case L't': r = L'\t'; break;
			case L'/': r = L'/'; break;
			case L'"': r = L'"'; break;
			case L'u':
				if((i = utf(s)) < 0)
					return -1;
				r = i;
				break;
			default:
				syntax(s, "\\%C - bad string escape char\n", r);
				r = L'?';
			}
		}
		if(s->bufsiz - (p - s->genbuf) >= UTFmax){
			n = runetochar(p, &r);
			p += n;
		}
	}
	*p = 0;

	if((s->str = strdup(s->genbuf)) == nil)
		sysfatal("string: no memory\n");
	return Tstring;
}

static int
number(State *s)
{
	double d;

	if(Bgetd(s->bp, &d) < 0)
		return -1;
	s->num = d;
	return Tnumber;
}

static int
keyword(State *s)
{
	Rune r;
	int i, n;
	char *p;

	p = s->genbuf;
	while(1){
		if((i = Bgetrune(s->bp)) < 0)
			return -1;
		r = i;

		if(! isalpharune(r))	/* normal exit from loop */
			break;

		if(s->bufsiz - (p - s->genbuf) >= UTFmax){
			n = runetochar(p, &r);
			p += n;
		}
	}
	Bungetrune(s->bp);
	*p = 0;

	if(strcmp(s->genbuf, "true") == 0)
		return Ttrue;
	if(strcmp(s->genbuf, "false") == 0)
		return Tfalse;
	if(strcmp(s->genbuf, "null") == 0)
		return Tnull;
	syntax(s, "'%s' - unknown keyword\n", s->genbuf);
	return Tnull;
}
		
/*
 * By the spec, number must start with '-' or a dight,
 * but we allow '+' also just in case
 */
static int
lex(State *s)
{
	Rune r;
	int i;
	
	do{
		if((i = Bgetrune(s->bp)) < 0)
			return -1;
		r = i;
		if(r == '\n')
			s->line++;
	}while(isspacerune(r));

	switch(r){
	case -1:
		return -1;
	case L']':
		return ']';
	case L'[':
		return Tarray;
	case L'}':
		return '}';
	case L'{':
		return Tmap;
	case L':':
		return ':';
	case L',':
		return ',';
	case L'"':
		Bungetrune(s->bp);
		return string(s);
	case L'-': case L'+':
	case L'0': case L'1': case L'2': case L'3': case L'4': 
	case L'5': case L'6': case L'7': case L'8': case L'9': 
		Bungetrune(s->bp);
		return number(s);
	default:
		Bungetrune(s->bp);
		return keyword(s);
	}
}


static Jvalue *
value(State *s, int tok)
{
	Jvalue *v;

	if((v = mallocz(sizeof(Jvalue), 1)) == nil)
		sysfatal("No memory\n");
	v->type = tok;

	switch(tok){
	case Tmap:
		v->map = map(s, lex(s));
		break;
	case Tarray:
		v->array = array(s, lex(s));
		break;
	case Tstring:
		v->str = s->str;
		s->str = nil;
		break;
	case Tnumber:
		v->num = s->num;
		break;
	case Ttrue:
		break;
	case Tfalse:
		break;
	case Tnull:
		break;
	case -1:			/* EOF */
		v->type = Tempty;
		return v;
	default:
		syntax(s, "got %T expected 'value'\n", tok);
		break;
	}
	return v;
}

static Jmap *
maplist(State *s, int tok)
{
	Jmap *m;

	if(tok != Tstring){
		syntax(s, "got %T expected 'mapname'\n", tok);
		return nil;
	}

	if((m = mallocz(sizeof(Jmap), 1)) == nil)
		sysfatal("No memory\n");
	m->name = s->str;
	s->str = nil;

	if((tok = lex(s)) != ':')
		syntax(s, "got %T expected ':'\n", tok);

	if((m->value = value(s, lex(s))) == nil)
		syntax(s, "json_parse: unexpected EOF in map\n");
	m->next = nil;

	tok = lex(s);
	switch(tok){
	case '}':
		break;
	case ',':
		m->next = maplist(s, lex(s));
		break;
	case -1:
		break;
	default:
		syntax(s, "got %T expected '}' or ','\n", tok);
		break;
	}
	return m;
}

static Jarray *
arraylist(State *s, int tok, Jarray *a)
{
	if(a == nil){
		if((a = mallocz(sizeof(Jarray), 1)) == nil)
			sysfatal("No memory\n");
	
		a->used = 0;
		a->alloc = 1;
		if((a->base = mallocz(sizeof(Jvalue) * a->alloc, 1)) == nil)
			sysfatal("No memory\n");
	}
	else
	if(a->used >= a->alloc){
		a->alloc *= 2;
		if((a->base = realloc(a->base, sizeof(Jvalue) * a->alloc)) == nil)
			sysfatal("No memory\n");
	}
		
	if((a->base[a->used++] = value(s, tok)) == nil)
		syntax(s, "json_parse: unexpected EOF in array\n");

	tok = lex(s);
	switch(tok){
	case ']':
		break;
	case ',':
		return arraylist(s, lex(s), a);
	case -1:
		break;
	default:
		syntax(s, "got %T expected ']' or ','\n", tok);
		break;
	}
	return a;
}

static Jmap *
map(State *s, int tok)
{
	if(tok == '}')
		return nil;
	return maplist(s, tok);
}

static Jarray *
array(State *s, int tok)
{
	if(tok == ']')
		return nil;
	return arraylist(s, tok, nil);
}

static Jvalue *
object(State *s, int tok)
{
	Jvalue *v;

	if((v = mallocz(sizeof(Jvalue), 1)) == nil)
		sysfatal("No memory\n");
	if(tok == -1){
		v->type = Tempty;
		return v;
	}

	v->type = tok;

	switch(tok){
	case Tmap:
		v->map = map(s, lex(s));
		break;
	case Tarray:
		v->array = array(s, lex(s));
		break;
	default:
		syntax(s, "got %T expected '[' or '{'\n", tok);
		break;
	}
	return v;
}

Jvalue *
json_Bparse(Biobuf *bp, char *file)
{
	State st;
	Jvalue *v;

	st.bp = bp;
	st.file = file;
	st.line = 1;
	st.bufsiz = 8192;
	st.str = nil;

	fmtinstall('T', tokfmt);
	st.genbuf = mallocz(st.bufsiz, 1);
	v = object(&st, lex(&st));
	free(st.genbuf);
	free(st.str);
	return v; 
}

Jvalue *
json_parse(int fd, char *file)
{
	void *v;
	Biobuf inp;

	Binit(&inp, fd, OREAD);
	v = json_Bparse(&inp, file);
	Bterm(&inp);
	return v; 
}
