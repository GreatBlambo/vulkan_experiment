#pragma once

#include <stdint.h>
#include <functional>

#include "memory.h"

namespace FileSystem {

void load_temp_file(const char* filename, const std::function< void(const Memory::Buffer&) >& on_file_load);
void load_temp_files(const char** filenames, size_t num_files,
                     const std::function< void(const Memory::Buffer*, size_t) >& on_files_load);

void initialize(const char* path_to_mount);
void deinit();

}    // namespace FileSystem