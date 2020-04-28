#include "file_system.h"
#include "utils.h"

#include <physfs.h>

#include <vector>

/////////////////////////////////////////////////////////////////////////////////////////////////
// Files ////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

namespace FileSystem {

void load_temp_file(const char* filename, std::function< void(const Memory::Buffer&) > on_file_load) {
    int result = PHYSFS_exists(filename);
    if (!result) {
        RUNTIME_ERROR("Error loading %s", filename);
    }

    PHYSFS_file*  file      = PHYSFS_openRead(filename);
    PHYSFS_sint64 file_size = PHYSFS_fileLength(file);

    uint8_t* buffer = new uint8_t[file_size];
    PHYSFS_read(file, buffer, 1, file_size);
    PHYSFS_close(file);

    on_file_load({ buffer, (size_t) file_size });
    delete[] buffer;
}

void load_temp_files(const char** filenames, size_t num_files,
                     std::function< void(const Memory::Buffer*, size_t) > on_files_load) {
    std::vector< Memory::Buffer > results;
    for (size_t i = 0; i < num_files; i++) {
        const char* filename = filenames[i];
        int         result   = PHYSFS_exists(filename);
        if (!result) {
            RUNTIME_ERROR("Error loading %s", filename);
        }

        PHYSFS_file*  file      = PHYSFS_openRead(filename);
        PHYSFS_sint64 file_size = PHYSFS_fileLength(file);

        uint8_t* buffer = new uint8_t[file_size];
        PHYSFS_read(file, buffer, 1, file_size);
        PHYSFS_close(file);

        results.push_back({ buffer, (size_t) file_size });
    }

    on_files_load(results.data(), results.size());

    for (const Memory::Buffer& result : results) {
        delete[] result.data;
    }
}

void initialize(const char* path_to_mount) {
    PHYSFS_init(NULL);
    if (!PHYSFS_mount(path_to_mount, "", 1)) {
        RUNTIME_ERROR("Failed to mount %s folder", path_to_mount);
    }
}

void deinit() { PHYSFS_deinit(); }

}    // namespace FileSystem