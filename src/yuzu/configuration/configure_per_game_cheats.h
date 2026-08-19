// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include <string>
#include <memory>
#include <array>

#include "common/common_types.h"
#include "core/memory/dmnt_cheat_types.h"

namespace Core {
class System;
}

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QPushButton;
class QLabel;

class ConfigurePerGameCheats : public QWidget {
    Q_OBJECT

public:
    explicit ConfigurePerGameCheats(Core::System& system, u64 title_id, const QString& file_name, QWidget* parent = nullptr);
    ~ConfigurePerGameCheats() override;

    void ApplyConfiguration();
    void LoadCheats();

signals:
    void CheatsChanged();

private slots:
    void OnItemChanged(QTreeWidgetItem* item, int column);
    void OnDownloadOnlineCheats();
    void OnAddCustomCheat();
    void OnOpenCheatsFolder();
    void OnSelectAll();
    void OnDeselectAll();
    void OnFilterTextChanged(const QString& text);

private:
    void RetranslateUI();
    void PopulateCheatTree();
    void SaveCheatsToFile();
    QString GetCheatsFilePath() const;
    QString GetCheatsDirectoryPath() const;

    Core::System& system;
    u64 title_id;
    QString file_name;
    std::array<u8, 0x20> build_id{};
    QString build_id_str;

    struct CheatItem {
        QString name;
        QString code;
        QString build_id;
        bool enabled{false};
        bool is_custom{false};

        CheatItem() = default;
        CheatItem(QString name_, QString code_, QString build_id_ = QString(), bool enabled_ = false, bool is_custom_ = false)
            : name(std::move(name_)), code(std::move(code_)), build_id(std::move(build_id_)), enabled(enabled_), is_custom(is_custom_) {}
    };
    QVector<CheatItem> cheat_items;

    QTreeWidget* tree_widget{nullptr};
    QLineEdit* search_field{nullptr};
    QLabel* info_label{nullptr};
    QLabel* status_label{nullptr};
    QPushButton* download_button{nullptr};
    QPushButton* add_button{nullptr};
    QPushButton* open_folder_button{nullptr};
    QPushButton* select_all_button{nullptr};
    QPushButton* deselect_all_button{nullptr};
};
