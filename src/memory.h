#pragma once

#include <memory>
#include <algorithm>

namespace Memory {
    #define KB(num) (size_t) (1024 * num)
    #define MB(num) (size_t) (1024 * KB(num))
    #define GB(num) (size_t) (1024 * MB(num))

    struct Buffer {
        uint8_t* data;
        size_t size;
    };

    class Arena {
      private:
        struct Block {
            Block* next = nullptr;
            Buffer buffer;
            size_t used = 0;
        };

        void* virtual_mem_root = nullptr;
        size_t virtual_pages_reserved = 0;
        size_t virtual_pages_committed = 0;

        Block* root_block = nullptr;

        Arena* backing_arena = nullptr;

        static const size_t GROWTH_FACTOR = 2;

        void* push(size_t size, size_t align);

        Block* append_block(Block* parent, size_t size);
        static void* push_onto_block(Block* block, size_t size, size_t align);

      public:
        Arena(size_t size, Arena* backing_arena);
        Arena(size_t virtual_mem_reserve);
        ~Arena();

        void reset();

        template<typename T>
        T* push(size_t number, size_t align = alignof(T)) {
            return (T*) push(number * sizeof(T), align);
        }
    };
    }    // namespace Memory