// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <vector>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>
#include <QPixmap>
#include <QString>
#include <QWidget>
#include "common/common_types.h"
#include "yuzu/amiibo_browser_dialog.h"

namespace Core {
class System;
}

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextEdit;
class QProgressBar;

class ConfigurePerGameAmiibo : public QWidget {
    Q_OBJECT

public:
    explicit ConfigurePerGameAmiibo(Core::System& system, u64 title_id, const QString& file_name, QWidget* parent = nullptr);
    ~ConfigurePerGameAmiibo() override;

    void ApplyConfiguration();

private slots:
    void OnSearchFilterChanged();
    void OnItemSelected(QListWidgetItem* current, QListWidgetItem* previous);
    void OnItemCheckChanged(QListWidgetItem* item);
    void OnRefreshClicked();
    void OnOpenAmiiboFolderClicked();

private:
    void SetupUi();
    void LoadAmiiboDatabase();
    void ParseDatabaseJson(const QByteArray& json_data);
    void PopulateSeriesFilter();
    void ApplyFilters();
    void DisplayAmiiboDetails(const AmiiboEntry& entry);
    void FetchImage(const QString& image_url, QLabel* target_label);
    QString GetGameAmiiboFolder() const;
    QString GenerateAndSaveAmiiboBin(const AmiiboEntry& entry);
    bool IsAmiiboInstalled(const AmiiboEntry& entry) const;

    Core::System& m_system;
    u64 m_title_id{0};
    QString m_file_name;
    QNetworkAccessManager* m_network_mgr{nullptr};
    std::vector<AmiiboEntry> m_all_amiibos;
    std::vector<int> m_filtered_indices;
    QMap<QString, QPixmap> m_image_cache;
    bool m_is_updating_ui{false};

    // UI
    QLineEdit* m_search_edit{nullptr};
    QComboBox* m_series_combo{nullptr};
    QComboBox* m_filter_scope_combo{nullptr};
    QListWidget* m_amiibo_list{nullptr};
    QProgressBar* m_progress_bar{nullptr};

    // Inspector
    QLabel* m_image_label{nullptr};
    QLabel* m_name_label{nullptr};
    QLabel* m_series_label{nullptr};
    QLabel* m_type_label{nullptr};
    QLabel* m_status_badge{nullptr};
    QTextEdit* m_games_text{nullptr};
    QPushButton* m_install_btn{nullptr};
    QPushButton* m_open_folder_btn{nullptr};
    QPushButton* m_refresh_btn{nullptr};
};
