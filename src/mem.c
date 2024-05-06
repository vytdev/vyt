#include <stdlib.h>
#include "mem.h"

// NOTE:
// - when ndx is -1, that page slot is available for reuse

int vminit(vmem *mem) {
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

  // set variables
  mem->_used = 0;
  mem->_alloc = 1;

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

  // set these to zero
  mem->_used = 0;
  mem->_alloc = 0;

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

      // the frame of this page is not NULL and this page owns that frame,
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
  return VOK;
}

int vmgetp(vmem *mem, vqword ndx, vmpage **out) {
  if (NULL == mem || NULL == mem->page) return VERROR;

  // invalid page index
  if (VPAGEMX < ndx) return VESEGV;

  rw_rlock(&mem->_lock);

  // find the page and return it
  for (vqword i = 0; i < mem->_alloc; i++) {
    if (ndx == mem->page[i].ndx) {
      *out = &mem->page[i];
      rw_runlock(&mem->_lock);
      return VOK;
    }
  }

  rw_runlock(&mem->_lock);

  // the page does not exist, raise segmentation fault
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
