#pragma once

#include "memory.h"

namespace Platform {
size_t get_page_size();
inline size_t get_num_pages(size_t bytes) {
    return (bytes / get_page_size()) + 1;
};

void* virtual_reserve(size_t size, size_t& pages_reserved);
void* virtual_commit(void* ptr, size_t size, size_t& pages_committed);
void virtual_decommit(void* ptr, size_t size);
void virtual_release(void* ptr);
}    // namespace Platform