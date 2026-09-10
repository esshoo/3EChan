#include "pc/textmgr.h"
#include "pc/textbackend.h"
#include "pc/arabictext.h"
#include "gen/common.h"
#include "pc/log.h"
#include "p3d/fileio.h"
#include <cstdlib>
#if CUSTOM_TEXT
#include "extra/customtext.h"
#endif


TextManager* g_textManager = nullptr;

static TextRenderState ResolveLocalizedRenderState(
    const TextManager* manager,
    const TextRenderState& source) {
    TextRenderState state = source;

#if CUSTOM_TEXT
    if (manager && g_customText.GetLanguage() == LangArabic) {
        const TextFontHandle arabicFont = manager->FindFont("Arabic");

        if (arabicFont) {
            state.font = arabicFont;

            // Give Arabic a heavier, wider display presence closer
            // to the original English menu typography.
            state.scaleX *= 1.08f;
            state.scaleY *= 1.03f;

            if (state.outline.enabled) {
                state.outline.thickness *= 1.15f;
            }
        }
    }
#endif

    return state;
}

static Utf8TextView ResolveLocalizedText(
    Utf8TextView source,
    std::string& storage) {
#if CUSTOM_TEXT
    if (
        g_customText.GetLanguage() == LangArabic &&
        ArabicText::ContainsArabic(source)
    ) {
        storage = ArabicText::PrepareForDisplay(source);
        return Utf8TextView(storage);
    }
#else
    (void)storage;
#endif

    return source;
}

TextManager::~TextManager() {
    Shutdown();
}

void TextManager::Init() {
    ResetState();
    m_stateStack.clear();
    m_fonts.clear();
    m_nextFontHandle = 1;
    m_warnedNoBackend = false;

    if (!m_backend) {
        m_backend = CreateDefaultTextBackend();
    }

    if (m_backend && !m_backend->Init()) {
        LOG("[TextManager] Backend init failed");
    }

#if CUSTOM_TEXT && defined(RC_PLATFORM_WINDOWS)
    const char* windowsDir = std::getenv("WINDIR");
    bool arabicFontLoaded = false;

    // Prefer the bundled 3EChan Arabic font when present.
    // If it is missing or cannot be loaded, keep the existing Windows
    // font fallback chain unchanged.
    static constexpr const char* kBundledArabicFont = "pc/fonts/3EChan.ttf";
    const std::string bundledArabicFontPath = p3d::io::ResolvePath(kBundledArabicFont);

    if (p3d::io::FileExists(bundledArabicFontPath)) {
        TextFontDesc arabicDesc = {};
        arabicDesc.name = "Arabic";
        arabicDesc.path = kBundledArabicFont;
        arabicDesc.pixelHeight = 48;

        if (LoadFont(arabicDesc)) {
            LOG("[TextManager] Arabic font loaded: %s", kBundledArabicFont);
            arabicFontLoaded = true;
        }
    }

    if (!arabicFontLoaded && windowsDir && windowsDir[0]) {
        static const char* kArabicFontFiles[] = {
            "tradbdo.ttf",      // Traditional Arabic Bold - primary display font
            "tahomabd.ttf",     // Strong fallback
            "arialbd.ttf",
            "segoeuib.ttf",
            "tahoma.ttf",
            "arial.ttf",
            "segoeui.ttf",
        };

        for (const char* fontFile : kArabicFontFiles) {
            const std::string fontPath = std::string(windowsDir) + "/Fonts/" + fontFile;

            TextFontDesc arabicDesc = {};
            arabicDesc.name = "Arabic";
            arabicDesc.path = fontPath.c_str();
            arabicDesc.pixelHeight = 48;

            if (LoadFont(arabicDesc)) {
                LOG("[TextManager] Arabic font loaded: %s", fontPath.c_str());
                arabicFontLoaded = true;
                break;
            }
        }
    }

    if (!arabicFontLoaded) {
        LOG("[TextManager] WARNING: No Arabic-capable Windows font could be loaded");
    }
#endif
}

void TextManager::Shutdown() {
    if (m_backend) {
        m_backend->Shutdown();
        delete m_backend;
        m_backend = nullptr;
    }

    m_stateStack.clear();
    m_fonts.clear();
    ResetState();
    m_warnedNoBackend = false;
}

TextFontHandle TextManager::LoadFont(const TextFontDesc& desc) {
    if (!desc.name || !desc.name[0] || !desc.path || !desc.path[0] || desc.pixelHeight <= 0) {
        LOG("[TextManager] Refusing to load invalid font descriptor");
        return 0;
    }

    if (TextFontHandle existing = FindFont(desc.name)) {
        return existing;
    }

    LoadedFont font = {};
    font.handle = m_nextFontHandle++;
    font.name = desc.name;
    font.path = desc.path;
    font.pixelHeight = desc.pixelHeight;

    if (!ReadWholeFile(desc.path, font.fileData)) {
        LOG("[TextManager] Failed to read font file: %s", desc.path);
        return 0;
    }

    if (m_backend) {
        TextBackendFontDesc backendDesc = {};
        backendDesc.handle = font.handle;
        backendDesc.name = font.name.c_str();
        backendDesc.fileData = font.fileData.data();
        backendDesc.fileSize = (s32)font.fileData.size();
        backendDesc.pixelHeight = font.pixelHeight;

        if (!m_backend->RegisterFont(backendDesc)) {
            LOG("[TextManager] Backend failed to register font: %s", desc.name);
            return 0;
        }
    }

    m_fonts.push_back(font);
    return font.handle;
}

TextFontHandle TextManager::FindFont(const char* name) const {
    if (!name || !name[0]) {
        return 0;
    }

    for (s32 i = 0; i < (s32)m_fonts.size(); i++) {
        if (m_fonts[i].name == name) {
            return m_fonts[i].handle;
        }
    }

    return 0;
}

void TextManager::ResetState() {
    m_state = {};
}

void TextManager::PushState() {
    m_stateStack.push_back(m_state);
}

void TextManager::PopState() {
    if (m_stateStack.empty()) {
        return;
    }

    m_state = m_stateStack.back();
    m_stateStack.pop_back();
}

void TextManager::SetFont(TextFontHandle font) {
    m_state.font = font;
}

bool TextManager::SetFontByName(const char* name) {
    TextFontHandle handle = FindFont(name);
    if (!handle) {
        return false;
    }

    m_state.font = handle;
    return true;
}

void TextManager::SetScale(f32 scaleX, f32 scaleY) {
    m_state.scaleX = scaleX;
    m_state.scaleY = scaleY;
}

void TextManager::SetWrapWidth(f32 wrapWidth) {
    m_state.wrapWidth = wrapWidth;
}

void TextManager::SetLineSpacing(s32 lineSpacing) {
    m_state.lineSpacing = lineSpacing;
}

void TextManager::SetAlignment(TextAlign align) {
    m_state.align = align;
}

void TextManager::SetColor(u8 r, u8 g, u8 b, u8 a) {
    m_state.color = { r, g, b, a };
}

void TextManager::SetShadow(bool enabled, f32 offsetX, f32 offsetY,
                            u8 r, u8 g, u8 b, u8 a) {
    m_state.shadow.enabled = enabled;
    m_state.shadow.offsetX = offsetX;
    m_state.shadow.offsetY = offsetY;
    m_state.shadow.color = { r, g, b, a };
}

void TextManager::SetOutline(bool enabled, f32 thickness,
                             u8 r, u8 g, u8 b, u8 a) {
    m_state.outline.enabled = enabled;
    m_state.outline.thickness = thickness;
    m_state.outline.color = { r, g, b, a };
}

void TextManager::SetPromptsEnabled(bool enabled) {
    m_state.promptsEnabled = enabled;
}

TextBounds TextManager::MeasureString(Utf8TextView text) const {
    if (m_backend) {
        const TextRenderState state = ResolveLocalizedRenderState(this, m_state);
        std::string localizedStorage;
        const Utf8TextView displayText = ResolveLocalizedText(text, localizedStorage);
        return m_backend->Measure(displayText, state);
    }

    TextBounds bounds = {};
    if (!text.IsEmpty()) {
        bounds.lineCount = 1;
    }
    return bounds;
}

s32 TextManager::CountWrappedLines(Utf8TextView text) const {
    if (m_backend) {
        const TextRenderState state = ResolveLocalizedRenderState(this, m_state);
        std::string localizedStorage;
        const Utf8TextView displayText = ResolveLocalizedText(text, localizedStorage);
        return m_backend->CountWrappedLines(displayText, state);
    }

    return text.IsEmpty() ? 0 : 1;
}

TextBounds TextManager::MeasureToken(const char* token) const {
#if CUSTOM_TEXT
    return MeasureString(g_customText.GetText(token));
#else
    (void)token;
    return {};
#endif
}

s32 TextManager::CountWrappedLinesToken(const char* token) const {
#if CUSTOM_TEXT
    return CountWrappedLines(g_customText.GetText(token));
#else
    (void)token;
    return 0;
#endif
}

void TextManager::PrintToken(const char* token, f32 x, f32 y) const {
#if CUSTOM_TEXT
    PrintString(g_customText.GetText(token), x, y);
#else
    (void)token;
    (void)x;
    (void)y;
#endif
}

void TextManager::PrintString(const char* text, f32 x, f32 y) const {
    PrintString(Utf8TextView(text), x, y);
}

void TextManager::PrintString(Utf8TextView text, f32 x, f32 y) const {
    if (text.IsEmpty()) {
        return;
    }

    const TextRenderState state = ResolveLocalizedRenderState(this, m_state);
    std::string localizedStorage;
    const Utf8TextView displayText = ResolveLocalizedText(text, localizedStorage);
    const LoadedFont* font = FindLoadedFont(state.font);
    if (!font) {
        LOG("[TextManager] PrintString called without a valid font selected");
        return;
    }

    if (m_backend) {
        m_backend->Draw(displayText, x, y, state);
    }

    if ((!m_backend || !m_backend->IsReady()) && !m_warnedNoBackend) {
        const_cast<TextManager*>(this)->m_warnedNoBackend = true;
        LOG("[TextManager] Unicode shaping/raster backend is not wired yet; PrintString is currently inert");
    }
}

bool TextManager::ReadWholeFile(const char* path, std::vector<u8>& outData) {
    outData.clear();

    if (!path || !path[0]) {
        return false;
    }

    auto data = p3d::io::ReadFile(p3d::io::ResolvePath(path));
    if (!data || data->empty()) {
        return false;
    }

    outData = std::move(*data);
    return true;
}

const TextManager::LoadedFont* TextManager::FindLoadedFont(TextFontHandle handle) const {
    if (!handle) {
        return nullptr;
    }

    for (s32 i = 0; i < (s32)m_fonts.size(); i++) {
        if (m_fonts[i].handle == handle) {
            return &m_fonts[i];
        }
    }

    return nullptr;
}
