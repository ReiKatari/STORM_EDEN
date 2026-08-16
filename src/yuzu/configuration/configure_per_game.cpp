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
#include <QRegularExpression>
#include <QString>
#include <QTabBar>
#include <QTimer>

#include "common/cityhash.h"
#include "common/fs/fs_util.h"
#include "common/fs/path_util.h"
#include "common/string_util.h"
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
#include "qt_common/qt_common.h"
#include "qt_common/util/vk.h"
#include "ui_configure_per_game.h"
#include "yuzu/configuration/configuration_shared.h"
#include "yuzu/configuration/configure_applets.h"
#include "yuzu/configuration/configure_audio.h"
#include "yuzu/configuration/configure_cpu.h"
#include "yuzu/configuration/configure_graphics.h"
#include "yuzu/configuration/configure_graphics_advanced.h"
#include "yuzu/configuration/configure_graphics_extensions.h"
#include "yuzu/configuration/configure_input_per_game.h"
#include "yuzu/configuration/configure_network.h"
#include "yuzu/configuration/configure_per_game.h"
#include "yuzu/configuration/configure_per_game_addons.h"
#include "yuzu/configuration/configure_system.h"
#include "yuzu/util/util.h"

ConfigurePerGame::ConfigurePerGame(QWidget* parent, u64 title_id_, const std::string& file_name,
                                   std::vector<VkDeviceInfo::Record>& vk_device_records,
                                   Core::System& system_)
    : QDialog(parent), ui(std::make_unique<Ui::ConfigurePerGame>()), title_id{title_id_},
      system{system_},
      builder{std::make_unique<ConfigurationShared::Builder>(this, !system_.IsPoweredOn())},
      tab_group{std::make_shared<std::vector<ConfigurationShared::Tab*>>()} {
    const auto file_path = std::filesystem::path(Common::FS::ToU8String(file_name));
    const auto file_path_hash = Common::CityHash64(file_name.data(), file_name.size());
    const auto specific_config = fmt::format("{:016X}_{:016X}", title_id, file_path_hash);
    const auto legacy_config = title_id == 0 ? Common::FS::PathToUTF8String(file_path.filename())
                                             : fmt::format("{:016X}", title_id);

    std::filesystem::path custom_path = Common::FS::GetEdenPath(Common::FS::EdenPath::ConfigDir) / "custom";
    std::string config_file_name = specific_config;
    if (!std::filesystem::exists(custom_path / (specific_config + ".ini")) &&
        std::filesystem::exists(custom_path / (legacy_config + ".ini"))) {
        std::error_code ec;
        std::filesystem::copy_file(custom_path / (legacy_config + ".ini"), custom_path / (specific_config + ".ini"), std::filesystem::copy_options::skip_existing, ec);
    }
    game_config = std::make_unique<QtConfig>(config_file_name, Config::ConfigType::PerGameConfig);
    addons_tab = std::make_unique<ConfigurePerGameAddons>(system_, this);
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

    ui->tabWidget->addTab(addons_tab.get(), tr("Add-Ons"));
    ui->tabWidget->addTab(system_tab.get(), tr("System"));
    ui->tabWidget->addTab(cpu_tab.get(), tr("ЦП"));
    ui->tabWidget->addTab(graphics_tab.get(), tr("Graphics"));
    ui->tabWidget->addTab(graphics_advanced_tab.get(), tr("Adv. Graphics"));
    ui->tabWidget->addTab(graphics_extensions_tab.get(), tr("Ext. Graphics"));
    ui->tabWidget->addTab(audio_tab.get(), tr("Audio"));
    ui->tabWidget->addTab(input_tab.get(), tr("Input Profiles"));
    ui->tabWidget->addTab(network_tab.get(), tr("Network"));
    ui->tabWidget->addTab(applets_tab.get(), tr("Applets"));

    setFocusPolicy(Qt::ClickFocus);
    setWindowTitle(tr("Параметры игры"));

    ui->display_name->setLineWrapMode(QTextEdit::WidgetWidth);
    ui->display_filename->setLineWrapMode(QTextEdit::WidgetWidth);
    ui->display_name->setAcceptRichText(false);
    ui->display_filename->setAcceptRichText(false);

    QFont bold_font = font();
    bold_font.setBold(true);
    ui->tabWidget->tabBar()->setFont(bold_font);
    ui->label->setFont(bold_font);
    ui->label_2->setFont(bold_font);
    ui->label_3->setFont(bold_font);
    ui->label_4->setFont(bold_font);
    ui->label_5->setFont(bold_font);
    ui->label_6->setFont(bold_font);
    ui->label_7->setFont(bold_font);

    addons_tab->SetTitleId(title_id);

    scene = new QGraphicsScene;
    ui->icon_view->setScene(scene);

    if (system.IsPoweredOn()) {
        QPushButton* apply_button = ui->buttonBox->addButton(QDialogButtonBox::Apply);
        connect(apply_button, &QAbstractButton::clicked, this,
                &ConfigurePerGame::HandleApplyButtonClicked);
    }

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

    const FileSys::PatchManager pm{title_id, system.GetFileSystemController(),
                                   system.GetContentProvider()};
    const auto control = pm.GetControlMetadata();
    const auto loader = Loader::GetLoader(system, file);

    QString title_text;
    QString dev_text;
    QString ver_str;

    FileSys::NACP file_nacp;
    if (loader != nullptr && loader->ReadControlData(file_nacp) == Loader::ResultStatus::Success) {
        title_text = QString::fromStdString(file_nacp.GetApplicationName());
        dev_text = QString::fromStdString(file_nacp.GetDeveloperName());
        const auto nacp_ver = file_nacp.GetVersionString();
        if (!nacp_ver.empty() && nacp_ver != "0") {
            ver_str = QString::fromStdString(nacp_ver);
        }
    }

    if (title_text.trimmed().isEmpty() && loader != nullptr) {
        std::string title;
        if (loader->ReadTitle(title) == Loader::ResultStatus::Success) {
            title_text = QString::fromStdString(title);
        }
    }

    if (title_text.trimmed().isEmpty()) {
        title_text = QString::fromStdString(file->GetName());
    }

    // Extract exact version from the file name if available, strictly excluding file sizes like (0.45 GB)
    static const QRegularExpression fn_ver_regex{QStringLiteral(R"((?:[\(\[\s]v?|\b)([0-9]+\.[0-9]+(?:\.[0-9]+)*)(?!\s*(?:GB|MB|KB|TB|ГБ|МБ|КБ|Б|B)\b))")};
    const auto m = fn_ver_regex.match(QString::fromStdString(file->GetName()));
    if (m.hasMatch() && m.hasCaptured(1)) {
        const QString parsed_ver = m.captured(1);
        if (!parsed_ver.isEmpty() && (ver_str.isEmpty() || ver_str == QStringLiteral("1.0.0"))) {
            ver_str = parsed_ver;
        }
    }

    if (ver_str.isEmpty()) {
        static const QRegularExpression fn_vnum_regex{QStringLiteral(R"(\[v([0-9]+)\])")};
        const auto vm = fn_vnum_regex.match(QString::fromStdString(file->GetName()));
        if (vm.hasMatch()) {
            const u32 vnum = vm.captured(1).toUInt();
            if (vnum == 0) {
                ver_str = QStringLiteral("1.0.0");
            } else {
                ver_str = QStringLiteral("v%1").arg(vnum);
            }
        }
    }

    if (ver_str.isEmpty()) {
        ver_str = QStringLiteral("1.0.0");
    }

    ui->display_name->setPlainText(title_text);
    ui->display_developer->setText(dev_text);
    ui->display_version->setText(ver_str);

    if (control.second != nullptr) {
        scene->clear();

        QPixmap map;
        const auto bytes = control.second->ReadAllBytes();
        map.loadFromData(bytes.data(), static_cast<u32>(bytes.size()));

        const int icon_dim = (std::max)({ui->icon_view->width(), ui->icon_view->height(), 240});
        scene->addPixmap(map.scaled(icon_dim, icon_dim,
                                    Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        std::vector<u8> bytes;
        if (loader->ReadIcon(bytes) == Loader::ResultStatus::Success) {
            scene->clear();

            QPixmap map;
            map.loadFromData(bytes.data(), static_cast<u32>(bytes.size()));

            const int icon_dim = (std::max)({ui->icon_view->width(), ui->icon_view->height(), 240});
            scene->addPixmap(map.scaled(icon_dim, icon_dim,
                                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }

    ui->display_filename->setPlainText(QString::fromStdString(file->GetName()));

    QString format_str = QString::fromStdString(Loader::GetFileTypeString(loader ? loader->GetFileType() : Loader::FileType::Unknown));
    const auto ext = Common::ToLower(std::string(Common::FS::GetExtensionFromFilename(file->GetName())));
    if (ext == "nsz") {
        format_str = QStringLiteral("NSZ");
    } else if (ext == "xcz") {
        format_str = QStringLiteral("XCZ");
    }
    ui->display_format->setText(format_str);

    const auto valueText = QtCommon::ReadableByteSize(file->GetSize());
    ui->display_size->setText(valueText);
}
