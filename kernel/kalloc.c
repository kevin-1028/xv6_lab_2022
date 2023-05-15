// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define ST_CNT 64

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 wait_steal[ST_CNT];
} kmem[NCPU];

const uint name_size = sizeof("kmem cup 0");
char kmem_lock_name[NCPU][sizeof("kmem cup 0")];


void
kinit()
{
  for(int i = 0; i < NCPU; i++){
    snprintf(kmem_lock_name[i], name_size, "kmem cup %d", i);
    initlock(&kmem[i].lock, kmem_lock_name[i]);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  push_off();
  uint cpu = cpuid();
  pop_off();

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);
}

int steal(uint cpu){
    uint leave = ST_CNT;
    int id = 0;

    memset(kmem[cpu].wait_steal, 0, sizeof(kmem[cpu].wait_steal));
    for(int i = 0; i < NCPU; i++){
        if(i == cpu)
            continue;
        acquire(&kmem[i].lock);
        while(kmem[i].freelist && leave){
            kmem[cpu].wait_steal[id] = (uint64)kmem[i].freelist;
            id++;
            kmem[i].freelist = kmem[i].freelist->next;
            leave--;
        }
        release(&kmem[i].lock);
        if(leave == 0){
            break;
        }
    }
    return id;
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  uint cpu = cpuid();
  
  acquire(&kmem[cpu].lock);
  r = kmem[cpu].freelist;
  if(r){
    kmem[cpu].freelist = r->next;
    release(&kmem[cpu].lock);
  }else{
    release(&kmem[cpu].lock);
    int cnt = steal(cpu);
    if(!cnt){
        pop_off();
        return 0;
    }
    acquire(&kmem[cpu].lock);
    for(int i = 0; i < cnt; i++){
        if(!kmem[cpu].wait_steal[i]){
            break;
        }
        // concept is like kfree()
        ((struct run*)kmem[cpu].wait_steal[i])->next = kmem[cpu].freelist;
        kmem[cpu].freelist = (struct run*)kmem[cpu].wait_steal[i];
    }
    r = kmem[cpu].freelist;
    kmem[cpu].freelist = r->next;
    release(&kmem[cpu].lock);
  }
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  pop_off();
  return (void*)r;
}

