#include "code_editor.h"

#include <cctype>
#include <cstdio>
#include <imgui.h>

namespace magnetar {

// ─── C-style tokenizer (copied from TextEditor.cpp C++ definition) ───────────
// This avoids std::regex which has severe performance problems on MinGW.

static bool TokenizeCStyleString(const char* in_begin, const char* in_end,
                                  const char*& out_begin, const char*& out_end)
{
    const char* p = in_begin;
    if (*p == '"') {
        p++;
        while (p < in_end) {
            if (*p == '\\' && p + 1 < in_end)
                p += 2;  // skip escaped character
            else if (*p == '"') {
                out_begin = in_begin;
                out_end = p + 1;
                return true;
            } else
                p++;
        }
    }
    return false;
}

static bool TokenizeCStyleCharacterLiteral(const char* in_begin, const char* in_end,
                                            const char*& out_begin, const char*& out_end)
{
    const char* p = in_begin;
    if (*p == '\'') {
        p++;
        if (p < in_end && *p == '\\')
            p++;  // skip escape
        if (p < in_end)
            p++;  // skip the actual char
        if (p < in_end && *p == '\'') {
            out_begin = in_begin;
            out_end = p + 1;
            return true;
        }
    }
    return false;
}

static bool TokenizeCStyleIdentifier(const char* in_begin, const char* in_end,
                                      const char*& out_begin, const char*& out_end)
{
    const char* p = in_begin;
    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
        p++;
        while (p < in_end && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
               (*p >= '0' && *p <= '9') || *p == '_'))
            p++;
        out_begin = in_begin;
        out_end = p;
        return true;
    }
    return false;
}

static bool TokenizeCStyleNumber(const char* in_begin, const char* in_end,
                                  const char*& out_begin, const char*& out_end)
{
    const char* p = in_begin;

    // Optional sign
    if (p < in_end && (*p == '+' || *p == '-'))
        p++;

    // Hex
    if (p < in_end && *p == '0' && p + 1 < in_end && (*(p + 1) == 'x' || *(p + 1) == 'X')) {
        p += 2;
        bool hasDigits = false;
        while (p < in_end && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))) {
            p++;
            hasDigits = true;
        }
        if (hasDigits) {
            // unsigned suffix
            if (p < in_end && (*p == 'u' || *p == 'U'))
                p++;
            out_begin = in_begin;
            out_end = p;
            return true;
        }
        return false;
    }

    // Decimal / float
    const char* start = p;
    bool hasDigits = false;
    while (p < in_end && *p >= '0' && *p <= '9') {
        p++;
        hasDigits = true;
    }
    // Decimal point
    if (p < in_end && *p == '.') {
        p++;
        while (p < in_end && *p >= '0' && *p <= '9') {
            p++;
            hasDigits = true;
        }
    }
    if (!hasDigits)
        return false;

    // Exponent
    if (p < in_end && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < in_end && (*p == '+' || *p == '-'))
            p++;
        while (p < in_end && *p >= '0' && *p <= '9')
            p++;
    }
    // Float suffix
    if (p < in_end && (*p == 'f' || *p == 'F'))
        p++;
    // Unsigned suffix
    if (p < in_end && (*p == 'u' || *p == 'U'))
        p++;

    if (p > start) {
        out_begin = in_begin;
        out_end = p;
        return true;
    }
    return false;
}

static bool TokenizeCStylePunctuation(const char* in_begin, const char* /*in_end*/,
                                       const char*& out_begin, const char*& out_end)
{
    switch (*in_begin) {
        case '[': case ']': case '{': case '}': case '!': case '%': case '^': case '&':
        case '*': case '(': case ')': case '-': case '+': case '=': case '~': case '|':
        case '<': case '>': case '?': case '/': case ';': case ',': case '.': case ':':
            out_begin = in_begin;
            out_end = in_begin + 1;
            return true;
        default:
            return false;
    }
}

// ─── CodeEditor implementation ───────────────────────────────────────────────

CodeEditor::CodeEditor()
{
    setupGlslLanguage();
    setupMagnetarPalette();
    m_editor.SetShowWhitespaces(false);
    m_editor.SetTabSize(4);
}

void CodeEditor::render()
{
    m_editor.Render("##CodeEditor");
    updateAutocomplete();
    renderAutocompletePopup();
}

std::string CodeEditor::getText() const
{
    return m_editor.GetText();
}

void CodeEditor::setText(const std::string& text)
{
    m_editor.SetText(text);
}

bool CodeEditor::isTextChanged() const
{
    return m_editor.IsTextChanged();
}

void CodeEditor::setErrorMarkers(const std::map<int, std::string>& markers)
{
    m_editor.SetErrorMarkers(markers);
}

void CodeEditor::clearErrorMarkers()
{
    TextEditor::ErrorMarkers empty;
    m_editor.SetErrorMarkers(empty);
}

int CodeEditor::getCursorLine() const
{
    return m_editor.GetCursorPosition().mLine;
}

int CodeEditor::getCursorCol() const
{
    return m_editor.GetCursorPosition().mColumn;
}

void CodeEditor::setCursorPosition(int line, int col)
{
    m_editor.SetCursorPosition(TextEditor::Coordinates(line, col));
}

// ─── GLSL language definition ────────────────────────────────────────────────

void CodeEditor::setupGlslLanguage()
{
    TextEditor::LanguageDefinition langDef;

    langDef.mName = "GLSL";
    langDef.mCommentStart = "/*";
    langDef.mCommentEnd = "*/";
    langDef.mSingleLineComment = "//";
    langDef.mAutoIndentation = true;
    langDef.mCaseSensitive = true;
    langDef.mPreprocChar = '#';

    // Use the C-style tokenizer callback to avoid std::regex performance issues on MinGW
    langDef.mTokenize = [](const char* in_begin, const char* in_end,
                           const char*& out_begin, const char*& out_end,
                           TextEditor::PaletteIndex& paletteIndex) -> bool
    {
        paletteIndex = TextEditor::PaletteIndex::Max;

        while (in_begin < in_end && isascii(*in_begin) && isblank(*in_begin))
            in_begin++;

        if (in_begin == in_end) {
            out_begin = in_end;
            out_end = in_end;
            paletteIndex = TextEditor::PaletteIndex::Default;
        }
        else if (TokenizeCStyleString(in_begin, in_end, out_begin, out_end))
            paletteIndex = TextEditor::PaletteIndex::String;
        else if (TokenizeCStyleCharacterLiteral(in_begin, in_end, out_begin, out_end))
            paletteIndex = TextEditor::PaletteIndex::CharLiteral;
        else if (TokenizeCStyleIdentifier(in_begin, in_end, out_begin, out_end))
            paletteIndex = TextEditor::PaletteIndex::Identifier;
        else if (TokenizeCStyleNumber(in_begin, in_end, out_begin, out_end))
            paletteIndex = TextEditor::PaletteIndex::Number;
        else if (TokenizeCStylePunctuation(in_begin, in_end, out_begin, out_end))
            paletteIndex = TextEditor::PaletteIndex::Punctuation;

        return paletteIndex != TextEditor::PaletteIndex::Max;
    };

    // ── GLSL keywords ──
    static const char* const keywords[] = {
        "void", "bool", "int", "uint", "float", "double",
        "vec2", "vec3", "vec4", "ivec2", "ivec3", "ivec4",
        "uvec2", "uvec3", "uvec4", "bvec2", "bvec3", "bvec4",
        "dvec2", "dvec3", "dvec4",
        "mat2", "mat3", "mat4", "mat2x3", "mat2x4", "mat3x2", "mat3x4", "mat4x2", "mat4x3",
        "dmat2", "dmat3", "dmat4",
        "sampler2D", "sampler3D", "samplerCube", "sampler2DShadow",
        "samplerCubeShadow", "sampler2DArray",
        "isampler2D", "isampler3D", "usampler2D", "usampler3D", "image2D",
        "struct", "uniform", "in", "out", "inout",
        "layout", "flat", "smooth", "noperspective", "centroid",
        "const", "coherent", "volatile", "restrict", "readonly", "writeonly",
        "buffer", "shared", "precision", "highp", "mediump", "lowp",
        "if", "else", "for", "while", "do", "switch", "case", "default",
        "break", "continue", "return", "discard", "true", "false"
    };
    for (auto& k : keywords)
        langDef.mKeywords.insert(k);

    // ── GLSL built-in functions and variables (shown as known identifiers) ──
    static const char* const builtins[] = {
        "texture", "textureLod", "textureOffset", "texelFetch",
        "textureGrad", "textureSize", "textureLodOffset", "textureProj",
        "textureProjLod", "textureGather",
        "sin", "cos", "tan", "asin", "acos", "atan",
        "sinh", "cosh", "tanh", "asinh", "acosh", "atanh",
        "pow", "exp", "log", "exp2", "log2", "sqrt", "inversesqrt",
        "abs", "sign", "floor", "ceil", "round", "roundEven", "trunc",
        "fract", "mod", "modf", "fma",
        "min", "max", "clamp", "mix", "step", "smoothstep",
        "isnan", "isinf",
        "length", "distance", "dot", "cross", "normalize",
        "reflect", "refract", "faceforward",
        "matrixCompMult", "transpose", "inverse", "determinant", "outerProduct",
        "lessThan", "greaterThan", "lessThanEqual", "greaterThanEqual",
        "equal", "notEqual", "any", "all", "not",
        "dFdx", "dFdy", "fwidth",
        "dFdxFine", "dFdyFine", "dFdxCoarse", "dFdyCoarse",
        "fwidthFine", "fwidthCoarse",
        "floatBitsToInt", "floatBitsToUint", "intBitsToFloat", "uintBitsToFloat",
        "packSnorm2x16", "unpackSnorm2x16", "packHalf2x16", "unpackHalf2x16",
        "packUnorm2x16", "unpackUnorm2x16",
        "bitfieldExtract", "bitfieldInsert", "bitfieldReverse",
        "bitCount", "findLSB", "findMSB",
        "imageLoad", "imageStore", "imageSize",
        "barrier", "memoryBarrier",
        "atomicAdd", "atomicMin", "atomicMax", "atomicAnd",
        "atomicOr", "atomicXor", "atomicExchange", "atomicCompSwap",
        "gl_FragCoord", "gl_FragColor", "gl_Position",
        "gl_VertexID", "gl_InstanceID", "gl_FrontFacing",
        "gl_PointSize", "gl_FragDepth",
        "mainImage", "fragCoord"
    };
    for (auto& b : builtins) {
        TextEditor::Identifier id;
        id.mDeclaration = "Built-in function";
        langDef.mIdentifiers.insert(std::make_pair(std::string(b), id));
    }

    // ── Shadertoy uniforms ──
    auto addUniform = [&](const char* name, const char* decl) {
        TextEditor::Identifier id;
        id.mDeclaration = decl;
        langDef.mIdentifiers.insert(std::make_pair(std::string(name), id));
    };
    addUniform("iTime",              "uniform float iTime");
    addUniform("iTimeDelta",         "uniform float iTimeDelta");
    addUniform("iFrame",             "uniform int iFrame");
    addUniform("iResolution",        "uniform vec3 iResolution");
    addUniform("iMouse",             "uniform vec4 iMouse");
    addUniform("iDate",              "uniform vec4 iDate");
    addUniform("iChannel0",          "uniform sampler2D iChannel0");
    addUniform("iChannel1",          "uniform sampler2D iChannel1");
    addUniform("iChannel2",          "uniform sampler2D iChannel2");
    addUniform("iChannel3",          "uniform sampler2D iChannel3");
    addUniform("iChannelResolution", "uniform vec3 iChannelResolution[4]");

    m_editor.SetLanguageDefinition(langDef);
}

// ─── Magnetar palette ────────────────────────────────────────────────────────

void CodeEditor::setupMagnetarPalette()
{
    TextEditor::Palette palette;

    palette[(int)TextEditor::PaletteIndex::Default]               = IM_COL32(0xf9, 0xfa, 0xfb, 0xff);  // off-white
    palette[(int)TextEditor::PaletteIndex::Keyword]               = IM_COL32(0x8b, 0x5c, 0xf6, 0xff);  // violet
    palette[(int)TextEditor::PaletteIndex::Number]                = IM_COL32(0x06, 0xb6, 0xd4, 0xff);  // cyan
    palette[(int)TextEditor::PaletteIndex::String]                = IM_COL32(0xd9, 0x46, 0xef, 0xff);  // magenta
    palette[(int)TextEditor::PaletteIndex::CharLiteral]           = IM_COL32(0xd9, 0x46, 0xef, 0xff);  // magenta
    palette[(int)TextEditor::PaletteIndex::Punctuation]           = IM_COL32(0x6b, 0x72, 0x80, 0xff);  // dust gray
    palette[(int)TextEditor::PaletteIndex::Preprocessor]          = IM_COL32(0xd9, 0x46, 0xef, 0xff);  // magenta
    palette[(int)TextEditor::PaletteIndex::Identifier]            = IM_COL32(0xf9, 0xfa, 0xfb, 0xff);  // off-white
    palette[(int)TextEditor::PaletteIndex::KnownIdentifier]       = IM_COL32(0x06, 0xb6, 0xd4, 0xff);  // cyan
    palette[(int)TextEditor::PaletteIndex::PreprocIdentifier]     = IM_COL32(0xd9, 0x46, 0xef, 0xff);  // magenta
    palette[(int)TextEditor::PaletteIndex::Comment]               = IM_COL32(0x6b, 0x72, 0x80, 0xff);  // dust gray
    palette[(int)TextEditor::PaletteIndex::MultiLineComment]      = IM_COL32(0x6b, 0x72, 0x80, 0xff);  // dust gray
    palette[(int)TextEditor::PaletteIndex::Background]            = IM_COL32(0x0a, 0x0a, 0x0f, 0xff);  // void black
    palette[(int)TextEditor::PaletteIndex::Cursor]                = IM_COL32(0xf9, 0xfa, 0xfb, 0xff);  // off-white
    palette[(int)TextEditor::PaletteIndex::Selection]             = IM_COL32(0x8b, 0x5c, 0xf6, 0x59);  // violet 35%
    palette[(int)TextEditor::PaletteIndex::ErrorMarker]           = IM_COL32(0xef, 0x44, 0x44, 0x4d);  // red tinted bg
    palette[(int)TextEditor::PaletteIndex::Breakpoint]            = IM_COL32(0x00, 0x00, 0x00, 0x00);  // unused
    palette[(int)TextEditor::PaletteIndex::LineNumber]            = IM_COL32(0x6b, 0x72, 0x80, 0xff);  // dust gray
    palette[(int)TextEditor::PaletteIndex::CurrentLineFill]       = IM_COL32(0x8b, 0x5c, 0xf6, 0x0f);  // subtle violet
    palette[(int)TextEditor::PaletteIndex::CurrentLineFillInactive] = IM_COL32(0x8b, 0x5c, 0xf6, 0x08);
    palette[(int)TextEditor::PaletteIndex::CurrentLineEdge]       = IM_COL32(0x8b, 0x5c, 0xf6, 0x20);  // violet dim

    m_editor.SetPalette(palette);
}

// ─── Autocomplete ────────────────────────────────────────────────────────────

void CodeEditor::updateAutocomplete()
{
    auto cursor = m_editor.GetCursorPosition();
    int curLine = cursor.mLine;

    // Dismiss if the cursor jumped to a different line
    if (m_acVisible && curLine != m_acPrevLine) {
        dismissAutocomplete();
        return;
    }

    // Extract the word ending at the cursor from the current line text
    std::string lineText = m_editor.GetCurrentLineText();
    int col = cursor.mColumn;

    // Walk backwards from cursor to find identifier start
    int wordStart = col;
    while (wordStart > 0 && wordStart <= static_cast<int>(lineText.size())) {
        char c = lineText[wordStart - 1];
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
            --wordStart;
        else
            break;
    }

    std::string word;
    if (wordStart < col && col <= static_cast<int>(lineText.size())) {
        word = lineText.substr(wordStart, col - wordStart);
    }

    // Only trigger on 2+ char identifiers starting with a letter or underscore
    if (word.size() >= 2 &&
        (std::isalpha(static_cast<unsigned char>(word[0])) || word[0] == '_'))
    {
        // Only re-filter if the prefix changed
        if (word != m_acPrefix) {
            m_acPrefix = word;
            m_acFiltered = GlslAutocomplete::filter(m_acPrefix);
            m_acSelected = 0;
        }

        if (!m_acFiltered.empty()) {
            // Don't show popup if there's exactly one match and it equals the typed word
            if (m_acFiltered.size() == 1 && m_acFiltered[0]->name == word) {
                dismissAutocomplete();
                return;
            }
            m_acVisible = true;
            m_acPrevLine = curLine;
            m_editor.SetHandleKeyboardInputs(false);
        } else {
            dismissAutocomplete();
        }
    } else {
        dismissAutocomplete();
    }
}

void CodeEditor::renderAutocompletePopup()
{
    if (!m_acVisible || m_acFiltered.empty()) return;

    // Handle keyboard input while popup is visible
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        dismissAutocomplete();
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        m_acSelected = (m_acSelected + 1) % static_cast<int>(m_acFiltered.size());
    }
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        m_acSelected = (m_acSelected - 1 + static_cast<int>(m_acFiltered.size()))
                       % static_cast<int>(m_acFiltered.size());
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Tab) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        acceptCompletion();
        return;
    }

    // Compute popup position from editor widget rect + cursor coordinates
    ImVec2 editorOrigin = ImGui::GetItemRectMin();
    float lineHeight = ImGui::GetFontSize();
    float charWidth  = ImGui::CalcTextSize("x").x;

    auto cursor = m_editor.GetCursorPosition();

    // The TextEditor renders line numbers on the left.  Estimate the gutter:
    // ~6 characters wide (4 digits + space + separator) × charWidth.
    float gutterWidth = charWidth * 6.0f;

    // Word start column: cursor column minus the length of the current prefix
    int wordStartCol = cursor.mColumn - static_cast<int>(m_acPrefix.size());
    if (wordStartCol < 0) wordStartCol = 0;

    float popupX = editorOrigin.x + gutterWidth + wordStartCol * charWidth;
    float popupY = editorOrigin.y + (cursor.mLine + 1) * lineHeight;

    // Clamp to viewport so the popup stays on screen
    ImVec2 vpSize = ImGui::GetIO().DisplaySize;
    int maxVisible = 8;
    int visibleCount = std::min(maxVisible, static_cast<int>(m_acFiltered.size()));
    float popupH = visibleCount * lineHeight + 8.0f;  // padding
    float popupW = 380.0f;
    if (popupX + popupW > vpSize.x) popupX = vpSize.x - popupW;
    if (popupY + popupH > vpSize.y) popupY = popupY - lineHeight - popupH;

    ImGui::SetNextWindowPos(ImVec2(popupX, popupY));
    ImGui::SetNextWindowSize(ImVec2(popupW, popupH));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0x14, 0x14, 0x1e, 0xf0));
    ImGui::PushStyleColor(ImGuiCol_Border,   IM_COL32(0x8b, 0x5c, 0xf6, 0x80));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 4.0f));

    if (ImGui::Begin("##autocomplete_popup", nullptr, flags)) {
        for (int i = 0; i < static_cast<int>(m_acFiltered.size()); ++i) {
            const auto* item = m_acFiltered[i];
            bool isSelected = (i == m_acSelected);

            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0x8b, 0x5c, 0xf6, 0x50));
            }

            // Category label prefix
            const char* catLabel = "";
            ImU32 catColor = IM_COL32(0x6b, 0x72, 0x80, 0xff);
            switch (item->category) {
                case CompletionCategory::Function:  catLabel = "fn "; catColor = IM_COL32(0x06, 0xb6, 0xd4, 0xff); break;
                case CompletionCategory::Type:      catLabel = "T  "; catColor = IM_COL32(0x8b, 0x5c, 0xf6, 0xff); break;
                case CompletionCategory::Keyword:   catLabel = "kw "; catColor = IM_COL32(0x8b, 0x5c, 0xf6, 0xff); break;
                case CompletionCategory::Uniform:   catLabel = "U  "; catColor = IM_COL32(0xfb, 0xbf, 0x24, 0xff); break;
                case CompletionCategory::Qualifier: catLabel = "Q  "; catColor = IM_COL32(0x6b, 0x72, 0x80, 0xff); break;
                case CompletionCategory::Variable:  catLabel = "V  "; catColor = IM_COL32(0x34, 0xd3, 0x99, 0xff); break;
            }

            ImGui::TextColored(ImColor(catColor).Value, "%s", catLabel);
            ImGui::SameLine();

            if (ImGui::Selectable(item->signature.c_str(), isSelected)) {
                m_acSelected = i;
                acceptCompletion();
                if (isSelected) ImGui::PopStyleColor();
                break;
            }

            if (isSelected) {
                ImGui::PopStyleColor();
                // Scroll to keep selected item visible
                if (ImGui::GetScrollMaxY() > 0)
                    ImGui::SetScrollHereY();
            }
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void CodeEditor::acceptCompletion()
{
    if (m_acSelected < 0 || m_acSelected >= static_cast<int>(m_acFiltered.size()))
        return;

    const auto* item = m_acFiltered[m_acSelected];
    auto cursor = m_editor.GetCursorPosition();

    // Select the partial word: from (line, col - prefix.size()) to (line, col)
    int startCol = cursor.mColumn - static_cast<int>(m_acPrefix.size());
    if (startCol < 0) startCol = 0;

    TextEditor::Coordinates selStart(cursor.mLine, startCol);
    TextEditor::Coordinates selEnd(cursor.mLine, cursor.mColumn);
    m_editor.SetSelection(selStart, selEnd);
    m_editor.InsertText(item->name);

    dismissAutocomplete();
}

void CodeEditor::dismissAutocomplete()
{
    if (m_acVisible) {
        m_acVisible = false;
        m_editor.SetHandleKeyboardInputs(true);
    }
    m_acSelected = 0;
    m_acPrefix.clear();
    m_acFiltered.clear();
    m_acPrevLine = -1;
}

} // namespace magnetar
