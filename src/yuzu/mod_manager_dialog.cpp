// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTabBar>
#include <QUrl>
#include <QVBoxLayout>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "qt_common/abstract/frontend.h"
#include "yuzu/configuration/configure_gamebanana_mods.h"
#include "yuzu/configuration/configure_per_game_addons.h"
#include "yuzu/mod_manager_dialog.h"

ModManagerDialog::ModManagerDialog(QWidget* parent, Core::System& system_, u64 title_id_,
                                   const QString& game_path_, const QString& game_name_)
    : QDialog(parent), system{system_}, title_id{title_id_}, game_path{game_path_},
      game_name{game_name_} {
    const QString display_title = game_name.isEmpty()
        ? QStringLiteral("0x%1").arg(title_id, 16, 16, QLatin1Char('0')).toUpper()
        : game_name;
    setWindowTitle(tr("🧩 STORM EDEN — Менеджер модов: %1").arg(display_title));
    resize(1320, 760);
    setMinimumSize(1150, 680);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(10, 10, 10, 10);
    main_layout->setSpacing(10);

    tab_widget = new QTabWidget(this);
    QFont tab_font = font();
    tab_font.setBold(true);
    tab_widget->tabBar()->setFont(tab_font);

    addons_tab = new ConfigurePerGameAddons(system, this);
    addons_tab->SetTitleId(title_id);
    if (!game_path.isEmpty()) {
        const auto v_file = Core::GetGameFileFromPath(QtCommon::vfs, game_path.toStdString());
        if (v_file) {
            addons_tab->LoadFromFile(v_file);
        }
    }

    gamebanana_tab = new ConfigureGameBananaMods(system, title_id, game_name, addons_tab, this);

    connect(gamebanana_tab, &ConfigureGameBananaMods::ModInstalled, this, [this]() {
        if (!game_path.isEmpty()) {
            const auto v_file = Core::GetGameFileFromPath(QtCommon::vfs, game_path.toStdString());
            if (v_file) {
                addons_tab->LoadFromFile(v_file);
            }
        }
    });

    tab_widget->addTab(gamebanana_tab, tr("🍌 Онлайн каталог GameBanana (60 FPS / Графика / Патчи)"));
    tab_widget->addTab(addons_tab, tr("📁 Установленные моды и дополнения"));

    main_layout->addWidget(tab_widget, 1);

    // Bottom Action Buttons
    auto* button_layout = new QHBoxLayout();
    button_layout->setSpacing(10);

    auto* open_folder_btn = new QPushButton(tr("📁 Открыть папку модов"), this);
    open_folder_btn->setStyleSheet(QStringLiteral("font-weight: bold; padding: 6px 16px; border-radius: 5px;"));
    button_layout->addWidget(open_folder_btn);

    button_layout->addStretch(1);

    auto* cancel_btn = new QPushButton(tr("Отмена"), this);
    cancel_btn->setStyleSheet(QStringLiteral("padding: 6px 16px; border-radius: 5px;"));
    button_layout->addWidget(cancel_btn);

    auto* apply_btn = new QPushButton(tr("💾 Сохранить и применить"), this);
    apply_btn->setStyleSheet(QStringLiteral("background-color: #00f2fe; color: #000000; font-weight: bold; padding: 6px 20px; border-radius: 5px;"));
    button_layout->addWidget(apply_btn);

    main_layout->addLayout(button_layout);

    connect(open_folder_btn, &QPushButton::clicked, this, &ModManagerDialog::OnOpenModFolder);
    connect(cancel_btn, &QPushButton::clicked, this, &QDialog::reject);
    connect(apply_btn, &QPushButton::clicked, this, &ModManagerDialog::OnApplyAndClose);
}

ModManagerDialog::~ModManagerDialog() = default;

void ModManagerDialog::OnOpenModFolder() {
    const QString load_dir = QString::fromStdString(Common::FS::PathToUTF8String(
        Common::FS::GetEdenPath(Common::FS::EdenPath::LoadDir)));
    const QString tid_str = QStringLiteral("%1").arg(title_id, 16, 16, QLatin1Char('0')).toUpper();
    const QString game_mod_dir = QDir(load_dir).filePath(tid_str);
    QDir().mkpath(game_mod_dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(game_mod_dir));
}

void ModManagerDialog::OnApplyAndClose() {
    if (addons_tab) {
        addons_tab->ApplyConfiguration();
    }
    accept();
}
