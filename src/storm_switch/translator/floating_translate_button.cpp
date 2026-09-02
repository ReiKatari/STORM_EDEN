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
#include "storm_switch/translator/floating_translate_button.h"

FloatingTranslateButton::FloatingTranslateButton(QWidget* parent)
    : QWidget(parent, Qt::SubWindow | Qt::FramelessWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    resize(60, 60);
    setWindowTitle(QStringLiteral("STORM SWITCH — Translate"));
    setToolTip(tr("🌐 Перевод экрана STORM SWITCH\n• Клик: мгновенный перевод экрана\n• Долгое нажатие / ПКМ: настройки и субтитры"));
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
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF r = rect().adjusted(3, 3, -3, -3);
    const qreal cx = r.center().x();
    const qreal cy = r.center().y();
    const qreal radius = r.width() / 2.0;

    // 1. Ambient Drop Shadow
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 180));
    p.drawEllipse(r.adjusted(2, 3, 2, 3));

    // 2. Outer Glow & Border Gradient
    QLinearGradient borderGrad(r.topLeft(), r.bottomRight());
    if (m_is_pressed) {
        borderGrad.setColorAt(0.0, QColor(245, 158, 11, 255));
        borderGrad.setColorAt(1.0, QColor(239, 68, 68, 255));
    } else if (m_is_hovered) {
        borderGrad.setColorAt(0.0, QColor(0, 229, 255, 255));
        borderGrad.setColorAt(0.5, QColor(139, 92, 246, 255));
        borderGrad.setColorAt(1.0, QColor(0, 229, 255, 255));
    } else {
        borderGrad.setColorAt(0.0, QColor(0, 229, 255, 190));
        borderGrad.setColorAt(0.5, QColor(99, 102, 241, 160));
        borderGrad.setColorAt(1.0, QColor(0, 229, 255, 190));
    }
    p.setPen(QPen(QBrush(borderGrad), m_is_hovered ? 2.6 : 1.9));

    // 3. Cyber Frosted Glass Background
    QRadialGradient bgGrad(QPointF(cx, cy - 6), radius * 1.2);
    if (m_is_pressed) {
        bgGrad.setColorAt(0.0, QColor(35, 25, 55, 245));
        bgGrad.setColorAt(0.6, QColor(18, 14, 32, 240));
        bgGrad.setColorAt(1.0, QColor(8, 6, 16, 245));
    } else if (m_is_hovered) {
        bgGrad.setColorAt(0.0, QColor(15, 38, 75, 240));
        bgGrad.setColorAt(0.6, QColor(10, 20, 42, 235));
        bgGrad.setColorAt(1.0, QColor(5, 10, 24, 245));
    } else {
        bgGrad.setColorAt(0.0, QColor(12, 26, 52, 220));
        bgGrad.setColorAt(0.6, QColor(8, 16, 34, 210));
        bgGrad.setColorAt(1.0, QColor(4, 8, 18, 225));
    }
    p.setBrush(bgGrad);
    p.drawEllipse(r);

    // 4. Inner Neon Ring
    p.setPen(QPen(QColor(139, 92, 246, m_is_hovered ? 140 : 70), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(r.adjusted(3.5, 3.5, -3.5, -3.5));

    // 5. Stylized Dual Translation Glyphs: "A" <-> "文"
    QFont fontA(QStringLiteral("Segoe UI"), 11, QFont::Bold);
    QFont fontKanji(QStringLiteral("Microsoft YaHei"), 11, QFont::Bold);

    // "A" (Latin) on the left
    p.setFont(fontA);
    p.setPen(m_is_pressed ? QColor(245, 158, 11) : (m_is_hovered ? QColor(0, 229, 255) : QColor(241, 245, 249)));
    p.drawText(QRectF(r.left() + 6, r.top() + 8, 18, 20), Qt::AlignCenter, QStringLiteral("A"));

    // Subtle translation arrows in center
    QFont arrowFont(QStringLiteral("Segoe UI"), 8, QFont::Bold);
    p.setFont(arrowFont);
    p.setPen(QColor(148, 163, 184, m_is_hovered ? 240 : 180));
    p.drawText(QRectF(r.left() + 21, r.top() + 9, 14, 18), Qt::AlignCenter, QStringLiteral("⇄"));

    // "文" (Kanji/Han) on the right
    p.setFont(fontKanji);
    p.setPen(m_is_pressed ? QColor(245, 158, 11) : (m_is_hovered ? QColor(168, 85, 247) : QColor(203, 213, 225)));
    p.drawText(QRectF(r.right() - 24, r.top() + 8, 18, 20), Qt::AlignCenter, QStringLiteral("文"));

    // 6. Modern "STORM" Badge at Bottom
    QFont badgeFont(QStringLiteral("Segoe UI"), 7, QFont::Bold);
    badgeFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    p.setFont(badgeFont);
    p.setPen(m_is_pressed ? QColor(245, 158, 11, 255) : (m_is_hovered ? QColor(0, 229, 255, 255) : QColor(0, 229, 255, 190)));
    p.drawText(QRectF(r.left(), r.bottom() - 18, r.width(), 14), Qt::AlignCenter, QStringLiteral("STORM"));
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
