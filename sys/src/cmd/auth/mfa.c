#include <u.h>
#include <libc.h>
#include <auth.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include <bio.h>

void
usage(void)
{
	fprint(2, "usage: %s [-m mtpt] [-w code] [fmt]\n", argv0);
	threadexits("usage");
}

static int
dorpc(AuthRpc *rpc, char *verb, char *val)
{
	int ret, len;

	len = 0;
	if(val != nil)
		len = strlen(val);
	for(;;){
		if((ret = auth_rpc(rpc, verb, val, len)) != ARneedkey && ret != ARbadkey)
			return ret;
		if(auth_getkey(rpc->arg) < 0)
			return -1;
	}
}

char *
getcode(AuthRpc *rpc, char *params)
{
	if(dorpc(rpc, "start", params) != ARok)
		sysfatal("auth_rpc start: %r");
	if(dorpc(rpc, "read", nil) != ARok)
		sysfatal("auth_rpc: %r");
	rpc->arg[rpc->narg] = '\0';
	return rpc->arg;
}

int
verifycode(AuthRpc *rpc, char *params, char *code)
{
	int ret;

	if(dorpc(rpc, "start", params) != ARok)
		sysfatal("auth_rpc start: %r");
	ret = dorpc(rpc, "write", code);
	if(ret < 0)
		sysfatal("auth_rpc: write %s: %r", code);
	return ret == ARok;
}

typedef struct Params Params;
struct Params
{
	char *proto;
	char *user;
	char *dom;
};

void
parseparams(char *s, Params *p)
{
	char *a[10];
	int n, i;

	n = tokenize(s, a, nelem(a));
	for(i = 0; i < n; i++)
		if(strncmp(a[i], "proto=", 6) == 0)
			p->proto = a[i]+6;
		else if(strncmp(a[i], "user=", 5) == 0)
			p->user = a[i]+5;
		else if(strncmp(a[i], "dom=", 4) == 0)
			p->dom = a[i]+4;
}

int
aopen(char *mtpt, char *name, int omode)
{
	char buf[512];

	snprint(buf, sizeof buf, "%s/factotum/%s", mtpt, name);
	return open(buf, omode);
}

void
list(char *mtpt)
{
	int fd, afd;
	char *s, *params;
	Biobuf b;
	AuthRpc *rpc;
	Params p;

	fd = aopen(mtpt, "ctl", OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	afd = aopen(mtpt, "rpc", ORDWR);
	if(afd < 0)
		sysfatal("open: %r");
	Binit(&b, fd, OREAD);
	while((s=Brdline(&b, '\n')) != nil){
		s[Blinelen(&b)-1] = '\0';
		if(strncmp(s, "key ", 4) != 0)
			continue;
		memset(&p, 0, sizeof p);
		parseparams(s, &p);
		if(strcmp(p.proto, "totp") != 0)
			continue;
		if(p.user == nil || p.dom == nil)
			continue;
		rpc = auth_allocrpc(afd);
		if(rpc == nil)
			sysfatal("auth_allocrpc: %r");
		params = smprint("proto=totp role=client user=%s dom=%s", p.user, p.dom);
		if(params == nil)
			sysfatal("smprint: %r");
		print("%s %s %s\n", p.user, p.dom, getcode(rpc, params));
		free(params);
		auth_freerpc(rpc);
	}
	close(afd);
	close(fd);
}

void
threadmain(int argc, char **argv)
{
	int afd;
	AuthRpc *rpc;
	char *mtpt, *params, *role, *code;

	mtpt = "/mnt";
	code = nil;
	role = "client";
	ARGBEGIN{
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 'w':
		code = EARGF(usage());
		role = "server";
		break;
	default:
		usage();
	}ARGEND

	quotefmtinstall();
	if(argc == 0 && code == nil){
		list(mtpt);
		threadexits(nil);
	}

	if(argc != 1)
		usage();

	afd = aopen(mtpt, "rpc", ORDWR);
	if(afd < 0)
		sysfatal("open: %r");
	rpc = auth_allocrpc(afd);
	if(rpc == nil)
		sysfatal("auth_allocrpc: %r");
	params = smprint("proto=totp role=%s %s", role, argv[0]);
	if(params == nil)
		sysfatal("smprint: %r");

	if(code == nil)
		print("%s\n", getcode(rpc, params));
	else{
		if(!verifycode(rpc, params, code)){
			fprint(2, "%s: %s: %r\n", argv0, params);
			threadexits("verify");
		}
	}
	free(params);
	auth_freerpc(rpc);
	close(afd);
	threadexits(nil);
}
