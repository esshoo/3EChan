#pragma once

#include "gen/utf8text.h"
#include <string>

namespace ArabicText {

bool ContainsArabic(Utf8TextView text);
std::string PrepareForDisplay(Utf8TextView text);

} // namespace ArabicText
