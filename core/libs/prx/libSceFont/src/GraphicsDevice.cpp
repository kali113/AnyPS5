#include "prx/libSceFont/include/FontGraphics.hpp"
#include "prx/libSceFont/include/FontGraphicsRuntime.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>

namespace {

constexpr int InvalidDevice = static_cast<int>(0x80460081);
constexpr int CommandAllocationFailed = static_cast<int>(0x804600A0);
constexpr int TextureAllocationFailed = static_cast<int>(0x804600A1);
constexpr int CacheAllocationFailed = static_cast<int>(0x804600A2);

using FontGraphics::GraphicsDevice;

bool ValidBuffer(void* address, std::uint32_t size) {
    return size >= 512 && size <= 0x40000000 && reinterpret_cast<std::uintptr_t>(address) % 512 == 0 && (address || size % 65536 == 0);
}

int InitializeRegion(void* address, std::uint32_t size, FontGraphics::MemoryRegion& region) {
    if (!address) return FontGraphics::AllocateRegion_nid_no_patch(size, region);
    region = {address, size, 0, nullptr};
    return scePthreadMutexInit(&region.Mutex, nullptr, "SceFontMutex");
}

void ReleaseRegions(GraphicsDevice& device) {
    auto commands = device.Commands;
    auto textures = device.Textures;
    device.Commands = {};
    device.Textures = {};
    if (!(device.Flags & 1)) commands.Size = 0;
    if (!(device.Flags & 2)) textures.Size = 0;
    std::exception_ptr failure;
    try { FontGraphics::ReleaseRegion_nid_no_patch(commands); } catch (...) { failure = std::current_exception(); }
    try { FontGraphics::ReleaseRegion_nid_no_patch(textures); } catch (...) { if (!failure) failure = std::current_exception(); }
    if (failure) std::rethrow_exception(failure);
}

int InitializePools(GraphicsDevice& device, const FontGraphicsDeviceDetail& detail) {
    auto* nodes = reinterpret_cast<FontGraphics::MainNode*>(&device + 1);
    FontGraphics::InitializeMainPool_nid_no_patch(device.CommandMain, device.Commands, nodes);
    nodes += device.CommandMain.Count;
    FontGraphics::InitializeMainPool_nid_no_patch(device.TextureMain, device.Textures, nodes);
    int result = FontGraphics::CreateCommandPool_nid_no_patch(device.CommandMain, detail.InitialCommandSize, device.CommandPool);
    if (result != 0) return result;
    device.Cache = static_cast<std::array<std::uintptr_t, 8>*>(FontGraphics::AllocatePool_nid_no_patch(*device.CommandPool, 64));
    if (!device.Cache) return CacheAllocationFailed;
    *device.Cache = {};
    return detail.InitialTextureSize >= 64
        ? FontGraphics::CreateTexturePool_nid_no_patch(device.CommandMain, device.TextureMain, detail.InitialTextureSize, device.TexturePool)
        : SCE_FONT_OK;
}

bool ReadMainUsage(FontGraphics::MainPool& pool, std::uint32_t& previous, FontGraphicsPoolUsage& usage) {
    if (scePthreadMutexLock(pool.Mutex) != 0) return false;
    const auto used = pool.Used * pool.BlockSize;
    usage = {pool.Size, used, pool.Size - used, pool.Peak * pool.BlockSize, std::bit_cast<std::int32_t>(used - previous)};
    previous = used;
    return scePthreadMutexUnlock(pool.Mutex) == 0;
}

bool ReadPoolUsage(FontGraphics::Pool* pool, std::uint32_t& previous, FontGraphicsPoolUsage& usage) {
    if (!pool || scePthreadMutexLock(pool->Mutex) != 0) return false;
    auto* segments = reinterpret_cast<const FontGraphics::PoolSegment*>(pool + 1);
    std::uint32_t size = 0;
    std::uint32_t used = 0;
    for (std::uint32_t i = 0; i < pool->SegmentCount; ++i) {
        size += segments[i].Size;
        used += segments[i].BlockSize * segments[i].Used;
    }
    usage = {size, used, size - used, pool->Peak * segments[0].BlockSize, std::bit_cast<std::int32_t>(used - previous)};
    previous = used;
    return scePthreadMutexUnlock(pool->Mutex) == 0;
}

}

bool FontGraphics::ValidDevice_nid_no_patch(const GraphicsDevice* device) {
    return device && device != reinterpret_cast<const GraphicsDevice*>(~std::uintptr_t{0}) && device->Tag == 0xF50 && device->Service && *static_cast<const std::uint16_t*>(device->Service) == 0xF0E;
}

extern "C" {

int APS5_VABI sceFontCreateGraphicsDevice(const FontMemory* memory, const FontGraphicsDeviceDetail* detail, void** device) {
    if (device) *device = nullptr;
    if (!memory) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (memory->mem_kind != 0xF00 || !memory->iface || !memory->iface->alloc || !memory->iface->dealloc) return SCE_FONT_ERROR_INVALID_MEMORY;
    if (!detail || detail->Tag != 0xFD6 || detail->Flags || detail->Reserved || !detail->Service || *static_cast<const std::uint16_t*>(detail->Service) != 0xF0E) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!ValidBuffer(detail->Commands, detail->CommandSize) || !ValidBuffer(detail->Textures, detail->TextureSize)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!detail->InitialCommandSize || detail->InitialCommandSize % 512 || detail->InitialTextureSize % 512) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (detail->InitialCommandSize > detail->CommandSize) return CommandAllocationFailed;
    if (detail->InitialTextureSize > detail->TextureSize) return TextureAllocationFailed;
    if (!device) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const auto bytes = static_cast<std::uint32_t>(sizeof(GraphicsDevice) + sizeof(FontGraphics::MainNode) * (detail->CommandSize / 512 + detail->TextureSize / 512));
    auto* created = static_cast<GraphicsDevice*>(memory->iface->alloc(memory->mspace_handle, bytes));
    if (!created) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    const auto deallocate = [function = memory->iface->dealloc, object = memory->mspace_handle](GraphicsDevice* value) { function(object, value); };
    std::unique_ptr<GraphicsDevice, decltype(deallocate)> owner(created, deallocate);
    std::memset(created, 0, bytes);
    created->Memory = *memory;
    created->Interface = *memory->iface;
    created->Memory.iface = &created->Interface;
    created->Service = detail->Service;
    int result;
    try {
        result = InitializeRegion(detail->Commands, detail->CommandSize, created->Commands);
        if (result == 0) {
            if (!detail->Commands) created->Flags |= 1;
            result = InitializeRegion(detail->Textures, detail->TextureSize, created->Textures);
            if (result == 0) {
                if (!detail->Textures) created->Flags |= 2;
                result = InitializePools(*created, *detail);
            } else result = TextureAllocationFailed;
        } else result = CommandAllocationFailed;
    } catch (...) {
        ReleaseRegions(*created);
        throw;
    }
    if (result != 0) {
        ReleaseRegions(*created);
        return result;
    }
    created->Tag = 0xF50;
    *device = owner.release();
    return SCE_FONT_OK;
}

int APS5_VABI sceFontDestroyGraphicsDevice(void** device) {
    if (!device) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* current = static_cast<GraphicsDevice*>(*device);
    if (!current || current->Tag != 0xF50 || !current->Service || *static_cast<const std::uint16_t*>(current->Service) != 0xF0E || current->Memory.mem_kind != 0xF00 || current->Memory.iface != &current->Interface || !current->Interface.alloc || !current->Interface.dealloc) return InvalidDevice;
    if (current->Cache && std::any_of(current->Cache->begin(), current->Cache->end(), [](auto value) { return value != 0; })) throw std::runtime_error("Font graphics indexed glyph cache cleanup is not implemented");
    const auto deallocate = [function = current->Interface.dealloc, object = current->Memory.mspace_handle](GraphicsDevice* value) { function(object, value); };
    std::unique_ptr<GraphicsDevice, decltype(deallocate)> owner(current, deallocate);
    *device = nullptr;
    current->Tag = 0;
    ReleaseRegions(*current);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsGetDeviceUsage(void* device, FontGraphicsDeviceUsage* usage) {
    auto* current = static_cast<GraphicsDevice*>(device);
    if (!current || device == reinterpret_cast<void*>(~std::uintptr_t{0}) || current->Tag != 0xF50 || !current->Service || *static_cast<const std::uint16_t*>(current->Service) != 0xF0E) {
        if (usage) *usage = {};
        return InvalidDevice;
    }
    if (!usage) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *usage = {};
    FontGraphicsDeviceUsage result{};
    if (!ReadMainUsage(current->CommandMain, current->PreviousCommandUsage, result.Commands.Main) ||
        !ReadPoolUsage(current->CommandPool, current->PreviousCommandPoolUsage, result.Commands.Pool) ||
        !ReadMainUsage(current->TextureMain, current->PreviousTextureUsage, result.Textures.Main) ||
        !ReadPoolUsage(current->TexturePool, current->PreviousTexturePoolUsage, result.Textures.Pool)) return SCE_FONT_ERROR_FATAL;
    result.Commands.Address = current->Flags & 1 ? nullptr : current->Commands.Address;
    result.Textures.Address = current->Flags & 2 ? nullptr : current->Textures.Address;
    *usage = result;
    return SCE_FONT_OK;
}

}

