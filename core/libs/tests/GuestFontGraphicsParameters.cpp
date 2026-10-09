#include "prx/libSceFont/include/FontGraphics.hpp"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

constexpr int InvalidParameter = static_cast<int>(0x80460002);
constexpr int InvalidColor = static_cast<int>(0x80460086);
constexpr int InvalidRegion = static_cast<int>(0x80460087);
constexpr int InvalidPlot = static_cast<int>(0x80460088);
constexpr int InvalidRates = static_cast<int>(0x80460089);

void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Font graphics parameters check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template <typename T>
T Filled() {
    T value;
    std::memset(&value, 0xA5, sizeof(value));
    return value;
}

template <typename T>
bool Same(const T& a, const T& b) {
    return std::memcmp(&a, &b, sizeof(T)) == 0;
}

void CheckRegions() {
    auto region = Filled<FontGraphicsRegion>();
    const auto untouched = region;
    Require(sceFontGraphicsRegionInit(nullptr, 0, 0, 1, 1) == InvalidParameter);
    Require(sceFontGraphicsRegionInit(&region, 2, 3, -1, 8) == InvalidParameter && Same(region, untouched));
    Require(sceFontGraphicsRegionInit(&region, 2, -3, 12, 8) == 0);
    Require(Same(region, FontGraphicsRegion{2, -3, 12, 8, 15, 0xF60, 0, 0}));
    Require(sceFontGraphicsRegionInit(&region, 2, 3, -0.0f, 0) == 0 && std::bit_cast<std::uint32_t>(region.Width) == 0x80000000);
    const auto empty = region;
    Require(sceFontGraphicsRegionInitRoundish(&region, 15, 2, 3, 0, 8, 0, 0) == InvalidParameter && Same(region, empty));
    Require(sceFontGraphicsRegionInitRoundish(&region, 256, 2, 3, 12, 8, 3, 1) == InvalidParameter && Same(region, empty));
    Require(sceFontGraphicsRegionInitRoundish(&region, 15, 2, 3, 12, 8, 5, 1) == InvalidParameter && Same(region, empty));
    Require(sceFontGraphicsRegionInitRoundish(&region, 15, 2, 3, 12, 8, 3, 4) == InvalidParameter && Same(region, empty));
    Require(sceFontGraphicsRegionInitRoundish(&region, 255, 2, 3, 12, 8, 4, 2) == 0);
    Require(Same(region, FontGraphicsRegion{2, 3, 12, 8, 255, 0xF60, 4, 2}));
    Require(sceFontGraphicsRegionInitRoundish(&region, 0, 2, 3, 12, 8, 0, 4) == 0);
    const auto roundish = region;
    Require(sceFontGraphicsRegionInitCircular(&region, 10, 20, 3, 0) == InvalidParameter && Same(region, roundish));
    Require(sceFontGraphicsRegionInitCircular(&region, 10, 20, 3, 4) == InvalidParameter && Same(region, roundish));
    Require(sceFontGraphicsRegionInitCircular(&region, 10, 20, 3, 1) == 0);
    Require(Same(region, FontGraphicsRegion{7, 17, 6, 6, 255, 0xF60, 3, 1}));
    const float nan = std::bit_cast<float>(0x7FC00123u);
    Require(sceFontGraphicsRegionInit(&region, nan, 0, nan, 0) == 0);
    Require(std::bit_cast<std::uint32_t>(region.X) == 0x7FC00123u && std::bit_cast<std::uint32_t>(region.Width) == 0x7FC00123u);
}

void CheckRates() {
    auto rates = Filled<FontGraphicsFillRates>();
    const auto untouched = rates;
    Require(sceFontGraphicsFillRatesInit(nullptr, nullptr, 0) == InvalidParameter);
    Require(sceFontGraphicsFillRatesSetFillEffect(&rates, nullptr, 0) == InvalidRates && Same(rates, untouched));
    Require(sceFontGraphicsFillRatesSetMapping(&rates, 1, 2, 3, 4) == InvalidRates && Same(rates, untouched));
    Require(sceFontGraphicsFillRatesSetLayout(&rates, 0, 0) == InvalidRates && Same(rates, untouched));
    const FontGraphicsFillRates zero{};
    for (std::uint32_t format = 0; format < 256; ++format) {
        std::uint8_t color = static_cast<std::uint8_t>(format);
        const bool valid = format == 0x10 || format == 0x20 || format == 0x30 || format == 0x40 || format == 0x50;
        rates = untouched;
        Require(sceFontGraphicsFillRatesInit(&rates, &color, 16) == (valid ? 0 : InvalidColor));
        if (valid) Require(Same(rates, FontGraphicsFillRates{{}, &color, 0xF01A, 16, 0xF77, 2, 0, {}}));
        else Require(Same(rates, zero));
    }
    for (std::uint32_t effect = 0; effect < 33; ++effect) {
        rates = untouched;
        const bool valid = effect <= 5 || effect == 8 || effect == 9 || effect == 16;
        Require(sceFontGraphicsFillRatesInit(&rates, nullptr, effect) == (valid ? 0 : InvalidParameter));
        if (valid) Require(rates.Effect == effect && rates.Tag == 0xF01A && rates.Flags == 2);
        else Require(Same(rates, zero));
    }
    for (std::intptr_t sentinel = -1; sentinel <= 2; ++sentinel) {
        const auto* color = reinterpret_cast<const void*>(sentinel);
        Require(sceFontGraphicsFillRatesInit(&rates, color, 9) == 0 && rates.Color == color);
    }
    const auto initialized = rates;
    Require(sceFontGraphicsFillRatesSetFillEffect(&rates, nullptr, 7) == InvalidParameter && Same(rates, initialized));
    std::uint8_t color = 0x11;
    Require(sceFontGraphicsFillRatesSetFillEffect(&rates, &color, 1) == InvalidColor && Same(rates, initialized));
    color = 0x40;
    Require(sceFontGraphicsFillRatesSetFillEffect(&rates, &color, 8) == 0 && rates.Color == &color && rates.Effect == 8);
    rates.Flags = 0xFF;
    Require(sceFontGraphicsFillRatesSetMapping(&rates, 1, -2, -3, 4) == 0 && rates.Flags == 0xFD);
    Require(rates.Mapping[0] == 1 && rates.Mapping[1] == -2 && rates.Mapping[2] == -3 && rates.Mapping[3] == 4);
    Require(sceFontGraphicsFillRatesSetLayout(&rates, 0x400, 0x24) == 0 && rates.Layout == 0 && rates.Flags == 0xFD);
    Require(sceFontGraphicsFillRatesSetLayout(&rates, 0, 0) == 0 && rates.Flags == 0xFC);
    const auto configured = rates;
    Require(sceFontGraphicsFillRatesSetLayout(&rates, 0x170, 3) == InvalidParameter && Same(rates, configured));
    Require(sceFontGraphicsFillRatesSetLayout(&rates, 1, 0) == InvalidParameter && Same(rates, configured));
}

void CheckMethods() {
    for (std::uint32_t source = 0; source <= 0x60; ++source) {
        for (std::uint32_t blend = 0; blend <= 13; ++blend) {
            auto method = Filled<FontGraphicsFillMethod>();
            const auto untouched = method;
            const bool validSource = source <= 0x50 && source % 16 == 0;
            const bool validBlend = (blend >= 1 && blend <= 7) || blend == 12;
            const bool valid = validSource && validBlend;
            Require(sceFontGraphicsFillMethodInit(&method, source, blend) == (valid ? 0 : InvalidParameter));
            if (valid) Require(Same(method, FontGraphicsFillMethod{source, blend, 15, 0xF01B, {}}));
            else Require(Same(method, untouched));
        }
    }
    Require(sceFontGraphicsFillMethodInit(nullptr, 0, 1) == InvalidParameter);
}

void CheckPlots() {
    auto plot = Filled<FontGraphicsFillPlot>();
    Require(sceFontGraphicsFillPlotInit(nullptr, nullptr) == InvalidPlot);
    Require(sceFontGraphicsFillPlotInit(&plot, nullptr) == 0);
    Require(Same(plot, FontGraphicsFillPlot{{}, nullptr, 0xF01C, 0, 0, 2, 0, {}}));
    const auto* sentinel = reinterpret_cast<const FontGraphicsRegion*>(UINTPTR_MAX);
    Require(sceFontGraphicsFillPlotInit(&plot, sentinel) == 0);
    Require(Same(plot, FontGraphicsFillPlot{{}, sentinel, 0xF01C, 0, 0xF77, 6, 0, {}}));
    FontGraphicsRegion region{2, -3, 12, 8, 15, 0xF60, 4, 2};
    Require(sceFontGraphicsFillPlotInit(&plot, &region) == 0);
    Require(Same(plot, FontGraphicsFillPlot{{2, -3, 14, 5}, &region, 0xF01C, 0, 0xF77, 1, 0, {}}));
    region.InnerRadius = 5;
    auto invalidated = plot;
    invalidated.Tag = 0;
    Require(sceFontGraphicsFillPlotInit(&plot, &region) == InvalidRegion && Same(plot, invalidated));
    Require(sceFontGraphicsFillPlotSetMapping(&plot, 1, 2, 3, 4) == InvalidPlot && Same(plot, invalidated));
    Require(sceFontGraphicsFillPlotSetLayout(&plot, 0, 0) == InvalidPlot && Same(plot, invalidated));
    region.InnerRadius = 2;
    Require(sceFontGraphicsFillPlotInit(&plot, &region) == 0);
    plot.Flags = 0xFF;
    Require(sceFontGraphicsFillPlotSetMapping(&plot, -1, 2, 3, -4) == 0 && plot.Flags == 0xFD);
    Require(plot.Mapping[0] == -1 && plot.Mapping[1] == 2 && plot.Mapping[2] == 3 && plot.Mapping[3] == -4);
    Require(sceFontGraphicsFillPlotSetLayout(&plot, 0x400, 0x24) == 0 && plot.Layout == 0x424 && plot.Flags == 0xFD);
    Require(sceFontGraphicsFillPlotSetLayout(&plot, 0, 0) == 0 && plot.Layout == 0 && plot.Flags == 0xFC);
    const auto configured = plot;
    Require(sceFontGraphicsFillPlotSetLayout(&plot, 0x400, 0x30) == InvalidParameter && Same(plot, configured));
    region.OuterRadius = std::numeric_limits<float>::quiet_NaN();
    Require(sceFontGraphicsFillPlotInit(&plot, &region) == InvalidRegion);
}

}

int main() {
    CheckRegions();
    CheckRates();
    CheckMethods();
    CheckPlots();
}
