#pragma once

#include <QString>

enum class LinearUnit
{
    Millimeters,
    Centimeters,
    Meters,
    Inches
};

namespace Units
{
/** One numeric coordinate in the file equals this many meters (e.g. mm → 0.001). */
inline double metersPerModelUnit(LinearUnit u)
{
    switch (u) {
    case LinearUnit::Millimeters:
        return 0.001;
    case LinearUnit::Centimeters:
        return 0.01;
    case LinearUnit::Meters:
        return 1.0;
    case LinearUnit::Inches:
        return 0.0254;
    }
    return 1.0;
}

inline QString label(LinearUnit u)
{
    switch (u) {
    case LinearUnit::Millimeters:
        return QStringLiteral("mm");
    case LinearUnit::Centimeters:
        return QStringLiteral("cm");
    case LinearUnit::Meters:
        return QStringLiteral("m");
    case LinearUnit::Inches:
        return QStringLiteral("in");
    }
    return {};
}

/** Convert a length stored in meters to the given unit for display. */
inline double metersToUnit(double meters, LinearUnit display)
{
    return meters / metersPerModelUnit(display);
}

inline QString formatDistance(double meters, LinearUnit display, int decimals = 2)
{
    const double v = metersToUnit(meters, display);
    return QString::number(v, 'f', decimals) + QLatin1Char(' ') + label(display);
}

inline QString formatSize3D(double mx, double my, double mz, LinearUnit display)
{
    return formatDistance(mx, display) + QStringLiteral(" × ") + formatDistance(my, display) + QStringLiteral(" × ")
        + formatDistance(mz, display);
}
} // namespace Units
