// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAction>
#include <QContextMenuEvent>
#include <QFile>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRadialGradient>
#include <QScreen>
#include <filesystem>
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "yuzu/translator/floating_translate_button.h"

FloatingTranslateButton::FloatingTranslateButton(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::SubWindow) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    resize(56, 56);
    setWindowTitle(QStringLiteral("STORM EDEN — Translate Button"));
    setToolTip(tr("🌐 Перевод экрана (Клик — перевести, Удержание/ПКМ — настройки)"));
}

FloatingTranslateButton::~FloatingTranslateButton() = default;

void FloatingTranslateButton::SetVisibleState(bool visible) {
    if (visible) {
        show();
        raise();
    } else {
        hide();
    }
}

void FloatingTranslateButton::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    QRect r = rect().adjusted(3, 3, -3, -3);
    const qreal cx = r.center().x();
    const qreal cy = r.center().y();
    const qreal radius = r.width() / 2.0;

    // Drop shadow
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 160));
    p.drawEllipse(r.adjusted(2, 2, 2, 2));

    // Outer glow / border
    QColor borderColor = m_is_hovered ? QColor(0, 229, 255, 240) : QColor(0, 229, 255, 170);
    if (m_is_pressed) {
        borderColor = QColor(245, 158, 11, 240);
    }
    p.setPen(QPen(borderColor, m_is_hovered ? 2.5 : 1.8));

    // Radial gradient background
    QRadialGradient bgGrad(QPointF(cx, cy - 4), radius);
    if (m_is_pressed) {
        bgGrad.setColorAt(0.0, QColor(30, 45, 75, 240));
        bgGrad.setColorAt(0.7, QColor(16, 24, 42, 230));
        bgGrad.setColorAt(1.0, QColor(8, 12, 22, 240));
    } else if (m_is_hovered) {
        bgGrad.setColorAt(0.0, QColor(25, 45, 80, 235));
        bgGrad.setColorAt(0.7, QColor(14, 22, 38, 225));
        bgGrad.setColorAt(1.0, QColor(7, 10, 18, 235));
    } else {
        bgGrad.setColorAt(0.0, QColor(18, 28, 50, 200));
        bgGrad.setColorAt(0.7, QColor(11, 16, 30, 190));
        bgGrad.setColorAt(1.0, QColor(5, 8, 15, 200));
    }
    p.setBrush(bgGrad);
    p.drawEllipse(r);

    // Inner subtle neon ring
    p.setPen(QPen(QColor(124, 58, 237, m_is_hovered ? 120 : 60), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(r.adjusted(3, 3, -3, -3));

    // Draw stylized Globe / Translate glyphs
    QFont font(QStringLiteral("Segoe UI"), 15, QFont::Bold);
    p.setFont(font);

    // Symbol: "文" and "A"
    p.setPen(m_is_pressed ? QColor(245, 158, 11) : (m_is_hovered ? QColor(0, 229, 255) : QColor(240, 246, 252)));
    p.drawText(r.adjusted(0, -3, 0, 0), Qt::AlignCenter, QStringLiteral("🌐"));

    // Tiny badge at bottom
    QFont tinyFont(QStringLiteral("Segoe UI"), 7, QFont::Bold);
    p.setFont(tinyFont);
    p.setPen(QColor(0, 229, 255, m_is_hovered ? 255 : 200));
    p.drawText(r.adjusted(0, 24, 0, 0), Qt::AlignCenter, QStringLiteral("EDEN"));
}

void FloatingTranslateButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_press_position = event->globalPosition().toPoint();
        m_drag_position = event->globalPosition().toPoint() - frameGeometry().topLeft();
        m_is_dragging = false;
        m_is_pressed = true;
        m_press_timer.start();
        update();
        event->accept();
    }
}

void FloatingTranslateButton::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        QPoint current_pos = event->globalPosition().toPoint();
        if ((current_pos - m_press_position).manhattanLength() > 5) {
            m_is_dragging = true;
            move(current_pos - m_drag_position);
        }
        event->accept();
    }
}

void FloatingTranslateButton::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_is_pressed = false;
        update();

        if (!m_is_dragging) {
            qint64 elapsed = m_press_timer.elapsed();
            if (elapsed >= 450) {
                // Long press: open settings
                emit OpenSettingsRequested();
            } else {
                // Short click: run translation
                emit TranslateRequested();
            }
        }
        m_is_dragging = false;
        event->accept();
    }
}

void FloatingTranslateButton::enterEvent(QEnterEvent* /*event*/) {
    m_is_hovered = true;
    update();
}

void FloatingTranslateButton::leaveEvent(QEvent* /*event*/) {
    m_is_hovered = false;
    m_is_pressed = false;
    update();
}

void FloatingTranslateButton::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu { background: #0D1424; color: #F8FAFC; border: 1px solid #00E5FF; border-radius: 8px; padding: 6px; }"
        "QMenu::item { padding: 8px 24px; border-radius: 4px; font-weight: 500; font-size: 13px; }"
        "QMenu::item:selected { background: #2563EB; color: #FFFFFF; }"
        "QMenu::separator { height: 1px; background: #1E293B; margin: 4px 8px; }"
    ));

    auto* act_translate = menu.addAction(tr("⚡ Перевести экран сейчас"));
    auto* act_settings = menu.addAction(tr("⚙️ Настройки переводчика..."));
    auto* act_hud = menu.addAction(tr("📺 Показать / Скрыть HUD субтитры"));
    menu.addSeparator();
    auto* act_hide = menu.addAction(tr("❌ Скрыть плавающую кнопку"));

    connect(act_translate, &QAction::triggered, this, &FloatingTranslateButton::TranslateRequested);
    connect(act_settings, &QAction::triggered, this, &FloatingTranslateButton::OpenSettingsRequested);
    connect(act_hud, &QAction::triggered, this, &FloatingTranslateButton::ToggleHUDRequested);
    connect(act_hide, &QAction::triggered, this, [this]() {
        SetVisibleState(false);
        std::filesystem::path config_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir);
        std::filesystem::path config_path = config_dir / "translator.json";
        std::error_code ec;
        QJsonObject root;
        QFile f_in(QString::fromStdString(config_path.string()));
        if (f_in.open(QIODevice::ReadOnly)) {
            root = QJsonDocument::fromJson(f_in.readAll()).object();
            f_in.close();
        }
        root[QStringLiteral("enable_floating_button")] = false;
        QFile f_out(QString::fromStdString(config_path.string()));
        if (f_out.open(QIODevice::WriteOnly)) {
            f_out.write(QJsonDocument(root).toJson());
            f_out.close();
        }
    });

    menu.exec(event->globalPos());
}
