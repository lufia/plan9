/* NVRAM emulation with i2c EEPROM on lpt port	*/
/* (c) 2005 Sergey Reva (rs_rlab@mail.ru)		*/
/* Hardware part try find in http://rs-rlab.narod.ru	*/
/* provide "AS IS", with NO WARRANTIES		*/

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"../port/error.h"

//Where this macros should be?
#define HIBYTE(x) ((x>>8)&0x00FF)
#define LOBYTE(x) ((x)&0x00FF)

enum
{
						//Files
	Qdir=		0x8000,	//dir
	Qdata=		0x0,		//data file - i2c
						//Bits
	POWER = 		0x02,	//Power ctl bit in data register
	SDAIN = 		0x20,	// in status register
	SDAOUT = 	0x01,	// in data register
	SCL=			0x01,	// in ctl register

	dataport=		0x378,	//Data register (for SDA and POWER pin)
						//lpt1 - 0x378
						//lpt2 - 0x278
						//lpt3 - 0x3bc
	statport,				//Status register (for SDA pin)
	ctlport,				//Ctrl register (for SCL pin)

	romsize=		32768,	// IC type - Size
						// 24c512 - 65536	24c64 - 8192
						// 24c256 - 32768	24c32 - 4096
						// 24c16 - 2048	24c08 - 1024
						// 24c04 - 512	24c02 - 256
						// 24c01 - 128
};

Dirtab i2cdir[]={
	".",		{Qdir, 0, QTDIR},	0,		DMDIR|0555,
	"i2c",	{Qdata},			romsize,	0666,
};

static void scl(uchar val)
{
	if (val)
		outb(ctlport,0);
	else
		outb(ctlport,SCL);

	microdelay(5);
}

static uchar sda(uchar val)
{
	if (val)
		outb(dataport,SDAOUT|POWER);
	else
		outb(dataport,POWER);

	microdelay(5);

	return inb(statport) & SDAIN;
}

static void i2cstart(void)
{
	sda(1);
	scl(1);
	sda(0);
	scl(0);
}

static void i2cstop(void)
{
	sda(0);
	scl(1);
	sda(1);
}

static uchar read8bit(void)
{
	int n;
	uchar data=0;
	
	for(n=0;n<8;n++)
	{
		scl(1);
		if (sda(1))
			data|=0x80>>n;
		scl(0);
	}
	
	sda(1);
	scl(1);
	scl(0);

	return data;
}

static void write8bit(uchar data)
{
	int n;

	for(n=0;n<8;n++)
	{
		if ((data<<n) & 0x80)
			sda(1);
		else
			sda(0);

		scl(1);
		scl(0);
	}

	sda(1);
	scl(1);
	scl(0);
}

static uchar readi2c(ushort addr)
{
	uchar data;

	i2cstart();
	write8bit(0xA0);
	if (romsize>256)
		write8bit(HIBYTE(addr));
	write8bit(LOBYTE(addr));

	i2cstart();
	write8bit(0xA1);
	data=read8bit();
	i2cstop();
	return data;
}

static void writei2c(ushort addr,uchar data)
{
	i2cstart();
	write8bit(0xA0);

	if (romsize>256)
		write8bit(HIBYTE(addr));
	write8bit(LOBYTE(addr));

	write8bit(data);
	i2cstop();

	delay(20);	// 15 ms typical delay
			// but I want be sure...
}


static Chan* i2cattach(char *spec)
{
	Chan *c;
	c = devattach('2', spec);
	c->qid.path = Qdir;
	return c;
}

static Walkqid* i2cwalk(Chan *c, Chan *nc, char **name, int nname)
{
	return devwalk(c, nc, name, nname, i2cdir, nelem(i2cdir), devgen);
}

static int i2cstat(Chan *c, uchar *dp, int n)
{
	return devstat(c, dp, n, i2cdir, nelem(i2cdir), devgen);
}

static Chan* i2copen(Chan *c, int omode)
{
	return devopen(c, omode, i2cdir, nelem(i2cdir), devgen);
}

static void i2cclose(Chan *c)
{
	if(c->qid.path == Qdata)
	{
		scl(1);
		sda(1);
	}
}

static long i2cread(Chan *c, void *a, long n, vlong offset)
{
	ulong o;
	uchar *d=a;

	if (c->qid.path==Qdir)
		return devdirread(c, a, n, i2cdir, nelem(i2cdir), devgen);

	if (c->qid.path!=Qdata)
		return 0;

	if (offset>=romsize)
		return 0;

	for(o=0;o<n;o++)
	{
		if (offset+o>=romsize)
			return o-1;
		d[o]=readi2c(offset+o);
	}
	return o;
}

static long i2cwrite(Chan *c, void *a, long n, vlong offset)
{
	ulong o;
	uchar *d=a;

	if(c->qid.path == Qdata)
	{

		if (offset>=romsize)
			return 0;

		for(o=0;o<n;o++)
		{
			if (offset+o>=romsize)
				return o-1;
			writei2c(offset+o,d[o]);
		}
		return o;
	}

	return 0;
}

void i2cinit(void)
{
	sda(1);
	scl(1);

	if (ioalloc(dataport,7,0,"lpt-i2c")<0)
		print("#2: cannot ioalloc range 0x%X+0x07\n",dataport);
}

Dev i2cdevtab = {
	'2',
	"i2c",

	devreset,
	i2cinit,
	devshutdown,
	i2cattach,
	i2cwalk,
	i2cstat,
	i2copen,
	devcreate,
	i2cclose,
	i2cread,
	devbread,
	i2cwrite,
	devbwrite,
	devremove,
	devwstat,
};
