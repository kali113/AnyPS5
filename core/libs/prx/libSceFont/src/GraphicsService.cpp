#include "prx/libSceFont/include/FontGraphics.hpp"
#include "prx/libSceFont/include/FontGraphicsMemory.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"

#include <array>
#include <atomic>
#include <bit>
#include <cstring>
#include <limits>
#include <memory>
#include <thread>

namespace {

constexpr int InvalidService = static_cast<int>(0x80460080);

struct BackendTable {
    std::array<void*, 28> Entries;
};

struct SharedContext {
    std::uint32_t Reserved;
    std::uint32_t References;
    std::uint64_t ReservedWord;
    std::uint32_t ContextSize;
    std::uint32_t Flags;
    FontGraphics::MemoryRegion Allocation;
    BackendTable Backend;
};

struct GraphicsService {
    std::uint16_t Tag;
    std::uint16_t Flags;
    std::array<std::uint32_t, 3> Reserved;
    FontMemory Memory;
    FontMemoryInterface Interface;
    std::uintptr_t* Shared;
    std::array<std::uint8_t, 120> ReservedBytes;
};

static_assert(sizeof(BackendTable) == 224);
static_assert(sizeof(SharedContext) == 280);
static_assert(offsetof(SharedContext, Backend) == 56);
static_assert(sizeof(GraphicsService) == 256);
static_assert(offsetof(GraphicsService, Shared) == 128);

class SharedLock {
public:
    explicit SharedLock(std::uintptr_t* slot) : slot(slot) {
        std::atomic_ref<std::uintptr_t> value(*slot);
        for (;;) {
            auto current = value.load(std::memory_order_acquire);
            if (current != std::numeric_limits<std::uintptr_t>::max() &&
                value.compare_exchange_weak(current, std::numeric_limits<std::uintptr_t>::max(), std::memory_order_acq_rel)) {
                Context = reinterpret_cast<SharedContext*>(current);
                break;
            }
            std::this_thread::yield();
        }
    }
    ~SharedLock() {
        std::atomic_ref<std::uintptr_t>(*slot).store(reinterpret_cast<std::uintptr_t>(Context), std::memory_order_release);
    }
    SharedContext* Context = nullptr;
private:
    std::uintptr_t* slot;
};

int APS5_VABI BuildBackend(void* destination, const FontGraphicsBackendImports* imports) {
    auto& table = *static_cast<BackendTable*>(destination);
    table = {};
    if (!imports || !imports->Items || imports->Count > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) return SCE_FONT_ERROR_FATAL;
    for (std::uint32_t i = 0; i < imports->Count; ++i) {
        const auto& entry = imports->Items[i];
        if (!entry.Value) continue;
        std::size_t index;
        switch (entry.Id) {
            case 0x47000001: index = 0; break;
            case 0x47000002: index = 1; break;
            case 0x47000003: index = 2; break;
            case 0x47000004: index = 3; break;
            case 0x47000005: index = 4; break;
            case 0x4700000B: index = 5; break;
            case 0x4700002D: index = 6; break;
            case 0x4700002E: index = 7; break;
            case 0x470000C0: index = 8; break;
            case 0x470000CB: index = 9; break;
            case 0x470000CE: index = 10; break;
            case 0x470000D1: index = 11; break;
            case 0x470000D2: index = 12; break;
            case 0x470000D4: index = 13; break;
            case 0x470000E0: index = 14; break;
            case 0x470000E1: index = 15; break;
            case 0x470000E2: index = 16; break;
            case 0x470000EC: index = 17; break;
            case 0x470000D0: index = 18; break;
            case 0x470000E3: index = 19; break;
            case 0x470000F0: index = 20; break;
            default: continue;
        }
        table.Entries[index] = entry.Id == 0x47000004
            ? reinterpret_cast<void*>(std::rotr(reinterpret_cast<std::uintptr_t>(entry.Value), 32)) : entry.Value;
    }
    return table.Entries[0] && table.Entries[1] ? SCE_FONT_OK : SCE_FONT_ERROR_FATAL;
}

bool ValidMemory(const FontMemory* memory) {
    return memory && memory->mem_kind == 0xF00 && memory->iface && memory->iface->alloc && memory->iface->dealloc;
}

}

extern "C" {

int APS5_VABI sceFontCreateGraphicsService(const FontMemory* memory, const FontGraphicsServiceDetail* detail, void** service) {
    return sceFontCreateGraphicsServiceWithEdition(memory, detail, nullptr, service);
}

int APS5_VABI sceFontCreateGraphicsServiceWithEdition(const FontMemory* memory, const FontGraphicsServiceDetail* detail, const void*, void** service) {
    if (service) *service = nullptr;
    if (!ValidMemory(memory) || !detail || detail->Tag != 0xE08 || !service) return SCE_FONT_ERROR_INVALID_PARAMETER;
    if (detail->Size < sizeof(GraphicsService)) return SCE_FONT_ERROR_FATAL;
    auto* created = static_cast<GraphicsService*>(memory->iface->alloc(memory->mspace_handle, sizeof(GraphicsService)));
    if (!created) return SCE_FONT_ERROR_ALLOCATION_FAILED;
    const auto deallocate = [function = memory->iface->dealloc, object = memory->mspace_handle](GraphicsService* value) { function(object, value); };
    std::unique_ptr<GraphicsService, decltype(deallocate)> owner(created, deallocate);
    *created = {};
    created->Memory = *memory;
    created->Interface = *memory->iface;
    created->Memory.iface = &created->Interface;
    int result = SCE_FONT_ERROR_INVALID_PARAMETER;
    if (detail->SharedContext && detail->WorkspaceSize >= sizeof(SharedContext) + 1) {
        SharedLock lock(detail->SharedContext);
        auto* context = lock.Context;
        result = SCE_FONT_ERROR_FATAL;
        if (!context) {
            FontGraphics::MemoryRegion region;
            if (FontGraphics::AllocateRegion_nid_no_patch(detail->WorkspaceSize, region) == 0) {
                context = static_cast<SharedContext*>(region.Address);
                std::memset(context, 0, sizeof(SharedContext));
                context->Flags = 1;
                context->Allocation = region;
                try {
                    if (detail->SelectBackend) detail->SelectBackend(&context->Backend, BuildBackend);
                    using Initialize = int (APS5_VABI*)(void*, void*, std::size_t);
                    const auto initialize = reinterpret_cast<Initialize>(context->Backend.Entries[0]);
                    if (!initialize || !context->Backend.Entries[1] || initialize(context, context + 1, detail->WorkspaceSize - sizeof(SharedContext)) != 0) context = nullptr;
                } catch (...) {
                    FontGraphics::ReleaseRegion_nid_no_patch(region);
                    throw;
                }
                if (context) lock.Context = context;
                else FontGraphics::ReleaseRegion_nid_no_patch(region);
            }
        }
        if (context && context->References != std::numeric_limits<std::uint32_t>::max()) {
            ++context->References;
            created->Tag = 0xF0E;
            created->Flags = detail->Flags;
            created->Shared = detail->SharedContext;
            *service = owner.release();
            return SCE_FONT_OK;
        }
    }
    return result;
}

int APS5_VABI sceFontDestroyGraphicsService(void** service) {
    if (!service) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* current = static_cast<GraphicsService*>(*service);
    if (!current || !ValidMemory(&current->Memory)) return InvalidService;
    const auto deallocate = [function = current->Memory.iface->dealloc, object = current->Memory.mspace_handle](GraphicsService* value) { function(object, value); };
    std::unique_ptr<GraphicsService, decltype(deallocate)> owner(current, deallocate);
    *service = nullptr;
    if (current->Shared) {
        SharedLock lock(current->Shared);
        auto* context = lock.Context;
        if (context && context->References && --context->References == 0) {
            using Finalize = void (APS5_VABI*)(void*);
            const auto allocation = context->Allocation;
            lock.Context = nullptr;
            try {
                reinterpret_cast<Finalize>(context->Backend.Entries[1])(context);
            } catch (...) {
                FontGraphics::ReleaseRegion_nid_no_patch(allocation);
                throw;
            }
            FontGraphics::ReleaseRegion_nid_no_patch(allocation);
        }
    }
    return SCE_FONT_OK;
}

}

