#include "platform.h"
#include "utils.h"

namespace Platform {

// Windows

#ifdef _WIN32
#include <windows.h>

size_t get_page_size() {
    static size_t page_size = 0;

    if (!page_size) {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);

        page_size = system_info.dwPageSize;
    }

    ASSERT(page_size > 0);
    
    return page_size;
}

void* virtual_reserve(size_t size, size_t& pages_reserved) {
    pages_reserved = get_num_pages(size);
    return VirtualAlloc(0, size, MEM_RESERVE, PAGE_NOACCESS);
}

void* virtual_commit(void* ptr, size_t size, size_t& pages_committed) {
    pages_committed = get_num_pages(size);
    return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
}

void virtual_decommit(void* ptr, size_t size) {
    VirtualFree(ptr, size, MEM_DECOMMIT);
}

void virtual_release(void* ptr) {
    VirtualFree(ptr, 0, MEM_RELEASE);
}
#else
    #error Platform not supported
#endif

}    // namespace Platform