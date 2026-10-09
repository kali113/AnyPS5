#include "prx/libSceFont/include/FontGraphics.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Font graphics service check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

std::atomic<int> allocations{0};
std::atomic<int> initializations{0};
std::atomic<int> finalizations{0};
std::atomic<int> initializationResult{0};
std::atomic<bool> allocationFailure{false};
std::atomic<bool> checkTable{false};
std::atomic<int> selectionResult{0};
std::atomic<int> callbackException{0};

void* APS5_VABI Allocate(void* object, std::uint32_t size) {
    Require(object == reinterpret_cast<void*>(0x1234));
    if (allocationFailure) return nullptr;
    void* result = std::malloc(size);
    if (result) ++allocations;
    return result;
}

void APS5_VABI Deallocate(void* object, void* pointer) {
    Require(object == reinterpret_cast<void*>(0x1234));
    if (pointer) --allocations;
    std::free(pointer);
}

int APS5_VABI Initialize(void* context, void* workspace, std::size_t size) {
    Require(reinterpret_cast<std::uintptr_t>(context) % 65536 == 0);
    Require(static_cast<char*>(workspace) - static_cast<char*>(context) == 280 && size == 65536 - 280);
    const std::uint32_t contextSize = 512;
    std::memcpy(static_cast<char*>(context) + 16, &contextSize, sizeof(contextSize));
    ++initializations;
    if (callbackException == 2) throw std::runtime_error("Injected initialization failure");
    return initializationResult;
}

void APS5_VABI Finalize(void*) {
    ++finalizations;
    if (callbackException == 3) throw std::runtime_error("Injected finalization failure");
}

void APS5_VABI Select(void* table, FontGraphicsBuildBackend build) {
    if (callbackException == 1) throw std::runtime_error("Injected selection failure");
    const FontGraphicsBackendImport entries[]{
        {0x47000001, 0, reinterpret_cast<void*>(&Initialize)},
        {0x47000002, 0, reinterpret_cast<void*>(&Finalize)},
        {0x47000004, 0, reinterpret_cast<void*>(0x0000008000000040ULL)},
        {0x470000D4, 0, reinterpret_cast<void*>(0x1111)},
        {0x470000D4, 0, nullptr},
        {0x470000D4, 0, reinterpret_cast<void*>(0x2222)},
        {0x12345678, 0, reinterpret_cast<void*>(0x3333)}
    };
    const FontGraphicsBackendImports imports{0, static_cast<std::uint32_t>(std::size(entries)), entries};
    selectionResult = build(table, &imports);
    if (checkTable) {
        const auto* values = static_cast<void**>(table);
        Require(values[0] == reinterpret_cast<void*>(&Initialize) && values[1] == reinterpret_cast<void*>(&Finalize));
        Require(values[3] == reinterpret_cast<void*>(0x0000004000000080ULL));
        Require(values[13] == reinterpret_cast<void*>(0x2222));
        Require(values[21] == nullptr && values[27] == nullptr);
    }
}

FontMemory MakeMemory(const FontMemoryInterface* interface) {
    return {0xF00, 0, 0, nullptr, reinterpret_cast<void*>(0x1234), interface, nullptr, nullptr, nullptr, nullptr};
}

void CheckErrors() {
    FontMemoryInterface interface{Allocate, Deallocate, nullptr, nullptr, nullptr, nullptr};
    auto memory = MakeMemory(&interface);
    std::uintptr_t shared = 0;
    FontGraphicsServiceDetail detail{0xE08, 0, 256, &shared, 65536, 0, Select};
    void* service = reinterpret_cast<void*>(0x1234);
    Require(sceFontCreateGraphicsService(nullptr, &detail, &service) == SCE_FONT_ERROR_INVALID_PARAMETER && !service);
    Require(sceFontCreateGraphicsService(&memory, nullptr, &service) == SCE_FONT_ERROR_INVALID_PARAMETER && !service);
    Require(sceFontCreateGraphicsService(&memory, &detail, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    detail.Size = 255;
    Require(sceFontCreateGraphicsService(&memory, &detail, &service) == SCE_FONT_ERROR_FATAL && !service);
    detail.Size = 256;
    detail.WorkspaceSize = 280;
    Require(sceFontCreateGraphicsService(&memory, &detail, &service) == SCE_FONT_ERROR_INVALID_PARAMETER && !service && !shared && allocations == 0);
    detail.WorkspaceSize = 65536;
    allocationFailure = true;
    Require(sceFontCreateGraphicsService(&memory, &detail, &service) == SCE_FONT_ERROR_ALLOCATION_FAILED && !service && !shared);
    allocationFailure = false;
    const int before = initializations;
    initializationResult = 7;
    Require(sceFontCreateGraphicsService(&memory, &detail, &service) == SCE_FONT_ERROR_FATAL && !service && !shared && allocations == 0);
    Require(initializations == before + 1);
    initializationResult = 0;
    Require(sceFontCreateGraphicsService(&memory, &detail, &service) == 0 && service && shared);
    Require(sceFontDestroyGraphicsService(&service) == 0 && !service && !shared && allocations == 0);
    Require(sceFontDestroyGraphicsService(nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontDestroyGraphicsService(&service) == static_cast<int>(0x80460080));
}

void CheckSharing() {
    FontMemoryInterface interface{Allocate, Deallocate, nullptr, nullptr, nullptr, nullptr};
    auto memory = MakeMemory(&interface);
    std::uintptr_t shared = 0;
    FontGraphicsServiceDetail detail{0xE08, 0x1234, 256, &shared, 65536, 0, Select};
    void* first = nullptr;
    void* second = nullptr;
    const int beforeInit = initializations;
    const int beforeFinal = finalizations;
    checkTable = true;
    Require(sceFontCreateGraphicsService(&memory, &detail, &first) == 0 && first && shared);
    checkTable = false;
    Require(selectionResult == 0);
    const auto originalShared = shared;
    Require(sceFontCreateGraphicsServiceWithEdition(&memory, &detail, reinterpret_cast<void*>(1), &second) == 0);
    Require(first != second && shared == originalShared && initializations == beforeInit + 1 && allocations == 2);
    interface.alloc = nullptr;
    interface.dealloc = nullptr;
    Require(sceFontDestroyGraphicsService(&first) == 0 && !first && shared == originalShared && allocations == 1 && finalizations == beforeFinal);
    Require(sceFontDestroyGraphicsService(&second) == 0 && !second && shared == 0 && allocations == 0 && finalizations == beforeFinal + 1);
}

void CheckExceptions() {
    const FontMemoryInterface interface{Allocate, Deallocate, nullptr, nullptr, nullptr, nullptr};
    const auto memory = MakeMemory(&interface);
    std::uintptr_t shared = 0;
    const FontGraphicsServiceDetail detail{0xE08, 0, 256, &shared, 65536, 0, Select};
    void* service = nullptr;
    for (int failure = 1; failure <= 3; ++failure) {
        callbackException = failure;
        bool threw = false;
        try {
            Require(sceFontCreateGraphicsService(&memory, &detail, &service) == 0);
            Require(sceFontDestroyGraphicsService(&service) == 0);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw && !service && !shared && allocations == 0);
    }
    callbackException = 0;
    Require(sceFontCreateGraphicsService(&memory, &detail, &service) == 0);
    Require(sceFontDestroyGraphicsService(&service) == 0 && !service && !shared && allocations == 0);
}

void CheckConcurrentSharing() {
    const FontMemoryInterface interface{Allocate, Deallocate, nullptr, nullptr, nullptr, nullptr};
    const auto memory = MakeMemory(&interface);
    std::uintptr_t shared = 0;
    const FontGraphicsServiceDetail detail{0xE08, 0, 256, &shared, 65536, 0, Select};
    void* anchor = nullptr;
    Require(sceFontCreateGraphicsService(&memory, &detail, &anchor) == 0);
    const int beforeInit = initializations;
    const int beforeFinal = finalizations;
    std::vector<std::thread> workers;
    for (int i = 0; i < 8; ++i) workers.emplace_back([&] {
        for (int iteration = 0; iteration < 100; ++iteration) {
            void* service = nullptr;
            Require(sceFontCreateGraphicsService(&memory, &detail, &service) == 0);
            Require(sceFontDestroyGraphicsService(&service) == 0 && !service);
        }
    });
    for (auto& worker : workers) worker.join();
    Require(allocations == 1 && initializations == beforeInit && finalizations == beforeFinal);
    Require(sceFontDestroyGraphicsService(&anchor) == 0 && shared == 0 && allocations == 0 && finalizations == beforeFinal + 1);
}

}

int main() {
    CheckErrors();
    CheckSharing();
    CheckConcurrentSharing();
    CheckExceptions();
    std::puts("Font graphics service lifecycle, rollback and concurrent sharing passed");
    return 0;
}
