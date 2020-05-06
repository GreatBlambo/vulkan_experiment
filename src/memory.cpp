#include "memory.h"
#include "utils.h"

#include "platform.h"

#include <algorithm>

namespace Memory {

void* Arena::push(size_t size, size_t align) {
    // Attempt to push onto the top of a arena. Return null if too full

    // Get the top of the arena.
    void* top = this->top();

    // Get the total bytes unused with the new allocation
    size_t unused = buffer.size - (used);

    // Align the top pointer and see if it still fits
    void* aligned_top = std::align(align, size, top, unused);

    // If it doesn't fit, return null
    if (!aligned_top) {
        return nullptr;
    }

    // If it fits, update the total used bytes in the arena and return the
    // result
    used = (buffer.size - unused) + size;
    return aligned_top;
}

void Arena::reset() {
    used = 0;
}

void Arena::reset(void* ptr) {
    ASSERT(this->inside(ptr));

    used = (uint8_t*) ptr - buffer.data;
}

void* Arena::top() {
    return (void*) (buffer.data + used);
}

bool Arena::inside(void* ptr) {
    return (uint8_t*) ptr >= buffer.data && (uint8_t*) ptr <= (uint8_t*) this->top();
}

VirtualHeap::VirtualHeap(size_t reserve_size) {
    arena.buffer.data = (uint8_t*) Platform::virtual_reserve(reserve_size, num_pages_reserved);
}

VirtualHeap::~VirtualHeap() {
    Platform::virtual_release(arena.buffer.data);
}

void* VirtualHeap::allocate_data(size_t size, size_t align) {
    // Attempt to push onto the existing arena
    void* result = arena.push(size, align);
    if (result) {
        // Size fits on existing arena
        return result;
    }

    // Size doesn't fit on current arena. Expand the arena by committing
    // more memory
    //
    // Do not apply growth factor if it's the first commit
    size_t size_needed = num_pages_committed == 0 ? size : std::max(arena.buffer.size, size + align) * GROWTH_FACTOR;

    // If growth size required > amount reserved, panic
    ASSERT_MSG(num_pages_reserved >= Platform::get_num_pages(size_needed),
               "Cannot commit (%llu) pages more than reserved (%llu) pages",
               Platform::get_num_pages(size_needed), num_pages_reserved);

    // Commit additional memory in reserved region
    size_t pages_committed;
    Platform::virtual_commit(arena.buffer.data + num_pages_committed * Platform::get_page_size(), size_needed, pages_committed);

    // Add to pages committed
    num_pages_committed += pages_committed;

    // Increase the size of the arena to the total number of pages
    // committed
    arena.buffer.size = num_pages_committed * Platform::get_page_size();

    // Push onto the new arena
    result = arena.push(size, align);
    return result;
}

void VirtualHeap::reset() {
    arena.reset();
}

LinearAllocator::LinearAllocator(size_t size, IAllocator& backing_allocator)
    : backing_allocator(backing_allocator) {
}

LinearAllocator::~LinearAllocator() {
}

Arena* LinearAllocator::append_arena(Arena* parent, size_t size) {
    // Request a new arena from backing arena or virtual memory and append
    // it to an existing arena

    Arena* new_arena = nullptr;
    size_t buffer_size = 0;
    // Push a new arena onto backing arena
    buffer_size = size;
    new_arena = (Arena*) backing_allocator.allocate<uint8_t>(sizeof(Arena) + buffer_size);

    // Fill in the buffer info for the new arena
    // The start of the buffer is right after the arena metadata we allocated
    new_arena->buffer.data = (uint8_t*) (new_arena + 1);
    new_arena->buffer.size = buffer_size;

    // Append this new arena to the parent arena if it exists
    if (parent) {
        parent->next = new_arena;
    }
    
    return new_arena;
}

void* LinearAllocator::allocate_data(size_t size, size_t align) {
    // If the alignment is not a power of 2, set it to 16
    if (!Utils::is_power_of_2(align)) {
        align = 16;
    }

    //Check if root arena exists
    if (!root_arena) {
        // No root arena exists, append a new one

        root_arena = append_arena(nullptr, size);
    }

    // Iterate through current arenas, starting at the root arena. 
    // If any has enough space, allocate and return the pointer. 
    // If not, create a new arena at the end of the chain.
    //
    // TODO: Look into ways to speed this up, EG maintain a bitfield
    // and a fixed list of offsets to skip to an arena which best fits
    Arena* current_arena = root_arena;
    void* result = nullptr;
    while (current_arena != nullptr) {
        // Attempt to push onto the current arena
        result = current_arena->push(size, align);
        if (result) {
            // Can push onto current arena. Return the result.
            break;
        }

        // Can't push onto arena, move onto the next
        // If there's no next, request a arena that will fit and return
        if (!current_arena->next) {
            // None of the current arenas fit. Request a new one from 
            // backing arena or virtual memory
            Arena* new_arena = append_arena(
                current_arena, LinearAllocator::GROWTH_FACTOR * std::max(size, current_arena->buffer.size));

            // Push onto new arena
            result = new_arena->push(size, align);

            break;
        } else {
            // Else, move on to next arena
            current_arena = current_arena->next;
        }
    }

    return result;
}

void LinearAllocator::reset() {
    // Iterate through all the arenas and reset the used memory to 0
    Arena* current_arena = root_arena;
    while (current_arena) {
        current_arena->reset();
        current_arena = current_arena->next;
    }
}

}    // namespace Memory