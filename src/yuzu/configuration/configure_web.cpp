// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2017 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QIcon>
#include <QMessageBox>
#include "yuzu/configuration/configure_web.h"

#if QT_VERSION_MAJOR >= 6
#include <QRegularExpressionValidator>
#else
#include <QRegExpValidator>
#endif

#include <QtConcurrentRun>
#include "common/settings.h"
#include "qt_common/config/uisettings.h"
#include "ui_configure_web.h"

ConfigureWeb::ConfigureWeb(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureWeb>()), m_rng{QRandomGenerator::system()} {
    ui->setupUi(this);

    QString user_regex = QStringLiteral(".{4,20}");
    QString token_regex = QStringLiteral("[a-z]{48}");

#if QT_VERSION_MAJOR >= 6
    QRegularExpressionValidator* username_validator = new QRegularExpressionValidator(this);
    QRegularExpressionValidator* token_validator = new QRegularExpressionValidator(this);

    username_validator->setRegularExpression(QRegularExpression(user_regex));
    token_validator->setRegularExpression(QRegularExpression(token_regex));
#else
    QRegExpValidator* username_validator = new QRegExpValidator(this);
    QRegExpValidator* token_validator = new QRegExpValidator(this);

    username_validator->setRegExp(QRegExp(user_regex));
    token_validator->setRegExp(QRegExp(token_regex));
#endif

    ui->edit_username->setValidator(username_validator);
    ui->edit_token->setValidator(token_validator);

    connect(ui->button_generate, &QPushButton::clicked, this, &ConfigureWeb::GenerateToken);

#ifndef USE_DISCORD_PRESENCE
    ui->discord_group->setVisible(false);
#endif

    SetConfiguration();
    RetranslateUI();
}

ConfigureWeb::~ConfigureWeb() = default;

void ConfigureWeb::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureWeb::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureWeb::SetConfiguration() {
    connect(ui->edit_username, &QLineEdit::textChanged, this, &ConfigureWeb::VerifyLogin);
    connect(ui->edit_token, &QLineEdit::textChanged, this, &ConfigureWeb::VerifyLogin);

    ui->edit_username->setText(QString::fromStdString(Settings::values.eden_username.GetValue()));
    ui->edit_token->setText(QString::fromStdString(Settings::values.eden_token.GetValue()));

    VerifyLogin();

    ui->toggle_discordrpc->setChecked(UISettings::values.enable_discord_presence.GetValue());
}

void ConfigureWeb::GenerateToken() {
    constexpr size_t length = 48;
    QString set = QStringLiteral("abcdefghijklmnopqrstuvwxyz");
    QString result;

    for (size_t i = 0; i < length; ++i) {
        size_t idx = m_rng->bounded(set.length());
        result.append(set.at(idx));
    }

    ui->edit_token->setText(result);
}

void ConfigureWeb::ApplyConfiguration() {
    UISettings::values.enable_discord_presence = ui->toggle_discordrpc->isChecked();
    Settings::values.eden_username = ui->edit_username->text().toStdString();
    Settings::values.eden_token = ui->edit_token->text().toStdString();
}

#include <QPainter>

static QPixmap CreateStatusBadge(bool ok) {
    QPixmap pix(20, 20);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    if (ok) {
        p.setPen(QPen(QColor(16, 185, 129), 1.5));
        p.setBrush(QColor(16, 185, 129, 220));
        p.drawEllipse(2, 2, 16, 16);

        p.setPen(QPen(Qt::white, 2.0));
        p.drawLine(5, 10, 8, 14);
        p.drawLine(8, 14, 15, 6);
    } else {
        p.setPen(QPen(QColor(239, 68, 68), 1.5));
        p.setBrush(QColor(239, 68, 68, 220));
        p.drawEllipse(2, 2, 16, 16);

        p.setPen(QPen(Qt::white, 2.0));
        p.drawLine(6, 6, 14, 14);
        p.drawLine(14, 6, 6, 14);
    }
    return pix;
}

void ConfigureWeb::VerifyLogin() {
    const bool username_good = ui->edit_username->hasAcceptableInput();
    const bool token_good = ui->edit_token->hasAcceptableInput();

    ui->label_username_verified->setPixmap(CreateStatusBadge(username_good));
    if (username_good) {
        ui->label_username_verified->setToolTip(tr("Имя пользователя принято", "Tooltip"));
    } else {
        ui->label_username_verified->setToolTip(tr("Имя пользователя должно быть от 4 до 20 символов", "Tooltip"));
    }

    ui->label_token_verified->setPixmap(CreateStatusBadge(token_good));
    if (token_good) {
        ui->label_token_verified->setToolTip(tr("Токен принят и действителен", "Tooltip"));
    } else {
        ui->label_token_verified->setToolTip(
            tr("Токен должен содержать 48 строчных символов (a-z)", "Tooltip"));
    }
}

void ConfigureWeb::SetWebServiceConfigEnabled(bool enabled) {
    ui->label_disable_info->setVisible(!enabled);
    ui->groupBoxWebConfig->setEnabled(enabled);
}
