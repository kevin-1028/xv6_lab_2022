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

#define NUM_BUCK 13
#define BUFMAP_HASH(dev, blockno) ((((dev) << 27) | (blockno)) % NUM_BUCK)

struct {
  struct spinlock evc_lock;
  struct buf buf[NBUF];

  struct buf hash_table[NUM_BUCK];
  struct spinlock hash_lock[NUM_BUCK];
  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;

void
binit(void)
{
  // initialize hash table and lock
  for(int i = 0; i < NUM_BUCK; i++){
    initlock(&bcache.hash_lock[i], "bcache_hash");
    bcache.hash_table[i].next = 0;
  }

  // initialize the buffer
  for(int i = 0; i < NBUF; i++){
    struct buf *b = &bcache.buf[i];
    initsleeplock(&b->lock, "buffer");
    b->last_used = 0;
    b->refcnt = 0;
    b->next = bcache.hash_table[0].next;
    bcache.hash_table[0].next = b;
  }

  initlock(&bcache.evc_lock, "bcache_eviction");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint key = BUFMAP_HASH(dev, blockno);

  acquire(&bcache.hash_lock[key]);

  // Is the block already cached?
  for(b = bcache.hash_table[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.hash_lock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached
  // to avoid the circular wait
  // when holding this key lock and another thread request this bucked key,
  // that thread will wait the key release.
  // However, that thread also holds another bucked key lock.
  // This threah request the key lock that thread holds, when access all of bucked 
  // to find the least used buffer, it will form deadlock
  release(&bcache.hash_lock[key]);
  // to avoid two thread access same blockno and cause two identical content of
  // cache buffer
  acquire(&bcache.evc_lock);

  for(b = bcache.hash_table[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      // why acquire the key lock in if
      // because another thread maybe access the same bucket
      // if acquire outside the if, that thread cannot perform first part that
      // check the block is in cache, which cause the effieiency down
      acquire(&bcache.hash_lock[key]);
      b->refcnt++;
      release(&bcache.hash_lock[key]);
      release(&bcache.evc_lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // still not found
  // check all of bucket
  // now only holing evc_lock and none of bucket lock is holded
  // it is safe to acquire any bucket lock
  //
  // find the least used buffer among all buckets
  struct buf *before_least = 0;
  uint holding_bucket = -1;
  for(int i = 0; i < NUM_BUCK; i++){
    // either holding nothing, or holding locks of bucket on the left side of current bucket
    acquire(&bcache.hash_lock[i]);
    int newfound = 0;
    for(b = &bcache.hash_table[i]; b->next; b = b->next){
        // find the least used buf in the i-th bucket
        // check the refcnt and before_least whether exist
        // compare the last used time between the newfonund buf b and previous found least used buf before_least 
        if(b->next->refcnt == 0 && (!before_least || b->next->last_used < before_least->next->last_used)){
            before_least = b;
            newfound = 1;
        }
    }
    if(!newfound){
        // not found the least use buf in this bucket
        release(&bcache.hash_lock[i]);
    }else{
        if(holding_bucket != -1) release(&bcache.hash_lock[holding_bucket]);
        holding_bucket = i;
    }
  }
  
  if(!before_least){
    panic("bget : no buffer");
  }

  b = before_least->next;
  if(holding_bucket != key){
    // remove the least used buf in holding bucket
    before_least->next = b->next;
    release(&bcache.hash_lock[holding_bucket]);
    // insert the least used but to the head of key bucket
    acquire(&bcache.hash_lock[key]);
    b->next = bcache.hash_table[key].next;
    bcache.hash_table[key].next = b;
  }

  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1;
  b->valid = 0;
  release(&bcache.hash_lock[key]);
  release(&bcache.evc_lock);
  acquiresleep(&b->lock);
  return b;
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

  uint key = BUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.hash_lock[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one use this buf
    b->last_used = ticks;
  }
  
  release(&bcache.hash_lock[key]);
}

void
bpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.hash_lock[key]);
  b->refcnt++;
  release(&bcache.hash_lock[key]);
}

void
bunpin(struct buf *b) {
  uint key = BUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.hash_lock[key]);
  b->refcnt--;
  release(&bcache.hash_lock[key]);
}


