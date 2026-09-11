#pragma once

#include "extra/customtext.h"
#include "pc/arabictext.h"

#include <cstdio>
#include <string>

namespace ImGuiLocalization {

inline bool IsArabic() {
    return g_customText.GetLanguage() == LangArabic;
}

inline std::string Text(
    const char* english,
    const char* arabicToken) {

    const char* fallback = english ? english : "";

    if (!IsArabic()) {
        return fallback;
    }

    const char* localized =
        arabicToken ? g_customText.GetString(arabicToken) : nullptr;

    if (!localized || !localized[0]) {
        return fallback;
    }

    return ArabicText::PrepareForDisplay(
        Utf8TextView(localized));
}

template <typename... Args>
inline std::string Format(
    const char* englishFormat,
    const char* arabicToken,
    Args... args) {

    const bool arabic = IsArabic();

    const char* format =
        englishFormat ? englishFormat : "";

    if (arabic && arabicToken) {
        const char* localized =
            g_customText.GetString(arabicToken);

        if (localized && localized[0]) {
            format = localized;
        }
    }

    char buffer[1024] = {};

    std::snprintf(
        buffer,
        sizeof(buffer),
        format,
        args...);

    if (!arabic) {
        return buffer;
    }

    return ArabicText::PrepareForDisplay(
        Utf8TextView(buffer));
}

inline std::string Label(
    const char* english,
    const char* arabicToken,
    const char* stableId = nullptr) {

    std::string result =
        Text(english, arabicToken);

    if (stableId && stableId[0]) {
        result += "###";
        result += stableId;
    }

    return result;
}

} // namespace ImGuiLocalization