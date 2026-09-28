/*
 * Copyright 2021 The Android Open Source Project
 * Copyright 2026 Ricardo Quesada
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file texture_asset_loader.cpp
 * @brief Implementation of POSIX path resolution, `libpng` RGBA decoding, and OpenGL texture upload.
 *
 * Key Design & Safety Details:
 *   1. Direct `FILE*` I/O (`png_init_io` + `png_set_sig_bytes`):
 *      Avoids the reference AGDK sample's custom `PNG_Memory_Read` callback, which had an
 *      off-by-one `<` bounds check that rejected reads reaching the final byte of the stream.
 *   2. C++ `setjmp` / `longjmp` Stack Frame Isolation (`read_png_stream_into_rgba`):
 *      `libpng` reports decoding errors on malformed PNG chunks via `longjmp(png_jmpbuf(png_ptr))`.
 *      In C++, jumping via `longjmp` across a stack frame that owns local objects with
 *      non-trivial destructors (such as `std::string` or `std::vector`) is Undefined Behavior.
 *      Therefore, `setjmp` is isolated inside `read_png_stream_into_rgba()`, which declares
 *      only POD locals and writes into caller-owned pointers (`out_rgba`), while
 *      `DecodePNGToRGBA()` owns `resolved_path` and guarantees `png_destroy_read_struct` and
 *      `fclose` cleanup.
 */

#include "texture_asset_loader.h"

#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(__linux__)
#include <limits.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#endif

#if defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include <png.h>

namespace {

constexpr size_t kPngSigLength = 8;

bool file_exists_readable(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (fp != nullptr) {
        std::fclose(fp);
        return true;
    }
    return false;
}

// Resolves the directory containing the running binary via `/proc/self/exe` (Linux)
// or `_NSGetExecutablePath` (macOS) so assets can be located regardless of `cwd`.
std::string get_executable_dir() {
#if defined(__linux__)
    char buf[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';
        std::string full_path(buf);
        size_t slash = full_path.find_last_of('/');
        if (slash != std::string::npos) {
            return full_path.substr(0, slash);
        }
    }
#elif defined(__APPLE__)
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) == 0) {
        std::string full_path(buf);
        size_t slash = full_path.find_last_of('/');
        if (slash != std::string::npos) {
            return full_path.substr(0, slash);
        }
    }
#endif
    return "";
}

// Isolated `setjmp` helper: must NOT instantiate any C++ objects with non-trivial
// destructors in this stack frame so `libpng` `longjmp` on corrupt files is UB-free.
bool read_png_stream_into_rgba(FILE* fp,
                               png_structp png_ptr,
                               png_infop info_ptr,
                               uint32_t* out_width,
                               uint32_t* out_height,
                               std::vector<uint8_t>* out_rgba) {
    if (setjmp(png_jmpbuf(png_ptr))) {
        return false;
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, static_cast<int>(kPngSigLength));
    png_read_info(png_ptr, info_ptr);

    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bit_depth = 0;
    int color_type = -1;
    if (png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, nullptr, nullptr, nullptr) != 1) {
        return false;
    }

    if (width == 0 || height == 0 || bit_depth != 8 || color_type != PNG_COLOR_TYPE_RGB_ALPHA) {
        return false;
    }

    const size_t row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    if (row_bytes < static_cast<size_t>(width) * 4) {
        return false;
    }

    out_rgba->resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    for (png_uint_32 y = 0; y < height; ++y) {
        png_bytep dst_row = out_rgba->data() + static_cast<size_t>(y) * static_cast<size_t>(width) * 4;
        png_read_row(png_ptr, dst_row, nullptr);
    }

    if (out_width != nullptr) {
        *out_width = static_cast<uint32_t>(width);
    }
    if (out_height != nullptr) {
        *out_height = static_cast<uint32_t>(height);
    }
    return true;
}

// Uploads an 8-bit/channel RGBA pixel buffer to a new OpenGL 2D texture object.
// Sets `GL_UNPACK_ALIGNMENT` to 1 byte and clamps UV coordinates to `GL_CLAMP_TO_EDGE`
// to prevent border artifacts when scaling button/stick sprites in Dear ImGui.
TextureAssetHandle createTexture(uint32_t texWidth, uint32_t texHeight, const void* texPixels) {
    TextureAssetHandle returnHandle = TextureAssetLoader::INVALID_TEXTURE;
    glGetError();
    GLuint textureID = 0;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(texWidth), static_cast<GLsizei>(texHeight), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, texPixels);
    GLenum glErr = glGetError();
    if (glErr == GL_NO_ERROR && textureID != 0) {
        returnHandle = static_cast<TextureAssetHandle>(textureID);
    } else if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
    return returnHandle;
}

}  // namespace

std::string TextureAssetLoader::ResolveAssetPath(const char* filename) {
    if (filename == nullptr || filename[0] == '\0') {
        return "";
    }

    std::vector<std::string> candidates;
    const std::string exe_dir = get_executable_dir();
    if (!exe_dir.empty()) {
        candidates.push_back(exe_dir + "/assets/" + filename);
        candidates.push_back(exe_dir + "/../assets/" + filename);
    }
    candidates.push_back(std::string("./assets/") + filename);
    candidates.push_back(std::string("../assets/") + filename);
    candidates.push_back(std::string("examples/posix_imgui/assets/") + filename);
    candidates.push_back(std::string(filename));

    for (const std::string& candidate : candidates) {
        if (file_exists_readable(candidate)) {
            return candidate;
        }
    }
    return "";
}

bool TextureAssetLoader::DecodePNGToRGBA(const char* filename,
                                         uint32_t* out_width,
                                         uint32_t* out_height,
                                         std::vector<uint8_t>* out_rgba) {
    if (out_rgba == nullptr) {
        return false;
    }
    out_rgba->clear();

    const std::string resolved_path = ResolveAssetPath(filename);
    if (resolved_path.empty()) {
        return false;
    }

    FILE* fp = std::fopen(resolved_path.c_str(), "rb");
    if (fp == nullptr) {
        return false;
    }

    png_byte sig[kPngSigLength];
    if (std::fread(sig, 1, kPngSigLength, fp) != kPngSigLength || png_sig_cmp(sig, 0, kPngSigLength) != 0) {
        std::fclose(fp);
        return false;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png_ptr == nullptr) {
        std::fclose(fp);
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        std::fclose(fp);
        return false;
    }

    const bool ok = read_png_stream_into_rgba(fp, png_ptr, info_ptr, out_width, out_height, out_rgba);
    if (!ok) {
        out_rgba->clear();
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    std::fclose(fp);
    return ok;
}

TextureAssetHandle TextureAssetLoader::loadTextureAsset(const char* filename,
                                                        uint32_t* textureWidth,
                                                        uint32_t* textureHeight) {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba_pixels;
    if (!DecodePNGToRGBA(filename, &width, &height, &rgba_pixels)) {
        return INVALID_TEXTURE;
    }

    TextureAssetHandle handle = createTexture(width, height, rgba_pixels.data());
    if (handle != INVALID_TEXTURE) {
        if (textureWidth != nullptr) {
            *textureWidth = width;
        }
        if (textureHeight != nullptr) {
            *textureHeight = height;
        }
    }
    return handle;
}

void TextureAssetLoader::unloadTextureAsset(const TextureAssetHandle textureReference) {
    if (textureReference != INVALID_TEXTURE && textureReference != 0) {
        GLuint textureID = static_cast<GLuint>(textureReference);
        glDeleteTextures(1, &textureID);
    }
}
