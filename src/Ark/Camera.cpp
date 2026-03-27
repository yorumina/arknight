#include "App.hpp"
#include "config.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "Util/Input.hpp"
#include "Util/Keycode.hpp"

using namespace Ark;

namespace {
constexpr float CAMERA_EPSILON = 0.0001F;
constexpr float KEYBOARD_PAN_SPEED = 780.0F;
constexpr float ZOOM_STEP = 1.10F;
}

void App::ResetCameraToStageDefaults() {
    m_Camera.zoom = std::clamp(m_Camera.defaultZoom, m_Camera.minZoom, m_Camera.maxZoom);
    m_Camera.pan = m_Camera.defaultPan;
    m_Camera.dragging = false;
}

glm::vec2 App::RawCursorToPtsd(const glm::vec2& rawCursor) const {
    const float sx = std::max(CAMERA_EPSILON, m_Camera.projectionScaleX * m_Camera.zoom);
    const float sy = std::max(CAMERA_EPSILON, m_Camera.projectionScaleY * m_Camera.zoom);
    return {rawCursor.x / sx - m_Camera.pan.x,
            rawCursor.y / sy - m_Camera.pan.y};
}

void App::UpdateCameraControls(float deltaTimeMs, const glm::vec2& rawCursor) {
    if (Util::Input::IsKeyDown(Util::Keycode::C)) {
        ResetCameraToStageDefaults();
    }

    const float dtSec = std::max(0.0F, deltaTimeMs) / 1000.0F;
    glm::vec2 panRawDelta{0.0F, 0.0F};
    if (Util::Input::IsKeyPressed(Util::Keycode::A) || Util::Input::IsKeyPressed(Util::Keycode::LEFT)) {
        panRawDelta.x -= KEYBOARD_PAN_SPEED * dtSec;
    }
    if (Util::Input::IsKeyPressed(Util::Keycode::D) || Util::Input::IsKeyPressed(Util::Keycode::RIGHT)) {
        panRawDelta.x += KEYBOARD_PAN_SPEED * dtSec;
    }
    if (Util::Input::IsKeyPressed(Util::Keycode::W) || Util::Input::IsKeyPressed(Util::Keycode::UP)) {
        panRawDelta.y += KEYBOARD_PAN_SPEED * dtSec;
    }
    if (Util::Input::IsKeyPressed(Util::Keycode::S) || Util::Input::IsKeyPressed(Util::Keycode::DOWN)) {
        panRawDelta.y -= KEYBOARD_PAN_SPEED * dtSec;
    }

    {
        const float sx = std::max(CAMERA_EPSILON, m_Camera.projectionScaleX * m_Camera.zoom);
        const float sy = std::max(CAMERA_EPSILON, m_Camera.projectionScaleY * m_Camera.zoom);
        m_Camera.pan.x += panRawDelta.x / sx;
        m_Camera.pan.y += panRawDelta.y / sy;
    }

    if (Util::Input::IsKeyDown(Util::Keycode::MOUSE_MB)) {
        m_Camera.dragging = true;
        m_Camera.dragCursorStart = rawCursor;
        m_Camera.dragPanStart = m_Camera.pan;
    }
    if (m_Camera.dragging && Util::Input::IsKeyPressed(Util::Keycode::MOUSE_MB)) {
        const glm::vec2 rawDelta = rawCursor - m_Camera.dragCursorStart;
        const float sx = std::max(CAMERA_EPSILON, m_Camera.projectionScaleX * m_Camera.zoom);
        const float sy = std::max(CAMERA_EPSILON, m_Camera.projectionScaleY * m_Camera.zoom);
        m_Camera.pan.x = m_Camera.dragPanStart.x + rawDelta.x / sx;
        m_Camera.pan.y = m_Camera.dragPanStart.y + rawDelta.y / sy;
    }
    if (Util::Input::IsKeyUp(Util::Keycode::MOUSE_MB)) {
        m_Camera.dragging = false;
    }

    if (Util::Input::IfScroll()) {
        const float scrollY = Util::Input::GetScrollDistance().y;
        if (std::abs(scrollY) > CAMERA_EPSILON) {
            const glm::vec2 anchorBefore = RawCursorToPtsd(rawCursor);
            m_Camera.zoom = std::clamp(
                m_Camera.zoom * std::pow(ZOOM_STEP, scrollY),
                m_Camera.minZoom,
                m_Camera.maxZoom);

            const float sx = std::max(CAMERA_EPSILON, m_Camera.projectionScaleX * m_Camera.zoom);
            const float sy = std::max(CAMERA_EPSILON, m_Camera.projectionScaleY * m_Camera.zoom);
            m_Camera.pan.x = rawCursor.x / sx - anchorBefore.x;
            m_Camera.pan.y = rawCursor.y / sy - anchorBefore.y;
        }
    }
}

ImVec2 App::ToScreenPosition(const glm::vec2& p) const {
    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const float sx = m_Camera.projectionScaleX * m_Camera.zoom;
    const float sy = m_Camera.projectionScaleY * m_Camera.zoom;
    return {W * 0.5F + (p.x + m_Camera.pan.x) * sx,
            H * 0.5F - (p.y + m_Camera.pan.y) * sy};
}

BoardLayout App::GetBoardLayout() const {
    if (m_HasBoardLayoutOverride) {
        return m_BoardLayoutOverride;
    }

    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);

    const float hudRW = 362.0F;
    const float marginL = 24.0F;
    const float marginR = 22.0F;
    const float marginT = 46.0F;
    const float marginB = 28.0F;
    const float availW = std::max(220.0F, W - hudRW - marginL - marginR);
    const float availH = std::max(220.0F, H - marginT - marginB);

    const float cols = static_cast<float>(std::max(1, m_StageWidth));
    const float rows = static_cast<float>(std::max(1, m_StageHeight));
    const float projX = std::max(CAMERA_EPSILON, m_Camera.projectionScaleX);
    const float projY = std::max(CAMERA_EPSILON, m_Camera.projectionScaleY);
    const float cellSizeRaw = std::floor(std::min(availW / (cols * projX), availH / (rows * projY)));

    BoardLayout layout;
    layout.cellSize = std::clamp(cellSizeRaw, 24.0F, 120.0F);
    layout.topLeftX = -(cols * 0.5F) * layout.cellSize;
    layout.topLeftY = (rows * 0.5F) * layout.cellSize;
    return layout;
}

glm::vec2 App::ToPtsdPosition(const glm::vec2& boardPos) const {
    const auto layout = GetBoardLayout();
    return {layout.topLeftX + boardPos.x * layout.cellSize,
            layout.topLeftY - boardPos.y * layout.cellSize};
}

glm::vec2 App::ToBoardCenter(const glm::ivec2& cell) const {
    return {static_cast<float>(cell.x) + 0.5F, static_cast<float>(cell.y) + 0.5F};
}

std::optional<glm::ivec2> App::ToCell(const glm::vec2& p) const {
    if (!IsInsideBoard(p)) return std::nullopt;
    const auto layout = GetBoardLayout();
    const int col = static_cast<int>((p.x - layout.topLeftX) / layout.cellSize);
    const int row = static_cast<int>((layout.topLeftY - p.y) / layout.cellSize);
    if (col < 0 || col >= m_StageWidth || row < 0 || row >= m_StageHeight) return std::nullopt;
    return glm::ivec2(col, row);
}

bool App::IsInsideBoard(const glm::vec2& p) const {
    const auto layout = GetBoardLayout();
    return p.x >= layout.topLeftX && p.x < layout.topLeftX + m_StageWidth * layout.cellSize &&
           p.y <= layout.topLeftY && p.y > layout.topLeftY - m_StageHeight * layout.cellSize;
}
