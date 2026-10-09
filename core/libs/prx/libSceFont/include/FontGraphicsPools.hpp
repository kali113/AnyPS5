#ifndef CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICSPOOLS_HPP
#define CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICSPOOLS_HPP

#include "prx/libSceFont/include/FontGraphicsMemory.hpp"

namespace FontGraphics {

struct MainNode {
    std::uint32_t Count;
    std::uint32_t Previous;
    std::uint32_t Next;
};

struct MainPool {
    char* Base;
    std::uint32_t Size;
    std::uint32_t BlockSize;
    std::uint32_t Count;
    std::uint32_t Used;
    MainNode* Nodes;
    MainNode* First;
    MainNode* Last;
    MainNode Sentinel;
    std::uint32_t Reserved;
    PthreadMutex* Mutex;
    std::uint32_t SnapshotUsed;
    std::uint32_t Peak;
};

struct PoolSegment {
    char* Base;
    std::uint32_t Size;
    std::uint32_t BlockSize;
    std::uint32_t Count;
    std::uint32_t Used;
    std::uint16_t* Metadata;
    std::uint16_t* First;
    std::uint16_t Sentinel;
    std::uint16_t Reserved[3];
};

struct Pool {
    PthreadMutex* Mutex;
    std::uint32_t BlockSpan;
    std::uint32_t SegmentCount;
    std::uint32_t Used;
    std::uint32_t Peak;
    void* Storage;
};

static_assert(sizeof(MainNode) == 12);
static_assert(sizeof(MainPool) == 80);
static_assert(sizeof(PoolSegment) == 48);
static_assert(sizeof(Pool) == 32);

void InitializeMainPool_nid_no_patch(MainPool& pool, MemoryRegion& region, MainNode* nodes);
void* AllocateMainPool_nid_no_patch(MainPool& pool, std::uint32_t bytes);
std::uint32_t FreeMainPool_nid_no_patch(MainPool& pool, void* address);
void* AllocatePool_nid_no_patch(Pool& pool, std::uint32_t bytes);
std::uint32_t FreePool_nid_no_patch(Pool& pool, void* address, std::uint32_t bytes);
int CreateCommandPool_nid_no_patch(MainPool& commands, std::uint32_t bytes, Pool*& output);
int CreateTexturePool_nid_no_patch(MainPool& commands, MainPool& textures, std::uint32_t bytes, Pool*& output);

}

#endif
