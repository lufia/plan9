#include <u.h>
#include <libc.h>
#include <libsec.h>

extern int tsmemcmp(void*, void*, ulong);

static void
load128(uchar b[16], ulong w[4])
{
	w[0] = (ulong)b[15] | (ulong)b[14]<<8 | (ulong)b[13]<<16 | (ulong)b[12]<<24;
	w[1] = (ulong)b[11] | (ulong)b[10]<<8 | (ulong)b[9]<<16 | (ulong)b[8]<<24;
	w[2] = (ulong)b[7] | (ulong)b[6]<<8 | (ulong)b[5]<<16 | (ulong)b[4]<<24;
	w[3] = (ulong)b[3] | (ulong)b[2]<<8 | (ulong)b[1]<<16 | (ulong)b[0]<<24;
}

static void
store128(ulong w[4], uchar b[16])
{
	ulong *e;

	b += 16;
	e = w+4;
	for(e = w+4; w < e; w++){
		*--b = *w;
		*--b = *w>>8;
		*--b = *w>>16;
		*--b = *w>>24;
	}
}

static void
gfmul(ulong x[4], ulong y[4], ulong z[4])
{
	long m, i;

	z[0] = z[1] = z[2] = z[3] = 0;
	for(i = 127; i >= 0; i--){
		m = ((long)y[i>>5] << 31-(i&31)) >> 31;
		z[0] ^= x[0] & m;
		z[1] ^= x[1] & m;
		z[2] ^= x[2] & m;
		z[3] ^= x[3] & m;
		m = ((long)x[0]<<31) >> 31;
		x[0] = x[0]>>1 | x[1]<<31;
		x[1] = x[1]>>1 | x[2]<<31;
		x[2] = x[2]>>1 | x[3]<<31;
		x[3] = x[3]>>1 ^ (0xe1000000 & m);
	}
}

static void
prepareM(ulong h[4], ulong m[16][256][4])
{
	ulong x[4], i, j;

	for(i = 0; i < nelem(m); i++)
		for(j = 0; j < nelem(m[i]); j++){
			x[0] = x[1] = x[2] = x[3] = 0;
			x[i>>2] = j<<((i&3)<<3);
			gfmul(x, h, m[i][j]);
		}
}

static void
ghash1(AESGCMstate *s, ulong x[4], ulong y[4])
{
	ulong *xi, i;

	x[0] ^= y[0];
	x[1] ^= y[1];
	x[2] ^= y[2];
	x[3] ^= y[3];

	y[0] = y[1] = y[2] = y[3] = 0;
	for(i = 0; i < 16; i++){
		xi = s->M[i][(x[i>>2]>>((i&3)<<3))&0xff];
		y[0] ^= xi[0];
		y[1] ^= xi[1];
		y[2] ^= xi[2];
		y[3] ^= xi[3];
	}
}

static void
ghashn(AESGCMstate *s, uchar *p, ulong len, ulong y[4])
{
	uchar tmp[16];
	ulong x[4];

	while(len >= 16){
		load128(p, x);
		ghash1(s, x, y);
		p += 16;
		len -= 16;
	}
	if(len > 0){
		memmove(tmp, p, len);
		memset(tmp+len, 0, 16-len);
		load128(tmp, x);
		ghash1(s, x, y);
	}
}

static ulong
aesxctr1(AESstate *s, uchar ctr[AESbsize], uchar *p, ulong len)
{
	uchar tmp[AESbsize];
	ulong i;

	aes_encrypt(s->ekey, s->rounds, ctr, tmp);
	if(len > AESbsize)
		len = AESbsize;
	for(i = 0; i < len; i++)
		p[i] ^= tmp[i];
	return len;
}

static void
aesxctrn(AESstate *s, uchar *p, ulong len)
{
	uchar ctr[AESbsize];
	ulong i;

	memmove(ctr, s->ivec, AESbsize);
	while(len > 0){
		for(i = AESbsize-1; i >= AESbsize-4; i--)
			if(++ctr[i] != 0)
				break;
		if(aesxctr1(s, ctr, p, len) < AESbsize)
			break;
		p += AESbsize;
		len -= AESbsize;
	}
}

void
aesgcm_setiv(AESGCMstate *s, uchar *iv, int ivlen)
{
	ulong l[4], y[4];

	if(ivlen == 96/8){
		memmove(s->ivec, iv, ivlen);
		memset(s->ivec+ivlen, 0, AESbsize-ivlen);
		s->ivec[AESbsize-1] = 1;
		return;
	}
	memset(y, 0, sizeof y);
	ghashn(s, iv, ivlen, y);
	l[0] = ivlen << 3;
	l[1] = ivlen >> 29;
	l[2] = l[3] = 0;
	ghash1(s, l, y);
	store128(y, s->ivec);
}

void
setupAESGCMstate(AESGCMstate *s, uchar *key, int keylen, uchar *iv, int ivlen)
{
	setupAESstate(s, key, keylen, nil);

	memset(s->ivec, 0, AESbsize);
	aes_encrypt(s->ekey, s->rounds, s->ivec, s->ivec);
	load128(s->ivec, s->H);
	memset(s->ivec, 0, AESbsize);
	prepareM(s->H, s->M);

	if(iv != nil && ivlen > 0)
		aesgcm_setiv(s, iv, ivlen);
}

void
aesgcm_encrypt(uchar *p, ulong n, uchar *a, ulong na, uchar tag[16], AESGCMstate *s)
{
	ulong l[4], y[4];

	memset(y, 0, sizeof y);
	ghashn(s, a, na, y);
	aesxctrn(s, p, n);
	ghashn(s, p, n, y);
	l[0] = n<<3;
	l[1] = n>>29;
	l[2] = na<<3;
	l[3] = na>>29;
	ghash1(s, l, y);
	store128(y, tag);
	aesxctr1(s, s->ivec, tag, 16);
}

int
aesgcm_decrypt(uchar *p, ulong n, uchar *a, ulong na, uchar tag[16], AESGCMstate *s)
{
	ulong l[4], y[4];
	uchar tmp[16];

	memset(y, 0, sizeof y);
	ghashn(s, a, na, y);
	ghash(s, p, n, y);
	l[0] = n<<3;
	l[1] = n>>29;
	l[2] = na<<3;
	l[3] = na>>29;
	ghash1(s, l, y);
	store128(y, tmp);
	aesxctr1(s, s->ivec, tmp, 16);
	if(tsmemcmp(tag, tmp, 16) != 0)
		return -1;
	aesxctrn(s, p, n);
	return 0;
}
