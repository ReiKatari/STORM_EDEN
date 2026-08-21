// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <memory>
#include <utility>

#include <fmt/format.h>

#include <QDesktopServices>
#include <QHeaderView>
#include <QMenu>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <QTreeView>

#include "common/common_types.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "configuration/addon/mod_select_dialog.h"
#include "core/core.h"
#include "core/file_sys/patch_manager.h"
#include "core/loader/loader.h"
#include "frontend_common/mod_manager.h"
#include "qt_common/abstract/frontend.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/titledb.h"
#include "qt_common/util/mod.h"
#include "ui_configure_per_game_addons.h"
#include "yuzu/configuration/configure_input.h"
#include "yuzu/configuration/configure_per_game_addons.h"

ConfigurePerGameAddons::ConfigurePerGameAddons(Core::System& system_, QWidget* parent)
    : QWidget(parent), ui{std::make_unique<Ui::ConfigurePerGameAddons>()}, system{system_} {
    ui->setupUi(this);

    layout = new QVBoxLayout;
    tree_view = new QTreeView;
    item_model = new QStandardItemModel(tree_view);
    tree_view->setModel(item_model);
    tree_view->setAlternatingRowColors(true);
    tree_view->setSelectionMode(QHeaderView::ExtendedSelection);
    tree_view->setSelectionBehavior(QHeaderView::SelectRows);
    tree_view->setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setSortingEnabled(true);
    tree_view->setEditTriggers(QHeaderView::NoEditTriggers);
    tree_view->setUniformRowHeights(true);
    tree_view->setContextMenuPolicy(Qt::CustomContextMenu);

    item_model->insertColumns(0, 2);
    item_model->setHeaderData(0, Qt::Horizontal, tr("ТИП / ЭЛЕМЕНТ"));
    item_model->setHeaderData(1, Qt::Horizontal, tr("ВЕРСИЯ / НАЗВАНИЕ ДОПОЛНЕНИЯ"));

    tree_view->header()->setStretchLastSection(true);
    tree_view->header()->setMinimumSectionSize(160);
    tree_view->header()->resizeSection(0, 180);
    tree_view->header()->resizeSection(1, 480);

    ui->folder->setText(tr("📁 Установить мод из папки..."));
    ui->zip->setText(tr("📦 Установить мод из архива (ZIP)..."));

    // We must register all custom types with the Qt Automoc system so that we are able to use it
    // with signals/slots. In this case, QList falls under the umbrella of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tree_view);

    ui->scrollArea->setLayout(layout);

    ui->scrollArea->setEnabled(!system.IsPoweredOn());

    connect(item_model, &QStandardItemModel::itemChanged, this,
            &ConfigurePerGameAddons::OnItemChanged);
    connect(item_model, &QStandardItemModel::itemChanged,
            [] { UISettings::values.is_game_list_reload_pending.exchange(true); });

    connect(ui->folder, &QAbstractButton::clicked, this, &ConfigurePerGameAddons::InstallModFolder);
    connect(ui->zip, &QAbstractButton::clicked, this, &ConfigurePerGameAddons::InstallModZip);

    connect(tree_view, &QTreeView::customContextMenuRequested, this,
            &ConfigurePerGameAddons::showContextMenu);
}

ConfigurePerGameAddons::~ConfigurePerGameAddons() = default;

void ConfigurePerGameAddons::OnItemChanged(QStandardItem* item) {
    if (update_items.size() > 1 && item->checkState() == Qt::Checked) {
        auto it = std::find(update_items.begin(), update_items.end(), item);
        if (it != update_items.end()) {
            for (auto* update_item : update_items) {
                if (update_item != item && update_item->checkState() == Qt::Checked) {
                    disconnect(item_model, &QStandardItemModel::itemChanged, this,
                               &ConfigurePerGameAddons::OnItemChanged);
                    update_item->setCheckState(Qt::Unchecked);
                    connect(item_model, &QStandardItemModel::itemChanged, this,
                            &ConfigurePerGameAddons::OnItemChanged);
                }
            }
        }
    }
}

void ConfigurePerGameAddons::ApplyConfiguration() {
    std::vector<std::string> disabled_addons;

    for (const auto& item : list_items) {
        const auto disabled = item.front()->checkState() == Qt::Unchecked;
        if (disabled) {
            QVariant dlcData = item.front()->data(DLC_INDEX);
            if (dlcData.isValid()) {
                quint32 dlc_num = dlcData.toUInt();
                disabled_addons.push_back(fmt::format("DLC@{}", dlc_num));
                continue;
            }

            QVariant userData = item.front()->data(NUMERIC_VERSION);
            if (userData.isValid() && userData.canConvert<quint32>() &&
                (item.front()->text() == tr("Обновление") || item.front()->text() == QStringLiteral("Update"))) {
                quint32 numeric_version = userData.toUInt();
                disabled_addons.push_back(fmt::format("Update@{}", numeric_version));
            } else if (item.front()->text() == tr("Обновление") || item.front()->text() == QStringLiteral("Update")) {
                disabled_addons.push_back("Update");
            } else if (item.front()->text() == tr("Дополнения") || item.front()->text() == QStringLiteral("DLC")) {
                disabled_addons.push_back("DLC");
            } else {
                disabled_addons.push_back(item.front()->text().toStdString());
            }
        }
    }

    // Preserve existing cheat entries in Settings::values.disabled_addons[title_id]
    const auto& current_entries = Settings::values.disabled_addons[title_id];
    for (const auto& entry : current_entries) {
        if (entry.starts_with("__ENABLED__:")) {
            disabled_addons.push_back(entry);
        }
    }

    Settings::values.disabled_addons[title_id] = disabled_addons;
}

void ConfigurePerGameAddons::SetTitleId(u64 id) {
    this->title_id = id;
}

void ConfigurePerGameAddons::SetGameVersion(const QString& version) {
    this->game_version_str = version;
}

void ConfigurePerGameAddons::LoadFromFile(FileSys::VirtualFile file_) {
    file = std::move(file_);
    LoadConfiguration();
}

void ConfigurePerGameAddons::InstallMods(const QStringList& mods) {
    QStringList failed;
    for (const auto& mod : mods) {
        if (FrontendCommon::InstallMod(mod.toStdString(), title_id, true) ==
            FrontendCommon::Failed) {
            failed << QFileInfo(mod).baseName();
        }
    }

    if (failed.empty()) {
        QtCommon::Frontend::Information(tr("Mod Install Succeeded"),
                                        tr("Successfully installed all mods."));

        item_model->removeRows(0, item_model->rowCount());
        list_items.clear();
        LoadConfiguration();

        UISettings::values.is_game_list_reload_pending.exchange(true);
    } else {
        QtCommon::Frontend::Critical(
            tr("Mod Install Failed"),
            tr("Failed to install the following mods:\n\t%1\nCheck the log for details.")
                .arg(failed.join(QStringLiteral("\n\t"))));
    }
}

void ConfigurePerGameAddons::InstallModPath(const QString& path, const QString& fallbackName) {
    const auto mods = QtCommon::Mod::GetModFolders(path, fallbackName);

    if (mods.size() > 1) {
        ModSelectDialog* dialog = new ModSelectDialog(mods, this);
        connect(dialog, &ModSelectDialog::modsSelected, this, &ConfigurePerGameAddons::InstallMods);
        dialog->show();
    } else if (!mods.empty()) {
        InstallMods(mods);
    }
}

void ConfigurePerGameAddons::InstallModFolder() {
    const auto path = QtCommon::Frontend::GetExistingDirectory(
        tr("Mod Folder"), QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    if (path.isEmpty()) {
        return;
    }

    InstallModPath(path);
}

void ConfigurePerGameAddons::InstallModZip() {
    // TODO(crueter): use GetOpenFileName to allow select multiple ZIPs
    const auto path = QtCommon::Frontend::GetOpenFileName(
        tr("Zipped Mod Location"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        tr("Zipped Archives (*.zip)"));
    if (path.isEmpty()) {
        return;
    }

    const QString extracted = QtCommon::Mod::ExtractMod(path);
    if (!extracted.isEmpty())
        InstallModPath(extracted, QFileInfo(path).baseName());
}

void ConfigurePerGameAddons::AddonDeleteRequested(QList<QModelIndex> selected) {
    QList<QModelIndex> filtered;
    for (const QModelIndex& index : selected) {
        if (!index.data(PATCH_LOCATION).toString().isEmpty())
            filtered << index;
    }

    if (filtered.empty()) {
        QtCommon::Frontend::Critical(tr("Invalid Selection"),
                                     tr("Only mods, cheats, and patches can be deleted.\nTo delete "
                                        "NAND-installed updates, right-click the game in the game "
                                        "list and click Remove -> Remove Installed Update."));
        return;
    }

    const auto header = tr("You are about to delete the following installed mods:\n");
    QString selected_str;
    for (const QModelIndex& index : filtered) {
        selected_str = selected_str % index.data().toString() % QStringLiteral("\n");
    }

    const auto footer = tr("\nOnce deleted, these can NOT be recovered. Are you 100% sure "
                           "you want to delete them?");

    QString caption = header % selected_str % footer;

    auto choice = QtCommon::Frontend::Warning(tr("Delete add-on(s)?"), caption,
                                              QtCommon::Frontend::StandardButton::Yes |
                                                  QtCommon::Frontend::StandardButton::No);

    if (choice == QtCommon::Frontend::StandardButton::No)
        return;

    for (const QModelIndex& index : filtered) {
        std::filesystem::remove_all(index.data(PATCH_LOCATION).toString().toStdString());
    }

    QtCommon::Frontend::Information(tr("Successfully deleted"),
                                    tr("Successfully deleted all selected mods."));

    item_model->removeRows(0, item_model->rowCount());
    list_items.clear();
    LoadConfiguration();

    UISettings::values.is_game_list_reload_pending.exchange(true);
}

void ConfigurePerGameAddons::showContextMenu(const QPoint& pos) {
    const QModelIndex index = tree_view->indexAt(pos);
    auto selected = tree_view->selectionModel()->selectedRows();
    if (index.isValid() && selected.empty()) {
        QModelIndex idx = item_model->index(index.row(), 0);
        if (idx.isValid())
            selected << idx;
    }

    if (selected.empty())
        return;

    QMenu context_menu(this);
    context_menu.addAction(tr("Delete Selected"),
                           [this, selected] { AddonDeleteRequested(selected); });

    if (selected.length() == 1) {
        auto loc = selected.at(0).data(PATCH_LOCATION).toString();
        if (QFileInfo::exists(loc)) {
            context_menu.addAction(tr("&Open in File Manager"),
                                   [loc]() { QDesktopServices::openUrl(QUrl::fromLocalFile(loc)); });
        }
    }

    context_menu.exec(tree_view->viewport()->mapToGlobal(pos));
}

void ConfigurePerGameAddons::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigurePerGameAddons::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigurePerGameAddons::LoadConfiguration() {
    if (file == nullptr) {
        return;
    }

    const FileSys::PatchManager pm{title_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};
    const auto loader = Loader::GetLoader(system, file);

    FileSys::VirtualFile update_raw;
    loader->ReadUpdateRaw(update_raw);

    const auto& disabled = Settings::values.disabled_addons[title_id];

    update_items.clear();
    list_items.clear();
    item_model->removeRows(0, item_model->rowCount());

    std::vector<FileSys::Patch> patches = pm.GetPatches(update_raw);

    bool update_added = false;
    int mod_counter = 1;

    // Ensure TitleDB is loaded
    TitleDB::TitleDatabase::Instance().WaitLoaded(std::chrono::milliseconds(3000));

    for (const auto& patch : patches) {
        if (patch.type == FileSys::PatchType::DLC || patch.name == "DLC") {
            QStringList dlc_list = QString::fromStdString(patch.version).split(QLatin1Char(','), Qt::SkipEmptyParts);
            if (dlc_list.isEmpty()) {
                dlc_list.append(QStringLiteral("1"));
            }
            for (const auto& dlc_idx_str : dlc_list) {
                const u32 dlc_num = dlc_idx_str.trimmed().toUInt();
                const u64 dlc_tid = (title_id & 0xFFFFFFFFFFFFF000) | (dlc_num > 0 ? (0x1000 | (dlc_num & 0x7FF)) : 0x1001);

                auto* const first_item = new QStandardItem;
                first_item->setText(tr("Дополнение #%1").arg(dlc_num > 0 ? dlc_num : 1));
                first_item->setCheckable(true);
                first_item->setData(static_cast<quint32>(dlc_num), DLC_INDEX);

                QString dlc_title;
                auto opt_entry = TitleDB::TitleDatabase::Instance().Lookup(dlc_tid);
                if (opt_entry.has_value() && !opt_entry->name.empty()) {
                    dlc_title = QString::fromStdString(opt_entry->name).trimmed();
                }
                if (dlc_title.isEmpty() || dlc_title.startsWith(QStringLiteral("Дополнение #"))) {
                    const auto tdb_dlcs = TitleDB::TitleDatabase::Instance().GetDlcs(title_id);
                    for (const auto& d : tdb_dlcs) {
                        const std::string d_hex = fmt::format("{:016X}", dlc_tid);
                        if (d.id == d_hex && !d.name.empty()) {
                            dlc_title = QString::fromStdString(d.name).trimmed();
                            break;
                        }
                    }
                    if ((dlc_title.isEmpty() || dlc_title.startsWith(QStringLiteral("Дополнение #"))) && dlc_num > 0 && dlc_num <= tdb_dlcs.size()) {
                        if (!tdb_dlcs[dlc_num - 1].name.empty()) {
                            dlc_title = QString::fromStdString(tdb_dlcs[dlc_num - 1].name).trimmed();
                        }
                    }
                }
                if (dlc_title.isEmpty()) {
                    dlc_title = tr("Дополнение #%1").arg(dlc_num > 0 ? dlc_num : 1);
                }

                const std::string dlc_key = fmt::format("DLC@{}", dlc_num);
                const std::string dlc_tid_key = fmt::format("DLC@{:016X}", dlc_tid);
                bool dlc_disabled = (std::find(disabled.begin(), disabled.end(), "DLC") != disabled.end()) ||
                                     (std::find(disabled.begin(), disabled.end(), dlc_key) != disabled.end()) ||
                                     (std::find(disabled.begin(), disabled.end(), dlc_tid_key) != disabled.end());
                first_item->setCheckState(dlc_disabled ? Qt::Unchecked : Qt::Checked);

                auto* const name_item = new QStandardItem{dlc_title};
                list_items.push_back(QList<QStandardItem*>{first_item, name_item});
                item_model->appendRow(list_items.back());
            }
            continue;
        }

        QString name = QString::fromStdString(patch.name);
        QString version_display = QString::fromStdString(patch.version);

        if (patch.type == FileSys::PatchType::Update || patch.name == "Update") {
            if (update_added) {
                continue;
            }

            name = tr("Обновление");

            if (!game_version_str.isEmpty() && game_version_str != QStringLiteral("1.0.0") && game_version_str != QStringLiteral("0")) {
                version_display = game_version_str;
            } else if (file != nullptr) {
                static const QRegularExpression fn_pair_ver_regex{QStringLiteral(R"(\(([0-9]+\.[0-9]+(?:\.[0-9]+)*)\s*-\s*([0-9]+))")};
                const auto fm = fn_pair_ver_regex.match(QString::fromStdString(file->GetName()));
                if (fm.hasMatch() && !fm.captured(1).isEmpty()) {
                    version_display = fm.captured(1);
                } else {
                    static const QRegularExpression fn_ver_regex{QStringLiteral(R"((?:[\(\[\s]v?|\b)([0-9]+\.[0-9]+(?:\.[0-9]+)*)(?!\s*(?:GB|MB|KB|TB|ГБ|МБ|КБ|Б|B)\b))")};
                    const auto m = fn_ver_regex.match(QString::fromStdString(file->GetName()));
                    if (m.hasMatch() && m.hasCaptured(1)) {
                        version_display = m.captured(1);
                    }
                }
            }

            if (version_display.isEmpty() || version_display == QStringLiteral("1.0.0") || version_display == QStringLiteral("0") || version_display == QStringLiteral("PACKED")) {
                if (const auto nacp = pm.GetControlMetadata().first; nacp != nullptr) {
                    const auto nacp_ver = nacp->GetVersionString();
                    if (!nacp_ver.empty() && nacp_ver != "0") {
                        version_display = QString::fromStdString(nacp_ver);
                    }
                }
            }

            while (version_display.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
                version_display.remove(0, 1);
            }
            version_display = version_display.trimmed();
            if (version_display.isEmpty() || version_display == QStringLiteral("0") || version_display == QStringLiteral("PACKED")) {
                version_display = QStringLiteral("1.0.0");
            }
        } else if (patch.type == FileSys::PatchType::Mod) {
            name = tr("Модификация #%1").arg(mod_counter++);
            QString mod_display = QString::fromStdString(patch.name);
            if (mod_display.compare(QStringLiteral("romfs"), Qt::CaseInsensitive) == 0) {
                mod_display = tr("Вшитый RomFS (LayeredFS)");
            } else if (mod_display.compare(QStringLiteral("exefs"), Qt::CaseInsensitive) == 0) {
                mod_display = tr("Вшитый ExeFS (LayeredExeFS)");
            } else if (!patch.version.empty() && patch.version != "Cheats" && !mod_display.contains(QString::fromStdString(patch.version))) {
                mod_display = QStringLiteral("%1 (%2)").arg(mod_display, QString::fromStdString(patch.version));
            }
            version_display = mod_display;
        }

        auto* const first_item = new QStandardItem;
        first_item->setText(name);
        first_item->setCheckable(true);

        const bool is_external_update = patch.type == FileSys::PatchType::Update &&
                                        patch.source == FileSys::PatchSource::External &&
                                        patch.numeric_version != 0;

        const bool is_mod = patch.type == FileSys::PatchType::Mod;

        if (is_external_update) {
            first_item->setData(static_cast<quint32>(patch.numeric_version), NUMERIC_VERSION);
        } else if (is_mod) {
            first_item->setData(QString::fromStdString(patch.location), PATCH_LOCATION);
        }

        bool patch_disabled = false;
        if (is_external_update) {
            std::string disabled_key = fmt::format("Update@{}", patch.numeric_version);
            patch_disabled =
                std::find(disabled.begin(), disabled.end(), disabled_key) != disabled.end();
        } else if (patch.type == FileSys::PatchType::Update) {
            std::string disabled_key = "Update";
            patch_disabled =
                std::find(disabled.begin(), disabled.end(), disabled_key) != disabled.end();
        } else {
            const std::string key = patch.name;
            patch_disabled =
                std::find(disabled.begin(), disabled.end(), key) != disabled.end();
        }

        bool should_enable = !patch_disabled;

        if (patch.type == FileSys::PatchType::Update) {
            update_items.push_back(first_item);
            update_added = true;
        }

        first_item->setCheckState(should_enable ? Qt::Checked : Qt::Unchecked);

        list_items.push_back(QList<QStandardItem*>{
            first_item, new QStandardItem{version_display}});
        item_model->appendRow(list_items.back());
    }

    if (!update_added && (!game_version_str.isEmpty() && game_version_str != QStringLiteral("1.0.0") && game_version_str != QStringLiteral("0"))) {
        auto* const first_item = new QStandardItem;
        first_item->setText(tr("Обновление"));
        first_item->setCheckable(true);

        bool patch_disabled = std::find(disabled.begin(), disabled.end(), "Update") != disabled.end();
        first_item->setCheckState(patch_disabled ? Qt::Unchecked : Qt::Checked);

        auto* const name_item = new QStandardItem{game_version_str};
        list_items.push_back(QList<QStandardItem*>{first_item, name_item});
        item_model->appendRow(list_items.back());
        update_items.push_back(first_item);
        update_added = true;
    }

    tree_view->resizeColumnToContents(0);
    tree_view->header()->resizeSection(0, std::max(200, tree_view->columnWidth(0)));
    tree_view->header()->resizeSection(1, 520);
}
