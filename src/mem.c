#include <stdlib.h>
#include "mem.h"

// NOTE:
// - when ndx is -1, that page slot is available for reuse
// - we use lru caching!

int vminit(vmem *mem, vword cachesz) {
  if (NULL == mem) return VERROR;

  // try to allocate page table
  mem->page = (vmpage*)malloc(sizeof(vmpage));
  if (NULL == mem->page) return VENOMEM;
  mem->page[0].ndx = -1;
  mem->page[0].flags = 0;
  mem->page[0].frame = NULL;

  // setup resource lock
  if (0 != rw_init(&mem->_lock)) {
    free(mem->page);
    mem->page = NULL;
    return VENOMEM;
  }

  // setup cache
  mem->cache_pool = (_vmem_cache*)malloc(sizeof(_vmem_cache) * cachesz);
  if (NULL == mem->cache_pool) {
    free(mem->page);
    mem->page = NULL;
    rw_destroy(&mem->_lock);
    return VENOMEM;
  }

  // initialize the cache
  for (int i = 0; i < cachesz; i++) {
    mem->cache_pool[i].ndx = -1;
    mem->cache_pool[i].offst = 0;
    mem->cache_pool[i].prev = NULL;
    mem->cache_pool[i].next = NULL;
  }
  fmtx_init(&mem->_cache_lock);

  // set variables
  mem->_used = 0;
  mem->_alloc = 1;

  mem->_cache_size = cachesz;
  mem->_cache_used = 0;
  mem->_cache_head = &mem->cache_pool[0];
  mem->_cache_tail = &mem->cache_pool[0];

  return VOK;
}

int vmdestroy(vmem *mem) {
  if (NULL == mem || NULL == mem->page) return VERROR;

  // if mem->page is still allocated, iterate through the page table and try to
  // free them all
  if (NULL != mem->page) {
    for (vqword i = 0; i < mem->_alloc; i++) {

      // if the page's frame is still there, and this memory owns that frame,
      // then free that
      if (
        -1 != mem->page[i].ndx &&
        (VPOWNED & mem->page[i].flags) &&
        NULL != mem->page[i].frame
      ) {
        free(mem->page[i].frame);
      }

    }

    // free the page table
    free(mem->page);
    mem->page = NULL;
  }

  // free the cache
  if (NULL != mem->cache_pool)
    free(mem->cache_pool);
  mem->cache_pool = NULL;
  mem->_cache_size = 0;
  mem->_cache_used = 0;
  mem->_cache_head = NULL;
  mem->_cache_tail = NULL;

  // set these to zero
  mem->_used = 0;
  mem->_alloc = 0;

  // destroy the rw lock
  rw_destroy(&mem->_lock);

  return VOK;
}

int vmmap(vmem *mem, vqword ndx, vbyte flags) {
  if (NULL == mem || NULL == mem->page) return VENOMEM;

  // invalid page index
  if (VPAGEMX < ndx) return VESEGV;

  // acquire the write lock, this is to prevent the possibility of having
  // multiple calls to vmmap having the same ndx to allocate separate slots for
  // that same page. we're wasting memory
  rw_wlock(&mem->_lock);

  // ptr into an element within the page table
  vmpage *avail = NULL;

  // if there's still some slots in the table is available, try to grab them
  if (mem->_alloc > mem->_used) {
    for (vqword i = 0; i < mem->_alloc; i++) {

      // the requested page ndx is already mapped! stop the operation
      if (mem->page[i].ndx == ndx) {
        rw_wunlock(&mem->_lock);
        return VOK;
      }

      // check if this is an empty slot, we can use this to map the page
      if (NULL == avail && -1 == mem->page[i].ndx) avail = &mem->page[i];

    }
  }

  // the page table is full, try to resize it
  else {
    vmpage *tmp = (vmpage*)realloc(mem->page, sizeof(vmpage) * mem->_alloc * 2);

    // failed to re-allocate the page table
    if (NULL == tmp) {
      rw_wunlock(&mem->_lock);
      return VENOMEM;
    }

    // initialize the new data
    for (vqword i = 0; i < mem->_alloc; i++) {
      tmp[mem->_alloc + i].ndx = -1;
      tmp[mem->_alloc + i].flags = 0;
      tmp[mem->_alloc + i].frame = NULL;
    }

    // the end of the last allocation is now available! use it
    avail = &tmp[mem->_alloc];
    mem->_alloc *= 2;
    mem->page = tmp;
  }

  // set some variables on the page
  avail->ndx = ndx;
  avail->flags = flags;
  mem->_used++;

  // release the lock, allow other tasks to access the memory
  rw_wunlock(&mem->_lock);
  return VOK;
}

int vmunmap(vmem *mem, vqword ndx) {
  if (NULL == mem || NULL == mem->page) return VERROR;

  // invalid page index
  if (VPAGEMX < ndx) return VESEGV;

  // we need a write-lock here, 'cause other thread might about to access this
  // page
  rw_wlock(&mem->_lock);

  // find the page and unmap it
  for (vqword i = 0; i < mem->_alloc; i++) {
    if (ndx == mem->page[i].ndx) {

      // if the frame of this page is not NULL and this page owns that frame,
      // de-allocate the frame
      if ((mem->page[i].flags & VPOWNED) && NULL != mem->page[i].frame)
        free(mem->page[i].frame);

      // reset the variables in slot for later reuse
      mem->page[i].frame = NULL;
      mem->page[i].ndx = -1;
      mem->page[i].flags = 0;
      mem->_used--;

      break;
    }
  }

  rw_wunlock(&mem->_lock);

  // remove the page from cache, if it is currently cached
  fmtx_lock(&mem->_cache_lock);
  _vmem_cache *ent = mem->_cache_head;
  for (int i = 0; i < mem->_cache_used; i++) {
    if (ent->ndx == ndx) {
      if (NULL != ent->prev) ent->prev->next = ent->next;
      else mem->_cache_head = ent->next;
      if (NULL != ent->next) ent->next->prev = ent->prev;
      else mem->_cache_tail = ent->prev;
      ent->ndx = -1;
      ent->offst = 0;
      ent->prev = NULL;
      ent->next = NULL;
      break;
    }
    if (ent->next == NULL) break;
    ent = ent->next;
  }
  fmtx_unlock(&mem->_cache_lock);

  return VOK;
}

int vmgetp(vmem *mem, vqword ndx, vmpage **out) {
  if (NULL == mem || NULL == mem->page || NULL == out) return VERROR;

  // invalid page index
  if (VPAGEMX < ndx) return VESEGV;

  _vmem_cache *ent = NULL;

  // check the cache first
  fmtx_lock(&mem->_cache_lock);
  ent = mem->_cache_head;
  for (int i = 0; i < mem->_cache_used; i++) {
    if (ent->ndx == ndx) {
      *out = mem->page + ent->offst;
      if (NULL != ent->prev) ent->prev->next = ent->next;
      if (NULL != ent->next) ent->next->prev = ent->prev;
      mem->_cache_head->prev = ent;
      ent->next = mem->_cache_head;
      ent->prev = NULL;
      mem->_cache_head = ent;
      fmtx_unlock(&mem->_cache_lock);
      // cache hit
      return VOK;
    }
    // cache miss
    if (ent->next == NULL) break;
    ent = ent->next;
  }
  fmtx_unlock(&mem->_cache_lock);

  // acquire rlock
  rw_rlock(&mem->_lock);

  // page table lookup, find the page and return it
  for (vqword i = 0; i < mem->_alloc; i++) {
    if (ndx == mem->page[i].ndx) {
      *out = &mem->page[i];
      rw_runlock(&mem->_lock);
      // page table hit, update the cache

      fmtx_lock(&mem->_cache_lock);

      // cache is full, evict the lru
      if (mem->_cache_size <= mem->_cache_used) {
        if (NULL != mem->_cache_tail->prev)
          mem->_cache_tail->prev->next = NULL;
        ent = mem->_cache_tail;
        if (NULL != mem->_cache_tail->prev)
          mem->_cache_tail = mem->_cache_tail->prev;
        mem->_cache_head->prev = ent;
        ent->prev = NULL;
        ent->next = mem->_cache_head;
        mem->_cache_head = ent;
        ent->ndx = ndx;
        ent->offst = &mem->page[i] - mem->page;
        fmtx_unlock(&mem->_cache_lock);
        return VOK;
      }

      // find a free entry in the pool
      for (int i = 0; i < mem->_cache_size; i++) {
        if (mem->cache_pool[i].ndx == -1) {
          ent = &mem->cache_pool[i];
          mem->_cache_head->prev = ent;
          ent->prev = NULL;
          ent->next = mem->_cache_head;
          mem->_cache_head = ent;
          ent->ndx = ndx;
          ent->offst = &mem->page[i] - mem->page;
          mem->_cache_used++;
          fmtx_unlock(&mem->_cache_lock);
          return VOK;
        }
      }

    }
  }

  rw_runlock(&mem->_lock);

  // page table miss, the page does not exist, raise segmentation fault
  return VESEGV;
}

int vmgetd(vmem *mem, vbyte *out, vqword addr, vqword sz, vbyte perm) {
  if (NULL == mem || NULL == mem->page || NULL == out) return VERROR;

  // access to 0x0 (NULL) is not allowed
  if (0 == addr) return VENULL;

  // nothing to read
  if (0 == sz) return VOK;

  // we only need to check the permission flags
  perm &= 7;

  vqword ndx = addr >> 14;
  vqword disp = addr & 0x3fff;
  vmpage *curr = NULL;
  int stat = VOK;

  rw_rlock(&mem->_lock);

  for (vqword i = 0; i < sz; i++) {
    if (NULL == curr || disp >= VPAGESZ) {
      // transition to next page
      if (NULL != curr) disp = 0;

      // attempt to get the page
      stat = vmgetp(mem, ndx++, &curr);
      if (VOK != stat) {
        rw_runlock(&mem->_lock);
        return stat;
      }

      // check for permissions
      if ((curr->flags & perm) != perm) {
        rw_runlock(&mem->_lock);
        return VEACCES;
      }

      // initialize the page if needed
      if (NULL == curr->frame) {
        curr->frame = (vbyte*)malloc(VPAGESZ);
        if (NULL == curr->frame) {
          rw_runlock(&mem->_lock);
          return VENOMEM;
        }
      }

    }
    // copy the byte
    out[i] = curr->frame[disp++];
  }

  rw_runlock(&mem->_lock);
  return VOK;
}

int vmsetd(vmem *mem, vbyte *in, vqword addr, vqword sz, vbyte perm) {
  if (NULL == mem || NULL == mem->page || NULL == in) return VERROR;

  // access to 0x0 (NULL) is not allowed
  if (0 == addr) return VENULL;

  // nothing to write
  if (0 == sz) return VOK;

  // we only need to check the permission flags
  perm &= 7;

  vqword ndx = addr >> 14;
  vqword disp = addr & 0x3fff;
  vmpage *curr = NULL;
  int stat = VOK;

  rw_rlock(&mem->_lock);

  for (vqword i = 0; i < sz; i++) {
    if (NULL == curr || disp >= VPAGESZ) {
      // transition to next page
      if (NULL != curr) disp = 0;

      // attempt to get the page
      stat = vmgetp(mem, ndx++, &curr);
      if (VOK != stat) {
        rw_runlock(&mem->_lock);
        return stat;
      }

      // check for permissions
      if ((curr->flags & perm) != perm) {
        rw_runlock(&mem->_lock);
        return VEACCES;
      }

      // initialize the page if needed
      if (NULL == curr->frame) {
        curr->frame = (vbyte*)malloc(VPAGESZ);
        if (NULL == curr->frame) {
          rw_runlock(&mem->_lock);
          return VENOMEM;
        }
      }

    }
    // copy the byte
    curr->frame[disp++] = in[i];
  }

  rw_runlock(&mem->_lock);
  return VOK;
}

int vmfilld(vmem *mem, vqword addr, vqword sz, vbyte c) {
  if (NULL == mem || NULL == mem->page) return VERROR;

  // access to 0x0 (NULL) is not allowed
  if (0 == addr) return VENULL;

  // nothing to write
  if (0 == sz) return VOK;

  vqword ndx = addr >> 14;
  vqword disp = addr & 0x3fff;
  vmpage *curr = NULL;
  int stat = VOK;

  rw_rlock(&mem->_lock);

  for (vqword i = 0; i < sz; i++) {
    if (NULL == curr || disp >= VPAGESZ) {
      // transition to next page
      if (NULL != curr) disp = 0;

      // attempt to get the page
      stat = vmgetp(mem, ndx++, &curr);
      if (VOK != stat) {
        rw_runlock(&mem->_lock);
        return stat;
      }

      // initialize the page if needed
      if (NULL == curr->frame) {
        curr->frame = (vbyte*)malloc(VPAGESZ);
        if (NULL == curr->frame) {
          rw_runlock(&mem->_lock);
          return VENOMEM;
        }
      }

    }
    // copy the byte
    curr->frame[disp++] = c;
  }

  rw_runlock(&mem->_lock);
  return VOK;
}
