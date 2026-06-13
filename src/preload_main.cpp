#include "Ark/StageLoader.hpp"
#include "Util/Animation.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr std::array<const char*, 2> kAnimationExtensions{".apng", ".webm"};

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

const char* GetEnvOption(const char* name) {
    if (const char* value = std::getenv(name);
        value != nullptr && value[0] != '\0') {
        return value;
    }

    const std::string lowerName = ToLower(name);
    if (lowerName != name) {
        if (const char* value = std::getenv(lowerName.c_str());
            value != nullptr && value[0] != '\0') {
            return value;
        }
    }
    return nullptr;
}

void SetEnvValue(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void SetEnvIfUnset(const char* name, const char* value) {
    if (GetEnvOption(name) == nullptr) {
        SetEnvValue(name, value);
    }
}

#if defined(_WIN32)
void AppendPathIfPresent(std::string& pathValue, const std::filesystem::path& dir) {
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) return;

    const std::string dirString = dir.string();
    if (pathValue.find(dirString) != std::string::npos) return;

    if (!pathValue.empty() && pathValue.back() != ';') {
        pathValue += ';';
    }
    pathValue += dirString;
}

void AddCommonFfmpegLocationsToPath() {
    const char* rawPath = std::getenv("PATH");
    std::string pathValue = rawPath != nullptr ? rawPath : "";

    const auto before = pathValue;
    AppendPathIfPresent(pathValue, "C:\\Program Files\\ffmpeg\\bin");

    if (const char* userProfile = std::getenv("USERPROFILE");
        userProfile != nullptr && userProfile[0] != '\0') {
        AppendPathIfPresent(pathValue, std::filesystem::path(userProfile) / "scoop" / "shims");
    }

    if (pathValue != before) {
        _putenv_s("PATH", pathValue.c_str());
    }
}
#endif

bool CommandAvailable(const char* commandName) {
#if defined(_WIN32)
    const std::string command = std::string("where ") + commandName + " >nul 2>nul";
#else
    const std::string command = std::string("command -v ") + commandName + " >/dev/null 2>&1";
#endif
    return std::system(command.c_str()) == 0;
}

bool IsSupportedAnimationFile(const std::filesystem::path& path) {
    const std::string extension = ToLower(path.extension().string());
    return std::find(kAnimationExtensions.begin(), kAnimationExtensions.end(), extension) !=
           kAnimationExtensions.end();
}

std::filesystem::path NormalizePath(const std::filesystem::path& path) {
    std::error_code ec;
    auto normalized = std::filesystem::weakly_canonical(path, ec);
    if (!ec && !normalized.empty()) return normalized;

    normalized = std::filesystem::absolute(path, ec);
    return ec ? path.lexically_normal() : normalized.lexically_normal();
}

void AppendAnimationFiles(const std::filesystem::path& root,
                          std::vector<std::filesystem::path>& files,
                          std::set<std::string>& seen) {
    std::error_code ec;
    if (root.empty() ||
        !std::filesystem::exists(root, ec) ||
        !std::filesystem::is_directory(root, ec)) {
        return;
    }

    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }

        std::error_code entryEc;
        if (!it->is_regular_file(entryEc) || !IsSupportedAnimationFile(it->path())) {
            continue;
        }

        auto normalized = NormalizePath(it->path());
        if (seen.insert(normalized.string()).second) {
            files.push_back(std::move(normalized));
        }
    }
}

std::string DisplayPath(const std::filesystem::path& path) {
    std::error_code ec;
    const auto relative = std::filesystem::relative(path, std::filesystem::current_path(ec), ec);
    return ec ? path.string() : relative.string();
}

std::filesystem::path ReportedDiskCacheDir() {
    if (const char* overrideDir = GetEnvOption("ARKNIGHT_ANIMATION_DISK_CACHE_DIR");
        overrideDir != nullptr && overrideDir[0] != '\0') {
        return std::filesystem::path(overrideDir);
    }
    if (const char* xdgCache = GetEnvOption("XDG_CACHE_HOME");
        xdgCache != nullptr && xdgCache[0] != '\0') {
        return std::filesystem::path(xdgCache) / "arknight-linux" / "animation-cache";
    }
    if (const char* home = GetEnvOption("HOME");
        home != nullptr && home[0] != '\0') {
        return std::filesystem::path(home) / ".cache" / "arknight-linux" / "animation-cache";
    }

    std::error_code ec;
    const auto temp = std::filesystem::temp_directory_path(ec);
    return (ec ? std::filesystem::path(".") : temp) / "arknight-linux" / "animation-cache";
}

bool WarmAnimation(const std::filesystem::path& path,
                   std::size_t& frameCount,
                   std::string& errorMessage) {
    try {
        Util::Animation::ClearDecodedMediaCache();
        Util::Animation animation(path.string(), false, true, false, false);
        frameCount = animation.GetFrameCount();
        Util::Animation::ClearDecodedMediaCache();

        if (frameCount == 0) {
            errorMessage = "decoded zero frames";
            return false;
        }
        return true;
    } catch (const std::exception& error) {
        Util::Animation::ClearDecodedMediaCache();
        errorMessage = error.what();
        return false;
    }
}

} // namespace

int main(int, char**) {
#if defined(_WIN32)
    AddCommonFfmpegLocationsToPath();
#endif

    SetEnvValue("ARKNIGHT_ANIMATION_DISK_CACHE", "1");
    SetEnvIfUnset("ARKNIGHT_ANIMATION_TEXTURE_CACHE", "0");

    std::cout << "Arknight animation preload (headless)\n";
    std::cout << "Cache directory: " << ReportedDiskCacheDir().string() << "\n";

    if (!CommandAvailable("ffmpeg") || !CommandAvailable("ffprobe")) {
        std::cerr << "\nERROR: ffmpeg and ffprobe are required for animation preload.\n";
        std::cerr << "Install FFmpeg, then make sure ffmpeg.exe and ffprobe.exe are on PATH.\n";
        return 2;
    }

    std::vector<std::filesystem::path> files;
    std::set<std::string> seen;
    AppendAnimationFiles(Ark::ResolveOperatorDir(), files, seen);
    AppendAnimationFiles(Ark::ResolveEnemyDir(), files, seen);
    std::sort(files.begin(), files.end());

    if (files.empty()) {
        std::cerr << "\nERROR: no .apng or .webm animation files were found under data/operators or data/enemy.\n";
        return 1;
    }

    std::cout << "Found " << files.size() << " animation files.\n\n";

    std::vector<std::pair<std::filesystem::path, std::string>> failures;
    std::size_t decodedFrames = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        std::size_t frameCount = 0;
        std::string errorMessage;
        std::cout << "[" << (i + 1) << "/" << files.size() << "] "
                  << DisplayPath(files[i]) << " ... " << std::flush;

        if (WarmAnimation(files[i], frameCount, errorMessage)) {
            decodedFrames += frameCount;
            std::cout << "OK (" << frameCount << " frames)\n";
        } else {
            failures.emplace_back(files[i], std::move(errorMessage));
            std::cout << "FAILED\n";
        }
    }

    std::cout << "\nPreload summary\n";
    std::cout << "  Successful files: " << (files.size() - failures.size()) << "\n";
    std::cout << "  Failed files: " << failures.size() << "\n";
    std::cout << "  Decoded frames: " << decodedFrames << "\n";

    if (!failures.empty()) {
        std::cout << "\nFailed animation files:\n";
        for (const auto& [path, error] : failures) {
            std::cout << "  - " << DisplayPath(path) << ": " << error << "\n";
        }
        return 1;
    }

    return 0;
}
