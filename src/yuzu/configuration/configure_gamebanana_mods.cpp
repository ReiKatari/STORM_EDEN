// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <fstream>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "core/core.h"
#include "frontend_common/mod_manager.h"
#include "qt_common/util/mod.h"
#include "yuzu/configuration/configure_gamebanana_mods.h"
#include "yuzu/configuration/configure_per_game_addons.h"

ConfigureGameBananaMods::ConfigureGameBananaMods(Core::System& system_, u64 title_id_,
                                                 const QString& game_name_,
                                                 ConfigurePerGameAddons* addons_tab_,
                                                 QWidget* parent)
    : QWidget(parent), title_id{title_id_}, game_name{game_name_}, system{system_},
      addons_tab{addons_tab_} {
    network_manager = new QNetworkAccessManager(this);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    // 1. Header Banner
    game_header_label = new QLabel(this);
    game_header_label->setStyleSheet(QStringLiteral(
        "background-color: #1e293b; color: #f8fafc; padding: 8px 12px; border-radius: 6px; font-weight: bold; font-size: 13px;"));
    main_layout->addWidget(game_header_label);

    // 2. Search & Controls Bar
    auto* controls_layout = new QHBoxLayout;
    controls_layout->setSpacing(6);

    search_input = new QLineEdit(this);
    search_input->setPlaceholderText(tr("🔍 Поиск модов на GameBanana..."));
    search_input->setClearButtonEnabled(true);
    controls_layout->addWidget(search_input, 1);

    search_btn = new QPushButton(tr("Найти"), this);
    search_btn->setStyleSheet(QStringLiteral("font-weight: bold; padding: 4px 12px;"));
    controls_layout->addWidget(search_btn);

    sort_combo = new QComboBox(this);
    sort_combo->addItem(tr("По популярности"), QStringLiteral("Generic_MostDownloaded"));
    sort_combo->addItem(tr("По дате добавления"), QStringLiteral("Generic_Latest"));
    sort_combo->addItem(tr("По количеству лайков"), QStringLiteral("Generic_MostLiked"));
    sort_combo->addItem(tr("По просмотрам"), QStringLiteral("Generic_MostViewed"));
    controls_layout->addWidget(sort_combo);

    refresh_btn = new QPushButton(tr("🔄 Обновить"), this);
    controls_layout->addWidget(refresh_btn);

    main_layout->addLayout(controls_layout);

    // 3. Main Splitter (Left: Mod List, Right: Mod Details & Install)
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Left List
    mod_tree = new QTreeWidget(this);
    mod_tree->setHeaderLabels({tr("НАЗВАНИЕ МОДА"), tr("АВТОР"), tr("СКАЧИВАНИЙ"), tr("ЛАЙКОВ"), tr("КАТЕГОРИЯ")});
    mod_tree->setAlternatingRowColors(true);
    mod_tree->setRootIsDecorated(false);
    for (int i = 0; i < 5; ++i) {
        mod_tree->headerItem()->setTextAlignment(i, Qt::AlignCenter);
    }
    mod_tree->header()->setStretchLastSection(false);
    mod_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    mod_tree->header()->resizeSection(0, 320);
    mod_tree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    mod_tree->header()->resizeSection(1, 160);
    mod_tree->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    mod_tree->header()->resizeSection(2, 150);
    mod_tree->header()->setSectionResizeMode(3, QHeaderView::Interactive);
    mod_tree->header()->resizeSection(3, 150);
    mod_tree->header()->setSectionResizeMode(4, QHeaderView::Stretch);
    mod_tree->header()->resizeSection(4, 160);
    splitter->addWidget(mod_tree);

    // Right Details Pane
    details_widget = new QWidget(this);
    auto* details_layout = new QVBoxLayout(details_widget);
    details_layout->setContentsMargins(0, 0, 0, 0);
    details_layout->setSpacing(6);

    auto* details_top_bar = new QHBoxLayout;
    auto* details_header_label = new QLabel(tr("📄 Описание и установка мода"), this);
    details_header_label->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 10pt; color: #38bdf8;"));
    auto* close_details_btn = new QPushButton(tr("✖ Скрыть описание"), this);
    close_details_btn->setStyleSheet(QStringLiteral("padding: 4px 12px; font-weight: bold; background-color: #334155; color: #f8fafc; border-radius: 4px;"));
    connect(close_details_btn, &QPushButton::clicked, [this]() {
        if (details_widget) {
            details_widget->setVisible(false);
        }
        if (mod_tree) {
            mod_tree->clearSelection();
        }
    });
    details_top_bar->addWidget(details_header_label, 1);
    details_top_bar->addWidget(close_details_btn);
    details_layout->addLayout(details_top_bar);

    detail_browser = new QTextBrowser(this);
    detail_browser->setOpenExternalLinks(true);
    detail_browser->setPlaceholderText(tr("Выберите мод из списка слева для просмотра описания и файлов для установки."));
    details_layout->addWidget(detail_browser, 1);

    auto* files_label = new QLabel(tr("📦 Доступные файлы мода:"), this);
    files_label->setStyleSheet(QStringLiteral("font-weight: bold;"));
    details_layout->addWidget(files_label);

    files_combo = new QComboBox(this);
    details_layout->addWidget(files_combo);

    progress_bar = new QProgressBar(this);
    progress_bar->setRange(0, 100);
    progress_bar->setValue(0);
    progress_bar->setTextVisible(true);
    progress_bar->setVisible(false);
    details_layout->addWidget(progress_bar);

    status_label = new QLabel(this);
    status_label->setWordWrap(true);
    status_label->setStyleSheet(QStringLiteral("color: #00e676; font-weight: bold;"));
    details_layout->addWidget(status_label);

    auto* btn_layout = new QHBoxLayout;
    install_btn = new QPushButton(tr("📥 Скачать и установить мод"), this);
    install_btn->setStyleSheet(QStringLiteral("background-color: #2563eb; color: white; font-weight: bold; padding: 6px 16px; border-radius: 4px;"));
    install_btn->setEnabled(false);
    btn_layout->addWidget(install_btn, 1);

    open_folder_btn = new QPushButton(tr("📁 Папка модов игры"), this);
    btn_layout->addWidget(open_folder_btn);

    details_layout->addLayout(btn_layout);

    splitter->addWidget(details_widget);
    details_widget->setVisible(false);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    main_layout->addWidget(splitter, 1);

    // Connect signals
    connect(search_btn, &QPushButton::clicked, this, &ConfigureGameBananaMods::OnSearchClicked);
    connect(search_input, &QLineEdit::returnPressed, this, &ConfigureGameBananaMods::OnSearchClicked);
    connect(refresh_btn, &QPushButton::clicked, this, &ConfigureGameBananaMods::OnSearchClicked);
    connect(sort_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConfigureGameBananaMods::OnSortChanged);
    connect(mod_tree, &QTreeWidget::currentItemChanged, this, &ConfigureGameBananaMods::OnModSelected);
    connect(mod_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        OnModSelected(item, nullptr);
    });
    connect(install_btn, &QPushButton::clicked, this, &ConfigureGameBananaMods::OnInstallClicked);
    connect(open_folder_btn, &QPushButton::clicked, this, [this]() {
        const auto load_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::LoadDir) / fmt::format("{:016X}", title_id);
        std::filesystem::create_directories(load_dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(Common::FS::PathToUTF8String(load_dir))));
    });

    SetGameInfo(title_id, game_name);
}

ConfigureGameBananaMods::~ConfigureGameBananaMods() {
    if (current_reply) {
        current_reply->abort();
    }
    if (download_reply) {
        download_reply->abort();
    }
}

void ConfigureGameBananaMods::SetGameInfo(u64 title_id_, const QString& game_name_) {
    this->title_id = title_id_;
    this->game_name = game_name_;

    // Clean game title for search
    QString clean_title = game_name;
    clean_title.remove(QRegularExpression(QStringLiteral("\\[.*?\\]")));
    clean_title.remove(QRegularExpression(QStringLiteral("\\(.*?\\)")));
    clean_title = clean_title.trimmed();

    game_header_label->setText(tr("🎮 Игра: %1  |  Title ID: %2").arg(clean_title, QStringLiteral("%1").arg(title_id, 16, 16, QLatin1Char('0')).toUpper()));

    SearchMods();
}

void ConfigureGameBananaMods::OnSearchClicked() {
    SearchMods(search_input->text().trimmed());
}

void ConfigureGameBananaMods::OnSortChanged(int) {
    SearchMods(search_input->text().trimmed());
}

void ConfigureGameBananaMods::SearchMods(const QString& query) {
    if (current_reply) {
        current_reply->abort();
        current_reply = nullptr;
    }

    mod_tree->clear();
    current_mods.clear();
    detail_browser->clear();
    files_combo->clear();
    install_btn->setEnabled(false);
    status_label->setText(tr("Поиск модов на GameBanana..."));

    QString clean_title = game_name;
    clean_title.remove(QRegularExpression(QStringLiteral("\\[.*?\\]")));
    clean_title.remove(QRegularExpression(QStringLiteral("\\(.*?\\)")));
    clean_title = clean_title.trimmed();

    QString search_term = query.isEmpty() ? clean_title : QStringLiteral("%1 %2").arg(clean_title, query);
    QUrl search_url(QStringLiteral("https://gamebanana.com/apiv11/Util/Search/Results"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("_sSearchString"), search_term);
    q.addQueryItem(QStringLiteral("_sModelName"), QStringLiteral("Mod"));
    q.addQueryItem(QStringLiteral("_nPage"), QStringLiteral("1"));
    q.addQueryItem(QStringLiteral("_nPerpage"), QStringLiteral("40"));
    search_url.setQuery(q);

    QNetworkRequest request(search_url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("STORM-EDEN-Client/3.2.4"));

    current_reply = network_manager->get(request);
    connect(current_reply, &QNetworkReply::finished, this, [this]() {
        if (!current_reply) return;
        if (current_reply->error() != QNetworkReply::NoError) {
            status_label->setText(tr("Ошибка подключения к GameBanana: %1").arg(current_reply->errorString()));
            current_reply->deleteLater();
            current_reply = nullptr;
            return;
        }

        const auto data = current_reply->readAll();
        current_reply->deleteLater();
        current_reply = nullptr;

        const auto doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            status_label->setText(tr("Не удалось прочитать ответ GameBanana."));
            return;
        }

        const auto root_obj = doc.object();
        const auto records = root_obj.value(QStringLiteral("_aRecords")).toArray();

        for (const auto& val : records) {
            const auto obj = val.toObject();
            GameBananaModItem item;
            item.id = obj.value(QStringLiteral("_idRow")).toInt();
            item.name = obj.value(QStringLiteral("_sName")).toString();
            if (obj.contains(QStringLiteral("_aSubmitter"))) {
                item.submitter = obj.value(QStringLiteral("_aSubmitter")).toObject().value(QStringLiteral("_sName")).toString();
            }
            if (obj.contains(QStringLiteral("_aRootCategory"))) {
                item.category = obj.value(QStringLiteral("_aRootCategory")).toObject().value(QStringLiteral("_sName")).toString();
            }
            item.downloads = obj.value(QStringLiteral("_nDownloadCount")).toInt();
            item.likes = obj.value(QStringLiteral("_nLikeCount")).toInt();
            item.views = obj.value(QStringLiteral("_nViewCount")).toInt();

            qint64 ts = obj.value(QStringLiteral("_tsDateAdded")).toVariant().toLongLong();
            if (ts > 0) {
                item.date = QDateTime::fromSecsSinceEpoch(ts).toString(QStringLiteral("dd.MM.yyyy"));
            }

            current_mods.push_back(item);

            auto* tree_item = new QTreeWidgetItem(mod_tree);
            tree_item->setText(0, item.name.toUpper());
            tree_item->setText(1, item.submitter);
            tree_item->setText(2, QString::number(item.downloads));
            tree_item->setText(3, QString::number(item.likes));
            tree_item->setText(4, item.category);
            tree_item->setData(0, Qt::UserRole, item.id);
            tree_item->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
            for (int i = 1; i < 5; ++i) {
                tree_item->setTextAlignment(i, Qt::AlignCenter);
            }
        }

        if (current_mods.empty()) {
            status_label->setText(tr("Моды для игры не найдены. Попробуйте изменить поисковый запрос."));
            if (details_widget) {
                details_widget->setVisible(false);
            }
        } else {
            status_label->setText(tr("Найдено модов: %1. Нажмите на строку мода для просмотра описания и файлов.").arg(current_mods.size()));
        }
    });
}

void ConfigureGameBananaMods::OnModSelected(QTreeWidgetItem* current, QTreeWidgetItem*) {
    if (!current) return;
    int mod_id = current->data(0, Qt::UserRole).toInt();
    if (mod_id <= 0) return;

    if (details_widget) {
        details_widget->setVisible(true);
    }

    selected_mod_id = mod_id;
    selected_mod_name = current->text(0);
    LoadModDetails(mod_id);
}

void ConfigureGameBananaMods::LoadModDetails(int mod_id) {
    if (current_reply) {
        current_reply->abort();
        current_reply = nullptr;
    }

    files_combo->clear();
    current_files.clear();
    install_btn->setEnabled(false);
    detail_browser->setHtml(tr("<h3>Загрузка информации о моде...</h3>"));

    QUrl details_url(QStringLiteral("https://gamebanana.com/apiv11/Mod/%1/ProfilePage").arg(mod_id));
    QNetworkRequest request(details_url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("STORM-EDEN-Client/3.2.4"));

    current_reply = network_manager->get(request);
    connect(current_reply, &QNetworkReply::finished, this, [this]() {
        if (!current_reply) return;
        if (current_reply->error() != QNetworkReply::NoError) {
            detail_browser->setHtml(tr("<b>Ошибка загрузки деталей:</b> %1").arg(current_reply->errorString()));
            current_reply->deleteLater();
            current_reply = nullptr;
            return;
        }

        const auto data = current_reply->readAll();
        current_reply->deleteLater();
        current_reply = nullptr;

        const auto doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) return;

        const auto obj = doc.object();
        const QString name = obj.value(QStringLiteral("_sName")).toString();
        const QString desc = obj.value(QStringLiteral("_sDescription")).toString();
        const QString text = obj.value(QStringLiteral("_sText")).toString();
        const QString author = obj.value(QStringLiteral("_aSubmitter")).toObject().value(QStringLiteral("_sName")).toString();

        QString html = QStringLiteral("<h2>%1</h2><p><b>Автор:</b> %2</p><p><i>%3</i></p><hr/><div>%4</div>")
                           .arg(name, author, desc, text.isEmpty() ? desc : text);
        detail_browser->setHtml(html);

        // Parse Files
        const auto files_arr = obj.value(QStringLiteral("_aFiles")).toArray();
        for (const auto& fval : files_arr) {
            const auto fobj = fval.toObject();
            GameBananaFile file_info;
            file_info.id = fobj.value(QStringLiteral("_idRow")).toInt();
            file_info.name = fobj.value(QStringLiteral("_sFile")).toString();
            file_info.url = fobj.value(QStringLiteral("_sDownloadUrl")).toString();
            file_info.size = fobj.value(QStringLiteral("_nFilesize")).toVariant().toLongLong();
            file_info.description = fobj.value(QStringLiteral("_sDescription")).toString();

            current_files.push_back(file_info);

            const double size_mb = static_cast<double>(file_info.size) / (1024.0 * 1024.0);
            QString item_label = QStringLiteral("%1 (%2 MB)").arg(file_info.name, QString::number(size_mb, 'f', 1));
            if (!file_info.description.isEmpty()) {
                item_label += QStringLiteral(" — %1").arg(file_info.description);
            }
            files_combo->addItem(item_label, file_info.id);
        }

        install_btn->setEnabled(!current_files.empty());
    });
}

void ConfigureGameBananaMods::OnInstallClicked() {
    if (files_combo->currentIndex() < 0 || files_combo->currentIndex() >= static_cast<int>(current_files.size())) {
        return;
    }

    const auto& selected_file = current_files[files_combo->currentIndex()];
    DownloadAndInstallMod(selected_file, selected_mod_name);
}

void ConfigureGameBananaMods::OnDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        progress_bar->setValue(static_cast<int>((bytesReceived * 100) / bytesTotal));
    }
}

void ConfigureGameBananaMods::DownloadAndInstallMod(const GameBananaFile& file_info, const QString& mod_name) {
    if (file_info.url.isEmpty()) {
        QMessageBox::warning(this, tr("Ошибка"), tr("Ссылка для скачивания файла отсутствует."));
        return;
    }

    install_btn->setEnabled(false);
    progress_bar->setVisible(true);
    progress_bar->setValue(0);
    status_label->setStyleSheet(QStringLiteral("color: #38bdf8; font-weight: bold;"));
    status_label->setText(tr("Скачивание мода '%1'...").arg(file_info.name));

    QNetworkRequest request(QUrl(file_info.url));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("STORM-EDEN-Client/3.2.4"));

    download_reply = network_manager->get(request);
    connect(download_reply, &QNetworkReply::downloadProgress, this, &ConfigureGameBananaMods::OnDownloadProgress);
    connect(download_reply, &QNetworkReply::finished, this, [this, file_info, mod_name]() {
        if (!download_reply) return;
        progress_bar->setVisible(false);
        install_btn->setEnabled(true);

        if (download_reply->error() != QNetworkReply::NoError) {
            status_label->setStyleSheet(QStringLiteral("color: #ff5252; font-weight: bold;"));
            status_label->setText(tr("Ошибка скачивания: %1").arg(download_reply->errorString()));
            download_reply->deleteLater();
            download_reply = nullptr;
            return;
        }

        const auto file_data = download_reply->readAll();
        download_reply->deleteLater();
        download_reply = nullptr;

        // Save to temporary file
        const auto temp_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "temp_mods";
        std::filesystem::create_directories(temp_dir);
        const auto temp_file_path = temp_dir / file_info.name.toStdString();

        {
            std::ofstream out(temp_file_path, std::ios::binary);
            out.write(file_data.data(), file_data.size());
        }

        status_label->setText(tr("Распаковка и установка мода..."));

        // Extract and install
        const QString extracted = QtCommon::Mod::ExtractMod(QString::fromStdString(Common::FS::PathToUTF8String(temp_file_path)));
        QString clean_mod_name = mod_name;
        if (clean_mod_name.isEmpty()) {
            clean_mod_name = QFileInfo(file_info.name).baseName();
        }

        if (!extracted.isEmpty()) {
            const auto mod_folders = QtCommon::Mod::GetModFolders(extracted, clean_mod_name);
            for (const auto& mod_path : mod_folders) {
                FrontendCommon::InstallMod(mod_path.toStdString(), title_id, true);
            }
        } else {
            FrontendCommon::InstallMod(Common::FS::PathToUTF8String(temp_file_path), title_id, true);
        }

        status_label->setStyleSheet(QStringLiteral("color: #00e676; font-weight: bold;"));
        status_label->setText(tr("✅ Мод '%1' успешно установлен и активирован!").arg(clean_mod_name));

        QMessageBox::information(this, tr("Установка завершена"),
                                 tr("Мод «%1» успешно скачан, внедрен в игру и активирован в параметрах!").arg(clean_mod_name));

        emit ModInstalled();
    });
}
