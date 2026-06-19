// ─────────────────────────────────────────────────────────────────────────────
// App.cpp  –Arknights clone   (main lifecycle & input handling)
//
// Implementation is split across multiple files:
//   Camera.cpp        –Projection & coordinate mapping
//   EnemySystem.cpp   –Enemy spawning, movement & AI
//   OperatorSystem.cpp–Operator combat, skills & deployment helpers
//   Renderer/*        –All drawing / rendering code
//   GameLogic.cpp     - Wave management, game state, stage init
// ─────────────────────────────────────────────────────────────────────────────
#include "App.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "Ark/GameConstants.hpp"
#include "Ark/Renderer.hpp"
#include "Ark/Renderer/RendererShared.hpp"
#include "Ark/StageLoader.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Time.hpp"
#include "config.hpp"

using namespace Ark;

#ifdef _MSC_VER
#define popen _popen
#define pclose _pclose
#endif

namespace {
constexpr int MAX_OPS = Ark::GameConst::MAX_OPS;
constexpr float REDEPLOY_COOLDOWN_MS = Ark::GameConst::REDEPLOY_COOLDOWN_MS;

// Bottom operator bar layout constants (screen-space)
constexpr float OP_BAR_HEIGHT     = Ark::RendererConst::OP_BAR_HEIGHT;
constexpr float OP_CARD_WIDTH     = Ark::RendererConst::OP_CARD_WIDTH;
constexpr float OP_CARD_HEIGHT    = Ark::RendererConst::OP_CARD_HEIGHT;
constexpr float OP_CARD_SPACING   = Ark::RendererConst::OP_CARD_SPACING;
constexpr float LOADING_FADE_IN_MS = 500.0F;
constexpr float LOADING_HOLD_MS = 250.0F;
constexpr float LOADING_FADE_OUT_MS = 500.0F;
constexpr float OPENING_VIDEO_1_DEFAULT_START_MS = 5000.0F;
constexpr float OPENING_VIDEO_1_DEFAULT_END_MS = 600000.0F;
constexpr float OPENING_VIDEO_2_DEFAULT_START_MS = 2000.0F;
constexpr float OPENING_VIDEO_2_PAUSE_MS = 5000.0F;
constexpr float OPENING_VIDEO_SWITCH_MS = 500.0F;
constexpr float OPENING_FADE_MS = OPENING_VIDEO_SWITCH_MS * 0.5F;
constexpr float OPENING_MENU_FADE_MS = 700.0F;
constexpr int OPENING_VIDEO_DEFAULT_MAX_DIMENSION = 1280;
constexpr const char* STAGE_1_1_FILE = "Operation 1-1/stage";
constexpr const char* STAGE_1_2_FILE = "Operation 1-2/stage";
constexpr glm::vec2 MENU_REFERENCE_SIZE{2796.0F, 1290.0F};
constexpr glm::vec2 VIDEO_1_REFERENCE_SIZE{2342.0F, 1080.0F};
constexpr glm::vec2 VIDEO_2_REFERENCE_SIZE{2342.0F, 1080.0F};

struct VideoInfo {
    int width = 0;
    int height = 0;
    double fps = 50.0;
    double durationMs = 0.0;
};

std::string ShellQuote(const std::string& value) {
#ifdef _WIN32
    std::string quoted = "\"";
    for (char c : value) {
        if (c == '"') quoted += "\"\"";
        else quoted += c;
    }
    quoted += "\"";
    return quoted;
#else
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'') quoted += "'\\''";
        else quoted += c;
    }
    quoted += "'";
    return quoted;
#endif
}

bool ParseFrameRate(const std::string& raw, double& fps) {
    if (raw.empty()) return false;
    int numerator = 0;
    int denominator = 1;
    if (std::sscanf(raw.c_str(), "%d/%d", &numerator, &denominator) >= 1 && numerator > 0) {
        if (denominator <= 0) denominator = 1;
        fps = static_cast<double>(numerator) / static_cast<double>(denominator);
        return fps > 0.0;
    }
    return false;
}

double NormalizeVideoFps(double fps) {
    if (fps < 1.0 || fps > 120.0) return 50.0;
    return fps;
}

std::string ReadFirstLineFromCommand(const std::string& command) {
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) return {};

    char buffer[512] = {};
    const bool gotLine = std::fgets(buffer, sizeof(buffer), pipe) != nullptr;
    pclose(pipe);
    return gotLine ? std::string(buffer) : std::string{};
}

bool ProbeVideoInfo(const std::string& path, VideoInfo& info) {
    const std::string command =
        "ffprobe -v quiet -select_streams v:0 "
        "-show_entries stream=width,height,avg_frame_rate,r_frame_rate,duration "
        "-of csv=p=0:s=x " + ShellQuote(path);
    std::string line = ReadFirstLineFromCommand(command);
    if (line.empty()) return false;

    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

    std::vector<std::string> fields;
    std::stringstream input(line);
    std::string field;
    while (std::getline(input, field, 'x')) fields.push_back(field);
    if (fields.size() < 4) return false;

    try {
        info.width = std::stoi(fields[0]);
        info.height = std::stoi(fields[1]);
    } catch (const std::exception&) {
        return false;
    }
    if (info.width <= 0 || info.height <= 0) return false;

    double fps = 0.0;
    if ((fields.size() > 2 && ParseFrameRate(fields[2], fps)) ||
        (fields.size() > 3 && ParseFrameRate(fields[3], fps))) {
        info.fps = NormalizeVideoFps(fps);
    }

    if (fields.size() > 4) {
        try {
            const double durationSec = std::stod(fields[4]);
            if (durationSec > 0.0) info.durationMs = durationSec * 1000.0;
        } catch (const std::exception&) {
            info.durationMs = 0.0;
        }
    }
    return true;
}

int OpeningVideoMaxDimension() {
    if (const char* raw = std::getenv("ARKNIGHT_OPENING_VIDEO_MAX_DIMENSION");
        raw != nullptr && raw[0] != '\0') {
        try {
            return std::clamp(std::stoi(raw), 320, 2342);
        } catch (const std::exception&) {
            return OPENING_VIDEO_DEFAULT_MAX_DIMENSION;
        }
    }
    return OPENING_VIDEO_DEFAULT_MAX_DIMENSION;
}

void AddCommonFfmpegPathsOnWindows() {
#ifdef _WIN32
    static bool pathUpdated = []() {
        const char* path = std::getenv("PATH");
        std::string newPath = path ? path : "";
        bool updated = false;
        if (newPath.find("C:\\Program Files\\ffmpeg\\bin") == std::string::npos) {
            newPath += ";C:\\Program Files\\ffmpeg\\bin";
            updated = true;
        }
        const char* userProfile = std::getenv("USERPROFILE");
        if (userProfile != nullptr) {
            const std::string scoopShims = std::string(userProfile) + "\\scoop\\shims";
            if (newPath.find(scoopShims) == std::string::npos) {
                newPath += ";" + scoopShims;
                updated = true;
            }
        }
        if (updated) {
            _putenv_s("PATH", newPath.c_str());
        }
        return true;
    }();
    (void)pathUpdated;
#endif
}

std::string ResolveLoadingPageAsset(std::initializer_list<const char*> filenames) {
    const std::array<std::filesystem::path, 4> roots{
        std::filesystem::path("data") / "loadingpage",
        std::filesystem::path("..") / "data" / "loadingpage",
        std::filesystem::path("..") / ".." / "data" / "loadingpage",
        std::filesystem::path("..") / ".." / ".." / "data" / "loadingpage",
    };

    for (const auto* filename : filenames) {
        for (const auto& root : roots) {
            const auto path = root / filename;
            if (std::filesystem::exists(path)) return path.lexically_normal().string();
        }
    }
    return {};
}

bool ScreenToCoverImagePoint(float screenX, float screenY, const glm::vec2& imageSize, glm::vec2& imagePoint) {
    if (imageSize.x <= 0.0F || imageSize.y <= 0.0F) return false;

    const float screenW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float screenH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const float scale = std::max(screenW / imageSize.x, screenH / imageSize.y);
    const float drawW = imageSize.x * scale;
    const float drawH = imageSize.y * scale;
    const float x = (screenW - drawW) * 0.5F;
    const float y = (screenH - drawH) * 0.5F;

    imagePoint = {(screenX - x) / scale, (screenY - y) / scale};
    return imagePoint.x >= 0.0F && imagePoint.x <= imageSize.x &&
           imagePoint.y >= 0.0F && imagePoint.y <= imageSize.y;
}

float Cross(const glm::vec2& a, const glm::vec2& b, const glm::vec2& p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

bool PointInTriangle(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    const float c1 = Cross(a, b, p);
    const float c2 = Cross(b, c, p);
    const float c3 = Cross(c, a, p);
    const bool hasNeg = c1 < -0.001F || c2 < -0.001F || c3 < -0.001F;
    const bool hasPos = c1 > 0.001F || c2 > 0.001F || c3 > 0.001F;
    return !(hasNeg && hasPos);
}

bool PointInQuad(const glm::vec2& p, const std::array<glm::vec2, 4>& q) {
    return PointInTriangle(p, q[0], q[1], q[2]) || PointInTriangle(p, q[0], q[2], q[3]);
}

bool PointInExpandedQuadBounds(const glm::vec2& p, const std::array<glm::vec2, 4>& q, float padding) {
    float minX = q[0].x;
    float maxX = q[0].x;
    float minY = q[0].y;
    float maxY = q[0].y;
    for (const auto& point : q) {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }
    return p.x >= minX - padding && p.x <= maxX + padding &&
           p.y >= minY - padding && p.y <= maxY + padding;
}

bool QuadCoversImage(const std::array<glm::vec2, 4>& q, const glm::vec2& imageSize) {
    if (imageSize.x <= 0.0F || imageSize.y <= 0.0F) return false;

    const float toleranceX = std::max(2.0F, imageSize.x * 0.01F);
    const float toleranceY = std::max(2.0F, imageSize.y * 0.01F);
    float minX = q[0].x;
    float maxX = q[0].x;
    float minY = q[0].y;
    float maxY = q[0].y;
    for (const auto& point : q) {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }

    return minX <= toleranceX &&
           minY <= toleranceY &&
           maxX >= imageSize.x - toleranceX &&
           maxY >= imageSize.y - toleranceY;
}

std::array<glm::vec2, 4> ScaledQuad(const std::array<glm::vec2, 4>& source,
                                    const glm::vec2& sourceSize,
                                    const glm::vec2& targetSize) {
    std::array<glm::vec2, 4> result{};
    const float sx = sourceSize.x > 0.0F ? targetSize.x / sourceSize.x : 1.0F;
    const float sy = sourceSize.y > 0.0F ? targetSize.y / sourceSize.y : 1.0F;
    for (std::size_t i = 0; i < source.size(); ++i) {
        result[i] = {source[i].x * sx, source[i].y * sy};
    }
    return result;
}

std::array<glm::vec2, 4> FullQuadForImage(const glm::vec2& imageSize) {
    return {
        glm::vec2{0.0F, 0.0F},
        glm::vec2{imageSize.x, 0.0F},
        glm::vec2{imageSize.x, imageSize.y},
        glm::vec2{0.0F, imageSize.y},
    };
}

std::array<glm::vec2, 4> AwakenButtonQuadForImage(const glm::vec2& imageSize) {
    constexpr std::array<glm::vec2, 4> sourceQuad{
        glm::vec2{1016.0F, 713.0F},
        glm::vec2{1329.0F, 713.0F},
        glm::vec2{1329.0F, 819.0F},
        glm::vec2{1016.0F, 819.0F},
    };
    return ScaledQuad(sourceQuad, VIDEO_2_REFERENCE_SIZE, imageSize);
}
} // namespace

namespace Ark {
class OpeningVideoPlayer {
public:
    OpeningVideoPlayer() = default;
    OpeningVideoPlayer(const OpeningVideoPlayer&) = delete;
    OpeningVideoPlayer& operator=(const OpeningVideoPlayer&) = delete;

    ~OpeningVideoPlayer() {
        Close();
    }

    bool Open(const std::string& path) {
        Close();
        if (path.empty() || !std::filesystem::exists(path)) return false;

        AddCommonFfmpegPathsOnWindows();

        VideoInfo info;
        if (!ProbeVideoInfo(path, info)) return false;

        const int maxDimension = OpeningVideoMaxDimension();
        const int maxSourceDim = std::max(info.width, info.height);
        const double scale = maxSourceDim > maxDimension
            ? static_cast<double>(maxDimension) / static_cast<double>(maxSourceDim)
            : 1.0;
        m_FrameWidth = std::max(1, static_cast<int>(std::round(static_cast<double>(info.width) * scale)));
        m_FrameHeight = std::max(1, static_cast<int>(std::round(static_cast<double>(info.height) * scale)));
        m_FrameIntervalMs = 1000.0 / std::max(1.0, info.fps);
        m_DurationMs = static_cast<float>(info.durationMs);
        m_FrameBuffer.assign(static_cast<std::size_t>(m_FrameWidth) *
                                 static_cast<std::size_t>(m_FrameHeight) * 4U,
                             0);

        const std::string filter =
            "scale=" + std::to_string(m_FrameWidth) + ":" + std::to_string(m_FrameHeight) +
            ":flags=fast_bilinear,format=rgba";
        const std::string command =
            "ffmpeg -nostdin -hide_banner -loglevel quiet -i " + ShellQuote(path) +
            " -an -vf " + ShellQuote(filter) + " -f rawvideo -pix_fmt rgba -";

        m_Pipe = popen(command.c_str(), "rb");
        if (m_Pipe == nullptr) {
            Close();
            return false;
        }

        m_Path = path;
        m_ElapsedMs = 0.0F;
        m_FrameTimerMs = 0.0F;
        m_Playing = true;
        m_Ended = false;
        if (!ReadNextFrame()) {
            Close();
            return false;
        }
        return true;
    }

    void Close() {
        if (m_Pipe != nullptr) {
            pclose(m_Pipe);
            m_Pipe = nullptr;
        }
        m_Image.reset();
        m_FrameBuffer.clear();
        m_Path.clear();
        m_Playing = false;
        m_Ended = false;
        m_ElapsedMs = 0.0F;
        m_FrameTimerMs = 0.0F;
        m_DurationMs = 0.0F;
        m_FrameWidth = 0;
        m_FrameHeight = 0;
    }

    void Play() {
        if (!m_Ended) m_Playing = true;
    }

    void Pause() {
        m_Playing = false;
    }

    void Update(float deltaTimeMs) {
        if (!m_Playing || m_Ended || m_Pipe == nullptr) return;

        const float dt = std::max(0.0F, deltaTimeMs);
        m_ElapsedMs += dt;
        m_FrameTimerMs += dt;

        int framesToRead = 0;
        while (m_FrameTimerMs >= static_cast<float>(m_FrameIntervalMs) && framesToRead < 8) {
            m_FrameTimerMs -= static_cast<float>(m_FrameIntervalMs);
            ++framesToRead;
        }

        for (int i = 0; i < framesToRead; ++i) {
            if (!ReadNextFrame()) {
                m_Ended = true;
                m_Playing = false;
                return;
            }
        }

        if (m_DurationMs > 0.0F && m_ElapsedMs >= m_DurationMs) {
            m_Ended = true;
            m_Playing = false;
        }
    }

    [[nodiscard]] const std::shared_ptr<Util::Image>& GetImage() const {
        return m_Image;
    }

    [[nodiscard]] float GetElapsedMs() const {
        return m_ElapsedMs;
    }

    [[nodiscard]] bool IsEnded() const {
        return m_Ended;
    }

private:
    bool ReadNextFrame() {
        if (m_Pipe == nullptr || m_FrameBuffer.empty()) return false;

        const std::size_t frameSize = m_FrameBuffer.size();
        std::size_t totalRead = 0;
        while (totalRead < frameSize) {
            const std::size_t readNow = std::fread(m_FrameBuffer.data() + totalRead, 1,
                                                   frameSize - totalRead, m_Pipe);
            if (readNow == 0) break;
            totalRead += readNow;
        }
        if (totalRead != frameSize) return false;

        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
            m_FrameBuffer.data(),
            m_FrameWidth,
            m_FrameHeight,
            32,
            m_FrameWidth * 4,
            SDL_PIXELFORMAT_ABGR8888);
        if (surface == nullptr) return false;

        if (m_Image == nullptr) {
            m_Image = std::make_shared<Util::Image>(surface, true);
        } else {
            m_Image->SetSurface(surface);
        }
        SDL_FreeSurface(surface);
        return true;
    }

    FILE* m_Pipe = nullptr;
    std::string m_Path;
    std::vector<unsigned char> m_FrameBuffer;
    std::shared_ptr<Util::Image> m_Image;
    double m_FrameIntervalMs = 20.0;
    float m_FrameTimerMs = 0.0F;
    float m_ElapsedMs = 0.0F;
    float m_DurationMs = 0.0F;
    int m_FrameWidth = 0;
    int m_FrameHeight = 0;
    bool m_Playing = false;
    bool m_Ended = false;
};
} // namespace Ark

App::App() = default;
App::~App() = default;

void App::Start() {
    if (!m_Renderer) {
        m_Renderer = std::make_shared<Ark::AppRenderer>(*this);
    }

    BeginOpeningSequence();
    m_CurrentState = State::OPENING;
}

void App::BeginOpeningSequence() {
    m_OpeningVideo1Path = ResolveLoadingPageAsset({"loadingpage_1.mp4", "loginpage_1.mp4"});
    m_OpeningVideo2Path = ResolveLoadingPageAsset({"loadingpage_2.mp4", "loginpage_2.mp4"});
    m_OpeningMenuImagePath = ResolveLoadingPageAsset({"loadingpage_3.png"});
    m_OpeningMenuImage.reset();
    m_OpeningMenuButtons = {};
    m_OpeningVideo1Action = {};
    m_OpeningVideo2Action = {};
    m_OpeningFadeTimerMs = 0.0F;
    m_OpeningMouseWasDown = false;
    m_OpeningVideo2Awakened = false;
    m_OpeningPhase = OpeningPhase::VIDEO_1;
    m_OpeningVideo = std::make_unique<Ark::OpeningVideoPlayer>();

    if (!m_OpeningVideo->Open(m_OpeningVideo1Path)) {
        m_OpeningVideo.reset();
        LoadOpeningMenuImage();
        m_OpeningPhase = OpeningPhase::MENU_FADE_IN;
    }
    LoadOpeningVideoAction();
}

void App::BeginOpeningMenu() {
    m_OpeningVideo.reset();
    m_OpeningMenuImagePath = ResolveLoadingPageAsset({"loadingpage_3.png"});
    m_OpeningMenuImage.reset();
    m_OpeningMenuButtons = {};
    m_OpeningFadeTimerMs = 0.0F;
    m_OpeningMouseWasDown = false;
    m_OpeningVideo2Awakened = true;
    m_MissionClear = false;
    m_GameOver = false;
    m_FinishExitRequested = false;
    m_FinishExitTimerMs = 0.0F;
    m_ClearTimerMs = 0.0F;
    LoadOpeningMenuImage();
    m_OpeningPhase = OpeningPhase::MENU_FADE_IN;
    m_CurrentState = State::OPENING;
}

void App::BeginStageLoading(const std::string& stageFile) {
    m_OpeningVideo.reset();
    m_OpeningMenuImage.reset();
    m_CurrentStageFile = stageFile.empty() ? STAGE_1_1_FILE : stageFile;

    // Phase 1: Lightweight stage JSON loading (fast -- sets m_StageLoadingPath so
    // the loading screen image is available for rendering immediately).
    m_OperatorTemplates = Ark::LoadOperatorsWithFallback();
    if (!LoadStageFromJsonModule()) BuildFallbackStage();
    ResetDemo();

    m_ModelVanguard = std::make_shared<Util::Animation>(
        std::vector<std::string>{
            ASSETS_DIR "/sprites/cat/cat-0.bmp",
            ASSETS_DIR "/sprites/cat/cat-1.bmp",
            ASSETS_DIR "/sprites/cat/cat-2.bmp",
            ASSETS_DIR "/sprites/cat/cat-3.bmp",
            ASSETS_DIR "/sprites/cat/cat-4.bmp",
            ASSETS_DIR "/sprites/cat/cat-5.bmp",
            ASSETS_DIR "/sprites/cat/cat-6.bmp",
            ASSETS_DIR "/sprites/cat/cat-7.bmp",
        }, true, 100, true, 0);
    m_ModelGuard = std::make_shared<Util::Image>(ASSETS_DIR "/sprites/giraffe.png");
    m_ModelEnemy = std::make_shared<Util::Image>(ASSETS_DIR "/sprites/giraffe.png");

    // Transition to LOADING state — the loading screen will be rendered on the
    // next frame, then heavy work (animation decoding) happens afterwards.
    m_LoadingPhase = 0;
    m_LoadingTimerMs = 0.0F;
    m_CurrentState = State::LOADING;
}

void App::LoadOpeningMenuImage() {
    if (m_OpeningMenuImage == nullptr && !m_OpeningMenuImagePath.empty()) {
        m_OpeningMenuImage = std::make_shared<Util::Image>(m_OpeningMenuImagePath);
    }
    if (!m_OpeningMenuButtons[0].enabled && !m_OpeningMenuButtons[1].enabled) {
        LoadOpeningMenuButtons();
    }
}

void App::LoadOpeningMenuButtons() {
    const glm::vec2 imageSize = m_OpeningMenuImage != nullptr
        ? m_OpeningMenuImage->GetSize()
        : MENU_REFERENCE_SIZE;

    constexpr std::array<glm::vec2, 4> defaultStage11{
        glm::vec2{955.0F, 555.0F},
        glm::vec2{1325.0F, 555.0F},
        glm::vec2{1325.0F, 690.0F},
        glm::vec2{955.0F, 690.0F},
    };
    constexpr std::array<glm::vec2, 4> defaultStage12{
        glm::vec2{1430.0F, 540.0F},
        glm::vec2{1735.0F, 540.0F},
        glm::vec2{1735.0F, 675.0F},
        glm::vec2{1430.0F, 675.0F},
    };

    m_OpeningMenuButtons = {
        OpeningMenuButton{
            "stage_1_1",
            "Operation 1-1",
            STAGE_1_1_FILE,
            ScaledQuad(defaultStage11, MENU_REFERENCE_SIZE, imageSize),
            true
        },
        OpeningMenuButton{
            "stage_1_2",
            "Operation 1-2",
            STAGE_1_2_FILE,
            ScaledQuad(defaultStage12, MENU_REFERENCE_SIZE, imageSize),
            true
        },
    };

    const std::string configPath = ResolveLoadingPageAsset({"menu_buttons.json"});
    if (configPath.empty() || !std::filesystem::exists(configPath)) return;

    try {
        std::ifstream file(configPath);
        nlohmann::json doc;
        file >> doc;
        if (!doc.is_object()) return;

        glm::vec2 referenceSize = imageSize;
        if (doc.contains("reference_size") && doc["reference_size"].is_array() &&
            doc["reference_size"].size() == 2 &&
            doc["reference_size"][0].is_number() &&
            doc["reference_size"][1].is_number()) {
            referenceSize = {
                doc["reference_size"][0].get<float>(),
                doc["reference_size"][1].get<float>()
            };
        }
        if (referenceSize.x <= 0.0F || referenceSize.y <= 0.0F) {
            referenceSize = imageSize;
        }

        auto readPoint = [](const nlohmann::json& owner, const char* key) -> std::optional<glm::vec2> {
            if (!owner.contains(key) || !owner[key].is_array() || owner[key].size() != 2) return std::nullopt;
            if (!owner[key][0].is_number() || !owner[key][1].is_number()) return std::nullopt;
            return glm::vec2{owner[key][0].get<float>(), owner[key][1].get<float>()};
        };

        if (!doc.contains("buttons") || !doc["buttons"].is_array()) return;
        const auto& buttons = doc["buttons"];
        for (std::size_t i = 0; i < buttons.size() && i < m_OpeningMenuButtons.size(); ++i) {
            const auto& src = buttons[i];
            if (!src.is_object()) continue;
            const auto topLeft = readPoint(src, "top_left");
            const auto topRight = readPoint(src, "top_right");
            const auto bottomRight = readPoint(src, "bottom_right");
            const auto bottomLeft = readPoint(src, "bottom_left");
            if (!topLeft || !topRight || !bottomRight || !bottomLeft) continue;

            auto& dst = m_OpeningMenuButtons[i];
            dst.id = src.value("id", dst.id);
            dst.label = src.value("label", dst.label);
            dst.stageFile = src.value("stage", dst.stageFile);
            const std::array<glm::vec2, 4> raw{*topLeft, *topRight, *bottomRight, *bottomLeft};
            dst.corners = ScaledQuad(raw, referenceSize, imageSize);
            dst.enabled = true;
        }
    } catch (const std::exception&) {
        // Keep defaults if the calibration file is temporarily invalid.
    }
}

void App::LoadOpeningVideoAction() {
    const glm::vec2 imageSize = m_OpeningVideo != nullptr && m_OpeningVideo->GetImage() != nullptr
        ? m_OpeningVideo->GetImage()->GetSize()
        : VIDEO_1_REFERENCE_SIZE;

    m_OpeningVideo1Action = OpeningVideoAction{
        FullQuadForImage(imageSize),
        OPENING_VIDEO_1_DEFAULT_START_MS,
        OPENING_VIDEO_1_DEFAULT_END_MS,
        true
    };

    const std::string configPath = ResolveLoadingPageAsset({"menu_buttons.json"});
    if (configPath.empty() || !std::filesystem::exists(configPath)) return;

    try {
        std::ifstream file(configPath);
        nlohmann::json doc;
        file >> doc;
        if (!doc.is_object() || !doc.contains("video_1_action") ||
            !doc["video_1_action"].is_object()) {
            return;
        }

        const auto& action = doc["video_1_action"];
        m_OpeningVideo1Action.enabled = action.value("enabled", m_OpeningVideo1Action.enabled);

        auto readMs = [&](const char* msKey, const char* secKey, float fallback) {
            if (action.contains(msKey) && action[msKey].is_number()) {
                return action[msKey].get<float>();
            }
            if (action.contains(secKey) && action[secKey].is_number()) {
                return action[secKey].get<float>() * 1000.0F;
            }
            return fallback;
        };

        m_OpeningVideo1Action.startMs = std::max(0.0F, readMs("start_ms", "start_sec",
                                                              m_OpeningVideo1Action.startMs));
        m_OpeningVideo1Action.endMs = std::max(m_OpeningVideo1Action.startMs,
                                               readMs("end_ms", "end_sec",
                                                      m_OpeningVideo1Action.endMs));

        glm::vec2 referenceSize = imageSize;
        if (action.contains("reference_size") && action["reference_size"].is_array() &&
            action["reference_size"].size() == 2 &&
            action["reference_size"][0].is_number() &&
            action["reference_size"][1].is_number()) {
            referenceSize = {
                action["reference_size"][0].get<float>(),
                action["reference_size"][1].get<float>()
            };
        }
        if (referenceSize.x <= 0.0F || referenceSize.y <= 0.0F) {
            referenceSize = imageSize;
        }

        auto readPoint = [](const nlohmann::json& owner, const char* key) -> std::optional<glm::vec2> {
            if (!owner.contains(key) || !owner[key].is_array() || owner[key].size() != 2) return std::nullopt;
            if (!owner[key][0].is_number() || !owner[key][1].is_number()) return std::nullopt;
            return glm::vec2{owner[key][0].get<float>(), owner[key][1].get<float>()};
        };

        const auto topLeft = readPoint(action, "top_left");
        const auto topRight = readPoint(action, "top_right");
        const auto bottomRight = readPoint(action, "bottom_right");
        const auto bottomLeft = readPoint(action, "bottom_left");
        if (!topLeft || !topRight || !bottomRight || !bottomLeft) return;

        const std::array<glm::vec2, 4> raw{*topLeft, *topRight, *bottomRight, *bottomLeft};
        m_OpeningVideo1Action.corners = ScaledQuad(raw, referenceSize, imageSize);
    } catch (const std::exception&) {
        // Keep the default full-video action if the calibration file is invalid.
    }
}

void App::LoadOpeningVideo2Action() {
    const glm::vec2 imageSize = m_OpeningVideo != nullptr && m_OpeningVideo->GetImage() != nullptr
        ? m_OpeningVideo->GetImage()->GetSize()
        : VIDEO_2_REFERENCE_SIZE;

    m_OpeningVideo2Action = OpeningVideoAction{
        AwakenButtonQuadForImage(imageSize),
        OPENING_VIDEO_2_DEFAULT_START_MS,
        OPENING_VIDEO_1_DEFAULT_END_MS,
        true
    };

    const std::string configPath = ResolveLoadingPageAsset({"menu_buttons.json"});
    if (configPath.empty() || !std::filesystem::exists(configPath)) return;

    try {
        std::ifstream file(configPath);
        nlohmann::json doc;
        file >> doc;
        if (!doc.is_object() || !doc.contains("video_2_awaken") ||
            !doc["video_2_awaken"].is_object()) {
            return;
        }

        const auto& action = doc["video_2_awaken"];
        m_OpeningVideo2Action.enabled = action.value("enabled", m_OpeningVideo2Action.enabled);

        auto readMs = [&](const char* msKey, const char* secKey, float fallback) {
            if (action.contains(msKey) && action[msKey].is_number()) {
                return action[msKey].get<float>();
            }
            if (action.contains(secKey) && action[secKey].is_number()) {
                return action[secKey].get<float>() * 1000.0F;
            }
            return fallback;
        };

        m_OpeningVideo2Action.startMs = std::max(0.0F, readMs("start_ms", "start_sec",
                                                              m_OpeningVideo2Action.startMs));
        m_OpeningVideo2Action.endMs = std::max(m_OpeningVideo2Action.startMs,
                                               readMs("end_ms", "end_sec",
                                                      m_OpeningVideo2Action.endMs));

        glm::vec2 referenceSize = imageSize;
        if (action.contains("reference_size") && action["reference_size"].is_array() &&
            action["reference_size"].size() == 2 &&
            action["reference_size"][0].is_number() &&
            action["reference_size"][1].is_number()) {
            referenceSize = {
                action["reference_size"][0].get<float>(),
                action["reference_size"][1].get<float>()
            };
        }
        if (referenceSize.x <= 0.0F || referenceSize.y <= 0.0F) {
            referenceSize = imageSize;
        }

        auto readPoint = [](const nlohmann::json& owner, const char* key) -> std::optional<glm::vec2> {
            if (!owner.contains(key) || !owner[key].is_array() || owner[key].size() != 2) return std::nullopt;
            if (!owner[key][0].is_number() || !owner[key][1].is_number()) return std::nullopt;
            return glm::vec2{owner[key][0].get<float>(), owner[key][1].get<float>()};
        };

        const auto topLeft = readPoint(action, "top_left");
        const auto topRight = readPoint(action, "top_right");
        const auto bottomRight = readPoint(action, "bottom_right");
        const auto bottomLeft = readPoint(action, "bottom_left");
        if (!topLeft || !topRight || !bottomRight || !bottomLeft) return;

        const std::array<glm::vec2, 4> raw{*topLeft, *topRight, *bottomRight, *bottomLeft};
        m_OpeningVideo2Action.corners = ScaledQuad(raw, referenceSize, imageSize);
    } catch (const std::exception&) {
        // Keep the default awaken button if the calibration file is invalid.
    }
}

bool App::IsOpeningVideo1ActionClicked(float screenCursorX, float screenCursorY) const {
    if (!m_OpeningVideo1Action.enabled ||
        m_OpeningVideo == nullptr ||
        m_OpeningVideo->GetImage() == nullptr) {
        return false;
    }

    const float elapsedMs = m_OpeningVideo->GetElapsedMs();
    if (elapsedMs < m_OpeningVideo1Action.startMs || elapsedMs > m_OpeningVideo1Action.endMs) {
        return false;
    }

    const glm::vec2 imageSize = m_OpeningVideo->GetImage()->GetSize();
    if (QuadCoversImage(m_OpeningVideo1Action.corners, imageSize)) {
        return true;
    }

    glm::vec2 imagePoint{};
    if (!ScreenToCoverImagePoint(screenCursorX, screenCursorY,
                                 imageSize, imagePoint)) {
        return false;
    }
    const float padding = std::max(12.0F, std::min(imageSize.x, imageSize.y) * 0.035F);
    return PointInQuad(imagePoint, m_OpeningVideo1Action.corners) ||
           PointInExpandedQuadBounds(imagePoint, m_OpeningVideo1Action.corners, padding);
}

bool App::IsOpeningAwakenButtonClicked(float screenCursorX, float screenCursorY) const {
    if (!m_OpeningVideo2Action.enabled ||
        m_OpeningVideo == nullptr ||
        m_OpeningVideo->GetImage() == nullptr) {
        return false;
    }
    const glm::vec2 imageSize = m_OpeningVideo->GetImage()->GetSize();
    const float elapsedMs = m_OpeningVideo->GetElapsedMs();
    if (elapsedMs < m_OpeningVideo2Action.startMs || elapsedMs > m_OpeningVideo2Action.endMs) {
        return false;
    }

    glm::vec2 imagePoint{};
    if (!ScreenToCoverImagePoint(screenCursorX, screenCursorY, imageSize, imagePoint)) {
        return false;
    }
    if (QuadCoversImage(m_OpeningVideo2Action.corners, imageSize)) {
        return true;
    }
    const float padding = std::max(12.0F, std::min(imageSize.x, imageSize.y) * 0.035F);
    return PointInQuad(imagePoint, m_OpeningVideo2Action.corners) ||
           PointInExpandedQuadBounds(imagePoint, m_OpeningVideo2Action.corners, padding);
}

bool App::HandleOpeningMenuClick(float screenCursorX, float screenCursorY) {
    if (m_OpeningMenuImage == nullptr) return false;

    glm::vec2 imagePoint{};
    if (!ScreenToCoverImagePoint(screenCursorX, screenCursorY, m_OpeningMenuImage->GetSize(), imagePoint)) {
        return false;
    }

    for (const auto& button : m_OpeningMenuButtons) {
        if (!button.enabled) continue;
        if (PointInQuad(imagePoint, button.corners)) {
            BeginStageLoading(button.stageFile);
            return true;
        }
    }
    return false;
}

void App::Opening() {
    if (Util::Input::IfExit() || Util::Input::IsKeyUp(Util::Keycode::ESCAPE)) {
        m_CurrentState = State::END;
        return;
    }
    if (!m_Renderer) {
        m_Renderer = std::make_shared<Ark::AppRenderer>(*this);
    }

    const float dt = std::clamp(Util::Time::GetDeltaTimeMs(), 0.0F, 100.0F);
    int mouseX = 0;
    int mouseY = 0;
    const Uint32 mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);
    const bool leftDown = (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    const bool clicked = leftDown && !m_OpeningMouseWasDown;
    m_OpeningMouseWasDown = leftDown;
    const float screenCursorX = static_cast<float>(mouseX);
    const float screenCursorY = static_cast<float>(mouseY);

    auto drawVideo = [&](float alpha) {
        if (m_Renderer && m_OpeningVideo && m_OpeningVideo->GetImage()) {
            m_Renderer->DrawImageCover(m_OpeningVideo->GetImage(), alpha, true);
        }
    };
    auto drawMenu = [&](float alpha) {
        LoadOpeningMenuImage();
        if (m_Renderer && m_OpeningMenuImage) {
            m_Renderer->DrawImageCover(m_OpeningMenuImage, alpha, true);
        }
    };
    auto enterMenu = [&]() {
        m_OpeningVideo.reset();
        LoadOpeningMenuImage();
        m_OpeningFadeTimerMs = 0.0F;
        m_OpeningPhase = OpeningPhase::MENU_FADE_IN;
    };

    switch (m_OpeningPhase) {
    case OpeningPhase::VIDEO_1:
        if (m_OpeningVideo == nullptr) {
            enterMenu();
            drawMenu(0.0F);
            return;
        }
        m_OpeningVideo->Update(dt);
        drawVideo(1.0F);
        if (clicked && IsOpeningVideo1ActionClicked(screenCursorX, screenCursorY)) {
            m_OpeningFadeTimerMs = 0.0F;
            m_OpeningPhase = OpeningPhase::VIDEO_1_FADE_OUT;
        }
        return;

    case OpeningPhase::VIDEO_1_FADE_OUT:
        if (m_OpeningVideo) m_OpeningVideo->Update(dt);
        m_OpeningFadeTimerMs += dt;
        drawVideo(1.0F - (m_OpeningFadeTimerMs / OPENING_FADE_MS));
        if (m_OpeningFadeTimerMs >= OPENING_FADE_MS) {
            m_OpeningVideo = std::make_unique<Ark::OpeningVideoPlayer>();
            if (!m_OpeningVideo->Open(m_OpeningVideo2Path)) {
                enterMenu();
                return;
            }
            LoadOpeningVideo2Action();
            m_OpeningVideo2Awakened = false;
            m_OpeningFadeTimerMs = 0.0F;
            m_OpeningPhase = OpeningPhase::VIDEO_2_FADE_IN;
        }
        return;

    case OpeningPhase::VIDEO_2_FADE_IN:
        if (m_OpeningVideo == nullptr) {
            enterMenu();
            drawMenu(0.0F);
            return;
        }
        m_OpeningVideo->Update(dt);
        m_OpeningFadeTimerMs += dt;
        drawVideo(std::clamp(m_OpeningFadeTimerMs / OPENING_FADE_MS, 0.0F, 1.0F));
        if (m_OpeningFadeTimerMs >= OPENING_FADE_MS) {
            m_OpeningPhase = OpeningPhase::VIDEO_2;
        }
        return;

    case OpeningPhase::VIDEO_2:
        if (m_OpeningVideo == nullptr) {
            enterMenu();
            drawMenu(0.0F);
            return;
        }
        if (m_OpeningVideo->IsEnded()) {
            enterMenu();
            return;
        }

        if (!m_OpeningVideo2Action.enabled || m_OpeningVideo2Awakened) {
            m_OpeningVideo->Update(dt);
            drawVideo(1.0F);
            if (m_OpeningVideo->IsEnded()) {
                enterMenu();
            }
            return;
        }

        if (m_OpeningVideo->GetElapsedMs() < OPENING_VIDEO_2_PAUSE_MS) {
            const float remainingMs = OPENING_VIDEO_2_PAUSE_MS - m_OpeningVideo->GetElapsedMs();
            m_OpeningVideo->Update(std::min(dt, remainingMs));
        }
        drawVideo(1.0F);

        if (clicked && IsOpeningAwakenButtonClicked(screenCursorX, screenCursorY)) {
            m_OpeningVideo2Awakened = true;
            if (m_OpeningVideo) m_OpeningVideo->Play();
            m_OpeningPhase = OpeningPhase::VIDEO_2_RESUME;
            return;
        }

        if (m_OpeningVideo->GetElapsedMs() >= OPENING_VIDEO_2_PAUSE_MS) {
            m_OpeningVideo->Pause();
            m_OpeningPhase = OpeningPhase::VIDEO_2_PAUSED;
        }
        return;

    case OpeningPhase::VIDEO_2_PAUSED:
        drawVideo(1.0F);
        if (clicked && IsOpeningAwakenButtonClicked(screenCursorX, screenCursorY)) {
            m_OpeningVideo2Awakened = true;
            if (m_OpeningVideo) m_OpeningVideo->Play();
            m_OpeningPhase = OpeningPhase::VIDEO_2_RESUME;
        }
        return;

    case OpeningPhase::VIDEO_2_RESUME:
        if (m_OpeningVideo == nullptr) {
            enterMenu();
            drawMenu(0.0F);
            return;
        }
        m_OpeningVideo->Update(dt);
        drawVideo(1.0F);
        if (m_OpeningVideo->IsEnded()) {
            enterMenu();
        }
        return;

    case OpeningPhase::MENU_FADE_IN:
        m_OpeningFadeTimerMs += dt;
        drawMenu(std::clamp(m_OpeningFadeTimerMs / OPENING_MENU_FADE_MS, 0.0F, 1.0F));
        if (m_OpeningFadeTimerMs >= OPENING_MENU_FADE_MS) {
            m_OpeningPhase = OpeningPhase::MENU;
        }
        return;

    case OpeningPhase::MENU:
        drawMenu(1.0F);
        if (clicked) {
            HandleOpeningMenuClick(screenCursorX, screenCursorY);
        }
        return;
    }
}

void App::Loading() {
    if (Util::Input::IfExit() || Util::Input::IsKeyUp(Util::Keycode::ESCAPE)) {
        m_CurrentState = State::END;
        return;
    }

    const float dt = std::clamp(Util::Time::GetDeltaTimeMs(), 0.0F, 100.0F);

    auto drawLoading = [&](float alpha) {
        if (m_Renderer && !m_StageLoadingPath.empty()) {
            m_Renderer->DrawImageCover(m_StageLoadingPath, m_StageLoadingAlpha * std::clamp(alpha, 0.0F, 1.0F), true);
        }
    };

    // Phase 0: fade in the loading art before heavy initialization starts.
    if (m_LoadingPhase == 0) {
        m_LoadingTimerMs += dt;
        drawLoading(m_LoadingTimerMs / LOADING_FADE_IN_MS);
        if (m_LoadingTimerMs >= LOADING_FADE_IN_MS) {
            m_LoadingPhase = 1;
            m_LoadingTimerMs = 0.0F;
        }
        return;
    }

    // Phase 1: do heavy initialization while the loading art stays fully visible.
    if (m_LoadingPhase == 1) {
        drawLoading(1.0F);
        auto warmStaticImage = [&](const std::string& imagePath) {
            if (imagePath.empty()) return;
            const auto key = std::filesystem::path(imagePath).lexically_normal().string();
            if (m_StaticImageCache.find(key) != m_StaticImageCache.end()) return;
            m_StaticImageCache.emplace(key, std::make_shared<Util::Image>(key));
        };
        warmStaticImage(m_StageBackgroundPath);
        warmStaticImage(m_StageFinishPath);
        LoadOperatorAnimations();
        LoadEnemyAnimations();
        if (m_Renderer) {
            m_Renderer->LoadOperatorThumbnails();
        }
        m_LoadingPhase = 2;
        m_LoadingTimerMs = 0.0F;
        return;
    }

    // Phase 2: hold briefly, then fade out before entering gameplay.
    if (m_LoadingPhase == 2) {
        m_LoadingTimerMs += dt;
        if (m_LoadingTimerMs < LOADING_HOLD_MS) {
            drawLoading(1.0F);
            return;
        }

        const float fadeT = (m_LoadingTimerMs - LOADING_HOLD_MS) / LOADING_FADE_OUT_MS;
        drawLoading(1.0F - fadeT);
        if (fadeT < 1.0F) return;
    }

    // Done ??transition to game. Since the loading screen fades here, skip the
    // pre-stage wait.
    m_PreStageWaiting = false;
    m_PreStageTimerMs = 0.0F;
    if (!m_WaveRunning && !m_GameOver && !m_MissionClear) {
        StartWave();
    }
    m_CurrentState = State::UPDATE;
}
#if 0
void App::Loading() {
    if (Util::Input::IfExit() || Util::Input::IsKeyUp(Util::Keycode::ESCAPE)) {
        m_CurrentState = State::END;
        return;
    }

    const float dt = std::clamp(Util::Time::GetDeltaTimeMs(), 0.0F, 100.0F);

    auto drawLoading = [&](float alpha) {
        if (m_Renderer && !m_StageLoadingPath.empty()) {
            m_Renderer->DrawImageCover(m_StageLoadingPath, m_StageLoadingAlpha * std::clamp(alpha, 0.0F, 1.0F), true);
        }
    };

    // Phase 0: fade in the loading art before heavy initialization starts.
    if (m_LoadingPhase == 0) {
        m_LoadingTimerMs += dt;
        drawLoading(m_LoadingTimerMs / LOADING_FADE_IN_MS);
        if (m_LoadingTimerMs >= LOADING_FADE_IN_MS) {
            m_LoadingPhase = 1;
            m_LoadingTimerMs = 0.0F;
        }
        return;
    }

    // Phase 1: do heavy initialization while the loading art stays fully visible.
    if (m_LoadingPhase == 1) {
        drawLoading(1.0F);
        auto warmStaticImage = [&](const std::string& imagePath) {
            if (imagePath.empty()) return;
            const auto key = std::filesystem::path(imagePath).lexically_normal().string();
            if (m_StaticImageCache.find(key) != m_StaticImageCache.end()) return;
            m_StaticImageCache.emplace(key, std::make_shared<Util::Image>(key));
        };
        warmStaticImage(m_StageBackgroundPath);
        warmStaticImage(m_StageFinishPath);
        LoadOperatorAnimations();
        LoadEnemyAnimations();
        if (m_Renderer) {
            m_Renderer->LoadOperatorThumbnails();
        }
        m_LoadingPhase = 2;
        m_LoadingTimerMs = 0.0F;
        return;
    }

    // Phase 2: hold briefly, then fade out before entering gameplay.
    if (m_LoadingPhase == 2) {
        m_LoadingTimerMs += dt;
        if (m_LoadingTimerMs < LOADING_HOLD_MS) {
            drawLoading(1.0F);
            return;
        }

        const float fadeT = (m_LoadingTimerMs - LOADING_HOLD_MS) / LOADING_FADE_OUT_MS;
        drawLoading(1.0F - fadeT);
        if (fadeT < 1.0F) return;
    }

    // Done — transition to game. Since the loading screen fades here, skip the
    // pre-stage wait.
    m_PreStageWaiting = false;
    m_PreStageTimerMs = 0.0F;
    if (!m_WaveRunning && !m_GameOver && !m_MissionClear) {
        StartWave();
    }
    m_CurrentState = State::UPDATE;
}
#endif

void App::Update() {
    if (Util::Input::IfExit() || Util::Input::IsKeyUp(Util::Keycode::ESCAPE)) {
        m_CurrentState = State::END;
        return;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::R)) ResetDemo();
    if (Util::Input::IsKeyDown(Util::Keycode::M)) m_ShowMapModel = !m_ShowMapModel;
    if (Util::Input::IsKeyDown(Util::Keycode::Z)) m_CheatMode = !m_CheatMode;
    const float dt = std::clamp(Util::Time::GetDeltaTimeMs(), 0.0F, 100.0F);

    const auto raw = Util::Input::GetCursorPosition();
    const glm::vec2 rawCursor{raw.x, raw.y};

    // Screen-space cursor for bar hit-testing
    const float screenW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float screenH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const float screenCursorX = rawCursor.x + screenW * 0.5F;
    const float screenCursorY = screenH * 0.5F - rawCursor.y;
    const auto uiLayout = Ark::RendererConst::ComputeBattleUiLayout(screenW, screenH);

    bool uiConsumedLeftClick = false;
    if (!m_PreStageWaiting && !m_GameOver && !m_MissionClear &&
        Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
        if (m_ShowQuitConfirm) {
            uiConsumedLeftClick = true;
            if (uiLayout.quitBackButton.Contains(screenCursorX, screenCursorY)) {
                m_ShowQuitConfirm = false;
                m_GamePaused = m_PauseBeforeQuitConfirm;
            } else if (uiLayout.quitConfirmButton.Contains(screenCursorX, screenCursorY)) {
                m_CurrentState = State::END;
                return;
            }
        } else if (uiLayout.settingsButton.Contains(screenCursorX, screenCursorY)) {
            uiConsumedLeftClick = true;
            m_PauseBeforeQuitConfirm = m_GamePaused;
            m_GamePaused = true;
            m_ShowQuitConfirm = true;
        } else if (uiLayout.speedButton.Contains(screenCursorX, screenCursorY)) {
            uiConsumedLeftClick = true;
            m_GameSpeedMultiplier = (m_GameSpeedMultiplier < 1.5F) ? 2.0F : 1.0F;
        } else if (uiLayout.pauseButton.Contains(screenCursorX, screenCursorY)) {
            uiConsumedLeftClick = true;
            m_GamePaused = !m_GamePaused;
        }
    }

    if (!m_PreStageWaiting && !m_GamePaused && !m_ShowQuitConfirm) {
        UpdateCameraControls(dt, rawCursor);
    }
    const glm::vec2 ptsdCursor = RawCursorToPtsd(rawCursor);

    auto buildDisplayOps = [&]() {
        std::vector<int> displayOps;
        const int opCount = static_cast<int>(m_OperatorTemplates.size());
        for (int i = 0; i < opCount; ++i) {
            if (!IsOperatorTypeOnField(i)) displayOps.push_back(i);
        }
        std::stable_sort(displayOps.begin(), displayOps.end(),
                         [this](int lhs, int rhs) {
                             const auto& a = m_OperatorTemplates.at(static_cast<std::size_t>(lhs));
                             const auto& b = m_OperatorTemplates.at(static_cast<std::size_t>(rhs));
                             return a.cost < b.cost;
                         });
        return displayOps;
    };

    auto findOperatorCardAt = [&](float x, float y) {
        const float barY = screenH - OP_BAR_HEIGHT;
        if (y < barY - 28.0F || y > barY + OP_CARD_HEIGHT || !m_WaveRunning) return -1;

        const auto displayOps = buildDisplayOps();
        const int dispCount = static_cast<int>(displayOps.size());
        if (dispCount <= 0) return -1;

        const float totalW = dispCount * OP_CARD_WIDTH + (dispCount - 1) * OP_CARD_SPACING;
        const float startX = screenW - totalW - 24.0F;
        for (int idx = 0; idx < dispCount; ++idx) {
            const int typeIndex = displayOps[idx];
            const float cx = startX + idx * (OP_CARD_WIDTH + OP_CARD_SPACING);
            const float cy = barY - (typeIndex == m_SelectedOperatorCardType ? 22.0F : 0.0F);
            if (x >= cx && x <= cx + OP_CARD_WIDTH && y >= cy && y <= cy + OP_CARD_HEIGHT) {
                return typeIndex;
            }
        }
        return -1;
    };

    auto canDragOperatorCard = [&](int typeIndex) {
        if (typeIndex < 0 || typeIndex >= static_cast<int>(m_OperatorTemplates.size())) return false;
        if (!IsOperatorTypeAvailable(typeIndex)) return false;
        if (static_cast<int>(m_Operators.size()) >= MAX_OPS) return false;
        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(typeIndex));
        return m_DP >= static_cast<float>(opType.cost);
    };

    auto operatorInfoVisible = [&]() {
        return m_DraggingFromBar || m_WaitingForDirection ||
               m_SelectedOperatorCardType >= 0 || m_SelectedOperatorId != -1;
    };
    const float dragBlendTarget = (m_DraggingFromBar || m_WaitingForDirection) ? 1.0F : 0.0F;
    const float dragBlendStep = std::clamp(dt / 170.0F, 0.0F, 1.0F);
    m_OperatorDetailDragBlend += (dragBlendTarget - m_OperatorDetailDragBlend) * dragBlendStep;

    auto operatorInfoTabAt = [&](float x, float y) {
        if (!operatorInfoVisible()) return -1;
        const float scale = uiLayout.scale;
        const float panelW = std::clamp(screenW * 0.29F, 390.0F, 560.0F);
        const float panelH = screenH;
        const float portraitH = std::min(panelH * 0.62F, 500.0F);
        const float tabH = 44.0F * scale;
        if (x < 0.0F || x > panelW || y < portraitH || y > portraitH + tabH) return -1;
        const int tab = static_cast<int>(std::floor(x / (panelW / 3.0F)));
        return std::clamp(tab, 0, 2);
    };

    if (!m_GameOver && !m_MissionClear && !m_PreStageWaiting && !m_ShowQuitConfirm &&
        !uiConsumedLeftClick && Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB)) {
        const int clickedTab = operatorInfoTabAt(screenCursorX, screenCursorY);
        if (clickedTab >= 0) {
            m_OperatorInfoTab = clickedTab;
            uiConsumedLeftClick = true;
        } else if (m_GamePaused) {
            const int clickedCard = findOperatorCardAt(screenCursorX, screenCursorY);
            if (clickedCard >= 0) {
                m_SelectedOperatorCardType = clickedCard;
                m_DragOperatorType = clickedCard;
                m_SelectedOperatorId = -1;
                m_OperatorInfoTab = 0;
                uiConsumedLeftClick = true;
            }
        }
    }

    if (!m_GameOver && !m_MissionClear && !m_PreStageWaiting &&
        !m_GamePaused && !m_ShowQuitConfirm && !uiConsumedLeftClick) {
        if (m_OperatorCardPressActive) {
            m_DragScreenPos = {screenCursorX, screenCursorY};
            if (Util::Input::IsKeyPressed(Util::Keycode::MOUSE_LB)) {
                const glm::vec2 diff = m_DragScreenPos - m_OperatorCardPressPos;
                if (!m_DraggingFromBar && glm::length(diff) > 8.0F &&
                    canDragOperatorCard(m_PressedOperatorCardType)) {
                    m_DraggingFromBar = true;
                    m_DragOperatorType = m_PressedOperatorCardType;
                    m_SelectedOperatorCardType = m_PressedOperatorCardType;
                    m_SelectedOperatorId = -1;
                    m_OperatorCardPressActive = false;
                    m_PressedOperatorCardType = -1;
                }
            }
            if (Util::Input::IsKeyUp(Util::Keycode::MOUSE_LB)) {
                m_OperatorCardPressActive = false;
                m_PressedOperatorCardType = -1;
            }
        }

        // ── Right-click: retreat operator ────────────────────────────
        if (Util::Input::IsKeyPressed(Util::Keycode::MOUSE_RB) && !m_IsDeploying &&
            !m_DraggingFromBar && !m_WaitingForDirection) {
            const auto cell = ToCell(ptsdCursor);
            if (cell) {
                for (auto it = m_Operators.begin(); it != m_Operators.end(); ++it) {
                    if (it->cell == *cell) {
                        if (m_SelectedOperatorId == it->id) m_SelectedOperatorId = -1;
                        if (!IsValidOperatorTypeIndex(it->typeIndex)) {
                            m_Operators.erase(it);
                            break;
                        }
                        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(it->typeIndex));
                        // Retreat refund
                        if (opType.name == "Bagpipe" || opType.name == u8"風笛") {
                            m_DP = std::min(m_MaxDP, m_DP + static_cast<float>(opType.cost)); 
                        } else {
                            m_DP = std::min(m_MaxDP, m_DP + std::floor(static_cast<float>(opType.cost) / 2.0F)); 
                        }
                        // Release blocked enemies
                        for (auto& e : m_Enemies) {
                            if (e.blockedByOperatorId == it->id) e.blockedByOperatorId = -1;
                        }
                        // Cleanup animation instance
                        if (it->typeIndex >= 0 &&
                            it->typeIndex < static_cast<int>(m_OperatorAnims.size())) {
                            m_OperatorAnims[static_cast<std::size_t>(it->typeIndex)].activeInstances.erase(it->id);
                        }
                        // Start redeploy cooldown (90 seconds)
                        m_OperatorRedeployCooldownMs[it->typeIndex] = REDEPLOY_COOLDOWN_MS;
                        m_Operators.erase(it);
                        break;
                    }
                }
            }
        }

        // ── Direction selection phase ─────────────────────────────────
        if (m_WaitingForDirection) {
            if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB) && !m_IsDirectionDragging) {
                m_IsDirectionDragging = true;
                m_DirectionDragStart = {screenCursorX, screenCursorY};
            }

            if (m_IsDirectionDragging) {
                glm::vec2 diff{
                    screenCursorX - m_DirectionDragStart.x,
                    screenCursorY - m_DirectionDragStart.y
                };
                if (std::abs(diff.x) > 25.0F || std::abs(diff.y) > 25.0F) {
                    if (std::abs(diff.x) > std::abs(diff.y)) {
                        m_DeployingDirection = diff.x > 0 ? glm::ivec2(1, 0) : glm::ivec2(-1, 0);
                    } else {
                        m_DeployingDirection = diff.y > 0 ? glm::ivec2(0, 1) : glm::ivec2(0, -1);
                    }
                }
                
                if (Util::Input::IsKeyUp(Util::Keycode::MOUSE_LB)) {
                    // Finalize deployment if they dragged far enough
                    if ((std::abs(diff.x) > 25.0F || std::abs(diff.y) > 25.0F) &&
                        IsValidOperatorTypeIndex(m_DragOperatorType)) {
                        const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(m_DragOperatorType));
                        m_DP -= static_cast<float>(opType.cost);

                        Operator newOp;
                        newOp.id        = m_NextOperatorId++;
                        newOp.typeIndex = m_DragOperatorType;
                        newOp.cell      = m_DirectionCell;
                        newOp.direction = m_DeployingDirection;
                        newOp.cooldownMs= 200.0F;
                        newOp.hp        = opType.hp;
                        newOp.maxHp     = opType.hp;
                        newOp.def       = opType.def;
                        newOp.deathAnimationFinished = false;
                        newOp.deathElapsedMs = 0.0F;
                        newOp.redeployCooldownStarted = false;
                        const bool isBagpipe = (opType.name == "Bagpipe" || opType.name == u8"風笛");
                        newOp.sp        = isBagpipe ? 2.0F : opType.initialSp;
                        if (isBagpipe) {
                            newOp.sp = std::clamp(newOp.sp, 0.0F, Ark::GameConst::BAGPIPE_MAX_SP);
                        } else {
                            newOp.sp = std::min(newOp.sp, opType.maxSp);
                        }

                        m_Operators.push_back(newOp);
                        m_SelectedOperatorId = -1;
                        m_SelectedOperatorCardType = -1;
                        m_DragOperatorType = -1;
                    } else {
                        // Cancel deployment if dropped inside the center area
                        m_SelectedOperatorId = -1;
                    }
                    m_WaitingForDirection = false;
                    m_DraggingFromBar = false;
                    m_IsDeploying = false;
                    m_IsDirectionDragging = false;
                    m_OperatorCardPressActive = false;
                    m_PressedOperatorCardType = -1;
                }
            }

            // Right-click cancels direction selection
            if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_RB)) {
                m_WaitingForDirection = false;
                m_DraggingFromBar = false;
                m_IsDeploying = false;
                m_IsDirectionDragging = false;
            }
        }
        // ── Drag from operator bar ────────────────────────────────────
        else if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_LB) &&
                 !m_DraggingFromBar && !m_OperatorCardPressActive) {
            const int clickedCard = findOperatorCardAt(screenCursorX, screenCursorY);
            if (clickedCard >= 0) {
                m_SelectedOperatorCardType = clickedCard;
                m_DragOperatorType = clickedCard;
                m_SelectedOperatorId = -1;
                m_DragScreenPos = {screenCursorX, screenCursorY};
                m_OperatorInfoTab = 0;
                if (canDragOperatorCard(clickedCard)) {
                    m_OperatorCardPressActive = true;
                    m_PressedOperatorCardType = clickedCard;
                    m_OperatorCardPressPos = m_DragScreenPos;
                }
            } else {
                // Click on board – check skill activation or operator selection
                const auto cell = ToCell(ptsdCursor);
                bool clickedOperator = false;
                if (cell && m_WaveRunning) {
                    for (auto& op : m_Operators) {
                        if (op.cell != *cell) continue;
                        clickedOperator = true;
                        m_SelectedOperatorId = op.id;
                        m_SelectedOperatorCardType = -1;
                        m_DragOperatorType = -1;
                        m_OperatorCardPressActive = false;
                        m_PressedOperatorCardType = -1;
                        m_OperatorInfoTab = 0;
                        HandleSkillActivation(*cell);
                        break;
                    }
                }
                if (!clickedOperator) {
                    m_SelectedOperatorId = -1;
                    m_SelectedOperatorCardType = -1;
                }
            }
        }

        // ── Update drag position ──────────────────────────────────────
        if (m_DraggingFromBar && Util::Input::IsKeyPressed(Util::Keycode::MOUSE_LB)) {
            m_DragScreenPos = {screenCursorX, screenCursorY};
            
            // Show deploy preview based on hovered cell
            const auto cell = ResolveDeploymentCell(m_DragOperatorType, ptsdCursor);
            if (cell) {
                m_IsDeploying = true;
                m_DeployingCell = *cell;
            } else {
                m_IsDeploying = false;
            }
        }

        // ── Release drag – drop on cell ───────────────────────────────
        if (m_DraggingFromBar && Util::Input::IsKeyUp(Util::Keycode::MOUSE_LB)) {
            const auto cell = ResolveDeploymentCell(m_DragOperatorType, ptsdCursor);
            if (cell && m_WaveRunning && m_DragOperatorType >= 0) {
                if (IsOperatorTypeAvailable(m_DragOperatorType) &&
                    IsDeployableCellForOperatorType(m_DragOperatorType, *cell) && !IsCellOccupied(*cell) &&
                    static_cast<int>(m_Operators.size()) < MAX_OPS) {
                    const auto& opType = m_OperatorTemplates.at(static_cast<std::size_t>(m_DragOperatorType));
                    if (m_DP >= static_cast<float>(opType.cost)) {
                        // Enter direction selection phase
                        m_WaitingForDirection = true;
                        m_DirectionCell = *cell;
                        m_DeployingDirection = {1, 0};
                        m_IsDeploying = true;
                        m_DeployingCell = *cell;
                        m_DraggingFromBar = false;
                    } else {
                        m_DraggingFromBar = false;
                        m_IsDeploying = false;
                    }
                } else {
                    m_DraggingFromBar = false;
                    m_IsDeploying = false;
                }
            } else {
                m_DraggingFromBar = false;
                m_IsDeploying = false;
            }
            m_OperatorCardPressActive = false;
            m_PressedOperatorCardType = -1;
        }
    }

    const bool slowOperatorInfo = operatorInfoVisible() && !m_GamePaused && !m_ShowQuitConfirm;
    const float cheatSpeedMultiplier = m_CheatMode ? 10.0F : 1.0F;
    const float playSpeed = m_GameSpeedMultiplier * cheatSpeedMultiplier * (slowOperatorInfo ? 0.125F : 1.0F);
    const float gameDt = (m_MissionClear || m_GameOver) ? dt : dt * playSpeed;
    UpdateGame(gameDt);
    if (m_Renderer) {
        m_Renderer->DrawScene(ptsdCursor);
    }
}

void App::End() {}
