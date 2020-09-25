#include <u.h>
#include <libc.h>
#include <bio.h>

enum {
	Dmistart	= 0x000F0000,
	Dmiend	= 0x000FFFFF,
	Dmisz	= Dmiend+1 - Dmistart,
};

typedef	struct	Enum	Enum;
typedef	struct	Item	Item;
typedef	struct	Itemv	Itemv;
typedef	struct	Smbuf	Smbuf;
typedef	struct	Sm	Sm;
typedef	struct	Smval	Smval;
typedef uvlong	uintmem;		/* wierd */

struct Enum {
	uint	v;
	char	*s;
};

struct Smbuf {
	uchar	sig[4];
	uchar	cksum;
	uchar	len;
	uchar	vers[2];
	uchar	maxss[2];		/* maximum structure size */
	uchar	eprev;		/* entry point revision */
	uchar	fma[5];		/* formatted area */
	uchar	dmi[5];		/* _DMI_ */
	uchar	dmicksum;
	uchar	stlen[2];
	uchar	stpa[4];		/* structure table pa */
	uchar	nstrut[2];
	uchar	bcdrev;
};

struct Smval {
	int	type;
	int	hand;
	int	len;

	Item	*i;
	Itemv	*v;
};

struct Sm {
	Smbuf	raw;
	uintmem	stpa;
	uint	stlen;

	uint	n;		/* count of structures */
	Smval	*v;
};

struct Item{
	char	*name;
	int	nbytes;
	int	type;
	int	offset;
	char	*fmt;
	Enum	*tab;
};

struct Itemv{
	union	{
		char	*s;
		uvlong	i;
		uchar	uuid[16];
	};
};

enum {
	Biosinfo	= 0,
	Sysinfo	= 1,
	Module	= 2,
	Chassis	= 3,
	Cpu	= 4,
	Memctlr	= 5,		/* obs. */
	Memstk	= 6,		/* obs. */
	Cache	= 7,
	Conn	= 8,
	Slot	= 9,
	Obddev	= 10,		/* obs. */
	Oemstr	= 11,
	Sysconf	= 12,
	Bioslang	= 13,
	Groupa	= 14,
	Syslog	= 15,		/* requires extra poking around */
	Pmem	= 16,
	Mdev	= 17,
	Merror	= 18,
	Mmap	= 19,
	Mmdev	= 20,
	Pointer	= 21,
	Battery	= 22,
	Wdog	= 23,
	Hwsec	= 24,
	Syspwr	= 25,
	Vprobe	= 26,
	Fan	= 27,
	Tprobe	= 28,
	Cprobe	= 29,
	Bootinfo = 32,
	Merror64 = 33,
	Mgmt	= 34,
	Mgmtc	= 35,
	Mgmtdt	= 36,
	Memch	= 37,
	Ipmi	= 38,
	Ps	= 39,
	Obd	= 41,
	Inactive	= 126,
	End	= 127,
};

Enum tabletab[] = {
	0,	"biosinfo",
	1,	"sysinfo",
	2,	"module",
	3,	"chassis",
	4,	"cpu",
	5,	"memctlr",
	6,	"memstk",
	7,	"cache",
	8,	"conn",
	9,	"slot",
	10,	"obddev",
	11,	"oemstr",
	12,	"sysconf",
	13,	"bioslang",
	14,	"groupa",
	15,	"syslog",
	16,	"pmem",
	17,	"mdev",
	18,	"merror",
	19,	"mmap",
	20,	"mmdev",
	21,	"pointer",
	22,	"battery",
	23,	"wdog",
	24,	"hwsec",
	25,	"syspwr",
	26,	"vprobe",
	27,	"fan",
	28,	"tprobe",
	29,	"cprobe",
	31,	"bis",
	32,	"bootinfo",
	33,	"merror64",
	34,	"mgmt",
	35,	"mgmtc",
	36,	"mgmtdt",
	37,	"memch",
	38,	"ipmi",
	39,	"ps",
	41,	"obd",
	126,	"inactive",
	127,	"end",
	0,	nil,
};

uchar	*dmianchor[0x20];
int	rfd = -1;
int	prtype = -1;
int	prhand = -1;
int	verbose;
Biobuf	outsb;
Biobuf	*out;

#define	Bvprint(...)	do if(verbose) Bprint(__VA_ARGS__); while(0)

void
realmodefd(void)
{
	if(rfd != -1)
		return;
	rfd = open("/dev/realmodemem", OREAD);
	if(rfd == -1)
		sysfatal("open: %r");
}

void*
emalloc(usize sz)
{
	void *v;

	v = malloc(sz);
	if(v == nil)
		sysfatal("malloc: %r");
	memset(v, 0, sz);
	setmalloctag(v, getcallerpc(&sz));
	return v;
}

enum {
	String,
	Integer,
	Uchar,
	Enumf,
	Enumbv,
};

Item	biositems[] = {
	"vendor",	1,	String,	-1,	"%s",		nil,
	"version",	1,	String,	-1,	"%s",		nil,
	"biosstartseg",	2,	Integer,	-1,	"%#.4llux",	nil,
	"biosdate",	1,	String,	-1,	"%s",		nil,
	"biosromsz",	1,	Integer,	-1,	"%#.4llux",	nil,	// _+1<<16
	"biosflags",	8,	Uchar,	-1,	"%.8H",		nil,
	"extflags0",	1,	Integer,	-1,	"%.2llux",	nil,
	"extflags1",	1,	Integer,	-1,	"%.2llux",	nil,
	"release0",	1,	Integer,	-1,	"%.2llux",	nil,
	"release1",	1,	Integer,	-1,	"%.2llux",	nil,
	"embedrelease0",	1,	Integer,	-1,	"%.2llux",	nil,
	"embedrelease1",	1,	Integer,	-1,	"%.2llux",	nil,
};

Enum waketype[] = {
	0,	"reserved",
	1,	"other",
	2,	"unknown",
	3,	"apmtimer",
	4,	"modem",
	5,	"lan",
	6,	"powersw",
	7,	"pcipme",
	8,	"powerrestored",
	0,	nil,
};

Item	sysitems[] = {
	"mfg",		1,	String,	-1,	"%s",		nil,
	"product",	1,	String,	-1,	"%s",		nil,
	"version",	1,	String,	-1,	"%s",		nil,
	"serial",		1,	String,	-1,	"%s",		nil,
	"uuid",		16,	Uchar,	-1,	"%.16H",	nil,
	"waketype",	1,	Enumf,	-1,	"%s",		waketype,
	"sku",		1,	String,	-1,	"%s",		nil,
	"family",		1,	String,	-1,	"%s",		nil,
};

Item	moditems[] = {
	"bbmfg",		1,	String,	-1,	"%s",		nil,
	"product",	1,	String,	-1,	"%s",		nil,
	"bbversion",	1,	String,	-1,	"%s",		nil,
	"bbserial",	1,	String,	-1,	"%s",		nil,
	"assettag",	1,	String,	-1,	"%s",		nil,
	"features",	1,	Integer,	-1,	"%#.2llux",	nil,
	"chassisloc",	1,	String,	-1,	"%s",		nil,
	"chassishand",	2,	Integer,	-1,	"%#.4llux",	nil,
	"bbtype",		1,	Integer,	-1,	"%lld",		nil,
	/* continued object handles; 2 bytes each */
};

Item	chassisitems[] = {
	"chassismfg",	1,	String,	-1,	"%s",		nil,
	"chassistype",	1,	Integer,	-1,	"%lld",		nil,
	"chassisver",	1,	String,	-1,	"%s",		nil,
	"chassisserial",	1,	String,	-1,	"%s",		nil,
	"chassisassettag",	1,	String,	-1,	"%s",		nil,

	"bootstate",	1,	Integer,	-1,	"%lld",		nil,
	"powerst",	1,	Integer,	-1,	"%lld",		nil,
	"thermalst",	1,	Integer,	-1,	"%lld",		nil,
	"securityst",	1,	Integer,	-1,	"%lld",		nil,

	"height",		1,	Integer,	-1,	"%lld",		nil,
	"powercord#",	1,	Integer,	-1,	"%lld",		nil,
	"elementcnt",	1,	Integer,	-1,	"%lld",		nil,

	/* containted elements & sku number */
};

Enum sockettype[] = {
	1,	"other",
	2,	"unknown",
	3,	"daughter board",
	4,	"zif socket",
	5,	"replacable piggy back",
	6,	"none",
	7,	"lif socket",
	8,	"slot 1",
	9,	"slot 2",
	0xa,	"370-pin socket",
	0xb,	"slot a",
	0xc,	"slot m",
	0xd,	"socket 423",
	0xe,	"socket a (socket 462)",
	0xf,	"socket 478",
	0x10,	"socket 754",
	0x11,	"socket 940",
	0x12,	"socket 939",
	0x13,	"socket mPGA604",
	0x14,	"socket lga771",
	0x15,	"socket lga775",
	0x16,	"socket s1",
	0x17,	"socket am2",
	0x18,	"socket f",
	0x19,	"socket lga1366",
	0x1a,	"socket g34",
	0x1b,	"socket am3",
	0x1c,	"socket c32",
	0x1d,	"socket lga1156",
	0x1d,	"socket lga1567",
	0x1f,	"socket pga988a",
	0x20,	"socket bg1288",
	0x21,	"socket rpga988b",
	0x22,	"socket bga1023",
	0x23,	"socket bga1224",
	0x24,	"socket bga1155",
	0x25,	"socket bga1356",
	0x26,	"socket lga2011",
	0x27,	"socket fs1",
	0x28,	"socket fs2",
	0x29,	"socket fm1",
	0x2a,	"socket fm2",
	0,	nil,
};

Item	cpuitems[] = {
	"socket",		1,	String,	-1,	"%s",		nil,
	"cputype",	1,	Integer,	-1,	"%lld",		nil,
	"cpufamily",	1,	Integer,	-1,	"%lld",		nil,
	"cpumfg",	1,	String,	-1,	"%s",		nil,
	"cpuid",		8,	Integer,	-1,	"%llux",		nil,
	"cpuvers",	1,	String,	-1,	"%s",		nil,
	"cpuvolt",	1,	Integer,	-1,	"%lld",		nil,
	"clockmhz",	2,	Integer,	-1,	"%lld",		nil,
	"cpuspeed",	2,	Integer,	-1,	"%lld",		nil,
	"cpuspeed (now)",	2,	Integer,	-1,	"%lld",		nil,
	"cpustatus",	1,	Integer,	-1,	"%lld",		nil,
	"cpuupgrade",	1,	Enumf,	-1,	"%s",		sockettype,
	"cpul1",		2,	Integer,	-1,	"%.2llux",	nil,
	"cpul2",		2,	Integer,	-1,	"%.2llux",	nil,
	"cpul3",		2,	Integer,	-1,	"%.2llux",	nil,
	"cpuserial",	1,	String,	-1,	"%s",		nil,
	"cpuassettag",	1,	String,	-1,	"%s",		nil,
	"cpupart#",	1,	String,	-1,	"%s",		nil,
	"cpucore#",	1,	Integer,	-1,	"%lld",		nil,
	"cpucoreen",	1,	Integer,	-1,	"%lld",		nil,
	"cputhreadcnt",	1,	Integer,	-1,	"%lld",		nil,
	"cpuenum",	2,	Integer,	-1,	"%#.4llux",	nil,
	"cpufamily2",	2,	Integer,	-1,	"%#.4llux",	nil,
};

Enum ecctypetab[] = {
	1,	"other",
	2,	"unknown",
	3,	"none",
	4,	"8-bit parity",
	5,	"32-bit ecc",
	6,	"32-bit ecc",
	7,	"32-bit ecc",
	8,	"crc",
	0,	nil,
};

/* bitfield */
Enum ecccap[] = {
	1<<0,	"other",
	1<<1,	"unknown",
	1<<2,	"none",
	1<<3,	"single-bit",
	1<<4,	"double-bit",
	1<<5,	"error scrubbing",
	0,	nil,
};

Enum interleave[] = {
	1,	"other",
	2,	"unknown",
	3,	"one-way",
	4,	"two-way",
	5,	"four-way",
	6,	"eight-way",
	7,	"sixteen-way",
	0,	nil,
};

Item	memctlritems[] = {
	"ecctype",	1,	Enumf,	-1,	"%s",		ecctypetab,
	"ecccap",		1,	Enumbv,	-1,	"%s",		ecccap,
	"interleavesupp",	1,	Enumf,	-1,	"%s",		interleave,
	"interleavecurr",	1,	Enumf,	-1,	"%s",		interleave,
	"maxmodsz",	1,	Integer,	-1,	"%ℛ",		nil,
	"speedsupp",	2,	Integer,	-1,	"%#.4llux",	nil,
	"typessupp",	2,	Integer,	-1,	"%#.4llux",	nil,
	"voltages",	1,	Integer,	-1,	"%#.2llux",	nil,
	"memslots",	1,	Integer,	-1,	"%lld",		nil,
/*	"handles"	2*memslots,	Integer,	-1,	"%#.4llux",	nil,	*/
/*	"eccenabled"	1,	Integer,	-1,	"%.2llux",		nil,	*/
};

int
fmtℛ(Fmt *f)
{
	char *prefix;
	uint code;

	prefix = "";
	code = va_arg(f->args, uvlong);
	if(f->flags & FmtSharp){
		switch(code){
		case 0x7d:
			return fmtprint(f, "not determinable");
		case 0x7e:
			return fmtprint(f, "installed but disabled");
		case 0x7f:
			return fmtprint(f, "not installed");
		}
		if(code & 0x80)
			prefix = "dual-port ";
		code &= ~0x80;
	}
	return fmtprint(f, "%s%dMB", prefix, 1<<code);
}

Enum moduletype[] = {
	1<<0,	"other",
	1<<1,	"unknown",
	1<<2,	"standard",
	1<<3,	"fast page mode",
	1<<4,	"edo",
	1<<5,	"parity",
	1<<6,	"ecc",
	1<<7,	"simm",
	1<<8,	"dimm",
	1<<9,	"burst edo",
	1<<10,	"sdram",
	0,	nil,
};

Item	memstkitems[] = {
	"memsocket",	1,	String,	-1,	"%s",		nil,
	"bankconn",	1,	Integer,	-1,	"%#.2llux",	nil,	/* nybble-encoding */
	"currspeed (ns)",	1,	Integer,	-1,	"%lld",		nil,
	"memtype",	2,	Integer,	-1,	"%#.4llux",	nil,
	"installsz",	1,	Integer,	-1,	"%#ℛ",		nil,
	"enablesz",	1,	Integer,	-1,	"%#ℛ",		nil,
	"errorsts",	1,	Integer,	-1,	"%.2llux",	nil,
};

Enum	sramtype[] = {
	1<<0,	"other",
	1<<1,	"unknown",
	1<<2,	"non-burst",
	1<<3,	"pipeline burst",
	1<<4,	"sync",
	1<<5,	"async",
	0,	nil,
};

Enum	cachetype[] = {
	1,	"other",
	2,	"unknown",
	3,	"instruction",
	4,	"data",
	5,	"unified",
	0,	nil,
};

Enum	cacheecc[] = {
	1,	"other",
	2,	"unknown",
	3,	"none",
	4,	"parity",
	5,	"single-bit ecc",
	6,	"multi-bit ecc",
};

Enum	assoctype[] = {
	1,	"other",
	2,	"unknown",
	3,	"direct mapped",
	4,	"2-way set-associative",
	5,	"4-way set-associative",
	6,	"fully associative",
	7,	"8-way set-associative",
	8,	"16-way set-associative",
	9,	"12-way set-associative",
	10,	"24-way set-associative",
	11,	"32-way set-associative",
	12,	"48-way set-associative",
	13,	"64-way set-associative",
	14,	"20-way set-associative",
	0,	nil,
};

Item	cacheitems[] = {
	"socket",		1,	String,	-1,	"%s",		nil,
	"config",		2,	Integer,	-1,	"%#.4llux",	nil,
	"maxsize",	2,	Integer,	-1,	"%#.4llux",	nil,
	"instalsz",	2,	Integer,	-1,	"%#.4llux",	nil,
	"sramtype",	2,	Enumf,	-1,	"%s",		sramtype,
	"installtype",	2,	Integer,	-1,	"%#.4llux",	nil,
	"speed (ns)",	1,	Integer,	-1,	"%lld",		nil,
	"ecctype",	1,	Enumf,	-1,	"%s",		cacheecc,
	"logicaltype",	1,	Enumf,	-1,	"%s",		cachetype,
	"assoc",		1,	Enumf,	-1,	"%s",		assoctype,
};

Enum extconn[] = {
	0,	"none",
	1,	"centronics",
	2,	"mimi centronics",
	3,	"proprietary",
	4,	"db-25 male",
	5,	"db-25 female",
	6,	"db-15 male",
	7,	"db-15 female",
	8,	"db-9 male",
	9,	"db-9 female",
	0xa,	"rj11",
	0xb,	"rj45",
	0xc,	"50-pin miniscsi",
	0xd,	"mini-din",
	0xe,	"micro-din",
	0xf,	"ps/2",
	0x10,	"infrared",
	0x11,	"hp-hil",
	0x12,	"usb",
	0x13,	"ssa scsi",
	0x14,	"circular din-8 male",
	0x15,	"circular din-8 female",
	0x16,	"onbard ide",
	0x17,	"onboard floppy",
	0x18,	"9-pin dual inline (pin 10 cut)",
	0x19,	"25-pin dual inline (pin 26 cut)",
	0x20,	"40-pin dual inline",
	0x21,	"68-pin dual inline",
	0x1c,	"onboard sound input from cd-rom",
	0x1d,	"mini-centronics type-14",
	0x1e,	"mini-centronics type-26",
	0x1f,	"headphone hack",
	0x20,	"bnc",
	0x21,	"1394",
	0x22,	"sas/sata receptacle",
	0xa0,	"pc-98",
	0xa1,	"pc-98hireso",
	0xa2,	"pc-h98",
	0xa3,	"pc-98note",
	0xa3,	"pc-98full",
	0,	nil,
};

Item	connitems[] = {
	"intrefdes",	1,	String,	-1,	"%s",		nil,
	"intconntype",	1,	Enumf,	-1,	"%s",		extconn,
	"extrefdes",	1,	String,	-1,	"%s",		nil,
	"extconntype",	1,	Enumf,	-1,	"%s",		extconn,
	"porttype",	1,	Integer,	-1,	"%#.2llux",	nil,
};

Enum slottype[] = {
	1,	"other",
	2,	"unknown",
	3,	"isa",
	4,	"mca",
	5,	"eisa",
	6,	"pci",
	7,	"pcmcia",
	8,	"vl-vesa",
	9,	"propoetart",
	0xa,	"processor card slot",
	0xb,	"propietary memory card slot",
	0xc,	"i/o reiser card slot",
	0xd,	"nubus",
	0xe,	"pci66mhz",
	0xf,	"agp",
	0x10,	"agp2x",
	0x11,	"agp4x",
	0x12,	"pci-x",
	0x13,	"agp8x",
	0xa0,	"pc-98/c20",
	0xa1,	"pc-98/c24",
	0xa2,	"pc-98/e",
	0xa3,	"pc-98/localbus",
	0xa4,	"pc-98/card",
	0xa5,	"pcie",
	0xa6,	"pcie x1",
	0xa7,	"pcie x2",
	0xa8,	"pcie x4",
	0xa9,	"pcie x8",
	0xaa,	"pcie x16",

	0xab,	"pcie gen2",
	0xac,	"pcie gen2 x1",
	0xad,	"pcie gen2 x2",
	0xae,	"pcie gen2 x4",
	0xaf,	"pcie gen2 x8",
	0xb0,	"pcie gen2 x16",

	0xb1,	"pcie gen3",
	0xb2,	"pcie gen3 x1",
	0xb3,	"pcie gen3 x2",
	0xb4,	"pcie gen3 x4",
	0xb5,	"pcie gen3 x8",
	0xb6,	"pcie gen3 x16",
	0,	nil,
};

Enum slotwidth[] = {
	1,	"other",
	2,	"unknown",
	3,	"8bit",
	4,	"16bit",
	5,	"32bit",
	6,	"64bit",
	7,	"128bit",
	8,	"1x",
	9,	"2x",
	0xa,	"4x",
	0xb,	"8x",
	0xc,	"12x",
	0xd,	"16x",
	0xe,	"32x",
	0,	nil,
};

Enum slotuse[] = {
	1,	"other",
	2,	"unknown",
	3,	"available",
	4,	"inuse",
	0,	nil,
};

Enum slotlength[] = {
	1,	"other",
	2,	"unknown",
	3,	"short",
	4,	"long",
	0,	nil,
};

Item	slotitems[] = {
	"slotname",	1,	String,	-1,	"%s",		nil,
	"slottype",	1,	Enumf,	-1,	"%s",		slottype,
	"datawidth",	1,	Enumf,	-1,	"%s",		slotwidth,
	"use",		1,	Enumf,	-1,	"%s",		slotuse,
	"slotlen",		1,	Enumf,	-1,	"%s",		slotlength,
	"slotid",		2,	Integer,	-1,	"%lld",		nil,
	"slotbits0",	1,	Integer,	-1,	"%.2llux",	nil,
	"slotbits1",	1,	Integer,	-1,	"%.2llux",	nil,
	"segment",	1,	Integer,	-1,	"%.2llux",	nil,
	"bus",		1,	Integer,	-1,	"%.2llux",	nil,
	"devfn",		1,	Integer,	-1,	"%.2llux",	nil,
};

Item	oemitems[] = {
	"count",		1,	Integer,	-1,	"%llud",		nil,
};

Item	sysconfitems[] = {
	"count",		1,	Integer,	-1,	"%lld",		nil,
};

Item	bioslangitems[] = {
	"installable",	1,	Integer,	-1,	"%lld",		nil,
	"flags",		1,	Integer,	-1,	"%lld",		nil,
	"reserved",	15,	Uchar,	-1,	"%.15H",	nil,
	"currlang",	1,	String,	-1,	"%s",		nil,
};

Enum accesstab[] = {
	0,	"i/o indexed 1x 8-bit index/1x 8-bit data",
	1,	"i/o indexed 2x 8-bit index/2x 8-bit data",
	2,	"i/o indexed 1x 16-bit index/1x 8-bit data",
	3,	"32-bit mmio",
	4,	"general purpose nv data access method",
	0,	nil,
};

Enum statustab[] = {
	1<<0,	"valid",
	1<<1,	"full",
	0,	nil,
};

Item	syslogitems[] = {
	"loglength",	2,	Integer,	-1,	"%#.4llux",	nil,
	"hdroffset",	2,	Integer,	-1,	"%#.4llux",	nil,
	"dataoffset",	2,	Integer,	-1,	"%#.4llux",	nil,
	"access",		1,	Enumf,	-1,	"%s",		accesstab,
	"status",		1,	Enumbv,	-1,	"%s",		statustab,
	"token",		4,	Integer,	-1,	"%#.8llux",	nil,
	"pa",		4,	Integer,	-1,	"%#.8llux",	nil,
	"format",		1,	Integer,	-1,	"%lld",		nil,
	"ndesc",		1,	Integer,	-1,	"%lld",		nil,
	"desclen",	1,	Integer,	-1,	"%lld",		nil,
	/* list goes here */
	"t0",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t1",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t2",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t3",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t4",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t5",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t6",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t7",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t8",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t9",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t10",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t11",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t12",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t13",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t14",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t15",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t16",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t17",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t18",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t19",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t20",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t21",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t22",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t23",		2,	Uchar,	-1,	"%.2lH",		nil,
	"t24",		2,	Uchar,	-1,	"%.2lH",		nil,
};

Enum	memuse[] = {
	1,	"other",
	2,	"unknown",
	3,	"system memory",
	4,	"video memory",
	5,	"flash memory",
	6,	"nvram",
	7,	"cache",
	0,	nil,
};

Item	pmemitems[] = {
	"loc",		1,	Integer,	-1,	"%lld",		nil,
	"use",		1,	Enumf,	-1,	"%s",		memuse,
	"ecc",		1,	Integer,	-1,	"%lld",		nil,
	"capacity",	4,	Integer,	-1,	"%lld kb",	nil,
	"errhandle",	2,	Integer,	-1,	"%.4llux",	nil,
	"#devices",	2,	Integer,	-1,	"%lld",		nil,
	"ecapacity",	8,	Integer,	-1,	"%lld bytes",	nil,
};

Enum	formfactor[] = {
	1,	"other",
	2,	"unknown",
	3,	"simm",
	4,	"sip",
	5,	"chip",
	6,	"dip",
	7,	"zip",
	8,	"proprietary card",
	9,	"dimm",
	0xa,	"tsop",
	0xb,	"row of chips",
	0xc,	"rimm",
	0xd,	"sodimm",
	0xe,	"srimm",
	0xf,	"fb-dimm",
	0,	nil,
};

Enum	mdevmemtype[] = {
	1,	"other",
	2,	"unknown",
	3,	"dram",
	4,	"edram",
	5,	"vram",
	6,	"sram",
	7,	"ram",
	8,	"rom",
	9,	"flash",
	0xa,	"eeprom",
	0xb,	"feprom",
	0xc,	"eprom",
	0xd,	"cdram",
	0xe,	"3dram",
	0xf,	"sdram",
	0x10,	"sgram",
	0x11,	"rdram",
	0x12,	"ddr",
	0x13,	"ddr2",
	0x14,	"ddr2 fb-dimm",
	0x18,	"ddr3",
	0x19,	"fbd2",
	0,	nil,
};

Enum	typedetail[] = {
	1<<1,	"other",
	1<<2,	"unknown",
	1<<3,	"fast-paged",
	1<<4,	"static- column",
	1<<5,	"pseudo-static",
	1<<6,	"rambus",
	1<<7,	"synchronous",
	1<<8,	"cmos",
	1<<9,	"edo",
	1<<10,	"window dram",
	1<<11,	"cache dram",
	1<<12,	"non-volatile",
	1<<13,	"registered/buffered",
	1<<14,	"unregistered/unbuffered",
	0,	nil,
};

int
fmt𝒮(Fmt *f)
{
	uint u;

	u = va_arg(f->args, uvlong);
	if(u & 0x8000)
		return fmtprint(f, "%d kb", u&~0x8000);
	return fmtprint(f, "%d mb", u);
}

Item	mdevitems[] = {
	"paryhnd",	2,	Integer,	-1,	"%#.4llux",	nil,
	"errhnd",		2,	Integer,	-1,	"%#.4llux",	nil,
	"twidth",		2,	Integer,	-1,	"%llud",		nil,
	"width",		2,	Integer,	-1,	"%llud",		nil,
	"size",		2,	Integer,	-1,	"%𝒮",		nil,
	"formfactor",	1,	Enumf,	-1,	"%s",		formfactor,
	"deviceset",	1,	Integer,	-1,	"%llud",		nil,
	"locator",	1,	String,	-1,	"%s",		nil,
	"bankloc",	1,	String,	-1,	"%s",		nil,
	"memtype",	1,	Enumf,	-1,	"%s",		mdevmemtype,
	"typedetail",	2,	Enumf,	-1,	"%s",		typedetail,
	"speed",		2,	Integer,	-1,	"%llud Mhz",	nil,
	"mfgr",		1,	String,	-1,	"%s",		nil,
	"serial",		1,	String,	-1,	"%s",		nil,
	"assettag",	1,	String,	-1,	"%s",		nil,
	"part#",		1,	String,	-1,	"%s",		nil,
	"attr",		1,	Integer,	-1,	"%.2llux",	nil,		// rank or zero
	"xsize",		4,	Integer,	-1,	"%#.4llud MB",	nil,
	"cfgspeed",	2,	Integer,	-1,	"%llud Mhz",	nil,
};

Enum errortype[] = {
	1,	"other",
	2,	"unknown",
	3,	"ok",
	4,	"bad read",
	5,	"parity error",
	6,	"single-bit error",
	7,	"double-bit error",
	8,	"muti-bit error",
	9,	"nybble error",
	0xa,	"cksum error",
	0xb,	"crc error",
	0xc,	"corrected single-bit error",
	0xd,	"corrected error",
	0xe,	"uncorrectable error",
	0,	nil,
};

Enum errorgran[] = {
	1,	"other",
	2,	"unknown",
	3,	"device",
	4,	"memory partition",
	0,	nil,
};

Enum memoryop[] = {
	1,	"other",
	2,	"unknown",
	3,	"read",
	4,	"write",
	0,	nil,
};

Item	merroritems[] = {
	"errortype",	1,	Enumf,	-1,	"%s",		errortype,
	"errorgran",	1,	Enumf,	-1,	"%s",		errorgran,
	"errorop",	1,	Enumf,	-1,	"%s",		memoryop,
	"syndrome",	4,	Integer,	-1,	"%#.8llux",	nil,
	"physaddr",	4,	Integer,	-1,	"%#.8llux",	nil,
	"devaddr",	4,	Integer,	-1,	"%#.8llux",	nil,
	"resolution",	4,	Integer,	-1,	"%#.8llux",	nil,
};

Item	mmapitems[] = {
	"start",		4,	Integer,	-1,	"%#.8llux kb",	nil,
	"end",		4,	Integer,	-1,	"%#.8llux kb",	nil,
	"ahand",		2,	Integer,	-1,	"%#.4llux",	nil,
	"partwidth",	1,	Integer,	-1,	"%lld",		nil,
	"xstart",		8,	Integer,	-1,	"%#.16llux",	nil,
	"xend",		8,	Integer,	-1,	"%#.16llux",	nil,
};

/* prec→multiplier, width→prec */
int
fmt×(Fmt *f)
{
	uvlong kb, m;

	kb = va_arg(f->args, uvlong);
	m = 1024;
	if(f->flags & FmtPrec){
		f->flags &= ~FmtPrec;
		m = f->prec;
	}
	if(f->flags & FmtWidth)
		return fmtprint(f, "%#.*llux", f->width, kb*m);
	return fmtprint(f, "%#.8llux", kb*m);
}

Item	mmdevitems[] = {
	"start",		4,	Integer,	-1,	"%8.1024×",	nil,
	"end",		4,	Integer,	-1,	"%8.1024×",	nil,
	"dhand",		2,	Integer,	-1,	"%#.4llux",	nil,
	"ahand",		2,	Integer,	-1,	"%#.4llux",	nil,
	"row",		1,	Integer,	-1,	"%lld",		nil,
	"interleave",	1,	Integer,	-1,	"%lld",		nil,
	"idepth",		1,	Integer,	-1,	"%lld",		nil,
	"xstart",		8,	Integer,	-1,	"%#.16llux",	nil,
	"xend",		8,	Integer,	-1,	"%#.16llux",	nil,
};

Item	ptritems[] = {
	"type",		1,	Integer,	-1,	"%lld",		nil,
	"interface",	1,	Integer,	-1,	"%lld",		nil,
	"buttons",	1,	Integer,	-1,	"%lld",		nil,
};

Enum batterychemistry[] = {
	1,	"other",
	2,	"unknown",
	3,	"lead acid",
	4,	"nicad",
	5,	"nimh",
	6,	"lion",
	7,	"zincair",
	8,	"li polymer",
	0,	nil,
};

Item	batteryitems[] = {
	"location",	1,	String,	-1,	"%s",		nil,
	"mfgr",		1,	String,	-1,	"%s",		nil,
	"mfgr date",	1,	String,	-1,	"%s",		nil,
	"serial",		1,	String,	-1,	"%s",		nil,
	"devname",	1,	String,	-1,	"%s",		nil,

	"chemistry",	1,	Enumf,	-1,	"%s",		batterychemistry,
	"capacity",	2,	Integer,	-1,	"%lld mWh",	nil,
	"voltage",	2,	Integer,	-1,	"%lld mV",	nil,
	"sbds vers",	1,	Integer,	-1,	"%lld",		nil,
	"maxerror%",	1,	Integer,	-1,	"%lld",		nil,
	"sbds serial",	2,	Integer,	-1,	"%.4llux",	nil,
	"sbds mfgdate",	2,	Integer,	-1,	"%.4llux",	nil,
	"sbds chem",	1,	String,	-1,	"%s",		nil,
	"capacity ×",	1,	Integer,	-1,	"%lld",		nil,
	"oemword",	2,	Integer,	-1,	"%llux",		nil,
};

Item	wdogitems[] = {
	"cap",		1,	Integer,	-1,	"%.2llux",	nil,
  	"rstcount",	2,	Integer,	-1,	"%lld",		nil,
  	"rstlim",		2,	Integer,	-1,	"%lld",		nil,
  	"interval",	2,	Integer,	-1,	"%lld min",	nil,
  	"timeout",	2,	Integer,	-1,	"%lld min",	nil,
};

Item	hwsecitems[] = {
	"settings",	1,	Integer,	-1,	"%.2llux",	nil,
};

Item	syspwritems[] = {
  	"month",		1,	Integer,	-1,	"%.2llux",	nil,
 	"mday",		1,	Integer,	-1,	"%.2llux",	nil,
	"day",		1,	Integer,	-1,	"%.2llux",	nil,
    	"min",		1,	Integer,	-1,	"%.2llux",	nil,
    	"osec",		1,	Integer,	-1,	"%.2llux",	nil,
};

Item	vprobeitems[] = {
	"desc",		1,	String,	-1,	"%s",		nil,
  	"locstat",		1,	Integer,	-1,	"%.2llux",	nil,

  	"max",		2,	Integer,	-1,	"%.2llud mV",	nil,
  	"min",		2,	Integer,	-1,	"%.2llud mV",	nil,
  	"res",		2,	Integer,	-1,	"%.2llud mV",	nil,
  	"tolerance",	2,	Integer,	-1,	"%.2llud mV",	nil,
  	"accuracy",	2,	Integer,	-1,	"%.2llud mV",	nil,
  	"oem",		4,	Integer,	-1,	"%.8llux",	nil,
  	"nominal",	2,	Integer,	-1,	"%.2llud mV",	nil,
};

Enum fantypestat[] = {
	1<<5,	"other",
	2<<5,	"unknown",
	3<<5,	"ok",
	4<<5,	"non-critical",
	5<<5,	"critical",
	6<<5,	"non-recoverable",

	1,	"other",
	2,	"unknown",
	3,	"fan",
	4,	"centrifugal blower",
	5,	"chip fan",
	6,	"cabinet fan",
	7,	"powersupply fan",
	8,	"heat pipe",
	9,	"integrated refrig",
	0xa,	"active cooling",
	0xb,	"passive cooling",
	0,	nil,
};

Item	fanitems[] = {
	"tprobhnd",	2,	Integer,	-1,	"%.4llux",	nil,
  	"typestat",	1,	Integer,	-1,	"%.2llux",	nil,
	"coolgroup",	1,	Integer,	-1,	"%.2llux",	nil,
	"oem",		4,	Integer,	-1,	"%.8llux",	nil,
	"nominalspd",	2,	Integer,	-1,	"%.4lld",		nil,
	"description",	1,	String,	-1,	"%s",		nil,
};

Item	tprobeitems[] = {
	"desc",		1,	String,	-1,	"%s",			nil,
  	"locstat",		1,	Integer,	-1,	"%.2llux",		nil,
  	"max",		2,	Integer,	-1,	"%.llud  1/10th °C",	nil,
  	"min",		2,	Integer,	-1,	"%.llud  1/10th °C",	nil,
  	"res",		2,	Integer,	-1,	"%.llud  1/10th °C",	nil,
	"tolerance",	2,	Integer,	-1,	"%.llud  1/10th °C",	nil,
  	"accuracy",	2,	Integer,	-1,	"%.llud  1/10th °C",	nil,
  	"oem",		4,	Integer,	-1,	"%.8llux",		nil,
  	"nominal",	2,	Integer,	-1,	"%.llud  1/10th °C",	nil,
};

Item	cprobeitems[] = {
	"desc",		1,	String,	-1,	"%s",		nil,
  	"locstat",		1,	Integer,	-1,	"%.2llux",	nil,
  	"max",		2,	Integer,	-1,	"%.llud  mA",	nil,
  	"min",		2,	Integer,	-1,	"%.llud  mA",	nil,
  	"res",		2,	Integer,	-1,	"%.llud  mA",	nil,
	"tolerance",	2,	Integer,	-1,	"%.llud  mA",	nil,
  	"accuracy",	2,	Integer,	-1,	"%.llud  mA",	nil,
  	"oem",		4,	Integer,	-1,	"%.8llux",	nil,
  	"nominal",	2,	Integer,	-1,	"%.llud  mA",	nil,
};

Item	bootinfoitems[] = {
	"undefined",	6,	Integer,	-1,	"%llux",		nil,
	"status",		10,	Uchar,	-1,	"%.10H",	nil,
};

Item	memerr64items[] = {
	"type",		1,	Integer,	-1,	"%llud",		nil,
	"gran",		1,	Integer,	-1,	"%llud",		nil,
	"operation",	1,	Integer,	-1,	"%llud",		nil,
	"synrdome",	4,	Integer,	-1,	"%.8llux",	nil,

	"addr",		8,	Integer,	-1,	"%.16llux",	nil,
	"devaddr",	8,	Integer,	-1,	"%.16llux",	nil,
	"resolution",	4,	Integer,	-1,	"%.8llux",	nil,
};

Enum mgmtaddrtype[] = {
	1,	"other",
	2,	"unknown",
	3,	"i/o port",
	4,	"memory",
	5,	"smbus",
	0,	nil,
};

Item	mgmtitems[] = {
	"desc",		1,	String,	-1,	"%s",		nil,
	"type",		1,	Integer,	-1,	"%lld",		nil,
	"addr",		4,	Integer,	-1,	"%#.8llx",	nil,
	"addrtype",	1,	Enumf,	-1,	"%s",		mgmtaddrtype,
};

Item	mgmtcitems[] = {
	"desc",		1,	String,	-1,	"%s",		nil,
	"devhand",	2,	Integer,	-1,	"%.4llux",	nil,
	"cmphand",	2,	Integer,	-1,	"%.4llux",	nil,
	"thand",		2,	Integer,	-1,	"%.4llux",	nil,
};

Item	mgmtdtitems[] = {
	"low thresh",	2,	Integer,	-1,	"%.4llud",	nil,
	"hi thresh",	2,	Integer,	-1,	"%.4llud",	nil,
	"low crit thresh",	2,	Integer,	-1,	"%.4llud",	nil,
	"hi crit thresh",	2,	Integer,	-1,	"%.4llud",	nil,
	"low nr thresh",	2,	Integer,	-1,	"%.4llud",	nil,
	"hi nr thresh",	2,	Integer,	-1,	"%.4llud",	nil,
};

Item	memchanitems[] = {
	"type",		1,	Integer,	-1,	"%llud",		nil,
	"maxload",	1,	Integer,	-1,	"%llud",		nil,
	"devcnt",		1,	Integer,	-1,	"%llud",		nil,
	"load1",		1,	Integer,	-1,	"%llud",		nil,
	"hndl1",		2,	Integer,	-1,	"%.4llux",	nil,
};

Item	ipmiitems[] = {
	"type",		1,	Integer,	-1,	"%llud",		nil,
	"version",	1,	Integer,	-1,	"%#.2llux",	nil,
	"i²c slaveaddr",	1,	Integer,	-1,	"%#.2llux",	nil,
	"nvstoraddr",	1,	Integer,	-1,	"%#.2llux",	nil,
	"pa",		8,	Integer,	-1,	"%#.16llux",	nil,
	"pa modify",	1,	Integer,	-1,	"%#.2llux",	nil,
	"irq#",		1,	Integer,	-1,	"%#.2llux",	nil,
};

Item	psitems[] = {
	"pwrgrp",	1,	Integer,	-1,	"%llud",		nil,

	"loc",		1,	String,	-1,	"%s",		nil,
	"name",		1,	String,	-1,	"%s",		nil,
	"mfg",		1,	String,	-1,	"%s",		nil,
	"serial",		1,	String,	-1,	"%s",		nil,
	"assetag",	1,	String,	-1,	"%s",		nil,
	"part#",		1,	String,	-1,	"%s",		nil,
	"rev",		1,	String,	-1,	"%s",		nil,
	"pwrecap",	2,	Integer,	-1,	"%llud mW",	nil,
	"flags",		2,	Integer,	-1,	"%.4llux",	nil,
	"input v hdl",	2,	Integer,	-1,	"%.4llux",	nil,
	"fan hdl",	2,	Integer,	-1,	"%.4llux",	nil,
	"current hdl",	2,	Integer,	-1,	"%.4llux",	nil,
};

Item	obditems[] = {
	"name",		1,	String,	-1,	"%s",		nil,
	"type",		1,	Integer,	-1,	"%.2llux",	nil,
	"inst",		1,	Integer,	-1,	"%.2llux",	nil,
	"segment",	1,	Integer,	-1,	"%.2llux",	nil,
	"bus",		1,	Integer,	-1,	"%.2llux",	nil,
	"devfn",		1,	Integer,	-1,	"%.2llux",	nil,
};

typedef struct Itemtab Itemtab;
struct Itemtab {
	Item	*tab;
	int	n;
};

Itemtab	itemtab[] = {
[Biosinfo]	biositems,	nelem(biositems),
[Sysinfo]		sysitems,		nelem(sysitems),
[Module]	moditems,	nelem(moditems),
[Chassis]		chassisitems,	nelem(chassisitems),
[Cpu]		cpuitems,	nelem(cpuitems),
[Memctlr]	memctlritems,	nelem(memctlritems),
[Memstk]	memstkitems,	nelem(memstkitems),
[Cache]		cacheitems,	nelem(cacheitems),
[Conn]		connitems,	nelem(connitems),
[Slot]		slotitems,	nelem(slotitems),
[Oemstr]		oemitems,	nelem(oemitems),
[Sysconf]	sysconfitems,	nelem(sysconfitems),
[Bioslang]	bioslangitems,	nelem(bioslangitems),
[Syslog]		syslogitems,	nelem(syslogitems),
[Pmem]		pmemitems,	nelem(pmemitems),
[Mdev]		mdevitems,	nelem(mdevitems),
[Merror]		merroritems,	nelem(merroritems),
[Mmap]		mmapitems,	nelem(mmapitems),
[Mmdev]		mmdevitems,	nelem(mmdevitems),
[Pointer]		ptritems,		nelem(ptritems),
[Battery]		batteryitems,	nelem(batteryitems),
[Wdog]		wdogitems,	nelem(wdogitems),
[Hwsec]		hwsecitems,	nelem(hwsecitems),
[Syspwr]		syspwritems,	nelem(syspwritems),
[Vprobe]	vprobeitems,	nelem(vprobeitems),
[Fan]		fanitems,	nelem(fanitems),
[Tprobe]		tprobeitems,	nelem(tprobeitems),
[Cprobe]		cprobeitems,	nelem(cprobeitems),
[Bootinfo]	bootinfoitems,	nelem(bootinfoitems),
[Merror64]	memerr64items,	nelem(memerr64items),
[Mgmt]		mgmtitems,	nelem(mgmtitems),
[Mgmtc]		mgmtcitems,	nelem(mgmtcitems),
[Mgmtdt]	mgmtdtitems,	nelem(mgmtdtitems),
[Memch]		memchanitems,	nelem(memchanitems),
[Ipmi]		ipmiitems,	nelem(ipmiitems),
[Ps]		psitems,		nelem(psitems),
[Obd]		obditems,	nelem(obditems),
};

void
setoffsets(Item *tab, int n)
{
	int i, o;

	o = 4;
	for(i = 0; i < n; i++){
		tab[i].offset = o;
		o += tab[i].nbytes;
	}
}

void
tabinit(void)
{
	int i;

	for(i = 0; i < nelem(itemtab); i++)
		setoffsets(itemtab[i].tab, itemtab[i].n);
}

char*
getstring(uchar *p, int i, uchar *e)
{
	uchar *q;

	if(i == 0)
		return nil;
	for(;; i--){
		if(i == 1)
			break;
		p = memchr(p, 0, e - p - 1);
		if(p == nil)
			sysfatal("bad strings");	// return nil;
		p++;
	}
	q = memchr(p, 0, e - p - 1);
	if(q == nil || q-p == 0)
		return nil;
	return strdup((char*)p);
}

void
crack(uchar *p, uchar *e, Item *tabi, Itemv *tabv, int n)
{
	int i, len;
	Item *t;
	Itemv *v;

	len = p[1];
	for(i = 0; i < n; i++){
		t = tabi + i;
		v = tabv + i;

		if(p + t->offset + t->nbytes >= p+len){
			/* cheater way to protect against new std. versions */
			continue;
		}

		switch(t->type){
		default:
			sysfatal("bad type");
		case String:
			v->s = getstring(p + len, p[t->offset], e);
			break;
		case Enumf:
		case Enumbv:
		case Integer:
			if(p + t->offset + t->nbytes >= e)
				sysfatal("range");
			v->i = getle(p + t->offset, t->nbytes);
			break;
		case Uchar:
			if(p + t->offset + t->nbytes >= e)
				sysfatal("range");
			memcpy(v->uuid, p + t->offset, t->nbytes);
			break;
		}
	}
}

void
dumpenum(Enum *p)
{
	for(; p->s != nil; p++)
		Bprint(out, "%s	%d\n", p->s, p->v);
}

char*
enumtostr(Enum *p, uvlong i)
{
	for(; p->s != nil; p++)
		if(i == p->v)
			return p->s;
	return nil;
}

int
strtoenum(Enum *p, char *s)
{
	for(; p->s != nil; p++)
		if(cistrcmp(s, p->s) == 0)
			return p->v;
	return -1;
}

char*
enumbvtostr(Enum *p, uint i, char *s0, char *e)
{
	char *s;

	s = s0;
	for(; p->s != nil; p++)
		if(i & p->v){
			if(s != s0)
				s = seprint(s, e, " ");
			s = seprint(s, e, "%s", p->s);
		}
	if(s != s0)
		return s0;
	return nil;
}

void
printitems(int type, Item *tabi, Itemv *tabv, int n)
{
	char *s, buf[128];
	int i;
	Item *t;
	Itemv *v;

//	Bprint(out, "type %d\n", type);
	USED(type);
	for(i = 0; i < n; i++){
		t = tabi + i;
		v = tabv + i;
		Bprint(out, "	%-12s	", t->name);
		switch(t->type){
		default:
			sysfatal("bad type");
		case String:
			Bprint(out, t->fmt, v->s);
			break;
		case Integer:
			Bprint(out, t->fmt, v->i);
			break;
		case Uchar:
			Bprint(out, t->fmt, v->uuid);
			break;
		case Enumf:
			s = enumtostr(t->tab, v->i);
			if(s != nil){
				Bprint(out, t->fmt, s);
				Bprint(out, " (%#llux)", v->i);
			}
			else
				Bprint(out, "%lld", v->i);
			break;
		case Enumbv:
			s = enumbvtostr(t->tab, v->i, buf, buf+sizeof buf);
			if(s != nil){
				Bprint(out, t->fmt, s);
				Bprint(out, " (%#llux)", v->i);
			}
			else
				Bprint(out, "%lld", v->i);
			break;
		}
		Bprint(out, "\n");
	}
}

uchar*
next(uchar *p, uchar *e)
{
	int ns;

	p += p[1];
	if(p >= e)
		sysfatal("structures out-of-bounds");

	for(ns = 0;; ns++){
		p = memchr(p, 0, e - p - 1);
		if(p == nil)
			sysfatal("bad strings");
		p++;
		if(p[0] == 0){
			p++;
			break;
		}
	}
	USED(ns);
	return p;
}

Sm*
finddmi(void)
{
	uchar *buf, *p, *e;
	int i, len;
	Sm *sm;
	Smval *val;

	realmodefd();
	buf = emalloc(Dmisz);
	if(pread(rfd, buf, Dmisz, Dmistart) != Dmisz)
		sysfatal("read: %r");

	e = buf + Dmisz;
	for(p = buf; p < e; p += 16)
		if(memcmp(p, "_SM_", 4) == 0)
			goto found;
	sysfatal("no dmi structure");
found:
	Bvprint(out, "found at %#lux\n", p-buf+Dmistart);
	len = p[5];
	/* do checksums */

	sm = emalloc(len);
	sm->raw = *(Smbuf*)p;
	Bvprint(out, "dmi %ud.%ud\n", sm->raw.vers[0], sm->raw.vers[1]);

	sm->stpa = getle(sm->raw.stpa, 4);
	sm->stlen = getle(sm->raw.stlen, 2);
	Bvprint(out, "structure pa %llux len %d count %d\n", sm->stpa, sm->stlen, (uint)getle(sm->raw.nstrut, 2));

	sm->n = getle(sm->raw.nstrut, 2);
	sm->v = emalloc(sm->n*sizeof sm->v[0]);

	free(buf);
	buf = emalloc(sm->stlen);
	if(pread(rfd, buf, sm->stlen, sm->stpa) != sm->stlen)
		sysfatal("read: %r");
	p = buf;
	e = buf + sm->stlen;
	for(i = 0; i < sm->n; i++){
		val = sm->v + i;
		val->type = p[0];
		val->hand = getle(p+2, 2);
		val->len = p[1];
		if(val->type < nelem(itemtab)){
			val->v = emalloc(itemtab[val->type].n*sizeof val->v[0]);
			val->i = itemtab[val->type].tab;
			crack(p, e, val->i, val->v, itemtab[val->type].n);
		}
		p = next(p, e);
	}
	return sm;
}

void
printdmi(Sm *sm)
{
	int i;
	Smval *val;

	for(i = 0; i < sm->n; i++){
		val = sm->v + i;
		if(prtype != -1 && val->type != prtype)
			continue;
		if(prhand != -1 && val->hand != prhand)
			continue;
		Bprint(out, "%d: type %s %ud len %ud handle %ud\n",
			i, enumtostr(tabletab, val->type), val->type, val->len, val->hand);
		if(val->type < nelem(itemtab))
			printitems(val->type, itemtab[val->type].tab, val->v, itemtab[val->type].n);
	}
}

Smval*
handtosmval(Sm *sm, int hand)
{
	int i;
	Smval *val;

	for(i = 0; i < sm->n; i++){
		val = sm->v + i;
		if(val->hand == hand)
			return val;
	}
	return nil;
}

Itemv*
getval(char *name, Smval *v, int n)
{
	int i;

	for(i = 0; i < n; i++)
		if(cistrcmp(name, v->i[i].name) == 0)
			return v->v + i;
	return nil;
}

void
memhack(Sm *sm)
{
	int i, cnt;
	uvlong start, end;
	Itemv *v;
	Smval *val, *mem;

	for(i = 0; i < sm->n; i++){
		val = sm->v + i;
		if(val->type != Mmdev)
			continue;
		start = 0;
		end = 0;
		cnt = itemtab[val->type].n;
		v = getval("start", val, cnt);
		if(v != nil && v->i != 0xffffffff)
			start = (v->i+0)*1024;
		else{
			v = getval("xstart", val, cnt);
			if(v != nil)
				start = v->i;
		}

		v = getval("end", val, cnt);
		if(v != nil && v->i != 0xffffffff){
			end = v->i;
			if((end & 0xffff) == 0xffff)
				end++;
			end *= 1024;
		}else if((v = getval("xend", val, cnt)) != nil){
			end = v->i;
			if((end & 0xffff) == 0xffff)
				end++;
		}

//		if(end == 0)
//			continue;

		mem = handtosmval(sm, getval("dhand", val, cnt)->i);
		if(mem == nil)
			continue;
		Bprint(out, "%#.16llux	%#.16llux	"
			"%-15s	%-20s"
			"\n",
			start, end,
			getval("mfgr", mem, itemtab[Mdev].n)->s, 
		//	getval("serial", mem, itemtab[Mdev].n)->s
			getval("part#", mem, itemtab[Mdev].n)->s
			);
	}
}

fmtℬ(Fmt *f)
{
	int i;
	uvlong v, t, c, m;

	v = va_arg(f->args, uvlong);
	m = 1;
	t = 0;
	for(i = 0; i < 2*sizeof v; i++){
		c = v>>4*i & 0xf;
		t += c*m;
		m *= 10;
	}
	return fmtprint(f, "%lld", t);
}

/* cmos address i/o ports 0x70/0x71 */
Item type1header[] = {
	"oem res",	5,	Uchar,	-1,	"%.5H",		nil,
	"mewindow",	1,	Integer,	-1,	"%ℬ",		nil,
	"meincr",		1,	Integer,	-1,	"%lld",		nil,
	"logclearadr",	1,	Integer,	-1,	"%#llux",	nil,
	"logclearbit",	1,	Integer,	-1,	"%#llux",	nil,
	"cmoscksumstart",	1,	Integer,	-1,	"%#llux",	nil,
	"cmoscksumcnt",	1,	Integer,	-1,	"%#llux",	nil,
	"cmoscksumadr",	1,	Integer,	-1,	"%#llux",	nil,
	"reserved",	3,	Integer,	-1,	"%#llux",	nil,
	"headerrev",	1,	Integer,	-1,	"%llud",		nil,
};

Enum	logtype[] = {
	0,	"reserved",
	1,	"single-bit ecc error",
	2,	"multi-bit ecc error",
	3,	"parity memory error",
	4,	"bus timeout",
	5,	"io channel check",
	6,	"software nmi",
	7,	"post memory resize",
	8,	"post error",
	9,	"pci parity error",
	0xa,	"pci system error",
	0xb,	"cpu failure",
	0xc,	"eisa failsafe timer timeout",
	0xd,	"correctable memory log disabled",
	0xe,	"loging disabled (log spam)",
	0xf,	"reserved (0xf)",
	0x10,	"system physical limits exceeded",
	0x11,	"watchdog reset",
	0x12,	"system configuration info",
	0x13,	"harddrive info",
	0x14,	"system reconfigured",
	0x15,	"uncorrectable cpu-complex error",
	0x16,	"log cleared",
	0x17,	"system boot",
	0x17,	"system boot",
};

Enum	varliabledatafmt[] = {
	0,	"none",
	1,	"handle",		/* 2 bytes */
	2,	"multiple-event",		/* 2 bytes */
	3,	"multiple-event handle",	/* 4 bytes */
	4,	"post results bitmap",	/* 8 bytes */
	5,	"system mgmt",		/* 4 bytes */
	6,	"multiple-event sm",	/* 8 bytes */
};

Item logdesc[] = {
	"type",		1,	Integer,	-1,	"%lld",	nil,
	"datafmt",	1,	Integer,	-1,	"%lld",	nil,
};

Smval*
smmatch(Sm *sm, Smval *val, int type)
{
	int i;

	if(val == nil)
		i = 0;
	else
		i = val - sm->v;
	for(; i < sm->n; i++){
		val = sm->v + i;
		if(val->type != type)
			continue;
		return val;
	}
	return nil;
}

typedef struct	Logfn	Logfn;
typedef struct	Log	Log;

struct Logfn {
	char	*name;
	int	(*open)(Log*);
	void	(*close)(Log*);
	int	(*read)(Log*, void*, int, uvlong);
	int	(*write)(Log*, void*, int, uvlong);
};

struct Log {
	int	fd;
	Logfn	*l;

	int	valid;
	int	format;
	uint	len;
	uvlong	hdroffset;
	uvlong	dataoffset;
	uvlong	pa;
	int	access;
};

enum {
	Biosmem0	= 0xff000000,
	Biosmemsz	= 16*1024*1024,
};

int
mmioopen(Log *l)
{
	if(l->pa >= 1ull<<32 || l->pa < Biosmem0){
		werrstr("mmio range: %#llux", l->pa);
		return -1;
	}
	if(l->len > Biosmemsz){
		werrstr("mmio size: %#ux", l->len);
		return -1;
	}
	return open("/dev/resmem", OREAD);
}

void
mmioclose(Log *l)
{
	close(l->fd);
}

int
mmioread(Log *l, void *a, int n, uvlong o)
{
	return pread(l->fd, a, n, o+l->pa);
}

int
mmiowrite(Log *l, void *a, int n, uvlong o)
{
	return pwrite(l->fd, a, n, o+l->pa);
}

Logfn	logfntab[] = {
[3]	{"mmio",		mmioopen,	mmioclose,	mmioread,	mmiowrite,	},
};

uvlong
getv(Smval *val, int cnt, char *s, uvlong dflt)
{
	Itemv *v;

	v = getval(s, val, cnt);
	if(v != nil)
		return v->i;
	return dflt;
}

typedef struct Loge Loge;
struct Loge {
	uchar	type;
	uchar	len;
	uchar	bcddate[6];
};

void
loghack(Sm *sm)
{
	int cnt;
	Smval *val;
	Log log;
	Loge e;
	uintmem o;

	val = smmatch(sm, nil, Syslog);
	if(val == nil)
		return;
	cnt = itemtab[val->type].n;

	memset(&log, 0, sizeof log);
	log.valid = getv(val, cnt, "status", 0) & 1;
	log.len = getv(val, cnt, "loglength", 0);
	log.hdroffset = getv(val, cnt, "hdroffset", 0);
	log.dataoffset = getv(val, cnt, "dataoffset", 0);
	log.pa = getv(val, cnt, "pa", 0);
	log.access = getv(val, cnt, "access", -1);
	log.format = getv(val, cnt, "format", -1);

	if(log.valid == 0)
		sysfatal("log invalid %lld", getv(val, cnt, "status", 0));
	if((uint)log.access >= nelem(logfntab) || logfntab[log.access].write == nil)
		sysfatal("access method %d not supported", log.access);
	if(log.format != 1)
		sysfatal("unknown log hdr format %d", log.format);

	log.l = logfntab + log.access;

	print("access method %s\n", log.l->name);

	print("%lld %lld\n", getval("ndesc", val, cnt)->i, getval("desclen", val, cnt)->i);

	if((log.fd = log.l->open(&log)) == -1)
		sysfatal("can't open log: %r");

	char buf[32];

	if(log.l->read(&log, buf, 16, log.hdroffset) != 16)
		sysfatal("read: %r");
	print("%.16H\n", buf);

	for(o = log.dataoffset;; o += e.len){
		memset(&e, 0, sizeof e);
		log.l->read(&log, buf, 8, o);
		e.type = buf[0];
		if(e.type == 0xff)
			break;
		e.len = buf[1] & ~0x80;
		if(e.len < 8)
			break;
		memcpy(e.bcddate, buf+2, 6);

		print("type %.2ux len %d date %#.6H\n", e.type, e.len, e.bcddate);

//		log.l->read(&log, buf, e.len, o);
//		print("data %.*H", e.len-8, buf+8);
	}

	log.l->close(&log);
}

void
outinit(void)
{
	if(Binit(&outsb, 1, OWRITE) == -1)
		sysfatal("Binit: %r");
	out = &outsb;
}

void
outterm(void)
{
	Bterm(out);
	out = nil;
}

void
usage(void)
{
	fprint(2, "usage: dmi [-vmls] [-t type] [-h hand]\n");
	exits("usage");
}

void
main(int argc, char **argv)
{
	char *s, *p;
	Sm *sm;
	void (*f)(Sm*);

	fmtinstall(L'ℛ', fmtℛ);
	fmtinstall(L'𝒮', fmt𝒮);
	fmtinstall(L'×', fmt×);
	fmtinstall(L'ℬ', fmtℬ);		/* bcd */
	fmtinstall('H', encodefmt);
	tabinit();
	outinit();
	f = printdmi;
	ARGBEGIN{
	case 'v':
		verbose = 1;
		break;
	case 't':
		s = EARGF(usage());
		prtype = strtoul(s, &p, 0);
		if(p == s)
			prtype = strtoenum(tabletab, s);
		if(prtype == -1)
			sysfatal("bad type");
		break;
	case 'h':
		prhand = strtoul(EARGF(usage()), &p, 0);
		break;
	case 'l':
		dumpenum(tabletab);
		outterm();
		exits("");
	case 'm':
		f = memhack;
		break;
	case 's':
		f = loghack;
		break;
	default:
		usage();
	}ARGEND
	if(argc != 0)
		usage();

	sm = finddmi();
	f(sm);
	outterm();
	exits("");
}
