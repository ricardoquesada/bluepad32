// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file app_icon.h
 * @brief Self-contained Bluepad32 application icon rasterizer and platform icon dispatcher.
 *
 * Architectural Role:
 *   Provides a zero-dependency, CWD-independent application icon for the Bluepad32 POSIX
 *   Dear ImGui Controller Tester across macOS (Cocoa Dock / Cmd+Tab App Switcher) and
 *   Linux/POSIX (X11 `_NET_WM_ICON` window and taskbar icon):
 *     1. `BuildAppIconRgba()` rasterizes the canonical Bluepad32 gamepad-and-circuit-traces
 *        logo (`docs/images/bluepad32_logo.svg`) onto a rounded macOS-style squircle badge
 *        into an in-memory 256x256 RGBA8888 buffer using 4x4 supersampled Signed Distance
 *        Fields (SDF).
 *     2. `ApplyApplicationIcon()` dispatches the generated RGBA8888 buffer to the active
 *        platform backend (`[NSApp setApplicationIconImage:]` on macOS, where GLFW's
 *        `glfwSetWindowIcon()` is unsupported and emits `GLFW_FEATURE_UNAVAILABLE`; or
 *        `glfwSetWindowIcon()` on X11).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

struct GLFWwindow;

/// Width and height in pixels of the generated square RGBA8888 application icon.
inline constexpr int kAppIconSize = 256;

/// Number of RGBA channels per pixel (`R`, `G`, `B`, `A`).
inline constexpr size_t kAppIconChannels = 4;

/// Total byte size of the 256x256 RGBA8888 application icon buffer (`262,144` bytes).
inline constexpr size_t kAppIconByteCount =
    static_cast<size_t>(kAppIconSize) * static_cast<size_t>(kAppIconSize) * kAppIconChannels;

/**
 * @brief Rasterizes the 256x256 RGBA8888 Bluepad32 application icon into `out_rgba`.
 *
 * Pure, deterministic C++23 function safe to call in headless unit tests without an active
 * GLFW or OpenGL context.
 *
 * @param out_rgba Destination buffer of exactly `kAppIconByteCount` (`256 * 256 * 4`) bytes.
 */
void BuildAppIconRgba(std::span<uint8_t, kAppIconByteCount> out_rgba) noexcept;

/**
 * @brief Generates and applies the Bluepad32 application icon to the running process/window.
 *
 * Platform Behavior:
 *   - **macOS (`__APPLE__`)**: GLFW's `glfwSetWindowIcon()` is unimplemented on Cocoa and
 *     emits `GLFW_FEATURE_UNAVAILABLE`. Instead, this function wraps the RGBA8888 buffer in
 *     a CoreGraphics `CGImageRef` and assigns an AppKit `NSImage` to
 *     `[NSApplication sharedApplication].applicationIconImage`, populating the macOS Dock
 *     and `Cmd+Tab` Application Switcher icon even for an unbundled CLI executable.
 *   - **Linux / X11 (`!__APPLE__`)**: Forwards the RGBA8888 buffer to `glfwSetWindowIcon()`
 *     when the active GLFW platform supports window icons (X11).
 *
 * @param window Active GLFW window handle; if `nullptr`, returns immediately as a safe no-op.
 */
void ApplyApplicationIcon(GLFWwindow* window) noexcept;
