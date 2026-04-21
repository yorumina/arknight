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
        DrawGrid();
        DrawOperators(layout, false);
        DrawEnemies(layout);
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

        m_App.m_StageBackgroundPath = m_App.m_StageFinishPath;
        float alpha = std::clamp((t - FINISH_FADE_TO_BLACK_MS - FINISH_BLACKOUT_MS) / FINISH_FADE_IN_MS, 0.0F, 1.0F);
        if (m_App.m_FinishExitRequested) {
            alpha = std::min(alpha, 1.0F - std::clamp(m_App.m_FinishExitTimerMs / FINISH_FADE_OUT_MS, 0.0F, 1.0F));
        }
        m_App.m_StageBackgroundAlpha = m_App.m_StageFinishAlpha * alpha;
        DrawLoadingScreen();
        return;
    }

    drawGameplayFrame();
}

void Ark::AppRenderer::DrawLoadingScreen() {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float screenW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float screenH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);

    draw->AddRectFilled({0.0F, 0.0F}, {screenW, screenH}, IM_COL32(0, 0, 0, 255));

    if (m_App.m_StageBackgroundPath != m_App.m_StageOverlayLoadedPath) {
        m_App.m_StageBackground.reset();
        m_App.m_StageOverlayLoadedPath = m_App.m_StageBackgroundPath;
    }

    if (m_App.m_StageBackground == nullptr && !m_App.m_StageBackgroundPath.empty()) {
        m_App.m_StageBackground = std::make_shared<Util::Image>(m_App.m_StageBackgroundPath);
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
            const int alpha = static_cast<int>(std::clamp(m_App.m_StageBackgroundAlpha, 0.0F, 1.0F) * 255.0F);
            draw->AddImage(
                reinterpret_cast<void*>(static_cast<intptr_t>(m_App.m_StageBackground->GetTextureId())),
                {x, y},
                {x + drawW, y + drawH},
                {0.0F, 0.0F},
                {1.0F, 1.0F},
                IM_COL32(255, 255, 255, alpha)
            );
        }
    }
}
