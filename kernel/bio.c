// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET 13
#define HASH(bid) (bid % NBUCKET)

struct hashbuf {
  // Linked list of all buffers in this bucket, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
  struct spinlock lock;
};

struct {
  struct buf buf[NBUF];
  struct hashbuf buckets[NBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  char lockname[16];  // only 10(7+2+1) bytes will be used

  for(int i = 0; i < NBUCKET; i++) {
    // init the spinlocks of hash bucket
    snprintf(lockname, sizeof(lockname), "bcache_%d" ,i);
    initlock(&bcache.buckets[i].lock, "bcache");  
    // init the heads of hash bucket,
    // pointing to itself means it's empty
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  int bid = HASH(blockno);

  acquire(&bcache.buckets[bid].lock);

  // Is the block already cached?
  for(b = bcache.buckets[bid].head.next; b != &bcache.buckets[bid].head; b = b->next) {
    if(b->dev == dev && b->blockno == blockno) {  // b is cached
      b->refcnt++;

      // b is used, so we need to update the timestamp
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);

      release(&bcache.buckets[bid].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.

  // search buckets for the LRU unused buffer
  for(int j = 0; j < NBUCKET; j++) {
    int i = (bid + j) % NBUCKET;
    // acqr bucket's lock before we change buckets[i]'s LRU cache list.
    if(i != bid) {
      if(!holding(&bcache.buckets[i].lock))  // prevent dead lock
        acquire(&bcache.buckets[i].lock);
      else  // find in current bucket, doesn't need to lock again
        continue;  
    }

    // Go through bucket[i]'s LRU cache list, and
    // find the most recently used buffer to replace the current buffer
    b = 0;
    for(struct buf* tmp = bcache.buckets[i].head.next;
    tmp != &bcache.buckets[i].head; 
    tmp = tmp->next) {
      if(tmp->refcnt == 0 && (b == 0 || tmp->timestamp < b->timestamp))  
        // tmp has no reference,
        // and has been more recently used than b
        b = tmp;
    }

    if(b) {  // b has been replaced
      if(i != bid) {  // stealed from other bucket
        // remove b form buckets[i]'s LRU cache list
        // since it's no longer in this bucket
        b->next->prev = b->prev;
        b->prev->next = b->next;
        release(&bcache.buckets[i].lock);

        // add b right after buckets[bid]'s LRU cache list's head
        b->next = bcache.buckets[bid].head.next;
        b->prev = &bcache.buckets[bid].head;
        bcache.buckets[bid].head.next->prev = b;
        bcache.buckets[bid].head.next = b;
      }

      // Recycle the least recently used (LRU) unused buffer.
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;

      // update b->timestamp
      acquire(&tickslock);
      b->timestamp = ticks;
      release(&tickslock);

      release(&bcache.buckets[bid].lock);  // finished modifying current bucket

      acquiresleep(&b->lock);
      return b;
    }
    else {  // can't find a LRU buffer in bucket[i]
      if(i != bid)
        release(&bcache.buckets[i].lock);
    }
  }

  panic("bget: no buffers");
} 


// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bid = HASH(b->blockno);

  // acquire(&bcache.lock);
  // acquire the lock of hash bucket instead of a meta lock
  acquire(&bcache.buckets[bid].lock);
  b->refcnt--;

  // use buf->timestamp to apply LRU
  // update timestap here
  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);

  release(&bcache.buckets[bid].lock);
}

void
bpin(struct buf *b) {
  int bid = HASH(b->blockno);
  acquire(&bcache.buckets[bid].lock);
  b->refcnt++;
  release(&bcache.buckets[bid].lock);
}

void
bunpin(struct buf *b) {
  int bid = HASH(b->blockno);
  acquire(&bcache.buckets[bid].lock);
  b->refcnt--;
  release(&bcache.buckets[bid].lock);
}


