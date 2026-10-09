#include "prx/libSceFont/include/FontGraphicsMemory.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"

#include <limits>
#include <stdexcept>

extern "C" {
std::size_t APS5_VABI sceKernelGetDirectMemorySize();
int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);
int APS5_VABI sceKernelReleaseDirectMemory(std::int64_t, std::size_t);
}

int FontGraphics::AllocateRegion_nid_no_patch(std::size_t size, MemoryRegion& region) {
    region = {};
    const std::size_t alignment = size >= 0x200000 && size % 0x200000 == 0 ? 0x200000 : 0x10000;
    if (size > std::numeric_limits<std::size_t>::max() - alignment + 1) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    const std::size_t bytes = (size + alignment - 1) & ~(alignment - 1);
    std::int64_t physical;
    if (sceKernelAllocateDirectMemory(0, static_cast<std::int64_t>(sceKernelGetDirectMemorySize()), bytes, alignment, 0, &physical) != 0) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    MemoryRegion allocated{nullptr, bytes, physical, nullptr};
    PthreadMutexattr attributes = nullptr;
    int result;
    try {
        void* address = reinterpret_cast<void*>(std::uintptr_t{0x200000000});
        result = sceKernelMapDirectMemory(&address, bytes, 51, 0, physical, alignment);
        if (result == 0 && !address) result = SCE_FONT_ERROR_ALLOCATION_FAILED;
        allocated.Address = address;
        if (result == 0) result = scePthreadMutexattrInit(&attributes);
        if (result == 0) result = scePthreadMutexattrSetprotocol(&attributes, 1);
        if (result == 0) result = scePthreadMutexattrSettype(&attributes, 2);
        if (result == 0) result = scePthreadMutexInit(&allocated.Mutex, &attributes, "SceFontMutex");
    } catch (...) {
        if (attributes) scePthreadMutexattrDestroy(&attributes);
        ReleaseRegion_nid_no_patch(allocated);
        throw;
    }
    const int attributeResult = attributes ? scePthreadMutexattrDestroy(&attributes) : 0;
    if (result != 0 || attributeResult != 0) {
        ReleaseRegion_nid_no_patch(allocated);
        if (attributeResult != 0) throw std::runtime_error("Font graphics mutex attributes cleanup failed");
        return SCE_FONT_ERROR_ALLOCATION_FAILED;
    }
    region = allocated;
    return SCE_FONT_OK;
}

void FontGraphics::ReleaseRegion_nid_no_patch(MemoryRegion region) {
    const int memoryResult = region.Size ? sceKernelReleaseDirectMemory(region.PhysicalOffset, region.Size) : 0;
    const int mutexResult = region.Mutex ? scePthreadMutexDestroy(&region.Mutex) : 0;
    if (memoryResult != 0) throw std::runtime_error("Font graphics direct memory release failed");
    if (mutexResult != 0) throw std::runtime_error("Font graphics mutex cleanup failed");
}
