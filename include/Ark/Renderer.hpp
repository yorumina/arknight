#pragma once

#include "App.hpp"
#include <glm/glm.hpp>
#include <optional>

class App;

namespace Ark {

class AppRenderer {
public:
    AppRenderer(App& app) : m_App(app) {}

    void LoadOperatorThumbnails();
    void DrawScene(const glm::vec2& cursorPtsd);
    void DrawGrid();
    void DrawOperators(const BoardLayout& layout, bool drawHighgroundOnly);
    void DrawEnemies(const BoardLayout& layout);
    void DrawHighgroundTopLayer();
    void DrawMarkerTopLayer();
    void DrawBeams();
    void DrawDeployPreview(const glm::vec2& ptsdCursor, const std::optional<glm::ivec2>& hoverCell, const BoardLayout& layout);
    void DrawHUD(float screenW);
    void DrawOperatorBar(float screenW, float screenH);
    void DrawDeploymentInfo(float screenW, float screenH);
    void DrawOperatorDetails(const OperatorTemplate& t, const Operator* opOnField, float screenH);
    void DrawLoadingScreen();

private:
    App& m_App;
};

} // namespace Ark
