// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <memory>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "eden/util/hardware_analyzer.h"

namespace Ui {
class ConfigureAutoOptimizer;
}

class ConfigureAutoOptimizer : public QDialog {
    Q_OBJECT

public:
    explicit ConfigureAutoOptimizer(QWidget* parent, const std::vector<u64>& title_ids, const std::vector<std::string>& game_names);
    ~ConfigureAutoOptimizer() override;

private:
    void StartOptimization();
    QString ApplySettingsForGame(u64 title_id, const std::string& game_name);
    void SetupUI();

    std::unique_ptr<Ui::ConfigureAutoOptimizer> ui;
    std::vector<u64> title_ids;
    std::vector<std::string> game_names;
    Util::HardwareInfo hw_info;
};
