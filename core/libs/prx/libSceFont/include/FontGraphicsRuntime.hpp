#ifndef CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICSRUNTIME_HPP
#define CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICSRUNTIME_HPP

#include "prx/libSceFont/include/FontGraphicsPools.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"

#include <array>

namespace FontGraphics {

struct GraphicsDevice {
    std::uint16_t Tag;
    std::uint16_t Reserved;
    std::uint32_t Flags;
    FontMemory Memory;
    FontMemoryInterface Interface;
    void* Service;
    FontGraphics::MemoryRegion Commands;
    FontGraphics::MemoryRegion Textures;
    FontGraphics::MainPool CommandMain;
    FontGraphics::MainPool TextureMain;
    FontGraphics::Pool* CommandPool;
    FontGraphics::Pool* TexturePool;
    std::array<std::uintptr_t, 8>* Cache;
    std::uint32_t PreviousCommandUsage;
    std::uint32_t PreviousTextureUsage;
    std::uint32_t PreviousCommandPoolUsage;
    std::uint32_t PreviousTexturePoolUsage;
    std::array<std::uint8_t, 120> ReservedBytes;
};

static_assert(sizeof(GraphicsDevice) == 512);
static_assert(offsetof(GraphicsDevice, Commands) == 128);
static_assert(offsetof(GraphicsDevice, CommandPool) == 352);

struct GraphicsDesign {
    std::uint16_t Tag;
    std::uint16_t Mode;
    std::uint32_t Status;
    const FontMemory* Memory;
    GraphicsDevice* Device;
    GraphicsDevice* DrawingDevice;
    void* Compiled;
    void* Reserved;
    void* Objects;
    void* LastObject;
    std::array<std::byte, 272> Root;
    void* ActiveDrawing;
};

static_assert(sizeof(GraphicsDesign) == 344);
static_assert(offsetof(GraphicsDesign, Root) == 64);
static_assert(offsetof(GraphicsDesign, ActiveDrawing) == 336);

bool ValidDevice_nid_no_patch(const GraphicsDevice* device);

}

#endif
