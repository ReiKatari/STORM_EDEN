// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QGraphicsOpacityEffect>
#include <QIODevice>

#include <QImage>
#include <QImageReader>
#include <QLabel>
#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QStyleOption>
#include <QTime>
#include <QTimer>
#include <ankerl/unordered_dense.h>
#include "common/settings.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/loader/loader.h"
#include "qt_common/config/uisettings.h"
#include "ui_loading_screen.h"
#include "video_core/rasterizer_interface.h"
#include "eden/loading_screen.h"

LoadingScreen::LoadingScreen(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::LoadingScreen>()),
      previous_stage(VideoCore::LoadCallbackStage::Complete) {
    ui->setupUi(this);
    setMinimumSize(Layout::MinimumSize::Width, Layout::MinimumSize::Height);

    // Apply premium dark background gradient
    setStyleSheet(QStringLiteral("background: qlineargradient(spread:pad, x1:0, y1:0, x2:0, y2:1, stop:0 #0a0b10, stop:1 #12131a);"));

    // Programmatically initialize and style the game title label
    game_title = new QLabel(ui->fade_parent);
    game_title->setObjectName(QStringLiteral("game_title"));
    game_title->setStyleSheet(QStringLiteral(
        "color: #ffffff;"
        "font: 700 32px \"Segoe UI\", \"Outfit\", \"Inter\", sans-serif;"
        "background: transparent;"
    ));
    game_title->setAlignment(Qt::AlignCenter);

    // Style the logo icon to be transparent with no borders
    ui->logo->setStyleSheet(QStringLiteral("background: transparent; border: none;"));
    ui->logo->setScaledContents(true);
    ui->logo->setFixedSize(128, 128);
    ui->logo->setMargin(0);
    ui->logo->setVisible(false);



    // Reconstruct the layout programmatically for a perfectly centered, premium stack
    ui->verticalLayout->removeWidget(ui->stage);
    ui->verticalLayout->removeWidget(ui->progress_bar);
    ui->verticalLayout->removeWidget(ui->value);
    ui->verticalLayout_2->removeWidget(ui->logo);


    ui->verticalLayout->setSpacing(20);
    ui->verticalLayout->setContentsMargins(40, 40, 40, 40);

    // Build the centered stack with larger bottom stretch to elevate title and icon higher
    ui->verticalLayout->addStretch(2); // Top stretch
    ui->verticalLayout->addWidget(game_title, 0, Qt::AlignCenter);
    ui->verticalLayout->addWidget(ui->logo, 0, Qt::AlignCenter);
    ui->verticalLayout->addWidget(ui->stage, 0, Qt::AlignCenter);
    ui->verticalLayout->addWidget(ui->progress_bar, 0, Qt::AlignCenter);
    ui->verticalLayout->addWidget(ui->value, 0, Qt::AlignCenter);
    ui->verticalLayout->addStretch(3); // Bottom stretch (larger to push content up)



    // Override stage and value labels styling for a state-of-the-art look
    ui->stage->setStyleSheet(QStringLiteral(
        "background-color: transparent;"
        "color: #38bdf8;"
        "font: 600 18px \"Segoe UI\", \"Outfit\", \"Inter\", sans-serif;"
        "letter-spacing: 0.5px;"
    ));
    ui->value->setStyleSheet(QStringLiteral(
        "background-color: transparent;"
        "color: #94a3b8;"
        "font: 500 13px \"Segoe UI\", \"Outfit\", \"Inter\", sans-serif;"
        "margin-top: 5px;"
    ));

    // Sleek progress bar style overrides with high-tech glowing gradients
    ui->progress_bar->setMinimumSize(QSize(600, 20));
    ui->progress_bar->setMaximumSize(QSize(600, 20));
    ui->progress_bar->setStyleSheet(QStringLiteral(
        "QProgressBar {"
        "  background-color: rgba(15, 23, 42, 0.6);"
        "  border: 1px solid rgba(56, 189, 248, 0.2);"
        "  border-radius: 10px;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:0, stop:0 #06b6d4, stop:0.5 #3b82f6, stop:1 #6366f1);"
        "  border-radius: 9px;"
        "}"
    ));

    // Create a fade out effect to hide this loading screen widget.
    opacity_effect = new QGraphicsOpacityEffect(this);
    opacity_effect->setOpacity(1);
    ui->fade_parent->setGraphicsEffect(opacity_effect);
    fadeout_animation = std::make_unique<QPropertyAnimation>(opacity_effect, "opacity");
    fadeout_animation->setDuration(500);
    fadeout_animation->setStartValue(1);
    fadeout_animation->setEndValue(0);
    fadeout_animation->setEasingCurve(QEasingCurve::OutQuad);

    // After the fade completes, hide the widget and reset the opacity
    connect(fadeout_animation.get(), &QPropertyAnimation::finished, [this] {
        hide();
        opacity_effect->setOpacity(1);
        emit Hidden();
    });
    connect(this, &LoadingScreen::LoadProgress, this, &LoadingScreen::OnLoadProgress,
            Qt::QueuedConnection);
    qRegisterMetaType<VideoCore::LoadCallbackStage>();
}

LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::Prepare(Loader::AppLoader& loader) {
    // Clear and reset all UI elements immediately to prevent design-time placeholders from flashing
    ui->logo->setPixmap(QPixmap());
    game_title->setText(QStringLiteral(""));
    ui->stage->setText(QStringLiteral(""));
    ui->value->setText(QStringLiteral(""));
    ui->progress_bar->setValue(0);
    ui->progress_bar->setMinimum(0);
    ui->progress_bar->setMaximum(100);

    std::vector<u8> buffer;
    if (loader.ReadIcon(buffer) == Loader::ResultStatus::Success) {
        QPixmap map;
        map.loadFromData(buffer.data(), static_cast<uint>(buffer.size()));
        ui->logo->setPixmap(map.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->logo->setVisible(true);
    } else {
        ui->logo->setVisible(false);
    }

    // Read the game title dynamically
    std::string name;
    if (loader.ReadTitle(name) == Loader::ResultStatus::Success) {
        game_title->setText(QString::fromStdString(name));
    } else {
        game_title->setText(tr("Unknown Game"));
    }

    slow_shader_compile_start = false;
    compile_start_time = {};
    initial_value = 0;

    OnLoadProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
}

void LoadingScreen::OnLoadComplete() {
    fadeout_animation->start(QPropertyAnimation::KeepWhenStopped);
}

void LoadingScreen::OnLoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value,
                                   std::size_t total) {
    if (stage == VideoCore::LoadCallbackStage::Complete) {
        // Wait for FirstFrameDisplayed instead of hiding immediately
        ui->stage->setText(tr("Starting Game..."));
        ui->progress_bar->hide();
        return;
    }

    using namespace std::chrono;
    const auto now = steady_clock::now();

    // Reset compile stats when transitioning stages
    if (stage != previous_stage) {
        if (stage == VideoCore::LoadCallbackStage::Build) {
            compile_start_time = now;
            initial_value = value;
        } else {
            compile_start_time = {};
            initial_value = 0;
        }
        previous_stage = stage;
        slow_shader_compile_start = false;
    }

    if (total != previous_total) {
        if (total > 0) {
            ui->progress_bar->setMinimum(0);
            ui->progress_bar->setMaximum(static_cast<int>(total));
        } else {
            ui->progress_bar->setMinimum(0);
            ui->progress_bar->setMaximum(100);
        }
        previous_total = total;
    }

    if (stage == VideoCore::LoadCallbackStage::Complete) {
        ui->progress_bar->setRange(0, 0);
    }

    // Determine active interface language
    std::string lang = UISettings::values.language.GetValue();
    if (lang.empty()) {
        lang = QLocale().name().left(2).toStdString();
    }
    std::transform(lang.begin(), lang.end(), lang.begin(), ::tolower);
    const bool is_russian = (lang.rfind("ru", 0) == 0);

    // Get active Graphics API name
    const auto api = Settings::values.renderer_backend.GetValue();
    QString api_name;
    if (api == Settings::RendererBackend::Vulkan) {
        api_name = QStringLiteral("Vulkan");
    } else if (api == Settings::RendererBackend::OpenGL_GLSL ||
               api == Settings::RendererBackend::OpenGL_GLASM ||
               api == Settings::RendererBackend::OpenGL_SPIRV) {
        api_name = QStringLiteral("OpenGL");
    } else {
        api_name = QStringLiteral("Graphics");
    }

    // Dynamic stats: Compile speed and Completion percentage
    double speed = 0.0;
    if (stage == VideoCore::LoadCallbackStage::Build && compile_start_time != steady_clock::time_point{}) {
        auto elapsed = duration_cast<milliseconds>(now - compile_start_time).count() / 1000.0;
        if (elapsed > 0.1 && value > initial_value) {
            speed = (value - initial_value) / elapsed;
        }
    }
    int pct = (total > 0) ? static_cast<int>((value * 100) / total) : 0;

    // Calculate standard ETA
    QString estimate;
    QString formatted_time;
    if (now - previous_time > milliseconds{50} || slow_shader_compile_start) {
        if (!slow_shader_compile_start) {
            slow_shader_start = now;
            slow_shader_compile_start = true;
            slow_shader_first_value = value;
        }
        const auto diff = duration_cast<milliseconds>(now - slow_shader_start);
        if (diff > seconds{1}) {
            const auto eta_mseconds =
                static_cast<long>(static_cast<double>(total - slow_shader_first_value) /
                                  (value - slow_shader_first_value) * diff.count());
            formatted_time = QTime(0, 0, 0, 0)
                                 .addMSecs(std::max<long>(eta_mseconds - diff.count() + 1000, 1000))
                                 .toString(QStringLiteral("mm:ss"));
            estimate = tr("ETA %1").arg(formatted_time);
        }
    }

    // Set stage and status texts localized appropriately
    if (stage == VideoCore::LoadCallbackStage::Prepare) {
        ui->stage->setText(is_russian ? QStringLiteral("Подготовка графического конвейера...")
                                       : QStringLiteral("Preparing graphics pipeline..."));
        ui->progress_bar->hide();
    } else if (stage == VideoCore::LoadCallbackStage::Build) {
        ui->stage->setText(is_russian 
            ? QStringLiteral("Компиляция %1 шейдеров: %2 из %3 (%4%)").arg(api_name).arg(value).arg(total).arg(pct)
            : QStringLiteral("Compiling %1 shaders: %2 / %3 (%4%)").arg(api_name).arg(value).arg(total).arg(pct));
        ui->progress_bar->show();
    } else {
        ui->stage->setText(is_russian ? QStringLiteral("Запуск игры...") : QStringLiteral("Launching game..."));
        ui->progress_bar->show();
    }

    // Combine Speed, ETA, and API info into a clean footer label
    QStringList footer_items;
    if (stage == VideoCore::LoadCallbackStage::Build) {
        if (speed > 0.1) {
            footer_items << (is_russian ? QStringLiteral("Скорость: %1 шейд/сек").arg(static_cast<int>(speed))
                                        : QStringLiteral("Speed: %1 shaders/sec").arg(static_cast<int>(speed)));
        }
        if (!formatted_time.isEmpty()) {
            footer_items << (is_russian ? QStringLiteral("Оставшееся время: %1").arg(formatted_time) : estimate);
        }
    }
    footer_items << QStringLiteral("API: %1").arg(api_name);
    ui->value->setText(footer_items.join(QStringLiteral("  •  ")));

    if (total > 0) {
        ui->progress_bar->setValue(static_cast<int>(value));
    } else {
        ui->progress_bar->setValue(0);
    }
    previous_time = now;
}

void LoadingScreen::paintEvent(QPaintEvent* event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);
}

void LoadingScreen::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
}

void LoadingScreen::Clear() {
}
