#include <u.h>
#include <libc.h>

#define X86STEPPING(x)	((x) & 0x0F)
#define X86MODEL(x)	((x)>>4 & 0xf | (x)>>12 & 0xf0)
#define X86FAMILY(x)	((x)>>8 & 0xf | (x)>>16 & 0xf0)

enum {
	Highstdfunc 	= 0x0,		/* also returns vendor string */
	Procsig,
	Proctlbcache,
	Procserial,
	X2apic		= 0xb,

	/* Procsig.cx; doesn't work */
	Hasx2apic	= 1<<21,
};

typedef struct Cpuidreg Cpuidreg;

struct Cpuidreg {
	u32int	ax;
	u32int	bx;
	u32int	cx;
	u32int	dx;
};

void	cpuid1(Cpuidreg*);

char *fn1dx[32*2] = {
/* 0 */	"fpu",	"onboard x87",
/* 1 */	"vme",	"virtual mode extensions",
/* 2 */	"de",	"debugging extensions",
/* 3 */	"pse",	"page size extensions",
/* 4 */	"tsc",	"time stamp counter",
/* 5 */	"msr",	"model-specific registers",
/* 6 */	"pae",	"physical address extension",
/* 7 */	"mce",	"machine check exception",
/* 8 */	"cx8",	"CMPXCHGQ",
/* 9 */	"apic",	"onboard apic",
/* 10 */	"--",	"reserved",
/* 11 */	"sep",	"SYSENTER",
/* 12 */	"mtrr",	"memory type range registers",
/* 13 */	"pge",	"cr4 page global enable",
/* 14 */	"mca",	"machine check architecture",
/* 15 */	"cmov",	"conditional move",
/* 16 */	"pat",	"page attribute table",
/* 17 */	"pse36",	"36-bit huge pages",
/* 18 */	"pn",	"processor serial#",
/* 19 */	"clflush",	"CLFLUSH",
/* 20 */	"--",	"reserved",
/* 21 */	"dts",	"debug store",
/* 22 */	"acpi",	"onboard acpi thermal msrs",
/* 23 */	"mmx",	"mmx",
/* 24 */	"fxsr",	"FXSAVE",
/* 25 */	"sse",	"SSE",
/* 26 */	"sse2",	"SSE2",
/* 27 */	"ss",	"self-snoop",
/* 28 */	"htt",	"hyperthreading",
/* 29 */	"tm",	"automatic thermal throttle",
/* 30 */	"ia64",	"itanium emulating x86",
/* 31 */	"pbe",	"pending break enable",
};

char *fn1cx[32*2] = {
/* 0 */	"sse3",	"SSE3",
/* 1 */	"pclmulqdq",	"carryless multiplication",
/* 2 */	"dtes64",	"64-bit debug store",
/* 3 */	"mon",	"MONITOR",
/* 4 */	"dscpl",	"CPL qualified debug store",
/* 5 */	"vmx",	"virtual machine extensions",
/* 6 */	"smx",	"safer mode extensions",
/* 7 */	"est",	"enhanced speedstep",
/* 8 */	"tm2",	"thermal monitor 2",
/* 9 */	"ssse3",	"supplemntal SSE3",
/* 10 */	"cid",	"context id",
/* 11 */	"--",	"reserved",
/* 12 */	"fma",	"fused multiply-add",
/* 13 */	"cx16",	"CMPXCHGO",
/* 14 */	"xtpr",	"xtpr update control",
/* 15 */	"pdcm",	"perfmon and debug",
/* 16 */	"--",	"reserved",
/* 17 */	"pcid",	"process context identifiers",
/* 18 */	"dca",	"direct cache access dma write",
/* 19 */	"sse41",	"SSE 4.1",
/* 20 */	"sse42",	"SSE 4.2",
/* 21 */	"x2apic",	"x2apic",
/* 22 */	"movbe",	"atom(tm) big-endian mov",
/* 23 */	"popcnt",	"POPCNT",
/* 24 */	"tscdead",	"tsc deadline",
/* 25 */	"aes",	"AES",
/* 26 */	"xsave",	"XSAVE",
/* 27 */	"osxsave",	"os-enabled XSAVE",
/* 28 */	"avx",	"advanced vector etensions",
/* 29 */	"f16",	"half-precision fp",
/* 30 */	"rdrnd",	"RDRAND",
/* 31 */	"hyp",	"running on hypervisor",
};

char *fn80000001dx[32*2] = {
/* 0 */	"--",	"reserved",
/* 1 */	"--",	"reserved",
/* 2 */	"--",	"reserved",
/* 3 */	"--",	"reserved",
/* 4 */	"--",	"reserved",
/* 5 */	"--",	"reserved",
/* 6 */	"--",	"reserved",
/* 7 */	"--",	"reserved",
/* 8 */	"--",	"reserved",
/* 9 */	"--",	"reserved",
/* 10 */	"--",	"reserved",
/* 11 */	"syscall",	"syscall instruction",
/* 12 */	"--",	"reserved",
/* 13 */	"--",	"reserved",
/* 14 */	"--",	"reserved",
/* 15 */	"--",	"reserved",
/* 16 */	"--",	"reserved",
/* 17 */	"--",	"reserved",
/* 18 */	"--",	"reserved",
/* 19 */	"--",	"reserved",
/* 20 */	"nox",	"execute disable",
/* 21 */	"--",	"reserved",
/* 22 */	"--",	"reserved",
/* 23 */	"--",	"reserved",
/* 24 */	"--",	"reserved",
/* 25 */	"--",	"reserved",
/* 26 */	"gb",	"1gb pages",
/* 27 */	"rdtscp",	"RDTSCP supported",
/* 28 */	"--",	"reserved",
/* 29 */	"amd64",	"64-bit long mode supported",
/* 30 */	"--",	"reserved",
/* 31 */	"--",	"reserved",
};

char *intel80000001cx[32*2] = {
/* 0 */	"lahf64",	"LAHF in 64-bit mode",
/* 1 */	"--",	"reserved",
/* 2 */	"--",	"reserved",
/* 3 */	"--",	"reserved",
/* 4 */	"--",	"reserved",
/* 5 */	"--",	"reserved",
/* 6 */	"--",	"reserved",
/* 7 */	"--",	"reserved",
/* 8 */	"--",	"reserved",
/* 9 */	"--",	"reserved",
/* 10 */	"--",	"reserved",
/* 11 */	"--",	"reserved",
/* 12 */	"--",	"reserved",
/* 13 */	"--",	"reserved",
/* 14 */	"--",	"reserved",
/* 15 */	"--",	"reserved",
/* 16 */	"--",	"reserved",
/* 17 */	"--",	"reserved",
/* 18 */	"--",	"reserved",
/* 19 */	"--",	"reserved",
/* 20 */	"--",	"reserved",
/* 21 */	"--",	"reserved",
/* 22 */	"--",	"reserved",
/* 23 */	"--",	"reserved",
/* 24 */	"--",	"reserved",
/* 25 */	"--",	"reserved",
/* 26 */	"--",	"reserved",
/* 27 */	"--",	"reserved",
/* 28 */	"--",	"reserved",
/* 29 */	"--",	"reserved",
/* 30 */	"--",	"reserved",
/* 31 */	"--",	"reserved",
};

char *amd80000001cx[32*2] = {
/* 0 */	"lahf64",	"LAHF in 64-bit mode",
/* 1 */	"legacymp",	"legacy multicpre",
/* 2 */	"svm",	"secure virtual machine",
/* 3 */	"xapic",	"extended apic register",
/* 4 */	"altmovcr8",	"lock mov cr0 means mov cr8",
/* 5 */	"abm",	"advanced bit maniupation",
/* 6 */	"sse4a",	"sse4a support",
/* 7 */	"misalignsse",	"misaligned sse mode",
/* 8 */	"3dnowpref",	"prefetch(w) support",
/* 9 */	"osvw",	"os-visible workaround (apm)",
/* 10 */	"ibs",	"instruction-based sampling",
/* 11 */	"xop",	"extended operation support (apm6)",
/* 12 */	"skinit",	"skinit support",
/* 13 */	"wdt",	"watchdog timer support",
/* 14 */	"--",	"reserved",
/* 15 */	"lwp",	"lightweight profiling support",
/* 16 */	"fma4",	"4-op fma",
/* 17 */	"--",	"reserved",
/* 18 */	"--",	"reserved",
/* 19 */	"nodeid",	"support for msrc001_001c",
/* 20 */	"--",	"reserved",
/* 21 */	"tbm",	"trailing-bit manipulation",
/* 22 */	"--",	"reserved",
/* 23 */	"--",	"reserved",
/* 24 */	"--",	"reserved",
/* 25 */	"--",	"reserved",
/* 26 */	"--",	"reserved",
/* 27 */	"--",	"reserved",
/* 28 */	"--",	"reserved",
/* 29 */	"--",	"reserved",
/* 30 */	"--",	"reserved",
/* 31 */	"--",	"reserved",
};

void
decodetab(char *tab[32*2], uint r, int verbose)
{
	char buf[1024], *p, *e, *fmt;
	int i;

	fmt = "%s ";
	if(verbose)
		fmt = "\n\t%s";
	e = buf + sizeof buf;
	p = buf;
	for(i = 0; i < 32; i++)
		if(r & 1<<i)
			p = seprint(p, e, fmt, tab[2*i + verbose]);

	*p = 0;
	print("%s\n", buf);
}

void
fn1bx(uint r, int)
{
	char buf[1024], *p, *e;
	int i;

	e = buf + sizeof buf;
	p = buf;

	i = (r & 0xff00) >> 8;
	p = seprint(p, e, "clsize=%d ", i*8);
	i = (r & 0xff0000) >> 16;
	p = seprint(p, e, "maxapicid=%d ", i);
	i = (r & 0xff000000) >> 24;
	seprint(p, e, "initialapicid=%d ", i);

	print("%s\n", buf);
}

void
checkwired(int mach)
{
	char buf[128];
	int fd;

	for(;;){
		sleep(1);	
		fd = open("/dev/mach", OREAD);
		if(fd == -1)
			/* old kernel; just hope */
			return;
		buf[0] = ' ';
		buf[read(fd, buf, sizeof buf)] = 0;
		close(fd);
		if(atoi(buf) == mach)
			return;
		sysfatal("wire fails");
	}
}

void
procwired(int mach)
{
	char buf[128];
	int fd;

	snprint(buf, sizeof buf, "/proc/%d/ctl", getpid());
	fd = open(buf, OWRITE);
	if(fprint(fd, "wired %d", mach) < 0)
		sysfatal("procwired: %r");
	close(fd);
	checkwired(mach);	/* don't trust kernel yet */
}

int
sysmach(void)
{
	char buf[8192], *p;
	int n, fd;

	fd = open("/dev/sysstat", OREAD);
	if(fd == -1)
		sysfatal("open: %r");
	n = read(fd, buf, sizeof buf);
	if(n == -1)
		sysfatal("read: %r");
	close(fd);
	buf[n] = 0;
	p = buf;
	for(n = 0;; n++){
		if((p = strchr(p, '\n')) == nil)
			return n;
		p++;
	}
}

char*
append(char *s, uint r)
{
	memmove(s, &r, 4);
	return s+4;
}

char*
vendorstring(char *buf)
{
	char *p;
	Cpuidreg r;

	memset(&r, 0, sizeof r);
	r.ax = Highstdfunc;
	cpuid1(&r);
	p = buf;
	p = append(p, r.bx);
	p = append(p, r.dx);
	p = append(p, r.cx);
	*p = 0;
	return buf;
}

int
isamd(void)
{
	char buf[16];

	return strcmp(vendorstring(buf), "AuthenticAMD") == 0;
}

void
usage(void)
{
	fprint(2, "usage: cpuid [-w mach] [-efisvt] [-n fn]\n");
	exits("usage");
}

char *tab[] = {"ax", "bx", "cx", "dx", };
int
strtoidx(char *s)
{
	int i;

	for(i = 0; i < nelem(tab); i++)
		if(cistrncmp(s, tab[i], 2) == 0)
			return i;
	usage();
	return -1;
}

void
strtofn(char *s, Cpuidreg *r)
{
	char *p;
	uint *u, i, v;

	memset(r, 0, sizeof *r);
	u = (uint*)r;
	for(i = 0; i < 4; i++){
		if(strchr(s, '=') - s == 2){
			i = strtoidx(s);
			s += 3;
		}
		v = strtoul(s, &p, 0);
		u[i] = v;
		if(*p == 0)
			break;
		s = p+1;
	}
}

void
main(int argc, char **argv)
{
	char flag[32], buf[64], *p;
	uint mach[256], apic[256];
	int i, j, k, nmach, smach, nflag;
	Cpuidreg r, fn[32];

	nflag = 0;
	memset(mach, 0xff, sizeof mach);
	memset(fn, 0, sizeof fn);
	ARGBEGIN{
	case 'n':
		strtofn(EARGF(usage()), fn + nflag);
	case 'c':
	case 'e':
	case 'f':
	case 'i':
	case 's':
	case 'v':
	case 't':
		if(nflag == nelem(flag))
			sysfatal("too many flags");
		flag[nflag++] = ARGC();
		break;
	case 'w':
		mach[nflag] = strtoul(EARGF(usage()), 0, 0);
		break;
	default:
		usage();
	}ARGEND
	if(argc != 0)
		usage();
	if(nflag == 0)
		flag[nflag++] = 's';
	for(j = 0; j < nflag; j++){
		memset(&r, 0, sizeof r);
		if(mach[j] != ~0)
			procwired(mach[j]);
		switch(flag[j]){
		case 'n':
			r = fn[j];
			cpuid1(&r);
			print("%.8ux %.8ux %.8ux %.8ux\n",
				r.ax, r.bx, r.cx, r.dx);
			break;
		case 'c':
			memset(&r, 0, sizeof r);
			r.ax = 1;
			cpuid1(&r);
			print("bx\t");
			fn1bx(r.bx, 0);
			break;
		case 'e':
			memset(&r, 0, sizeof r);
			r.ax = 0x80000001;
			cpuid1(&r);
			print("dx\t");
			decodetab(fn80000001dx, r.dx, 0);
			print("cx\t");
			if(isamd())
				decodetab(amd80000001cx, r.cx, 0);
			else
				decodetab(intel80000001cx, r.cx, 0);
			break;
		case 'f':
			memset(&r, 0, sizeof r);
			r.ax = 1;
			cpuid1(&r);
			print("dx\t");
			decodetab(fn1dx, r.dx, 0);
			print("cx\t");
			decodetab(fn1cx, r.cx, 0);
			break;
		case 'i':
			p = buf;
			for(i = 0; i < 3; i++){
				memset(&r, 0, sizeof r);
				r.ax = 0x80000002+i;
				cpuid1(&r);
				p = append(p, r.ax);
				p = append(p, r.bx);
				p = append(p, r.cx);
				p = append(p, r.dx);
			}
			*p = 0;
			print("%s\n", buf);
			break;
		case 's':
			r.ax = Procsig;
			cpuid1(&r);
			print("%.8ux %.2ux.%.2ux.%.2ux\n",
				r.ax, X86FAMILY(r.ax), X86MODEL(r.ax),
				X86STEPPING(r.ax));
			break;
		case 'v':
			print("%s\n", vendorstring(buf));
			break;
		case 't':
			procwired(0);
			nmach = 1;
			for(i = 0;; i++){
				memset(&r, 0, sizeof r);
				r.ax = X2apic;
				r.cx = i;
				cpuid1(&r);
				if((r.cx & 0xff00) == 0)
					goto inval;
				switch(i){
				case 0:
					snprint(buf, sizeof buf, "thread");
					break;
				case 1:
					nmach = r.bx & 0xffff;
					snprint(buf, sizeof buf, "core");
					break;
				case 2:
					snprint(buf, sizeof buf, "pkg");
					break;
				default:
					snprint(buf, sizeof buf, "clust%d", i-3);
					break;
				}
				print("%s\t%.8ux %.8ux %.8ux %.8ux\n",
					buf, r.ax, r.bx, r.cx, r.dx);
			inval:
				if((r.ax == 0 || i == 0) && r.bx == 0)
					break;
			}
			smach = sysmach();
			if(smach%nmach == 0)
				nmach = smach;
			for(i = 0; i < nmach; i++){
				procwired(i);
				apic[i] = -1;
				memset(&r, 0, sizeof r);
				r.ax = X2apic;
				r.cx = 1;
				cpuid1(&r);
				if((r.cx & 0xff00) == 0)
					continue;
				apic[i] = r.dx;
				for(k = 0; k < i; k++)
					if(apic[k] == apic[i])
						goto dup;
				print("mach%d\tapic %.8ux\n", i, apic[i]);
			}
		dup:
			if(i != nmach)
				print("%d machs inactive\n", nmach - i);
			break;
		}
	}
	exits("");
}
