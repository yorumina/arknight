#include "commands.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Context.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "cli_utils.hpp"
#include "json_utils.hpp"
#include "types.hpp"

#ifdef _MSC_VER
#define popen _popen
#define pclose _pclose
#endif

namespace ark_builder {
namespace {

struct Vec2 {
    float x = 0.0F;
    float y = 0.0F;
};

struct Canvas {
    ImVec2 min{0.0F, 0.0F};
    ImVec2 max{0.0F, 0.0F};
    Vec2 refSize{1.0F, 1.0F};
    float scale = 1.0F;

    [[nodiscard]] ImVec2 ToScreen(const Vec2& p) const {
        return {min.x + p.x * scale, min.y + p.y * scale};
    }

    [[nodiscard]] Vec2 ToReference(const ImVec2& p) const {
        return {(p.x - min.x) / scale, (p.y - min.y) / scale};
    }

    [[nodiscard]] bool Contains(const ImVec2& p) const {
        return p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y;
    }
};

enum class Corner { TopLeft = 0, TopRight = 1, BottomRight = 2, BottomLeft = 3 };

struct OpeningVideoAction {
    bool enabled = true;
    float startMs = 5000.0F;
    float endMs = 600000.0F;
    float frameSec = 5.0F;
    std::array<Vec2, 4> corners{};
};

enum class OpeningTarget { Video1, Video2 };

constexpr Vec2 kDefaultVideoSize{2342.0F, 1080.0F};
constexpr std::array<Vec2, 4> kDefaultAwakenButton{
    Vec2{1016.0F, 713.0F},
    Vec2{1329.0F, 713.0F},
    Vec2{1329.0F, 819.0F},
    Vec2{1016.0F, 819.0F},
};

auto TargetConfigKey(OpeningTarget target) -> const char* {
    return target == OpeningTarget::Video2 ? "video_2_awaken" : "video_1_action";
}

auto TargetLabel(OpeningTarget target) -> const char* {
    return target == OpeningTarget::Video2 ? "loginpage_2 awaken area" : "loginpage_1 transition area";
}

auto PointToJson(const Vec2& point) -> json {
    return json::array({point.x, point.y});
}

auto PointFromJson(const json& owner, const char* key) -> std::optional<Vec2> {
    if (!owner.contains(key) || !owner[key].is_array() || owner[key].size() != 2) return std::nullopt;
    if (!owner[key][0].is_number() || !owner[key][1].is_number()) return std::nullopt;
    return Vec2{owner[key][0].get<float>(), owner[key][1].get<float>()};
}

auto CornerKey(Corner corner) -> const char* {
    switch (corner) {
    case Corner::TopLeft: return "top_left";
    case Corner::TopRight: return "top_right";
    case Corner::BottomRight: return "bottom_right";
    case Corner::BottomLeft: return "bottom_left";
    }
    return "top_left";
}

auto CornerLabel(Corner corner) -> const char* {
    switch (corner) {
    case Corner::TopLeft: return "Top Left";
    case Corner::TopRight: return "Top Right";
    case Corner::BottomRight: return "Bottom Right";
    case Corner::BottomLeft: return "Bottom Left";
    }
    return "Corner";
}

auto FullQuad(const Vec2& size) -> std::array<Vec2, 4> {
    return {
        Vec2{0.0F, 0.0F},
        Vec2{size.x, 0.0F},
        Vec2{size.x, size.y},
        Vec2{0.0F, size.y},
    };
}

auto ScalePoint(const Vec2& point, const Vec2& from, const Vec2& to) -> Vec2 {
    const float sx = from.x > 0.0F ? to.x / from.x : 1.0F;
    const float sy = from.y > 0.0F ? to.y / from.y : 1.0F;
    return {point.x * sx, point.y * sy};
}

auto ScaleQuad(const std::array<Vec2, 4>& quad, const Vec2& from, const Vec2& to) -> std::array<Vec2, 4> {
    std::array<Vec2, 4> result{};
    for (std::size_t i = 0; i < quad.size(); ++i) {
        result[i] = ScalePoint(quad[i], from, to);
    }
    return result;
}

auto ResolveMenuConfigPath() -> std::filesystem::path {
    auto dir = ResolveDataRoot() / "loadingpage";
    std::filesystem::create_directories(dir);
    return (dir / "menu_buttons.json").lexically_normal();
}

auto ShellQuote(const std::string& value) -> std::string {
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

auto FormatSeconds(float seconds) -> std::string {
    std::ostringstream output;
    output << std::fixed << std::setprecision(3) << std::max(0.0F, seconds);
    return output.str();
}

auto ReadFirstLineFromCommand(const std::string& command) -> std::string {
    FILE* pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) return {};

    char buffer[256] = {};
    const bool gotLine = std::fgets(buffer, sizeof(buffer), pipe) != nullptr;
    pclose(pipe);
    return gotLine ? std::string(buffer) : std::string{};
}

auto ProbeVideoDurationSec(const std::filesystem::path& videoPath) -> float {
    AddCommonFfmpegPathsOnWindows();
    const std::string command =
        "ffprobe -v quiet -show_entries format=duration "
        "-of default=noprint_wrappers=1:nokey=1 " + ShellQuote(videoPath.string());
    std::string line = ReadFirstLineFromCommand(command);
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
    if (line.empty()) return 0.0F;
    try {
        return std::max(0.0F, std::stof(line));
    } catch (const std::exception&) {
        return 0.0F;
    }
}

auto FirstPositionalAfterCommand(const std::vector<std::string>& args) -> std::optional<std::string> {
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (!args[i].empty() && args[i][0] == '-') {
            if (i + 1 < args.size() && !args[i + 1].empty() && args[i + 1][0] != '-') ++i;
            continue;
        }
        return args[i];
    }
    return std::nullopt;
}

auto ParseTargetOption(const std::vector<std::string>& args) -> std::optional<OpeningTarget> {
    const auto raw = FindOption(args, "--target");
    if (!raw) return std::nullopt;
    if (*raw == "video1" || *raw == "loginpage_1" || *raw == "1") return OpeningTarget::Video1;
    if (*raw == "video2" || *raw == "loginpage_2" || *raw == "2") return OpeningTarget::Video2;
    throw CliError("--target must be video1 or video2");
}

auto InferTargetFromPath(const std::filesystem::path& videoPath) -> OpeningTarget {
    std::string filename = videoPath.filename().string();
    std::transform(filename.begin(), filename.end(), filename.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (filename.find("_2") != std::string::npos ||
        filename.find("page2") != std::string::npos ||
        filename.find("video2") != std::string::npos) {
        return OpeningTarget::Video2;
    }
    return OpeningTarget::Video1;
}

auto ResolveOpeningVideoPath(const std::vector<std::string>& args,
                             std::optional<OpeningTarget> preferredTarget) -> std::filesystem::path {
    if (const auto userPath = FirstPositionalAfterCommand(args)) {
        std::filesystem::path videoPath = *userPath;
        if (!videoPath.is_absolute()) {
            if (!std::filesystem::exists(videoPath)) {
                videoPath = ResolveDataRoot() / "loadingpage" / videoPath;
            }
        }
        videoPath = videoPath.lexically_normal();
        if (!std::filesystem::exists(videoPath)) {
            throw CliError("opening video not found: " + videoPath.string());
        }
        return videoPath;
    }

    const auto dir = ResolveDataRoot() / "loadingpage";
    if (preferredTarget && *preferredTarget == OpeningTarget::Video2) {
        const std::array<std::filesystem::path, 2> candidates{
            dir / "loadingpage_2.mp4",
            dir / "loginpage_2.mp4",
        };
        for (const auto& candidate : candidates) {
            if (std::filesystem::exists(candidate)) return candidate.lexically_normal();
        }
        throw CliError("opening video not found: data/loadingpage/loadingpage_2.mp4 or loginpage_2.mp4");
    }

    const std::array<std::filesystem::path, 2> candidates{
        dir / "loadingpage_1.mp4",
        dir / "loginpage_1.mp4",
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) return candidate.lexically_normal();
    }
    throw CliError("opening video not found: data/loadingpage/loadingpage_1.mp4 or loginpage_1.mp4");
}

auto ExtractVideoFrame(const std::filesystem::path& videoPath, float frameSec) -> std::filesystem::path {
    AddCommonFfmpegPathsOnWindows();
    auto outputPath = (std::filesystem::temp_directory_path() / "arknightbuilder_opening_frame.png").lexically_normal();
    std::error_code ignored;
    std::filesystem::remove(outputPath, ignored);

    const std::string command =
        "ffmpeg -y -hide_banner -loglevel error -ss " + FormatSeconds(frameSec) +
        " -i " + ShellQuote(videoPath.string()) +
        " -frames:v 1 " + ShellQuote(outputPath.string());
    const int result = std::system(command.c_str());
    if (result != 0 || !std::filesystem::exists(outputPath)) {
        throw CliError("unable to extract video frame with ffmpeg");
    }
    return outputPath;
}

auto LoadSavedFrameSec(const std::filesystem::path& configPath, const char* configKey) -> std::optional<float> {
    if (!std::filesystem::exists(configPath)) return std::nullopt;
    try {
        const json doc = LoadJson(configPath);
        if (!doc.is_object() || !doc.contains(configKey) ||
            !doc[configKey].is_object()) {
            return std::nullopt;
        }
        const auto& action = doc[configKey];
        if (action.contains("frame_sec") && action["frame_sec"].is_number()) {
            return action["frame_sec"].get<float>();
        }
    } catch (const std::exception&) {
    }
    return std::nullopt;
}

auto LoadAction(const std::filesystem::path& configPath,
                const Vec2& frameSize,
                float durationSec,
                OpeningTarget target) -> OpeningVideoAction {
    OpeningVideoAction action;
    if (target == OpeningTarget::Video2) {
        action.startMs = 5500.0F;
        action.endMs = durationSec > 0.0F ? std::max(5500.0F, durationSec * 1000.0F) : 600000.0F;
        action.frameSec = 5.5F;
        action.corners = ScaleQuad(kDefaultAwakenButton, kDefaultVideoSize, frameSize);
    } else {
        action.startMs = 5000.0F;
        action.endMs = durationSec > 0.0F ? std::max(5000.0F, durationSec * 1000.0F) : 600000.0F;
        action.frameSec = 5.0F;
        action.corners = FullQuad(frameSize);
    }

    if (!std::filesystem::exists(configPath)) return action;

    try {
        const json doc = LoadJson(configPath);
        const char* configKey = TargetConfigKey(target);
        if (!doc.is_object() || !doc.contains(configKey) ||
            !doc[configKey].is_object()) {
            return action;
        }

        const auto& src = doc[configKey];
        action.enabled = src.value("enabled", action.enabled);
        if (src.contains("start_ms") && src["start_ms"].is_number()) {
            action.startMs = src["start_ms"].get<float>();
        } else if (src.contains("start_sec") && src["start_sec"].is_number()) {
            action.startMs = src["start_sec"].get<float>() * 1000.0F;
        }
        if (src.contains("end_ms") && src["end_ms"].is_number()) {
            action.endMs = src["end_ms"].get<float>();
        } else if (src.contains("end_sec") && src["end_sec"].is_number()) {
            action.endMs = src["end_sec"].get<float>() * 1000.0F;
        }
        if (src.contains("frame_sec") && src["frame_sec"].is_number()) {
            action.frameSec = src["frame_sec"].get<float>();
        }
        action.startMs = std::max(0.0F, action.startMs);
        action.endMs = std::max(action.startMs, action.endMs);

        Vec2 referenceSize = frameSize;
        if (src.contains("reference_size") && src["reference_size"].is_array() &&
            src["reference_size"].size() == 2 &&
            src["reference_size"][0].is_number() &&
            src["reference_size"][1].is_number()) {
            referenceSize = {src["reference_size"][0].get<float>(), src["reference_size"][1].get<float>()};
        }
        if (referenceSize.x <= 0.0F || referenceSize.y <= 0.0F) referenceSize = kDefaultVideoSize;

        const auto topLeft = PointFromJson(src, "top_left");
        const auto topRight = PointFromJson(src, "top_right");
        const auto bottomRight = PointFromJson(src, "bottom_right");
        const auto bottomLeft = PointFromJson(src, "bottom_left");
        if (topLeft && topRight && bottomRight && bottomLeft) {
            const std::array<Vec2, 4> raw{*topLeft, *topRight, *bottomRight, *bottomLeft};
            action.corners = ScaleQuad(raw, referenceSize, frameSize);
        }
    } catch (const std::exception&) {
    }

    return action;
}

void SaveAction(const std::filesystem::path& configPath,
                const OpeningVideoAction& action,
                const Vec2& frameSize,
                OpeningTarget target) {
    json doc = json::object();
    if (std::filesystem::exists(configPath)) {
        try {
            doc = LoadJson(configPath);
            if (!doc.is_object()) doc = json::object();
        } catch (const std::exception&) {
            doc = json::object();
        }
    }

    doc[TargetConfigKey(target)] = {
        {"enabled", action.enabled},
        {"frame_sec", action.frameSec},
        {"start_ms", action.startMs},
        {"end_ms", action.endMs},
        {"reference_size", json::array({frameSize.x, frameSize.y})},
        {"top_left", PointToJson(action.corners[0])},
        {"top_right", PointToJson(action.corners[1])},
        {"bottom_right", PointToJson(action.corners[2])},
        {"bottom_left", PointToJson(action.corners[3])},
    };
    SaveJson(configPath, doc);
}

auto DistanceSq(ImVec2 a, ImVec2 b) -> float {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

auto HoveredCorner(const OpeningVideoAction& action, const Canvas& canvas, ImVec2 mouse) -> std::optional<Corner> {
    constexpr float hitRadius = 13.0F;
    float bestDistanceSq = hitRadius * hitRadius;
    std::optional<Corner> best;
    for (std::size_t i = 0; i < action.corners.size(); ++i) {
        const ImVec2 handle = canvas.ToScreen(action.corners[i]);
        const float distSq = DistanceSq(mouse, handle);
        if (distSq <= bestDistanceSq) {
            bestDistanceSq = distSq;
            best = static_cast<Corner>(i);
        }
    }
    return best;
}

auto ClampedReferencePoint(const Canvas& canvas, ImVec2 point) -> Vec2 {
    const Vec2 ref = canvas.ToReference(point);
    return {
        std::clamp(ref.x, 0.0F, canvas.refSize.x),
        std::clamp(ref.y, 0.0F, canvas.refSize.y)
    };
}

void DrawImageCover(ImDrawList* draw, const std::shared_ptr<Util::Image>& image, const Canvas& canvas) {
    if (!image || image->GetTextureId() == 0) return;
    const glm::vec2 imageSize = image->GetSize();
    if (imageSize.x <= 0.0F || imageSize.y <= 0.0F) return;

    const float canvasW = canvas.max.x - canvas.min.x;
    const float canvasH = canvas.max.y - canvas.min.y;
    const float scale = std::max(canvasW / imageSize.x, canvasH / imageSize.y);
    const float drawW = imageSize.x * scale;
    const float drawH = imageSize.y * scale;
    const ImVec2 min{canvas.min.x + (canvasW - drawW) * 0.5F,
                     canvas.min.y + (canvasH - drawH) * 0.5F};
    const ImVec2 max{min.x + drawW, min.y + drawH};
    draw->PushClipRect(canvas.min, canvas.max, true);
    draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(image->GetTextureId())), min, max);
    draw->PopClipRect();
}

void DrawAction(ImDrawList* draw,
                const OpeningVideoAction& action,
                const Canvas& canvas,
                OpeningTarget target,
                std::optional<Corner> hoveredCorner,
                std::optional<Corner> activeCorner) {
    const std::array<ImVec2, 4> q{
        canvas.ToScreen(action.corners[0]),
        canvas.ToScreen(action.corners[1]),
        canvas.ToScreen(action.corners[2]),
        canvas.ToScreen(action.corners[3]),
    };
    draw->AddQuadFilled(q[0], q[1], q[2], q[3], IM_COL32(255, 180, 48, 70));
    draw->AddQuad(q[0], q[1], q[2], q[3], IM_COL32(255, 195, 64, 250), 2.8F);
    draw->AddText({q[0].x + 8.0F, q[0].y - 20.0F}, IM_COL32(255, 220, 90, 255),
                  TargetLabel(target));

    for (std::size_t i = 0; i < action.corners.size(); ++i) {
        const Corner corner = static_cast<Corner>(i);
        const bool active = activeCorner && *activeCorner == corner;
        const bool hovered = hoveredCorner && *hoveredCorner == corner;
        const ImVec2 p = canvas.ToScreen(action.corners[i]);
        const float radius = active ? 8.0F : hovered ? 7.0F : 5.5F;
        const ImU32 fill = active ? IM_COL32(255, 160, 34, 255)
                         : hovered ? IM_COL32(255, 248, 132, 255)
                                   : IM_COL32(255, 230, 70, 245);
        draw->AddCircleFilled(p, radius, fill);
        draw->AddCircle(p, radius + 3.0F, IM_COL32(20, 20, 20, 245), 20, 1.4F);
    }
}

} // namespace

void RunOpeningCalibrate(const std::vector<std::string>& args) {
    const auto targetOption = ParseTargetOption(args);
    const auto videoPath = ResolveOpeningVideoPath(args, targetOption);
    const OpeningTarget target = targetOption.value_or(InferTargetFromPath(videoPath));
    const char* configKey = TargetConfigKey(target);
    const auto configPath = ResolveMenuConfigPath();
    const float durationSec = ProbeVideoDurationSec(videoPath);

    const auto frameSecOption = FindOption(args, "--frame-sec");
    float initialFrameSec = frameSecOption
        ? static_cast<float>(ParseDouble(*frameSecOption, "frame-sec"))
        : LoadSavedFrameSec(configPath, configKey).value_or(target == OpeningTarget::Video2 ? 5.5F : 5.0F);
    if (durationSec > 0.0F) initialFrameSec = std::clamp(initialFrameSec, 0.0F, durationSec);

    auto framePath = ExtractVideoFrame(videoPath, initialFrameSec);
    auto context = Core::Context::GetInstance();
    auto frameImage = std::make_shared<Util::Image>(framePath.string());
    glm::vec2 rawFrameSize = frameImage->GetSize();
    Vec2 frameSize{std::max(1.0F, rawFrameSize.x), std::max(1.0F, rawFrameSize.y)};

    auto action = LoadAction(configPath, frameSize, durationSec, target);
    action.frameSec = initialFrameSec;
    if (const auto startSec = FindOption(args, "--start-sec")) {
        action.startMs = std::max(0.0F, static_cast<float>(ParseDouble(*startSec, "start-sec")) * 1000.0F);
    }
    if (const auto endSec = FindOption(args, "--end-sec")) {
        action.endMs = std::max(action.startMs, static_cast<float>(ParseDouble(*endSec, "end-sec")) * 1000.0F);
    }

    Corner selectedCorner = Corner::TopLeft;
    std::optional<Corner> draggingCorner;
    bool wasLeftDown = false;
    bool dirty = false;
    std::string status = std::filesystem::exists(configPath)
        ? "Loaded " + std::string(configKey) + " from " + configPath.string()
        : "Ready with default " + std::string(configKey);

    auto reloadFrame = [&]() {
        const Vec2 oldSize = frameSize;
        if (durationSec > 0.0F) action.frameSec = std::clamp(action.frameSec, 0.0F, durationSec);
        framePath = ExtractVideoFrame(videoPath, action.frameSec);
        frameImage = std::make_shared<Util::Image>(framePath.string());
        rawFrameSize = frameImage->GetSize();
        frameSize = {std::max(1.0F, rawFrameSize.x), std::max(1.0F, rawFrameSize.y)};
        action.corners = ScaleQuad(action.corners, oldSize, frameSize);
    };

    while (!context->GetExit()) {
        context->Setup();
        ImGuiIO& io = ImGui::GetIO();
        const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
        if ((keyboardState != nullptr && keyboardState[SDL_SCANCODE_ESCAPE] != 0) ||
            Util::Input::IfExit()) {
            context->SetExit(true);
        }

        ImGui::SetNextWindowPos({12.0F, 12.0F}, ImGuiCond_Once);
        ImGui::SetNextWindowSize({380.0F, 560.0F}, ImGuiCond_Once);
        ImGui::Begin("Arknight Builder - Opening Video");

        ImGui::Checkbox("Enabled", &action.enabled);

        float startSec = action.startMs / 1000.0F;
        float endSec = action.endMs / 1000.0F;
        if (ImGui::InputFloat("Start sec", &startSec, 0.1F, 1.0F, "%.3f")) {
            action.startMs = std::max(0.0F, startSec * 1000.0F);
            action.endMs = std::max(action.startMs, action.endMs);
            dirty = true;
        }
        if (ImGui::InputFloat("End sec", &endSec, 0.1F, 1.0F, "%.3f")) {
            action.endMs = std::max(action.startMs, endSec * 1000.0F);
            dirty = true;
        }
        if (ImGui::InputFloat("Preview frame sec", &action.frameSec, 0.1F, 1.0F, "%.3f")) {
            dirty = true;
        }
        if (durationSec > 0.0F) {
            ImGui::Text("Video duration: %.3fs", durationSec);
        }
        ImGui::Text("Frame: %.0fx%.0f", frameSize.x, frameSize.y);
        ImGui::Text("Video: %s", videoPath.filename().string().c_str());

        const char* cornerLabels[] = {"Top Left", "Top Right", "Bottom Right", "Bottom Left"};
        int cornerIndex = static_cast<int>(selectedCorner);
        if (ImGui::Combo("Corner", &cornerIndex, cornerLabels, 4)) {
            selectedCorner = static_cast<Corner>(cornerIndex);
        }

        if (ImGui::Button("Save")) {
            action.endMs = std::max(action.startMs, action.endMs);
            SaveAction(configPath, action, frameSize, target);
            dirty = false;
            status = "Saved " + std::string(configKey) + " to " + configPath.string();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload Config")) {
            action = LoadAction(configPath, frameSize, durationSec, target);
            draggingCorner.reset();
            dirty = false;
            status = "Reloaded config";
        }
        if (ImGui::Button("Reload Frame")) {
            reloadFrame();
            status = "Reloaded frame at " + FormatSeconds(action.frameSec) + "s";
        }

        ImGui::Text("Drag yellow point: move corner");
        ImGui::Text("Click frame: set selected corner");
        ImGui::Text("Config key: %s", configKey);
        ImGui::Text("Runtime: click must hit this target");
        ImGui::Text("Status: %s%s", status.c_str(), dirty ? " *" : "");
        ImGui::End();

        const ImVec2 viewportMin{410.0F, 18.0F};
        const ImVec2 viewportMax{io.DisplaySize.x - 18.0F, io.DisplaySize.y - 18.0F};
        Canvas canvas;
        canvas.refSize = frameSize;
        const float availW = std::max(1.0F, viewportMax.x - viewportMin.x);
        const float availH = std::max(1.0F, viewportMax.y - viewportMin.y);
        canvas.scale = std::min(availW / canvas.refSize.x, availH / canvas.refSize.y);
        const ImVec2 size{canvas.refSize.x * canvas.scale, canvas.refSize.y * canvas.scale};
        canvas.min = {viewportMin.x + (availW - size.x) * 0.5F, viewportMin.y + (availH - size.y) * 0.5F};
        canvas.max = {canvas.min.x + size.x, canvas.min.y + size.y};

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        draw->AddRectFilled(viewportMin, viewportMax, IM_COL32(12, 14, 18, 255));
        draw->AddRectFilled(canvas.min, canvas.max, IM_COL32(22, 25, 30, 255));
        DrawImageCover(draw, frameImage, canvas);

        int mouseX = 0;
        int mouseY = 0;
        const Uint32 mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);
        const bool leftDown = (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        const bool leftPressed = leftDown && !wasLeftDown;
        const bool leftReleased = !leftDown && wasLeftDown;
        const ImVec2 mouse{static_cast<float>(mouseX), static_cast<float>(mouseY)};
        const bool mouseOnCanvas = canvas.Contains(mouse);

        std::optional<Corner> hoveredCorner;
        Vec2 refMouse{};
        if (mouseOnCanvas || draggingCorner) {
            refMouse = ClampedReferencePoint(canvas, mouse);
        }
        if (mouseOnCanvas) {
            hoveredCorner = HoveredCorner(action, canvas, mouse);
        }

        DrawAction(draw, action, canvas, target, hoveredCorner, draggingCorner);

        if (mouseOnCanvas) {
            draw->AddLine({canvas.min.x, mouse.y}, {canvas.max.x, mouse.y}, IM_COL32(255, 255, 255, 95), 1.0F);
            draw->AddLine({mouse.x, canvas.min.y}, {mouse.x, canvas.max.y}, IM_COL32(255, 255, 255, 95), 1.0F);

            if (leftPressed && hoveredCorner) {
                draggingCorner = hoveredCorner;
                selectedCorner = *hoveredCorner;
            } else if (leftPressed) {
                action.corners[static_cast<std::size_t>(selectedCorner)] = refMouse;
                dirty = true;
                status = "Set " + std::string(CornerKey(selectedCorner));
            }
        }

        if (draggingCorner && leftDown) {
            action.corners[static_cast<std::size_t>(*draggingCorner)] = refMouse;
            dirty = true;
            status = "Dragging " + std::string(CornerLabel(*draggingCorner));
        }
        if (draggingCorner && leftReleased) {
            status = "Moved " + std::string(CornerLabel(*draggingCorner));
            draggingCorner.reset();
        }

        wasLeftDown = leftDown;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        context->Update();
        if (Util::Input::IfExit()) {
            context->SetExit(true);
        }
    }

    if (dirty) {
        std::cout << "Opening video calibration has unsaved changes; use Save before closing to persist edits.\n";
    }

    frameImage.reset();
    std::error_code ignored;
    std::filesystem::remove(framePath, ignored);
}

} // namespace ark_builder
