// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QAbstractTextDocumentLayout>
#include <QDesktopServices>
#include <QIcon>
#include <QPainter>
#include <QTableWidget>
#include <QTextEdit>
#include <fmt/ranges.h>
#include "dep_hashes.h"
#include "ui_deps_dialog.h"
#include "yuzu/deps_dialog.h"

DepsDialog::DepsDialog(QWidget* parent) : QDialog(parent), ui{std::make_unique<Ui::DepsDialog>()} {
    ui->setupUi(this);

    struct ProjectEntry {
        QString name;
        QString url;
        QString desc;
    };

    const std::vector<ProjectEntry> acknowledgements = {
        {QStringLiteral("Eden Emulator Project & Camille LaVey"), QStringLiteral("https://git.eden-emu.dev/eden-emu/eden"), QStringLiteral("Базовый форк эмулятора Switch")},
        {QStringLiteral("Yuzu Emulator Team"), QStringLiteral("https://yuzu-emu.org"), QStringLiteral("Архитектура ядра эмуляции Switch")},
        {QStringLiteral("Ryujinx Team"), QStringLiteral("https://ryujinx.org"), QStringLiteral("Исследования FS и Horizon OS")},
        {QStringLiteral("Sudachi & Citron Projects"), QStringLiteral("https://github.com/sudachi-emu"), QStringLiteral("Оптимизации производительности")},
        {QStringLiteral("Tinfoil & Blawar"), QStringLiteral("https://tinfoil.io"), QStringLiteral("Формат NSZ/NCZ и база TitleDB")},
        {QStringLiteral("Zstandard (zstd) / Yann Collet"), QStringLiteral("https://github.com/facebook/zstd"), QStringLiteral("Библиотека сжатия ZSTD (Meta)")},
        {QStringLiteral("FFmpeg Project"), QStringLiteral("https://ffmpeg.org"), QStringLiteral("Декодер NVDEC видео/аудио")},
        {QStringLiteral("Dynarmic (MerryMage)"), QStringLiteral("https://github.com/merryhime/dynarmic"), QStringLiteral("JIT-рекомпилятор ARMv8")},
        {QStringLiteral("Mozilla Cubeb"), QStringLiteral("https://github.com/mozilla/cubeb"), QStringLiteral("Кросс-платформенный звук")},
        {QStringLiteral("Qt Project & The Qt Company"), QStringLiteral("https://www.qt.io"), QStringLiteral("Графический интерфейс Qt6")},
        {QStringLiteral("Vulkan SDK & LunarG"), QStringLiteral("https://vulkan.lunarg.com"), QStringLiteral("Графический API Vulkan")},
    };

    const int total_rows = static_cast<int>(acknowledgements.size() + Common::dep_hashes.size());
    ui->tableDeps->setRowCount(total_rows);

    QStringList labels;
    labels << tr("Проект / Зависимость") << tr("Назначение / Версия");

    ui->tableDeps->setHorizontalHeaderLabels(labels);
    ui->tableDeps->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeMode::Stretch);
    ui->tableDeps->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeMode::Interactive);
    ui->tableDeps->horizontalHeader()->setMinimumSectionSize(220);

    int row = 0;
    for (const auto& ack : acknowledgements) {
        std::string link = fmt::format("<a href=\"{}\"><b>{}</b></a>", ack.url.toStdString(), ack.name.toStdString());
        QTableWidgetItem* nameItem = new QTableWidgetItem(QString::fromStdString(link));
        QTableWidgetItem* descItem = new QTableWidgetItem(ack.desc);

        ui->tableDeps->setItem(row, 0, nameItem);
        ui->tableDeps->setItem(row, 1, descItem);
        row++;
    }

    for (std::size_t i = 0; i < Common::dep_hashes.size(); ++i) {
        const std::string name = Common::dep_names.at(i);
        const std::string sha = Common::dep_hashes.at(i);
        const std::string url = Common::dep_urls.at(i);

        std::string dependency = fmt::format("<a href=\"{}\">{}</a>", url, name);

        QTableWidgetItem* nameItem = new QTableWidgetItem(QString::fromStdString(dependency));
        QTableWidgetItem* shaItem = new QTableWidgetItem(QString::fromStdString(sha));

        ui->tableDeps->setItem(row, 0, nameItem);
        ui->tableDeps->setItem(row, 1, shaItem);
        row++;
    }

    ui->tableDeps->setItemDelegateForColumn(0, new LinkItemDelegate(this));
}

DepsDialog::~DepsDialog() = default;

LinkItemDelegate::LinkItemDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

void LinkItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const {
    auto options = option;
    initStyleOption(&options, index);

    QTextDocument doc;
    QString html = index.data(Qt::DisplayRole).toString();
    doc.setHtml(html);

    options.text.clear();

    painter->save();
    painter->translate(options.rect.topLeft());
    doc.drawContents(painter, QRectF(0, 0, options.rect.width(), options.rect.height()));
    painter->restore();
}

QSize LinkItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const {
    QStyleOptionViewItem options = option;
    initStyleOption(&options, index);

    QTextDocument doc;
    doc.setHtml(options.text);
    doc.setTextWidth(options.rect.width());
    return QSize(doc.idealWidth(), doc.size().height());
}

bool LinkItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                   const QStyleOptionViewItem& option, const QModelIndex& index) {
    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QString html = index.data(Qt::DisplayRole).toString();
            QTextDocument doc;
            doc.setHtml(html);
            doc.setTextWidth(option.rect.width());

            // this is kinda silly but it werks
            QAbstractTextDocumentLayout* layout = doc.documentLayout();

            QPoint pos = mouseEvent->pos() - option.rect.topLeft();
            int charPos = layout->hitTest(pos, Qt::ExactHit);

            if (charPos >= 0) {
                QTextCursor cursor(&doc);
                cursor.setPosition(charPos);

                QTextCharFormat format = cursor.charFormat();

                if (format.isAnchor()) {
                    QString href = format.anchorHref();
                    if (!href.isEmpty()) {
                        QDesktopServices::openUrl(QUrl(href));
                        return true;
                    }
                }
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
