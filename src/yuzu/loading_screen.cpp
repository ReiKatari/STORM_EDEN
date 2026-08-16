// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QBuffer>
#include <QByteArray>
#include <QGraphicsOpacityEffect>
#include <QIODevice>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QStyleOption>
#include <QTime>
#include <QTimer>
#include <ankerl/unordered_dense.h>
#include "common/settings.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/loader/loader.h"
#include "ui_loading_screen.h"
#include "video_core/rasterizer_interface.h"
#include "yuzu/loading_screen.h"

#if !YUZU_QT_MOVIE_MISSING
#include <QMovie>
#endif

LoadingScreen::LoadingScreen(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::LoadingScreen>()),
      previous_stage(VideoCore::LoadCallbackStage::Complete) {
    ui->setupUi(this);
    setMinimumSize(Layout::MinimumSize::Width, Layout::MinimumSize::Height);

    opacity_effect = new QGraphicsOpacityEffect(this);
    opacity_effect->setOpacity(1);
    ui->fade_parent->setGraphicsEffect(opacity_effect);
    fadeout_animation = std::make_unique<QPropertyAnimation>(opacity_effect, "opacity");
    fadeout_animation->setDuration(450);
    fadeout_animation->setStartValue(1);
    fadeout_animation->setEndValue(0);
    fadeout_animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(fadeout_animation.get(), &QPropertyAnimation::finished, [this] {
        hide();
        opacity_effect->setOpacity(1);
        emit Hidden();
    });
    connect(this, &LoadingScreen::LoadProgress, this, &LoadingScreen::OnLoadProgress,
            Qt::QueuedConnection);
    qRegisterMetaType<VideoCore::LoadCallbackStage>();

    stage_translations = {
        {VideoCore::LoadCallbackStage::Prepare, tr("ПОДГОТОВКА СИСТЕМЫ...")},
        {VideoCore::LoadCallbackStage::Build, tr("КОМПИЛЯЦИЯ ШЕЙДЕРОВ: %1 / %2")},
        {VideoCore::LoadCallbackStage::Complete, tr("ЗАПУСК ИГРОВОГО ПРОЦЕССА...")},
    };
}

LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::SetGameInfo(const QString& name, const QString& version, const QString& dev,
                                u64 title_id, const QPixmap& icon, const QString& format) {
    if (!icon.isNull()) {
        ui->game_icon->setPixmap(icon.scaled(180, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    ui->game_title->setWordWrap(true);
    ui->game_title->setAlignment(Qt::AlignCenter);
    const QString display_name = name.isEmpty() ? tr("Запуск игры...") : name;
    if (display_name.length() > 40) {
        ui->game_title->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 15pt; font-weight: 800; background: transparent; margin-top: 4px; margin-bottom: 4px;"));
    } else {
        ui->game_title->setStyleSheet(QStringLiteral("color: #ffffff; font-size: 18pt; font-weight: 800; background: transparent; margin-top: 4px; margin-bottom: 4px;"));
    }
    ui->game_title->setText(display_name);

    QStringList meta_lines;
    if (!version.isEmpty()) {
        meta_lines << QStringLiteral("<b>Версия:</b> %1").arg(version);
    }
    if (!dev.isEmpty()) {
        meta_lines << QStringLiteral("<b>Разработчик:</b> %1").arg(dev);
    }
    if (title_id != 0) {
        meta_lines << QStringLiteral("<b>ID приложения:</b> %1").arg(
            QString::number(title_id, 16).toUpper().rightJustified(16, QLatin1Char('0')));
    }
    if (!format.isEmpty()) {
        meta_lines << QStringLiteral("<b>Формат:</b> %1").arg(format.toUpper());
    }

    meta_lines << QStringLiteral("<b>Архитектура:</b> 64-bit ARM");

    const QString backend_str = (Settings::values.renderer_backend.GetValue() == Settings::RendererBackend::Vulkan)
                                    ? QStringLiteral("Vulkan")
                                    : QStringLiteral("OpenGL");
    meta_lines << QStringLiteral("<b>Рендерер:</b> %1").arg(backend_str);

    const QString cpu_backend_str = (Settings::values.cpu_accuracy.GetValue() == Settings::CpuAccuracy::Auto ||
                                     Settings::values.cpu_accuracy.GetValue() == Settings::CpuAccuracy::Accurate)
                                        ? QStringLiteral("Dynarmic JIT")
                                        : QStringLiteral("Dynarmic JIT (Unsafe)");
    meta_lines << QStringLiteral("<b>Бэкенд ЦП:</b> %1").arg(cpu_backend_str);

    ui->game_meta->setText(meta_lines.join(QStringLiteral("<br/>")));
}

void LoadingScreen::Prepare(Loader::AppLoader& loader) {
    std::vector<u8> buffer;
    if (loader.ReadBanner(buffer) == Loader::ResultStatus::Success) {
#ifdef YUZU_QT_MOVIE_MISSING
        QPixmap map;
        map.loadFromData(buffer.data(), static_cast<uint>(buffer.size()));
        ui->banner->setPixmap(map);
#else
        backing_mem = std::make_unique<QByteArray>(reinterpret_cast<char*>(buffer.data()),
                                                   static_cast<int>(buffer.size()));
        backing_buf = std::make_unique<QBuffer>(backing_mem.get());
        backing_buf->open(QIODevice::ReadOnly);
        animation = std::make_unique<QMovie>(backing_buf.get(), QByteArray());
        animation->start();
        ui->banner->setMovie(animation.get());
#endif
        buffer.clear();
    }
    if (loader.ReadLogo(buffer) == Loader::ResultStatus::Success) {
        QPixmap map;
        map.loadFromData(buffer.data(), static_cast<uint>(buffer.size()));
        ui->logo->setPixmap(map.scaled(120, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        buffer.clear();
    }

    if (ui->game_icon->pixmap().isNull()) {
        std::vector<u8> icon_bytes;
        if (loader.ReadIcon(icon_bytes) == Loader::ResultStatus::Success) {
            QPixmap map;
            map.loadFromData(icon_bytes.data(), static_cast<uint>(icon_bytes.size()));
            ui->game_icon->setPixmap(map.scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }

    slow_shader_compile_start = false;
    OnLoadProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
}

void LoadingScreen::OnLoadComplete() {
    if (fadeout_animation->state() != QAbstractAnimation::Running && isVisible()) {
        fadeout_animation->start(QPropertyAnimation::KeepWhenStopped);
    }
}

void LoadingScreen::ShowShutdownState() {
    fadeout_animation->stop();
    opacity_effect->setOpacity(1.0);
    ui->stage->setText(tr("ЗАВЕРШЕНИЕ ЭМУЛЯЦИИ И СОХРАНЕНИЕ ДАННЫХ..."));
    ui->value->setText(QString{});
    ui->progress_bar->setRange(0, 0);
    show();
    raise();
}

void LoadingScreen::OnLoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value,
                                   std::size_t total) {
    using namespace std::chrono;
    const auto now = steady_clock::now();

    if (stage != previous_stage) {
        previous_stage = stage;
        slow_shader_compile_start = false;
    }

    if (stage == VideoCore::LoadCallbackStage::Prepare) {
        ui->progress_bar->setRange(0, 0);
        ui->stage->setText(stage_translations[stage]);
        ui->value->setText(QString{});
    } else if (stage == VideoCore::LoadCallbackStage::Build) {
        if (total != previous_total) {
            ui->progress_bar->setMaximum(static_cast<int>(total));
            previous_total = total;
        }
        ui->progress_bar->setValue(static_cast<int>(value));
        ui->stage->setText(stage_translations[stage].arg(value).arg(total));

        QString estimate;
        if (now - previous_time > milliseconds{50} || slow_shader_compile_start) {
            if (!slow_shader_compile_start) {
                slow_shader_start = steady_clock::now();
                slow_shader_compile_start = true;
                slow_shader_first_value = value;
            }
            const auto diff = duration_cast<milliseconds>(now - slow_shader_start);
            if (diff > seconds{1} && (value > slow_shader_first_value)) {
                const auto eta_mseconds =
                    static_cast<long>(static_cast<double>(total - slow_shader_first_value) /
                                      (value - slow_shader_first_value) * diff.count());
                estimate =
                    tr("Осталось: %1")
                        .arg(QTime(0, 0, 0, 0)
                                 .addMSecs(std::max<long>(eta_mseconds - diff.count() + 1000, 1000))
                                 .toString(QStringLiteral("mm:ss")));
            }
        }
        ui->value->setText(estimate);
    } else if (stage == VideoCore::LoadCallbackStage::Complete) {
        ui->progress_bar->setRange(0, 100);
        ui->progress_bar->setValue(100);
        ui->stage->setText(stage_translations[stage]);
        ui->value->setText(QString{});
        QTimer::singleShot(2500, this, [this] {
            if (isVisible()) {
                OnLoadComplete();
            }
        });
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

void LoadingScreen::Clear() {
#ifndef YUZU_QT_MOVIE_MISSING
    animation.reset();
    backing_buf.reset();
    backing_mem.reset();
#endif
    ui->game_icon->clear();
    ui->game_title->clear();
    ui->game_meta->clear();
}
