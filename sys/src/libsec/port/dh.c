#include <u.h>
#include <libc.h>
#include <mp.h>
#include <libsec.h>

mpint *
dh_new(DHstate *s, mpint *p, mpint *q, mpint *g)
{
	mpint *pm1;
	int n;

	memset(s, 0, sizeof *s);
	if(mpcmp(g, mpone) <= 0)
		return nil;

	n = mpsignif(p);
	pm1 = mpnew(n);
	mpsub(p, mpone, pm1);
	s->p = mpcopy(p);
	s->g = mpcopy(g);
	s->q = mpcopy(q != nil ? q : pm1);
	s->x = mpnew(npsignif(s->q));
	s->y = mpnew(n);
	for(;;){
		mpnrand(s->q, genrandom, s->x);
		mpexp(s->g, s->x, s->p, s->y);
		if(mpcmp(s->y, mpone) > 0 && mpcmp(s->y, pm1) < 0)
			break;
	}
	mpfree(pm1);
	return s->y;
}

mpint *
dh_finish(DHstate *s, mpint *y)
{
	mpint *k;

	k = nil;
	if(y == nil || s->x == nil || s->p == nil || s->q == nil)
		goto out;
	if(mpcmp(y, mpone) <= 0)
		goto out;
	k = mpnew(mpsignif(s->p));
	mpsub(s->p, mpone, k);
	if(mpcmp(y, k) >= 0){
	bad:
		mpfree(k);
		k = nil;
		goto out;
	}
	if(mpcmp(s->q, k) < 0){
		mpexp(y, s->q, s->p, k);
		if(mpcmp(k, mpone) != 0)
			goto bad;
	}

out:
	mpfree(s->p);
	mpfree(s->q);
	mpfree(s->g);
	mpfree(s->x);
	mpfree(s->y);
	memset(s, 0, sizeof *s);
	return k;
}
