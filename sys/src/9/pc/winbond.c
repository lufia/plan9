/*
 * winbond 82567hg &c hw monitors
 * copyright © 2008 erik quanstrom,
 * coraid, inc.
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "ureg.h"
#include "../port/error.h"

enum{
	Stemp	= 1<<0,
	Temp	= 1<<1,
	Fans	= 1<<2,
	Dog	= 1<<3,
	Volts	= 1<<4,
	Beep	= 1<<5,

	Maxfans	= 10,

	/* below 0x30 are available through any fn */
	Id	= 0x20,
	Rev	= 0x21,
	Wdog	= 0x08,
	Hwmon	= 0x0b,

	/* dog registers */
	Dscale	= 0xf5,
	Dogto	= 0xf6,
	Dogst	= 0xf7,		/* 0x10 force to, 0x08 timed out */

	/* hw monitor function registers */
	Hwstat	= 0x30,

	/* hw monitor io port */
	Index	= 0x2e,
	Data	= 0x2f,
	Cfg	= 0x40,
	Bank	= 0x4e,
	Vbat	= 0x5d,

	/* fans registers */
	Fandiv1	= 0x47,
	Pinctl	= 0x4b,
	Diode	= 0x59,

	Syst	= 0x27,
	Cputin	= 0x150,
	Cputoff	= 0x455,
	Beepctl	= 0x453,
};

static	Lock	wblock;
static	int	hwaddr;
static	uchar	cap;
static	uint	fandiv[Maxfans];
static	uint	vid;
static	uint	vidµv;

static uchar
wbread(int reg)
{
	outb(Index, reg);
	return inb(Data);
}

static ushort
wbreadw(int reg)
{
	return wbread(reg)<<8 | wbread(reg + 1);
}

static void
wbwrite(int r, ushort u)
{
	outb(Index, r);
	outb(Data, u);
}

static void
selfn(int n)
{
	lock(&wblock);
	outb(Index, 0x87);
	outb(Index, 0x87);
	wbwrite(7, n);
}

static void
release(void)
{
	unlock(&wblock);
}

static ushort
hwread0(int r)
{
	while(inb(hwaddr + 5) & 0x80)
		;
	outb(hwaddr + 5, r);
	return inb(hwaddr + 6);
}

static void
hwwrite0(int r, ushort v)
{
	while(inb(hwaddr + 5) & 0x80)
		;
	outb(hwaddr + 5, r);
	outb(hwaddr + 6, v);
}

static ushort
banksw(int r)
{
	int bank;

	bank = r>>8;
	r &= 0xff;
	outb(hwaddr + 5, Bank);
	outb(hwaddr + 6, 0x80 | bank);
	return r;
}

static ushort
hwread(int r)
{
	return hwread0(banksw(r));
}

static void
hwwrite(int r, ushort v)
{
	if(v & 0xff00){
		hwwrite0(banksw(r), v>>8);
		r++;
	}
	hwwrite0(banksw(r), v);
}

static ushort
hwreadw(int r)
{
	return hwread(r)<<8 | hwread(r + 1);
}

static long
wbtemp(Chan*, void *a, long n, vlong offset)
{
	char buf[64], *p, *e;
	short s, i;

	selfn(Hwmon);
	s = hwread(Syst);
	release();
	p = buf;
	e = p + sizeof buf;
	for(i = 0; i < conf.nmach; i++)
		p = seprint(p, e, "%d 1\n", s);
	return readstr(offset, a, n, buf);
}

int
fanspeed(int i, ushort v)
{
	if(v == 0)
		return fandiv[i]/76;
	if(v == 255)
		return 0;
	return fandiv[i]/v;
}

static int fantab[] = {0x28, 0x29, 0x2a, 0x3f, 0x553};
static long
wbfans(Chan*, void *a, long n, vlong offset)
{
	char buf[256], *p, *e;
	uint s[Maxfans], i;

	selfn(Hwmon);
	for(i = 0; i < nelem(fantab); i++)
		s[i] = fanspeed(i, hwread(fantab[i]));
	release();
	p = buf;
	e = p + sizeof buf;
	for(i = 0; i < nelem(fantab); i++)
		p = seprint(p, e, "%d\n", s[i]);
	return readstr(offset, a, n, buf);
}

static void
cfandiv(void)
{
	uint u, i;

	u = hwread(Fandiv1);
	fandiv[0] = (u & 0x30) >> 4;
	fandiv[1] = (u & 0xc0) >> 6;
	u = hwread(Pinctl);
	fandiv[2] = (u & 0xc0) >> 6;
	u = hwread(Vbat);
	for(i = 0; i < 3; i++){
		fandiv[i] |= (u >> i+3) & 4;
		fandiv[i] = 1<<fandiv[i];
	}
	for(i = 0; i < 3; i++)
		fandiv[i] = 1350000/fandiv[i];
	/* cheet */
	fandiv[3] = 1350000/2;
	fandiv[4] = 1350000/2;
}

static int dogto = 0;

static long
wbdogr(Chan*, void *a, long n, vlong offset)
{
	char buf[64];
	int t;

	t = 0;
	if(dogto != 0){
		selfn(Wdog);
		t = wbread(Dogto);
		if(wbread(Dscale) & 8)
			t *= 60;
		release();
	}
	snprint(buf, sizeof buf, "%d %d\n", dogto, t);
	return readstr(offset, a, n, buf);
}

static Cmdtab dogcmd[] = {
	0,	"start",	2,
	1,	"pat",	1,
	2,	"stop",	2,
};

static void
setdog(void)
{
	int t;
	static int once;

	selfn(Wdog);
	if(once == 0){
		wbwrite(0x30, 1);
		once = 1;
	}
	t = dogto;
	if(t > 180){
		wbwrite(Dscale, 8);
		t /= 60;
	}else
		wbwrite(Dscale, 0);
	wbwrite(Dogto, t);
	release();
}

static long
wbdogw(Chan*, void *a, long n, vlong)
{
	char c, *s;
	Cmdbuf *cb;
	Cmdtab *ct;

	cb = parsecmd(a, n);
	if(waserror()){
		free(cb);
		nexterror();
	}
	ct = lookupcmd(cb, dogcmd, nelem(dogcmd));
	switch(ct->index){
	case 0:
		dogto = strtoul(cb->f[1], &s, 0);
		while(c = *s++)switch(c){
		case 's':
			break;
		case 'm':
			dogto *= 60;
			break;
		default:
			error(Ebadarg);
		}
		if(dogto < 0 || dogto > 200*60)
			error(Ebadarg);
		break;
	case 2:
		dogto = 0;
		break;
	}
	poperror();
	free(cb);

	setdog();

	return n;
}

enum{
	V3,
	V12,
	V5,
	Vvsb,
	V5n,
	V12n,
	Vcorea,
	Vcoreb,
	Vb,
	Vmax,
	V18,
	V15,
};

static char *vname[] = {
[V3]	"+3.3",
[Vvsb]	"vsb",
[V5]	"+5",
[V12]	"+12",
[V5n]	"-5",
[V12n]	"-12",
[Vcorea]	"Vcorea",
[Vcoreb]	"Vcoreb",
[Vb]	"Vbat",
[V15]	"+1.5",
[V18]	"+1.8",
};

static int volt[Vmax][4] = {
	{V3, 	0x30, 0x22, 0x2f},
	{Vvsb,	0x555, 0x550, 0x554},
	{V5,	0x32, 0x23, 0x31},
	{V12,	0x34, 0x24, 0x33},
	{V5n,	0x38, 0x26, 0x37},
	{V12n,	0x36, 0x25, 0x35},
	{Vcorea,	0x2c, 0x20, 0x2b},
	{Vcoreb,	0x2e, 0x21, 0x2d},
	{Vb,	0x557, 0x551, 0x556},
};

static int smvolt[Vmax][4] = {
	{V12n, 	0x30, 0x22, 0x2f},			/* should be V3 */
	{Vvsb,	0x555, 0x550, 0x554},
	{V5,	0x32, 0x23, 0x31},
	{V12,	0x34, 0x24, 0x33},
	{V18,	0x38, 0x26, 0x37},			/* should be V5n */
	{V15,	0x36, 0x25, 0x35},			/* should be V12n */
	{Vcorea,	0x2c, 0x20, 0x2b},
	{Vcoreb,	0x2e, 0x21, 0x2d},
	{Vb,	0x557, 0x551, 0x556},
};

static int smpvolt[Vmax][4] = {
	{Vcorea,	0x20,	0x20,	0x20},
	{V12,	0x21,	0x21,	0x21},
	{V3,	0x22,	0x22,	0x22},
	{V18,	0x24,	0x24,	0x24},		/* dimma */
	{V5,	0x25,	0x25,	0x25},
	{V15,	0x26,	0x26,	0x26},
	{Vvsb,	0x550,	0x550,	0x550},
	{Vb,	0x551,	0x551,	0x551},
};

/*
 * the vid11 calculation is as follows
 * return 1600000 - (u - 2)*6250;
 */

static uint
map10(uint v)
{
	uint r;

	if(v == 0 || v == 0x29)
		return ~0;
	if((v & 0x7c) == 0x7c)
		return ~0;
	r = 1856250;
	if(v <= 0x28){
		/* the 6250µV bit is inverted */
		r += 25000;
		v += 0x80;
	}
	return r - (v/2)*12500 + (v&1)*6250;
}

static int
vidcvt(int v)
{
	int i, u;
	static int order[] = {4, 3, 2, 1, 0, 5, 6};

	u = 0;
	for(i = 0; i < nelem(order); i++){
		u <<= 1;
		if(v & 1<<order[i])
			u |= 1;
	}
	return map10(u);
}

static int
rawtomv(int r, int scale)
{
	int β, neg, f;

	neg = 0;
	f = 1000;
	β = 1000;
	switch(scale){
	case Vcorea:
	case Vcoreb:
		if(vidµv == ~0)
			break;
		β = vidµv / 256;
		break;
	case V12:		/* 28kΩ 10kΩ */
		β = 3800;
		break;
	case V5:			/* 34kΩ 50kΩ */
		β = 1660;
		break;
	case Vvsb:		/* 17kΩ 33kΩ */
		β = 1520;
		break;
	case V5n:		/* 56kΩ 120kΩ */
		neg = 1;
		β = 681;
		break;
	case V12n:		/* 56kΩ 232kΩ */
		neg = 1;
		β = 806;
		break;
	}
	r *= 16;
	if(neg)
		r = (r*f - 3600*β)/(f - β);
	else
		r = (r*β)/f;
	return r;
}

static int
mvprint(char *buf, int n, int mv)
{
	uint amv;

	if(mv < 0){
		amv = -mv;
		if(amv%10 >= 5)
			mv -= 10;
	}else{
		amv = mv;
		if(amv%10 >= 5)
			mv += 10;
	}
	return snprint(buf, n, "%d.%.02d", mv/1000, (amv%1000)/10);
}

static char*
prvolt(char *p, char *e, char *s, int scale, int *reg)
{
	char buf[3][32];
	int i, v[3];

	for(i = 0; i < 3; i++){
		v[i] = hwread(reg[i]);
		mvprint(buf[i], sizeof buf[i], rawtomv(v[i], scale));
	}
	p = seprint(p ,e, "%s" "\t" "%s < %s < %s\t", s, buf[0], buf[1], buf[2]);
	return seprint(p ,e, "\t" "%d < %d < %d\n", v[0], v[1], v[2]);

}

static long
wbvolt(Chan*, void *a, long n, vlong offset)
{
	char *buf, *p, *e;
	int i, scale;

	buf = smalloc(1024);
	p = buf;
	e = buf + 1024;
	selfn(Hwmon);
	for(i = 0; i < Vmax; i++){
		scale = smpvolt[i][0];
		p = prvolt(p, e, vname[scale], scale, smpvolt[i] + 1);
	}
	release();
	n = readstr(offset, a, n, buf);
	free(buf);
	return n;
}

static long
wbbeep(Chan*, void *, long n, vlong)
{
	selfn(Hwmon);
	hwwrite(Beepctl, hwread(Beepctl) | 0x20);
	release();

	if(waserror())
		nexterror();
	tsleep(&up->sleep, return0, 0, 500);
	poperror();

	selfn(Hwmon);
	hwwrite(Beepctl, hwread(Beepctl) & ~0x20);
	release();

	return n;
}

static void
limbo(char *msg)
{
	iofree(Index);
	iofree(hwaddr);
	print("  %s\n", msg);
	release();
}

void
winbondlink(void)
{
	char *chip;
	int id, rev, i²c;

	chip = " ";
	selfn(0);
	i²c = 1;
	switch(wbread(Id)){
	case 0x52:
	case 0xb0:	/* 83627dhg-p */
		chip = "83627dhg-p ";
		cap = Beep | Dog | Fans | Stemp | Volts;
		break;
	case 0x88:
	case 0xa0:
		cap = Beep | Dog | Fans | Stemp;
		break;
	case 0xf2:	/* 83627hf */
		chip = "83627hf ";
		cap = Beep | Fans | Stemp | Volts;
		i²c = 0;
		break;
	default:
		break;
	}
	wbwrite(7, Hwmon);	/* BOTCH */
	hwaddr = wbreadw(0x60) & ~0x7;
	id = wbreadw(Id);
	rev = wbread(Rev);
	print("winbond %s%.4ux.%.2x hw %.4ux \n", chip, id, rev, hwaddr);
	if(cap == 0 || hwaddr == 0){
		limbo("no capabilities");
		return;
	}
	wbwrite(Hwstat, wbread(Hwstat) | 1);
	if(i²c){
		hwwrite(0x4a, 0x89);	/* sleezy way to disable i2c */
		hwwrite(0x48, 0x1f);
		hwwrite(Cfg, 0x88);
		while(hwread(Cfg) & 0x80)
			;
	}
	hwwrite(Cfg, 1);
	hwwrite(Cfg, 9);

	if(ioalloc(Index, 2, 0, "winbond") == -1){
		limbo("ioalloc fails");
		return;
	}
	if(ioalloc(hwaddr, 2, 0, "winbond.hw") == -1){
		limbo("ioalloc fails");
		return;
	}
	if(cap & Stemp){
		addarchfile("cputemp", 0444, wbtemp, nil);	/* backstop */
		addarchfile("wbtemp", 0444, wbtemp, nil);
	}
	if(cap & Fans){
		cfandiv();
		addarchfile("fans", 0444, wbfans, nil);
	}
	if(cap & Dog)
		addarchfile("dog", 0644, wbdogr, wbdogw);
	if(cap & Volts){
		hwwrite(Vbat, hwread(Vbat) | 1);
		vid = hwread(0x47) & 0xf | (hwread(0x49)&1) << 4;
		vidµv = vidcvt(vid);
		addarchfile("volt", 0444, wbvolt, nil);
	}
	if(cap & Beep)
		addarchfile("beep", 0200, nil, wbbeep);
	release();
}

static void
unused(void)
{
	int t[] = {14, 24, 48, 0};

	print("cr22-26 %.2x %.2x\n", wbread(0x22), wbread(0x26));
	print("rt %.2ux %.2ux %.2ux\n", hwread(0x459), hwread(0x45a), hwread(0x45b));
	print("fanctl %.2ux\n", hwread(0x04d));
	print("pin div %d; input %dmhz\n", 1<< (hwread(Pinctl)>>4 & 3), t[hwread(Pinctl)>>2 & 3]);
	//hwwrite(0x3b, 200); hwwrite(0x3c, 200); hwwrite(0x3d, 200);
	print("fan lim %d %d %d\n", hwread(0x3b), hwread(0x3c), hwread(0x3d));
	print("smi %.2x %.2x\n", hwread(0x43), hwread(0x44));
	print("0x40 %d conf\n", hwread(0x40));
	print("pwm1 %.2x %.2x; 0x4a %.2x\n", hwread(0x5a), hwread(0x5b), hwread(0x4a));
	print("clock %dmhz; irq %.2x %.2x\n", 24*(1+(wbread(0x24)>>6 & 1)), hwread(0x41), hwread(0x42));
	print("0x46 %d chassis\n", hwread(0x46));
	print("0x48 %d serial\n", hwread(0x48));
	print("0x4a %.2ux\n", hwread(0x4a));
	print("0x4f %.2ux\n", hwread(0x4f));
	print("0x62 %.4ux\n", hwreadw(0x62));
	print("0x550 %d 5vsb\n", hwread(0x550));
	print("0x27 %d\n", hwread(0x27));
	if((cap & Temp) == 0)
		return;
	print("Cputin %d\n", hwread(Cputin));
	print("0x152 %d\n", hwread(0x152));
	print("0x153 %d\n", hwread(0x153));
	print("0x154 %d\n", hwread(0x154));
	print("0x155 %d\n", hwread(0x155));
	print("Cputin2 %d\n", hwread(0x250));
	print("0x454 %d %d %d temp offset\n", hwread(0x454), hwread(0x455), hwread(0x456));
	print("0x459 %.2ux %.2ux %.2ux rt hw\n", hwread(0x459), hwread(0x45a), hwread(0x45b));
}
