/* mpi-mul.c  -  MPI functions
 * Copyright (C) 1994, 1996, 1998, 2001, 2002,
 *               2003 Free Software Foundation, Inc.
 *
 * This file is part of Libgcrypt.
 *
 * Note: This code is heavily based on the GNU MP Library.
 *	 Actually it's the same code with only minor changes in the
 *	 way the data is stored; this is to support the abstraction
 *	 of an optional secure memory allocation which may be used
 *	 to avoid revealing of sensitive data due to paging etc.
 */

#include <linux/export.h>

#include "mpi-internal.h"

int mpi_mul(MPI w, MPI u, MPI v)
{
	mpi_size_t usize, vsize, wsize;
	mpi_ptr_t up, vp, wp;
	mpi_limb_t cy;
	int usign, vsign, sign_product;
	int assign_wp = 0;
	mpi_ptr_t tmp_limb = NULL;
	int err = 0;

	if (u->nlimbs < v->nlimbs) {
		/* Swap U and V. */
		usize = v->nlimbs;
		usign = v->sign;
		up    = v->d;
		vsize = u->nlimbs;
		vsign = u->sign;
		vp    = u->d;
	} else {
		usize = u->nlimbs;
		usign = u->sign;
		up    = u->d;
		vsize = v->nlimbs;
		vsign = v->sign;
		vp    = v->d;
	}
	sign_product = usign ^ vsign;
	wp = w->d;

	/* Ensure W has space enough to store the result.  */
	wsize = usize + vsize;
	if (w->alloced < wsize) {
		if (wp == up || wp == vp) {
			wp = mpi_alloc_limb_space(wsize);
			if (!wp)
				return -ENOMEM;
			assign_wp = 1;
		} else {
			err = mpi_resize(w, wsize);
			if (err)
				return err;
			wp = w->d;
		}
	} else { /* Make U and V not overlap with W.	*/
		if (wp == up) {
			/* W and U are identical.  Allocate temporary space for U. */
			up = tmp_limb = mpi_alloc_limb_space(usize);
			if (!up)
				return -ENOMEM;
			/* Is V identical too?  Keep it identical with U.  */
			if (wp == vp)
				vp = up;
			/* Copy to the temporary space.  */
			MPN_COPY(up, wp, usize);
		} else if (wp == vp) {
			/* W and V are identical.  Allocate temporary space for V. */
			vp = tmp_limb = mpi_alloc_limb_space(vsize);
			if (!vp)
				return -ENOMEM;
			/* Copy to the temporary space.  */
			MPN_COPY(vp, wp, vsize);
		}
	}

	if (!vsize)
		wsize = 0;
	else {
		err = mpihelp_mul(wp, up, usize, vp, vsize, &cy);
		if (err) {
			if (assign_wp)
				mpi_free_limb_space(wp);
			goto free_tmp_limb;
		}
		wsize -= cy ? 0:1;
	}

	if (assign_wp)
		mpi_assign_limb_space(w, wp, wsize);
	w->nlimbs = wsize;
	w->sign = sign_product;

free_tmp_limb:
	if (tmp_limb)
		mpi_free_limb_space(tmp_limb);
	return err;
}
EXPORT_SYMBOL_GPL(mpi_mul);

int mpi_mulm(MPI w, MPI u, MPI v, MPI m)
{
	return mpi_mul(w, u, v) ?:
	       mpi_tdiv_r(w, w, m);
}
EXPORT_SYMBOL_GPL(mpi_mulm);
