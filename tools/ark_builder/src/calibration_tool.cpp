#include "commands.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Context.hpp"
#include "Util/Input.hpp"
#include "Util/Image.hpp"
#include "cli_utils.hpp"
#include "json_utils.hpp"
#include "types.hpp"

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

enum class Corner { TopLeft = 0, TopRight = 1, BottomLeft = 2, BottomRight = 3 };

struct StageModel {
    std::filesystem::path path;
    json doc;
    int width = 0;
    int height = 0;
    Vec2 referenceSize{2048.0F, 945.0F};
    std::string backgroundPath;
};

auto PointFromJson(const json& owner, const char* key) -> std::optional<Vec2> {
    if (!owner.contains(key) || !owner[key].is_array() || owner[key].size() != 2) return std::nullopt;
    if (!owner[key][0].is_number() || !owner[key][1].is_number()) return std::nullopt;
    return Vec2{owner[key][0].get<float>(), owner[key][1].get<float>()};
}

auto PointToJson(const Vec2& point) -> json {
    return json::array({point.x, point.y});
}

auto TileAt(const StageModel& stage, int x, int y) -> std::string {
    if (!stage.doc.contains("tiles") || !stage.doc["tiles"].is_array()) return "empty";
    if (y < 0 || y >= static_cast<int>(stage.doc["tiles"].size())) return "empty";
    const auto& row = stage.doc["tiles"][static_cast<std::size_t>(y)];
    if (!row.is_array() || x < 0 || x >= static_cast<int>(row.size())) return "empty";
    const auto& cell = row[static_cast<std::size_t>(x)];
    if (cell.is_string()) return cell.get<std::string>();
    if (cell.is_object()) return cell.value("type", std::string{"empty"});
    return "empty";
}

auto NormalizedTileAt(const StageModel& stage, int x, int y) -> std::string {
    std::string tile = TileAt(stage, x, y);
    std::transform(tile.begin(), tile.end(), tile.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return tile;
}

auto IsMappableTile(const StageModel& stage, int x, int y) -> bool {
    return NormalizedTileAt(stage, x, y) != "empty";
}

auto Lerp(const Vec2& a, const Vec2& b, float t) -> Vec2 {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

auto QuadPoint(const std::array<Vec2, 4>& q, float u, float v) -> Vec2 {
    return Lerp(Lerp(q[0], q[1], u), Lerp(q[3], q[2], u), v);
}

auto BaseBoardQuad(const StageModel& stage) -> std::array<Vec2, 4> {
    const json* art = nullptr;
    if (stage.doc.contains("board_art") && stage.doc["board_art"].is_object()) {
        art = &stage.doc["board_art"];
    }

    const auto topLeft = art != nullptr
        ? PointFromJson(*art, "top_left").value_or(Vec2{0.0F, 0.0F})
        : Vec2{0.0F, 0.0F};
    const auto topRight = art != nullptr
        ? PointFromJson(*art, "top_right").value_or(Vec2{stage.referenceSize.x, 0.0F})
        : Vec2{stage.referenceSize.x, 0.0F};
    const auto bottomRight = art != nullptr
        ? PointFromJson(*art, "bottom_right").value_or(stage.referenceSize)
        : stage.referenceSize;
    const auto bottomLeft = art != nullptr
        ? PointFromJson(*art, "bottom_left").value_or(Vec2{0.0F, stage.referenceSize.y})
        : Vec2{0.0F, stage.referenceSize.y};
    return {topLeft, topRight, bottomRight, bottomLeft};
}

auto FallbackCellQuad(const StageModel& stage, int x, int y) -> std::array<Vec2, 4> {
    const auto base = BaseBoardQuad(stage);
    const float w = static_cast<float>(std::max(1, stage.width));
    const float h = static_cast<float>(std::max(1, stage.height));
    const float x0 = static_cast<float>(x) / w;
    const float x1 = static_cast<float>(x + 1) / w;
    const float y0 = static_cast<float>(y) / h;
    const float y1 = static_cast<float>(y + 1) / h;
    return {
        QuadPoint(base, x0, y0),
        QuadPoint(base, x1, y0),
        QuadPoint(base, x1, y1),
        QuadPoint(base, x0, y1),
    };
}

auto FindCellOverride(const json& cells, int x, int y) -> int {
    if (!cells.is_array()) return -1;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (!cells[i].is_object()) continue;
        if (cells[i].value("x", -1) == x && cells[i].value("y", -1) == y) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

auto CellQuad(const StageModel& stage, int x, int y) -> std::array<Vec2, 4> {
    if (stage.doc.contains("board_art") && stage.doc["board_art"].is_object() &&
        stage.doc["board_art"].contains("cells")) {
        const auto& cells = stage.doc["board_art"]["cells"];
        const int index = FindCellOverride(cells, x, y);
        if (index >= 0) {
            const auto& cell = cells[static_cast<std::size_t>(index)];
            const auto topLeft = PointFromJson(cell, "top_left");
            const auto topRight = PointFromJson(cell, "top_right");
            const auto bottomRight = PointFromJson(cell, "bottom_right");
            const auto bottomLeft = PointFromJson(cell, "bottom_left");
            if (topLeft && topRight && bottomRight && bottomLeft) {
                return {*topLeft, *topRight, *bottomRight, *bottomLeft};
            }
        }
    }
    return FallbackCellQuad(stage, x, y);
}

auto CornerKey(Corner corner) -> const char* {
    switch (corner) {
    case Corner::TopLeft: return "top_left";
    case Corner::TopRight: return "top_right";
    case Corner::BottomLeft: return "bottom_left";
    case Corner::BottomRight: return "bottom_right";
    }
    return "top_left";
}

auto CornerLabel(Corner corner) -> const char* {
    switch (corner) {
    case Corner::TopLeft: return "left top";
    case Corner::TopRight: return "right top";
    case Corner::BottomLeft: return "left bottom";
    case Corner::BottomRight: return "right bottom";
    }
    return "corner";
}

auto CornerFromQuadIndex(std::size_t index) -> Corner {
    switch (index) {
    case 0: return Corner::TopLeft;
    case 1: return Corner::TopRight;
    case 2: return Corner::BottomRight;
    case 3: return Corner::BottomLeft;
    default: return Corner::TopLeft;
    }
}

void SetCellCorner(StageModel& stage, int x, int y, Corner corner, const Vec2& point) {
    if (!stage.doc.contains("board_art") || !stage.doc["board_art"].is_object()) {
        stage.doc["board_art"] = json::object();
    }
    auto& art = stage.doc["board_art"];
    art["reference_size"] = json::array({stage.referenceSize.x, stage.referenceSize.y});
    if (!art.contains("cells") || !art["cells"].is_array()) {
        art["cells"] = json::array();
    }
    auto& cells = art["cells"];
    int index = FindCellOverride(cells, x, y);
    if (index < 0) {
        const auto q = FallbackCellQuad(stage, x, y);
        json cell = {
            {"x", x},
            {"y", y},
            {"top_left", PointToJson(q[0])},
            {"top_right", PointToJson(q[1])},
            {"bottom_right", PointToJson(q[2])},
            {"bottom_left", PointToJson(q[3])},
        };
        cells.push_back(std::move(cell));
        index = static_cast<int>(cells.size()) - 1;
    }
    cells[static_cast<std::size_t>(index)][CornerKey(corner)] = PointToJson(point);
}

void PruneUnmappableCellOverrides(StageModel& stage) {
    if (!stage.doc.contains("board_art") || !stage.doc["board_art"].is_object() ||
        !stage.doc["board_art"].contains("cells") || !stage.doc["board_art"]["cells"].is_array()) {
        return;
    }

    json kept = json::array();
    for (const auto& cell : stage.doc["board_art"]["cells"]) {
        if (!cell.is_object()) continue;
        const int x = cell.value("x", -1);
        const int y = cell.value("y", -1);
        if (x < 0 || y < 0 || x >= stage.width || y >= stage.height) continue;
        if (!IsMappableTile(stage, x, y)) continue;
        kept.push_back(cell);
    }
    stage.doc["board_art"]["cells"] = std::move(kept);
}

auto VertexForCorner(int x, int y, Corner corner) -> glm::ivec2 {
    switch (corner) {
    case Corner::TopLeft: return {x, y};
    case Corner::TopRight: return {x + 1, y};
    case Corner::BottomLeft: return {x, y + 1};
    case Corner::BottomRight: return {x + 1, y + 1};
    }
    return {x, y};
}

auto SetConnectedVertex(StageModel& stage, const glm::ivec2& vertex, const Vec2& point) -> int {
    int updated = 0;
    auto setIfValid = [&](int x, int y, Corner corner) {
        if (x < 0 || y < 0 || x >= stage.width || y >= stage.height) return;
        if (!IsMappableTile(stage, x, y)) return;
        SetCellCorner(stage, x, y, corner, point);
        ++updated;
    };

    setIfValid(vertex.x,     vertex.y,     Corner::TopLeft);
    setIfValid(vertex.x - 1, vertex.y,     Corner::TopRight);
    setIfValid(vertex.x,     vertex.y - 1, Corner::BottomLeft);
    setIfValid(vertex.x - 1, vertex.y - 1, Corner::BottomRight);
    return updated;
}

auto SetConnectedCellCorner(StageModel& stage, int x, int y, Corner corner, const Vec2& point) -> int {
    return SetConnectedVertex(stage, VertexForCorner(x, y, corner), point);
}

auto ResolveStageBackgroundPath(const std::filesystem::path& stagePath, const json& doc) -> std::string {
    if (!doc.contains("background") || !doc["background"].is_object()) return {};
    const std::string image = doc["background"].value("image", std::string{});
    if (image.empty()) return {};
    const auto direct = stagePath.parent_path() / image;
    if (std::filesystem::exists(direct)) return direct.lexically_normal().string();
    const auto dataRelative = ResolveDataRoot() / image;
    if (std::filesystem::exists(dataRelative)) return dataRelative.lexically_normal().string();
    return direct.lexically_normal().string();
}

auto LoadStageModel(const std::filesystem::path& path) -> StageModel {
    StageModel stage;
    stage.path = path;
    stage.doc = LoadJson(path);
    stage.width = stage.doc.value("width", 0);
    stage.height = stage.doc.value("height", 0);
    if (stage.doc.contains("board_art") && stage.doc["board_art"].is_object()) {
        const auto& art = stage.doc["board_art"];
        if (art.contains("reference_size") && art["reference_size"].is_array() &&
            art["reference_size"].size() == 2 && art["reference_size"][0].is_number() &&
            art["reference_size"][1].is_number()) {
            stage.referenceSize = {art["reference_size"][0].get<float>(), art["reference_size"][1].get<float>()};
        }
    }
    stage.referenceSize.x = std::max(1.0F, stage.referenceSize.x);
    stage.referenceSize.y = std::max(1.0F, stage.referenceSize.y);
    stage.backgroundPath = ResolveStageBackgroundPath(path, stage.doc);
    return stage;
}

auto ListStageFiles() -> std::vector<std::filesystem::path> {
    std::vector<std::filesystem::path> result;
    const auto root = ResolveDataRoot() / "levels";
    if (!std::filesystem::exists(root)) return result;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        result.push_back(entry.path().lexically_normal());
    }
    std::sort(result.begin(), result.end());
    return result;
}

void DrawDashedLine(ImDrawList* draw, ImVec2 a, ImVec2 b, ImU32 color, float thickness = 1.0F) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.0F) return;
    const ImVec2 dir{dx / len, dy / len};
    constexpr float dash = 10.0F;
    constexpr float gap = 7.0F;
    for (float t = 0.0F; t < len; t += dash + gap) {
        const float t1 = std::min(len, t + dash);
        draw->AddLine({a.x + dir.x * t, a.y + dir.y * t},
                      {a.x + dir.x * t1, a.y + dir.y * t1},
                      color, thickness);
    }
}

auto PointInTriangle(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) -> bool {
    auto cross = [](const Vec2& p0, const Vec2& p1, const Vec2& p2) {
        return (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
    };
    const float c1 = cross(a, b, p);
    const float c2 = cross(b, c, p);
    const float c3 = cross(c, a, p);
    const bool neg = c1 < -0.001F || c2 < -0.001F || c3 < -0.001F;
    const bool pos = c1 > 0.001F || c2 > 0.001F || c3 > 0.001F;
    return !(neg && pos);
}

auto PointInQuad(const Vec2& p, const std::array<Vec2, 4>& q) -> bool {
    return PointInTriangle(p, q[0], q[1], q[2]) || PointInTriangle(p, q[0], q[2], q[3]);
}

auto DistanceSq(ImVec2 a, ImVec2 b) -> float {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

auto HoveredCell(const StageModel& stage, const Vec2& refPoint) -> std::optional<glm::ivec2> {
    for (int y = 0; y < stage.height; ++y) {
        for (int x = 0; x < stage.width; ++x) {
            if (!IsMappableTile(stage, x, y)) continue;
            if (PointInQuad(refPoint, CellQuad(stage, x, y))) {
                return glm::ivec2{x, y};
            }
        }
    }
    return std::nullopt;
}

auto ClampedReferencePoint(const Canvas& canvas, ImVec2 point) -> Vec2 {
    const Vec2 ref = canvas.ToReference(point);
    return {
        std::clamp(ref.x, 0.0F, canvas.refSize.x),
        std::clamp(ref.y, 0.0F, canvas.refSize.y)
    };
}

auto HoveredCornerHandle(const StageModel& stage, const Canvas& canvas,
                         int selectedX, int selectedY, ImVec2 mouse) -> std::optional<Corner> {
    if (selectedX < 0 || selectedY < 0 || selectedX >= stage.width || selectedY >= stage.height) {
        return std::nullopt;
    }
    if (!IsMappableTile(stage, selectedX, selectedY)) return std::nullopt;

    constexpr float hitRadius = 13.0F;
    float bestDistanceSq = hitRadius * hitRadius;
    std::optional<Corner> best;
    const auto q = CellQuad(stage, selectedX, selectedY);
    for (std::size_t i = 0; i < q.size(); ++i) {
        const ImVec2 handle = canvas.ToScreen(q[i]);
        const float distSq = DistanceSq(mouse, handle);
        if (distSq <= bestDistanceSq) {
            bestDistanceSq = distSq;
            best = CornerFromQuadIndex(i);
        }
    }
    return best;
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
    draw->AddImage(reinterpret_cast<void*>(static_cast<intptr_t>(image->GetTextureId())),
                   min, max);
    draw->PopClipRect();
}

void DrawStageOverlay(ImDrawList* draw, const StageModel& stage, const Canvas& canvas,
                      int selectedX, int selectedY, std::optional<glm::ivec2> hovered,
                      std::optional<Corner> hoveredCorner, std::optional<Corner> activeCorner) {
    for (int y = 0; y < stage.height; ++y) {
        for (int x = 0; x < stage.width; ++x) {
            if (!IsMappableTile(stage, x, y)) continue;
            const auto q = CellQuad(stage, x, y);
            const std::array<ImVec2, 4> s{
                canvas.ToScreen(q[0]), canvas.ToScreen(q[1]),
                canvas.ToScreen(q[2]), canvas.ToScreen(q[3])
            };
            const bool selected = x == selectedX && y == selectedY;
            const bool isHovered = hovered && hovered->x == x && hovered->y == y;
            const ImU32 fill = selected ? IM_COL32(255, 166, 32, 64)
                             : isHovered ? IM_COL32(92, 210, 255, 46)
                                         : IM_COL32(80, 180, 255, 18);
            const ImU32 line = selected ? IM_COL32(255, 186, 54, 245)
                             : isHovered ? IM_COL32(116, 225, 255, 220)
                                         : IM_COL32(160, 210, 255, 92);
            draw->AddQuadFilled(s[0], s[1], s[2], s[3], fill);
            draw->AddQuad(s[0], s[1], s[2], s[3], line, selected ? 2.4F : 1.0F);
        }
    }

    if (selectedX >= 0 && selectedY >= 0 && selectedX < stage.width && selectedY < stage.height &&
        IsMappableTile(stage, selectedX, selectedY)) {
        const auto q = CellQuad(stage, selectedX, selectedY);
        const Vec2 center = QuadPoint(q, 0.5F, 0.5F);
        const Vec2 operatorPoint = QuadPoint(q, 0.5F, 0.62F);
        const ImVec2 c = canvas.ToScreen(center);
        const ImVec2 op = canvas.ToScreen(operatorPoint);
        draw->AddCircleFilled(c, 4.5F, IM_COL32(255, 60, 60, 245));
        draw->AddCircleFilled(op, 4.5F, IM_COL32(80, 255, 140, 245));
        draw->AddText({c.x + 8.0F, c.y - 18.0F}, IM_COL32(255, 80, 80, 245), "enemy");
        draw->AddText({op.x + 8.0F, op.y + 2.0F}, IM_COL32(90, 255, 150, 245), "operator");
        for (std::size_t i = 0; i < q.size(); ++i) {
            const Corner corner = CornerFromQuadIndex(i);
            const bool active = activeCorner && *activeCorner == corner;
            const bool hover = hoveredCorner && *hoveredCorner == corner;
            const ImVec2 sp = canvas.ToScreen(q[i]);
            const float radius = active ? 8.0F : hover ? 7.0F : 5.0F;
            const ImU32 fill = active ? IM_COL32(255, 160, 34, 255)
                             : hover ? IM_COL32(255, 248, 132, 255)
                                     : IM_COL32(255, 230, 70, 245);
            draw->AddCircleFilled(sp, radius, fill);
            draw->AddCircle(sp, radius + 3.0F, IM_COL32(20, 20, 20, 245), 20, 1.4F);
        }
    }
}

} // namespace

void RunCalibrate(const std::vector<std::string>& args) {
    auto stageFiles = ListStageFiles();
    if (stageFiles.empty()) {
        throw CliError("no stage JSON files found under data/levels");
    }

    std::filesystem::path selectedPath = args.size() >= 2 ? ResolveLevelFilePath(args[1]) : stageFiles.front();
    if (std::find(stageFiles.begin(), stageFiles.end(), selectedPath) == stageFiles.end() &&
        std::filesystem::exists(selectedPath)) {
        stageFiles.push_back(selectedPath);
        std::sort(stageFiles.begin(), stageFiles.end());
    }

    StageModel stage = LoadStageModel(selectedPath);
    std::shared_ptr<Util::Image> background;
    int selectedStageIndex = 0;
    for (std::size_t i = 0; i < stageFiles.size(); ++i) {
        if (stageFiles[i] == selectedPath) selectedStageIndex = static_cast<int>(i);
    }
    int selectedX = 0;
    int selectedY = 0;
    Corner selectedCorner = Corner::TopLeft;
    std::optional<Corner> draggingCorner;
    bool wasLeftDown = false;
    bool wasRightDown = false;
    bool dirty = false;
    std::string status = "Ready";

    auto context = Core::Context::GetInstance();

    auto reloadBackground = [&]() {
        background.reset();
        if (!stage.backgroundPath.empty() && std::filesystem::exists(stage.backgroundPath)) {
            background = std::make_shared<Util::Image>(stage.backgroundPath);
        }
    };
    reloadBackground();

    while (!context->GetExit()) {
        context->Setup();
        ImGuiIO& io = ImGui::GetIO();
        const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
        if ((keyboardState != nullptr && keyboardState[SDL_SCANCODE_ESCAPE] != 0) ||
            Util::Input::IfExit()) {
            context->SetExit(true);
        }

        ImGui::SetNextWindowPos({12.0F, 12.0F}, ImGuiCond_Once);
        ImGui::SetNextWindowSize({360.0F, 620.0F}, ImGuiCond_Once);
        ImGui::Begin("Arknight Builder - Coordinate Calibration");

        if (ImGui::BeginCombo("Stage", stage.path.lexically_relative(ResolveDataRoot() / "levels").generic_string().c_str())) {
            for (int i = 0; i < static_cast<int>(stageFiles.size()); ++i) {
                const std::string label = stageFiles[static_cast<std::size_t>(i)]
                    .lexically_relative(ResolveDataRoot() / "levels").generic_string();
                if (ImGui::Selectable(label.c_str(), selectedStageIndex == i)) {
                    selectedStageIndex = i;
                    stage = LoadStageModel(stageFiles[static_cast<std::size_t>(i)]);
                    selectedX = std::clamp(selectedX, 0, std::max(0, stage.width - 1));
                    selectedY = std::clamp(selectedY, 0, std::max(0, stage.height - 1));
                    reloadBackground();
                    draggingCorner.reset();
                    dirty = false;
                    status = "Loaded " + label;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Text("Stage size: %dx%d", stage.width, stage.height);
        ImGui::InputInt("Cell X", &selectedX);
        ImGui::InputInt("Cell Y", &selectedY);
        selectedX = std::clamp(selectedX, 0, std::max(0, stage.width - 1));
        selectedY = std::clamp(selectedY, 0, std::max(0, stage.height - 1));
        ImGui::Text("Tile: %s", TileAt(stage, selectedX, selectedY).c_str());
        if (!IsMappableTile(stage, selectedX, selectedY)) {
            ImGui::TextColored({1.0F, 0.42F, 0.18F, 1.0F}, "empty tiles are intentionally not mapped.");
        }

        const char* cornerLabels[] = {"Left Top", "Right Top", "Left Bottom", "Right Bottom"};
        int cornerIndex = static_cast<int>(selectedCorner);
        if (ImGui::Combo("Corner", &cornerIndex, cornerLabels, 4)) {
            selectedCorner = static_cast<Corner>(cornerIndex);
        }

        if (ImGui::Button("Save")) {
            PruneUnmappableCellOverrides(stage);
            SaveJson(stage.path, stage.doc);
            dirty = false;
            status = "Saved " + stage.path.string();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload")) {
            stage = LoadStageModel(stage.path);
            reloadBackground();
            draggingCorner.reset();
            dirty = false;
            status = "Reloaded";
        }
        ImGui::Text("Drag yellow point: move corner");
        ImGui::Text("Click map: set selected corner");
        ImGui::Text("Right click map: select hovered cell");
        ImGui::Text("Red dot: enemy center");
        ImGui::Text("Green dot: operator position");
        ImGui::Text("Status: %s%s", status.c_str(), dirty ? " *" : "");
        ImGui::End();

        const ImVec2 viewportMin{390.0F, 18.0F};
        const ImVec2 viewportMax{io.DisplaySize.x - 18.0F, io.DisplaySize.y - 18.0F};
        Canvas canvas;
        canvas.refSize = stage.referenceSize;
        const float availW = std::max(1.0F, viewportMax.x - viewportMin.x);
        const float availH = std::max(1.0F, viewportMax.y - viewportMin.y);
        canvas.scale = std::min(availW / canvas.refSize.x, availH / canvas.refSize.y);
        const ImVec2 size{canvas.refSize.x * canvas.scale, canvas.refSize.y * canvas.scale};
        canvas.min = {viewportMin.x + (availW - size.x) * 0.5F, viewportMin.y + (availH - size.y) * 0.5F};
        canvas.max = {canvas.min.x + size.x, canvas.min.y + size.y};

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        draw->AddRectFilled(viewportMin, viewportMax, IM_COL32(12, 14, 18, 255));
        draw->AddRectFilled(canvas.min, canvas.max, IM_COL32(22, 25, 30, 255));
        DrawImageCover(draw, background, canvas);

        int mouseX = 0;
        int mouseY = 0;
        const Uint32 mouseButtons = SDL_GetMouseState(&mouseX, &mouseY);
        const bool leftDown = (mouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
        const bool rightDown = (mouseButtons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
        const bool leftPressed = leftDown && !wasLeftDown;
        const bool rightPressed = rightDown && !wasRightDown;
        const bool leftReleased = !leftDown && wasLeftDown;
        const ImVec2 mouse{static_cast<float>(mouseX), static_cast<float>(mouseY)};
        const bool mouseOnCanvas = canvas.Contains(mouse);
        std::optional<glm::ivec2> hovered;
        std::optional<Corner> hoveredCorner;
        Vec2 refMouse{};
        if (mouseOnCanvas || draggingCorner) {
            refMouse = ClampedReferencePoint(canvas, mouse);
        }
        if (mouseOnCanvas) {
            hovered = HoveredCell(stage, refMouse);
            hoveredCorner = HoveredCornerHandle(stage, canvas, selectedX, selectedY, mouse);
        }
        DrawStageOverlay(draw, stage, canvas, selectedX, selectedY, hovered, hoveredCorner, draggingCorner);

        if (mouseOnCanvas) {
            DrawDashedLine(draw, {canvas.min.x, mouse.y}, {canvas.max.x, mouse.y}, IM_COL32(255, 255, 255, 135));
            DrawDashedLine(draw, {mouse.x, canvas.min.y}, {mouse.x, canvas.max.y}, IM_COL32(255, 255, 255, 135));
            if (rightPressed && hovered) {
                selectedX = hovered->x;
                selectedY = hovered->y;
                draggingCorner.reset();
            }
            if (leftPressed && hoveredCorner) {
                draggingCorner = hoveredCorner;
                selectedCorner = *hoveredCorner;
            } else if (leftPressed) {
                if (IsMappableTile(stage, selectedX, selectedY)) {
                    const int updated = SetConnectedCellCorner(stage, selectedX, selectedY, selectedCorner, refMouse);
                    dirty = true;
                    status = "Set " + std::string(CornerKey(selectedCorner)) +
                             " for cell (" + std::to_string(selectedX) + "," + std::to_string(selectedY) +
                             "), linked corners: " + std::to_string(updated);
                }
            }
        }

        if (draggingCorner && leftDown) {
            if (IsMappableTile(stage, selectedX, selectedY)) {
                const int updated = SetConnectedCellCorner(stage, selectedX, selectedY, *draggingCorner, refMouse);
                dirty = true;
                status = "Dragging " + std::string(CornerLabel(*draggingCorner)) +
                         " for cell (" + std::to_string(selectedX) + "," + std::to_string(selectedY) +
                         "), linked corners: " + std::to_string(updated);
            }
        }
        if (draggingCorner && leftReleased) {
            status = "Moved " + std::string(CornerLabel(*draggingCorner)) +
                     " for cell (" + std::to_string(selectedX) + "," + std::to_string(selectedY) + ")";
            draggingCorner.reset();
        }

        wasLeftDown = leftDown;
        wasRightDown = rightDown;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        context->Update();
        if (Util::Input::IfExit()) {
            context->SetExit(true);
        }
    }

    if (dirty) {
        std::cout << "Calibration has unsaved changes; use Save before closing to persist edits.\n";
    }
}

} // namespace ark_builder
