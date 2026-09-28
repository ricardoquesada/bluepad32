/*
 * Copyright 2020 The Android Open Source Project
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
 * @file texture_asset_loader.h
 * @brief POSIX `libpng` + OpenGL 2D sprite texture loader for the controller UI.
 *
 * Architectural Role & Threading Context:
 *   Adapts the Android Game Development Kit (AGDK) `TextureAssetLoader` from Android's
 *   `AAssetManager` to standard POSIX file I/O, `libpng`, and OpenGL (`glTexImage2D`).
 *
 *   To support deterministic headless unit testing (`test_posix_imgui`) without an
 *   X11/Wayland display or OpenGL context, the loader splits texture loading into two
 *   distinct layers:
 *     1. CPU-side path resolution and PNG-to-RGBA decoding (`ResolveAssetPath()` and
 *        `DecodePNGToRGBA()`), which have zero OpenGL dependencies and can be invoked
 *        from any thread or headless test runner.
 *     2. GPU-side OpenGL 2D texture creation and deletion (`loadTextureAsset()` and
 *        `unloadTextureAsset()`), which must be called on Thread 1 (the main GLFW/ImGui
 *        thread) while a valid OpenGL context is current.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

/// 64-bit opaque texture handle storing an OpenGL `GLuint` texture ID, compatible with
/// Dear ImGui 1.93+ `ImTextureID` (`ImU64`) via `static_cast<ImTextureID>(handle)`.
typedef uint64_t TextureAssetHandle;

class TextureAssetLoader {
   public:
    /// Sentinel value returned when a texture asset fails to resolve, decode, or upload.
    static constexpr TextureAssetHandle INVALID_TEXTURE = 0xFFFFFFFFFFFFFFFFULL;

    /**
     * @brief Resolves a relative asset filename (e.g., `"gamecontroller/Button_A_Active.png"`)
     *        to an existing readable file path on disk.
     *
     * Searches candidate directories in order:
     *   1. `<executable_dir>/assets/<filename>`
     *   2. `<executable_dir>/../assets/<filename>`
     *   3. `./assets/<filename>`
     *   4. `../assets/<filename>`
     *   5. `examples/posix_imgui/assets/<filename>`
     *   6. `<filename>` (direct/absolute path)
     *
     * Thread-safety: Safe to call without an OpenGL context from any thread.
     *
     * @param filename Relative path under `assets/` (or direct path).
     * @return Resolved filesystem path if found and readable, or `""` otherwise.
     */
    static std::string ResolveAssetPath(const char* filename);

    /**
     * @brief Decodes a PNG asset file into a tightly packed 8-bit/channel RGBA pixel buffer
     *        on the CPU without requiring an active OpenGL context.
     *
     * Example:
     * @code
     * uint32_t w = 0, h = 0;
     * std::vector<uint8_t> rgba;
     * if (TextureAssetLoader::DecodePNGToRGBA("gamecontroller/Button_A_Active.png", &w, &h, &rgba)) {
     *     // rgba.size() == w * h * 4
     * }
     * @endcode
     *
     * @param filename        Asset path passed to `ResolveAssetPath()`.
     * @param[out] out_width  Receives decoded image width in pixels (may be null).
     * @param[out] out_height Receives decoded image height in pixels (may be null).
     * @param[out] out_rgba   Receives `width * height * 4` bytes of RGBA8 pixel data (must not be null).
     * @return True if the file is a valid 8-bit RGBA PNG and was decoded completely; false on error.
     */
    static bool DecodePNGToRGBA(const char* filename,
                                uint32_t* out_width,
                                uint32_t* out_height,
                                std::vector<uint8_t>* out_rgba);

    /**
     * @brief Decodes a PNG asset file and uploads it into a new OpenGL `GL_TEXTURE_2D` object.
     *
     * Must be called on Thread 1 (the main UI thread) with an active OpenGL context.
     *
     * @param filename           Relative asset path under `assets/` (e.g., `"gamecontroller/Button_A_Active.png"`).
     * @param[out] textureWidth  Receives texture width in pixels on success (may be null).
     * @param[out] textureHeight Receives texture height in pixels on success (may be null).
     * @return OpenGL texture ID cast to `TextureAssetHandle`, or `INVALID_TEXTURE` on failure.
     */
    static TextureAssetHandle loadTextureAsset(const char* filename, uint32_t* textureWidth, uint32_t* textureHeight);

    /**
     * @brief Deletes an OpenGL 2D texture previously created by `loadTextureAsset()`.
     *
     * Must be called on Thread 1 (the main UI thread) before destroying the OpenGL context.
     * Passing `INVALID_TEXTURE` or `0` is a safe no-op.
     *
     * @param textureReference Handle returned by `loadTextureAsset()`.
     */
    static void unloadTextureAsset(const TextureAssetHandle textureReference);
};
