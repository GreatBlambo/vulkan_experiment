#pragma once

#include <memory>
#include <algorithm>

namespace Memory {

#define KB(num) (size_t)(1024 * num)
#define MB(num) (size_t)(1024 * KB(num))
#define GB(num) (size_t)(1024 * MB(num))
#define TB(num) (size_t)(1024 * GB(num))

template <typename T>
struct Array {
    T* items = nullptr;
    size_t num_items = 0;

    Array(T* items, size_t num_items) 
        : items(items),
        , num_items(num_items){
    }

    inline T& operator[](size_t index) {
        ASSERT(index < num_items);
        return items[i];
    }

    inline size_t size() {
        return num_items;
    }
};

struct Buffer {
    uint8_t* data = nullptr;
    size_t size   = 0;
};

struct Arena {
    Arena* next = nullptr;
    Buffer buffer;
    size_t used = 0;
    void* push(size_t size, size_t align);
    void reset();
    void reset(void* ptr);
    void* top();
    bool inside(void* ptr);
};

class IAllocator {
  public:
    virtual void* allocate_data(size_t size, size_t align) = 0;
    virtual void* reallocate_data(void* ptr, size_t size, size_t align) = 0;
    virtual void free(void* ptr) = 0;

    template < typename T >
    T* allocate(size_t number, size_t align = alignof(T)) {
        return (T*) allocate_data(number * sizeof(T), align);
    }

    template < typename T >
    T* reallocate(T* ptr, size_t number, size_t align = alignof(T)) {
        return (T*) reallocate_data((void*) ptr, number * sizeof(T), align);
    }
};

class VirtualHeap : public IAllocator {
  private:
    Arena arena;
    size_t num_pages_committed = 0;
    size_t num_pages_reserved  = 0;

    static const size_t GROWTH_FACTOR = 2;

  public:
    VirtualHeap(size_t reserve_size);
    ~VirtualHeap();

    void* allocate_data(size_t size, size_t align);
    inline void* reallocate_data(void* ptr, size_t size, size_t align) {
        return allocate_data(size, align);
    };
    inline void free(void* ptr) {
        // No-op
    };
    void reset();
};

class LinearAllocator : public IAllocator {
  private:
    Arena* root_arena = nullptr;

    IAllocator& backing_allocator;

    static const size_t GROWTH_FACTOR = 2;

    Arena* append_arena(Arena* parent, size_t size);

  public:
    LinearAllocator(size_t size, IAllocator& backing_allocator);
    ~LinearAllocator();

    void* allocate_data(size_t size, size_t align);
    inline void* reallocate_data(void* ptr, size_t size, size_t align) {
        return allocate_data(size, align);
    };
    inline void free(void* ptr) {
        // No-op
    };

    void reset();
};

}    // namespace Memory