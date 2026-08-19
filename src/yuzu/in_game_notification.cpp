// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include "yuzu/in_game_notification.h"

InGameNotificationOverlay::InGameNotificationOverlay(QWidget* parent)
    : QWidget(parent, Qt::SubWindow | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);

    setFixedSize(460, 68);

    m_opacity_effect = new QGraphicsOpacityEffect(this);
    m_opacity_effect->setOpacity(0.0);
    setGraphicsEffect(m_opacity_effect);

    m_fade_anim = new QPropertyAnimation(m_opacity_effect, "opacity", this);
    m_fade_anim->setDuration(240);

    m_hide_timer.setSingleShot(true);
    connect(&m_hide_timer, &QTimer::timeout, this, [this]() {
        m_fade_anim->stop();
        m_fade_anim->setStartValue(m_opacity_effect->opacity());
        m_fade_anim->setEndValue(0.0);
        connect(m_fade_anim, &QPropertyAnimation::finished, this, &QWidget::hide, Qt::UniqueConnection);
        m_fade_anim->start();
    });

    hide();
}

void InGameNotificationOverlay::ShowNotification(const QString& title, const QString& message,
                                                 const QColor& accent, const QString& iconText,
                                                 int displayDurationMs) {
    m_title = title;
    m_message = message;
    m_accent_color = accent;
    m_icon = iconText;

    if (parentWidget()) {
        int x = (parentWidget()->width() - width()) / 2;
        int y = 28;
        move(x, y);
    }

    m_hide_timer.stop();
    m_fade_anim->disconnect(SIGNAL(finished()));
    m_fade_anim->stop();

    show();
    raise();

    m_fade_anim->setStartValue(m_opacity_effect->opacity());
    m_fade_anim->setEndValue(1.0);
    m_fade_anim->start();

    update();
    m_hide_timer.start(displayDurationMs);
}

void InGameNotificationOverlay::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(2, 2, -2, -2);

    // Background pill shape
    QPainterPath bgPath;
    bgPath.addRoundedRect(r, 14, 14);

    // Dark cyberpunk glassmorphism gradient
    QLinearGradient bgGrad(0, 0, 0, height());
    bgGrad.setColorAt(0.0, QColor(13, 20, 36, 235));
    bgGrad.setColorAt(1.0, QColor(8, 12, 22, 245));
    p.fillPath(bgPath, bgGrad);

    // Neon accent outline
    QPen borderPen(m_accent_color, 1.5);
    p.setPen(borderPen);
    p.setBrush(Qt::NoBrush);
    p.drawPath(bgPath);

    // Glowing top bar accent
    QPainterPath topBar;
    topBar.addRoundedRect(QRect(r.x() + 24, r.y(), r.width() - 48, 3), 1.5, 1.5);
    p.fillPath(topBar, m_accent_color);

    // Left Icon badge
    QRect iconRect(r.x() + 14, r.y() + (r.height() - 38) / 2, 38, 38);
    QPainterPath iconBadge;
    iconBadge.addRoundedRect(iconRect, 10, 10);
    QColor badgeBg = m_accent_color;
    badgeBg.setAlpha(45);
    p.fillPath(iconBadge, badgeBg);
    p.setPen(QPen(m_accent_color, 1.2));
    p.drawPath(iconBadge);

    // Draw icon symbol
    p.setPen(m_accent_color);
    QFont iconFont(QStringLiteral("Segoe UI Emoji"), 16, QFont::Bold);
    p.setFont(iconFont);
    p.drawText(iconRect, Qt::AlignCenter, m_icon);

    // Title
    QRect titleRect(iconRect.right() + 14, r.y() + 10, r.width() - 80, 22);
    p.setPen(QColor(241, 245, 249));
    QFont titleFont(QStringLiteral("Segoe UI"), 11, QFont::Bold);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    p.setFont(titleFont);
    p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, m_title);

    // Subtitle Message
    QRect msgRect(iconRect.right() + 14, r.y() + 32, r.width() - 80, 20);
    p.setPen(QColor(148, 163, 184));
    QFont msgFont(QStringLiteral("Segoe UI"), 9, QFont::Normal);
    p.setFont(msgFont);
    p.drawText(msgRect, Qt::AlignLeft | Qt::AlignVCenter, m_message);
}
