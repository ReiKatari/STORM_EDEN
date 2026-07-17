// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace Core {
class System;
}

class ConfigureGameBanana : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureGameBanana(Core::System& system_, QWidget* parent = nullptr, u64 title_id_ = 0);
    ~ConfigureGameBanana() override;

private:
    void SearchMods();
    void FetchGameMods(const QString& query);
    void ExecuteModSearch(const QString& query, const std::vector<int>& mod_ids);
    void OnSearchFinished(QNetworkReply* reply, const QString& query);
    void DownloadSelectedMod();
    void OnDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void OnDownloadFinished();
    QString GetGameName() const;

    Core::System& system;
    u64 title_id;
    int gamebanana_game_id = 0;

    QListWidget* list_widget;
    QLineEdit* search_box;
    QPushButton* search_button;
    QPushButton* download_button;
    QProgressBar* progress_bar;
    QNetworkAccessManager* network_manager;
    QNetworkReply* current_download;
    
    struct ModInfo {
        QString name;
        QString download_url;
    };
    std::vector<ModInfo> current_mods;
};
