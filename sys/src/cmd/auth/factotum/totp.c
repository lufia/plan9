/*
 * TOTP - Time-based One-time Password authentication
 *
 * Protocol:
 *
 *	C -> S:	code
 *	S -> C:	ok / bad [msg]
 */

#include "dat.h"

typedef struct State State;
struct State
{
	Key *key;
	uchar *data;
	int n;
};

enum
{
	CHaveResp,
	SNeedResp,
	Maxphase,
};

static char *phasenames[Maxphase] =
{
[CHaveResp]	"CHaveResp",
[SNeedResp]	"SNeedResp",
};

static int
base32d(uchar *dest, int ndest, char *src, int nsrc)
{
	char *s, *tab;
	uchar *start;
	int i, u[8];

	if((nsrc%8) != 0)
		return -1;
	if(s = memchr(src, '=', nsrc)){
		if((src+nsrc-s) >= 8)
			return -1;
		nsrc = s-src;
	}
	if(ndest+1 < (5*nsrc+7)/8)
		return -1;
	start = dest;
	tab = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
	while(nsrc>=8){
		for(i = 0; i < 8; i++){
			s = strchr(tab, toupper(src[i]));
			if(s == nil)
				return -1;
			u[i] = s-tab;
		}
		*dest++ = (u[0]<<3) | (0x7 & (u[1]>>2));
		*dest++ = ((0x3 & u[1])<<6) | (u[2]<<1) | (0x1 & (u[3]>>4));
		*dest++ = ((0xf & u[3])<<4) | (0xf & (u[4]>>1));
		*dest++ = ((0x1 & u[4])<<7) | (u[5]<<2) | (0x3 & (u[6]>>3));
		*dest++ = ((0x7 & u[6])<<5) | u[7];
		src  += 8;
		nsrc -= 8;
	}
	if(nsrc > 0){
		if(nsrc == 1 || nsrc == 3 || nsrc == 6)
			return -1;
		for(i=0; i<nsrc; i++){
			s = strchr(tab,(int)src[i]);
			if(s == nil)
				return -1;
			u[i] = s-tab;
		}
		*dest++ = (u[0]<<3) | (0x7 & (u[1]>>2));
		if(nsrc == 2)
			goto out;
		*dest++ = ((0x3 & u[1])<<6) | (u[2]<<1) | (0x1 & (u[3]>>4));
		if(nsrc == 4)
			goto out;
		*dest++ = ((0xf & u[3])<<4) | (0xf & (u[4]>>1));
		if(nsrc == 5)
			goto out;
		*dest++ = ((0x1 & u[4])<<7) | (u[5]<<2) | (0x3 & (u[6]>>3));
	}
out:
	return dest-start;
}

static int
genhotp(uchar *key, int n, uvlong c, int digits)
{
	uchar data[sizeof(uvlong)], digest[SHA1dlen];
	uchar *p, *bp, *ep;
	int i;
	u32int h, d;

	for(i = 0; i < sizeof data; i++)
		data[sizeof(data)-1-i] = (c >> (8*i)) & 0xff;
	hmac_sha1(data, sizeof data, key, n, digest, nil);
	bp = &digest[digest[sizeof(digest)-1] & 0xf];
	ep = bp+3;
	h = 0;
	for(p = ep; p >= bp; p--)
		h |= *p << (8*(ep-p));
	h &= 0x7fffffff;
	d = 1;
	for(i = 0; i < digits && i < 8; i++)
		d *= 10;
	return h % d;
}

#define Stepsize	30e9
enum {
	Maxdigits = 8,
};

static int
gentotp(uchar *key, int n, vlong t, int digits)
{
	return genhotp(key, n, t/Stepsize, digits);
}

static int
totpinit(Proto *p, Fsstate *fss)
{
	int iscli, ret, n, phase;
	char *secret;
	uchar key[512];
	Key *k;
	Keyinfo ki;
	State *s;

	if((iscli = isclient(_strfindattr(fss->attr, "role"))) < 0)
		return failure(fss, nil);

	fss->phasename = phasenames;
	fss->maxphase = Maxphase;

	if(iscli){
		phase = CHaveResp;
		ret = findkey(&k, mkkeyinfo(&ki, fss, nil), "%s", p->keyprompt);
	}else{
		phase = SNeedResp;
		ret = findkey(&k, mkkeyinfo(&ki, fss, nil), nil);
	}
	if(ret != RpcOk)
		return ret;
	secret = _strfindattr(k->privattr, "!secret");
	if(secret == nil){
		closekey(k);
		return failure(fss, "key has no secret");
	}
	n = base32d(key, sizeof key, secret, strlen(secret));
	if(n < 0){
		closekey(k);
		return failure(fss, "secret not base32 encoded");
	}
	setattrs(fss->attr, k->attr);
	s = emalloc(sizeof *s + n);
	s->key = k;
	s->data = (uchar*)(s+1);
	memmove(s->data, key, n);
	s->n = n;
	fss->ps = s;
	fss->phase = phase;
	return RpcOk;
}

static int
totpwrite(Fsstate *fss, void *va, uint n)
{
	char buf[Maxdigits+1];
	int i, code;
	vlong t;
	State *s;
	static vlong ms[] = { 0, -Stepsize, Stepsize };

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "write");		

	case SNeedResp:
		if(n >= sizeof(buf)-1)
			return failure(fss, "response too long");
		memmove(buf, va, n);
		buf[n] = '\0';
		code = atoi(buf);
		t = nsec();
		for(i = 0; i < nelem(ms); i++)
			if(gentotp(s->data, s->n, t+ms[i], n) == code){
				fss->phase = Established;
				return RpcOk;
			}
		return failure(fss, "bad response");
	}
}

static int
totpread(Fsstate *fss, void *va, uint *n)
{
	char buf[Maxdigits+1];
	int m, code, digits;
	State *s;

	s = fss->ps;
	switch(fss->phase){
	default:
		return phaseerror(fss, "read");

	case CHaveResp:
		digits = 6;
		code = gentotp(s->data, s->n, nsec(), digits);
		snprint(buf, sizeof buf, "%.*d", digits, code);
		m = strlen(buf);
		if(m > *n)
			return toosmall(fss, m);
		memmove(va, buf, m);
		*n = m;
		fss->phase = Established;
		return RpcOk;
	}
}

static void
totpclose(Fsstate *fss)
{
	State *s;

	s = fss->ps;
	if(s->key)
		closekey(s->key);
	free(s);
}

Proto totp =
{
.name=		"totp",
.init=			totpinit,
.write=		totpwrite,
.read=		totpread,
.close=		totpclose,
.addkey=		replacekey,
.keyprompt=	"user? dom? !secret?"
};
