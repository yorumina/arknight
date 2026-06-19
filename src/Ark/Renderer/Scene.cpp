#include "App.hpp"
#include "Ark/Renderer.hpp"
#include "RendererShared.hpp"
#include "config.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>

using namespace Ark;
using namespace Ark::RendererConst;

namespace {
std::string ResolveLevelPicPath(const std::string& filename) {
    const std::array<std::filesystem::path, 6> bases{
        "data/levels_pic",
        "../data/levels_pic",
        "../../data/levels_pic",
        "../../../data/levels_pic",
        ".",
        "..",
    };
    for (const auto& base : bases) {
        const auto path = base / filename;
        if (std::filesystem::exists(path)) return path.lexically_normal().string();
    }
    return {};
}

std::shared_ptr<Util::Image> LoadOverlayImage(const std::string& imagePath) {
    if (imagePath.empty()) return nullptr;
    static std::map<std::string, std::shared_ptr<Util::Image>> cache;
    const auto key = std::filesystem::path(imagePath).lexically_normal().string();
    if (auto it = cache.find(key); it != cache.end()) {
        return it->second;
    }
    auto image = std::make_shared<Util::Image>(key);
    cache.emplace(key, image);
    return image;
}

void DrawForegroundImageCover(const std::string& imagePath, float alpha) {
    auto image = LoadOverlayImage(imagePath);
    if (!image || image->GetTextureId() == 0) return;

    const float screenW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float screenH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const glm::vec2 imageSize = image->GetSize();
    if (imageSize.x <= 0.0F || imageSize.y <= 0.0F) return;

    const float scale = std::max(screenW / imageSize.x, screenH / imageSize.y);
    const float drawW = imageSize.x * scale;
    const float drawH = imageSize.y * scale;
    const float x = (screenW - drawW) * 0.5F;
    const float y = (screenH - drawH) * 0.5F;
    const int imageAlpha = static_cast<int>(std::clamp(alpha, 0.0F, 1.0F) * 255.0F);

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddImage(
        reinterpret_cast<void*>(static_cast<intptr_t>(image->GetTextureId())),
        {x, y},
        {x + drawW, y + drawH},
        {0.0F, 0.0F},
        {1.0F, 1.0F},
        IM_COL32(255, 255, 255, imageAlpha)
    );
}
} // namespace

void Ark::AppRenderer::DrawScene(const glm::vec2& cursor) {
    const float W = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float H = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);
    const auto layout = m_App.GetBoardLayout();
    auto drawGameplayFrame = [&]() {
        auto drawOperatorDetailsPanel = [&]() {
            if (m_App.m_DraggingFromBar || m_App.m_WaitingForDirection || m_App.m_SelectedOperatorCardType >= 0) {
                if (m_App.m_DragOperatorType >= 0) {
                    DrawOperatorDetails(m_App.m_DragOperatorType, nullptr, H);
                } else if (m_App.m_SelectedOperatorCardType >= 0) {
                    DrawOperatorDetails(m_App.m_SelectedOperatorCardType, nullptr, H);
                }
            } else if (m_App.m_SelectedOperatorId != -1) {
                const Operator* op = nullptr;
                for (auto& o : m_App.m_Operators) {
                    if (o.id == m_App.m_SelectedOperatorId) { op = &o; break; }
                }
                if (op) {
                    DrawOperatorDetails(op->typeIndex, op, H);
                }
            }
        };

        DrawStageBackground();
        if (m_App.m_ShowMapModel) DrawGrid();
        const auto hoverCell = m_App.ToCell(cursor);
        DrawDeployPreview(hoverCell, layout, true);
        DrawEnemies(layout);
        DrawOperators(layout, false);
        if (m_App.m_ShowMapModel) {
            DrawHighgroundTopLayer();
            DrawMarkerTopLayer();
        }
        DrawOperators(layout, true);
        DrawBeams();
        DrawDeployPreview(hoverCell, layout, false);
        DrawOperatorBar(W, H);
        DrawDeploymentInfo(W, H);

        if (m_App.m_ShowQuitConfirm) {
            drawOperatorDetailsPanel();
            DrawHUD(W);
        } else {
            DrawHUD(W);
            drawOperatorDetailsPanel();
        }
    };

    if (m_App.m_PreStageWaiting && !m_App.m_StageLoadingPath.empty()) {
        const float t = std::clamp(m_App.m_PreStageTimerMs, 0.0F, PRE_STAGE_TOTAL_MS);
        const float fadeIn = std::clamp(t / PRE_STAGE_FADE_MS, 0.0F, 1.0F);
        const float fadeOut = std::clamp((PRE_STAGE_TOTAL_MS - t) / PRE_STAGE_FADE_MS, 0.0F, 1.0F);
        DrawImageCover(m_App.m_StageLoadingPath, m_App.m_StageLoadingAlpha * std::min(fadeIn, fadeOut), true);
        return;
    }

    if (m_App.m_MissionClear) {
        const float t = std::max(0.0F, m_App.m_ClearTimerMs);
        drawGameplayFrame();
        if (t < MISSION_COMPLETE_TOTAL_MS) {
            DrawMissionCompleteOverlay(W, H);
            return;
        }

        if (!m_App.m_StageFinishPath.empty()) {
            float alpha = std::clamp((t - MISSION_COMPLETE_TOTAL_MS) / FINISH_FADE_IN_MS, 0.0F, 1.0F);
            if (m_App.m_FinishExitRequested) {
                alpha = std::min(alpha, 1.0F - std::clamp(m_App.m_FinishExitTimerMs / FINISH_FADE_OUT_MS, 0.0F, 1.0F));
            }
            DrawImageCover(m_App.m_StageFinishPath, m_App.m_StageFinishAlpha * alpha, false);
        }
        return;
    }

    if (m_App.m_GameOver) {
        static const std::string levelFailPath = ResolveLevelPicPath("level_fail.png");
        const float alpha = std::clamp(m_App.m_GameOverTimerMs / LEVEL_FAIL_FADE_IN_MS, 0.0F, 1.0F);
        drawGameplayFrame();
        DrawForegroundImageCover(levelFailPath, alpha);
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
        IM_COL32(22, 26, 31, 255),
        IM_COL32(18, 22, 28, 255),
        IM_COL32(8, 10, 14, 255),
        IM_COL32(12, 15, 20, 255)
    );

    const bool hasStageArt = !m_App.m_StageBackgroundPath.empty();
    if (hasStageArt) {
        DrawImageCover(m_App.m_StageBackgroundPath, m_App.m_StageBackgroundAlpha, false);
    }

    const float edgeW = std::max(42.0F, screenW * 0.032F);
    draw->AddRectFilledMultiColor({0.0F, 0.0F}, {edgeW, screenH},
                                  IM_COL32(10, 11, 12, 255), IM_COL32(38, 39, 39, 255),
                                  IM_COL32(18, 18, 18, 255), IM_COL32(4, 4, 4, 255));
    draw->AddRectFilledMultiColor({screenW - edgeW, 0.0F}, {screenW, screenH},
                                  IM_COL32(38, 39, 39, 255), IM_COL32(10, 11, 12, 255),
                                  IM_COL32(4, 4, 4, 255), IM_COL32(18, 18, 18, 255));
    for (float y : {screenH * 0.14F, screenH * 0.86F}) {
        draw->AddRectFilled({edgeW * 0.36F, y - 2.0F}, {edgeW * 0.62F, y + 2.0F},
                            IM_COL32(130, 132, 132, 190), 1.0F);
        draw->AddRectFilled({screenW - edgeW * 0.62F, y - 2.0F}, {screenW - edgeW * 0.36F, y + 2.0F},
                            IM_COL32(130, 132, 132, 190), 1.0F);
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

    if (!hasStageArt) {
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
        const auto key = std::filesystem::path(imagePath).lexically_normal().string();
        auto it = m_App.m_StaticImageCache.find(key);
        if (it != m_App.m_StaticImageCache.end()) {
            m_App.m_StageBackground = it->second;
        } else {
            auto image = std::make_shared<Util::Image>(key);
            m_App.m_StaticImageCache.emplace(key, image);
            m_App.m_StageBackground = std::move(image);
        }
    }

    if (m_App.m_StageBackground != nullptr && m_App.m_StageBackground->GetTextureId() != 0) {
        const glm::vec2 imageSize = m_App.m_StageBackground->GetSize();
        if (imageSize.x > 0.0F && imageSize.y > 0.0F) {
            const float scale = drawBlackFill
                ? std::min(screenW / imageSize.x, screenH / imageSize.y)
                : std::max(screenW / imageSize.x, screenH / imageSize.y);
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

void Ark::AppRenderer::DrawImageCover(const std::shared_ptr<Util::Image>& image, float alpha, bool drawBlackFill) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const float screenW = static_cast<float>(PTSD_Config::WINDOW_WIDTH);
    const float screenH = static_cast<float>(PTSD_Config::WINDOW_HEIGHT);

    if (drawBlackFill) {
        draw->AddRectFilled({0.0F, 0.0F}, {screenW, screenH}, IM_COL32(0, 0, 0, 255));
    }

    if (image == nullptr || image->GetTextureId() == 0) return;

    const glm::vec2 imageSize = image->GetSize();
    if (imageSize.x <= 0.0F || imageSize.y <= 0.0F) return;

    const float scale = std::max(screenW / imageSize.x, screenH / imageSize.y);
    const float drawW = imageSize.x * scale;
    const float drawH = imageSize.y * scale;
    const float x = (screenW - drawW) * 0.5F;
    const float y = (screenH - drawH) * 0.5F;
    const int imageAlpha = static_cast<int>(std::clamp(alpha, 0.0F, 1.0F) * 255.0F);
    draw->AddImage(
        reinterpret_cast<void*>(static_cast<intptr_t>(image->GetTextureId())),
        {x, y},
        {x + drawW, y + drawH},
        {0.0F, 0.0F},
        {1.0F, 1.0F},
        IM_COL32(255, 255, 255, imageAlpha)
    );
}

void Ark::AppRenderer::DrawLoadingScreen() {
    DrawImageCover(m_App.m_StageBackgroundPath, m_App.m_StageBackgroundAlpha, true);
}
