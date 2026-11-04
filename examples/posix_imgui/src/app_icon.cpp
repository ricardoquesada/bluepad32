// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Ricardo Quesada
// http://retro.moe/unijoysticle2

/**
 * @file app_icon.cpp
 * @brief Implementation of the Bluepad32 Signed Distance Field (SDF) application icon
 *        rasterizer and macOS Cocoa / X11 platform icon dispatchers.
 */

#include "app_icon.h"

#include <algorithm>
#include <array>
#include <cmath>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#endif

namespace {

/// Canonical Bluepad32 brand navy blue (`#1B467B` from `docs/images/logo_no_text.png`).
constexpr float kBrandNavyR = 27.0f;
constexpr float kBrandNavyG = 70.0f;
constexpr float kBrandNavyB = 123.0f;

/// Porcelain white card fill (`#FAFCFF`) for high contrast on both Dark and Light Docks.
constexpr float kCardFillR = 250.0f;
constexpr float kCardFillG = 252.0f;
constexpr float kCardFillB = 255.0f;

/// Subtle slate-blue inner card rim (`#D4DFEE`).
constexpr float kCardBorderR = 212.0f;
constexpr float kCardBorderG = 223.0f;
constexpr float kCardBorderB = 238.0f;

/// Signed distance from point `(px, py)` to a circle centered at `(cx, cy)` with radius `r`.
constexpr float SdfCircle(float px, float py, float cx, float cy, float r) noexcept {
    const float dx = px - cx;
    const float dy = py - cy;
    return std::sqrt(dx * dx + dy * dy) - r;
}

/// Signed distance from point `(px, py)` to an axis-aligned rounded box centered at `(cx, cy)`
/// with half-extents `(hx, hy)` and corner radius `r`.
constexpr float SdfRoundedBox(float px, float py, float cx, float cy, float hx, float hy, float r) noexcept {
    const float qx = std::abs(px - cx) - (hx - r);
    const float qy = std::abs(py - cy) - (hy - r);
    const float out_x = std::max(qx, 0.0f);
    const float out_y = std::max(qy, 0.0f);
    const float outside_dist = std::sqrt(out_x * out_x + out_y * out_y);
    const float inside_dist = std::min(std::max(qx, qy), 0.0f);
    return outside_dist + inside_dist - r;
}

/// Signed distance from point `(px, py)` to a capsule (line segment `(ax, ay)` -> `(bx, by)`)
/// with half-thickness (radius) `r`.
constexpr float SdfSegment(float px, float py, float ax, float ay, float bx, float by, float r) noexcept {
    const float pax = px - ax;
    const float pay = py - ay;
    const float bax = bx - ax;
    const float bay = by - ay;
    const float ba_len_sq = bax * bax + bay * bay;
    const float h = (ba_len_sq > 0.0f) ? std::clamp((pax * bax + pay * bay) / ba_len_sq, 0.0f, 1.0f) : 0.0f;
    const float dx = pax - bax * h;
    const float dy = pay - bay * h;
    return std::sqrt(dx * dx + dy * dy) - r;
}

/// Smooth CSG subtraction (`base \ cut`) with fillet radius `k`, producing a continuous fillet
/// where the bottom-center arch meets the left and right controller grips.
constexpr float SdfSmoothSubtract(float d_base, float d_cut, float k) noexcept {
    const float h = std::clamp(0.5f - 0.5f * (d_base + d_cut) / k, 0.0f, 1.0f);
    return (d_base * (1.0f - h) + (-d_cut) * h) + k * h * (1.0f - h);
}

/**
 * @brief Evaluates the signed distance in screen pixels from `(px, py)` (in `[0, 256]^2`)
 *        to the canonical Bluepad32 gamepad-and-circuit-traces silhouette.
 *
 * All logo coordinates mirror the `viewBox="0 0 1024 1024"` geometry of
 * `docs/images/bluepad32_logo.svg`, centered at `(531.5, 408.0)` and scaled by `kLogoScale`
 * into the 256x256 icon canvas.
 */
constexpr float EvaluateBluepad32LogoSdfPx(float px, float py) noexcept {
    constexpr float kLogoScale = 0.39f;
    constexpr float kSvgCenterX = 531.5f;
    constexpr float kSvgCenterY = 414.0f;
    constexpr float kCanvasCenter = 128.0f;

    const float sx = kSvgCenterX + (px - kCanvasCenter) / kLogoScale;
    const float sy = kSvgCenterY + (py - kCanvasCenter) / kLogoScale;

    // 1. Gamepad body: left grip circle + right grip circle + central bridge,
    //    smoothly hollowed at the bottom center arch between the grips.
    const float d_left_grip = SdfCircle(sx, sy, 415.5f, 499.0f, 101.0f);
    const float d_right_grip = SdfCircle(sx, sy, 647.5f, 499.0f, 101.0f);
    const float d_bridge = SdfRoundedBox(sx, sy, 531.5f, 486.0f, 116.0f, 88.0f, 4.0f);
    const float d_body_raw = std::min({d_left_grip, d_right_grip, d_bridge});
    const float d_bottom_arch = SdfCircle(sx, sy, 531.5f, 602.0f, 56.0f);
    const float d_body = SdfSmoothSubtract(d_body_raw, d_bottom_arch, 20.0f);

    // 2. Four circuit-board traces and terminal pads extending above the top edge (`sy = 398`).
    constexpr float kTraceRadius = 6.2f;
    constexpr float kPadOuterRadius = 18.5f;
    constexpr float kPadInnerRadius = 7.5f;

    const float d_trace_1 = SdfSegment(sx, sy, 443.0f, 323.2f, 443.0f, 405.0f, kTraceRadius);
    const float d_trace_2a = SdfSegment(sx, sy, 443.0f, 232.0f, 481.0f, 270.0f, kTraceRadius);
    const float d_trace_2b = SdfSegment(sx, sy, 481.0f, 270.0f, 481.0f, 405.0f, kTraceRadius);
    const float d_trace_3 = SdfSegment(sx, sy, 585.0f, 321.7f, 585.0f, 405.0f, kTraceRadius);
    const float d_trace_4 = SdfSegment(sx, sy, 623.0f, 233.5f, 623.0f, 405.0f, kTraceRadius);

    const float d_pad_1 = SdfCircle(sx, sy, 443.0f, 323.2f, kPadOuterRadius);
    const float d_pad_2 = SdfCircle(sx, sy, 443.0f, 232.0f, kPadOuterRadius);
    const float d_pad_3 = SdfCircle(sx, sy, 585.0f, 321.7f, kPadOuterRadius);
    const float d_pad_4 = SdfCircle(sx, sy, 623.0f, 233.5f, kPadOuterRadius);

    const float d_hole_1 = SdfCircle(sx, sy, 443.0f, 323.2f, kPadInnerRadius);
    const float d_hole_2 = SdfCircle(sx, sy, 443.0f, 232.0f, kPadInnerRadius);
    const float d_hole_3 = SdfCircle(sx, sy, 585.0f, 321.7f, kPadInnerRadius);
    const float d_hole_4 = SdfCircle(sx, sy, 623.0f, 233.5f, kPadInnerRadius);
    const float d_pad_holes = std::min({d_hole_1, d_hole_2, d_hole_3, d_hole_4});

    const float d_traces_solid =
        std::min({d_trace_1, d_trace_2a, d_trace_2b, d_trace_3, d_trace_4, d_pad_1, d_pad_2, d_pad_3, d_pad_4});
    const float d_traces = std::max(d_traces_solid, -d_pad_holes);

    // Combine gamepad body and circuit traces.
    float d_logo = std::min(d_body, d_traces);

    // 3. Left D-pad: subtract white outer cross cutout, then union inner navy cross.
    const float d_dpad_outer = std::min(SdfRoundedBox(sx, sy, 415.5f, 513.0f, 55.0f, 25.0f, 5.0f),
                                        SdfRoundedBox(sx, sy, 415.5f, 513.0f, 25.0f, 55.0f, 5.0f));
    const float d_dpad_inner = std::min(SdfRoundedBox(sx, sy, 415.5f, 513.0f, 43.0f, 13.0f, 3.5f),
                                        SdfRoundedBox(sx, sy, 415.5f, 513.0f, 13.0f, 43.0f, 3.5f));
    d_logo = std::max(d_logo, -d_dpad_outer);
    d_logo = std::min(d_logo, d_dpad_inner);

    // 4. Right action button cluster: subtract white circular plate, then union 4 navy buttons.
    const float d_btn_plate = SdfCircle(sx, sy, 647.5f, 513.0f, 64.9f);
    constexpr float kBtnOffset = 32.44f;
    constexpr float kBtnRadius = 16.22f;
    const float d_buttons = std::min({
        SdfCircle(sx, sy, 647.5f, 513.0f - kBtnOffset, kBtnRadius),
        SdfCircle(sx, sy, 647.5f, 513.0f + kBtnOffset, kBtnRadius),
        SdfCircle(sx, sy, 647.5f - kBtnOffset, 513.0f, kBtnRadius),
        SdfCircle(sx, sy, 647.5f + kBtnOffset, 513.0f, kBtnRadius),
    });
    d_logo = std::max(d_logo, -d_btn_plate);
    d_logo = std::min(d_logo, d_buttons);

    return d_logo * kLogoScale;
}

}  // namespace

void BuildAppIconRgba(std::span<uint8_t, kAppIconByteCount> out_rgba) noexcept {
    constexpr float kCanvasCenter = 128.0f;
    constexpr float kCardHalfExtent = 112.0f;  // 224x224 squircle card with 16px Dock margin per side
    constexpr float kCardCornerRadius = 50.0f;
    constexpr float kBorderThickness = 3.0f;

    for (int y = 0; y < kAppIconSize; ++y) {
        for (int x = 0; x < kAppIconSize; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;

            // Signed distance in pixels to the outer rounded-square card.
            const float d_card = SdfRoundedBox(px, py, kCanvasCenter, kCanvasCenter, kCardHalfExtent, kCardHalfExtent,
                                               kCardCornerRadius);

            const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(kAppIconSize) + static_cast<size_t>(x)) *
                               kAppIconChannels;

            // Fast reject pixels completely outside the anti-aliased card boundary.
            if (d_card >= 1.0f) {
                out_rgba[idx + 0] = 0;
                out_rgba[idx + 1] = 0;
                out_rgba[idx + 2] = 0;
                out_rgba[idx + 3] = 0;
                continue;
            }

            // Anti-aliased card coverage (`0.0` outside .. `1.0` inside) and subtle inner border blend.
            const float card_alpha = std::clamp(0.5f - d_card, 0.0f, 1.0f);
            const float border_mix = std::clamp(1.0f - (-d_card / kBorderThickness), 0.0f, 1.0f);

            const float bg_r = kCardFillR * (1.0f - border_mix) + kCardBorderR * border_mix;
            const float bg_g = kCardFillG * (1.0f - border_mix) + kCardBorderG * border_mix;
            const float bg_b = kCardFillB * (1.0f - border_mix) + kCardBorderB * border_mix;

            // Evaluate the Bluepad32 logo SDF with 2x2 subpixel supersampling near edges.
            const float d_center = EvaluateBluepad32LogoSdfPx(px, py);
            float logo_cov = 0.0f;
            if (d_center <= -1.2f) {
                logo_cov = 1.0f;
            } else if (d_center >= 1.2f) {
                logo_cov = 0.0f;
            } else {
                constexpr std::array<float, 2> kSubOffsets{-0.25f, 0.25f};
                float sum = 0.0f;
                for (float oy : kSubOffsets) {
                    for (float ox : kSubOffsets) {
                        const float sd = EvaluateBluepad32LogoSdfPx(px + ox, py + oy);
                        sum += std::clamp(0.5f - sd, 0.0f, 1.0f);
                    }
                }
                logo_cov = sum * 0.25f;
            }

            const float final_r = bg_r * (1.0f - logo_cov) + kBrandNavyR * logo_cov;
            const float final_g = bg_g * (1.0f - logo_cov) + kBrandNavyG * logo_cov;
            const float final_b = bg_b * (1.0f - logo_cov) + kBrandNavyB * logo_cov;

            out_rgba[idx + 0] = static_cast<uint8_t>(std::clamp(final_r + 0.5f, 0.0f, 255.0f));
            out_rgba[idx + 1] = static_cast<uint8_t>(std::clamp(final_g + 0.5f, 0.0f, 255.0f));
            out_rgba[idx + 2] = static_cast<uint8_t>(std::clamp(final_b + 0.5f, 0.0f, 255.0f));
            out_rgba[idx + 3] = static_cast<uint8_t>(std::clamp(card_alpha * 255.0f + 0.5f, 0.0f, 255.0f));
        }
    }
}

void ApplyApplicationIcon(GLFWwindow* window) noexcept {
    if (window == nullptr) {
        return;
    }

    std::array<uint8_t, kAppIconByteCount> rgba_pixels{};
    BuildAppIconRgba(std::span<uint8_t, kAppIconByteCount>{rgba_pixels});

#if defined(__APPLE__)
    // Why we use CoreGraphics + Cocoa's `[NSApp setApplicationIconImage:]` on macOS:
    //   1. In GLFW's Cocoa backend (`_glfwSetWindowIconCocoa` in `cocoa_window.m`),
    //      `glfwSetWindowIcon()` is unimplemented and emits `GLFW_FEATURE_UNAVAILABLE`
    //      ("Cocoa: Regular windows do not have icons on macOS").
    //   2. Because `bluepad32_posix_imgui_example_app` is built as an unbundled CLI executable
    //      (so developers can run `sudo ./bluepad32_posix_imgui_example_app` directly), macOS
    //      has no `.app/Contents/Info.plist` icon bundle to read. Assigning `NSImage` to
    //      `NSApplication.sharedApplication.applicationIconImage` updates the macOS Dock tile
    //      and `Cmd+Tab` Application Switcher icon at runtime.
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    if (color_space == nullptr) {
        return;
    }

    CGDataProviderRef provider = CGDataProviderCreateWithData(nullptr, rgba_pixels.data(), rgba_pixels.size(), nullptr);
    if (provider == nullptr) {
        CGColorSpaceRelease(color_space);
        return;
    }

    const CGBitmapInfo bitmap_info =
        static_cast<CGBitmapInfo>(kCGImageAlphaLast) | static_cast<CGBitmapInfo>(kCGBitmapByteOrderDefault);
    CGImageRef cg_image = CGImageCreate(static_cast<size_t>(kAppIconSize), static_cast<size_t>(kAppIconSize), 8, 32,
                                        static_cast<size_t>(kAppIconSize) * kAppIconChannels, color_space, bitmap_info,
                                        provider, nullptr, true, kCGRenderingIntentDefault);

    if (cg_image != nullptr) {
        const Class ns_app_class = objc_getClass("NSApplication");
        const Class ns_image_class = objc_getClass("NSImage");
        if (ns_app_class != nullptr && ns_image_class != nullptr) {
            using SharedAppFn = id (*)(Class, SEL);
            using AllocFn = id (*)(Class, SEL);
            using InitWithCgImageFn = id (*)(id, SEL, CGImageRef, CGSize);
            using SetIconFn = void (*)(id, SEL, id);
            using ReleaseFn = void (*)(id, SEL);

            const id ns_app =
                reinterpret_cast<SharedAppFn>(objc_msgSend)(ns_app_class, sel_registerName("sharedApplication"));
            const id ns_image_alloc =
                reinterpret_cast<AllocFn>(objc_msgSend)(ns_image_class, sel_registerName("alloc"));
            if (ns_app != nullptr && ns_image_alloc != nullptr) {
                const CGSize icon_size =
                    CGSizeMake(static_cast<CGFloat>(kAppIconSize), static_cast<CGFloat>(kAppIconSize));
                const id ns_image = reinterpret_cast<InitWithCgImageFn>(objc_msgSend)(
                    ns_image_alloc, sel_registerName("initWithCGImage:size:"), cg_image, icon_size);
                if (ns_image != nullptr) {
                    reinterpret_cast<SetIconFn>(objc_msgSend)(ns_app, sel_registerName("setApplicationIconImage:"),
                                                              ns_image);
                    reinterpret_cast<ReleaseFn>(objc_msgSend)(ns_image, sel_registerName("release"));
                }
            }
        }
        CGImageRelease(cg_image);
    }

    CGDataProviderRelease(provider);
    CGColorSpaceRelease(color_space);
#else
    GLFWimage icon_image{};
    icon_image.width = kAppIconSize;
    icon_image.height = kAppIconSize;
    icon_image.pixels = rgba_pixels.data();
#if defined(GLFW_PLATFORM_X11)
    // In GLFW 3.4+, Wayland emits `GLFW_FEATURE_UNAVAILABLE` on `glfwSetWindowIcon()`.
    // Guard with `glfwGetPlatform() == GLFW_PLATFORM_X11` so X11 sets `_NET_WM_ICON`
    // while Wayland sessions remain warning-free.
    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        glfwSetWindowIcon(window, 1, &icon_image);
    }
#else
    glfwSetWindowIcon(window, 1, &icon_image);
#endif
#endif
}
