#include <cstdint>

#include "prx/libSceFont/include/FontGraphics.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"

namespace {

constexpr int InvalidColor = static_cast<int>(0x80460086);
constexpr int InvalidRegion = static_cast<int>(0x80460087);
constexpr int InvalidPlot = static_cast<int>(0x80460088);
constexpr int InvalidRates = static_cast<int>(0x80460089);

bool IsObject(const void* object) {
    return object && reinterpret_cast<std::uintptr_t>(object) != UINTPTR_MAX;
}

bool IsColor(const void* color) {
    const auto value = reinterpret_cast<std::intptr_t>(color);
    if (value >= -1 && value <= 2) return true;
    const auto format = *static_cast<const std::uint8_t*>(color);
    return format >= 0x10 && format <= 0x50 && (format & 15) == 0;
}

bool IsEffect(std::uint32_t effect) {
    return effect < 6 || effect == 8 || effect == 9 || effect == 16;
}

bool IsLayout(std::int32_t layout, std::uint32_t alignment) {
    const bool validLayout = layout == 0 || layout == 0x170 || layout == 0x207 || layout == 0x400 || layout == 0x800 || layout == 0xF77;
    const auto horizontal = alignment & 15;
    const auto vertical = (alignment >> 4) & 15;
    return validLayout && horizontal <= 4 && horizontal != 3 && vertical <= 4 && vertical != 3;
}

template <typename T>
void SetMapping(T& target, float x, float y, float width, float height) {
    target.Mapping[0] = x;
    target.Mapping[1] = y;
    target.Mapping[2] = width;
    target.Mapping[3] = height;
    target.Flags = (target.Flags & 0xFC) | 1;
}

}

extern "C" {

int APS5_VABI sceFontGraphicsRegionInit(FontGraphicsRegion* region, float x, float y, float width, float height) {
    if (!region || width < 0 || height < 0) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *region = {x, y, width, height, 15, 0xF60, 0, 0};
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsRegionInitRoundish(FontGraphicsRegion* region, std::uint32_t mode, float x, float y, float width, float height, float outerRadius, float innerRadius) {
    if (!region || mode > 255 || width <= 0 || height <= 0 || outerRadius < 0 || innerRadius < 0 ||
        (outerRadius != 0 && innerRadius > outerRadius) || outerRadius > width * 0.5f || outerRadius > height * 0.5f) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *region = {x, y, width, height, mode, 0xF60, outerRadius, innerRadius};
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsRegionInitCircular(FontGraphicsRegion* region, float x, float y, float outerRadius, float innerRadius) {
    if (!region || outerRadius <= 0 || innerRadius <= 0 || innerRadius > outerRadius) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *region = {x - outerRadius, y - outerRadius, outerRadius + outerRadius, outerRadius + outerRadius, 255, 0xF60, outerRadius, innerRadius};
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsFillRatesInit(FontGraphicsFillRates* rates, const void* color, std::uint32_t effect) {
    if (!IsObject(rates)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *rates = {};
    if (!IsColor(color)) return InvalidColor;
    if (!IsEffect(effect)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    rates->Color = color;
    rates->Tag = 0xF01A;
    rates->Effect = static_cast<std::uint16_t>(effect);
    rates->Layout = 0xF77;
    rates->Flags = 2;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsFillRatesSetFillEffect(FontGraphicsFillRates* rates, const void* color, std::uint32_t effect) {
    if (!IsObject(rates) || rates->Tag != 0xF01A) return InvalidRates;
    if (!IsColor(color)) return InvalidColor;
    if (!IsEffect(effect)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    rates->Color = color;
    rates->Effect = static_cast<std::uint16_t>(effect);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsFillRatesSetMapping(FontGraphicsFillRates* rates, float x, float y, float width, float height) {
    if (!IsObject(rates) || rates->Tag != 0xF01A) return InvalidRates;
    SetMapping(*rates, x, y, width, height);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsFillRatesSetLayout(FontGraphicsFillRates* rates, std::int32_t layout, std::uint32_t alignment) {
    if (!IsObject(rates) || rates->Tag != 0xF01A) return InvalidRates;
    if (!IsLayout(layout, alignment)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    rates->Flags = (rates->Flags & 0xFE) | ((static_cast<std::uint32_t>(layout) | alignment) != 0);
    rates->Layout = 0;
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsFillMethodInit(FontGraphicsFillMethod* method, std::uint32_t source, std::uint32_t blend) {
    if (!method || source > 0x50 || (source & 15) != 0 || !((blend >= 1 && blend <= 7) || blend == 12)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    *method = {source, blend, 15, 0xF01B, {}};
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsFillPlotInit(FontGraphicsFillPlot* plot, const FontGraphicsRegion* region) {
    if (!IsObject(plot)) return InvalidPlot;
    if (IsObject(region)) {
        const float halfExtent = (region->Width < region->Height ? region->Width : region->Height) * 0.5f;
        if (region->Tag != 0xF60 || !(region->OuterRadius >= 0) || !(region->InnerRadius >= 0) ||
            !(halfExtent >= (region->OuterRadius == 0 ? region->InnerRadius : region->OuterRadius)) ||
            (region->OuterRadius != 0 && !(region->OuterRadius >= region->InnerRadius))) {
            plot->Tag = 0;
            return InvalidRegion;
        }
    }
    *plot = {};
    plot->Region = region;
    plot->Tag = 0xF01C;
    plot->Flags = 2;
    if (reinterpret_cast<std::uintptr_t>(region) == UINTPTR_MAX) {
        plot->Flags = 6;
        plot->Layout = 0xF77;
    } else if (region) {
        plot->Mapping[0] = region->X;
        plot->Mapping[1] = region->Y;
        plot->Mapping[2] = region->X + region->Width;
        plot->Mapping[3] = region->Y + region->Height;
        plot->Flags = 1;
        plot->Layout = 0xF77;
    }
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsFillPlotSetMapping(FontGraphicsFillPlot* plot, float x, float y, float width, float height) {
    if (!IsObject(plot) || plot->Tag != 0xF01C) return InvalidPlot;
    SetMapping(*plot, x, y, width, height);
    return SCE_FONT_OK;
}

int APS5_VABI sceFontGraphicsFillPlotSetLayout(FontGraphicsFillPlot* plot, std::int32_t layout, std::uint32_t alignment) {
    if (!IsObject(plot) || plot->Tag != 0xF01C) return InvalidPlot;
    if (!IsLayout(layout, alignment)) return SCE_FONT_ERROR_INVALID_PARAMETER;
    const auto combined = static_cast<std::uint32_t>(layout) | alignment;
    plot->Layout = static_cast<std::uint16_t>(combined);
    plot->Flags = (plot->Flags & 0xF6) | (combined != 0) | 8;
    return SCE_FONT_OK;
}

}
