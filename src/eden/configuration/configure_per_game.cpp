// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <fmt/ranges.h>

#include <QAbstractButton>
#include <QCheckBox>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include "common/fs/fs_util.h"
#include "common/settings_enums.h"
#include "common/settings_input.h"
#include "configuration/shared_widget.h"
#include "core/core.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/xts_archive.h"
#include "core/loader/loader.h"
#include "frontend_common/config.h"
#include "qt_common/config/uisettings.h"
#include "ui_configure_per_game.h"
#include "eden/configuration/configuration_shared.h"
#include "eden/configuration/configure_applets.h"
#include "eden/configuration/configure_audio.h"
#include "eden/configuration/configure_cpu.h"
#include "eden/configuration/configure_graphics.h"
#include "eden/configuration/configure_graphics_advanced.h"
#include "eden/configuration/configure_graphics_extensions.h"
#include "eden/configuration/configure_input_per_game.h"
#include "eden/configuration/configure_network.h"
#include "eden/configuration/configure_per_game.h"
#include "eden/configuration/configure_gamebanana.h"
#include "eden/configuration/configure_per_game_addons.h"
#include "eden/configuration/configure_system.h"
#include "eden/util/util.h"
#include "eden/vk_device_info.h"

ConfigurePerGame::ConfigurePerGame(QWidget* parent, u64 title_id_, const std::string& file_name,
                                   std::vector<VkDeviceInfo::Record>& vk_device_records,
                                   Core::System& system_)
    : QDialog(parent), ui(std::make_unique<Ui::ConfigurePerGame>()), title_id{title_id_},
      system{system_},
      builder{std::make_unique<ConfigurationShared::Builder>(this, !system_.IsPoweredOn())},
      tab_group{std::make_shared<std::vector<ConfigurationShared::Tab*>>()} {
    const auto file_path = std::filesystem::path(Common::FS::ToU8String(file_name));
    const auto config_file_name = title_id == 0 ? Common::FS::PathToUTF8String(file_path.filename())
                                                : fmt::format("{:016X}", title_id);
    game_config = std::make_unique<QtConfig>(config_file_name, Config::ConfigType::PerGameConfig);
    addons_tab = std::make_unique<ConfigurePerGameAddons>(system_, this);
    gamebanana_tab = std::make_unique<ConfigureGameBanana>(system_, this, title_id);
    audio_tab = std::make_unique<ConfigureAudio>(system_, tab_group, *builder, this);
    cpu_tab = std::make_unique<ConfigureCpu>(system_, tab_group, *builder, this);
    graphics_advanced_tab =
        std::make_unique<ConfigureGraphicsAdvanced>(system_, tab_group, *builder, this);
    graphics_extensions_tab =
        std::make_unique<ConfigureGraphicsExtensions>(system_, tab_group, *builder, this);
    graphics_tab = std::make_unique<ConfigureGraphics>(
        system_, vk_device_records, [&]() { graphics_advanced_tab->ExposeComputeOption(); },
        [](Settings::AspectRatio, Settings::ResolutionSetup) {}, tab_group, *builder, this);
    input_tab = std::make_unique<ConfigureInputPerGame>(system_, game_config.get(), this);
    system_tab = std::make_unique<ConfigureSystem>(system_, tab_group, *builder, this);
    network_tab = std::make_unique<ConfigureNetwork>(system_, this);
    applets_tab = std::make_unique<ConfigureApplets>(system_, tab_group, *builder, this);

    ui->setupUi(this);

    std::string active_lang = UISettings::values.language.GetValue();
    bool is_ru = (active_lang.rfind("ru", 0) == 0 ||
                  (active_lang.empty() && QLocale::system().name().startsWith(QStringLiteral("ru"))));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(is_ru ? QString::fromUtf8("ОК") : tr("OK"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(is_ru ? QString::fromUtf8("Отмена") : tr("Cancel"));
    ui->display_name->setLineWrapMode(QTextEdit::WidgetWidth);
    ui->display_filename->setLineWrapMode(QTextEdit::WidgetWidth);

    ui->tabWidget->setStyleSheet(QStringLiteral("QTabBar::tab { font-weight: bold; }"));

    ui->tabWidget->addTab(addons_tab.get(), tr("Add-ons"));
    ui->tabWidget->addTab(gamebanana_tab.get(), tr("GameBanana Mods"));
    ui->tabWidget->addTab(system_tab.get(), tr("System"));
    ui->tabWidget->addTab(cpu_tab.get(), tr("CPU"));
    ui->tabWidget->addTab(graphics_tab.get(), tr("Graphics"));
    ui->tabWidget->addTab(graphics_advanced_tab.get(), tr("Adv. Graphics"));
    ui->tabWidget->addTab(graphics_extensions_tab.get(), tr("Ext. Graphics"));
    ui->tabWidget->addTab(audio_tab.get(), tr("Audio"));
    ui->tabWidget->addTab(input_tab.get(), tr("Input Profiles"));
    ui->tabWidget->addTab(network_tab.get(), tr("Network"));
    ui->tabWidget->addTab(applets_tab.get(), tr("Applets"));

    setFocusPolicy(Qt::ClickFocus);
    setWindowTitle(tr("Properties"));

    addons_tab->SetTitleId(title_id);

    scene = new QGraphicsScene;
    scene->setSceneRect(0, 0, 256, 256);
    ui->icon_view->setScene(scene);
    ui->icon_view->setStyleSheet(QStringLiteral("background: transparent; border: none;"));

    if (system.IsPoweredOn()) {
        QPushButton* apply_button = ui->buttonBox->addButton(QDialogButtonBox::Apply);
        connect(apply_button, &QAbstractButton::clicked, this,
                &ConfigurePerGame::HandleApplyButtonClicked);
    }
    
    connect(&metadata_watcher, &QFutureWatcher<AsyncMetadataResult>::finished, this, &ConfigurePerGame::AsyncMetadataLoaded);

    LoadConfiguration();
}

ConfigurePerGame::~ConfigurePerGame() = default;

void ConfigurePerGame::ApplyConfiguration() {
    for (const auto tab : *tab_group) {
        tab->ApplyConfiguration();
    }
    addons_tab->ApplyConfiguration();
    input_tab->ApplyConfiguration();
    network_tab->ApplyConfiguration();
    applets_tab->ApplyConfiguration();

    if (Settings::IsDockedMode() && Settings::values.players.GetValue()[0].controller_type ==
                                        Settings::ControllerType::Handheld) {
        Settings::values.use_docked_mode.SetValue(Settings::ConsoleMode::Handheld);
        Settings::values.use_docked_mode.SetGlobal(true);
    }

    system.ApplySettings();
    Settings::LogSettings();

    game_config->SaveAllValues();
}

void ConfigurePerGame::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QDialog::changeEvent(event);
}

void ConfigurePerGame::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigurePerGame::HandleApplyButtonClicked() {
    UISettings::values.configuration_applied = true;
    ApplyConfiguration();
}

void ConfigurePerGame::LoadFromFile(FileSys::VirtualFile file_) {
    file = std::move(file_);
    LoadConfiguration();
}

void ConfigurePerGame::LoadConfiguration() {
    if (file == nullptr) {
        return;
    }

    addons_tab->LoadFromFile(file);

    ui->display_title_id->setText(
        QStringLiteral("%1").arg(title_id, 16, 16, QLatin1Char{'0'}).toUpper());

    ui->display_version->setText(tr("Loading..."));
    ui->display_name->setPlainText(tr("Loading..."));
    ui->display_name->setAlignment(Qt::AlignLeft);
    ui->display_developer->setText(tr("Loading..."));
    
    auto file_copy = file;
    auto tid = title_id;
    auto* sys = &system;
    
    metadata_watcher.setFuture(QtConcurrent::run([tid, sys, file_copy]() -> AsyncMetadataResult {
        AsyncMetadataResult result;
        const FileSys::PatchManager pm{tid, sys->GetFileSystemController(), sys->GetContentProvider()};
        const auto control = pm.GetControlMetadata();
        const auto loader = Loader::GetLoader(*sys, file_copy);
        
        if (loader != nullptr) {
            result.format_string = Loader::GetFileTypeString(loader->GetFileType());
        }
        
        if (control.first != nullptr) {
            result.version_string = control.first->GetVersionString();
            result.application_name = control.first->GetApplicationName();
            result.developer_name = control.first->GetDeveloperName();
        } else {
            if (loader->ReadTitle(result.application_name) != Loader::ResultStatus::Success) {
                result.application_name = "Unknown";
            }
            FileSys::NACP nacp;
            if (loader->ReadControlData(nacp) == Loader::ResultStatus::Success) {
                result.developer_name = nacp.GetDeveloperName();
            }
            result.version_string = "1.0.0";
        }
        
        if (control.second != nullptr) {
            result.icon_bytes = control.second->ReadAllBytes();
        } else {
            loader->ReadIcon(result.icon_bytes);
        }
        
        return result;
    }));

    ui->display_filename->setPlainText(QString::fromStdString(file->GetName()));
    ui->display_filename->setAlignment(Qt::AlignLeft);

    ui->display_format->setText(tr("Loading..."));

    const auto valueText = ReadableByteSize(file->GetSize());
    ui->display_size->setText(valueText);
}

void ConfigurePerGame::AsyncMetadataLoaded() {
    const auto result = metadata_watcher.result();
    
    ui->display_version->setText(QString::fromStdString(result.version_string));
    ui->display_name->setPlainText(QString::fromStdString(result.application_name));
    ui->display_name->setAlignment(Qt::AlignLeft);
    ui->display_developer->setText(QString::fromStdString(result.developer_name));
    
    if (!result.format_string.empty()) {
        ui->display_format->setText(QString::fromStdString(result.format_string));
    }
    
    scene->clear();
    if (!result.icon_bytes.empty()) {
        QPixmap map;
        map.loadFromData(result.icon_bytes.data(), static_cast<u32>(result.icon_bytes.size()));
        scene->addPixmap(map.scaled(256, 256,
                                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    }
}
