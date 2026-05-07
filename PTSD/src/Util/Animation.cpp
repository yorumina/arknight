#include "Util/Animation.hpp"
#include "Util/Logger.hpp"
#include "Util/Time.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {
constexpr int VIDEO_MAX_DIMENSION = 256;

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

bool IsWebmPath(const std::string &path) {
    return ToLower(std::filesystem::path(path).extension().string()) == ".webm";
}

bool ProbeVideoSize(const std::string &path, int &width, int &height) {
    static std::unordered_map<std::string, std::pair<int, int>> sizeCache;
    if (const auto it = sizeCache.find(path); it != sizeCache.end()) {
        width = it->second.first;
        height = it->second.second;
        return true;
    }

    const std::string command =
        "ffprobe -v error -select_streams v:0 "
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

Animation::Animation(const std::string &mediaPath, bool play, bool looping, bool useAA)
    : m_UseAA(useAA),
      m_State(play ? State::PLAY : State::PAUSE),
      m_Interval(41),
      m_Looping(looping),
      m_Cooldown(0) {
    if (IsWebmPath(mediaPath)) {
        if (!LoadVideoPosterFrame(mediaPath)) {
            LOG_ERROR("Failed to load WebM animation: '{}'", mediaPath);
        }
        return;
    }

    m_GifAnimation.reset(IMG_LoadAnimation(mediaPath.c_str()));
    if (!m_GifAnimation || m_GifAnimation->count <= 0) {
        LOG_ERROR("Failed to load GIF animation: '{}'", mediaPath);
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
    if (!m_VideoFrames.empty()) return m_VideoFrames.size();
    return m_StreamFrames ? m_FramePaths.size() : m_Frames.size();
}

std::shared_ptr<Util::Image> Animation::GetCurrentImage() const {
    if (m_GifAnimation) {
        EnsureGifFrameLoaded();
        return m_StreamFrame;
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
    SDL_Surface *surface = m_VideoFrames[targetIndex];
    if (!m_StreamFrame) {
        m_StreamFrame = std::make_shared<Util::Image>(surface, m_UseAA);
        m_LoadedVideoIndex = targetIndex;
        return;
    }
    if (m_LoadedVideoIndex == targetIndex) return;
    m_StreamFrame->SetSurface(surface);
    m_LoadedVideoIndex = targetIndex;
}

bool Animation::LoadVideoPosterFrame(const std::string &mediaPath) {
    // Cache decoded frames across Animation instances sharing the same video.
    // Kept alive for the process lifetime to avoid GL teardown issues.
    struct VideoCache {
        std::vector<SDL_Surface*> surfaces;
        double intervalMs = 41.0;
    };
    static auto& videoCache =
        *new std::unordered_map<std::string, VideoCache>();

    if (const auto it = videoCache.find(mediaPath); it != videoCache.end()) {
        m_VideoFrames = it->second.surfaces;
        m_Interval = it->second.intervalMs;
        return !m_VideoFrames.empty();
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

    // Probe video frame rate (e.g. "60/1" → 60 fps → ~16.67 ms/frame)
    double intervalMs = 41.0; // default fallback (~24fps)
    {
        const std::string fpsCmd =
            "ffprobe -v error -select_streams v:0 "
            "-show_entries stream=r_frame_rate "
            "-of csv=p=0:s=x " + ShellQuote(mediaPath);
        FILE *fpsPipe = popen(fpsCmd.c_str(), "r");
        if (fpsPipe) {
            char buf[64] = {};
            if (fgets(buf, sizeof(buf), fpsPipe)) {
                int num = 0, den = 1;
                if (sscanf(buf, "%d/%d", &num, &den) >= 1 && num > 0) {
                    if (den <= 0) den = 1;
                    intervalMs = 1000.0 * static_cast<double>(den) / static_cast<double>(num);
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
    const std::string command =
        "ffmpeg -nostdin -hide_banner -loglevel quiet -i " + ShellQuote(mediaPath) +
        " -an -vf " + ShellQuote(filter) +
        " -f rawvideo -pix_fmt rgba -";

    FILE *pipe = popen(command.c_str(), "r");
    if (pipe == nullptr) return false;

    std::vector<unsigned char> frameBuffer(frameSizeBytes);
    std::vector<SDL_Surface*> surfaces;

    while (true) {
        std::size_t totalRead = 0;
        while (totalRead < frameSizeBytes) {
            const std::size_t readNow = fread(frameBuffer.data() + totalRead, 1,
                                               frameSizeBytes - totalRead, pipe);
            if (readNow == 0) break;
            totalRead += readNow;
        }
        if (totalRead != frameSizeBytes) break; // EOF or short read → done

        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
            frameBuffer.data(),
            frameWidth,
            frameHeight,
            32,
            frameWidth * 4,
            SDL_PIXELFORMAT_ABGR8888);
        if (surface == nullptr) break;

        SDL_Surface *copiedSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
        SDL_FreeSurface(surface);
        
        if (copiedSurface == nullptr) break;
        surfaces.push_back(copiedSurface);
    }
    pclose(pipe);

    if (surfaces.empty()) return false;

    // Cache and apply
    videoCache[mediaPath] = {surfaces, intervalMs};
    m_VideoFrames = surfaces;
    return true;
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
    if (m_StreamFrames || m_GifAnimation) {
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

void Animation::Pause() {
    if (m_State == State::PLAY || m_State == State::COOLDOWN) {
        m_State = State::PAUSE;
    }
}

void Animation::Update() {
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

    m_TimeBetweenFrameUpdate += Util::Time::GetDeltaTimeMs();
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
