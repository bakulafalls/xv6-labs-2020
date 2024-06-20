// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int count[(PHYSTOP - KERNBASE) / PGSIZE];   // 引用计数，大小为RAM最大页数
} cow_ref_count;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&cow_ref_count.lock, "cow_ref_count");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    cow_ref_count.count[((uint64)p - KERNBASE) / PGSIZE] = 1;  
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&cow_ref_count.lock);
  if(--cow_ref_count.count[((uint64)pa - KERNBASE) / PGSIZE] == 0) {  // no proc is using this page
    release(&cow_ref_count.lock);

    r = (struct run*)pa;

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  else {  // can not free page since there are other procs using it
    release(&cow_ref_count.lock);
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;

  if(r) {
    kmem.freelist = r->next;
    acquire(&cow_ref_count.lock);
    cow_ref_count.count[((uint64)r - KERNBASE) / PGSIZE] = 1;  // set count to 1 when alloc a new page
    release(&cow_ref_count.lock);
  }
    
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

/**
 * @brief 使cow_ref_count.count加1，call from uvmcopy()
 * @param pa cow page’s address
*/
void
kadd_cow_ref_count(uint64 pa)
{
  acquire(&cow_ref_count.lock);
  cow_ref_count.count[(pa - KERNBASE) / PGSIZE]++;
  release(&cow_ref_count.lock);
}

/**
 * @brief 获取当前的cow 引用计数， call from cawalloc()
 * @param pa cow page’s address
 * @return 当前cow 引用计数 
*/
int
get_cow_ref_count(uint64 pa)
{
  return cow_ref_count.count[(pa - KERNBASE) / PGSIZE];
}
