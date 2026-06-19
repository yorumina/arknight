#include "App.hpp"

#include "Core/Context.hpp"

#include "Util/Input.hpp"

#include <cstdlib>
#include <filesystem>
#include <system_error>

#if defined(_WIN32)
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

namespace {
void SetEnvIfUnset(const char *name, const char *value) {
#if defined(__unix__)
    if (std::getenv(name) == nullptr) {
        setenv(name, value, 0);
    }
#else
    (void)name;
    (void)value;
#endif
}

bool PathExists(const char *path) {
    std::error_code error;
    return std::filesystem::exists(path, error);
}

bool HasNvidiaOffloadSupport() {
#if defined(__linux__)
    return PathExists("/proc/driver/nvidia/version") ||
           PathExists("/usr/share/glvnd/egl_vendor.d/10_nvidia.json") ||
           PathExists("/usr/share/glvnd/glx_vendor.d/10_nvidia.json");
#else
    return false;
#endif
}

void PreferDiscreteGpu() {
#if defined(__linux__)
    SetEnvIfUnset("DRI_PRIME", "1");

    if (HasNvidiaOffloadSupport()) {
        SetEnvIfUnset("__NV_PRIME_RENDER_OFFLOAD", "1");
        SetEnvIfUnset("__GLX_VENDOR_LIBRARY_NAME", "nvidia");
        SetEnvIfUnset("__VK_LAYER_NV_optimus", "NVIDIA_only");
    }
#endif
}
} // namespace

int main(int, char **) {
    PreferDiscreteGpu();

    auto context = Core::Context::GetInstance();
    App app;

    // set icon in window.
    context->SetWindowIcon(ASSETS_DIR "/icon.jpg");

    while (!context->GetExit()) {
        context->Setup();

        switch (app.GetCurrentState()) {
        case App::State::START:
            app.Start();
            break;

        case App::State::OPENING:
            app.Opening();
            break;

        case App::State::LOADING:
            app.Loading();
            break;

        case App::State::UPDATE:
            app.Update();
            break;

        case App::State::END:
            app.End();
            context->SetExit(true);
            break;
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        context->Update();
    }
    return 0;
}
