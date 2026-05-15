#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "RendererShared.hpp"
#include "config.hpp"

#include <algorithm>
#include <cmath>

using namespace Ark;
using namespace Ark::RendererConst;

void Ark::AppRenderer::DrawScene(const glm::vec2& cursor) {
    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const auto layout = m_App.GetBoardLayout();
    auto drawGameplayFrame = [&]() {
        DrawStageBackground();
        DrawGrid();
        DrawEnemies(layout);
        DrawOperators(layout, false);
        DrawHighgroundTopLayer();
        DrawMarkerTopLayer();
        DrawOperators(layout, true);
        DrawBeams();
        DrawDeployPreview(cursor, m_App.ToCell(cursor), layout);
        DrawHUD(W);
        DrawOperatorBar(W, H);
        DrawDeploymentInfo(W, H);
        
        // Draw Operator Details panel
        if (m_App.m_DraggingFromBar || m_App.m_WaitingForDirection) {
            if (m_App.m_DragOperatorType >= 0) {
                const auto& t = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(m_App.m_DragOperatorType));
                DrawOperatorDetails(t, nullptr, H);
            }
        } else if (m_App.m_SelectedOperatorId != -1) {
            const Operator* op = nullptr;
            for (auto& o : m_App.m_Operators) {
                if (o.id == m_App.m_SelectedOperatorId) { op = &o; break; }
            }
            if (op) {
                const auto& t = m_App.m_OperatorTemplates.at(static_cast<std::size_t>(op->typeIndex));
                DrawOperatorDetails(t, op, H);
            }
        }
    };

    if (m_App.m_PreStageWaiting && !m_App.m_StageLoadingPath.empty()) {
        const float t = std::clamp(m_App.m_PreStageTimerMs, 0.0F, PRE_STAGE_TOTAL_MS);
        const float fadeIn = std::clamp(t / PRE_STAGE_FADE_MS, 0.0F, 1.0F);
        const float fadeOut = std::clamp((PRE_STAGE_TOTAL_MS - t) / PRE_STAGE_FADE_MS, 0.0F, 1.0F);
        DrawImageCover(m_App.m_StageLoadingPath, m_App.m_StageLoadingAlpha * std::min(fadeIn, fadeOut), true);
        return;
    }

    if (m_App.m_MissionClear && !m_App.m_StageFinishPath.empty()) {
        const float t = std::max(0.0F, m_App.m_ClearTimerMs);
        if (t < FINISH_FADE_TO_BLACK_MS + FINISH_BLACKOUT_MS) {
            drawGameplayFrame();
            float blackAlpha = 1.0F;
            if (t < FINISH_FADE_TO_BLACK_MS) {
                blackAlpha = std::clamp(t / FINISH_FADE_TO_BLACK_MS, 0.0F, 1.0F);
            }
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                {0.0F, 0.0F},
                {static_cast<float>(PTSD_Config::WINDOW_WIDTH), static_cast<float>(PTSD_Config::WINDOW_HEIGHT)},
                IM_COL32(0, 0, 0, static_cast<int>(blackAlpha * 255.0F))
            );
            return;
        }

        float alpha = std::clamp((t - FINISH_FADE_TO_BLACK_MS - FINISH_BLACKOUT_MS) / FINISH_FADE_IN_MS, 0.0F, 1.0F);
        if (m_App.m_FinishExitRequested) {
            alpha = std::min(alpha, 1.0F - std::clamp(m_App.m_FinishExitTimerMs / FINISH_FADE_OUT_MS, 0.0F, 1.0F));
        }
        DrawImageCover(m_App.m_StageFinishPath, m_App.m_StageFinishAlpha * alpha, true);
        return;
    }

    drawGameplayFrame();
}

void Ark::AppRenderer::DrawStageBackground() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float screenW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float screenH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);

    draw->AddRectFilledMultiColor(
        {0.0F, 0.0F},
        {screenW, screenH},
        IM_COL32(18, 22, 28, 255),
        IM_COL32(22, 27, 34, 255),
        IM_COL32(8, 10, 14, 255),
        IM_COL32(12, 15, 20, 255)
    );

    if (!m_App.m_StageBackgroundPath.empty()) {
        DrawImageCover(m_App.m_StageBackgroundPath, m_App.m_StageBackgroundAlpha, false);
        draw->AddRectFilled({0.0F, 0.0F}, {screenW, screenH}, IM_COL32(4, 6, 10, 70));
        return;
    }

    const auto layout = m_App.GetBoardLayout();
    const float margin = layout.cellSize * 1.35F;
    const float l = layout.topLeftX - margin;
    const float r = layout.topLeftX + static_cast<float>(m_App.m_StageWidth) * layout.cellSize + margin;
    const float t = layout.topLeftY + margin;
    const float b = layout.topLeftY - static_cast<float>(m_App.m_StageHeight) * layout.cellSize - margin;
    const ImVec2 q1 = m_App.ToScreenPosition({l, t});
    const ImVec2 q2 = m_App.ToScreenPosition({r, t});
    const ImVec2 q3 = m_App.ToScreenPosition({r, b});
    const ImVec2 q4 = m_App.ToScreenPosition({l, b});

    draw->AddQuadFilled(q1, q2, q3, q4, IM_COL32(39, 45, 52, 235));
    draw->AddQuad(q1, q2, q3, q4, IM_COL32(95, 107, 122, 120), 1.4F);

    for (int i = -2; i <= m_App.m_StageWidth + 2; i += 2) {
        const float x = layout.topLeftX + static_cast<float>(i) * layout.cellSize;
        const ImVec2 a = m_App.ToScreenPosition({x, t});
        const ImVec2 c = m_App.ToScreenPosition({x, b});
        draw->AddLine(a, c, IM_COL32(20, 24, 30, 125), 2.0F);
    }
    for (int i = -2; i <= m_App.m_StageHeight + 2; i += 2) {
        const float y = layout.topLeftY - static_cast<float>(i) * layout.cellSize;
        const ImVec2 a = m_App.ToScreenPosition({l, y});
        const ImVec2 c = m_App.ToScreenPosition({r, y});
        draw->AddLine(a, c, IM_COL32(76, 86, 98, 72), 1.4F);
    }

    draw->AddRectFilledMultiColor(
        {0.0F, 0.0F},
        {screenW, screenH},
        IM_COL32(0, 0, 0, 90),
        IM_COL32(0, 0, 0, 90),
        IM_COL32(0, 0, 0, 135),
        IM_COL32(0, 0, 0, 135)
    );
}

void Ark::AppRenderer::DrawImageCover(const std::string& imagePath, float alpha, bool drawBlackFill) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float screenW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float screenH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);

    if (drawBlackFill) {
        draw->AddRectFilled({0.0F, 0.0F}, {screenW, screenH}, IM_COL32(0, 0, 0, 255));
    }

    if (imagePath != m_App.m_StageOverlayLoadedPath) {
        m_App.m_StageBackground.reset();
        m_App.m_StageOverlayLoadedPath = imagePath;
    }

    if (m_App.m_StageBackground == nullptr && !imagePath.empty()) {
        m_App.m_StageBackground = std::make_shared<Util::Image>(imagePath);
    }

    if (m_App.m_StageBackground != nullptr && m_App.m_StageBackground->GetTextureId() != 0) {
        const glm::vec2 imageSize = m_App.m_StageBackground->GetSize();
        if (imageSize.x > 0.0F && imageSize.y > 0.0F) {
            // Cover scaling: always fill full screen (crop overflow if needed).
            const float scale = std::max(screenW / imageSize.x, screenH / imageSize.y);
            const float drawW = imageSize.x * scale;
            const float drawH = imageSize.y * scale;
            const float x = (screenW - drawW) * 0.5F;
            const float y = (screenH - drawH) * 0.5F;
            const int imageAlpha = static_cast<int>(std::clamp(alpha, 0.0F, 1.0F) * 255.0F);
            draw->AddImage(
                reinterpret_cast<void*>(static_cast<intptr_t>(m_App.m_StageBackground->GetTextureId())),
                {x, y},
                {x + drawW, y + drawH},
                {0.0F, 0.0F},
                {1.0F, 1.0F},
                IM_COL32(255, 255, 255, imageAlpha)
            );
        }
    }
}

void Ark::AppRenderer::DrawLoadingScreen() {
    DrawImageCover(m_App.m_StageBackgroundPath, m_App.m_StageBackgroundAlpha, true);
}
