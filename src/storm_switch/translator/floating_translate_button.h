// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QElapsedTimer>
#include <QPoint>
#include <QWidget>

class QMenu;

class FloatingTranslateButton : public QWidget {
    Q_OBJECT

public:
    explicit FloatingTranslateButton(QWidget* parent = nullptr);
    ~FloatingTranslateButton() override;

    void SetVisibleState(bool visible);

signals:
    void TranslateRequested();
    void OpenSettingsRequested();
    void ToggleHUDRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QPoint m_drag_position;
    QPoint m_press_position;
    QElapsedTimer m_press_timer;
    bool m_is_dragging{false};
    bool m_is_hovered{false};
    bool m_is_pressed{false};
};
