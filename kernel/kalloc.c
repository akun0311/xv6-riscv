// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

//疑问
//1. 我们是如何知道的, kalloc是管理物理内存的?
//   kalloc管理是的是事先给定的物理内存地址范围[end--PHYSTOP]
//   事先给定的物理内存地址范围是通过 xv6 book里面给出的

//2. kalloc是如何和真实的物理内存交互的?
//   我们得到了物理内存的实际地址, 然后通过页表将这个物理地址和虚拟地址联系起来
#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

//函数声明
void freerange(void *pa_start, void *pa_end);

//first address after kernel. 
//defined by kernel.ld
extern char end[];
                  

//通过run这个结构体, 实际上我们可以发现, 
//每个进程包括内核都有自己的虚拟内存, 这些代码都是存储于它自己的虚拟内存里面的
//然后对应到实际上的物理内存

//run结构体, 管理
struct run {
  struct run *next;
};

//一个匿名的C语言结构体, 在初始化的时候, 创建了一个结构体变量kmem
//kmem使用单链表来管理所有空闲的物理内存
//物理内存的单位的PAGE(4096 bytes)
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;


//kinit函数初始化所有的物理内存
void
kinit()
{
  //初始化kmem的锁
  initlock(&kmem.lock, "kmem");
  //将end-->PHYSTOP之间的物理内存标记为已用
  freerange(end, (void*)PHYSTOP);
}

//释放一部分范围内的物理内存
//将pa_start-->pa_end, 
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

//freerange函数调用kfree函数, freerange函数给kfree的参数是从0->PHYSTOP
//在kfree函数中, kfree的操作是从底部往顶部开始释放内存
//因此最终的结果就是, kmem.freelist指向PHYSTOP
//示例: pa->0 4096 8192
//当pa = 4096时, kmem.freelist = 0,    r = 4096, 结果是kmem.freelist = 4096
//当pa = 8192时, kmem.freelist = 4096, r = 8192, 结果是kmem.freelist = 8192
void
kfree(void *pa)
{
  struct run *r;
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  memset(pa, 1, PGSIZE);
  r = (struct run*)pa;
  acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// kalloc用于给所有的进程分配物理内存.
// 每次分配的物理内存大小是PAGESIZE
// 分配物理页的时候, 是分配的kmem的freelist指向的第一个空闲物理内存页
// 然后将freelist指向下一个链表节点
// 当r = kmem.freelist = 0的时候, 说明物理内存已经被分配完了, 此时我们返回0

// 那么这里隐含的一个事实是, kmem.freelist在最开始是指向物理内存的最高点的
// 当然事实上也正是如此
// 在内存分配的时候, 从物理内存的最高点, 一直往下面分配, 直到物理内存的值为0
void *
kalloc(void)
{
  struct run *r;  
  acquire(&kmem.lock); 
  r = kmem.freelist;   
  //if(r)的原因
  //1. r = 0说明物理内存已经被分配完了
  //2. 当指针r = 0的时候, 说明是空指针, 我们不能访问该指针指向的内容, 否则会出现指针错误
  if(r) 
    kmem.freelist = r->next; //让kmem的freelist指向单链表的下一个节点
  release(&kmem.lock); //释放锁
  //当指针r = 0的时候, 说明是空指针, 我们不能访问该指针指向的内容, 否则会出现指针错误
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  //如果r =0, 那么也是返回r
  return (void*)r;   
}
