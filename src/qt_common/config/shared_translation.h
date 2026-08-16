// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 Torzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <utility>
#include <vector>
#include <QObject>
#include <QString>
#include "common/common_types.h"
#include "common/settings_enums.h"

namespace ConfigurationShared {
using TranslationMap = std::map<u32, std::pair<QString, QString>>;
using ComboboxTranslations = std::vector<std::pair<u32, QString>>;
using ComboboxTranslationMap = std::map<u32, ComboboxTranslations>;

std::unique_ptr<TranslationMap> InitializeTranslations(QObject* parent);

std::unique_ptr<ComboboxTranslationMap> ComboboxEnumeration(QObject* parent);

static const std::map<Settings::AntiAliasing, QString> anti_aliasing_texts_map = {
    {Settings::AntiAliasing::None, QStringLiteral("Отключено")},
    {Settings::AntiAliasing::Fxaa, QStringLiteral("FXAA")},
    {Settings::AntiAliasing::Smaa, QStringLiteral("SMAA")},
};

static const std::map<Settings::ScalingFilter, QString> scaling_filter_texts_map = {
    {Settings::ScalingFilter::NearestNeighbor, QStringLiteral("Ближайший сосед")},
    {Settings::ScalingFilter::Bilinear, QStringLiteral("Билинейный")},
    {Settings::ScalingFilter::Bicubic, QStringLiteral("Бикубический")},
    {Settings::ScalingFilter::Gaussian, QStringLiteral("Гаусс")},
    {Settings::ScalingFilter::Lanczos, QStringLiteral("Ланцош")},
    {Settings::ScalingFilter::ScaleForce, QStringLiteral("ScaleForce")},
    {Settings::ScalingFilter::Fsr, QStringLiteral("FSR")},
    {Settings::ScalingFilter::Area, QStringLiteral("Area")},
    {Settings::ScalingFilter::Mmpx, QStringLiteral("MMPX")},
    {Settings::ScalingFilter::ZeroTangent, QStringLiteral("Zero-Tangent")},
    {Settings::ScalingFilter::BSpline, QStringLiteral("B-Spline")},
    {Settings::ScalingFilter::Mitchell, QStringLiteral("Mitchell")},
    {Settings::ScalingFilter::Spline1, QStringLiteral("Spline-1")},
    {Settings::ScalingFilter::Sgsr, QStringLiteral("SGSR")},
    {Settings::ScalingFilter::SgsrEdge, QStringLiteral("SGSR EdgeDir")},
};

static const std::map<Settings::ConsoleMode, QString> use_docked_mode_texts_map = {
    {Settings::ConsoleMode::Docked, QStringLiteral("В док-станции")},
    {Settings::ConsoleMode::Handheld, QStringLiteral("Портативный")},
};

static const std::map<Settings::GpuAccuracy, QString> gpu_accuracy_texts_map = {
    {Settings::GpuAccuracy::Low, QStringLiteral("Быстрый")},
    {Settings::GpuAccuracy::High, QStringLiteral("Высокая точность")},
};

static const std::map<Settings::RendererBackend, QString> renderer_backend_texts_map = {
    {Settings::RendererBackend::Vulkan, QStringLiteral("Vulkan")},
    {Settings::RendererBackend::OpenGL_GLSL, QStringLiteral("OpenGL GLSL")},
    {Settings::RendererBackend::OpenGL_SPIRV, QStringLiteral("OpenGL SPIRV")},
    {Settings::RendererBackend::OpenGL_GLASM, QStringLiteral("OpenGL GLASM")},
    {Settings::RendererBackend::Null, QStringLiteral("Отключено")},
};

} // namespace ConfigurationShared
