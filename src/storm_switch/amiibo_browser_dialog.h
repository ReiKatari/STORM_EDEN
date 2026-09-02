// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <vector>
#include <QDialog>
#include <QList>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QString>

namespace Core {
class System;
}

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTextEdit;
class QProgressBar;

struct AmiiboEntry {
    QString character;
    QString name;
    QString game_series;
    QString amiibo_series;
    QString type;
    QString head;
    QString tail;
    QString image_url;
    QString release_na;
    QString release_jp;
    QString release_eu;
    QList<QString> switch_games;
};

class AmiiboBrowserDialog : public QDialog {
    Q_OBJECT

public:
    explicit AmiiboBrowserDialog(QWidget* parent, Core::System& system);
    ~AmiiboBrowserDialog() override;

signals:
    void AmiiboSelectedForLoading(const QString& file_path);
    void AmiiboRemoveRequested();

private slots:
    void OnListLoaded();
    void OnSearchFilterChanged();
    void OnItemSelected(QListWidgetItem* current, QListWidgetItem* previous);
    void OnSaveAmiiboClicked();
    void OnLoadAmiiboClicked();
    void OnDisconnectAmiiboClicked();
    void OnOpenAmiiboFolderClicked();
    void OnRefreshClicked();

private:
    void SetupUi();
    void FetchAmiiboDatabase();
    void ParseDatabaseJson(const QByteArray& json_data);
    void PopulateSeriesFilter();
    void ApplyFilters();
    void DisplayAmiiboDetails(const AmiiboEntry& entry);
    void FetchImage(const QString& image_url, QLabel* target_label);
    QString GenerateAndSaveAmiiboBin(const AmiiboEntry& entry);

    Core::System& m_system;
    QNetworkAccessManager* m_network_mgr{nullptr};
    std::vector<AmiiboEntry> m_all_amiibos;
    std::vector<int> m_filtered_indices;
    QMap<QString, QPixmap> m_image_cache;

    // UI elements
    QLineEdit* m_search_edit{nullptr};
    QComboBox* m_series_combo{nullptr};
    QComboBox* m_type_combo{nullptr};
    QListWidget* m_amiibo_list{nullptr};
    QLabel* m_status_label{nullptr};
    QProgressBar* m_progress_bar{nullptr};

    // Inspector
    QLabel* m_image_label{nullptr};
    QLabel* m_name_label{nullptr};
    QLabel* m_series_label{nullptr};
    QLabel* m_game_series_label{nullptr};
    QLabel* m_type_label{nullptr};
    QLabel* m_id_label{nullptr};
    QLabel* m_status_badge{nullptr};
    QTextEdit* m_games_text{nullptr};
    QPushButton* m_save_btn{nullptr};
    QPushButton* m_load_btn{nullptr};
    QPushButton* m_disconnect_btn{nullptr};
    QPushButton* m_open_folder_btn{nullptr};
    QPushButton* m_refresh_btn{nullptr};
};
