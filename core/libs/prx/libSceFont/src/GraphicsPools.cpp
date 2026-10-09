#include "prx/libSceFont/include/FontGraphicsPools.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include "prx/libkernel/Pthread/include/Mutex.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

constexpr std::uint32_t LinkMask = 0x1FFFFF;
constexpr std::uint32_t CountMask = 0x3FFFFF;
constexpr std::uint32_t BlockSize = 64;
constexpr int CommandAllocationFailed = static_cast<int>(0x804600A0);
constexpr int TextureAllocationFailed = static_cast<int>(0x804600A1);

class PoolLock {
public:
    explicit PoolLock(PthreadMutex* mutex) : mutex(mutex) {
        if (scePthreadMutexLock(mutex) != 0) throw std::runtime_error("Font graphics pool lock failed");
    }
    ~PoolLock() { scePthreadMutexUnlock(mutex); }
private:
    PthreadMutex* mutex;
};

void SetLink(std::uint32_t& field, std::uint32_t value) {
    field = (field & ~LinkMask) | (value & LinkMask);
}

FontGraphics::PoolSegment* Segments(FontGraphics::Pool& pool) {
    return reinterpret_cast<FontGraphics::PoolSegment*>(&pool + 1);
}

void InitializeSegments(FontGraphics::Pool& pool, char* data, std::uint32_t blocks, PthreadMutex* mutex, void* storage) {
    const std::uint32_t count = (blocks + 32767) / 32768;
    pool = {mutex, BlockSize * 32768, count, 0, 0, storage};
    auto* segments = Segments(pool);
    auto* metadata = reinterpret_cast<std::uint16_t*>(segments + count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto size = std::min(blocks, 32768u);
        segments[i] = {data, BlockSize * size, BlockSize, size, 0, metadata, metadata, static_cast<std::uint16_t>(size * 2), {}};
        *metadata = 0;
        metadata += size;
        data += size * BlockSize;
        blocks -= size;
    }
}

void* AllocateSegment(FontGraphics::PoolSegment& segment, std::uint32_t blocks) {
    if (blocks == 0 || blocks > segment.Count - segment.Used) return nullptr;
    auto* boundary = segment.First;
    auto* previous = boundary;
    std::uint32_t index = 0;
    std::uint32_t start = 0;
    std::uint32_t available = 0;
    for (;;) {
        const auto value = *boundary;
        auto next = static_cast<std::uint32_t>(value >> 1);
        if (next == 0) next = segment.Count;
        if (value & 1) {
            previous = boundary;
            available = 0;
        } else {
            if (available == 0) start = index;
            available += next - index;
            if (available >= blocks) {
                const auto end = start + blocks;
                for (auto i = start; i < end; ++i) segment.Metadata[i] |= 1;
                if (available == blocks) {
                    const auto link = end >= segment.Count ? static_cast<std::uint16_t>(segment.Count * 2) : static_cast<std::uint16_t>(segment.Metadata[end] & 0xFFFE);
                    *previous = (*previous & 1) | link;
                } else {
                    *previous = (*previous & 1) | static_cast<std::uint16_t>(end * 2);
                    segment.Metadata[end] = value & 0xFFFE;
                }
                segment.Used += blocks;
                return segment.Base + start * segment.BlockSize;
            }
        }
        if (next >= segment.Count) return nullptr;
        boundary = segment.Metadata + next;
        index = next;
    }
}

}

void FontGraphics::InitializeMainPool_nid_no_patch(MainPool& pool, MemoryRegion& region, MainNode* nodes) {
    const auto count = static_cast<std::uint32_t>(region.Size / 512);
    pool = {static_cast<char*>(region.Address), static_cast<std::uint32_t>(region.Size), 512, count, 0, nodes, nodes, nodes, {0, 0, count & LinkMask}, 0, &region.Mutex, 0, 0};
    *nodes = {};
}

void* FontGraphics::AllocateMainPool_nid_no_patch(MainPool& pool, std::uint32_t bytes) {
    PoolLock lock(pool.Mutex);
    const std::uint32_t blocks = (bytes + pool.BlockSize - 1) / pool.BlockSize;
    if (!blocks) return nullptr;
    auto* node = pool.Last;
    auto index = node == pool.First ? 0u : static_cast<std::uint32_t>(node - pool.Nodes);
    for (;;) {
        auto end = node->Next & LinkMask;
        if (!end) end = pool.Count;
        if (!(node->Count & CountMask) && end - index >= blocks) {
            const auto previousIndex = node->Previous & LinkMask;
            auto* previous = previousIndex ? pool.Nodes + previousIndex : pool.First;
            if (end - index == blocks) {
                node->Count = (node->Count & ~CountMask) | blocks;
                if (index + blocks >= pool.Count) SetLink(previous->Next, pool.Count);
                else {
                    const auto next = pool.Nodes[index + blocks].Next & LinkMask;
                    SetLink(previous->Next, next);
                    if (next && next < pool.Count) SetLink(pool.Nodes[next].Previous, previousIndex);
                    else pool.Last = previous;
                }
            } else {
                index = end - blocks;
                SetLink(node->Next, index);
                auto& allocated = pool.Nodes[index];
                allocated.Count = (allocated.Count & ~CountMask) | blocks;
                SetLink(allocated.Previous, static_cast<std::uint32_t>(node - pool.Nodes));
                if (end < pool.Count) end = pool.Nodes[end].Next & LinkMask;
                SetLink(allocated.Next, end);
                if (end && end < pool.Count) SetLink(pool.Nodes[end].Previous, index);
                else pool.Last = &allocated;
            }
            pool.Used += blocks;
            pool.SnapshotUsed = pool.Used;
            pool.Peak = std::max(pool.Peak, pool.Used);
            return pool.Base + index * pool.BlockSize;
        }
        if (!index) return nullptr;
        index = node->Previous & LinkMask;
        node = index ? pool.Nodes + index : pool.First;
    }
}

void* FontGraphics::AllocatePool_nid_no_patch(Pool& pool, std::uint32_t bytes) {
    PoolLock lock(pool.Mutex);
    auto* segments = Segments(pool);
    for (std::uint32_t i = 0; i < pool.SegmentCount; ++i) {
        const auto blocks = (bytes + segments[i].BlockSize - 1) / segments[i].BlockSize;
        if (void* result = AllocateSegment(segments[i], blocks)) {
            pool.Used += blocks;
            pool.Peak = std::max(pool.Peak, pool.Used);
            return result;
        }
    }
    return nullptr;
}

int FontGraphics::CreateCommandPool_nid_no_patch(MainPool& commands, std::uint32_t bytes, Pool*& output) {
    if (bytes < sizeof(Pool)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    constexpr std::uint32_t chunk = (BlockSize + 2) * 32768 + sizeof(PoolSegment);
    const auto remaining = bytes - static_cast<std::uint32_t>(sizeof(Pool));
    const auto full = remaining / chunk;
    const auto tail = remaining % chunk;
    const auto blocks = full * 32768 + (tail >= BlockSize + 2 + sizeof(PoolSegment) ? (tail - sizeof(PoolSegment)) / (BlockSize + 2) : 0);
    if (!blocks) return SCE_FONT_ERROR_INVALID_PARAMETER;
    auto* data = static_cast<char*>(AllocateMainPool_nid_no_patch(commands, bytes));
    if (!data) return CommandAllocationFailed;
    auto* pool = reinterpret_cast<Pool*>(data + blocks * BlockSize);
    InitializeSegments(*pool, data, blocks, commands.Mutex, pool);
    output = pool;
    return SCE_FONT_OK;
}

int FontGraphics::CreateTexturePool_nid_no_patch(MainPool& commands, MainPool& textures, std::uint32_t bytes, Pool*& output) {
    const auto blocks = bytes / BlockSize;
    const auto segments = (blocks + 32767) / 32768;
    if (!blocks) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const auto metadataBytes = static_cast<std::uint32_t>(sizeof(Pool) + segments * sizeof(PoolSegment) + blocks * 2);
    auto* pool = static_cast<Pool*>(AllocateMainPool_nid_no_patch(commands, metadataBytes));
    if (!pool) return CommandAllocationFailed;
    auto* data = static_cast<char*>(AllocateMainPool_nid_no_patch(textures, bytes));
    if (!data) return TextureAllocationFailed;
    InitializeSegments(*pool, data, blocks, commands.Mutex, data + blocks * BlockSize);
    output = pool;
    return SCE_FONT_OK;
}

std::uint32_t FontGraphics::FreePool_nid_no_patch(Pool& pool, void* address, std::uint32_t bytes) {
    PoolLock lock(pool.Mutex);
    auto* segments = Segments(pool);
    const auto base = reinterpret_cast<std::uintptr_t>(segments[0].Base);
    const auto pointer = reinterpret_cast<std::uintptr_t>(address);
    if (pointer < base || pointer >= reinterpret_cast<std::uintptr_t>(pool.Storage)) return 0;
    const auto segmentIndex = static_cast<std::uint32_t>((pointer - base) / pool.BlockSpan);
    if (segmentIndex >= pool.SegmentCount) return 0;
    auto& segment = segments[segmentIndex];
    const auto offset = pointer - reinterpret_cast<std::uintptr_t>(segment.Base);
    const auto index = static_cast<std::uint32_t>(offset / segment.BlockSize);
    const auto blocks = (bytes + segment.BlockSize - 1) / segment.BlockSize;
    if (offset % segment.BlockSize || index >= segment.Count || blocks == 0 || blocks > segment.Used || blocks > segment.Count - index) return 0;
    auto* first = segment.First;
    auto* boundary = first;
    auto* previous = first;
    std::uint32_t start = 0;
    std::uint16_t value = *boundary;
    auto next = static_cast<std::uint32_t>(value >> 1);
    for (std::uint32_t steps = 0; value >= 2 && index >= next; ++steps) {
        if (steps >= segment.Count) return 0;
        previous = boundary;
        start = next;
        boundary = segment.Metadata + next;
        value = *boundary;
        next = value >> 1;
    }
    if (!(value & 1)) return 0;
    const auto end = index + blocks;
    auto* freed = first;
    std::uint16_t link;
    if (start == index) {
        if (end < next) {
            if (index != 0) freed = segment.Metadata + index;
            segment.Metadata[end] = (value & 0xFFFE) | (segment.Metadata[end] & 1);
            link = static_cast<std::uint16_t>(end * 2);
        } else if (index != 0) {
            auto following = end;
            if (end < segment.Count) following = segment.Metadata[end] >> 1;
            freed = segment.Metadata + index;
            link = static_cast<std::uint16_t>(following * 2);
        } else if (end >= segment.Count) {
            segment.Used -= blocks;
            *freed &= 0xFFFE;
            pool.Used -= blocks;
            return blocks;
        } else {
            link = segment.Metadata[end] & 0xFFFE;
            previous = freed;
        }
    } else {
        freed = segment.Metadata + index;
        *boundary = static_cast<std::uint16_t>(index * 2) | (value & 1);
        if (end < next) {
            *freed = (*freed & 1) | static_cast<std::uint16_t>(end * 2);
            previous = segment.Metadata + end;
            link = value & 0xFFFE;
        } else {
            link = end >= segment.Count ? static_cast<std::uint16_t>(end * 2) : static_cast<std::uint16_t>(segment.Metadata[end] & 0xFFFE);
            previous = freed;
        }
    }
    *previous = (*previous & 1) | link;
    *freed &= 0xFFFE;
    segment.Used -= blocks;
    pool.Used -= blocks;
    return blocks;
}

std::uint32_t FontGraphics::FreeMainPool_nid_no_patch(MainPool& pool, void* address) {
    PoolLock lock(pool.Mutex);
    const auto base = reinterpret_cast<std::uintptr_t>(pool.Base);
    const auto pointer = reinterpret_cast<std::uintptr_t>(address);
    if (pointer < base || (pointer - base) % pool.BlockSize) return 0;
    const auto index = static_cast<std::uint32_t>((pointer - base) / pool.BlockSize);
    if (index >= pool.Count) return 0;
    auto* current = pool.First;
    auto next = current->Next & LinkMask;
    std::uint32_t start = 0;
    for (std::uint32_t steps = 0; next && next <= index; ++steps) {
        if (steps >= pool.Count || next >= pool.Count) return 0;
        start = next;
        current = pool.Nodes + next;
        next = current->Next & LinkMask;
    }
    auto* allocation = pool.Nodes + index;
    const auto blocks = allocation->Count & CountMask;
    if (!(current->Count & CountMask) || !blocks || blocks > pool.Used || blocks > pool.Count - index) return 0;
    const auto end = index + blocks;
    if (start == index) {
        if (end < next) {
            const auto previousIndex = current->Previous & LinkMask;
            auto* previous = previousIndex ? pool.Nodes + previousIndex : pool.First;
            auto* tail = pool.Nodes + end;
            SetLink(tail->Previous, previousIndex);
            SetLink(tail->Next, next);
            SetLink(previous->Next, end);
            if (next >= pool.Count) pool.Last = tail;
            else SetLink(pool.Nodes[next].Previous, end);
        } else if (index) {
            const auto previousIndex = allocation->Previous & LinkMask;
            auto* previous = previousIndex ? pool.Nodes + previousIndex : pool.First;
            const auto following = end < pool.Count ? pool.Nodes[end].Next & LinkMask : end;
            SetLink(previous->Next, following);
            if (following && following < pool.Count) SetLink(pool.Nodes[following].Previous, previousIndex);
            else pool.Last = previous;
        } else if (end < pool.Count) {
            const auto following = pool.Nodes[end].Next & LinkMask;
            SetLink(pool.First->Next, following);
            if (following && following < pool.Count) SetLink(pool.Nodes[following].Previous, 0);
            else pool.Last = pool.First;
        }
    } else {
        SetLink(current->Next, index);
        SetLink(allocation->Previous, start);
        if (end >= next) {
            const auto following = end >= pool.Count ? end : pool.Nodes[end].Next & LinkMask;
            SetLink(allocation->Next, following);
            if (following && following < pool.Count) SetLink(pool.Nodes[following].Previous, index);
            else pool.Last = allocation;
        } else {
            SetLink(allocation->Next, end);
            auto* tail = pool.Nodes + end;
            SetLink(tail->Previous, index);
            SetLink(tail->Next, next);
            if (next >= pool.Count) pool.Last = tail;
            else SetLink(pool.Nodes[next].Previous, end);
        }
    }
    allocation->Count &= ~CountMask;
    pool.Used -= blocks;
    pool.SnapshotUsed = pool.Used;
    return blocks;
}
