// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <QTabWidget>
#include <memory>
#include "common/common_types.h"

namespace Core {
class System;
}

class ConfigurePerGameAddons;
class ConfigureGameBananaMods;

class ModManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModManagerDialog(QWidget* parent, Core::System& system, u64 title_id,
                              const QString& game_path, const QString& game_name);
    ~ModManagerDialog() override;

private slots:
    void OnOpenModFolder();
    void OnApplyAndClose();

private:
    Core::System& system;
    u64 title_id{0};
    QString game_path;
    QString game_name;

    QTabWidget* tab_widget{nullptr};
    ConfigureGameBananaMods* gamebanana_tab{nullptr};
    ConfigurePerGameAddons* addons_tab{nullptr};
};
