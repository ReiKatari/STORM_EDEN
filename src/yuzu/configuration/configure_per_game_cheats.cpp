// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QTextEdit>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <algorithm>
#include <filesystem>
#include <fmt/format.h>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/hex_util.h"
#include "common/logging.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/patch_manager.h"
#include "core/loader/loader.h"
#include "core/memory/cheat_engine.h"
#include "yuzu/configuration/configure_per_game_cheats.h"

static bool IsBogusCheatText(const QString& content) {
    return content.contains(QStringLiteral("52800001")) &&
           content.contains(QStringLiteral("5280270F")) &&
           (content.contains(QStringLiteral("Infinite Health")) || content.contains(QStringLiteral("Бесконечное здоровье")));
}

ConfigurePerGameCheats::ConfigurePerGameCheats(Core::System& system_, u64 title_id_, const QString& file_name_, QWidget* parent)
    : QWidget(parent), system(system_), title_id(title_id_), file_name(file_name_) {
    
    if (system.IsPoweredOn() && system.GetApplicationProcessProgramID() == title_id) {
        build_id = system.GetApplicationProcessBuildID();
        build_id_str = QString::fromStdString(Common::HexToString(build_id)).left(16).toUpper();
    } else {
        // Find existing build_id files in cheats directory
        const auto dir_path = GetCheatsDirectoryPath();
        QDir cdir(dir_path);
        const auto files = cdir.entryList(QStringList() << QStringLiteral("*.txt"), QDir::Files);
        for (const auto& fname : files) {
            QString base = fname.left(fname.lastIndexOf(QLatin1Char('.'))).trimmed().toUpper();
            if (base.length() == 16 && !base.startsWith(QStringLiteral("CHEAT"))) {
                build_id_str = base;
                break;
            }
        }
        if (build_id_str.isEmpty()) {
            build_id_str = QString(QStringLiteral("%1")).arg(title_id, 16, 16, QLatin1Char('0')).toUpper();
        }
    }

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(10, 10, 10, 10);
    main_layout->setSpacing(8);

    // Header info panel
    info_label = new QLabel(this);
    const QString tid_str = QString(QStringLiteral("%1")).arg(title_id, 16, 16, QLatin1Char('0')).toUpper();
    const bool is_active_now = system.IsPoweredOn() && (system.GetApplicationProcessProgramID() == title_id);
    info_label->setText(tr("🎮 <b>ID приложения:</b> <span style='color:#00f2fe;'>%1</span> &nbsp;&nbsp;|&nbsp;&nbsp; ⚙️ <b>ID сборки (Build ID):</b> <span style='color:#c084fc;'>%2</span> &nbsp;&nbsp;|&nbsp;&nbsp; 💡 <b>Режим:</b> %3")
                            .arg(tid_str, build_id_str, is_active_now ? tr("<b style='color:#00f2fe;'>⚡ Активен в памяти (горячее применение)</b>") : tr("<span style='color:#94a3b8;'>⏸️ Офлайн (применится при запуске)</span>")));
    info_label->setStyleSheet(QStringLiteral("font-size: 9.5pt; color: #e2e8f0; padding: 8px 12px; background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 rgba(15, 23, 42, 0.9), stop:1 rgba(30, 41, 59, 0.8)); border-radius: 8px; border: 1px solid rgba(0, 242, 254, 0.3);"));
    main_layout->addWidget(info_label);

    // Search and action toolbar
    auto* toolbar_layout = new QHBoxLayout();
    toolbar_layout->setSpacing(6);

    search_field = new QLineEdit(this);
    search_field->setPlaceholderText(tr("🔍 Поиск читов по названию или действию..."));
    search_field->setClearButtonEnabled(true);
    search_field->setStyleSheet(QStringLiteral("QLineEdit { padding: 6px 10px; border-radius: 6px; border: 1px solid #334155; background: #0f172a; color: #f8fafc; font-size: 9.5pt; } QLineEdit:focus { border: 1px solid #00f2fe; }"));
    toolbar_layout->addWidget(search_field, 1);

    download_button = new QPushButton(tr("📥 Скачать базу читов"), this);
    download_button->setToolTip(tr("Скачать проверенные чит-коды из официальных баз Tinfoil и Atmosphere"));
    download_button->setStyleSheet(QStringLiteral("QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0891b2, stop:1 #06b6d4); color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 6px; border: none; font-size: 9pt; } QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0e7490, stop:1 #0891b2); } QPushButton:disabled { background: #334155; color: #64748b; }"));
    toolbar_layout->addWidget(download_button);

    add_button = new QPushButton(tr("➕ Добавить чит"), this);
    add_button->setToolTip(tr("Добавить собственный чит-код в формате Atmosphere"));
    add_button->setStyleSheet(QStringLiteral("QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #059669, stop:1 #10b981); color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 6px; border: none; font-size: 9pt; } QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #047857, stop:1 #059669); }"));
    toolbar_layout->addWidget(add_button);

    open_folder_button = new QPushButton(tr("📁 Папка"), this);
    open_folder_button->setToolTip(tr("Открыть папку читов игры в проводнике"));
    open_folder_button->setStyleSheet(QStringLiteral("QPushButton { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #d97706, stop:1 #f59e0b); color: #ffffff; font-weight: bold; padding: 6px 14px; border-radius: 6px; border: none; font-size: 9pt; } QPushButton:hover { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #b45309, stop:1 #d97706); }"));
    toolbar_layout->addWidget(open_folder_button);

    main_layout->addLayout(toolbar_layout);

    // Cheats tree widget
    tree_widget = new QTreeWidget(this);
    tree_widget->setHeaderLabels({tr("Чит-код / Описание"), tr("Статус"), tr("Версия / Сборка"), tr("Опкоды (Инструкция)")});
    tree_widget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_widget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree_widget->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    tree_widget->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    tree_widget->setAlternatingRowColors(true);
    tree_widget->setRootIsDecorated(false);
    tree_widget->setStyleSheet(QStringLiteral("QTreeWidget { background: #0b0f19; alternate-background-color: #111827; border: 1px solid #1e293b; border-radius: 6px; color: #f1f5f9; } QTreeWidget::item { padding: 4px 0px; } QTreeWidget::item:selected { background: rgba(0, 242, 254, 0.15); color: #ffffff; } QHeaderView::section { background: #1e293b; color: #94a3b8; font-weight: bold; padding: 6px; border: 1px solid #0f172a; }"));
    main_layout->addWidget(tree_widget, 1);

    // Selection bottom bar
    auto* bottom_layout = new QHBoxLayout();
    select_all_button = new QPushButton(tr("☑️ Включить все"), this);
    select_all_button->setStyleSheet(QStringLiteral("QPushButton { background: #1e293b; color: #38bdf8; font-weight: bold; padding: 5px 12px; border-radius: 5px; border: 1px solid #334155; } QPushButton:hover { background: #334155; }"));
    deselect_all_button = new QPushButton(tr("⬜ Отключить все"), this);
    deselect_all_button->setStyleSheet(QStringLiteral("QPushButton { background: #1e293b; color: #94a3b8; font-weight: bold; padding: 5px 12px; border-radius: 5px; border: 1px solid #334155; } QPushButton:hover { background: #334155; }"));
    status_label = new QLabel(this);
    status_label->setStyleSheet(QStringLiteral("color: #94a3b8; font-size: 9pt;"));

    bottom_layout->addWidget(select_all_button);
    bottom_layout->addWidget(deselect_all_button);
    bottom_layout->addStretch(1);
    bottom_layout->addWidget(status_label);
    main_layout->addLayout(bottom_layout);

    // Connections
    connect(tree_widget, &QTreeWidget::itemChanged, this, &ConfigurePerGameCheats::OnItemChanged);
    connect(download_button, &QPushButton::clicked, this, &ConfigurePerGameCheats::OnDownloadOnlineCheats);
    connect(add_button, &QPushButton::clicked, this, &ConfigurePerGameCheats::OnAddCustomCheat);
    connect(open_folder_button, &QPushButton::clicked, this, &ConfigurePerGameCheats::OnOpenCheatsFolder);
    connect(select_all_button, &QPushButton::clicked, this, &ConfigurePerGameCheats::OnSelectAll);
    connect(deselect_all_button, &QPushButton::clicked, this, &ConfigurePerGameCheats::OnDeselectAll);
    connect(search_field, &QLineEdit::textChanged, this, &ConfigurePerGameCheats::OnFilterTextChanged);

    LoadCheats();
}

ConfigurePerGameCheats::~ConfigurePerGameCheats() = default;

void ConfigurePerGameCheats::RetranslateUI() {
    // UI strings are managed by Qt translations
}

QString ConfigurePerGameCheats::GetCheatsDirectoryPath() const {
    const auto cheats_base = Common::FS::GetEdenPath(Common::FS::EdenPath::EdenDir) / "cheats";
    const auto title_dir = cheats_base / fmt::format("{:016X}", title_id);
    return QString::fromStdString(Common::FS::PathToUTF8String(title_dir));
}

QString ConfigurePerGameCheats::GetCheatsFilePath() const {
    const auto dir_path = GetCheatsDirectoryPath();
    const auto clean_build_id = build_id_str.trimmed().toUpper();
    return QDir(dir_path).filePath(QStringLiteral("%1.txt").arg(clean_build_id));
}

void ConfigurePerGameCheats::LoadCheats() {
    cheat_items.clear();
    const auto dir_path = GetCheatsDirectoryPath();
    QDir dir(dir_path);
    dir.mkpath(dir_path);

    const auto& disabled = Settings::values.disabled_addons[title_id];

    const QStringList txt_files = dir.entryList(QStringList() << QStringLiteral("*.txt"), QDir::Files);
    for (const auto& fname : txt_files) {
        const QString full_path = dir.filePath(fname);
        QFile file(full_path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;

        const QString all_content = QString::fromUtf8(file.readAll());
        file.close();

        if (IsBogusCheatText(all_content)) {
            QFile::remove(full_path);
            continue;
        }

        QString bid_label = fname.left(fname.lastIndexOf(QLatin1Char('.'))).toUpper();
        if (bid_label == QStringLiteral("CHEATS") || bid_label == QString(QStringLiteral("%1")).arg(title_id, 16, 16, QLatin1Char('0')).toUpper()) {
            bid_label = tr("Все версии");
        }

        QTextStream stream(all_content.toUtf8());
        QString current_cheat_name;
        QString current_cheat_code;

        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QStringLiteral("#")) || line.startsWith(QStringLiteral("//")) || line.startsWith(QStringLiteral(";"))) continue;

            if (line.startsWith(QStringLiteral("[")) && line.contains(QStringLiteral("]"))) {
                if (!current_cheat_name.isEmpty() && !current_cheat_code.isEmpty()) {
                    // Strictly disabled by default unless explicitly in __ENABLED__ list
                    const bool is_enabled = std::find(disabled.cbegin(), disabled.cend(), "__ENABLED__:" + current_cheat_name.toStdString()) != disabled.cend();

                    // Avoid duplicate cheat names
                    bool exists = std::any_of(cheat_items.begin(), cheat_items.end(), [&](const CheatItem& ci) {
                        return ci.name == current_cheat_name;
                    });
                    if (!exists) {
                        cheat_items.push_back({current_cheat_name, current_cheat_code.trimmed(), bid_label, is_enabled, false});
                    }
                }
                int close_idx = line.indexOf(QLatin1Char(']'));
                current_cheat_name = line.mid(1, close_idx - 1).trimmed();
                current_cheat_code.clear();
            } else {
                current_cheat_code += line + QStringLiteral("\n");
            }
        }

        if (!current_cheat_name.isEmpty() && !current_cheat_code.isEmpty()) {
            const bool is_enabled = std::find(disabled.cbegin(), disabled.cend(), "__ENABLED__:" + current_cheat_name.toStdString()) != disabled.cend();

            bool exists = std::any_of(cheat_items.begin(), cheat_items.end(), [&](const CheatItem& ci) {
                return ci.name == current_cheat_name;
            });
            if (!exists) {
                cheat_items.push_back({current_cheat_name, current_cheat_code.trimmed(), bid_label, is_enabled, false});
            }
        }
    }

    PopulateCheatTree();
}

void ConfigurePerGameCheats::PopulateCheatTree() {
    tree_widget->blockSignals(true);
    tree_widget->clear();

    const QString search_query = search_field->text().trimmed();
    int enabled_count = 0;
    const bool is_game_running = system.IsPoweredOn() && (system.GetApplicationProcessProgramID() == title_id);

    QFont monospace_font(QStringLiteral("Consolas"));
    monospace_font.setStyleHint(QFont::Monospace);
    monospace_font.setPointSize(9);

    for (int i = 0; i < cheat_items.size(); i++) {
        const auto& item = cheat_items[i];
        if (!search_query.isEmpty() && !item.name.contains(search_query, Qt::CaseInsensitive)) {
            continue;
        }

        if (item.enabled) {
            enabled_count++;
        }

        auto* tree_item = new QTreeWidgetItem(tree_widget);
        tree_item->setText(0, item.is_custom ? tr("⭐ %1 [Пользовательский]").arg(item.name) : item.name);
        tree_item->setCheckState(0, item.enabled ? Qt::Checked : Qt::Unchecked);
        tree_item->setData(0, Qt::UserRole, i);

        if (is_game_running && item.enabled) {
            tree_item->setText(1, tr("⚡ Активен в игре"));
            tree_item->setForeground(1, QColor(QStringLiteral("#00f2fe")));
            QFont bold_f = tree_item->font(1);
            bold_f.setBold(true);
            tree_item->setFont(1, bold_f);
        } else if (item.enabled) {
            tree_item->setText(1, tr("✅ Включен"));
            tree_item->setForeground(1, QColor(QStringLiteral("#10b981")));
            QFont bold_f = tree_item->font(1);
            bold_f.setBold(true);
            tree_item->setFont(1, bold_f);
        } else {
            tree_item->setText(1, tr("⚪ Выключен"));
            tree_item->setForeground(1, QColor(QStringLiteral("#64748b")));
        }

        tree_item->setText(2, item.build_id.isEmpty() ? tr("🌐 Все версии") : QStringLiteral("🏷️ %1").arg(item.build_id));
        tree_item->setForeground(2, QColor(QStringLiteral("#38bdf8")));

        QString preview_code = item.code.split(QStringLiteral("\n"), Qt::SkipEmptyParts).value(0, QString{});
        if (item.code.count(QStringLiteral("\n")) > 1) {
            preview_code += QString(QStringLiteral(" (+%1 строк)")).arg(item.code.count(QStringLiteral("\n")));
        }
        tree_item->setText(3, preview_code);
        tree_item->setFont(3, monospace_font);
        tree_item->setForeground(3, QColor(QStringLiteral("#94a3b8")));
    }

    if (cheat_items.isEmpty()) {
        status_label->setText(tr("Чит-коды для этой игры не найдены. Нажмите «📥 Скачать базу читов» или добавьте свой код."));
    } else {
        status_label->setText(tr("Всего читов: <b>%1</b> &nbsp;|&nbsp; Включено: <b style='color:%2;'>%3</b> &nbsp;|&nbsp; Статус: <b>%4</b>")
                                  .arg(cheat_items.size())
                                  .arg(enabled_count > 0 ? QStringLiteral("#00f2fe") : QStringLiteral("#94a3b8"))
                                  .arg(enabled_count)
                                  .arg(is_game_running ? tr("<span style='color:#00f2fe;'>⚡ Применяются в реальном времени</span>") : (enabled_count > 0 ? tr("<span style='color:#10b981;'>✅ Готовы к запуску</span>") : tr("<span style='color:#64748b;'>⚪ Все отключены</span>"))));
    }
    tree_widget->blockSignals(false);
}

void ConfigurePerGameCheats::OnItemChanged(QTreeWidgetItem* item, int column) {
    if (column != 0 || !item) return;

    int idx = item->data(0, Qt::UserRole).toInt();
    if (idx >= 0 && idx < cheat_items.size()) {
        const bool is_checked = (item->checkState(0) == Qt::Checked);
        cheat_items[idx].enabled = is_checked;
        const bool is_game_running = system.IsPoweredOn() && (system.GetApplicationProcessProgramID() == title_id);
        if (is_game_running && is_checked) {
            item->setText(1, tr("⚡ Активен в игре"));
            item->setForeground(1, QColor(QStringLiteral("#00f2fe")));
            QFont bold_f = item->font(1);
            bold_f.setBold(true);
            item->setFont(1, bold_f);
        } else if (is_checked) {
            item->setText(1, tr("✅ Включен"));
            item->setForeground(1, QColor(QStringLiteral("#10b981")));
            QFont bold_f = item->font(1);
            bold_f.setBold(true);
            item->setFont(1, bold_f);
        } else {
            item->setText(1, tr("⚪ Выключен"));
            item->setForeground(1, QColor(QStringLiteral("#64748b")));
            QFont normal_f = item->font(1);
            normal_f.setBold(false);
            item->setFont(1, normal_f);
        }
    }

    int enabled_count = 0;
    for (const auto& it : cheat_items) {
        if (it.enabled) enabled_count++;
    }
    const bool is_game_running = system.IsPoweredOn() && (system.GetApplicationProcessProgramID() == title_id);
    status_label->setText(tr("Всего читов: <b>%1</b> &nbsp;|&nbsp; Включено: <b style='color:%2;'>%3</b> &nbsp;|&nbsp; Статус: <b>%4</b>")
                              .arg(cheat_items.size())
                              .arg(enabled_count > 0 ? QStringLiteral("#00f2fe") : QStringLiteral("#94a3b8"))
                              .arg(enabled_count)
                              .arg(is_game_running ? tr("<span style='color:#00f2fe;'>⚡ Применяются в реальном времени</span>") : (enabled_count > 0 ? tr("<span style='color:#10b981;'>✅ Готовы к запуску</span>") : tr("<span style='color:#64748b;'>⚪ Все отключены</span>"))));

    ApplyConfiguration();
    emit CheatsChanged();
}

void ConfigurePerGameCheats::OnFilterTextChanged(const QString& /*text*/) {
    PopulateCheatTree();
}

void ConfigurePerGameCheats::OnSelectAll() {
    for (auto& item : cheat_items) {
        item.enabled = true;
    }
    PopulateCheatTree();
    ApplyConfiguration();
    emit CheatsChanged();
}

void ConfigurePerGameCheats::OnDeselectAll() {
    for (auto& item : cheat_items) {
        item.enabled = false;
    }
    PopulateCheatTree();
    ApplyConfiguration();
    emit CheatsChanged();
}

void ConfigurePerGameCheats::OnOpenCheatsFolder() {
    const auto dir_path = GetCheatsDirectoryPath();
    QDir().mkpath(dir_path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir_path));
}

void ConfigurePerGameCheats::OnDownloadOnlineCheats() {
    const auto tid_hex = QString(QStringLiteral("%1")).arg(title_id, 16, 16, QLatin1Char('0')).toUpper();

    download_button->setEnabled(false);
    download_button->setText(tr("⏳ Загрузка..."));

    // URLs: Tinfoil official media database, Atmosphere switch-cheats-db and mirrors
    QStringList urls_to_try = {
        QString(QStringLiteral("https://tinfoil.media/api/cheats/%1")).arg(tid_hex),
        QString(QStringLiteral("https://tinfoil.io/api/cheats/%1")).arg(tid_hex),
        QString(QStringLiteral("https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master/cheats/%1.json")).arg(tid_hex),
        QString(QStringLiteral("https://cdn.jsdelivr.net/gh/HamletDuFromage/switch-cheats-db@master/cheats/%1.json")).arg(tid_hex),
        QString(QStringLiteral("https://raw.githubusercontent.com/HamletDuFromage/switch-cheats-db/master/titles/%1.txt")).arg(tid_hex)
    };

    bool downloaded = false;
    QString downloaded_raw;
    bool is_json = false;

    QNetworkAccessManager manager;
    for (const auto& url_str : urls_to_try) {
        QNetworkRequest request{QUrl(url_str)};
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("STORM_EDEN_Emulator/4.3.0"));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

        QNetworkReply* reply = manager.get(request);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer.start(6000);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
            downloaded_raw = QString::fromUtf8(reply->readAll());
            if (!downloaded_raw.trimmed().isEmpty() && !IsBogusCheatText(downloaded_raw)) {
                // If it's a JSON array or object, check if it contains actual cheat entries
                if (url_str.contains(QStringLiteral("api/cheats"), Qt::CaseInsensitive) || url_str.contains(QStringLiteral(".json"), Qt::CaseInsensitive)) {
                    QJsonDocument test_doc = QJsonDocument::fromJson(downloaded_raw.toUtf8());
                    if (test_doc.isArray() && !test_doc.array().isEmpty()) {
                        downloaded = true;
                        is_json = true;
                        reply->deleteLater();
                        break;
                    } else if (test_doc.isObject() && !test_doc.object().isEmpty()) {
                        downloaded = true;
                        is_json = true;
                        reply->deleteLater();
                        break;
                    }
                } else {
                    downloaded = true;
                    is_json = false;
                    reply->deleteLater();
                    break;
                }
            }
        }
        reply->deleteLater();
    }

    download_button->setEnabled(true);
    download_button->setText(tr("📥 Скачать базу читов"));

    if (downloaded && !downloaded_raw.isEmpty()) {
        const auto dir_path = GetCheatsDirectoryPath();
        QDir().mkpath(dir_path);

        if (is_json) {
            QJsonDocument doc = QJsonDocument::fromJson(downloaded_raw.toUtf8());
            if (doc.isArray()) {
                // Tinfoil Media API format: [ { "name": "...", "build_id": "...", "source": "..." } ]
                const QJsonArray arr = doc.array();
                QMap<QString, QString> bid_map;
                QString combined_cheats;

                for (const auto& item_val : arr) {
                    const QJsonObject item = item_val.toObject();
                    QString c_name = item.value(QStringLiteral("name")).toString().trimmed();
                    QString c_bid = item.value(QStringLiteral("build_id")).toString().trimmed().toUpper();
                    QString c_source = item.value(QStringLiteral("source")).toString().trimmed();

                    if (c_name.isEmpty() || c_source.isEmpty()) continue;
                    if (!c_name.startsWith(QLatin1Char('['))) c_name = QStringLiteral("[%1]").arg(c_name);
                    if (!c_name.endsWith(QLatin1Char(']'))) c_name = QStringLiteral("%1]").arg(c_name);

                    QString entry = QStringLiteral("%1\n%2\n\n").arg(c_name, c_source);
                    if (c_bid.isEmpty()) c_bid = build_id_str.trimmed().toUpper();

                    bid_map[c_bid] += entry;
                    combined_cheats += entry;
                }

                for (auto it = bid_map.begin(); it != bid_map.end(); ++it) {
                    QFile bid_file(QDir(dir_path).filePath(QStringLiteral("%1.txt").arg(it.key())));
                    if (bid_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QTextStream stream(&bid_file);
                        stream << it.value();
                        bid_file.close();
                    }
                }

                if (!combined_cheats.isEmpty()) {
                    QFile all_file(QDir(dir_path).filePath(QStringLiteral("cheats.txt")));
                    if (all_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QTextStream stream(&all_file);
                        stream << combined_cheats;
                        all_file.close();
                    }
                }
            } else if (doc.isObject()) {
                // Switch-cheats-db format: { "BUILD_ID": { "Cheat Name": "..." } }
                const QJsonObject root = doc.object();
                QString combined_cheats;

                for (auto it = root.begin(); it != root.end(); ++it) {
                    const QString bid_key = it.key().trimmed().toUpper();
                    const QJsonObject bid_cheats = it.value().toObject();

                    QString bid_file_content;
                    for (auto c_it = bid_cheats.begin(); c_it != bid_cheats.end(); ++c_it) {
                        const QString cheat_body = c_it.value().toString();
                        if (!cheat_body.trimmed().isEmpty()) {
                            bid_file_content += cheat_body.trimmed() + QStringLiteral("\n\n");
                            combined_cheats += cheat_body.trimmed() + QStringLiteral("\n\n");
                        }
                    }

                    if (!bid_file_content.isEmpty()) {
                        QFile bid_file(QDir(dir_path).filePath(QStringLiteral("%1.txt").arg(bid_key)));
                        if (bid_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                            QTextStream stream(&bid_file);
                            stream << bid_file_content;
                            bid_file.close();
                        }
                    }
                }

                if (!combined_cheats.isEmpty()) {
                    QFile all_file(QDir(dir_path).filePath(QStringLiteral("cheats.txt")));
                    if (all_file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QTextStream stream(&all_file);
                        stream << combined_cheats;
                        all_file.close();
                    }
                }
            }
        } else {
            // Direct .txt file
            QFile file(QDir(dir_path).filePath(QStringLiteral("cheats.txt")));
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                stream << downloaded_raw;
                file.close();
            }
        }

        LoadCheats();
        if (!cheat_items.isEmpty()) {
            QMessageBox::information(this, tr("Читы загружены"),
                                     tr("Успешно загружена база чит-кодов из Tinfoil / Atmosphere!\nНайдено уникальных читов: %1").arg(cheat_items.size()));
        } else {
            QMessageBox::information(this, tr("Чит-коды"),
                                     tr("Для данной версии/сборки игры читы в базе пока не найдены."));
        }
    } else {
        QMessageBox::information(this, tr("Чит-коды"),
                                 tr("Для данной игры (Title ID: %1) в базах Tinfoil и Atmosphere пока нет чит-кодов.\nВы можете добавить чит вручную кнопкой «➕ Добавить чит».").arg(tid_hex));
    }
}

void ConfigurePerGameCheats::OnAddCustomCheat() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Добавить чит-код"));
    dialog.resize(500, 360);

    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("<b>Название чита:</b>"), &dialog));
    auto* name_edit = new QLineEdit(&dialog);
    name_edit->setPlaceholderText(tr("Например: Бесконечные жизни или 999 монет"));
    layout->addWidget(name_edit);

    layout->addWidget(new QLabel(tr("<b>Код чита (формат Atmosphere):</b>"), &dialog));
    auto* code_edit = new QTextEdit(&dialog);
    code_edit->setPlaceholderText(tr("04000000 01234567 00000001\n580F0000 01234568\n..."));
    code_edit->setFontFamily(QStringLiteral("Courier New"));
    layout->addWidget(code_edit);

    auto* btn_box = new QHBoxLayout();
    auto* ok_btn = new QPushButton(tr("Сохранить чит"), &dialog);
    ok_btn->setStyleSheet(QStringLiteral("background-color: #00f2fe; color: #000000; font-weight: bold; padding: 6px 16px; border-radius: 5px;"));
    auto* cancel_btn = new QPushButton(tr("Отмена"), &dialog);
    btn_box->addStretch(1);
    btn_box->addWidget(cancel_btn);
    btn_box->addWidget(ok_btn);
    layout->addLayout(btn_box);

    connect(ok_btn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancel_btn, &QPushButton::clicked, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QString cheat_name = name_edit->text().trimmed();
        QString cheat_code = code_edit->toPlainText().trimmed();
        if (cheat_name.isEmpty() || cheat_code.isEmpty()) {
            QMessageBox::warning(this, tr("Ошибка"), tr("Название и код чита не могут быть пустыми!"));
            return;
        }

        cheat_items.push_back({cheat_name, cheat_code, tr("Пользовательский"), false, true});
        SaveCheatsToFile();
        PopulateCheatTree();
        ApplyConfiguration();
        emit CheatsChanged();
    }
}

void ConfigurePerGameCheats::SaveCheatsToFile() {
    const auto dir_path = GetCheatsDirectoryPath();
    QDir().mkpath(dir_path);
    const auto file_path = GetCheatsFilePath();

    QFile file(file_path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        for (const auto& item : cheat_items) {
            stream << QString(QStringLiteral("[%1]\n")).arg(item.name);
            stream << item.code.trimmed() << QStringLiteral("\n\n");
        }
        file.close();
    }
}

void ConfigurePerGameCheats::ApplyConfiguration() {
    SaveCheatsToFile();

    // Update disabled_addons list with explicit enabled state, preserving non-cheat addons (mods, DLC, updates)
    auto& disabled = Settings::values.disabled_addons[title_id];

    // Remove existing cheat tags for current items
    std::erase_if(disabled, [&](const std::string& s) {
        if (s.starts_with("__ENABLED__:")) {
            const std::string name = s.substr(12);
            return std::any_of(cheat_items.begin(), cheat_items.end(), [&](const CheatItem& ci) {
                return ci.name.toStdString() == name;
            });
        }
        return std::any_of(cheat_items.begin(), cheat_items.end(), [&](const CheatItem& ci) {
            return ci.name.toStdString() == s;
        });
    });

    std::vector<Core::Memory::CheatEntry> active_cheat_entries;
    const Core::Memory::TextCheatParser parser;

    for (const auto& item : cheat_items) {
        if (item.enabled) {
            disabled.push_back("__ENABLED__:" + item.name.toStdString());
            std::string formatted = fmt::format("[{}]\n{}\n", item.name.toStdString(), item.code.toStdString());
            auto parsed = parser.Parse(formatted);
            for (auto& p : parsed) {
                active_cheat_entries.push_back(std::move(p));
            }
        } else {
            disabled.push_back(item.name.toStdString());
        }
    }

    // If game is actively running, reload cheat engine in real-time!
    if (system.IsPoweredOn() && system.HasCheatEngine()) {
        system.ReloadCheatList(active_cheat_entries);
        LOG_INFO(Common, "Dynamically reloaded {} active cheat entries during gameplay for title_id=0x{:016X}",
                 active_cheat_entries.size(), title_id);
    }
}
