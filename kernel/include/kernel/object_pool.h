#ifndef __KERNEL_INCLUDE_KERNEL_MM_KMEM_H__
#define __KERNEL_INCLUDE_KERNEL_MM_KMEM_H__

/**
 * @file include/object_pool.h
 * 
 * Object allocator.
 */

#ifndef __OSDEV_KERNEL__
#error "This is a kernel header; user programs should not #include it"
#endif

#include <kernel/list.h>
#include <kernel/spinlock.h>

#define OBJECT_POOL_NAME_MAX  64

/**
 * Object pool descriptor.
 */
struct ObjectPool {
  /** Spinlock protecting this pool. */
  struct SpinLock   lock;

  /** Empty slabs (all blocks allocated). */
  struct ListLink   slabs_empty;
  /** Partial slabs (some blocks allocated, some free). */
  struct ListLink   slabs_partial;
  /** Complete slabs (all blocks free). */
  struct ListLink   slabs_full;

  /** The number of object per one slab. */
  unsigned          slab_capacity;
  /** Page block order for each slab. */
  unsigned          slab_page_order;

  /** Size of a single block in bytes. */
  size_t            block_size;
  /** Byte alignment of a single block. */
  size_t            block_align;

  /** Single of a single object. */
  size_t            obj_size;
  /** Function to construct objects in the cache. */
  void            (*obj_ctor)(void *, size_t);
  /** Function to undo object construction. */
  void            (*obj_dtor)(void *, size_t);

  /** The maximum slab color offset. */
  size_t            color_max;
  /** The color offset to be used by the next slab. */
  size_t            color_next;

  /** Link into the global list of cache descriptors. */
  struct ListLink   link;

  /** Human-readable cache name (for debugging purposes). */
  char              name[OBJECT_POOL_NAME_MAX + 1];
};

struct ObjectPoolTag {
  void                 *object;
  struct ObjectPoolTag *next;
};

/**
 * Object slab descriptor.
 */
struct ObjectPoolSlab {
  /** Linkage in the cache. */
  struct ListLink         link;
  /** The pool this slab belongs to. */
  struct ObjectPool      *pool;
  /** Address of the buffer containing all memory blocks. */
  void                   *data;
  /** Address of the bufers containing all block tags. */
  struct ObjectPoolTag   *tags;
  /** Linked list of free block tags. */
  struct ObjectPoolTag   *free;
  /** Reference count for allocated blocks. */       
  unsigned                used_count;
};

struct ObjectPool *object_pool_create(const char *, size_t, size_t,
                                      void (*)(void *, size_t),
                                      void (*)(void *, size_t));
int                object_pool_destroy(struct ObjectPool *);
void              *object_pool_get(struct ObjectPool *);
void               object_pool_put(struct ObjectPool *, void *);

void               object_pool_init(void);

#endif  // !__KERNEL_INCLUDE_KERNEL_MM_KMEM_H__