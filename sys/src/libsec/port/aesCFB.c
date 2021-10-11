#include <u.h>
#include <libc.h>
#include <libsec.h>

void
aesCFBencrypt(uchar *p, int len, AESstate *s)
{
	int i;
	u32int a, o;

	o = s->offset;
	while(len > 0){
		if(o % 16){
		odd:
			a = s->ivec[o++ % 16] ^= *p;
			*p++ = a;
			len--;
			continue;
		}
		aes_encrypt(s->ekey, s->rounds, s->ivec, s->ivec);
		if(len < 16 || ((p-(uchar*)0) & 3) != 0)
			goto odd;
		((u32int*)p)[0] = (((u32int*)s->ivec)[0] ^= ((u32int*)p)[0]);
		((u32int*)p)[1] = (((u32int*)s->ivec)[1] ^= ((u32int*)p)[1]);
		((u32int*)p)[2] = (((u32int*)s->ivec)[2] ^= ((u32int*)p)[2]);
		((u32int*)p)[3] = (((u32int*)s->ivec)[3] ^= ((u32int*)p)[3]);
		o += 16;
		p += 16;
		len -= 16;
	}
	s->offset = o;
}

void
aesCFBdecrypt(uchar *p, int len, AESstate *s)
{
	int i;
	u32int a, o;

	o = s->offset;
	while(len > 0){
		if(o % 16){
		odd:
			a = *p;
			*p++ ^= s->ivec[o % 16];
			s->ivec[o++ % 16] = a;
			len--;
			continue;
		}
		aes_encrypt(s->ekey, s->rounds, s->ivec, s->ivec);
		if(len < 16 || ((p-(uchar*)0) & 3) != 0)
			goto odd;
		for(i = 0; i < 4; i++){
			a = ((u32int*)p)[i];
			((u32int*)p)[i] ^= ((u32int*)s->ivec)[i];
			((u32int*)s->ivec)[i] = a;
		}
		o += 16;
		p += 16;
		len -= 16;
	}
	s->offset = o;
}
