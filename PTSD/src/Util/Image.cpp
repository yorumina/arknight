#include "Util/Image.hpp"

#include "Util/Logger.hpp"
#include "pch.hpp"

#include "Core/Texture.hpp"
#include "Core/TextureUtils.hpp"
#include "Util/MissingTexture.hpp"

#include "config.hpp"
#include <glm/fwd.hpp>

std::shared_ptr<SDL_Surface> LoadSurface(const std::string &filepath) {
    auto surface = std::shared_ptr<SDL_Surface>(IMG_Load(filepath.c_str()),
                                                SDL_FreeSurface);

    if (surface == nullptr) {
        surface = {Util::GetMissingImageTextureSDLSurface(), SDL_FreeSurface};
        LOG_ERROR("Failed to load image: '{}'", filepath);
        LOG_ERROR("{}", IMG_GetError());
    }

    return surface;
}

bool IsSupportedTextureSurfaceFormat(Uint32 format) {
    switch (format) {
    case SDL_PIXELFORMAT_RGB24:
    case SDL_PIXELFORMAT_BGR24:
    case SDL_PIXELFORMAT_XRGB8888:
    case SDL_PIXELFORMAT_ARGB8888:
    case SDL_PIXELFORMAT_XBGR8888:
    case SDL_PIXELFORMAT_ABGR8888:
        return true;
    default:
        return false;
    }
}

namespace Util {
Image::Image(const std::string &filepath, bool useAA)
    : m_Path(filepath), m_UseAA(useAA) {
    InitDrawableResources();

    auto surface = LoadSurface(filepath);
    SetSurface(surface.get());
    UseAntiAliasing(useAA);
}

Image::Image(SDL_Surface *surface, bool useAA) {
    m_UseAA = useAA;
    InitDrawableResources();
    SetSurface(surface);
}

void Image::InitDrawableResources() {
    if (s_Program == nullptr) {
        InitProgram();
    }
    if (s_VertexArray == nullptr) {
        InitVertexArray();
    }

    m_UniformBuffer = std::make_unique<Core::UniformBuffer<Core::Matrices>>(
        *s_Program, "Matrices", 0);
}

void Image::SetSurface(SDL_Surface *surface) {
    if (surface == nullptr) {
        surface = GetMissingImageTextureSDLSurface();
    }

    auto converted = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>{nullptr, SDL_FreeSurface};
    if (!IsSupportedTextureSurfaceFormat(surface->format->format)) {
        converted.reset(SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0));
        if (converted == nullptr) {
            LOG_ERROR("Failed to convert image surface");
            surface = GetMissingImageTextureSDLSurface();
        } else {
            surface = converted.get();
        }
    }
    const GLint format = Core::SdlFormatToGlFormat(surface->format->format);

    if (m_Texture == nullptr) {
        m_Texture = std::make_unique<Core::Texture>(
            format, surface->w, surface->h, surface->pixels, m_UseAA);
    } else {
        m_Texture->UpdateData(format, surface->w, surface->h, surface->pixels);
    }
    m_Size = {surface->w, surface->h};
}

void Image::SetImage(const std::string &filepath) {
    auto surface = LoadSurface(filepath);

    SetSurface(surface.get());
    m_Path = filepath;
}

void Image::UseAntiAliasing(bool useAA) {
    m_UseAA = useAA;
    m_Texture->UseAntiAliasing(useAA);
}

void Image::Draw(const Core::Matrices &data) {
    m_UniformBuffer->SetData(0, data);

    m_Texture->Bind(UNIFORM_SURFACE_LOCATION);
    s_Program->Bind();
    s_Program->Validate();

    s_VertexArray->Bind();
    s_VertexArray->DrawTriangles();
}

void Image::InitProgram() {
    // TODO: Create `BaseProgram` from `Program` and pass it into `Drawable`
    s_Program =
        std::make_unique<Core::Program>(PTSD_ASSETS_DIR "/shaders/Base.vert",
                                        PTSD_ASSETS_DIR "/shaders/Base.frag");
    s_Program->Bind();

    GLint location = glGetUniformLocation(s_Program->GetId(), "surface");
    glUniform1i(location, UNIFORM_SURFACE_LOCATION);
}

void Image::InitVertexArray() {
    s_VertexArray = std::make_unique<Core::VertexArray>();

    // NOLINTBEGIN
    // These are vertex data for the rectangle but clang-tidy has magic
    // number warnings

    // Vertex
    s_VertexArray->AddVertexBuffer(std::make_unique<Core::VertexBuffer>(
        std::vector<float>{
            -0.5F, 0.5F,  //
            -0.5F, -0.5F, //
            0.5F, -0.5F,  //
            0.5F, 0.5F,   //
        },
        2));

    // UV
    s_VertexArray->AddVertexBuffer(std::make_unique<Core::VertexBuffer>(
        std::vector<float>{
            0.0F, 0.0F, //
            0.0F, 1.0F, //
            1.0F, 1.0F, //
            1.0F, 0.0F, //
        },
        2));

    // Index
    s_VertexArray->SetIndexBuffer(
        std::make_unique<Core::IndexBuffer>(std::vector<unsigned int>{
            0, 1, 2, //
            0, 2, 3, //
        }));
    // NOLINTEND
}

std::unique_ptr<Core::Program> Image::s_Program = nullptr;
std::unique_ptr<Core::VertexArray> Image::s_VertexArray = nullptr;

Util::AssetStore<std::shared_ptr<SDL_Surface>> Image::s_Store(LoadSurface);
} // namespace Util
