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
#include <QUrlQuery>
#include <QTextEdit>
#include <QPlainTextEdit>
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
    search_button->setMinimumWidth(180);
    search_button->setToolTip(tr("Search GameBanana"));
    
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
    QString query = search_box->text().trimmed();
    if (query.length() < 2) {
        QMessageBox::warning(this, tr("Warning"), tr("Please enter at least 2 characters to search."));
        return;
    }

    search_button->setEnabled(false);
    list_widget->clear();
    current_mods.clear();
    
    if (gamebanana_game_id == 0) {
        QString game_name = GetGameName();
        // Remove trailing details like "(Demo)" or version numbers if any
        game_name = game_name.split(QLatin1Char{'('})[0].trimmed();
        
        if (!game_name.isEmpty() && game_name != tr("Loading...")) {
            QUrl url(QStringLiteral("https://api.gamebanana.com/Core/List/Like"));
            QUrlQuery query_params;
            query_params.addQueryItem(QStringLiteral("itemtype"), QStringLiteral("Game"));
            query_params.addQueryItem(QStringLiteral("field"), QStringLiteral("name"));
            query_params.addQueryItem(QStringLiteral("match"), game_name);
            query_params.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
            url.setQuery(query_params);
            
            QNetworkRequest request(url);
            QNetworkReply* reply = network_manager->get(request);
            connect(reply, &QNetworkReply::finished, this, [this, reply, query]() {
                if (reply->error() == QNetworkReply::NoError) {
                    QByteArray response = reply->readAll();
                    QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
                    if (jsonResponse.isArray()) {
                        QJsonArray arr = jsonResponse.array();
                        if (!arr.isEmpty()) {
                            QJsonObject obj = arr[0].toObject();
                            gamebanana_game_id = obj[QLatin1StringView("id")].toInt();
                        }
                    }
                }
                reply->deleteLater();
                if (gamebanana_game_id != 0) {
                    FetchGameMods(query);
                } else {
                    search_button->setEnabled(true);
                    QMessageBox::warning(this, tr("Warning"), tr("Could not find this game on GameBanana."));
                }
            });
            return;
        } else {
            search_button->setEnabled(true);
            return;
        }
    }
    
    FetchGameMods(query);
}

void ConfigureGameBanana::FetchGameMods(const QString& query) {
    QUrl url(QStringLiteral("https://api.gamebanana.com/Core/List/New"));
    QUrlQuery query_params;
    query_params.addQueryItem(QStringLiteral("itemtype"), QStringLiteral("Mod"));
    query_params.addQueryItem(QStringLiteral("gameid"), QString::number(gamebanana_game_id));
    query_params.addQueryItem(QStringLiteral("page"), QStringLiteral("1"));
    query_params.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(query_params);
    
    QNetworkRequest request(url);
    QNetworkReply* reply = network_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, query]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
            std::vector<int> mod_ids;
            if (jsonResponse.isArray()) {
                QJsonArray arr = jsonResponse.array();
                for (const auto& itemVal : arr) {
                    QJsonArray item = itemVal.toArray();
                    if (item.size() >= 2) {
                        mod_ids.push_back(item[1].toInt());
                    }
                }
            }
            reply->deleteLater();
            
            if (!mod_ids.empty()) {
                ExecuteModSearch(query, mod_ids);
            } else {
                search_button->setEnabled(true);
                QMessageBox::information(this, tr("Information"), tr("No mods found for this game on GameBanana."));
            }
        } else {
            reply->deleteLater();
            search_button->setEnabled(true);
            QMessageBox::warning(this, tr("Error"), tr("Failed to fetch game mods."));
        }
    });
}

void ConfigureGameBanana::ExecuteModSearch(const QString& query, const std::vector<int>& mod_ids) {
    QUrl url(QStringLiteral("https://api.gamebanana.com/Core/Item/Data"));
    QUrlQuery query_params;
    
    // Limit to first 30 mods to avoid exceeding URL length limits
    size_t count = std::min(mod_ids.size(), size_t{30});
    for (size_t i = 0; i < count; ++i) {
        query_params.addQueryItem(QStringLiteral("itemtype[]"), QStringLiteral("Mod"));
        query_params.addQueryItem(QStringLiteral("itemid[]"), QString::number(mod_ids[i]));
        query_params.addQueryItem(QStringLiteral("fields[]"), QStringLiteral("name,Url().sProfileUrl(),Files().aFiles()"));
    }
    query_params.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
    url.setQuery(query_params);
    
    QNetworkRequest request(url);
    QNetworkReply* reply = network_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, query]() { OnSearchFinished(reply, query); });
}

QString ConfigureGameBanana::GetGameName() const {
    auto* parent = parentWidget();
    while (parent != nullptr) {
        auto* display_name = parent->findChild<QWidget*>("display_name");
        if (display_name != nullptr) {
            if (auto* label = qobject_cast<QLabel*>(display_name)) {
                return label->text();
            }
            if (auto* text_edit = qobject_cast<QTextEdit*>(display_name)) {
                return text_edit->toPlainText();
            }
            if (auto* plain_text_edit = qobject_cast<QPlainTextEdit*>(display_name)) {
                return plain_text_edit->toPlainText();
            }
            if (auto* line_edit = qobject_cast<QLineEdit*>(display_name)) {
                return line_edit->text();
            }
        }
        parent = parent->parentWidget();
    }
    return QString{};
}

void ConfigureGameBanana::OnSearchFinished(QNetworkReply* reply, const QString& query) {
    search_button->setEnabled(true);
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
        
        if (jsonResponse.isArray()) {
            QJsonArray records = jsonResponse.array();
            for (const auto& recordVal : records) {
                QJsonArray fields = recordVal.toArray();
                if (fields.size() >= 3) {
                    QString name = fields[0].toString();
                    QString profile_url = fields[1].toString();
                    QJsonObject files_obj = fields[2].toObject();
                    
                    // Filter mod name locally with exact case-insensitive match
                    if (name.contains(query, Qt::CaseInsensitive)) {
                        QString download_url;
                        // Find first file in files_obj
                        for (auto it = files_obj.begin(); it != files_obj.end(); ++it) {
                            QJsonObject file_data = it.value().toObject();
                            download_url = file_data[QLatin1StringView("_sDownloadUrl")].toString();
                            if (!download_url.isEmpty()) {
                                break;
                            }
                        }
                        
                        if (!download_url.isEmpty()) {
                            current_mods.push_back({name, download_url});
                            list_widget->addItem(name);
                        }
                    }
                }
            }
        }
        
        if (current_mods.empty()) {
            list_widget->addItem(tr("No matching mods found."));
        }
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to fetch mod details from GameBanana."));
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
