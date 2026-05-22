#include "Core/Context.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "Core/DebugMessageCallback.hpp"

#include "Util/Input.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"

#include "config.hpp"

using Util::ms_t;

namespace Core {
namespace {
std::string ToLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool IsLikelyCjkFont(const std::string& path) {
    const std::string lower = ToLowerAscii(path);
    return lower.find("noto") != std::string::npos ||
           lower.find("sourcehan") != std::string::npos ||
           lower.find("source-han") != std::string::npos ||
           lower.find("cjk") != std::string::npos ||
           lower.find("tc") != std::string::npos ||
           lower.find("wqy") != std::string::npos ||
           lower.find("arphic") != std::string::npos ||
           lower.find("uming") != std::string::npos ||
           lower.find("ukai") != std::string::npos;
}

const ImWchar* GetTraditionalChineseGlyphRanges() {
    static const ImWchar ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x2000, 0x206F, // General Punctuation
        0x3000, 0x30FF, // CJK Symbols and Japanese kana
        0x3100, 0x312F, // Bopomofo
        0x31A0, 0x31BF, // Bopomofo Extended
        0x31F0, 0x31FF, // Katakana Phonetic Extensions
        0x3200, 0x32FF, // Enclosed CJK Letters and Months
        0x3400, 0x4DBF, // CJK Unified Ideographs Extension A
        0x4E00, 0x9FFF, // CJK Unified Ideographs
        0xF900, 0xFAFF, // CJK Compatibility Ideographs
        0xFE10, 0xFE1F, // Vertical Forms
        0xFE30, 0xFE6F, // CJK Compatibility Forms + Small Form Variants
        0xFF00, 0xFFEF, // Halfwidth and Fullwidth Forms
        0xFFFD, 0xFFFD, // Replacement character
        0,
    };
    return ranges;
}

void AddSizedCjkFont(ImGuiIO& io, const std::string& path, float size,
                     std::vector<std::string>* loadedPaths, const char* label) {
    ImFontConfig fontCfg;
    fontCfg.OversampleH = 3;
    fontCfg.OversampleV = 2;
    fontCfg.PixelSnapH = true;
    fontCfg.MergeMode = false;
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        path.c_str(), size, &fontCfg, GetTraditionalChineseGlyphRanges());
    if (font != nullptr && loadedPaths != nullptr) {
        loadedPaths->push_back(path + " (" + std::string(label) + ")");
    }
}

std::vector<std::string> BuildBaseFontCandidates() {
    std::vector<std::string> candidates;
    if (const char* envFont = std::getenv("ARKNIGHT_UI_FONT"); envFont != nullptr && envFont[0] != '\0') {
        candidates.emplace_back(envFont);
    }
    if (const char* envFont = std::getenv("ARKNIGHT_UI_CJK_FONT"); envFont != nullptr && envFont[0] != '\0') {
        candidates.emplace_back(envFont);
    }
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/NotoSansCJKtc-Regular.otf");
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/NotoSansTC-Regular.otf");
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/NotoSansCJK-Regular.ttc");
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/NotoSansTC-Regular.ttf");
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/Inter.ttf");
    candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    candidates.emplace_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
    return candidates;
}

std::vector<std::string> BuildCjkFontCandidates() {
    std::vector<std::string> candidates;
    if (const char* envFont = std::getenv("ARKNIGHT_UI_CJK_FONT"); envFont != nullptr && envFont[0] != '\0') {
        candidates.emplace_back(envFont);
    }
    if (const char* envFont = std::getenv("ARKNIGHT_UI_FONT"); envFont != nullptr && envFont[0] != '\0') {
        candidates.emplace_back(envFont);
    }
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/NotoSansCJKtc-Regular.otf");
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/NotoSansTC-Regular.otf");
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/NotoSansCJK-Regular.ttc");
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/NotoSansTC-Regular.ttf");
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/NotoSansTC-Medium.ttf");
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/NotoSansCJKtc-Regular.ttf");
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/wqy-microhei.ttc");
    candidates.emplace_back(std::string(PTSD_ASSETS_DIR) + "/fonts/wqy-zenhei.ttc");
    candidates.emplace_back("/usr/share/fonts/noto/NotoSansCJK-Regular.ttc");
    candidates.emplace_back("/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc");
    candidates.emplace_back("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
    candidates.emplace_back("/usr/share/fonts/opentype/noto/NotoSansCJKtc-Regular.otf");
    candidates.emplace_back("/usr/share/fonts/opentype/noto/NotoSansTC-Regular.otf");
    candidates.emplace_back("/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc");
    candidates.emplace_back("/usr/share/fonts/truetype/noto/NotoSansCJKtc-Regular.otf");
    candidates.emplace_back("/usr/share/fonts/truetype/noto/NotoSansTC-Regular.ttf");
    candidates.emplace_back("/usr/share/fonts/truetype/arphic/uming.ttc");
    candidates.emplace_back("/usr/share/fonts/truetype/arphic/ukai.ttc");
    candidates.emplace_back("/usr/share/fonts/truetype/wqy/wqy-microhei.ttc");
    candidates.emplace_back("/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc");
    return candidates;
}

ImFont* LoadTraditionalChineseFont(ImGuiIO& io, std::vector<std::string>* loadedPaths, bool* cjkLoaded) {
    if (cjkLoaded != nullptr) *cjkLoaded = false;
    ImFont* baseFont = nullptr;
    std::unordered_set<std::string> visited;

    for (const auto &path : BuildBaseFontCandidates()) {
        if (!visited.insert(path).second) continue;
        if (!std::filesystem::exists(path)) continue;

        ImFontConfig fontCfg;
        fontCfg.OversampleH = 2;
        fontCfg.OversampleV = 2;
        fontCfg.PixelSnapH = false;
        fontCfg.MergeMode = false;

        const bool cjkBase = IsLikelyCjkFont(path);
        ImFont* font = io.Fonts->AddFontFromFileTTF(
            path.c_str(), 20.0F, &fontCfg,
            cjkBase ? GetTraditionalChineseGlyphRanges() : io.Fonts->GetGlyphRangesDefault());
        if (font != nullptr) {
            if (loadedPaths != nullptr) loadedPaths->push_back(path + " (base)");
                if (cjkBase && cjkLoaded != nullptr) *cjkLoaded = true;
                baseFont = font;
                if (cjkBase) {
                    AddSizedCjkFont(io, path, 24.0F, loadedPaths, "ui-24");
                    AddSizedCjkFont(io, path, 36.0F, loadedPaths, "large");
                }
            break;
        }
    }

    if (baseFont == nullptr) {
        baseFont = io.Fonts->AddFontDefault();
    }

    if (cjkLoaded == nullptr || !*cjkLoaded) {
        for (const auto &path : BuildCjkFontCandidates()) {
            if (!visited.insert(path).second) continue;
            if (!std::filesystem::exists(path)) continue;

            ImFontConfig fontCfg;
            fontCfg.OversampleH = 2;
            fontCfg.OversampleV = 2;
            fontCfg.PixelSnapH = false;
            fontCfg.MergeMode = true;

            ImFont* font = io.Fonts->AddFontFromFileTTF(
                path.c_str(), 20.0F, &fontCfg, GetTraditionalChineseGlyphRanges());
            if (font != nullptr) {
                if (loadedPaths != nullptr) loadedPaths->push_back(path + " (cjk)");
                if (cjkLoaded != nullptr) *cjkLoaded = true;
                AddSizedCjkFont(io, path, 24.0F, loadedPaths, "ui-24");
                AddSizedCjkFont(io, path, 36.0F, loadedPaths, "large");
                break;
            }
        }
    }
    return baseFont;
}

} // namespace

Context::Context() {
    Util::Logger::Init();
    PTSD_Config::Init();
    Util::Logger::SetLevel(PTSD_Config::DEFAULT_LOG_LEVEL);

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LOG_ERROR("Failed to initialize SDL");
        LOG_ERROR(SDL_GetError());
    }

    if (IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) < 0) {
        LOG_ERROR("Failed to initialize SDL_image");
        LOG_ERROR(SDL_GetError());
    }

    if (TTF_Init() < 0) {
        LOG_ERROR("Failed to initialize SDL_ttf");
        LOG_ERROR(SDL_GetError());
    }

    if (Mix_Init(MIX_INIT_MP3) < 0) {
        LOG_ERROR("Failed to initialize SDL_mixer");
        LOG_ERROR(SDL_GetError());
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        LOG_ERROR("Failed to initialize SDL_mixer");
        LOG_ERROR(SDL_GetError());
    }
    m_Window = SDL_CreateWindow(
        PTSD_Config::TITLE.c_str(), PTSD_Config::WINDOW_POS_X,
        PTSD_Config::WINDOW_POS_Y, PTSD_Config::WINDOW_WIDTH,
        PTSD_Config::WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

    if (m_Window == nullptr) {
        LOG_ERROR("Failed to create window");
        LOG_ERROR(SDL_GetError());
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    m_GlContext = SDL_GL_CreateContext(m_Window);

    if (m_GlContext == nullptr) {
        LOG_ERROR("Failed to initialize GL context");
        LOG_ERROR(SDL_GetError());
    }

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        GLuint err = glGetError();
        LOG_ERROR(reinterpret_cast<const char *>(glewGetErrorString(err)));
    }

#ifndef __APPLE__
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(Core::OpenGLDebugMessageCallback, nullptr);
#endif

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    LOG_INFO("OpenGL Info");
    LOG_INFO("  Vendor: {}", glGetString(GL_VENDOR));
    LOG_INFO("  Renderer: {}", glGetString(GL_RENDERER));
    LOG_INFO("  Version: {}", glGetString(GL_VERSION));
    LOG_INFO("  GLSL Version: {}", glGetString(GL_SHADING_LANGUAGE_VERSION));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
    io.Fonts->TexDesiredWidth = 8192;

    std::vector<std::string> loadedFontPaths;
    bool hasCjkGlyphs = false;
    ImFont* cjkFont = LoadTraditionalChineseFont(io, &loadedFontPaths, &hasCjkGlyphs);
    if (cjkFont != nullptr) {
        io.FontDefault = cjkFont;
        for (const auto& path : loadedFontPaths) {
            LOG_INFO("Loaded UI font: {}", path);
        }
        if (loadedFontPaths.empty()) {
            LOG_WARN("Using ImGui default font only. Chinese glyphs may not render.");
        } else if (!hasCjkGlyphs) {
            LOG_WARN("CJK font fallback failed. Chinese glyphs may not render.");
            LOG_WARN("Please set ARKNIGHT_UI_CJK_FONT=/absolute/path/to/NotoSansTC-Regular.ttf (or .ttc)");
        }
    } else {
        io.Fonts->AddFontDefault();
        LOG_WARN("Font initialization failed. Install Noto CJK and/or set ARKNIGHT_UI_CJK_FONT=/path/to/font.ttf");
    }

    ImGui_ImplSDL2_InitForOpenGL(m_Window, m_GlContext);
    ImGui_ImplOpenGL3_Init();
}
std::shared_ptr<Context> Context::s_Instance(nullptr);

Context::~Context() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyWindow(m_Window);
    SDL_GL_DeleteContext(m_GlContext);
    SDL_VideoQuit();
    Mix_HaltGroup(-1);
    Mix_CloseAudio();

    TTF_Quit();
    IMG_Quit();
    Mix_Quit();
    SDL_Quit();
}

void Context::Setup() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
}

void Context::Update() {
    Util::Input::Update();
    SDL_GL_SwapWindow(m_Window);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    static ms_t frameTime =
        PTSD_Config::FPS_CAP != 0 ? 1000.0F / PTSD_Config::FPS_CAP : 0;
    ms_t afterUpdate = Util::Time::GetElapsedTimeMs();
    ms_t updateTime = afterUpdate - m_BeforeUpdateTime;
    if (updateTime < frameTime) {
        SDL_Delay(static_cast<Uint32>(frameTime - updateTime));
    }
    m_BeforeUpdateTime = Util::Time::GetElapsedTimeMs();

    // Here's a figure explaining how Delta time & Delay work:
    //
    // --|--UT--|--Delay--|--UT--|--
    //   |---Delta time---|  ^ Last delta time used here
    //   ^                ^
    //   (s_Last)         (s_Now) Time::Update here
    //
    // # Updating/rendering time is denoted as "UT"
    Util::Time::Update();

#ifdef DEBUG_DELTA_TIME
    auto deltaTime = Util::Time::GetDeltaTimeMs();
    LOG_DEBUG("Delta(Update+Delay): {:.1f}({:.1f}+{:.1f}) ms, FPS: {:.1f}",
              deltaTime, updateTime,
              updateTime < frameTime ? frameTime - updateTime : 0,
              1000.0f / deltaTime);
#endif // DEBUG_DELTA_TIME
}

std::shared_ptr<Context> Context::GetInstance() {
    if (s_Instance == nullptr) {
        s_Instance = std::make_shared<Context>();
    }
    return s_Instance;
}

void Context::SetWindowIcon(const std::string &path) {
    SDL_Surface *image = IMG_Load(path.c_str());
    if (image) {
        SDL_SetWindowIcon(m_Window, image);
        SDL_FreeSurface(image);
        return;
    }
    LOG_ERROR("Failed to load image: {}", path);
}
} // namespace Core
