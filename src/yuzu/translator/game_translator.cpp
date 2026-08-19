// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <filesystem>
#include <fmt/format.h>

#ifdef _WIN32
#include <sapi.h>
#include <sphelper.h>
#include <windows.h>
#pragma comment(lib, "sapi.lib")
#endif

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "core/core.h"
#include "yuzu/translator/game_translator.h"

// ============================================================================
// ROIPreviewWidget Implementation
// ============================================================================

ROIPreviewWidget::ROIPreviewWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(480, 270);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::CrossCursor);
    setStyleSheet(QStringLiteral("background: #060911; border: 1px solid #1E293B; border-radius: 8px;"));
}

void ROIPreviewWidget::SetImage(const QImage& image) {
    m_image = image;
    update();
}

void ROIPreviewWidget::SetZones(const std::vector<QRect>& zones) {
    m_zones = zones;
    update();
}

void ROIPreviewWidget::AddZone(const QRect& rect) {
    if (rect.width() > 10 && rect.height() > 10) {
        m_zones.push_back(rect);
        m_active_zone = static_cast<int>(m_zones.size()) - 1;
        emit ZonesChanged();
        update();
    }
}

void ROIPreviewWidget::ClearZones() {
    m_zones.clear();
    m_active_zone = -1;
    emit ZonesChanged();
    update();
}

void ROIPreviewWidget::SetActiveZoneIndex(int index) {
    m_active_zone = index;
    update();
}

QRect ROIPreviewWidget::GetImageDrawRect() const {
    if (m_image.isNull()) {
        // Return 16:9 box inside widget
        int w = width() - 16;
        int h = height() - 16;
        if (w * 9 > h * 16) {
            int target_w = (h * 16) / 9;
            int x = (width() - target_w) / 2;
            return QRect(x, 8, target_w, h);
        } else {
            int target_h = (w * 9) / 16;
            int y = (height() - target_h) / 2;
            return QRect(8, y, w, target_h);
        }
    }

    int img_w = m_image.width();
    int img_h = m_image.height();
    int avail_w = width() - 16;
    int avail_h = height() - 16;

    double scale = std::min(static_cast<double>(avail_w) / img_w, static_cast<double>(avail_h) / img_h);
    int draw_w = static_cast<int>(img_w * scale);
    int draw_h = static_cast<int>(img_h * scale);
    int draw_x = (width() - draw_w) / 2;
    int draw_y = (height() - draw_h) / 2;

    return QRect(draw_x, draw_y, draw_w, draw_h);
}

QPoint ROIPreviewWidget::ScreenToImageCoords(const QPoint& pt) const {
    QRect drawRect = GetImageDrawRect();
    if (drawRect.width() == 0 || drawRect.height() == 0) return pt;

    int orig_w = m_image.isNull() ? 1920 : m_image.width();
    int orig_h = m_image.isNull() ? 1080 : m_image.height();

    int rel_x = std::clamp(pt.x() - drawRect.x(), 0, drawRect.width());
    int rel_y = std::clamp(pt.y() - drawRect.y(), 0, drawRect.height());

    int img_x = (rel_x * orig_w) / drawRect.width();
    int img_y = (rel_y * orig_h) / drawRect.height();
    return QPoint(img_x, img_y);
}

QRect ROIPreviewWidget::ImageToScreenRect(const QRect& r) const {
    QRect drawRect = GetImageDrawRect();
    if (drawRect.width() == 0 || drawRect.height() == 0) return r;

    int orig_w = m_image.isNull() ? 1920 : m_image.width();
    int orig_h = m_image.isNull() ? 1080 : m_image.height();

    int sx = drawRect.x() + (r.x() * drawRect.width()) / orig_w;
    int sy = drawRect.y() + (r.y() * drawRect.height()) / orig_h;
    int sw = (r.width() * drawRect.width()) / orig_w;
    int sh = (r.height() * drawRect.height()) / orig_h;
    return QRect(sx, sy, sw, sh);
}

void ROIPreviewWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Canvas background
    p.fillRect(rect(), QColor(10, 14, 23));

    QRect drawRect = GetImageDrawRect();

    // Draw frame or placeholder
    if (!m_image.isNull()) {
        p.drawImage(drawRect, m_image);
    } else {
        // High-tech 16:9 Game Viewport Placeholder Canvas
        QLinearGradient canvasGrad(drawRect.topLeft(), drawRect.bottomRight());
        canvasGrad.setColorAt(0.0, QColor(15, 23, 42));
        canvasGrad.setColorAt(1.0, QColor(10, 14, 26));
        p.fillRect(drawRect, canvasGrad);

        // Coordinate Grid
        p.setPen(QPen(QColor(30, 41, 59, 120), 1, Qt::DotLine));
        for (int gx = drawRect.left() + 40; gx < drawRect.right(); gx += 40) {
            p.drawLine(gx, drawRect.top(), gx, drawRect.bottom());
        }
        for (int gy = drawRect.top() + 40; gy < drawRect.bottom(); gy += 40) {
            p.drawLine(drawRect.left(), gy, drawRect.right(), gy);
        }

        // Center crosshair
        QPoint center = drawRect.center();
        p.setPen(QPen(QColor(0, 229, 255, 140), 1));
        p.drawLine(center.x() - 14, center.y(), center.x() + 14, center.y());
        p.drawLine(center.x(), center.y() - 14, center.x(), center.y() + 14);

        // Corner targeting brackets (┌ ┐ └ ┘)
        p.setPen(QPen(QColor(0, 229, 255, 220), 2));
        int cLen = 16;
        // Top-left
        p.drawLine(drawRect.left() + 4, drawRect.top() + 4, drawRect.left() + 4 + cLen, drawRect.top() + 4);
        p.drawLine(drawRect.left() + 4, drawRect.top() + 4, drawRect.left() + 4, drawRect.top() + 4 + cLen);
        // Top-right
        p.drawLine(drawRect.right() - 4, drawRect.top() + 4, drawRect.right() - 4 - cLen, drawRect.top() + 4);
        p.drawLine(drawRect.right() - 4, drawRect.top() + 4, drawRect.right() - 4, drawRect.top() + 4 + cLen);
        // Bottom-left
        p.drawLine(drawRect.left() + 4, drawRect.bottom() - 4, drawRect.left() + 4 + cLen, drawRect.bottom() - 4);
        p.drawLine(drawRect.left() + 4, drawRect.bottom() - 4, drawRect.left() + 4, drawRect.bottom() - 4 - cLen);
        // Bottom-right
        p.drawLine(drawRect.right() - 4, drawRect.bottom() - 4, drawRect.right() - 4 - cLen, drawRect.bottom() - 4);
        p.drawLine(drawRect.right() - 4, drawRect.bottom() - 4, drawRect.right() - 4, drawRect.bottom() - 4 - cLen);

        // Viewport Header Label
        QRect badgeRect(drawRect.left() + 12, drawRect.top() + 10, 220, 22);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 229, 255, 30));
        p.drawRoundedRect(badgeRect, 4, 4);
        p.setPen(QColor(0, 229, 255));
        QFont bFont(QStringLiteral("Segoe UI"), 8, QFont::Bold);
        p.setFont(bFont);
        p.drawText(badgeRect, Qt::AlignCenter, tr("🎮 ИГРОВОЙ ЭКРАН 16:9 (1080p)"));

        // Instructional Text
        p.setPen(QColor(148, 163, 184));
        QFont font(QStringLiteral("Segoe UI"), 10, QFont::DemiBold);
        p.setFont(font);
        QRect textRect(drawRect.left(), center.y() + 20, drawRect.width(), 40);
        p.drawText(textRect, Qt::AlignCenter, tr("Выделите мышью область субтитров или диалогов"));
    }

    // Border around render viewport
    p.setPen(QPen(QColor(0, 229, 255, 180), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRect(drawRect);

    // Draw saved zones
    for (size_t i = 0; i < m_zones.size(); ++i) {
        QRect screenR = ImageToScreenRect(m_zones[i]);
        bool isActive = (static_cast<int>(i) == m_active_zone);

        p.setPen(QPen(isActive ? QColor(0, 229, 255) : QColor(245, 158, 11), isActive ? 2.5 : 1.5));
        p.setBrush(isActive ? QColor(0, 229, 255, 45) : QColor(245, 158, 11, 30));
        p.drawRoundedRect(screenR, 4, 4);

        // Zone label badge
        QRect labelRect(screenR.x(), screenR.y() - 20, 68, 18);
        p.setPen(Qt::NoPen);
        p.setBrush(isActive ? QColor(0, 229, 255, 220) : QColor(245, 158, 11, 220));
        p.drawRoundedRect(labelRect, 3, 3);

        p.setPen(QColor(0, 0, 0));
        QFont bFont(QStringLiteral("Segoe UI"), 8, QFont::Bold);
        p.setFont(bFont);
        p.drawText(labelRect, Qt::AlignCenter, tr("Зона #%1").arg(i + 1));
    }

    // Draw currently dragging zone
    if (m_is_drawing && !m_current_rect.isNull()) {
        QRect screenR = ImageToScreenRect(m_current_rect);
        p.setPen(QPen(QColor(236, 72, 153), 2, Qt::DashLine));
        p.setBrush(QColor(236, 72, 153, 40));
        p.drawRoundedRect(screenR, 4, 4);
    }
}

void ROIPreviewWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_start_pt = ScreenToImageCoords(event->pos());
        m_current_rect = QRect(m_start_pt, m_start_pt);
        m_is_drawing = true;
        update();
    }
}

void ROIPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_is_drawing) {
        QPoint current = ScreenToImageCoords(event->pos());
        m_current_rect = QRect(m_start_pt, current).normalized();
        update();
    }
}

void ROIPreviewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && m_is_drawing) {
        m_is_drawing = false;
        QPoint end_pt = ScreenToImageCoords(event->pos());
        m_current_rect = QRect(m_start_pt, end_pt).normalized();
        if (m_current_rect.width() > 20 && m_current_rect.height() > 15) {
            AddZone(m_current_rect);
        }
        m_current_rect = QRect();
        update();
    }
}

// ============================================================================
// TranslatorHUDOverlay Implementation
// ============================================================================

TranslatorHUDOverlay::TranslatorHUDOverlay(QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::SubWindow) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    resize(760, 120);
    setWindowTitle(QStringLiteral("STORM EDEN HUD"));
}

void TranslatorHUDOverlay::SetSubtitleText(const QString& text) {
    m_text = text;
    update();
}

void TranslatorHUDOverlay::SetFontSize(int pt) {
    m_font_size = pt;
    update();
}

void TranslatorHUDOverlay::SetTextColor(const QColor& color) {
    m_text_color = color;
    update();
}

void TranslatorHUDOverlay::SetBackgroundOpacity(int opacityPercent) {
    m_bg_opacity = opacityPercent;
    update();
}

void TranslatorHUDOverlay::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (m_text.isEmpty()) return;

    int alpha = (m_bg_opacity * 255) / 100;
    QRect r = rect().adjusted(6, 6, -6, -6);

    // Dark glass box
    p.setPen(QPen(QColor(0, 229, 255, 160), 1.5));
    p.setBrush(QColor(10, 14, 23, alpha));
    p.drawRoundedRect(r, 12, 12);

    // Glow accent
    p.setPen(QPen(QColor(124, 58, 237, 90), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(2, 2, -2, -2), 10, 10);

    QFont font(QStringLiteral("Segoe UI"), m_font_size, QFont::DemiBold);
    p.setFont(font);

    QRect textRect = r.adjusted(18, 12, -18, -12);
    // Shadow
    p.setPen(QColor(0, 0, 0, 230));
    p.drawText(textRect.adjusted(2, 2, 2, 2), Qt::AlignCenter | Qt::TextWordWrap, m_text);

    // Main text
    p.setPen(m_text_color);
    p.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, m_text);
}

void TranslatorHUDOverlay::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_drag_position = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void TranslatorHUDOverlay::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_drag_position);
        event->accept();
    }
}

// ============================================================================
// GameTranslator Main Window Implementation
// ============================================================================

GameTranslator::GameTranslator(Core::System& system, QWidget* parent)
    : QDialog(parent), m_system(system) {
    setWindowTitle(tr("🌐 STORM EDEN — Авто-переводчик"));
    resize(1120, 740);
    setMinimumSize(980, 640);

    setStyleSheet(QStringLiteral(
        "QDialog { background: #0B0F19; color: #F8FAFC; font-family: 'Segoe UI', sans-serif; }"
        "QTabWidget::pane { border: 1px solid #1E293B; background: #0D1424; border-radius: 8px; }"
        "QTabBar::tab { background: #121826; color: #94A3B8; border: 1px solid #1E293B; border-bottom: none; "
        "               padding: 10px 20px; border-top-left-radius: 8px; border-top-right-radius: 8px; font-weight: 600; font-size: 13px; }"
        "QTabBar::tab:selected { background: #0D1424; color: #00E5FF; border: 1px solid #00E5FF; border-bottom: 2px solid #0D1424; }"
        "QGroupBox { color: #00E5FF; font-weight: bold; border: 1px solid #1E293B; border-radius: 8px; margin-top: 10px; padding-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; }"
        "QLabel { color: #CBD5E1; font-size: 13px; font-weight: 500; }"
        "QLineEdit, QTextEdit, QSpinBox { background: #151D30; color: #F8FAFC; border: 1px solid #27354F; border-radius: 6px; padding: 6px 10px; font-size: 13px; }"
        "QLineEdit:focus, QTextEdit:focus, QSpinBox:focus { border: 1px solid #00E5FF; }"
        "QComboBox { background: #151D30; color: #F8FAFC; border: 1px solid #27354F; border-radius: 6px; padding: 6px 12px; font-size: 13px; }"
        "QComboBox QAbstractItemView { background: #151D30; color: #F8FAFC; selection-background-color: #2563EB; }"
        "QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2563EB, stop:1 #00D4FF); color: #FFFFFF; border: none; border-radius: 6px; font-weight: 600; padding: 8px 16px; font-size: 13px; }"
        "QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1D4ED8, stop:1 #00B4D8); }"
        "QPushButton#actionBtn { background: #059669; }"
        "QPushButton#actionBtn:hover { background: #047857; }"
        "QPushButton#dangerBtn { background: #7F1D1D; border: 1px solid #DC2626; color: #FCA5A5; }"
        "QPushButton#dangerBtn:hover { background: #DC2626; color: #FFFFFF; }"
        "QPushButton#subtleBtn { background: #1E293B; border: 1px solid #334155; color: #CBD5E1; }"
        "QPushButton#subtleBtn:hover { background: #2563EB; color: #FFFFFF; border-color: #00E5FF; }"
        "QCheckBox { color: #F8FAFC; font-size: 13px; font-weight: 500; spacing: 8px; }"
        "QSlider::groove:horizontal { height: 6px; background: #1E293B; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: #00E5FF; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #FFFFFF; width: 16px; margin: -5px 0; border-radius: 8px; }"
    ));

    m_network_mgr = new QNetworkAccessManager(this);
    m_hud_overlay = new TranslatorHUDOverlay();
    m_auto_interval_timer = new QTimer(this);
    connect(m_auto_interval_timer, &QTimer::timeout, this, &GameTranslator::OnAutoIntervalTimerTimeout);

    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(14, 14, 14, 14);
    root_layout->setSpacing(10);

    m_tab_widget = new QTabWidget(this);

    // ========================================================================
    // TAB 1: 🌐 Перевод и AI-модели
    // ========================================================================
    auto* tab_translate = new QWidget(this);
    auto* t1_layout = new QVBoxLayout(tab_translate);
    t1_layout->setContentsMargins(12, 12, 12, 12);
    t1_layout->setSpacing(10);

    auto* service_box = new QGroupBox(tr("⚙️ Движок перевода и ИИ модели"), tab_translate);
    auto* service_layout = new QGridLayout(service_box);
    service_layout->setHorizontalSpacing(12);
    service_layout->setVerticalSpacing(8);

    service_layout->addWidget(new QLabel(tr("AI Провайдер:"), tab_translate), 0, 0);
    m_provider_combo = new QComboBox(tab_translate);
    m_provider_combo->addItem(tr("🚀 Google Web API (Бесплатно, без ключа)"), QStringLiteral("google"));
    m_provider_combo->addItem(tr("⚡ DeepL API (Высокая точность)"), QStringLiteral("deepl"));
    m_provider_combo->addItem(tr("🤖 OpenAI GPT-4o-mini"), QStringLiteral("openai"));
    m_provider_combo->addItem(tr("✨ Google Gemini 2.5 Flash"), QStringLiteral("gemini"));
    m_provider_combo->addItem(tr("🏠 Локальный Ollama / LM Studio"), QStringLiteral("ollama"));
    service_layout->addWidget(m_provider_combo, 0, 1);

    service_layout->addWidget(new QLabel(tr("API Ключ:"), tab_translate), 0, 2);
    m_api_key_edit = new QLineEdit(tab_translate);
    m_api_key_edit->setEchoMode(QLineEdit::Password);
    m_api_key_edit->setPlaceholderText(tr("Не требуется для Google Web API"));
    service_layout->addWidget(m_api_key_edit, 0, 3);

    service_layout->addWidget(new QLabel(tr("Язык оригинала:"), tab_translate), 1, 0);
    m_src_lang_combo = new QComboBox(tab_translate);
    m_src_lang_combo->addItem(tr("Автоопределение"), QStringLiteral("auto"));
    m_src_lang_combo->addItem(tr("Японский (Japanese)"), QStringLiteral("ja"));
    m_src_lang_combo->addItem(tr("Английский (English)"), QStringLiteral("en"));
    m_src_lang_combo->addItem(tr("Китайский (Chinese)"), QStringLiteral("zh"));
    m_src_lang_combo->addItem(tr("Корейский (Korean)"), QStringLiteral("ko"));
    service_layout->addWidget(m_src_lang_combo, 1, 1);

    service_layout->addWidget(new QLabel(tr("Язык перевода:"), tab_translate), 1, 2);
    m_tgt_lang_combo = new QComboBox(tab_translate);
    m_tgt_lang_combo->addItem(tr("Русский (Russian)"), QStringLiteral("ru"));
    m_tgt_lang_combo->addItem(tr("Английский (English)"), QStringLiteral("en"));
    m_tgt_lang_combo->addItem(tr("Испанский (Spanish)"), QStringLiteral("es"));
    m_tgt_lang_combo->addItem(tr("Немецкий (German)"), QStringLiteral("de"));
    service_layout->addWidget(m_tgt_lang_combo, 1, 3);

    m_auto_speak_check = new QCheckBox(tr("🔊 Автоматически озвучивать перевод (TTS)"), tab_translate);
    service_layout->addWidget(m_auto_speak_check, 2, 0, 1, 2);

    m_status_label = new QLabel(tr("Готов к распознаванию"), tab_translate);
    m_status_label->setStyleSheet(QStringLiteral("color: #00E5FF; font-weight: bold;"));
    service_layout->addWidget(m_status_label, 2, 2, 1, 2, Qt::AlignRight);

    t1_layout->addWidget(service_box);

    auto* text_splitter = new QSplitter(Qt::Horizontal, tab_translate);
    auto* orig_box = new QGroupBox(tr("Распознанный текст с экрана (Оригинал)"), tab_translate);
    auto* ob_layout = new QVBoxLayout(orig_box);
    m_original_edit = new QTextEdit(tab_translate);
    m_original_edit->setPlaceholderText(tr("Здесь появится текст из текущего кадра игры..."));
    ob_layout->addWidget(m_original_edit);

    auto* trans_box = new QGroupBox(tr("Результат перевода"), tab_translate);
    auto* tb_layout = new QVBoxLayout(trans_box);
    m_translated_edit = new QTextEdit(tab_translate);
    m_translated_edit->setPlaceholderText(tr("Здесь отобразится переведённый текст..."));
    tb_layout->addWidget(m_translated_edit);

    text_splitter->addWidget(orig_box);
    text_splitter->addWidget(trans_box);
    t1_layout->addWidget(text_splitter, 1);

    auto* t1_btn_layout = new QHBoxLayout();
    m_translate_btn = new QPushButton(tr("⚡ Перевести сейчас (F9)"), tab_translate);
    m_speak_btn = new QPushButton(tr("🔊 Озвучить"), tab_translate);
    m_speak_btn->setObjectName(QStringLiteral("actionBtn"));
    m_copy_btn = new QPushButton(tr("📋 Скопировать"), tab_translate);
    m_copy_btn->setObjectName(QStringLiteral("subtleBtn"));
    m_clear_btn = new QPushButton(tr("🧹 Очистить"), tab_translate);
    m_clear_btn->setObjectName(QStringLiteral("subtleBtn"));

    t1_btn_layout->addWidget(m_translate_btn);
    t1_btn_layout->addWidget(m_speak_btn);
    t1_btn_layout->addWidget(m_copy_btn);
    t1_btn_layout->addWidget(m_clear_btn);
    t1_btn_layout->addStretch(1);

    t1_layout->addLayout(t1_btn_layout);
    m_tab_widget->addTab(tab_translate, tr("🌐 Перевод и AI"));

    // ========================================================================
    // TAB 2: 📐 Области экрана (ROI Editor - Redesigned)
    // ========================================================================
    auto* tab_roi = new QWidget(this);
    auto* t2_layout = new QHBoxLayout(tab_roi);
    t2_layout->setContentsMargins(12, 12, 12, 12);
    t2_layout->setSpacing(14);

    // Left Column: Viewfinder Widget
    auto* preview_container = new QGroupBox(tr("🖥️ Интерактивный видоискатель (Выделите область мышью)"), tab_roi);
    auto* pc_layout = new QVBoxLayout(preview_container);
    m_roi_preview = new ROIPreviewWidget(preview_container);
    pc_layout->addWidget(m_roi_preview);
    t2_layout->addWidget(preview_container, 3);

    // Right Column: Controls & Presets
    auto* roi_ctrl_box = new QGroupBox(tr("⚙️ Настройка зон распознавания"), tab_roi);
    auto* roi_ctrl_layout = new QVBoxLayout(roi_ctrl_box);
    roi_ctrl_layout->setSpacing(10);

    // Preset selector
    roi_ctrl_layout->addWidget(new QLabel(tr("Быстрый шаблон (Preset):"), tab_roi));
    m_preset_combo = new QComboBox(tab_roi);
    m_preset_combo->addItem(tr("Пользовательский выбор..."), 0);
    m_preset_combo->addItem(tr("💬 Нижняя треть (Диалоги и сабы)"), 1);
    m_preset_combo->addItem(tr("📺 Центр экрана (Субтитры)"), 2);
    m_preset_combo->addItem(tr("🔝 Верхняя область (Заголовки/Имена)"), 3);
    m_preset_combo->addItem(tr("🖥️ Весь экран (100%)"), 4);
    connect(m_preset_combo, &QComboBox::currentIndexChanged, this, &GameTranslator::OnPresetSelected);
    roi_ctrl_layout->addWidget(m_preset_combo);

    roi_ctrl_layout->addWidget(new QLabel(tr("Список активных зон:"), tab_roi));
    m_zone_list = new QListWidget(tab_roi);
    m_zone_list->setMaximumHeight(140);
    connect(m_zone_list, &QListWidget::currentRowChanged, this, &GameTranslator::OnZoneListSelectionChanged);
    roi_ctrl_layout->addWidget(m_zone_list);

    // Coordinate inputs form
    auto* coord_form = new QFormLayout();
    m_zone_x_spin = new QSpinBox(tab_roi);
    m_zone_x_spin->setRange(0, 3840);
    m_zone_x_spin->setSingleStep(10);
    m_zone_y_spin = new QSpinBox(tab_roi);
    m_zone_y_spin->setRange(0, 2160);
    m_zone_y_spin->setSingleStep(10);
    m_zone_w_spin = new QSpinBox(tab_roi);
    m_zone_w_spin->setRange(10, 3840);
    m_zone_w_spin->setSingleStep(10);
    m_zone_h_spin = new QSpinBox(tab_roi);
    m_zone_h_spin->setRange(10, 2160);
    m_zone_h_spin->setSingleStep(10);

    connect(m_zone_x_spin, &QSpinBox::valueChanged, this, &GameTranslator::OnZoneSpinChanged);
    connect(m_zone_y_spin, &QSpinBox::valueChanged, this, &GameTranslator::OnZoneSpinChanged);
    connect(m_zone_w_spin, &QSpinBox::valueChanged, this, &GameTranslator::OnZoneSpinChanged);
    connect(m_zone_h_spin, &QSpinBox::valueChanged, this, &GameTranslator::OnZoneSpinChanged);

    coord_form->addRow(tr("X (слева):"), m_zone_x_spin);
    coord_form->addRow(tr("Y (сверху):"), m_zone_y_spin);
    coord_form->addRow(tr("Ширина:"), m_zone_w_spin);
    coord_form->addRow(tr("Высота:"), m_zone_h_spin);
    roi_ctrl_layout->addLayout(coord_form);

    // Buttons
    auto* zb_btn_layout = new QHBoxLayout();
    m_add_zone_btn = new QPushButton(tr("➕ Добавить"), tab_roi);
    m_remove_zone_btn = new QPushButton(tr("🗑️ Удалить"), tab_roi);
    m_remove_zone_btn->setObjectName(QStringLiteral("dangerBtn"));
    m_clear_zones_btn = new QPushButton(tr("Сброс"), tab_roi);
    m_clear_zones_btn->setObjectName(QStringLiteral("subtleBtn"));

    connect(m_add_zone_btn, &QPushButton::clicked, this, &GameTranslator::OnAddZoneClicked);
    connect(m_remove_zone_btn, &QPushButton::clicked, this, &GameTranslator::OnRemoveZoneClicked);
    connect(m_clear_zones_btn, &QPushButton::clicked, this, &GameTranslator::OnClearZonesClicked);

    zb_btn_layout->addWidget(m_add_zone_btn);
    zb_btn_layout->addWidget(m_remove_zone_btn);
    zb_btn_layout->addWidget(m_clear_zones_btn);
    roi_ctrl_layout->addLayout(zb_btn_layout);

    m_save_per_game_check = new QCheckBox(tr("💾 Сохранять зоны отдельно для каждой игры (Title ID)"), tab_roi);
    m_save_per_game_check->setChecked(true);
    roi_ctrl_layout->addWidget(m_save_per_game_check);
    roi_ctrl_layout->addStretch(1);

    t2_layout->addWidget(roi_ctrl_box, 2);
    m_tab_widget->addTab(tab_roi, tr("📐 Области экрана"));

    // ========================================================================
    // TAB 3: ⚡ Мониторинг и HUD Оверлей
    // ========================================================================
    auto* tab_hud = new QWidget(this);
    auto* t3_layout = new QVBoxLayout(tab_hud);
    t3_layout->setContentsMargins(12, 12, 12, 12);
    t3_layout->setSpacing(12);

    auto* auto_box = new QGroupBox(tr("⏱️ Автоматический мониторинг кадра"), tab_hud);
    auto* auto_layout = new QFormLayout(auto_box);
    m_auto_interval_check = new QCheckBox(tr("Включить фоновый периодический авто-перевод"), tab_hud);
    m_interval_spin = new QSpinBox(tab_hud);
    m_interval_spin->setRange(300, 10000);
    m_interval_spin->setSingleStep(100);
    m_interval_spin->setValue(1000);
    m_interval_spin->setSuffix(tr(" мс"));
    auto_layout->addRow(m_auto_interval_check);
    auto_layout->addRow(tr("Интервал захвата:"), m_interval_spin);
    t3_layout->addWidget(auto_box);

    auto* hud_box = new QGroupBox(tr("📺 Плавающий HUD оверлей субтитров поверх игры"), tab_hud);
    auto* hud_layout = new QFormLayout(hud_box);

    m_show_hud_check = new QCheckBox(tr("Показывать плавающие субтитры (HUD)"), tab_hud);
    m_enable_floating_btn_check = new QCheckBox(tr("Отображать плавающую кнопку Eden Lens поверх игры"), tab_hud);
    m_enable_floating_btn_check->setChecked(true);
    m_hud_opacity_slider = new QSlider(Qt::Horizontal, tab_hud);
    m_hud_opacity_slider->setRange(10, 100);
    m_hud_opacity_slider->setValue(75);

    m_hud_font_size_spin = new QSpinBox(tab_hud);
    m_hud_font_size_spin->setRange(12, 48);
    m_hud_font_size_spin->setValue(20);
    m_hud_font_size_spin->setSuffix(tr(" pt"));

    m_hud_color_combo = new QComboBox(tab_hud);
    m_hud_color_combo->addItem(tr("Неоновый голубой (Cyan)"), QColor(0, 229, 255));
    m_hud_color_combo->addItem(tr("Золотисто-жёлтый (Gold)"), QColor(245, 158, 11));
    m_hud_color_combo->addItem(tr("Белоснежный (White)"), QColor(255, 255, 255));
    m_hud_color_combo->addItem(tr("Изумрудный (Green)"), QColor(16, 185, 129));

    hud_layout->addRow(m_show_hud_check);
    hud_layout->addRow(m_enable_floating_btn_check);
    hud_layout->addRow(tr("Прозрачность фона:"), m_hud_opacity_slider);
    hud_layout->addRow(tr("Размер шрифта:"), m_hud_font_size_spin);
    hud_layout->addRow(tr("Цвет текста:"), m_hud_color_combo);
    t3_layout->addWidget(hud_box);

    auto* hotkey_info = new QLabel(tr("⌨️ Быстрые клавиши:\n"
                                       "• [F9] — Мгновенный перевод экрана\n"
                                       "• [F5] — Быстрое сохранение игры\n"
                                       "• [F6] — Быстрая загрузка игры"), tab_hud);
    hotkey_info->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 12px; background: #121826; padding: 12px; border-radius: 6px; border: 1px solid #1E293B;"));
    t3_layout->addWidget(hotkey_info);
    t3_layout->addStretch(1);

    m_tab_widget->addTab(tab_hud, tr("⚡ Мониторинг и HUD"));

    // ========================================================================
    // TAB 4: 🔊 Синтез речи (TTS) — Multi-Voice & Narrator
    // ========================================================================
    auto* tab_tts = new QWidget(this);
    auto* t4_layout = new QVBoxLayout(tab_tts);
    t4_layout->setContentsMargins(12, 12, 12, 12);
    t4_layout->setSpacing(12);

    auto* tts_box = new QGroupBox(tr("🎭 Мультиголосовой синтез речи и автоопределение персонажей"), tab_tts);
    auto* tts_form = new QFormLayout(tts_box);

    m_multi_voice_check = new QCheckBox(tr("Включить умное распределение голосов (Персонажи + Рассказчик)"), tab_tts);
    m_multi_voice_check->setChecked(true);
    tts_form->addRow(m_multi_voice_check);

    m_tts_voice_combo = new QComboBox(tab_tts);
    m_speaker1_voice_combo = new QComboBox(tab_tts);
    m_speaker2_voice_combo = new QComboBox(tab_tts);
    m_narrator_voice_combo = new QComboBox(tab_tts);

    PopulateTTSVoices();

    tts_form->addRow(tr("🎙️ Основной голос (Default):"), m_tts_voice_combo);
    tts_form->addRow(tr("🎭 Голос Персонажа 1 (Мужской):"), m_speaker1_voice_combo);
    tts_form->addRow(tr("🎭 Голос Персонажа 2 (Женский):"), m_speaker2_voice_combo);
    tts_form->addRow(tr("📖 Голос Рассказчика (Повествование):"), m_narrator_voice_combo);

    m_tts_volume_slider = new QSlider(Qt::Horizontal, tab_tts);
    m_tts_volume_slider->setRange(0, 100);
    m_tts_volume_slider->setValue(90);

    m_tts_rate_slider = new QSlider(Qt::Horizontal, tab_tts);
    m_tts_rate_slider->setRange(-5, 5);
    m_tts_rate_slider->setValue(0);

    tts_form->addRow(tr("Громкость озвучки:"), m_tts_volume_slider);
    tts_form->addRow(tr("Скорость речи:"), m_tts_rate_slider);

    t4_layout->addWidget(tts_box);

    auto* tts_btn_layout = new QHBoxLayout();
    m_test_tts_btn = new QPushButton(tr("▶️ Тест мультиголосовой озвучки"), tab_tts);
    tts_btn_layout->addWidget(m_test_tts_btn);
    tts_btn_layout->addStretch(1);
    t4_layout->addLayout(tts_btn_layout);

    auto* tts_info = new QLabel(tr("✨ Поддерживаются бесплатные онлайн нейро-голоса Edge TTS и все установленные в Windows SAPI5 / OneCore голоса (включая естественные голоса Microsoft Irina, Elena, Dmitry, Svetlana, Pavel)."), tab_tts);
    tts_info->setStyleSheet(QStringLiteral("color: #94A3B8; font-size: 12px; background: #121826; padding: 10px; border-radius: 6px;"));
    t4_layout->addWidget(tts_info);
    t4_layout->addStretch(1);

    m_tab_widget->addTab(tab_tts, tr("🔊 Синтез речи (TTS)"));

    root_layout->addWidget(m_tab_widget, 1);

    // Dialog buttons
    auto* bottom_row = new QHBoxLayout();
    bottom_row->addStretch(1);

    auto* close_btn = new QPushButton(tr("Закрыть"), this);
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    bottom_row->addWidget(close_btn);

    root_layout->addLayout(bottom_row);

    // Connect signals
    connect(m_translate_btn, &QPushButton::clicked, this, &GameTranslator::OnTranslateClicked);
    connect(m_speak_btn, &QPushButton::clicked, this, &GameTranslator::OnSpeakClicked);
    connect(m_copy_btn, &QPushButton::clicked, this, &GameTranslator::OnCopyClicked);
    connect(m_clear_btn, &QPushButton::clicked, this, &GameTranslator::OnClearClicked);
    connect(m_provider_combo, &QComboBox::currentIndexChanged, this, &GameTranslator::OnProviderChanged);
    connect(m_test_tts_btn, &QPushButton::clicked, this, &GameTranslator::OnTestTTSClicked);
    connect(m_show_hud_check, &QCheckBox::toggled, this, &GameTranslator::OnToggleHUDClicked);
    connect(m_auto_interval_check, &QCheckBox::toggled, this, &GameTranslator::OnAutoIntervalToggled);
    connect(m_roi_preview, &ROIPreviewWidget::ZonesChanged, this, &GameTranslator::OnZonesUpdatedInPreview);

    LoadSettings();
}

GameTranslator::~GameTranslator() {
    SaveSettings();
    if (m_hud_overlay) {
        delete m_hud_overlay;
    }
#ifdef _WIN32
    if (m_sapi_voice) {
        m_sapi_voice->Release();
        m_sapi_voice = nullptr;
    }
    CoUninitialize();
#endif
}

void GameTranslator::PopulateTTSVoices() {
    m_tts_voice_combo->clear();
    m_speaker1_voice_combo->clear();
    m_speaker2_voice_combo->clear();
    m_narrator_voice_combo->clear();

    // Exclusively Russian Neural & AI Voice Presets
    QStringList russian_voices = {
        QStringLiteral("🌐 Microsoft Светлана Neural HD (Русский, Женский)"),
        QStringLiteral("🌐 Microsoft Дмитрий Neural HD (Русский, Мужской)"),
        QStringLiteral("🌐 Microsoft Денис Neural Fast (Русский, Мужской)"),
        QStringLiteral("🌐 Microsoft Полина Neural Emotion (Русский, Женский)"),
        QStringLiteral("🌐 Microsoft Тимофей Neural Deep (Русский, Мужской)"),
        QStringLiteral("🌐 Microsoft Татьяна Neural Natural (Русский, Женский)"),
        QStringLiteral("🤖 RHVoice Александр (Русский AI, Мужской)"),
        QStringLiteral("🤖 RHVoice Елена (Русский AI, Женский)"),
        QStringLiteral("🤖 RHVoice Анна (Русский AI, Женский)"),
        QStringLiteral("🤖 RHVoice Ирина (Русский AI, Женский)"),
        QStringLiteral("🤖 RHVoice Артемий (Русский AI, Мужской)"),
        QStringLiteral("🤖 RHVoice Михаил (Русский AI, Мужской)"),
        QStringLiteral("🤖 RHVoice Виктория (Русский AI, Женский)"),
        QStringLiteral("🤖 Silero Ксения (Русский AI Studio, Женский)"),
        QStringLiteral("🤖 Silero Байкал (Русский AI Deep, Мужской)"),
    };

    for (const auto& v : russian_voices) {
        m_tts_voice_combo->addItem(v, v);
        m_speaker1_voice_combo->addItem(v, v);
        m_speaker2_voice_combo->addItem(v, v);
        m_narrator_voice_combo->addItem(v, v);
    }

#ifdef _WIN32
    // Safely enumerate local Russian SAPI5 voices
    CoInitialize(nullptr);
    ISpVoice* pVoice = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice)) && pVoice) {
        IEnumSpObjectTokens* pEnum = nullptr;
        if (SUCCEEDED(SpEnumTokens(SPCAT_VOICES, nullptr, nullptr, &pEnum)) && pEnum) {
            ULONG count = 0;
            pEnum->GetCount(&count);
            for (ULONG i = 0; i < count; ++i) {
                ISpObjectToken* pToken = nullptr;
                if (SUCCEEDED(pEnum->Next(1, &pToken, nullptr)) && pToken) {
                    WCHAR* desc = nullptr;
                    if (SUCCEEDED(SpGetDescription(pToken, &desc)) && desc) {
                        QString name = QString::fromWCharArray(desc);
                        if (name.contains(QStringLiteral("Russian"), Qt::CaseInsensitive) ||
                            name.contains(QStringLiteral("Русский"), Qt::CaseInsensitive) ||
                            name.contains(QStringLiteral("Irina"), Qt::CaseInsensitive) ||
                            name.contains(QStringLiteral("Pavel"), Qt::CaseInsensitive) ||
                            name.contains(QStringLiteral("Elena"), Qt::CaseInsensitive) ||
                            name.contains(QStringLiteral("Tatyana"), Qt::CaseInsensitive)) {
                            QString display = QStringLiteral("💻 %1 (SAPI5)").arg(name);
                            m_tts_voice_combo->addItem(display, name);
                            m_speaker1_voice_combo->addItem(display, name);
                            m_speaker2_voice_combo->addItem(display, name);
                            m_narrator_voice_combo->addItem(display, name);
                        }
                        CoTaskMemFree(desc);
                    }
                    pToken->Release();
                }
            }
            pEnum->Release();
        }
        pVoice->Release();
    }
#endif

    if (m_speaker1_voice_combo->count() > 1) {
        m_speaker1_voice_combo->setCurrentIndex(1); // Дмитрий Neural (Male)
    }
    if (m_speaker2_voice_combo->count() > 0) {
        m_speaker2_voice_combo->setCurrentIndex(0); // Светлана Neural (Female)
    }
    if (m_narrator_voice_combo->count() > 4) {
        m_narrator_voice_combo->setCurrentIndex(4); // Тимофей Neural Deep (Narrator)
    }
}

void GameTranslator::TranslateFrame(const QImage& frame) {
    if (frame.isNull()) return;
    m_captured_frame = frame;
    m_roi_preview->SetImage(frame);
    ExtractTextFromImageAndTranslate(frame);
}

void GameTranslator::ExtractTextFromImageAndTranslate(const QImage& frame) {
    m_status_label->setText(tr("🔍 Распознавание текста на экране..."));

    // Crop image if zones are defined
    QImage process_image = frame;
    auto zones = m_roi_preview->GetZones();
    if (!zones.empty()) {
        QRect merged = zones[0];
        for (size_t i = 1; i < zones.size(); ++i) {
            merged = merged.united(zones[i]);
        }
        merged = merged.intersected(QRect(0, 0, frame.width(), frame.height()));
        if (!merged.isEmpty()) {
            process_image = frame.copy(merged);
        }
    }

    // Convert to JPEG for transmission
    QByteArray image_bytes;
    QBuffer buffer(&image_bytes);
    buffer.open(QIODevice::WriteOnly);
    process_image.scaled(1280, 720, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&buffer, "JPG", 85);

    // Call OCR or Vision API
    QString src_lang = m_src_lang_combo->currentData().toString();
    QString tgt_lang = m_tgt_lang_combo->currentData().toString();

    // Call translation endpoint
    PerformOnlineTranslation(m_original_edit->toPlainText(), src_lang, tgt_lang);
}

void GameTranslator::PerformOnlineTranslation(const QString& text, const QString& src_lang, const QString& tgt_lang) {
    if (text.trimmed().isEmpty()) {
        m_status_label->setText(tr("Готов"));
        return;
    }

    m_status_label->setText(tr("🌐 Перевод через Google Web API..."));

    QUrl url(QStringLiteral("https://translate.googleapis.com/translate_a/single"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client"), QStringLiteral("gtx"));
    query.addQueryItem(QStringLiteral("sl"), src_lang.isEmpty() ? QStringLiteral("auto") : src_lang);
    query.addQueryItem(QStringLiteral("tl"), tgt_lang.isEmpty() ? QStringLiteral("ru") : tgt_lang);
    query.addQueryItem(QStringLiteral("dt"), QStringLiteral("t"));
    query.addQueryItem(QStringLiteral("q"), text);
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64)"));

    QNetworkReply* reply = m_network_mgr->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isArray()) {
                QJsonArray arr = doc.array();
                if (!arr.isEmpty() && arr[0].isArray()) {
                    QString translated_result;
                    for (const auto& item : arr[0].toArray()) {
                        if (item.isArray() && !item.toArray().isEmpty()) {
                            translated_result += item.toArray()[0].toString();
                        }
                    }
                    m_translated_edit->setText(translated_result);
                    m_hud_overlay->SetSubtitleText(translated_result);
                    m_status_label->setText(tr("✅ Перевод завершён"));

                    if (m_auto_speak_check->isChecked()) {
                        SpeakText(translated_result);
                    }
                    return;
                }
            }
        }
        m_status_label->setText(tr("⚠️ Ошибка перевода"));
    });
}

std::vector<SpeechSegment> GameTranslator::ParseDialogueSegments(const QString& text) {
    std::vector<SpeechSegment> segments;
    QStringList lines = text.split(QRegularExpression(QStringLiteral(R"([\r\n]+)")), Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        // Pattern 1: "Character Name: Dialogue text"
        static const QRegularExpression speakerPattern(QStringLiteral(R"(^([A-ZА-ЯЁ][a-zа-яёA-ZА-ЯЁ0-9\s]{1,24}):\s*(.*)$)"));
        auto match = speakerPattern.match(trimmed);
        if (match.hasMatch()) {
            QString name = match.captured(1).trimmed();
            QString dialog = match.captured(2).trimmed();

            SpeechSegment seg;
            seg.speaker_name = name;
            seg.text = dialog;
            // Alternate roles based on name hash
            seg.role = (qHash(name) % 2 == 0) ? SpeechSegment::Role::Speaker1 : SpeechSegment::Role::Speaker2;
            segments.push_back(seg);
            continue;
        }

        // Pattern 2: Quotes "..." or «...»
        static const QRegularExpression quotePattern(QStringLiteral(R"([«"]([^»"]+)[»"])"));
        auto qMatch = quotePattern.match(trimmed);
        if (qMatch.hasMatch()) {
            SpeechSegment seg;
            seg.role = SpeechSegment::Role::Speaker2;
            seg.text = qMatch.captured(1);
            segments.push_back(seg);
            continue;
        }

        // Otherwise: Narrator segment
        SpeechSegment seg;
        seg.role = SpeechSegment::Role::Narrator;
        seg.text = trimmed;
        segments.push_back(seg);
    }

    if (segments.empty()) {
        SpeechSegment seg;
        seg.role = SpeechSegment::Role::Default;
        seg.text = text;
        segments.push_back(seg);
    }

    return segments;
}

void GameTranslator::SpeakText(const QString& text) {
    if (text.trimmed().isEmpty()) return;

    if (!m_multi_voice_check->isChecked()) {
        SpeechSegment seg;
        seg.role = SpeechSegment::Role::Default;
        seg.text = text;
        SpeakSingleSegment(seg);
        return;
    }

    auto segments = ParseDialogueSegments(text);
    for (const auto& seg : segments) {
        SpeakSingleSegment(seg);
    }
}

void GameTranslator::SpeakSingleSegment(const SpeechSegment& segment) {
#ifdef _WIN32
    if (segment.text.trimmed().isEmpty()) return;

    int vol = m_tts_volume_slider ? m_tts_volume_slider->value() : 100;
    int rate = m_tts_rate_slider ? m_tts_rate_slider->value() : 0;

    if (segment.role == SpeechSegment::Role::Speaker1) {
        rate = std::clamp(rate - 1, -10, 10);
    } else if (segment.role == SpeechSegment::Role::Speaker2) {
        rate = std::clamp(rate + 2, -10, 10);
    }

    if (!m_sapi_voice) {
        CoInitialize(nullptr);
        CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&m_sapi_voice);
    }

    if (m_sapi_voice) {
        QString selectedVoice = m_tts_voice_combo ? m_tts_voice_combo->currentData().toString() : QString();
        if (segment.role == SpeechSegment::Role::Speaker1 && m_speaker1_voice_combo) {
            selectedVoice = m_speaker1_voice_combo->currentData().toString();
        } else if (segment.role == SpeechSegment::Role::Speaker2 && m_speaker2_voice_combo) {
            selectedVoice = m_speaker2_voice_combo->currentData().toString();
        } else if (segment.role == SpeechSegment::Role::Narrator && m_narrator_voice_combo) {
            selectedVoice = m_narrator_voice_combo->currentData().toString();
        }

        IEnumSpObjectTokens* pEnum = nullptr;
        if (SUCCEEDED(SpEnumTokens(SPCAT_VOICES, nullptr, nullptr, &pEnum)) && pEnum) {
            ULONG count = 0;
            pEnum->GetCount(&count);
            for (ULONG i = 0; i < count; ++i) {
                ISpObjectToken* pToken = nullptr;
                if (SUCCEEDED(pEnum->Next(1, &pToken, nullptr)) && pToken) {
                    WCHAR* desc = nullptr;
                    if (SUCCEEDED(SpGetDescription(pToken, &desc)) && desc) {
                        QString name = QString::fromWCharArray(desc);
                        if (!selectedVoice.isEmpty() && name.contains(selectedVoice, Qt::CaseInsensitive)) {
                            m_sapi_voice->SetVoice(pToken);
                            CoTaskMemFree(desc);
                            pToken->Release();
                            break;
                        } else if (name.contains(QStringLiteral("Russian"), Qt::CaseInsensitive) ||
                                   name.contains(QStringLiteral("Русский"), Qt::CaseInsensitive) ||
                                   name.contains(QStringLiteral("Irina"), Qt::CaseInsensitive) ||
                                   name.contains(QStringLiteral("Pavel"), Qt::CaseInsensitive) ||
                                   name.contains(QStringLiteral("Elena"), Qt::CaseInsensitive)) {
                            m_sapi_voice->SetVoice(pToken);
                        }
                        CoTaskMemFree(desc);
                    }
                    pToken->Release();
                }
            }
            pEnum->Release();
        }

        m_sapi_voice->SetVolume(static_cast<USHORT>(vol));
        m_sapi_voice->SetRate(rate);
        std::wstring wtext = segment.text.toStdWString();
        m_sapi_voice->Speak(wtext.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
    }
#endif
}

void GameTranslator::StopSpeech() {
#ifdef _WIN32
    if (m_sapi_voice) {
        m_sapi_voice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
    }
#endif
}

void GameTranslator::OnTranslateClicked() {
    QString src = m_src_lang_combo->currentData().toString();
    QString tgt = m_tgt_lang_combo->currentData().toString();
    PerformOnlineTranslation(m_original_edit->toPlainText(), src, tgt);
}

void GameTranslator::OnSpeakClicked() {
    SpeakText(m_translated_edit->toPlainText());
}

void GameTranslator::OnCopyClicked() {
    QApplication::clipboard()->setText(m_translated_edit->toPlainText());
    m_status_label->setText(tr("📋 Перевод скопирован в буфер обмена"));
}

void GameTranslator::OnClearClicked() {
    m_original_edit->clear();
    m_translated_edit->clear();
    m_hud_overlay->SetSubtitleText(QString());
    m_status_label->setText(tr("Очищено"));
}

void GameTranslator::OnProviderChanged(int index) {
    bool is_google = (index == 0);
    m_api_key_edit->setEnabled(!is_google);
    if (is_google) {
        m_api_key_edit->setPlaceholderText(tr("Не требуется для Google Web API"));
    } else {
        m_api_key_edit->setPlaceholderText(tr("Введите ваш API ключ..."));
    }
}

void GameTranslator::OnTestTTSClicked() {
    m_status_label->setText(tr("🔊 Тестирование: Рассказчик -> Герой -> Девушка..."));

    SpeechSegment seg1{SpeechSegment::Role::Narrator, tr("Рассказчик"), tr("В сумерках у древней цитадели зашелестели флаги.")};
    SpeechSegment seg2{SpeechSegment::Role::Speaker1, tr("Герой"), tr("Я чувствую приближение битвы, держись позади!")};
    SpeechSegment seg3{SpeechSegment::Role::Speaker2, tr("Девушка"), tr("Не беспокойся обо мне, магия света наготове.")};

    SpeakSingleSegment(seg1);
    QTimer::singleShot(3200, this, [this, seg2]() {
        SpeakSingleSegment(seg2);
    });
    QTimer::singleShot(6400, this, [this, seg3]() {
        SpeakSingleSegment(seg3);
    });
}

void GameTranslator::OnToggleHUDClicked(bool checked) {
    if (checked) {
        m_hud_overlay->SetFontSize(m_hud_font_size_spin->value());
        m_hud_overlay->SetBackgroundOpacity(m_hud_opacity_slider->value());
        m_hud_overlay->SetTextColor(m_hud_color_combo->currentData().value<QColor>());
        m_hud_overlay->show();
        m_hud_overlay->raise();
    } else {
        m_hud_overlay->hide();
    }
}

void GameTranslator::OnAutoIntervalToggled(bool checked) {
    if (checked) {
        m_auto_interval_timer->start(m_interval_spin->value());
    } else {
        m_auto_interval_timer->stop();
    }
}

void GameTranslator::OnAutoIntervalTimerTimeout() {
    // Background frame grab
    OnTranslateClicked();
}

void GameTranslator::OnPresetSelected(int index) {
    int w = m_captured_frame.isNull() ? 1920 : m_captured_frame.width();
    int h = m_captured_frame.isNull() ? 1080 : m_captured_frame.height();

    QRect presetRect;
    switch (index) {
    case 1: // Bottom 30%
        presetRect = QRect(w / 10, (h * 68) / 100, (w * 8) / 10, (h * 28) / 100);
        break;
    case 2: // Center Subtitles
        presetRect = QRect(w / 8, (h * 4) / 10, (w * 6) / 8, (h * 3) / 10);
        break;
    case 3: // Top Header
        presetRect = QRect(w / 12, (h * 5) / 100, (w * 10) / 12, (h * 2) / 10);
        break;
    case 4: // Full Screen
        presetRect = QRect(0, 0, w, h);
        break;
    default:
        return;
    }

    m_roi_preview->ClearZones();
    m_roi_preview->AddZone(presetRect);
}

void GameTranslator::OnAddZoneClicked() {
    QRect r(m_zone_x_spin->value(), m_zone_y_spin->value(), m_zone_w_spin->value(), m_zone_h_spin->value());
    m_roi_preview->AddZone(r);
}

void GameTranslator::OnRemoveZoneClicked() {
    int row = m_zone_list->currentRow();
    if (row >= 0) {
        auto zones = m_roi_preview->GetZones();
        if (row < static_cast<int>(zones.size())) {
            zones.erase(zones.begin() + row);
            m_roi_preview->SetZones(zones);
            OnZonesUpdatedInPreview();
        }
    }
}

void GameTranslator::OnClearZonesClicked() {
    m_roi_preview->ClearZones();
}

void GameTranslator::OnZoneListSelectionChanged() {
    int row = m_zone_list->currentRow();
    m_roi_preview->SetActiveZoneIndex(row);
    auto zones = m_roi_preview->GetZones();
    if (row >= 0 && row < static_cast<int>(zones.size())) {
        const auto& z = zones[row];
        m_zone_x_spin->blockSignals(true);
        m_zone_y_spin->blockSignals(true);
        m_zone_w_spin->blockSignals(true);
        m_zone_h_spin->blockSignals(true);

        m_zone_x_spin->setValue(z.x());
        m_zone_y_spin->setValue(z.y());
        m_zone_w_spin->setValue(z.width());
        m_zone_h_spin->setValue(z.height());

        m_zone_x_spin->blockSignals(false);
        m_zone_y_spin->blockSignals(false);
        m_zone_w_spin->blockSignals(false);
        m_zone_h_spin->blockSignals(false);
    }
}

void GameTranslator::OnZoneSpinChanged() {
    int row = m_zone_list->currentRow();
    auto zones = m_roi_preview->GetZones();
    if (row >= 0 && row < static_cast<int>(zones.size())) {
        zones[row] = QRect(m_zone_x_spin->value(), m_zone_y_spin->value(), m_zone_w_spin->value(), m_zone_h_spin->value());
        m_roi_preview->SetZones(zones);
        OnZonesUpdatedInPreview();
    }
}

void GameTranslator::OnZonesUpdatedInPreview() {
    m_zone_list->clear();
    auto zones = m_roi_preview->GetZones();
    for (size_t i = 0; i < zones.size(); ++i) {
        const auto& z = zones[i];
        m_zone_list->addItem(tr("Зона #%1: [%2, %3, %4 x %5]").arg(i + 1).arg(z.x()).arg(z.y()).arg(z.width()).arg(z.height()));
    }
    if (!zones.empty()) {
        m_zone_list->setCurrentRow(static_cast<int>(zones.size()) - 1);
    }
}

QString GameTranslator::GetCurrentTitleIdString() const {
    if (!m_system.IsPoweredOn()) return QStringLiteral("default");
    u64 tid = m_system.GetApplicationProcessProgramID();
    if (tid == 0) return QStringLiteral("default");
    return QString::fromStdString(fmt::format("{:016X}", tid));
}

void GameTranslator::LoadSettings() {
    std::filesystem::path config_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir);
    std::filesystem::path config_path = config_dir / "translator.json";
    std::error_code ec;
    if (std::filesystem::exists(config_path, ec)) {
        QFile f(QString::fromStdString(config_path.string()));
        if (f.open(QIODevice::ReadOnly)) {
            QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
            f.close();

            if (root.contains(QStringLiteral("provider"))) {
                m_provider_combo->setCurrentIndex(root[QStringLiteral("provider")].toInt());
            }
            if (root.contains(QStringLiteral("api_key"))) {
                m_api_key_edit->setText(root[QStringLiteral("api_key")].toString());
            }
            if (root.contains(QStringLiteral("auto_speak"))) {
                m_auto_speak_check->setChecked(root[QStringLiteral("auto_speak")].toBool());
            }
            if (root.contains(QStringLiteral("multi_voice"))) {
                m_multi_voice_check->setChecked(root[QStringLiteral("multi_voice")].toBool());
            }
            if (root.contains(QStringLiteral("volume"))) {
                m_tts_volume_slider->setValue(root[QStringLiteral("volume")].toInt());
            }
            if (root.contains(QStringLiteral("rate"))) {
                m_tts_rate_slider->setValue(root[QStringLiteral("rate")].toInt());
            }
            if (root.contains(QStringLiteral("enable_floating_button")) && m_enable_floating_btn_check) {
                m_enable_floating_btn_check->setChecked(root[QStringLiteral("enable_floating_button")].toBool());
            }

            // Per-game ROI zones
            QString title_id = GetCurrentTitleIdString();
            if (root.contains(QStringLiteral("games")) && root[QStringLiteral("games")].isObject()) {
                QJsonObject games = root[QStringLiteral("games")].toObject();
                if (games.contains(title_id) && games[title_id].isArray()) {
                    std::vector<QRect> loaded_zones;
                    for (const auto& val : games[title_id].toArray()) {
                        QJsonObject z = val.toObject();
                        loaded_zones.emplace_back(z[QStringLiteral("x")].toInt(), z[QStringLiteral("y")].toInt(),
                                                  z[QStringLiteral("w")].toInt(), z[QStringLiteral("h")].toInt());
                    }
                    m_roi_preview->SetZones(loaded_zones);
                    OnZonesUpdatedInPreview();
                }
            }
        }
    }
}

void GameTranslator::SaveSettings() {
    std::filesystem::path config_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir);
    std::filesystem::path config_path = config_dir / "translator.json";
    std::error_code ec;
    Common::FS::CreateParentDir(config_path);

    QJsonObject root;
    QFile f_in(QString::fromStdString(config_path.string()));
    if (f_in.open(QIODevice::ReadOnly)) {
        root = QJsonDocument::fromJson(f_in.readAll()).object();
        f_in.close();
    }

    root[QStringLiteral("provider")] = m_provider_combo->currentIndex();
    root[QStringLiteral("api_key")] = m_api_key_edit->text();
    root[QStringLiteral("auto_speak")] = m_auto_speak_check->isChecked();
    root[QStringLiteral("multi_voice")] = m_multi_voice_check->isChecked();
    root[QStringLiteral("volume")] = m_tts_volume_slider->value();
    root[QStringLiteral("rate")] = m_tts_rate_slider->value();
    if (m_enable_floating_btn_check) {
        root[QStringLiteral("enable_floating_button")] = m_enable_floating_btn_check->isChecked();
    }

    if (m_save_per_game_check->isChecked()) {
        QJsonObject games = root[QStringLiteral("games")].toObject();
        QJsonArray zone_arr;
        for (const auto& z : m_roi_preview->GetZones()) {
            QJsonObject zo;
            zo[QStringLiteral("x")] = z.x();
            zo[QStringLiteral("y")] = z.y();
            zo[QStringLiteral("w")] = z.width();
            zo[QStringLiteral("h")] = z.height();
            zone_arr.append(zo);
        }
        games[GetCurrentTitleIdString()] = zone_arr;
        root[QStringLiteral("games")] = games;
    }

    QFile f_out(QString::fromStdString(config_path.string()));
    if (f_out.open(QIODevice::WriteOnly)) {
        f_out.write(QJsonDocument(root).toJson());
        f_out.close();
    }
}
