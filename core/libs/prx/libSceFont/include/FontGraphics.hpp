#ifndef CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICS_HPP
#define CORE_LIBS_PRX_LIBSCEFONT_INCLUDE_FONTGRAPHICS_HPP

#include <cstddef>
#include <cstdint>

#include "prx/libc/include/general/VabiMacros.hpp"

struct FontMemory;

struct FontGraphicsBackendImport {
    std::uint32_t Id;
    std::uint32_t Reserved;
    void* Value;
};

struct FontGraphicsBackendImports {
    std::uint32_t Reserved;
    std::uint32_t Count;
    const FontGraphicsBackendImport* Items;
};

using FontGraphicsBuildBackend = int (APS5_VABI*)(void*, const FontGraphicsBackendImports*);
using FontGraphicsSelectBackend = void (APS5_VABI*)(void*, FontGraphicsBuildBackend);

struct FontGraphicsServiceDetail {
    std::uint16_t Tag;
    std::uint16_t Flags;
    std::uint32_t Size;
    std::uintptr_t* SharedContext;
    std::uint32_t WorkspaceSize;
    std::uint32_t Reserved;
    FontGraphicsSelectBackend SelectBackend;
};

static_assert(sizeof(FontGraphicsServiceDetail) == 32);
static_assert(offsetof(FontGraphicsServiceDetail, SelectBackend) == 24);

struct FontGraphicsRegion {
    float X;
    float Y;
    float Width;
    float Height;
    std::uint32_t Mode;
    std::uint32_t Tag;
    float OuterRadius;
    float InnerRadius;
};

struct FontGraphicsFillRates {
    float Mapping[4];
    const void* Color;
    std::uint16_t Tag;
    std::uint16_t Effect;
    std::uint16_t Layout;
    std::uint8_t Flags;
    std::uint8_t Reserved;
    std::uint64_t ReservedWords[4];
};

struct FontGraphicsFillPlot {
    float Mapping[4];
    const FontGraphicsRegion* Region;
    std::uint16_t Tag;
    std::uint16_t Reserved;
    std::uint16_t Layout;
    std::uint8_t Flags;
    std::uint8_t ReservedByte;
    std::uint64_t ReservedWords[4];
};

struct FontGraphicsFillMethod {
    std::uint32_t Source;
    std::uint32_t Blend;
    std::uint32_t Flags;
    std::uint32_t Tag;
    std::uint64_t ReservedWords[2];
};

static_assert(sizeof(FontGraphicsRegion) == 32);
static_assert(sizeof(FontGraphicsFillRates) == 64);
static_assert(sizeof(FontGraphicsFillPlot) == 64);
static_assert(sizeof(FontGraphicsFillMethod) == 32);
static_assert(offsetof(FontGraphicsFillRates, Color) == 16);
static_assert(offsetof(FontGraphicsFillRates, Tag) == 24);
static_assert(offsetof(FontGraphicsFillPlot, Region) == 16);
static_assert(offsetof(FontGraphicsFillPlot, Flags) == 30);

extern "C" {
int APS5_VABI sceFontCreateGraphicsService(const FontMemory* memory, const FontGraphicsServiceDetail* detail, void** service);
int APS5_VABI sceFontCreateGraphicsServiceWithEdition(const FontMemory* memory, const FontGraphicsServiceDetail* detail, const void* edition, void** service);
int APS5_VABI sceFontDestroyGraphicsService(void** service);
int APS5_VABI sceFontGraphicsRegionInit(FontGraphicsRegion* region, float x, float y, float width, float height);
int APS5_VABI sceFontGraphicsRegionInitRoundish(FontGraphicsRegion* region, std::uint32_t mode, float x, float y, float width, float height, float outerRadius, float innerRadius);
int APS5_VABI sceFontGraphicsRegionInitCircular(FontGraphicsRegion* region, float x, float y, float outerRadius, float innerRadius);
int APS5_VABI sceFontGraphicsFillRatesInit(FontGraphicsFillRates* rates, const void* color, std::uint32_t effect);
int APS5_VABI sceFontGraphicsFillRatesSetFillEffect(FontGraphicsFillRates* rates, const void* color, std::uint32_t effect);
int APS5_VABI sceFontGraphicsFillRatesSetMapping(FontGraphicsFillRates* rates, float x, float y, float width, float height);
int APS5_VABI sceFontGraphicsFillRatesSetLayout(FontGraphicsFillRates* rates, std::int32_t layout, std::uint32_t alignment);
int APS5_VABI sceFontGraphicsFillMethodInit(FontGraphicsFillMethod* method, std::uint32_t source, std::uint32_t blend);
int APS5_VABI sceFontGraphicsFillPlotInit(FontGraphicsFillPlot* plot, const FontGraphicsRegion* region);
int APS5_VABI sceFontGraphicsFillPlotSetMapping(FontGraphicsFillPlot* plot, float x, float y, float width, float height);
int APS5_VABI sceFontGraphicsFillPlotSetLayout(FontGraphicsFillPlot* plot, std::int32_t layout, std::uint32_t alignment);
}

#endif
