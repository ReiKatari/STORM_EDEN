// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "eden/configuration/configure_auto_optimizer.h"
#include "ui_configure_auto_optimizer.h"
#include "common/settings.h"
#include "qt_common/config/qt_config.h"
#include <QTimer>
#include <QVBoxLayout>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>

ConfigureAutoOptimizer::ConfigureAutoOptimizer(QWidget* parent, const std::vector<u64>& title_ids_, const std::vector<std::string>& game_names_)
    : QDialog(parent), ui(std::make_unique<Ui::ConfigureAutoOptimizer>()), title_ids(title_ids_), game_names(game_names_) {
    
    ui->setupUi(this);
    SetupUI();
    
    this->setWindowTitle(QStringLiteral("Авто-Оптимизатор"));
    if (ui->buttonBox->button(QDialogButtonBox::Close)) {
        ui->buttonBox->button(QDialogButtonBox::Close)->setText(QStringLiteral("Закрыть"));
    }
    hw_info = Util::HardwareAnalyzer::GetHardwareInfo();
    
    // Start optimization with a slight delay for UI to appear
    QTimer::singleShot(500, this, &ConfigureAutoOptimizer::StartOptimization);
}

ConfigureAutoOptimizer::~ConfigureAutoOptimizer() = default;

void ConfigureAutoOptimizer::SetupUI() {
    ui->label_title->setText(QStringLiteral("STORM EDEN Авто-Оптимизатор"));
    QIcon info_icon = QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation);
    ui->label_icon->setPixmap(info_icon.pixmap(48, 48));
}

void ConfigureAutoOptimizer::StartOptimization() {
    int total = static_cast<int>(title_ids.size());
    QString final_log;
    
    for (int i = 0; i < total; ++i) {
        u64 tid = title_ids[i];
        std::string name = game_names[i];
        
        ui->label_log->setText(tr("Оптимизируем %1...").arg(QString::fromStdString(name)));
        ui->progressBar->setValue((i * 100) / total);
        
        // Small delay to simulate processing for visual feedback
        QCoreApplication::processEvents();
        
        QString game_log = ApplySettingsForGame(tid, name);
        if (total == 1) {
            final_log = game_log;
        } else {
            final_log += QString::fromStdString(name) + QStringLiteral(":\n") + game_log + QStringLiteral("\n");
        }
    }
    
    ui->progressBar->setValue(100);
    ui->label_title->setText(QStringLiteral("Оптимизация завершена!"));
    ui->label_log->setText(final_log);
    ui->buttonBox->setEnabled(true);
}

QString ConfigureAutoOptimizer::ApplySettingsForGame(u64 title_id, const std::string& game_name) {
    QString log_output;

    // Determine target settings based on hardware tier and game
    Settings::ResolutionSetup target_res = Settings::ResolutionSetup::Res1X;
    QString res_str = QStringLiteral("1X");
    if (hw_info.tier >= Util::HardwareTier::HighEnd) {
        target_res = Settings::ResolutionSetup::Res2X;
        res_str = QStringLiteral("2X");
    }
    if (hw_info.tier == Util::HardwareTier::Enthusiast) {
        target_res = Settings::ResolutionSetup::Res3X;
        res_str = QStringLiteral("3X");
    }
    
    // Load per-game config
    QtConfig config(fmt::format("{:016X}", title_id), QtConfig::ConfigType::PerGameConfig);
    
    // Apply Settings
    Settings::values.vulkan_device.SetGlobal(true); // Let it use global device
    Settings::values.renderer_backend.SetGlobal(false);
    Settings::values.renderer_backend.SetValue(Settings::RendererBackend::Vulkan);
    log_output += QStringLiteral("• API: Vulkan\n");
    
    Settings::values.use_asynchronous_shaders.SetGlobal(false);
    Settings::values.use_asynchronous_shaders.SetValue(true);
    log_output += QStringLiteral("• Асинхронные шейдеры: Вкл\n");
    
    Settings::values.resolution_setup.SetGlobal(false);
    Settings::values.resolution_setup.SetValue(target_res);
    log_output += QStringLiteral("• Разрешение: ") + res_str + QStringLiteral("\n");
    
    // Zelda TOTK specific optimizations (0100F2C0115B6000)
    if (title_id == 0x0100F2C0115B6000ULL) {
        Settings::values.gpu_accuracy.SetGlobal(false);
        Settings::values.gpu_accuracy.SetValue(Settings::GpuAccuracy::Medium);
        log_output += QStringLiteral("• Точность GPU: Средняя (TOTK Fix)\n");
        
        Settings::values.memory_layout_mode.SetGlobal(false);
        Settings::values.memory_layout_mode.SetValue(Settings::MemoryLayout::Memory_6Gb); // Requires more memory
        log_output += QStringLiteral("• Объем памяти: 6 ГБ (TOTK Fix)\n");
    }
    
    config.SaveAllValues();
    return log_output;
}
