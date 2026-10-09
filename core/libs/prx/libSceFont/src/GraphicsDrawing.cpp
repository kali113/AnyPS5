#include "prx/libSceFont/include/FontGraphics.hpp"
#include "prx/libSceFont/include/FontGraphicsRuntime.hpp"

#include <cstring>
#include <stdexcept>

namespace {

using FontGraphics::GraphicsDesign;
using FontGraphics::GraphicsDevice;
constexpr int InvalidDevice = static_cast<int>(0x80460081);
constexpr int InvalidDesign = static_cast<int>(0x80460083);
constexpr int InvalidDrawing = static_cast<int>(0x80460084);
constexpr int CacheAllocationFailed = static_cast<int>(0x804600A2);
constexpr int DesignNotReady = static_cast<int>(0x804600A7);
constexpr int AlreadyDrawing = static_cast<int>(0x804600A8);

struct Drawing {
    std::array<std::byte, 256> Data;
};

template<class T> T Read(const void* object, std::size_t offset) {
    T result;
    std::memcpy(&result, static_cast<const std::byte*>(object) + offset, sizeof(result));
    return result;
}

template<class T> void Write(void* object, std::size_t offset, T value) {
    std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
}

void FreeMain(GraphicsDevice& device, void* pointer) {
    if (pointer && !FontGraphics::FreeMainPool_nid_no_patch(device.CommandMain, pointer)) throw std::runtime_error("Font graphics command allocation is invalid");
}

void FreeChain(GraphicsDevice& device, void* first) {
    while (first) {
        auto* next = Read<void*>(first, 0);
        FreeMain(device, first);
        first = next;
    }
}

void CheckClearable(const GraphicsDesign& design) {
    if (design.Compiled || Read<void*>(design.Root.data(), 264)) throw std::runtime_error("Font graphics compiled design cleanup is not implemented");
    if (design.ActiveDrawing && (Read<void*>(design.ActiveDrawing, 80) || Read<void*>(design.ActiveDrawing, 96))) throw std::runtime_error("Font graphics drawing reference cleanup is not implemented");
}

void ClearDesign(GraphicsDesign& design) {
    CheckClearable(design);
    auto& device = *design.Device;
    auto* object = design.Objects;
    std::uint8_t previousKind = 0;
    std::uint32_t previousId = ~std::uint32_t{0};
    while (object) {
        auto* next = Read<void*>(object, 8);
        const auto kind = Read<std::uint8_t>(object, 0);
        const auto id = Read<std::uint32_t>(object, 4);
        if (kind != previousKind || id != previousId) {
            if (!FontGraphics::FreePool_nid_no_patch(*device.CommandPool, object, Read<std::uint8_t>(object, 1))) throw std::runtime_error("Font graphics object allocation is invalid");
            previousKind = kind;
            previousId = id;
        }
        object = next;
    }
    design.Objects = nullptr;
    design.LastObject = nullptr;
    void* frame = design.Root.data();
    while (frame) {
        auto* next = Read<void*>(frame, 16);
        FreeMain(device, Read<void*>(frame, 56));
        Write<void*>(frame, 56, nullptr);
        if (frame != design.Root.data()) FreeMain(device, frame);
        frame = next;
    }
    Write<std::uint16_t>(design.Root.data(), 248, 0);
}

void CancelSession(GraphicsDesign& design) {
    CheckClearable(design);
    auto* drawing = design.ActiveDrawing;
    auto& device = *design.DrawingDevice;
    FreeMain(device, Read<void*>(drawing, 112));
    FreeChain(device, Read<void*>(drawing, 24));
    void* frame = design.Root.data();
    while (frame) {
        FreeChain(device, Read<void*>(frame, 208));
        FreeChain(device, Read<void*>(frame, 232));
        std::memset(static_cast<std::byte*>(frame) + 200, 0, 48);
        frame = Read<void*>(frame, 16);
    }
    const auto* memory = design.Memory;
    design.DrawingDevice = nullptr;
    design.ActiveDrawing = nullptr;
    Write<std::uint16_t>(drawing, 0, 0);
    memory->iface->dealloc(memory->mspace_handle, drawing);
}

}

extern "C" {

int APS5_VABI sceFontGraphicsStructureDesign(void* device, std::uint32_t mode, void** design) {
    if (design) *design = nullptr;
    auto* current = static_cast<GraphicsDevice*>(device);
    if (!FontGraphics::ValidDevice_nid_no_patch(current) || current->Memory.mem_kind != 0xF00 || current->Memory.iface != &current->Interface || !current->Interface.alloc || !current->Interface.dealloc) return InvalidDevice;
    if (!design || mode < 1 || mode > 4) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!current->CommandPool) return CacheAllocationFailed;
    auto* created = static_cast<GraphicsDesign*>(FontGraphics::AllocatePool_nid_no_patch(*current->CommandPool, sizeof(GraphicsDesign)));
    if (!created) return CacheAllocationFailed;
    *created = {};
    constexpr std::uint16_t modes[]{0x10, 0x40, 0x30, 0x4A};
    created->Tag = 0xF51;
    created->Mode = modes[mode - 1];
    created->Device = current;
    *design = created;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsDesignStartDrawing(void* design, void* device, void** drawing, void** handle) {
    if (drawing) *drawing = nullptr;
    if (handle) *handle = nullptr;
    auto* current = static_cast<GraphicsDesign*>(design);
    if (!current || current->Tag != 0xF51) return InvalidDesign;
    auto* selected = device == reinterpret_cast<void*>(~std::uintptr_t{0}) ? current->Device : static_cast<GraphicsDevice*>(device);
    if (!FontGraphics::ValidDevice_nid_no_patch(selected)) return InvalidDevice;
    if (current->Compiled && current->Status != 1) return DesignNotReady;
    if (!drawing) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (current->ActiveDrawing) return AlreadyDrawing;
    ClearDesign(*current);
    auto* created = static_cast<Drawing*>(selected->Interface.alloc(selected->Memory.mspace_handle, sizeof(Drawing)));
    if (!created) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    *created = {};
    current->DrawingDevice = selected;
    current->Memory = &selected->Memory;
    current->ActiveDrawing = created;
    auto* frame = current->Root.data();
    current->Root = {};
    Write<std::uint16_t>(frame, 0, 0xF54);
    Write<std::uint8_t>(frame, 2, current->Mode == 0x10 ? 1 : static_cast<std::uint8_t>(current->Mode));
    Write<const FontMemory**>(frame, 40, &current->Memory);
    for (auto offset : {64u, 84u, 104u, 124u, 160u, 164u}) Write<float>(frame, offset, 1.0f);
    Write<std::uint32_t>(frame, 200, 96);
    Write<std::uint32_t>(frame, 224, 96);
    Write<std::uint16_t>(frame, 248, current->Mode);
    Write<std::uint16_t>(created, 0, 0xF53);
    Write<std::uint8_t>(created, 2, 3);
    Write<const FontMemory**>(created, 8, &current->Memory);
    Write<std::uint32_t>(created, 16, 504);
    for (auto offset : {48u, 56u, 64u}) Write<void*>(created, offset, frame);
    *drawing = created;
    if (handle) *handle = frame;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsDrawingCancel(void** drawing) {
    if (!drawing) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (!*drawing || Read<std::uint16_t>(*drawing, 0) != 0xF53) return InvalidDrawing;
    auto* parent = Read<std::byte*>(*drawing, 8);
    if (!parent) return SCE_FONT_ERROR_FATAL;
    auto* design = reinterpret_cast<GraphicsDesign*>(parent - offsetof(GraphicsDesign, Memory));
    if (!FontGraphics::ValidDevice_nid_no_patch(design->DrawingDevice)) return SCE_FONT_ERROR_FATAL;
    CancelSession(*design);
    ClearDesign(*design);
    *drawing = nullptr;
    return SCE_FONT_OK;
}

}

