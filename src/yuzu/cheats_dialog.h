// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDialog>
#include <memory>
#include "common/common_types.h"

namespace Core {
class System;
}

class ConfigurePerGameCheats;

class CheatsDialog : public QDialog {
    Q_OBJECT

public:
    explicit CheatsDialog(QWidget* parent, Core::System& system, u64 title_id, const QString& file_name);
    ~CheatsDialog() override;

private:
    ConfigurePerGameCheats* cheats_widget{nullptr};
};
