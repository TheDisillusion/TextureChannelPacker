#pragma once

// Centralized color tokens and font families for the dark redesign. Tokens
// here mirror the design handoff document so widget code and the QSS theme
// agree on a single source of truth. Hex values are sRGB approximations of
// the design's oklch() definitions; channel chip fills are explicitly mixed
// in sRGB (not OKLCH) because OKLCH hue interpolation crosses through wrong
// hues for R↔B blends into a near-black base.

#include <QColor>
#include <QString>

namespace tcp::app::tokens {

// Surface tones.
//
// The design's OKLCH lightness values (`oklch(0.13 ...)` etc.) actually
// render substantially darker in sRGB than the design README's hex
// approximations implied. These values are picked to match what a browser
// actually rasterizes the OKLCH tokens to, and to give the bg ↔ panel
// contrast that the design screenshots show.
inline const QColor bg            {0x06, 0x07, 0x0a};
inline const QColor panel         {0x12, 0x13, 0x17};
inline const QColor panel_hi      {0x16, 0x18, 0x1c};
inline const QColor sunken        {0x0a, 0x0b, 0x0e};
inline const QColor raised        {0x26, 0x28, 0x2c};
inline const QColor chip_ink      {0x10, 0x10, 0x14};

// Text.
inline const QColor text          {255, 255, 255, 235}; // 0.92
inline const QColor text_dim      {255, 255, 255, 140}; // 0.55
inline const QColor text_mute     {255, 255, 255,  90}; // 0.35

// Hairline borders.
inline const QColor hairline        {255, 255, 255,  15}; // 0.06
inline const QColor hairline_strong {255, 255, 255,  25}; // 0.10

// Accent — soft green used only for the primary Export button and the status
// "ready" dot.
inline const QColor accent          {0x7c, 0xc9, 0x5f};

// Channel reference colors (sRGB approximations of design oklch values).
// These are the "saturated" colors used as the channel text on chips. The
// fill recipe blends 30% of this into the chip ink, with a 55% blend used
// for the chip border.
inline const QColor channel_R {0xd9, 0x66, 0x66};
inline const QColor channel_G {0x7e, 0xd4, 0x7e};
inline const QColor channel_B {0x71, 0x94, 0xd6};
inline const QColor channel_A {0xbd, 0xbd, 0xc4};

inline QColor channel_color(QChar letter) noexcept
{
    switch (letter.toUpper().unicode()) {
        case 'R': return channel_R;
        case 'G': return channel_G;
        case 'B': return channel_B;
        case 'A': return channel_A;
        case 'L': return text_dim; // Luminance — neutral
        default:  return text_dim;
    }
}

// sRGB mix: out = a*ratio + b*(1-ratio). Both source colors and the result
// are interpreted in plain sRGB. Used for the chip fill (channel @ 30% into
// chip_ink) and the chip border (channel @ 55% into transparent).
inline QColor srgb_mix(const QColor& a, double ratio, const QColor& b) noexcept
{
    const double inv = 1.0 - ratio;
    return QColor::fromRgbF(
        a.redF()   * ratio + b.redF()   * inv,
        a.greenF() * ratio + b.greenF() * inv,
        a.blueF()  * ratio + b.blueF()  * inv,
        a.alphaF() * ratio + b.alphaF() * inv
    );
}

// Family lists for the global UI font and the mono caption font. Falls back
// to system fonts when Inter / JetBrains Mono are not installed.
inline const QStringList ui_font_families  { QStringLiteral("Inter"),         QStringLiteral("Segoe UI"), QStringLiteral("Arial") };
inline const QStringList mono_font_families{ QStringLiteral("JetBrains Mono"), QStringLiteral("Consolas"), QStringLiteral("Courier New") };

} // namespace tcp::app::tokens
