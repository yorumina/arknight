#include "Util/Animation.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {
constexpr int VIDEO_MAX_DIMENSION = 256;
constexpr std::size_t DEFAULT_VIDEO_CACHE_LIMIT_MIB = 768U;
constexpr std::size_t BYTES_PER_MIB = 1024U * 1024U;
constexpr double DEFAULT_VIDEO_FPS = 24.0;
constexpr double MAX_REASONABLE_VIDEO_FPS = 60.0;
constexpr const char *VIDEO_CACHE_LIMIT_ENV = "ARKNIGHT_ANIMATION_CACHE_MB";
constexpr const char *VIDEO_TEXTURE_CACHE_ENV = "ARKNIGHT_ANIMATION_TEXTURE_CACHE";
constexpr const char *VIDEO_DROP_CPU_FRAMES_ENV = "ARKNIGHT_ANIMATION_DROP_CPU_FRAMES";
constexpr const char *VIDEO_DISK_CACHE_ENV = "ARKNIGHT_ANIMATION_DISK_CACHE";
constexpr const char *VIDEO_DISK_CACHE_DIR_ENV = "ARKNIGHT_ANIMATION_DISK_CACHE_DIR";
constexpr const char *DISK_CACHE_MAGIC = "AKANIM2";
constexpr std::uint32_t DISK_CACHE_VERSION = 2;

using SurfacePtr = std::shared_ptr<SDL_Surface>;
using ImagePtr = std::shared_ptr<Util::Image>;

struct VideoCache {
    std::vector<SurfacePtr> surfaces;
    std::vector<ImagePtr> textureFramesAA;
    std::vector<ImagePtr> textureFramesNearest;
    double intervalMs = 41.0;
    std::size_t bytes = 0;
    std::size_t surfaceBytes = 0;
    std::uint64_t lastUsed = 0;
};

std::unordered_map<std::string, VideoCache> &GetVideoCache() {
    static std::unordered_map<std::string, VideoCache> cache;
    return cache;
}

std::unordered_set<std::string> &GetFailedVideoCache() {
    static std::unordered_set<std::string> cache;
    return cache;
}

bool IsKnownFailedVideoPath(const std::string &mediaPath) {
    return GetFailedVideoCache().find(mediaPath) != GetFailedVideoCache().end();
}

bool MarkFailedVideoPath(const std::string &mediaPath) {
    return GetFailedVideoCache().insert(mediaPath).second;
}

std::size_t &GetVideoCacheBytes() {
    static std::size_t bytes = 0;
    return bytes;
}

std::uint64_t NextVideoCacheUse() {
    static std::uint64_t counter = 0;
    return ++counter;
}

const char *GetEnvOption(const char *name);

std::size_t GetVideoCacheLimitBytes() {
    const char *value = GetEnvOption(VIDEO_CACHE_LIMIT_ENV);
    if (value == nullptr || *value == '\0') {
        return DEFAULT_VIDEO_CACHE_LIMIT_MIB * BYTES_PER_MIB;
    }

    char *end = nullptr;
    const auto mib = std::strtoull(value, &end, 10);
    if (end == value) return DEFAULT_VIDEO_CACHE_LIMIT_MIB * BYTES_PER_MIB;
    if (mib == 0) return 0;
    return static_cast<std::size_t>(mib) * BYTES_PER_MIB;
}

void PruneVideoCache(const std::string &protectedPath) {
    const auto cacheLimitBytes = GetVideoCacheLimitBytes();
    if (cacheLimitBytes == 0) return;

    auto &cache = GetVideoCache();
    auto &cachedBytes = GetVideoCacheBytes();

    while (cachedBytes > cacheLimitBytes && cache.size() > 1) {
        auto victim = cache.end();
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->first == protectedPath) continue;
            if (victim == cache.end() || it->second.lastUsed < victim->second.lastUsed) {
                victim = it;
            }
        }

        if (victim == cache.end()) break;
        cachedBytes -= victim->second.bytes;
        cache.erase(victim);
    }
}

std::string ShellQuote(const std::string &value) {
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'') quoted += "'\\''";
        else quoted += c;
    }
    quoted += "'";
    return quoted;
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

const char *GetEnvOption(const char *name) {
    if (const char *value = std::getenv(name);
        value != nullptr && value[0] != '\0') {
        return value;
    }

    const std::string lowerName = ToLower(name);
    if (lowerName != name) {
        if (const char *value = std::getenv(lowerName.c_str());
            value != nullptr && value[0] != '\0') {
            return value;
        }
    }
    return nullptr;
}

bool IsEnvDisabled(const char *value) {
    if (value == nullptr || *value == '\0') return false;
    const std::string normalized = ToLower(value);
    return normalized == "0" || normalized == "false" ||
           normalized == "none" || normalized == "off";
}

bool IsVideoTextureCacheEnabled() {
    return !IsEnvDisabled(GetEnvOption(VIDEO_TEXTURE_CACHE_ENV));
}

bool ShouldDropCpuFramesAfterTextureUpload() {
    return !IsEnvDisabled(GetEnvOption(VIDEO_DROP_CPU_FRAMES_ENV));
}

bool IsDiskCacheEnabled() {
    return !IsEnvDisabled(GetEnvOption(VIDEO_DISK_CACHE_ENV));
}

std::vector<ImagePtr> &SelectTextureFrames(VideoCache &cache, bool useAA) {
    return useAA ? cache.textureFramesAA : cache.textureFramesNearest;
}

std::vector<ImagePtr> &SelectReusableTextureFrames(VideoCache &cache, bool useAA) {
    auto &preferred = SelectTextureFrames(cache, useAA);
    if (!preferred.empty()) return preferred;

    auto &alternate = useAA ? cache.textureFramesNearest : cache.textureFramesAA;
    if (!alternate.empty()) {
        for (const auto &frame : alternate) {
            frame->UseAntiAliasing(useAA);
        }
    }
    return alternate.empty() ? preferred : alternate;
}

void EnsureTextureFramesCached(VideoCache &cache,
                               const std::string &protectedPath,
                               bool useAA) {
    if (!IsVideoTextureCacheEnabled()) return;
    if (cache.surfaces.empty()) {
        (void) SelectReusableTextureFrames(cache, useAA);
        return;
    }

    auto &textureFrames = SelectTextureFrames(cache, useAA);
    if (!textureFrames.empty()) return;

    textureFrames.reserve(cache.surfaces.size());
    for (const auto &surface : cache.surfaces) {
        if (surface == nullptr) continue;
        textureFrames.push_back(std::make_shared<Util::Image>(surface.get(), useAA));
    }
    if (textureFrames.empty()) return;

    cache.bytes += cache.surfaceBytes;
    GetVideoCacheBytes() += cache.surfaceBytes;
    cache.lastUsed = NextVideoCacheUse();

    if (ShouldDropCpuFramesAfterTextureUpload() &&
        textureFrames.size() == cache.surfaces.size()) {
        cache.surfaces.clear();
        cache.surfaces.shrink_to_fit();
        if (cache.bytes >= cache.surfaceBytes) cache.bytes -= cache.surfaceBytes;
        auto &cachedBytes = GetVideoCacheBytes();
        cachedBytes = cachedBytes >= cache.surfaceBytes ? cachedBytes - cache.surfaceBytes : 0;
    }

    PruneVideoCache(protectedPath);
}

std::uint64_t Fnva1Hash(const std::string &value) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (unsigned char c : value) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string ToHex(std::uint64_t value) {
    std::ostringstream output;
    output << std::hex << std::setw(16) << std::setfill('0') << value;
    return output.str();
}

bool ParseFrameRate(const char *raw, double &fps) {
    if (raw == nullptr || raw[0] == '\0') return false;

    int num = 0;
    int den = 1;
    if (sscanf(raw, "%d/%d", &num, &den) >= 1 && num > 0) {
        if (den <= 0) den = 1;
        fps = static_cast<double>(num) / static_cast<double>(den);
        return fps > 0.0;
    }
    return false;
}

double NormalizeVideoFps(double fps) {
    if (fps < 1.0 || fps > MAX_REASONABLE_VIDEO_FPS) {
        return DEFAULT_VIDEO_FPS;
    }
    return fps;
}

double NormalizeVideoIntervalMs(double intervalMs) {
    if (intervalMs <= 0.0) return 1000.0 / DEFAULT_VIDEO_FPS;
    return 1000.0 / NormalizeVideoFps(1000.0 / intervalMs);
}

std::filesystem::path NormalizedMediaPath(const std::string &mediaPath) {
    std::error_code ec;
    auto normalized = std::filesystem::weakly_canonical(mediaPath, ec);
    if (!ec && !normalized.empty()) return normalized;
    normalized = std::filesystem::absolute(mediaPath, ec);
    return ec ? std::filesystem::path(mediaPath) : normalized;
}

std::filesystem::path GetDiskCacheDir() {
    if (const char *overrideDir = GetEnvOption(VIDEO_DISK_CACHE_DIR_ENV);
        overrideDir != nullptr && overrideDir[0] != '\0') {
        return std::filesystem::path(overrideDir);
    }
    if (const char *xdgCache = GetEnvOption("XDG_CACHE_HOME");
        xdgCache != nullptr && xdgCache[0] != '\0') {
        return std::filesystem::path(xdgCache) / "arknight-linux" / "animation-cache";
    }
    if (const char *home = GetEnvOption("HOME");
        home != nullptr && home[0] != '\0') {
        return std::filesystem::path(home) / ".cache" / "arknight-linux" / "animation-cache";
    }

    std::error_code ec;
    const auto temp = std::filesystem::temp_directory_path(ec);
    return (ec ? std::filesystem::path(".") : temp) / "arknight-linux" / "animation-cache";
}

std::filesystem::path GetDiskCachePath(const std::string &mediaPath) {
    const auto normalized = NormalizedMediaPath(mediaPath).string();
    const auto key = normalized + "|" + std::to_string(VIDEO_MAX_DIMENSION);
    return GetDiskCacheDir() / (ToHex(Fnva1Hash(key)) + ".akanim");
}

bool GetSourceFingerprint(const std::string &mediaPath,
                          std::uint64_t &fileSize,
                          std::int64_t &mtime) {
    std::error_code ec;
    fileSize = std::filesystem::file_size(mediaPath, ec);
    if (ec) return false;
    const auto writeTime = std::filesystem::last_write_time(mediaPath, ec);
    if (ec) return false;
    mtime = static_cast<std::int64_t>(writeTime.time_since_epoch().count());
    return true;
}

template <typename T>
bool ReadValue(std::ifstream &input, T &value) {
    input.read(reinterpret_cast<char *>(&value), sizeof(T));
    return input.good();
}

template <typename T>
bool WriteValue(std::ofstream &output, const T &value) {
    output.write(reinterpret_cast<const char *>(&value), sizeof(T));
    return output.good();
}

bool LoadFramesFromDiskCache(const std::string &mediaPath,
                             std::vector<SurfacePtr> &surfaces,
                             double &intervalMs) {
    if (!IsDiskCacheEnabled()) return false;

    std::uint64_t sourceSize = 0;
    std::int64_t sourceMtime = 0;
    if (!GetSourceFingerprint(mediaPath, sourceSize, sourceMtime)) return false;

    const auto cachePath = GetDiskCachePath(mediaPath);
    std::ifstream input(cachePath, std::ios::binary);
    if (!input.is_open()) return false;

    char magic[8] = {};
    input.read(magic, sizeof(magic));
    if (!input.good() || std::string(magic, magic + 7) != DISK_CACHE_MAGIC) return false;

    std::uint32_t version = 0;
    std::uint32_t maxDimension = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frameCount = 0;
    std::uint32_t frameBytes = 0;
    std::uint64_t cachedSourceSize = 0;
    std::int64_t cachedSourceMtime = 0;
    double cachedIntervalMs = 0.0;

    if (!ReadValue(input, version) ||
        !ReadValue(input, maxDimension) ||
        !ReadValue(input, width) ||
        !ReadValue(input, height) ||
        !ReadValue(input, frameCount) ||
        !ReadValue(input, frameBytes) ||
        !ReadValue(input, cachedSourceSize) ||
        !ReadValue(input, cachedSourceMtime) ||
        !ReadValue(input, cachedIntervalMs)) {
        return false;
    }

    if (version != DISK_CACHE_VERSION ||
        maxDimension != static_cast<std::uint32_t>(VIDEO_MAX_DIMENSION) ||
        cachedSourceSize != sourceSize ||
        cachedSourceMtime != sourceMtime ||
        width == 0 || height == 0 || frameCount == 0 ||
        frameBytes != width * height * 4U ||
        cachedIntervalMs <= 0.0) {
        return false;
    }

    surfaces.clear();
    surfaces.reserve(frameCount);
    for (std::uint32_t i = 0; i < frameCount; ++i) {
        auto surface = SurfacePtr(
            SDL_CreateRGBSurfaceWithFormat(0,
                                           static_cast<int>(width),
                                           static_cast<int>(height),
                                           32,
                                           SDL_PIXELFORMAT_ABGR8888),
            SDL_FreeSurface);
        if (surface == nullptr) return false;

        auto *pixels = static_cast<char *>(surface->pixels);
        const int rowBytes = static_cast<int>(width) * 4;
        for (std::uint32_t y = 0; y < height; ++y) {
            input.read(pixels + static_cast<int>(y) * surface->pitch, rowBytes);
            if (!input.good()) return false;
        }
        surfaces.push_back(std::move(surface));
    }

    intervalMs = NormalizeVideoIntervalMs(cachedIntervalMs);
    return !surfaces.empty();
}

void SaveFramesToDiskCache(const std::string &mediaPath,
                           const std::vector<SurfacePtr> &surfaces,
                           double intervalMs,
                           int frameWidth,
                           int frameHeight) {
    if (!IsDiskCacheEnabled() || surfaces.empty() ||
        frameWidth <= 0 || frameHeight <= 0 || intervalMs <= 0.0) {
        return;
    }

    std::uint64_t sourceSize = 0;
    std::int64_t sourceMtime = 0;
    if (!GetSourceFingerprint(mediaPath, sourceSize, sourceMtime)) return;

    const auto cachePath = GetDiskCachePath(mediaPath);
    std::error_code ec;
    std::filesystem::create_directories(cachePath.parent_path(), ec);
    if (ec) return;

    const auto tempPath = cachePath.string() + ".tmp";
    std::ofstream output(tempPath, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) return;

    char magic[8] = {};
    std::copy(DISK_CACHE_MAGIC, DISK_CACHE_MAGIC + 7, magic);
    output.write(magic, sizeof(magic));

    const std::uint32_t version = DISK_CACHE_VERSION;
    const std::uint32_t maxDimension = VIDEO_MAX_DIMENSION;
    const std::uint32_t width = static_cast<std::uint32_t>(frameWidth);
    const std::uint32_t height = static_cast<std::uint32_t>(frameHeight);
    const std::uint32_t frameCount = static_cast<std::uint32_t>(surfaces.size());
    const std::uint32_t frameBytes = width * height * 4U;

    if (!WriteValue(output, version) ||
        !WriteValue(output, maxDimension) ||
        !WriteValue(output, width) ||
        !WriteValue(output, height) ||
        !WriteValue(output, frameCount) ||
        !WriteValue(output, frameBytes) ||
        !WriteValue(output, sourceSize) ||
        !WriteValue(output, sourceMtime) ||
        !WriteValue(output, intervalMs)) {
        return;
    }

    for (const auto &surface : surfaces) {
        if (surface == nullptr || surface->pixels == nullptr) return;
        const auto *pixels = static_cast<const char *>(surface->pixels);
        const int rowBytes = frameWidth * 4;
        if (surface->pitch == rowBytes) {
            output.write(pixels, frameBytes);
        } else {
            for (int y = 0; y < frameHeight; ++y) {
                output.write(pixels + y * surface->pitch, rowBytes);
            }
        }
        if (!output.good()) return;
    }
    output.close();
    if (!output.good()) return;

    std::filesystem::remove(cachePath, ec);
    ec.clear();
    std::filesystem::rename(tempPath, cachePath, ec);
    if (ec) {
        std::filesystem::remove(tempPath, ec);
    }
}

std::string BuildFfmpegHwAccelArgs() {
    const char *rawArgs = GetEnvOption("ARKNIGHT_FFMPEG_HWACCEL_ARGS");
    if (rawArgs != nullptr && rawArgs[0] != '\0') {
        return std::string(rawArgs) + " ";
    }

    const char *accelEnv = GetEnvOption("ARKNIGHT_FFMPEG_HWACCEL");
    const std::string accel = accelEnv != nullptr ? ToLower(accelEnv) : "auto";
    if (accel.empty() || accel == "0" || accel == "false" ||
        accel == "none" || accel == "off") {
        return "";
    }
    return "-hwaccel " + ShellQuote(accel) + " ";
}

std::string BuildFfmpegDecodeCommand(const std::string &mediaPath,
                                     const std::string &filter,
                                     const std::string &hwAccelArgs) {
    return "ffmpeg -nostdin -hide_banner -loglevel quiet " +
           hwAccelArgs +
           "-i " + ShellQuote(mediaPath) +
           " -an -vf " + ShellQuote(filter) +
           " -f rawvideo -pix_fmt rgba -";
}

bool ReadDecodedFfmpegFrames(const std::string &command,
                             std::size_t frameSizeBytes,
                             int frameWidth,
                             int frameHeight,
                             std::vector<SurfacePtr> &surfaces) {
    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) return false;

    std::vector<unsigned char> frameBuffer(frameSizeBytes);
    bool decodeFailed = false;

    while (true) {
        std::size_t totalRead = 0;
        while (totalRead < frameSizeBytes) {
            const std::size_t readNow = fread(frameBuffer.data() + totalRead, 1,
                                               frameSizeBytes - totalRead, pipe);
            if (readNow == 0) break;
            totalRead += readNow;
        }
        if (totalRead != frameSizeBytes) break; // EOF or short read -> done

        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
            frameBuffer.data(),
            frameWidth,
            frameHeight,
            32,
            frameWidth * 4,
            SDL_PIXELFORMAT_ABGR8888);
        if (surface == nullptr) {
            decodeFailed = true;
            break;
        }

        auto copiedSurface = SurfacePtr(
            SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0),
            SDL_FreeSurface);
        SDL_FreeSurface(surface);

        if (copiedSurface == nullptr) {
            decodeFailed = true;
            break;
        }
        surfaces.push_back(std::move(copiedSurface));
    }
    pclose(pipe);

    return !decodeFailed && !surfaces.empty();
}

bool IsFfmpegDecodedPath(const std::string &path) {
    const auto extension = ToLower(std::filesystem::path(path).extension().string());
    return extension == ".webm" || extension == ".apng";
}

bool ProbeVideoSize(const std::string &path, int &width, int &height) {
    static std::unordered_map<std::string, std::pair<int, int>> sizeCache;
    if (const auto it = sizeCache.find(path); it != sizeCache.end()) {
        width = it->second.first;
        height = it->second.second;
        return true;
    }

    const std::string command =
        "ffprobe -v quiet -select_streams v:0 "
        "-show_entries stream=width,height "
        "-of csv=p=0:s=x " + ShellQuote(path);

    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) return false;

    char buffer[128] = {};
    const bool gotLine = fgets(buffer, sizeof(buffer), pipe) != nullptr;
    pclose(pipe);
    if (!gotLine) return false;

    std::string line(buffer);
    std::replace(line.begin(), line.end(), 'x', ' ');
    std::istringstream input(line);
    input >> width >> height;
    if (width <= 0 || height <= 0) return false;

    sizeCache[path] = {width, height};
    return true;
}
} // namespace

namespace Util {
Animation::Animation(const std::vector<std::string> &paths, bool play,
                     std::size_t interval, bool looping, std::size_t cooldown,
                     bool useAA, bool streamFrames)
    : m_StreamFrames(streamFrames),
      m_UseAA(useAA),
      m_State(play ? State::PLAY : State::PAUSE),
      m_Interval(interval),
      m_Looping(looping),
      m_Cooldown(cooldown) {
    if (m_StreamFrames) {
        m_FramePaths = paths;
        EnsureStreamFrameLoaded();
    } else {
        m_Frames.reserve(paths.size());
        for (const auto &path : paths) {
            m_Frames.push_back(std::make_shared<Util::Image>(path, useAA));
        }
    }
}

Animation::Animation(const std::vector<std::shared_ptr<Util::Image>> &frames, bool play,
                     std::size_t interval, bool looping, std::size_t cooldown)
    : m_Frames(frames),
      m_State(play ? State::PLAY : State::PAUSE),
      m_Interval(interval),
      m_Looping(looping),
      m_Cooldown(cooldown) {
}

Animation::Animation(const std::string &mediaPath,
                     bool play,
                     bool looping,
                     bool useAA,
                     bool cacheVideoTextures)
    : m_UseAA(useAA),
      m_CacheVideoTextures(cacheVideoTextures),
      m_State(play ? State::PLAY : State::PAUSE),
      m_Interval(41),
      m_Looping(looping),
      m_Cooldown(0) {
    if (IsFfmpegDecodedPath(mediaPath)) {
        if (IsKnownFailedVideoPath(mediaPath)) {
            return;
        }
        if (!LoadFfmpegFrames(mediaPath)) {
            if (MarkFailedVideoPath(mediaPath)) {
                LOG_ERROR("Failed to load media animation: '{}'", mediaPath);
            }
        }
        return;
    }

    m_GifAnimation.reset(IMG_LoadAnimation(mediaPath.c_str()));
    if (!m_GifAnimation || m_GifAnimation->count <= 0) {
        LOG_ERROR("Failed to load image animation: '{}'", mediaPath);
        LOG_ERROR("{}", IMG_GetError());
        m_FramePaths = {mediaPath};
        m_StreamFrames = true;
        EnsureStreamFrameLoaded();
        return;
    }

    EnsureGifFrameLoaded();
}

std::size_t Animation::GetFrameCount() const {
    if (m_GifAnimation) return static_cast<std::size_t>(std::max(0, m_GifAnimation->count));
    if (!m_VideoImageFrames.empty()) return m_VideoImageFrames.size();
    if (!m_VideoFrames.empty()) return m_VideoFrames.size();
    return m_StreamFrames ? m_FramePaths.size() : m_Frames.size();
}

std::shared_ptr<Util::Image> Animation::GetCurrentImage() const {
    if (m_GifAnimation) {
        EnsureGifFrameLoaded();
        return m_StreamFrame;
    }
    if (!m_VideoImageFrames.empty()) {
        return m_VideoImageFrames[std::min(m_Index, m_VideoImageFrames.size() - 1)];
    }
    if (!m_VideoFrames.empty()) {
        EnsureVideoFrameLoaded();
        return m_StreamFrame;
    }
    if (m_StreamFrames) {
        EnsureStreamFrameLoaded();
        return m_StreamFrame;
    }
    if (m_Frames.empty()) return nullptr;
    return m_Frames[std::min(m_Index, m_Frames.size() - 1)];
}

void Animation::EnsureStreamFrameLoaded() const {
    if (!m_StreamFrames || m_FramePaths.empty()) return;
    const auto targetIndex = std::min(m_Index, m_FramePaths.size() - 1);
    if (!m_StreamFrame) {
        m_StreamFrame = std::make_shared<Util::Image>(m_FramePaths[targetIndex], m_UseAA);
        m_LoadedStreamIndex = targetIndex;
        return;
    }
    if (m_LoadedStreamIndex == targetIndex) return;
    m_StreamFrame->SetImage(m_FramePaths[targetIndex]);
    m_LoadedStreamIndex = targetIndex;
}

void Animation::EnsureGifFrameLoaded() const {
    if (!m_GifAnimation || m_GifAnimation->count <= 0) return;
    const auto frameCount = static_cast<std::size_t>(m_GifAnimation->count);
    const auto targetIndex = std::min(m_Index, frameCount - 1);
    SDL_Surface *surface = m_GifAnimation->frames[targetIndex];
    if (!m_StreamFrame) {
        m_StreamFrame = std::make_shared<Util::Image>(surface, m_UseAA);
        m_LoadedGifIndex = targetIndex;
        return;
    }
    if (m_LoadedGifIndex == targetIndex) return;
    m_StreamFrame->SetSurface(surface);
    m_LoadedGifIndex = targetIndex;
}

void Animation::EnsureVideoFrameLoaded() const {
    if (m_VideoFrames.empty()) return;
    const auto targetIndex = std::min(m_Index, m_VideoFrames.size() - 1);
    SDL_Surface *surface = m_VideoFrames[targetIndex].get();
    if (!m_StreamFrame) {
        m_StreamFrame = std::make_shared<Util::Image>(surface, m_UseAA);
        m_LoadedVideoIndex = targetIndex;
        return;
    }
    if (m_LoadedVideoIndex == targetIndex) return;
    m_StreamFrame->SetSurface(surface);
    m_LoadedVideoIndex = targetIndex;
}

bool Animation::LoadFfmpegFrames(const std::string &mediaPath) {
    // Cache decoded frames across Animation instances sharing the same media,
    // keeping warm-up work in memory unless an explicit cache budget is configured.
    auto &videoCache = GetVideoCache();

    if (const auto it = videoCache.find(mediaPath); it != videoCache.end()) {
        it->second.lastUsed = NextVideoCacheUse();
        it->second.intervalMs = NormalizeVideoIntervalMs(it->second.intervalMs);
        m_Interval = it->second.intervalMs;
        if (m_CacheVideoTextures) {
            EnsureTextureFramesCached(it->second, mediaPath, m_UseAA);
        }
        const auto &textureFrames = m_CacheVideoTextures || it->second.surfaces.empty()
            ? SelectReusableTextureFrames(it->second, m_UseAA)
            : SelectTextureFrames(it->second, m_UseAA);
        if (m_CacheVideoTextures && !textureFrames.empty()) {
            m_VideoImageFrames = textureFrames;
        } else {
            m_VideoFrames = it->second.surfaces;
            if (m_VideoFrames.empty() && !textureFrames.empty()) {
                m_VideoImageFrames = textureFrames;
            }
        }
        return !m_VideoImageFrames.empty() || !m_VideoFrames.empty();
    }

    std::vector<SurfacePtr> diskSurfaces;
    double diskIntervalMs = 0.0;
    if (LoadFramesFromDiskCache(mediaPath, diskSurfaces, diskIntervalMs)) {
        m_Interval = diskIntervalMs;
        auto &cachedBytes = GetVideoCacheBytes();
        std::size_t surfaceBytes = 0;
        if (!diskSurfaces.empty()) {
            surfaceBytes = static_cast<std::size_t>(diskSurfaces.front()->w) *
                           static_cast<std::size_t>(diskSurfaces.front()->h) * 4U *
                           diskSurfaces.size();
        }
        cachedBytes += surfaceBytes;
        auto [cacheIt, inserted] = videoCache.emplace(
            mediaPath,
            VideoCache{diskSurfaces, {}, {}, diskIntervalMs, surfaceBytes, surfaceBytes,
                       NextVideoCacheUse()});
        (void) inserted;
        if (m_CacheVideoTextures) {
            EnsureTextureFramesCached(cacheIt->second, mediaPath, m_UseAA);
        }
        PruneVideoCache(mediaPath);
        const auto &textureFrames = m_CacheVideoTextures
            ? SelectReusableTextureFrames(cacheIt->second, m_UseAA)
            : SelectTextureFrames(cacheIt->second, m_UseAA);
        if (m_CacheVideoTextures && !textureFrames.empty()) {
            m_VideoImageFrames = textureFrames;
        } else {
            m_VideoFrames = cacheIt->second.surfaces;
        }
        return !m_VideoImageFrames.empty() || !m_VideoFrames.empty();
    }

    int sourceWidth = 0;
    int sourceHeight = 0;
    if (!ProbeVideoSize(mediaPath, sourceWidth, sourceHeight)) return false;

    const int maxSourceDim = std::max(sourceWidth, sourceHeight);
    const double scale = maxSourceDim > VIDEO_MAX_DIMENSION
        ? static_cast<double>(VIDEO_MAX_DIMENSION) / static_cast<double>(maxSourceDim)
        : 1.0;
    const int frameWidth = std::max(1, static_cast<int>(std::round(sourceWidth * scale)));
    const int frameHeight = std::max(1, static_cast<int>(std::round(sourceHeight * scale)));
    const std::size_t frameSizeBytes = static_cast<std::size_t>(frameWidth) *
                                       static_cast<std::size_t>(frameHeight) * 4U;

    // Probe video frame rate. Some exported sprite WebMs advertise 1000 fps,
    // which is metadata noise for gameplay animation, so clamp to a sane rate.
    double intervalMs = 1000.0 / DEFAULT_VIDEO_FPS;
    {
        const std::string fpsCmd =
            "ffprobe -v quiet -select_streams v:0 "
            "-show_entries stream=avg_frame_rate,r_frame_rate "
            "-of csv=p=0:s=x " + ShellQuote(mediaPath);
        FILE *fpsPipe = popen(fpsCmd.c_str(), "r");
        if (fpsPipe) {
            char buf[64] = {};
            if (fgets(buf, sizeof(buf), fpsPipe)) {
                std::string rates(buf);
                std::replace(rates.begin(), rates.end(), 'x', ' ');
                std::istringstream input(rates);
                std::string rate;
                while (input >> rate) {
                    double fps = 0.0;
                    if (!ParseFrameRate(rate.c_str(), fps)) continue;
                    fps = NormalizeVideoFps(fps);
                    intervalMs = 1000.0 / fps;
                    break;
                }
            }
            pclose(fpsPipe);
        }
    }
    m_Interval = intervalMs;

    // Decode ALL frames via ffmpeg rawvideo pipe
    const std::string filter =
        "scale=" + std::to_string(frameWidth) + ":" + std::to_string(frameHeight) +
        ":flags=fast_bilinear,format=rgba";
    std::vector<SurfacePtr> surfaces;
    const auto hwAccelArgs = BuildFfmpegHwAccelArgs();
    const bool decoded = ReadDecodedFfmpegFrames(
        BuildFfmpegDecodeCommand(mediaPath, filter, hwAccelArgs),
        frameSizeBytes, frameWidth, frameHeight, surfaces);
    if (!decoded && !hwAccelArgs.empty()) {
        surfaces.clear();
        if (!ReadDecodedFfmpegFrames(
                BuildFfmpegDecodeCommand(mediaPath, filter, ""),
                frameSizeBytes, frameWidth, frameHeight, surfaces)) {
            return false;
        }
    } else if (!decoded) {
        return false;
    }

    // Cache and apply
    auto &cachedBytes = GetVideoCacheBytes();
    const auto surfaceBytes = frameSizeBytes * surfaces.size();
    SaveFramesToDiskCache(mediaPath, surfaces, intervalMs, frameWidth, frameHeight);
    cachedBytes += surfaceBytes;
    auto [cacheIt, inserted] = videoCache.emplace(
        mediaPath,
        VideoCache{surfaces, {}, {}, intervalMs, surfaceBytes, surfaceBytes,
                   NextVideoCacheUse()});
    (void) inserted;
    if (m_CacheVideoTextures) {
        EnsureTextureFramesCached(cacheIt->second, mediaPath, m_UseAA);
    }
    PruneVideoCache(mediaPath);
    const auto &textureFrames = m_CacheVideoTextures
        ? SelectReusableTextureFrames(cacheIt->second, m_UseAA)
        : SelectTextureFrames(cacheIt->second, m_UseAA);
    if (m_CacheVideoTextures && !textureFrames.empty()) {
        m_VideoImageFrames = textureFrames;
    } else {
        m_VideoFrames = cacheIt->second.surfaces;
    }
    return !m_VideoImageFrames.empty() || !m_VideoFrames.empty();
}

void Animation::ClearDecodedMediaCache() {
    GetVideoCache().clear();
    GetFailedVideoCache().clear();
    GetVideoCacheBytes() = 0;
}

double Animation::GetCurrentFrameInterval() const {
    if (!m_GifAnimation || !m_GifAnimation->delays || m_GifAnimation->count <= 0)
        return m_Interval;

    const auto frameCount = static_cast<std::size_t>(m_GifAnimation->count);
    const auto targetIndex = std::min(m_Index, frameCount - 1);
    const int delayMs = m_GifAnimation->delays[targetIndex];
    return delayMs > 0 ? static_cast<double>(delayMs) : m_Interval;
}

glm::vec2 Animation::GetSize() const {
    const auto frame = GetCurrentImage();
    return frame ? frame->GetSize() : glm::vec2{0.0F, 0.0F};
}

GLuint Animation::GetTextureId() const {
    const auto frame = GetCurrentImage();
    return frame ? frame->GetTextureId() : 0;
}

void Animation::UseAntiAliasing(bool useAA) {
    m_UseAA = useAA;
    if (!m_VideoImageFrames.empty()) {
        for (const auto &frame : m_VideoImageFrames) {
            frame->UseAntiAliasing(useAA);
        }
        return;
    }
    if (m_StreamFrames || m_GifAnimation || !m_VideoFrames.empty()) {
        if (m_StreamFrame) m_StreamFrame->UseAntiAliasing(useAA);
        return;
    }
    for (const auto &frame : m_Frames) {
        frame->UseAntiAliasing(useAA);
    }
}

void Animation::SetCurrentFrame(std::size_t index) {
    const auto frameCount = GetFrameCount();
    m_Index = frameCount > 0 ? std::min(index, frameCount - 1) : 0;
    if (m_State == State::ENDED || m_State == State::COOLDOWN) {
        /*this make sure if user setframe on ENDED/COOLDOWN, will play from
         * where you set the frame*/
        m_IsChangeFrame = true;
    }
}

void Animation::Draw(const Core::Matrices &data) {
    const auto frame = GetCurrentImage();
    if (frame) frame->Draw(data);
    Update();
}

void Animation::Play() {
    if (m_State == State::PLAY)
        return;
    if (m_State == State::ENDED || m_State == State::COOLDOWN) {
        m_Index = m_IsChangeFrame ? m_Index : 0;
        m_IsChangeFrame = false;
    }
    m_State = State::PLAY;
}

void Animation::Restart() {
    m_Index = 0;
    m_TimeBetweenFrameUpdate = 0.0;
    m_CooldownEndTime = 0;
    m_IsChangeFrame = false;
    m_State = State::PLAY;
}

void Animation::Pause() {
    if (m_State == State::PLAY || m_State == State::COOLDOWN) {
        m_State = State::PAUSE;
    }
}

void Animation::Update() {
    Update(Util::Time::GetDeltaTimeMs());
}

void Animation::Update(float deltaTimeMs) {
    unsigned long nowTime = Util::Time::GetElapsedTimeMs();
    if (m_State == State::PAUSE || m_State == State::ENDED) {
        LOG_TRACE("[ANI] is pause");
        return;
    }

    if (m_State == State::COOLDOWN) {
        if (nowTime >= m_CooldownEndTime) {
            Play();
        }
        return;
    }

    m_TimeBetweenFrameUpdate += std::max(0.0F, deltaTimeMs);
    const double interval = std::max(1.0, GetCurrentFrameInterval());
    auto updateFrameCount =
        static_cast<unsigned int>(m_TimeBetweenFrameUpdate / interval);
    if (updateFrameCount <= 0)
        return;

    m_Index += updateFrameCount;
    m_TimeBetweenFrameUpdate = 0;

    const auto totalFramesCount = GetFrameCount();
    if (totalFramesCount == 0)
        return;

    if (m_Index >= totalFramesCount) {
        if (m_Looping) {
            m_CooldownEndTime = nowTime + m_Cooldown;
        }
        m_State = m_Looping ? State::COOLDOWN : State::ENDED;
        m_Index = totalFramesCount - 1;
    }
};
} // namespace Util
