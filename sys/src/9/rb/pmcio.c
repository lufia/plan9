/*
 *  Performance counters non portable part
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

#include	"pmc.h"

typedef struct PmcCfg PmcCfg;
typedef struct PmcCore PmcCore;

enum {
	PeUnk,
	PeMips,
};

enum {
	_PeUnk,
	/* Non architectural */
	PeMips24k,
	PeMipsOcteon,
};

enum {
	PeNreg24k	= 2,	/* Number of Pe/Pct regs for 24K */
};

enum {						/* HW Performance Counters Event Selector */
	PeExl		= (1 << 0),
	PeOS		= (1 << 1),
	PeSup		= (1 << 2),
	PeUsr		= (1 << 3),
	PeCtEna		= (1 << 4),
	PeM		= (1 << 31),
	PeEvMsk		= 0xfe0,
};

struct PmcCfg {
	int nregs;
	int vendor;
	int family;
	PmcCtlCtrId *pmcidsarch;
	PmcCtlCtrId *pmcids;
};

extern int pmcdebug;

static PmcCfg cfg;
static PmcCore pmccore[MAXMACH];

static void pmcmachupdate(void);

static int
getnregs(void)
{
	if(!(getconfig1() & (1 << 4)))
		return 0;
	if(!(getperfctl0() & PeM))
		return 1;
	if(!(getperfctl1() & PeM))
		return 2;
	return 2;
}

int
pmcnregs(void)
{
	int nregs;

	if(cfg.nregs != 0)
		return cfg.nregs;	/* don't call cpuid more than necessary */
	switch(cfg.vendor){
	default:
		nregs = getnregs();
	}
	if(nregs > PmcMaxCtrs)
		nregs = PmcMaxCtrs;
	return nregs;
}

PmcCtlCtrId pmcidsmips[] = {
	{"", ""},
};

/* Table 6.46 MIPS32 24K Processor Core Family Software User’s Manual */
PmcCtlCtrId pmcid24k[] = {
	{"cycle", "0x0", 2},
	{"instr executed", "0x1", 2},
	{"branch completed", "0x2", 0},
	{"branch mispred", "0x2", 1},
	{"return", "0x3", 0},
	{"return mispred", "0x3", 1},
	{"return not 31", "0x4", 0},
	{"return notpred", "0x4", 1},
	{"itlb access", "0x5", 0},
	{"itlb miss", "0x5", 1},
	{"dtlb access", "0x6", 0},
	{"dtlb miss", "0x6", 1},
	{"jtlb iaccess", "0x7", 0},
	{"jtlb imiss", "0x7", 1},
	{"jtlb daccess", "0x8", 0},
	{"jtlb dmiss", "0x8", 1},
	{"ic fetch", "0x9", 0},
	{"ic miss", "0x9", 1},
	{"dc loadstore", "0x10", 0},
	{"dc writeback", "0x10", 1},
	{"dc miss", "0x11", 2},  
	/* event 12 reserved */
	{"store miss", "0x13", 0},
	{"load miss", "0x13", 1},
	{"integer completed", "0x14", 0},
	{"fp completed", "0x14", 1},
	{"load completed", "0x15", 0},
	{"store completed", "0x15", 1},
	{"barrier completed", "0x16", 0},
	{"mips16 completed", "0x16", 1},
	{"nop completed", "0x17", 0},
	{"integer muldiv completed", "0x17", 1},
	{"rf stall", "0x18", 0},
	{"instr refetch", "0x18", 1},
	{"store cond completed", "0x19", 0},
	{"store cond failed", "0x19", 1},
	{"icache requests", "0x20", 0},
	{"icache hit", "0x20", 1},
	{"l2 writeback", "0x21", 0},
	{"l2 access", "0x21", 1},
	{"l2 miss", "0x22", 0},
	{"l2 err corrected", "0x22", 1},
	{"exceptions", "0x23", 0},
	/* events 23/1-24/1 reserved */
	{"rf cycles stalled", "0x24", 0},
	{"ifu cycles stalled", "0x25", 0},
	{"alu cycles stalled", "0x25", 1},
	/* events 26-32 reserved */
	{"uncached load", "0x33", 0},
	{"uncached store", "0x33", 1},
	{"cp2 reg to reg completed", "0x35", 0},
	{"mftc completed", "0x35", 1},
	/* event 36 reserved */
	{"ic blocked cycles", "0x37", 0},
	{"dc blocked cycles", "0x37", 1},
	{"l2 imiss stall cycles", "0x38", 0},
	{"l2 dmiss stall cycles", "0x38", 1},
	{"dmiss cycles", "0x39", 0},
	{"l2 miss cycles", "0x39", 1},
	{"uncached block cycles", "0x40", 0},
	{"mdu stall cycles", "0x41", 0},
	{"fpu stall cycles", "0x41", 1},
	{"cp2 stall cycles", "0x42", 0},
	{"corextend stall cycles", "0x42", 1},
	{"ispram stall cycles", "0x43", 0},
	{"dspram stall cycles", "0x43", 1},
	{"cache stall cycles", "0x44", 0},
	/* event 44/1 reserved */
	{"load to use stalls", "0x45", 0},
	{"base mispred stalls", "0x45", 1},
	{"cpo read stalls", "0x46", 0},
	{"branch mispred cycles", "0x46", 1},
	/* event 47 reserved */
	{"ifetch buffer full", "0x48", 0},
	{"fetch buffer allocated", "0x48", 1},
	{"ejtag itrigger", "0x49", 0},
	{"ejtag dtrigger", "0x49", 1},
	{"fsb lt quarter", "0x50", 0},
	{"fsb quarter to half", "0x50", 1},
	{"fsb gt half", "0x51", 0},
	{"fsb full pipeline stalls", "0x51", 1},
	{"ldq lt quarter", "0x52", 0},
	{"ldq quarter to half", "0x52", 1},
	{"ldq gt half", "0x53", 0},
	{"ldq full pipeline stalls", "0x53", 1},
	{"wbb lt quarter", "0x54", 0},
	{"wbb quarter to half", "0x54", 1},
	{"wbb gt half", "0x55", 0},
	{"wbb full pipeline stalls", "0x55", 1},
	/* events 56-63 reserved */
	{"request latency", "0x61", 0},
	{"request count", "0x61", 1},
	{"", ""},
};

PmcCtlCtrId pmcidocteon[] = {
	{"", ""},
};

#define MIPSMODEL(x)	(((x)>>8) & MASK(8))
#define MIPSFAMILY(x)	(((x)>>16) & MASK(8))

static int
pmcmipsfamily(void)
{
	u32int cpuid, fam, mod;

	cpuid = prid();

	fam = MIPSFAMILY(cpuid);
	mod = MIPSMODEL(cpuid);
	if(fam != 0x1)
		return PeUnk;
	switch(mod){
	case 0x93:
	case 0x96:
		return PeMips24k;
	}
	return PeUnk;
}

void
pmcinitctl(PmcCtl *p)
{
	memset(p, 0xff, sizeof(PmcCtl));
	p->enab = PmcCtlNullval;
	p->user = PmcCtlNullval;
	p->os = PmcCtlNullval;
	p->nodesc = 1;
}

void
pmcconfigure(void)
{
	int i, j, isrecog;

	isrecog = 0;

	if(MIPSFAMILY(prid()) == 1){
		isrecog++;
		cfg.vendor = PeMips;
		cfg.family = pmcmipsfamily();
		cfg.pmcidsarch = pmcidsmips;
		switch(cfg.family){
		case PeMips24k:
			cfg.pmcids = pmcid24k;
			break;
		case PeMipsOcteon:
			cfg.pmcids = pmcidocteon;
			break;
		}
	}else
		cfg.vendor = PeUnk;

	cfg.nregs = pmcnregs();
	if(isrecog)
		pmcupdate = pmcmachupdate;

	for(i = 0; i < MAXMACH; i++) {
		if(MACHP(i) != nil){
			for(j = 0; j < cfg.nregs; j++)
				pmcinitctl(&pmccore[i].ctr[j]);
		}
	}
}

int
pmctrans(PmcCtl *p, int regno)
{
	PmcCtlCtrId *pi;
	int n;

	n = 0;
	if(cfg.pmcidsarch != nil)
		for (pi = &cfg.pmcidsarch[0]; pi->portdesc[0] != '\0'; pi++){
			if (strncmp(p->descstr, pi->portdesc, strlen(pi->portdesc)) == 0){
				if (regno != pi->regno && pi->regno != 2){
					print("%s not supported on register %d\n", p->descstr, regno);
					break;
				}
				strncpy(p->descstr, pi->archdesc, strlen(pi->archdesc) + 1);
				n = 1;
				break;
			}
		}
	/* this ones supersede the other ones */
	if(cfg.pmcids != nil)
		for (pi = &cfg.pmcids[0]; pi->portdesc[0] != '\0'; pi++){
			if (strncmp(p->descstr, pi->portdesc, strlen(pi->portdesc)) == 0){
				if (regno != pi->regno && pi->regno != 2){
					print("%s not supported on register %d\n", p->descstr, regno);
					break;
				}
				strncpy(p->descstr, pi->archdesc, strlen(pi->archdesc) + 1);
				n = 1;
				break;
			}
		}
	if(pmcdebug != 0)
		print("really setting %s\n", p->descstr);
	return n;
}

#define SetEvMsk(v, e) ((v)|((e)&0x3ff<<5))
#define GetEvMsk(e) (((e)&0x3ff)<<5)

static u32int
getevt(u32int regno)
{
	u32int r;

	switch(regno){
	case 0:
		r = getperfctl0();
		break;
	case 1:
		r = getperfctl1();
		break;
	default:
		r = 0;
	}

	return r;
}

static int
setevt(u32int v, u32int regno)
{
	switch(regno){
	case 0:
		setperfctl0(v);
		break;
	case 1:
		setperfctl1(v);
		break;
	}

	return 0;
}

static int
getctl(PmcCtl *p, u32int regno)
{
	u32int r, e;

	r = getevt(regno);
	p->enab = (r&PeCtEna) != 0;
	p->user = (r&PeUsr) != 0;
	p->os = (r&PeOS) != 0;
	e = GetEvMsk(r);
	/* TODO inverse translation */
	snprint(p->descstr, KNAMELEN, "%#ux", e);
	p->nodesc = 0;
	return 0;
}

static int
pmcanyenab(void)
{
	int i;
	PmcCtl p;

	for (i = 0; i < cfg.nregs; i++) {
		if (getctl(&p, i) < 0)
			return -1;
		if (p.enab)
			return 1;
	}

	return 0;
}


static int
setctl(PmcCtl *p, int regno)
{
	u32int v, e;
	char *toks[2];
	char str[KNAMELEN];

	v = getevt(regno);
	v &= ~(PeEvMsk|PeCtEna|PeOS|PeSup|PeUsr|PeExl);
	if (p->enab != PmcCtlNullval)
		if (p->enab)
			v |= PeCtEna;
		else
			v &= ~PeCtEna;

	if (p->user != PmcCtlNullval)
		if (p->user)
			v |= PeUsr;
		else
			v &= ~PeUsr;

	if (p->os != PmcCtlNullval)
		if (p->os)
			v |= PeOS | PeSup;
		else
			v &= ~PeOS | PeSup;
	if (pmctrans(p, regno) < 0)
		return -1;

	if (p->nodesc == 0) {
		memmove(str, p->descstr, KNAMELEN);
		if (tokenize(str, toks, 1) != 1)
			return -1;
		e = atoi(toks[0]);
		v = SetEvMsk(v, e);
	}
	setevt(v, regno);
	if (pmcdebug) {
		v = getevt(regno);
		print("conf pmc[%#ux]: %#ux\n", regno, v);
	}
	return 0;
}

int
pmcdescstr(char *str, int nstr)
{
	PmcCtlCtrId *pi;
	int ns;

	ns = 0;

	if(pmcdebug != 0)
		print("vendor %x family %x nregs %d pmcnregs %d\n", cfg.vendor, cfg.family, cfg.nregs, pmcnregs());
	if(cfg.pmcidsarch == nil && cfg.pmcids == nil){
		*str = 0;
		return ns;
	}

	if(cfg.pmcidsarch != nil)
		for (pi = &cfg.pmcidsarch[0]; pi->portdesc[0] != '\0'; pi++)
			ns += snprint(str + ns, nstr - ns, "%s\n",pi->portdesc);
	if(cfg.pmcids != nil)
		for (pi = &cfg.pmcids[0]; pi->portdesc[0] != '\0'; pi++)
			ns += snprint(str + ns, nstr - ns, "%s\n",pi->portdesc);
	return ns;
}

static u32int
getctr(u32int regno)
{
	u32int r;

	switch(regno){
	case 0:
		r = getperfctr0();
		break;
	case 1:
		r = getperfctr1();
		break;
	default:
		r = 0;
	}

	return r;
}

static int
setctr(u32int v, u32int regno)
{
	switch(regno){
	case 0:
		setperfctr0(v);
		break;
	case 1:
		setperfctr1(v);
		break;
	}

	return 0;
}

u64int
pmcgetctr(u32int coreno, u32int regno)
{
	PmcCtr *p;
	u64int ctr;

	if (regno >= cfg.nregs)
		error("invalid reg");
	p = &pmccore[coreno].ctr[regno];

	ilock(&pmccore[coreno]);
	if(coreno == m->machno)
		ctr = getctr(regno);
	else
		ctr = p->ctr;
	iunlock(&pmccore[coreno]);

	return ctr;
}

int
pmcsetctr(u32int coreno, u64int v, u32int regno)
{
	PmcCtr *p;
	int n;

	if (regno >= cfg.nregs)
		error("invalid reg");
	p = &pmccore[coreno].ctr[regno];

	ilock(&pmccore[coreno]);
	if(coreno == m->machno)
		n = setctr(v, regno);
	else{
		p->ctr = v;
		p->ctrset |= PmcSet;
		p->stale = 1;
		n = 0;
	}
	iunlock(&pmccore[coreno]);

	return n;
}

static void
ctl2ctl(PmcCtl *dctl, PmcCtl *sctl)
{
	if(sctl->enab != PmcCtlNullval)
		dctl->enab = sctl->enab;
	if(sctl->user != PmcCtlNullval)
		dctl->user = sctl->user;
	if(sctl->os != PmcCtlNullval)
		dctl->os = sctl->os;
	if(sctl->nodesc == 0) {
		memmove(dctl->descstr, sctl->descstr, KNAMELEN);
		dctl->nodesc = 0;
	}
}

int
pmcsetctl(u32int coreno, PmcCtl *pctl, u32int regno)
{
	PmcCtr *p;
	int n;

	if (regno >= cfg.nregs)
		error("invalid reg");
	p = &pmccore[coreno].ctr[regno];

	ilock(&pmccore[coreno]);
	if(coreno == m->machno)
		n = setctl(pctl, regno);
	else{
		ctl2ctl(&p->PmcCtl, pctl);
		p->ctlset |= PmcSet;
		p->stale = 1;
		n = 0;
	}
	iunlock(&pmccore[coreno]);

	return n;
}

int
pmcgetctl(u32int coreno, PmcCtl *pctl, u32int regno)
{
	PmcCtr *p;
	int n;

	if (regno >= cfg.nregs)
		error("invalid reg");
	p = &pmccore[coreno].ctr[regno];

	ilock(&pmccore[coreno]);
	if(coreno == m->machno)
		n = getctl(pctl, regno);
	else{
		memmove(pctl, &p->PmcCtl, sizeof(PmcCtl));
		n = 0;
	}
	iunlock(&pmccore[coreno]);

	return n;
}

static void
pmcmachupdate(void)
{
	PmcCtr *p;
	int coreno, i, maxct;

	if((maxct = cfg.nregs) <= 0)
		return;
	coreno = m->machno;

	ilock(&pmccore[coreno]);
	for (i = 0; i < maxct; i++) {
		p = &pmccore[coreno].ctr[i];
		if(p->ctrset & PmcSet)
			setctr(p->ctr, i);
		if(p->ctlset & PmcSet)
			setctl(p, i);
		p->ctr = getctr(i);
		getctl(p, i);
		p->ctrset = PmcIgn;
		p->ctlset = PmcIgn;
		p->stale = 0;
	}
	iunlock(&pmccore[coreno]);
}
