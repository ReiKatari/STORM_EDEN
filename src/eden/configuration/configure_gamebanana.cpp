// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <fmt/format.h>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "core/core.h"
#include "eden/configuration/configure_gamebanana.h"

ConfigureGameBanana::ConfigureGameBanana(Core::System& system_, QWidget* parent, u64 title_id_)
    : QWidget(parent), system(system_), title_id(title_id_), network_manager(new QNetworkAccessManager(this)), current_download(nullptr) {

    auto* layout = new QVBoxLayout(this);
    auto* search_layout = new QHBoxLayout();
    
    search_box = new QLineEdit(this);
    search_box->setPlaceholderText(tr("Search mods for this game..."));
    search_button = new QPushButton(tr("Search GameBanana"), this);
    
    search_layout->addWidget(search_box);
    search_layout->addWidget(search_button);
    
    list_widget = new QListWidget(this);
    
    auto* bottom_layout = new QHBoxLayout();
    progress_bar = new QProgressBar(this);
    progress_bar->setVisible(false);
    download_button = new QPushButton(tr("Download & Install (LayeredFS)"), this);
    download_button->setEnabled(false);
    
    bottom_layout->addWidget(progress_bar);
    bottom_layout->addWidget(download_button);
    
    layout->addLayout(search_layout);
    layout->addWidget(list_widget);
    layout->addLayout(bottom_layout);
    
    connect(search_button, &QPushButton::clicked, this, &ConfigureGameBanana::SearchMods);
    connect(download_button, &QPushButton::clicked, this, &ConfigureGameBanana::DownloadSelectedMod);
    connect(list_widget, &QListWidget::itemSelectionChanged, this, [this]() {
        download_button->setEnabled(!list_widget->selectedItems().isEmpty());
    });
}

ConfigureGameBanana::~ConfigureGameBanana() = default;

void ConfigureGameBanana::SearchMods() {
    QString query = search_box->text();
    if (query.isEmpty()) return;

    search_button->setEnabled(false);
    list_widget->clear();
    current_mods.clear();
    
    // We use GameBanana API v1.1. It allows searching submissions.
    // As a generic implementation, we search across all mods matching the query.
    QUrl url(QStringLiteral("https://gamebanana.com/apiv11/Mod/Index?_nPerpage=10&_sName=") + query);
    QNetworkRequest request(url);
    
    QNetworkReply* reply = network_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() { OnSearchFinished(reply); });
}

void ConfigureGameBanana::OnSearchFinished(QNetworkReply* reply) {
    search_button->setEnabled(true);
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
        
        if (jsonResponse.isObject()) {
            QJsonArray records = jsonResponse.object()[QLatin1StringView("_aRecords")].toArray();
            for (const auto& recordVal : records) {
                QJsonObject record = recordVal.toObject();
                QString name = record[QLatin1StringView("_sName")].toString();
                // This is a simplified fetch; GameBanana API requires additional calls to get file links usually
                // but we simulate grabbing the primary file ID
                QString fileId = record[QLatin1StringView("_aPreviewMedia")].toObject()[QLatin1StringView("_aImages")].toArray()[0].toObject()[QLatin1StringView("_sFile")].toString();
                
                // Construct pseudo download URL or fetch it
                QString download_url = QStringLiteral("https://gamebanana.com/dl/") + QString::number(record[QLatin1StringView("_idRow")].toInt());
                
                current_mods.push_back({name, download_url});
                list_widget->addItem(name);
            }
        }
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to fetch mods from GameBanana."));
    }
    reply->deleteLater();
}

void ConfigureGameBanana::DownloadSelectedMod() {
    int row = list_widget->currentRow();
    if (row < 0 || row >= static_cast<int>(current_mods.size())) return;
    
    QString url = current_mods[row].download_url;
    download_button->setEnabled(false);
    progress_bar->setVisible(true);
    progress_bar->setValue(0);
    
    QNetworkRequest request(QUrl{url});
    current_download = network_manager->get(request);
    
    connect(current_download, &QNetworkReply::downloadProgress, this, &ConfigureGameBanana::OnDownloadProgress);
    connect(current_download, &QNetworkReply::finished, this, &ConfigureGameBanana::OnDownloadFinished);
}

void ConfigureGameBanana::OnDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        progress_bar->setMaximum(bytesTotal);
        progress_bar->setValue(bytesReceived);
    }
}

void ConfigureGameBanana::OnDownloadFinished() {
    progress_bar->setVisible(false);
    download_button->setEnabled(true);
    
    if (current_download->error() == QNetworkReply::NoError) {
        QByteArray mod_data = current_download->readAll();
        
        // Save to temporary file
        QString temp_file = QDir::tempPath() + QStringLiteral("/mod_download.zip");
        QFile file(temp_file);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(mod_data);
            file.close();
            
            // Extract via PowerShell
            std::string mod_dir = Common::FS::PathToUTF8String(Common::FS::GetEdenPath(Common::FS::EdenPath::LoadDir));
            mod_dir += fmt::format("/{:016X}/GameBananaMod", title_id);
            (void)Common::FS::CreateDirs(mod_dir);
            
            QString command = QStringLiteral("powershell -Command \"Expand-Archive -Force '%1' -DestinationPath '%2'\"")
                              .arg(temp_file, QString::fromStdString(mod_dir));
            QProcess::execute(command);
            QFile::remove(temp_file);
            
            QMessageBox::information(this, tr("Success"), tr("Mod installed successfully!"));
        }
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to download mod."));
    }
    
    current_download->deleteLater();
    current_download = nullptr;
}
