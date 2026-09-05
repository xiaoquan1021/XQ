#ifndef XQ_CORE_XQ_SCALE_SLOT_H
#define XQ_CORE_XQ_SCALE_SLOT_H

#include <string>

namespace xq {

// Biological/physical model scale identity. This is deliberately independent
// from rendering LOD and camera zoom. Absence is represented by
// std::optional<ScaleSlot> at the owning XQDataNode; the enum itself has no
// Unknown value, so invalid or legacy data cannot silently become Organ.
enum class ScaleSlot {
    Organ,
    Micro,
    Cell
};

// Stable persistence/diagnostic tokens. An invalid enum value yields an empty
// token; parsing is strict and leaves *out untouched on failure.
inline const char* scaleSlotToToken(ScaleSlot slot)
{
    switch (slot) {
    case ScaleSlot::Organ:
        return "organ";
    case ScaleSlot::Micro:
        return "micro";
    case ScaleSlot::Cell:
        return "cell";
    }
    return "";
}

inline bool scaleSlotFromToken(const std::string& token, ScaleSlot* out)
{
    if (out == nullptr) {
        return false;
    }

    ScaleSlot parsed = ScaleSlot::Organ;
    if (token == "organ") {
        parsed = ScaleSlot::Organ;
    } else if (token == "micro") {
        parsed = ScaleSlot::Micro;
    } else if (token == "cell") {
        parsed = ScaleSlot::Cell;
    } else {
        return false;
    }

    *out = parsed;
    return true;
}

} // namespace xq

#endif // XQ_CORE_XQ_SCALE_SLOT_H
