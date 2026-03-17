
enum {
	Tstring,
	Tnumber,
	Ttrue,
	Tfalse,
	Tnull,
	Tmap,
	Tarray,
	Tempty		/* nothing parsed, but no errors, empty stream */
};

typedef struct Jmap Jmap;
typedef struct Jarray Jarray;
typedef struct Jvalue Jvalue;

struct Jarray {
	Jvalue **base;
	int alloc;
	int used;
};

struct Jmap {
	char *name;
	Jvalue *value;
	Jmap *next;
};

struct Jvalue {
	int type;
	union {
		char *str;
		double num;
		int bool;
		Jarray *array;
		Jmap *map;
	};
};


/* json_free.c */
void json_free(Jvalue *v);

/* json_map.c */
Jmap *Jmapfirst(Jvalue *v, char *fmt, ...);
char *Jmapstr(Jmap *m);
Jmap *Jmapnext(Jmap *m);

/* json_parse.c */
Jvalue *json_parse(int, char *file);
Jvalue *json_Bparse(Biobuf *bp, char *file);

/* json_print.c */
void json_print(int, Jvalue *v);
void json_Bprint(Biobuf *bp, Jvalue *v);

/* json_strval.c */
char *json_strval(Jvalue *v, char delim, char *fmt, ...);

#pragma lib "libjson.a"
