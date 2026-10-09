#ifndef CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICSMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICSMEMORY_HPP

#include <cstddef>
#include <cstdint>

#include "SceTypes.hpp"

namespace FontGraphics {

struct MemoryRegion {
    void* Address;
    std::size_t Size;
    std::int64_t PhysicalOffset;
    PthreadMutex Mutex;
};

static_assert(sizeof(MemoryRegion) == 32);

int AllocateRegion_nid_no_patch(std::size_t size, MemoryRegion& region);
void ReleaseRegion_nid_no_patch(MemoryRegion region);

}

#endif
