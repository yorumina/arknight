#include "commands.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include <glm/glm.hpp>

#include "Core/Context.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
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

enum class Corner { TopLeft = 0, TopRight = 1, BottomRight = 2, BottomLeft = 3 };

struct MenuButton {
    std::string id;
    std::string label;
    std::string stage;
    std::array<Vec2, 4> corners{};
};

constexpr Vec2 kDefaultReferenceSize{2796.0F, 1290.0F};

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

auto ResolveLoadingPageImagePath(const std::vector<std::string>& args) -> std::filesystem::path {
    const std::string imageName = args.size() >= 2 ? args[1] : "loadingpage_3.png";
    std::filesystem::path imagePath = imageName;
    if (!imagePath.is_absolute()) {
        imagePath = ResolveDataRoot() / "loadingpage" / imagePath;
    }
    imagePath = imagePath.lexically_normal();
    if (!std::filesystem::exists(imagePath)) {
        throw CliError("loading page image not found: " + imagePath.string());
    }
    return imagePath;
}

auto ResolveMenuConfigPath() -> std::filesystem::path {
    auto dir = ResolveDataRoot() / "loadingpage";
    std::filesystem::create_directories(dir);
    return (dir / "menu_buttons.json").lexically_normal();
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

auto DefaultButtons(const Vec2& imageSize) -> std::array<MenuButton, 2> {
    constexpr std::array<Vec2, 4> stage11{
        Vec2{955.0F, 555.0F},
        Vec2{1325.0F, 555.0F},
        Vec2{1325.0F, 690.0F},
        Vec2{955.0F, 690.0F},
    };
    constexpr std::array<Vec2, 4> stage12{
        Vec2{1430.0F, 540.0F},
        Vec2{1735.0F, 540.0F},
        Vec2{1735.0F, 675.0F},
        Vec2{1430.0F, 675.0F},
    };

    return {
        MenuButton{"stage_1_1", "Operation 1-1", "Operation 1-1/stage", ScaleQuad(stage11, kDefaultReferenceSize, imageSize)},
        MenuButton{"stage_1_2", "Operation 1-2", "Operation 1-2/stage", ScaleQuad(stage12, kDefaultReferenceSize, imageSize)},
    };
}

auto LoadButtons(const std::filesystem::path& configPath, const Vec2& imageSize) -> std::array<MenuButton, 2> {
    auto buttons = DefaultButtons(imageSize);
    if (!std::filesystem::exists(configPath)) return buttons;

    json doc = LoadJson(configPath);
    if (!doc.is_object() || !doc.contains("buttons") || !doc["buttons"].is_array()) return buttons;

    Vec2 referenceSize = imageSize;
    if (doc.contains("reference_size") && doc["reference_size"].is_array() &&
        doc["reference_size"].size() == 2 && doc["reference_size"][0].is_number() &&
        doc["reference_size"][1].is_number()) {
        referenceSize = {doc["reference_size"][0].get<float>(), doc["reference_size"][1].get<float>()};
    }
    if (referenceSize.x <= 0.0F || referenceSize.y <= 0.0F) referenceSize = imageSize;

    const auto& jsonButtons = doc["buttons"];
    for (std::size_t i = 0; i < jsonButtons.size() && i < buttons.size(); ++i) {
        const auto& item = jsonButtons[i];
        if (!item.is_object()) continue;
        const auto topLeft = PointFromJson(item, "top_left");
        const auto topRight = PointFromJson(item, "top_right");
        const auto bottomRight = PointFromJson(item, "bottom_right");
        const auto bottomLeft = PointFromJson(item, "bottom_left");
        if (!topLeft || !topRight || !bottomRight || !bottomLeft) continue;

        buttons[i].id = item.value("id", buttons[i].id);
        buttons[i].label = item.value("label", buttons[i].label);
        buttons[i].stage = item.value("stage", buttons[i].stage);
        const std::array<Vec2, 4> raw{*topLeft, *topRight, *bottomRight, *bottomLeft};
        buttons[i].corners = ScaleQuad(raw, referenceSize, imageSize);
    }
    return buttons;
}

void SaveButtons(const std::filesystem::path& configPath,
                 const std::array<MenuButton, 2>& buttons,
                 const Vec2& imageSize) {
    json doc = json::object();
    if (std::filesystem::exists(configPath)) {
        try {
            doc = LoadJson(configPath);
            if (!doc.is_object()) doc = json::object();
        } catch (const std::exception&) {
            doc = json::object();
        }
    }

    doc["reference_size"] = json::array({imageSize.x, imageSize.y});
    doc["buttons"] = json::array();
    for (const auto& button : buttons) {
        doc["buttons"].push_back({
            {"id", button.id},
            {"label", button.label},
            {"stage", button.stage},
            {"top_left", PointToJson(button.corners[0])},
            {"top_right", PointToJson(button.corners[1])},
            {"bottom_right", PointToJson(button.corners[2])},
            {"bottom_left", PointToJson(button.corners[3])},
        });
    }
    SaveJson(configPath, doc);
}

auto DistanceSq(ImVec2 a, ImVec2 b) -> float {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

auto Cross(const Vec2& a, const Vec2& b, const Vec2& p) -> float {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

auto PointInTriangle(const Vec2& p, const Vec2& a, const Vec2& b, const Vec2& c) -> bool {
    const float c1 = Cross(a, b, p);
    const float c2 = Cross(b, c, p);
    const float c3 = Cross(c, a, p);
    const bool hasNeg = c1 < -0.001F || c2 < -0.001F || c3 < -0.001F;
    const bool hasPos = c1 > 0.001F || c2 > 0.001F || c3 > 0.001F;
    return !(hasNeg && hasPos);
}

auto PointInQuad(const Vec2& p, const std::array<Vec2, 4>& q) -> bool {
    return PointInTriangle(p, q[0], q[1], q[2]) || PointInTriangle(p, q[0], q[2], q[3]);
}

auto HoveredButton(const std::array<MenuButton, 2>& buttons, const Vec2& refPoint) -> std::optional<int> {
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        if (PointInQuad(refPoint, buttons[i].corners)) return static_cast<int>(i);
    }
    return std::nullopt;
}

auto HoveredCorner(const MenuButton& button, const Canvas& canvas, ImVec2 mouse) -> std::optional<Corner> {
    constexpr float hitRadius = 13.0F;
    float bestDistanceSq = hitRadius * hitRadius;
    std::optional<Corner> best;
    for (std::size_t i = 0; i < button.corners.size(); ++i) {
        const ImVec2 handle = canvas.ToScreen(button.corners[i]);
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

void DrawButtons(ImDrawList* draw, const std::array<MenuButton, 2>& buttons, const Canvas& canvas,
                 int selectedButton, std::optional<int> hoveredButton,
                 std::optional<Corner> hoveredCorner, std::optional<Corner> activeCorner) {
    for (std::size_t i = 0; i < buttons.size(); ++i) {
        const auto& button = buttons[i];
        const std::array<ImVec2, 4> q{
            canvas.ToScreen(button.corners[0]),
            canvas.ToScreen(button.corners[1]),
            canvas.ToScreen(button.corners[2]),
            canvas.ToScreen(button.corners[3]),
        };
        const bool selected = static_cast<int>(i) == selectedButton;
        const bool hovered = hoveredButton && *hoveredButton == static_cast<int>(i);
        const ImU32 fill = selected ? IM_COL32(255, 180, 48, 70)
                         : hovered ? IM_COL32(92, 210, 255, 54)
                                   : IM_COL32(80, 180, 255, 24);
        const ImU32 line = selected ? IM_COL32(255, 195, 64, 250)
                         : hovered ? IM_COL32(116, 225, 255, 230)
                                   : IM_COL32(160, 210, 255, 125);
        draw->AddQuadFilled(q[0], q[1], q[2], q[3], fill);
        draw->AddQuad(q[0], q[1], q[2], q[3], line, selected ? 2.8F : 1.4F);
        draw->AddText({q[0].x + 8.0F, q[0].y - 20.0F}, line, button.label.c_str());
    }

    const auto& selected = buttons[static_cast<std::size_t>(selectedButton)];
    for (std::size_t i = 0; i < selected.corners.size(); ++i) {
        const Corner corner = static_cast<Corner>(i);
        const bool active = activeCorner && *activeCorner == corner;
        const bool hovered = hoveredCorner && *hoveredCorner == corner;
        const ImVec2 p = canvas.ToScreen(selected.corners[i]);
        const float radius = active ? 8.0F : hovered ? 7.0F : 5.5F;
        const ImU32 fill = active ? IM_COL32(255, 160, 34, 255)
                         : hovered ? IM_COL32(255, 248, 132, 255)
                                   : IM_COL32(255, 230, 70, 245);
        draw->AddCircleFilled(p, radius, fill);
        draw->AddCircle(p, radius + 3.0F, IM_COL32(20, 20, 20, 245), 20, 1.4F);
    }
}

} // namespace

void RunMenuCalibrate(const std::vector<std::string>& args) {
    const auto imagePath = ResolveLoadingPageImagePath(args);
    const auto configPath = ResolveMenuConfigPath();

    auto context = Core::Context::GetInstance();
    auto image = std::make_shared<Util::Image>(imagePath.string());
    const glm::vec2 rawImageSize = image->GetSize();
    const Vec2 imageSize{
        std::max(1.0F, rawImageSize.x),
        std::max(1.0F, rawImageSize.y)
    };

    auto buttons = LoadButtons(configPath, imageSize);
    int selectedButton = 0;
    Corner selectedCorner = Corner::TopLeft;
    std::optional<Corner> draggingCorner;
    bool wasLeftDown = false;
    bool wasRightDown = false;
    bool dirty = false;
    std::string status = std::filesystem::exists(configPath)
        ? "Loaded " + configPath.string()
        : "Ready with default button quads";

    while (!context->GetExit()) {
        context->Setup();
        ImGuiIO& io = ImGui::GetIO();
        const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
        if ((keyboardState != nullptr && keyboardState[SDL_SCANCODE_ESCAPE] != 0) ||
            Util::Input::IfExit()) {
            context->SetExit(true);
        }

        ImGui::SetNextWindowPos({12.0F, 12.0F}, ImGuiCond_Once);
        ImGui::SetNextWindowSize({360.0F, 520.0F}, ImGuiCond_Once);
        ImGui::Begin("Arknight Builder - Menu Buttons");

        if (ImGui::BeginCombo("Button", buttons[static_cast<std::size_t>(selectedButton)].label.c_str())) {
            for (int i = 0; i < static_cast<int>(buttons.size()); ++i) {
                if (ImGui::Selectable(buttons[static_cast<std::size_t>(i)].label.c_str(), selectedButton == i)) {
                    selectedButton = i;
                    draggingCorner.reset();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Text("Stage: %s", buttons[static_cast<std::size_t>(selectedButton)].stage.c_str());
        ImGui::Text("Image: %.0fx%.0f", imageSize.x, imageSize.y);

        const char* cornerLabels[] = {"Top Left", "Top Right", "Bottom Right", "Bottom Left"};
        int cornerIndex = static_cast<int>(selectedCorner);
        if (ImGui::Combo("Corner", &cornerIndex, cornerLabels, 4)) {
            selectedCorner = static_cast<Corner>(cornerIndex);
        }

        if (ImGui::Button("Save")) {
            SaveButtons(configPath, buttons, imageSize);
            dirty = false;
            status = "Saved " + configPath.string();
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload")) {
            buttons = LoadButtons(configPath, imageSize);
            draggingCorner.reset();
            dirty = false;
            status = "Reloaded";
        }

        ImGui::Text("Drag yellow point: move corner");
        ImGui::Text("Click map: set selected corner");
        ImGui::Text("Right click map: select hovered button");
        ImGui::Text("Status: %s%s", status.c_str(), dirty ? " *" : "");
        ImGui::End();

        const ImVec2 viewportMin{390.0F, 18.0F};
        const ImVec2 viewportMax{io.DisplaySize.x - 18.0F, io.DisplaySize.y - 18.0F};
        Canvas canvas;
        canvas.refSize = imageSize;
        const float availW = std::max(1.0F, viewportMax.x - viewportMin.x);
        const float availH = std::max(1.0F, viewportMax.y - viewportMin.y);
        canvas.scale = std::min(availW / canvas.refSize.x, availH / canvas.refSize.y);
        const ImVec2 size{canvas.refSize.x * canvas.scale, canvas.refSize.y * canvas.scale};
        canvas.min = {viewportMin.x + (availW - size.x) * 0.5F, viewportMin.y + (availH - size.y) * 0.5F};
        canvas.max = {canvas.min.x + size.x, canvas.min.y + size.y};

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        draw->AddRectFilled(viewportMin, viewportMax, IM_COL32(12, 14, 18, 255));
        draw->AddRectFilled(canvas.min, canvas.max, IM_COL32(22, 25, 30, 255));
        DrawImageCover(draw, image, canvas);

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

        std::optional<int> hoveredButton;
        std::optional<Corner> hoveredCorner;
        Vec2 refMouse{};
        if (mouseOnCanvas || draggingCorner) {
            refMouse = ClampedReferencePoint(canvas, mouse);
        }
        if (mouseOnCanvas) {
            hoveredButton = HoveredButton(buttons, refMouse);
            hoveredCorner = HoveredCorner(buttons[static_cast<std::size_t>(selectedButton)], canvas, mouse);
        }

        DrawButtons(draw, buttons, canvas, selectedButton, hoveredButton, hoveredCorner, draggingCorner);

        if (mouseOnCanvas) {
            draw->AddLine({canvas.min.x, mouse.y}, {canvas.max.x, mouse.y}, IM_COL32(255, 255, 255, 95), 1.0F);
            draw->AddLine({mouse.x, canvas.min.y}, {mouse.x, canvas.max.y}, IM_COL32(255, 255, 255, 95), 1.0F);

            if (rightPressed && hoveredButton) {
                selectedButton = *hoveredButton;
                draggingCorner.reset();
            }
            if (leftPressed && hoveredCorner) {
                draggingCorner = hoveredCorner;
                selectedCorner = *hoveredCorner;
            } else if (leftPressed) {
                buttons[static_cast<std::size_t>(selectedButton)]
                    .corners[static_cast<std::size_t>(selectedCorner)] = refMouse;
                dirty = true;
                status = "Set " + std::string(CornerKey(selectedCorner)) +
                         " for " + buttons[static_cast<std::size_t>(selectedButton)].label;
            }
        }

        if (draggingCorner && leftDown) {
            buttons[static_cast<std::size_t>(selectedButton)]
                .corners[static_cast<std::size_t>(*draggingCorner)] = refMouse;
            dirty = true;
            status = "Dragging " + std::string(CornerLabel(*draggingCorner)) +
                     " for " + buttons[static_cast<std::size_t>(selectedButton)].label;
        }
        if (draggingCorner && leftReleased) {
            status = "Moved " + std::string(CornerLabel(*draggingCorner)) +
                     " for " + buttons[static_cast<std::size_t>(selectedButton)].label;
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
        std::cout << "Menu button calibration has unsaved changes; use Save before closing to persist edits.\n";
    }
}

} // namespace ark_builder
