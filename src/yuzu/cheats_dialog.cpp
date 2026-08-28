// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include "yuzu/cheats_dialog.h"
#include "yuzu/configuration/configure_per_game_cheats.h"

CheatsDialog::CheatsDialog(QWidget* parent, Core::System& system, u64 title_id, const QString& file_name)
    : QDialog(parent) {
    setWindowTitle(tr("⚡ Менеджер чит-кодов"));
    resize(1320, 760);
    setMinimumSize(1150, 680);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    cheats_widget = new ConfigurePerGameCheats(system, title_id, file_name, this);
    layout->addWidget(cheats_widget, 1);

    auto* button_layout = new QHBoxLayout();
    auto* close_button = new QPushButton(tr("Закрыть"), this);
    close_button->setStyleSheet(QStringLiteral("background-color: #00f2fe; color: #000000; font-weight: bold; padding: 6px 20px; border-radius: 5px;"));
    button_layout->addStretch(1);
    button_layout->addWidget(close_button);
    layout->addLayout(button_layout);

    connect(close_button, &QPushButton::clicked, this, [this]() {
        if (cheats_widget) {
            cheats_widget->ApplyConfiguration();
        }
        accept();
    });
}

CheatsDialog::~CheatsDialog() = default;
