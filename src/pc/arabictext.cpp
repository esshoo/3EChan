#include "pc/arabictext.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace ArabicText {
namespace {

struct JoinForm {
    u32 base;
    u32 isolated;
    u32 finalForm;
    u32 initial;
    u32 medial;
    bool joinsPrev;
    bool joinsNext;
};

static const JoinForm kForms[] = {
    { 0x0621, 0xFE80, 0,      0,      0,      false, false },
    { 0x0622, 0xFE81, 0xFE82, 0,      0,      true,  false },
    { 0x0623, 0xFE83, 0xFE84, 0,      0,      true,  false },
    { 0x0624, 0xFE85, 0xFE86, 0,      0,      true,  false },
    { 0x0625, 0xFE87, 0xFE88, 0,      0,      true,  false },
    { 0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C, true,  true  },
    { 0x0627, 0xFE8D, 0xFE8E, 0,      0,      true,  false },
    { 0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92, true,  true  },
    { 0x0629, 0xFE93, 0xFE94, 0,      0,      true,  false },
    { 0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98, true,  true  },
    { 0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C, true,  true  },
    { 0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0, true,  true  },
    { 0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4, true,  true  },
    { 0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8, true,  true  },
    { 0x062F, 0xFEA9, 0xFEAA, 0,      0,      true,  false },
    { 0x0630, 0xFEAB, 0xFEAC, 0,      0,      true,  false },
    { 0x0631, 0xFEAD, 0xFEAE, 0,      0,      true,  false },
    { 0x0632, 0xFEAF, 0xFEB0, 0,      0,      true,  false },
    { 0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4, true,  true  },
    { 0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8, true,  true  },
    { 0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC, true,  true  },
    { 0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0, true,  true  },
    { 0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4, true,  true  },
    { 0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8, true,  true  },
    { 0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC, true,  true  },
    { 0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0, true,  true  },
    { 0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4, true,  true  },
    { 0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8, true,  true  },
    { 0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC, true,  true  },
    { 0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0, true,  true  },
    { 0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4, true,  true  },
    { 0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8, true,  true  },
    { 0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC, true,  true  },
    { 0x0648, 0xFEED, 0xFEEE, 0,      0,      true,  false },
    { 0x0649, 0xFEEF, 0xFEF0, 0,      0,      true,  false },
    { 0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4, true,  true  },
};

enum Direction : u8 {
    DirRtl,
    DirLtr,
    DirNeutral,
};

struct LogicalItem {
    u32 codepoint = 0;
    std::string atom = {};
    Direction direction = DirNeutral;
};

struct VisualUnit {
    std::string bytes = {};
    Direction direction = DirNeutral;
    bool mirror = false;
};

const JoinForm* FindForm(u32 codepoint) {
    for (const JoinForm& form : kForms) {
        if (form.base == codepoint) {
            return &form;
        }
    }
    return nullptr;
}

bool IsTransparentMark(u32 codepoint) {
    return (codepoint >= 0x064B && codepoint <= 0x065F)
        || codepoint == 0x0670
        || (codepoint >= 0x06D6 && codepoint <= 0x06ED);
}

bool IsArabicCodepoint(u32 codepoint) {
    return (codepoint >= 0x0600 && codepoint <= 0x06FF)
        || (codepoint >= 0x0750 && codepoint <= 0x077F)
        || (codepoint >= 0x08A0 && codepoint <= 0x08FF)
        || (codepoint >= 0xFB50 && codepoint <= 0xFDFF)
        || (codepoint >= 0xFE70 && codepoint <= 0xFEFF);
}

bool IsStrongLtr(u32 codepoint) {
    return (codepoint >= '0' && codepoint <= '9')
        || (codepoint >= 'A' && codepoint <= 'Z')
        || (codepoint >= 'a' && codepoint <= 'z')
        || (codepoint >= 0x0660 && codepoint <= 0x0669)
        || (codepoint >= 0x06F0 && codepoint <= 0x06F9);
}

bool IsMirrorCodepoint(u32 codepoint) {
    return codepoint == '(' || codepoint == ')'
        || codepoint == '[' || codepoint == ']'
        || codepoint == '{' || codepoint == '}';
}

u32 MirrorCodepoint(u32 codepoint) {
    switch (codepoint) {
        case '(': return ')';
        case ')': return '(';
        case '[': return ']';
        case ']': return '[';
        case '{': return '}';
        case '}': return '{';
        default: return codepoint;
    }
}

bool DecodeNextUtf8(
    const char* text,
    s32 len,
    s32& cursor,
    u32& outCodepoint) {

    if (!text || cursor >= len) {
        return false;
    }

    const u8 c0 = (u8)text[cursor++];

    if (c0 < 0x80) {
        outCodepoint = c0;
        return true;
    }

    if ((c0 & 0xE0) == 0xC0 && cursor < len) {
        const u8 c1 = (u8)text[cursor];

        if ((c1 & 0xC0) == 0x80) {
            cursor++;
            outCodepoint =
                ((u32)(c0 & 0x1F) << 6)
                | (u32)(c1 & 0x3F);
            return true;
        }
    }
    else if ((c0 & 0xF0) == 0xE0 && cursor + 1 < len) {
        const u8 c1 = (u8)text[cursor];
        const u8 c2 = (u8)text[cursor + 1];

        if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80) {
            cursor += 2;
            outCodepoint =
                ((u32)(c0 & 0x0F) << 12)
                | ((u32)(c1 & 0x3F) << 6)
                | (u32)(c2 & 0x3F);
            return true;
        }
    }
    else if ((c0 & 0xF8) == 0xF0 && cursor + 2 < len) {
        const u8 c1 = (u8)text[cursor];
        const u8 c2 = (u8)text[cursor + 1];
        const u8 c3 = (u8)text[cursor + 2];

        if ((c1 & 0xC0) == 0x80
            && (c2 & 0xC0) == 0x80
            && (c3 & 0xC0) == 0x80) {

            cursor += 3;
            outCodepoint =
                ((u32)(c0 & 0x07) << 18)
                | ((u32)(c1 & 0x3F) << 12)
                | ((u32)(c2 & 0x3F) << 6)
                | (u32)(c3 & 0x3F);
            return true;
        }
    }

    outCodepoint = (u32)'?';
    return true;
}

void AppendUtf8(std::string& out, u32 codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back((char)codepoint);
    }
    else if (codepoint <= 0x7FF) {
        out.push_back((char)(0xC0 | (codepoint >> 6)));
        out.push_back((char)(0x80 | (codepoint & 0x3F)));
    }
    else if (codepoint <= 0xFFFF) {
        out.push_back((char)(0xE0 | (codepoint >> 12)));
        out.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (codepoint & 0x3F)));
    }
    else {
        out.push_back((char)(0xF0 | (codepoint >> 18)));
        out.push_back((char)(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (codepoint & 0x3F)));
    }
}

std::string EncodeUtf8(u32 codepoint) {
    std::string out;
    AppendUtf8(out, codepoint);
    return out;
}

Direction ClassifyDirection(u32 codepoint) {
    if (IsArabicCodepoint(codepoint)) {
        return DirRtl;
    }

    if (IsStrongLtr(codepoint)) {
        return DirLtr;
    }

    return DirNeutral;
}

std::vector<LogicalItem> DecodeLogicalItems(Utf8TextView text) {
    std::vector<LogicalItem> result;
    s32 cursor = 0;

    while (cursor < text.length) {
        if (text.data[cursor] == '<') {
            const void* endPtr = std::memchr(
                text.data + cursor,
                '>',
                (size_t)(text.length - cursor));

            if (endPtr) {
                const char* end = (const char*)endPtr;
                const s32 count =
                    (s32)(end - (text.data + cursor)) + 1;

                result.push_back({
                    0,
                    std::string(text.data + cursor, (size_t)count),
                    DirLtr
                });

                cursor += count;
                continue;
            }
        }

        if (text.data[cursor] == '%') {
            const s32 start = cursor++;

            if (cursor < text.length && text.data[cursor] == '%') {
                cursor++;
            }
            else {
                while (
                    cursor < text.length &&
                    std::strchr("-+ #0.123456789hljztL", text.data[cursor])
                ) {
                    cursor++;
                }

                if (cursor < text.length) {
                    cursor++;
                }
            }

            result.push_back({
                0,
                std::string(
                    text.data + start,
                    (size_t)(cursor - start)),
                DirLtr
            });

            continue;
        }

        u32 codepoint = 0;

        if (!DecodeNextUtf8(
            text.data,
            text.length,
            cursor,
            codepoint))
        {
            break;
        }

        // STB does not position combining marks.
        if (IsTransparentMark(codepoint)) {
            continue;
        }

        result.push_back({
            codepoint,
            {},
            ClassifyDirection(codepoint)
        });
    }

    return result;
}

bool PreviousJoinsNext(
    const std::vector<LogicalItem>& logical,
    s32 index) {

    for (s32 i = index - 1; i >= 0; i--) {
        const LogicalItem& item = logical[(size_t)i];

        if (!item.atom.empty()) {
            return false;
        }

        if (item.codepoint == 0x0640) {
            return true;
        }

        const JoinForm* form = FindForm(item.codepoint);
        return form && form->joinsNext;
    }

    return false;
}

bool NextJoinsPrevious(
    const std::vector<LogicalItem>& logical,
    s32 index) {

    for (
        s32 i = index + 1;
        i < (s32)logical.size();
        i++
    ) {
        const LogicalItem& item = logical[(size_t)i];

        if (!item.atom.empty()) {
            return false;
        }

        if (item.codepoint == 0x0640) {
            return true;
        }

        const JoinForm* form = FindForm(item.codepoint);
        return form && form->joinsPrev;
    }

    return false;
}

bool GetLamAlefForms(
    u32 alef,
    u32& outIsolated,
    u32& outFinal) {

    switch (alef) {
        case 0x0622:
            outIsolated = 0xFEF5;
            outFinal = 0xFEF6;
            return true;

        case 0x0623:
            outIsolated = 0xFEF7;
            outFinal = 0xFEF8;
            return true;

        case 0x0625:
            outIsolated = 0xFEF9;
            outFinal = 0xFEFA;
            return true;

        case 0x0627:
            outIsolated = 0xFEFB;
            outFinal = 0xFEFC;
            return true;

        default:
            return false;
    }
}

std::vector<VisualUnit> ShapeLogicalItems(Utf8TextView text) {
    const std::vector<LogicalItem> logical =
        DecodeLogicalItems(text);

    std::vector<VisualUnit> units;
    units.reserve(logical.size());

    for (
        s32 i = 0;
        i < (s32)logical.size();
        i++
    ) {
        const LogicalItem& item =
            logical[(size_t)i];

        if (!item.atom.empty()) {
            units.push_back({
                item.atom,
                item.direction,
                false
            });
            continue;
        }

        const u32 codepoint = item.codepoint;

        if (
            codepoint == 0x0644 &&
            i + 1 < (s32)logical.size() &&
            logical[(size_t)(i + 1)].atom.empty()
        ) {
            u32 isolated = 0;
            u32 finalForm = 0;

            if (GetLamAlefForms(
                logical[(size_t)(i + 1)].codepoint,
                isolated,
                finalForm))
            {
                const bool connectsPrev =
                    PreviousJoinsNext(logical, i);

                units.push_back({
                    EncodeUtf8(
                        connectsPrev
                            ? finalForm
                            : isolated),
                    DirRtl,
                    false
                });

                i++;
                continue;
            }
        }

        const JoinForm* form =
            FindForm(codepoint);

        u32 shaped = codepoint;

        if (form) {
            const bool connectsPrev =
                form->joinsPrev &&
                PreviousJoinsNext(logical, i);

            const bool connectsNext =
                form->joinsNext &&
                NextJoinsPrevious(logical, i);

            shaped = form->isolated;

            if (
                connectsPrev &&
                connectsNext &&
                form->medial
            ) {
                shaped = form->medial;
            }
            else if (
                connectsPrev &&
                form->finalForm
            ) {
                shaped = form->finalForm;
            }
            else if (
                connectsNext &&
                form->initial
            ) {
                shaped = form->initial;
            }
        }

        units.push_back({
            EncodeUtf8(shaped),
            ClassifyDirection(codepoint),
            IsMirrorCodepoint(codepoint)
        });
    }

    return units;
}

void ResolveNeutralDirections(
    std::vector<VisualUnit>& units) {

    size_t i = 0;

    while (i < units.size()) {
        if (units[i].direction != DirNeutral) {
            i++;
            continue;
        }

        const size_t start = i;

        while (
            i < units.size() &&
            units[i].direction == DirNeutral
        ) {
            i++;
        }

        const size_t end = i;

        Direction left = DirRtl;
        Direction right = DirRtl;

        for (size_t p = start; p > 0; p--) {
            if (
                units[p - 1].direction
                != DirNeutral
            ) {
                left = units[p - 1].direction;
                break;
            }
        }

        for (
            size_t p = end;
            p < units.size();
            p++
        ) {
            if (
                units[p].direction
                != DirNeutral
            ) {
                right = units[p].direction;
                break;
            }
        }

        const Direction resolved =
            (left == DirLtr && right == DirLtr)
                ? DirLtr
                : DirRtl;

        for (
            size_t p = start;
            p < end;
            p++
        ) {
            units[p].direction = resolved;
        }
    }
}

std::string PrepareLine(Utf8TextView line) {
    std::vector<VisualUnit> units =
        ShapeLogicalItems(line);

    bool hasArabic = false;

    for (const VisualUnit& unit : units) {
        if (unit.direction == DirRtl) {
            hasArabic = true;
            break;
        }
    }

    if (!hasArabic) {
        return std::string(
            line.data,
            (size_t)line.length);
    }

    ResolveNeutralDirections(units);

    // Backend always advances left-to-right.
    std::reverse(
        units.begin(),
        units.end());

    // Restore internal order of embedded LTR runs.
    for (
        size_t i = 0;
        i < units.size();
    ) {
        if (units[i].direction != DirLtr) {
            i++;
            continue;
        }

        size_t end = i + 1;

        while (
            end < units.size() &&
            units[end].direction == DirLtr
        ) {
            end++;
        }

        std::reverse(
            units.begin() + i,
            units.begin() + end);

        i = end;
    }

    for (VisualUnit& unit : units) {
        if (
            unit.mirror &&
            unit.bytes.size() == 1
        ) {
            unit.bytes[0] =
                (char)MirrorCodepoint(
                    (u8)unit.bytes[0]);
        }
    }

    std::string out;

    for (const VisualUnit& unit : units) {
        out += unit.bytes;
    }

    return out;
}

} // namespace

bool ContainsArabic(Utf8TextView text) {
    if (text.IsEmpty()) {
        return false;
    }

    s32 cursor = 0;

    while (cursor < text.length) {
        u32 codepoint = 0;

        if (!DecodeNextUtf8(
            text.data,
            text.length,
            cursor,
            codepoint))
        {
            break;
        }

        if (IsArabicCodepoint(codepoint)) {
            return true;
        }
    }

    return false;
}

std::string PrepareForDisplay(Utf8TextView text) {
    if (text.IsEmpty()) {
        return {};
    }

    std::string out;
    s32 lineStart = 0;

    for (
        s32 i = 0;
        i <= text.length;
        i++
    ) {
        if (
            i == text.length ||
            text.data[i] == '\n'
        ) {
            const Utf8TextView line(
                text.data + lineStart,
                i - lineStart);

            out += PrepareLine(line);

            if (i < text.length) {
                out.push_back('\n');
            }

            lineStart = i + 1;
        }
    }

    return out;
}

} // namespace ArabicText
