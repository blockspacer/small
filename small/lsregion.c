/*
 * Copyright 2010-2016, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "lsregion.h"

void *
lsregion_alloc_slow(struct lsregion *lsregion, size_t size, int64_t id)
{
	struct lslab *slab = NULL;
	size_t slab_size = lsregion->arena->slab_size;

	/* If there is an existing slab then try to use it. */
	if (! rlist_empty(&lsregion->slabs.slabs)) {
		slab = rlist_last_entry(&lsregion->slabs.slabs, struct lslab,
					next_in_list);
		assert(slab != NULL);
	}
	if ((slab != NULL && size > lslab_unused(slab)) ||
	    slab == NULL) {
		if (size + lslab_sizeof() >= slab_size) {
			/* Large allocation, use malloc() */
			slab_size = size + lslab_sizeof();
			struct quota *quota = lsregion->arena->quota;
			if (quota_use(quota, slab_size) < 0)
				return NULL;
			slab = malloc(slab_size);
			if (slab == NULL) {
				quota_release(quota, slab_size);
				return NULL;
			}
			lslab_create(slab, slab_size);
			rlist_add_tail_entry(&lsregion->slabs.slabs, slab,
					     next_in_list);
			lsregion->slabs.stats.total += slab_size;
		} else if (lsregion->cached != NULL) {
			/* If there is the cached slab then use it. */
			slab = lsregion->cached;
			lsregion->cached = NULL;
			rlist_add_tail_entry(&lsregion->slabs.slabs, slab,
					     next_in_list);
		} else {
			slab = (struct lslab *) slab_map(lsregion->arena);
			if (slab == NULL)
				return NULL;
			lslab_create(slab, slab_size);
			rlist_add_tail_entry(&lsregion->slabs.slabs, slab,
					     next_in_list);
			lsregion->slabs.stats.total += slab_size;
		}
	}
	assert(slab != NULL);
	assert(slab->max_id <= id);
	assert(size <= lslab_unused(slab));
	void *res = lslab_pos(slab);
	slab->slab_used += size;

	/* Update the memory block meta info. */
	assert(slab->max_id <= id);
	slab->max_id = id;
	lsregion->slabs.stats.used += size;
	return res;
}
