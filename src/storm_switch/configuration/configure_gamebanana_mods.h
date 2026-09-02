// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <vector>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QWidget>
#include "common/common_types.h"

namespace Core {
class System;
}

class ConfigurePerGameAddons;

struct GameBananaFile {
    int id{0};
    QString name;
    QString url;
    qint64 size{0};
    QString description;
};

struct GameBananaModItem {
    int id{0};
    QString name;
    QString submitter;
    int downloads{0};
    int likes{0};
    int views{0};
    QString date;
    QString category;
    QString preview_url;
};

class ConfigureGameBananaMods : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureGameBananaMods(Core::System& system_, u64 title_id_, const QString& game_name_,
                                     ConfigurePerGameAddons* addons_tab_, QWidget* parent = nullptr);
    ~ConfigureGameBananaMods() override;

    void SetGameInfo(u64 title_id_, const QString& game_name_);

signals:
    void ModInstalled();

private slots:
    void OnSearchClicked();
    void OnSortChanged(int index);
    void OnModSelected(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void OnInstallClicked();
    void OnDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void OnPrevPage();
    void OnNextPage();
    void OnFirstPage();

private:
    void SearchMods(const QString& query = {}, int page = 1);
    void LoadModDetails(int mod_id);
    void DownloadAndInstallMod(const GameBananaFile& file_info, const QString& mod_name);
    void PopulateModTree();
    void ApplySorting();

    u64 title_id{0};
    QString game_name;
    int gamebanana_game_id{0};
    int current_page{1};

    Core::System& system;
    ConfigurePerGameAddons* addons_tab{nullptr};
    QNetworkAccessManager* network_manager{nullptr};
    QNetworkReply* current_reply{nullptr};
    QNetworkReply* download_reply{nullptr};

    // UI
    QLineEdit* search_input{nullptr};
    QPushButton* search_btn{nullptr};
    QComboBox* sort_combo{nullptr};
    QPushButton* refresh_btn{nullptr};
    QPushButton* first_page_btn{nullptr};
    QPushButton* prev_page_btn{nullptr};
    QPushButton* next_page_btn{nullptr};
    QLabel* page_label{nullptr};
    QTreeWidget* mod_tree{nullptr};
    QWidget* details_widget{nullptr};
    QTextBrowser* detail_browser{nullptr};
    QComboBox* files_combo{nullptr};
    QPushButton* install_btn{nullptr};
    QPushButton* open_folder_btn{nullptr};
    QProgressBar* progress_bar{nullptr};
    QLabel* status_label{nullptr};
    QLabel* game_header_label{nullptr};

    std::vector<GameBananaModItem> current_mods;
    std::vector<GameBananaFile> current_files;
    int selected_mod_id{0};
    QString selected_mod_name;
};
