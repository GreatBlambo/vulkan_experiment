#include "memory.h"
#include "utils.h"

#include "platform.h"

#include <algorithm>

namespace Memory {
Arena::Arena(size_t size, Arena* backing_arena)
    : backing_arena(backing_arena) {
}

Arena::Arena(size_t virtual_mem_reserve)
    : backing_arena(nullptr) {
    virtual_mem_root = Platform::virtual_reserve(virtual_mem_reserve, virtual_pages_reserved);
}

Arena::~Arena() {
    if (virtual_mem_root) {
        Platform::virtual_release(virtual_mem_root);
    }
}

Arena::Block* Arena::append_block(Arena::Block* parent, size_t size) {
    // Request a new block from backing arena or virtual memory and append
    // it to an existing block

    Block* new_block = nullptr;
    size_t buffer_size = 0;
    if (backing_arena) {
        // A backing arena exists. Push a new block onto it
        buffer_size = size;
        new_block = (Block*) backing_arena->push<uint8_t>(sizeof(Block) + buffer_size);
    } else {
        // A backing arena does not exist. Push a new block onto virtual
        // memory
        ASSERT(Platform::get_num_pages(size) <= virtual_pages_reserved - virtual_pages_committed);

        size_t pages_committed = 0;
        void* top = (void*) ((uint8_t*) virtual_mem_root + (virtual_pages_committed * Platform::get_page_size()));
        new_block = (Block*) Platform::virtual_commit(top, size, pages_committed);

        buffer_size = pages_committed * Platform::get_page_size();

        virtual_pages_committed += pages_committed;
    }

    // Fill in the buffer info for the new block
    // The start of the buffer is right after the block metadata we allocated
    new_block->buffer.data = (uint8_t*) (new_block + 1);
    new_block->buffer.size = buffer_size;

    // Append this new block to the parent block if it exists
    if (parent) {
        parent->next = new_block;
    }
    
    return new_block;
}

void* Arena::push_onto_block(Arena::Block* block, size_t size, size_t align) {
    // Attempt to push onto the top of a block. Return null if too full

    // Get the top of the block.
    void* top = (void*) (block->buffer.data + block->used);

    // Get the total bytes unused with the new allocation
    size_t unused = block->buffer.size - (block->used);

    // Align the top pointer and see if it still fits
    void* aligned_top = std::align(align, size, top, unused);

    // If it doesn't fit, return null
    if (!aligned_top) {
        return nullptr;
    }

    // If it fits, update the total used bytes in the block and return the
    // result
    block->used = (block->buffer.size - unused) + size;
    return aligned_top;
}

void* Arena::push(size_t size, size_t align) {
    // If the alignment is not a power of 2, set it to 16
    if (!Utils::is_power_of_2(align)) {
        align = 16;
    }

    //Check if root block exists
    if (!root_block) {
        // No root block exists, append a new one

        root_block = append_block(nullptr, size);
    }

    // Iterate through current blocks, starting at the root block. 
    // If any has enough space, allocate and return the pointer. 
    // If not, create a new block at the end of the chain.
    //
    // TODO: Look into ways to speed this up, EG maintain a bitfield
    // and a fixed list of offsets to skip to a block which best fits
    Block* current_block = root_block;
    void* result = nullptr;
    while (current_block != nullptr) {
        // Attempt to push onto the current block
        result = push_onto_block(current_block, size, align);
        if (result) {
            // Can push onto current block. Return the result.
            break;
        }

        // Can't push onto block, move onto the next
        // If there's no next, request a block that will fit and return
        if (!current_block->next) {
            // None of the current blocks fit. Request a new one from 
            // backing arena or virtual memory
            Arena::Block* new_block = append_block(
                current_block, Arena::GROWTH_FACTOR * std::max(size, current_block->buffer.size));

            // Push onto new block
            result = push_onto_block(new_block, size, align);

            break;
        } else {
            // Else, move on to next block
            current_block = current_block->next;
        }
    }

    return result;
}

void Arena::reset() {
    // Iterate through all the blocks and reset the used memory to 0
    Arena::Block* current_block = root_block;
    while (current_block) {
        current_block->used = 0;
        current_block = current_block->next;
    }
}

}    // namespace Memory