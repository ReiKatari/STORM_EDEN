// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/amiibo_browser_dialog.h"

#include <algorithm>
#include <filesystem>
#include <random>
#include <QBoxLayout>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QTextEdit>
#include <QUrl>

#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "core/core.h"

AmiiboBrowserDialog::AmiiboBrowserDialog(QWidget* parent, Core::System& system)
    : QDialog(parent), m_system(system), m_network_mgr(new QNetworkAccessManager(this)) {
    SetupUi();
    FetchAmiiboDatabase();
}

AmiiboBrowserDialog::~AmiiboBrowserDialog() = default;

void AmiiboBrowserDialog::SetupUi() {
    setWindowTitle(tr("Онлайн-база и менеджер Amiibo — STORM EDEN"));
    resize(980, 680);
    setMinimumSize(850, 560);

    // Apply dark cyber styling
    setStyleSheet(QStringLiteral(
        "QDialog {"
        "  background-color: #0b0e14;"
        "  color: #e2e8f0;"
        "}"
        "QGroupBox {"
        "  font-weight: bold;"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 6px;"
        "  margin-top: 10px;"
        "  padding-top: 14px;"
        "  background-color: #111622;"
        "  color: #00f0ff;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  subcontrol-position: top left;"
        "  padding: 0 6px;"
        "  color: #00f0ff;"
        "}"
        "QLineEdit, QComboBox {"
        "  background-color: #161d2d;"
        "  border: 1px solid rgba(255, 255, 255, 0.15);"
        "  border-radius: 4px;"
        "  padding: 5px 8px;"
        "  color: #ffffff;"
        "  font-size: 9pt;"
        "}"
        "QLineEdit:focus, QComboBox:focus {"
        "  border: 1px solid #00f0ff;"
        "  background-color: #1a2336;"
        "}"
        "QListWidget {"
        "  background-color: #111622;"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 6px;"
        "  color: #ffffff;"
        "  padding: 4px;"
        "}"
        "QListWidget::item {"
        "  padding: 6px 10px;"
        "  border-radius: 4px;"
        "  margin-bottom: 2px;"
        "}"
        "QListWidget::item:hover {"
        "  background-color: rgba(0, 240, 255, 0.12);"
        "  color: #00f0ff;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: rgba(0, 240, 255, 0.25);"
        "  color: #ffffff;"
        "  border: 1px solid #00f0ff;"
        "}"
        "QPushButton {"
        "  background-color: #1a2336;"
        "  border: 1px solid rgba(0, 240, 255, 0.35);"
        "  border-radius: 5px;"
        "  color: #ffffff;"
        "  padding: 6px 14px;"
        "  font-weight: bold;"
        "  font-size: 8.5pt;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(0, 240, 255, 0.20);"
        "  border-color: #00f0ff;"
        "  color: #00f0ff;"
        "}"
        "QPushButton:pressed {"
        "  background-color: #00f0ff;"
        "  color: #000000;"
        "}"
        "QTextEdit {"
        "  background-color: #161d2d;"
        "  border: 1px solid rgba(255, 255, 255, 0.10);"
        "  border-radius: 4px;"
        "  color: #cbd5e1;"
        "  font-size: 8.5pt;"
        "}"
        "QProgressBar {"
        "  border: 1px solid rgba(255, 255, 255, 0.12);"
        "  border-radius: 3px;"
        "  text-align: center;"
        "  background-color: #111622;"
        "  color: #ffffff;"
        "  max-height: 14px;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #00f0ff;"
        "  border-radius: 2px;"
        "}"
    ));

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(8);

    // Top Filter Bar
    auto* top_filter_box = new QWidget(this);
    auto* filter_layout = new QHBoxLayout(top_filter_box);
    filter_layout->setContentsMargins(0, 0, 0, 0);
    filter_layout->setSpacing(8);

    m_search_edit = new QLineEdit(this);
    m_search_edit->setPlaceholderText(tr("🔍 Поиск по имени фигурки или игровой серии (напр. Zelda, Mario, Samus)..."));
    m_search_edit->setClearButtonEnabled(true);
    connect(m_search_edit, &QLineEdit::textChanged, this, &AmiiboBrowserDialog::OnSearchFilterChanged);
    filter_layout->addWidget(m_search_edit, 3);

    m_series_combo = new QComboBox(this);
    m_series_combo->addItem(tr("Все серии"));
    connect(m_series_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AmiiboBrowserDialog::OnSearchFilterChanged);
    filter_layout->addWidget(m_series_combo, 2);

    m_type_combo = new QComboBox(this);
    m_type_combo->addItems({tr("Все типы"), tr("Figure (Фигурка)"), tr("Card (Карта)"), tr("Yarn (Пряжа)")});
    connect(m_type_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AmiiboBrowserDialog::OnSearchFilterChanged);
    filter_layout->addWidget(m_type_combo, 1);

    m_refresh_btn = new QPushButton(tr("🔄 Обновить базу"), this);
    connect(m_refresh_btn, &QPushButton::clicked, this, &AmiiboBrowserDialog::OnRefreshClicked);
    filter_layout->addWidget(m_refresh_btn, 0);

    main_layout->addWidget(top_filter_box);

    // Splitter with List (left) and Inspector (right)
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);

    // Left container: Amiibo List + count
    auto* left_container = new QWidget(this);
    auto* left_layout = new QVBoxLayout(left_container);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(4);

    m_amiibo_list = new QListWidget(this);
    connect(m_amiibo_list, &QListWidget::currentItemChanged, this, &AmiiboBrowserDialog::OnItemSelected);
    left_layout->addWidget(m_amiibo_list);

    splitter->addWidget(left_container);

    // Right container: Inspector / Details
    auto* right_container = new QWidget(this);
    auto* right_layout = new QVBoxLayout(right_container);
    right_layout->setContentsMargins(8, 0, 0, 0);
    right_layout->setSpacing(8);

    auto* details_group = new QGroupBox(tr("Информация об Amiibo"), this);
    auto* details_layout = new QVBoxLayout(details_group);
    details_layout->setContentsMargins(10, 14, 10, 10);
    details_layout->setSpacing(6);

    // Image preview centered
    m_image_label = new QLabel(this);
    m_image_label->setMinimumSize(180, 180);
    m_image_label->setMaximumHeight(220);
    m_image_label->setAlignment(Qt::AlignCenter);
    m_image_label->setStyleSheet(QStringLiteral("background-color: #0c1018; border: 1px solid rgba(255,255,255,0.08); border-radius: 8px;"));
    m_image_label->setText(tr("Выберите Amiibo для просмотра"));
    details_layout->addWidget(m_image_label);

    m_name_label = new QLabel(tr("Имя: —"), this);
    m_name_label->setStyleSheet(QStringLiteral("font-size: 11pt; font-weight: bold; color: #00f0ff;"));
    details_layout->addWidget(m_name_label);

    m_series_label = new QLabel(tr("Серия Amiibo: —"), this);
    details_layout->addWidget(m_series_label);

    m_game_series_label = new QLabel(tr("Игровая вселенная: —"), this);
    details_layout->addWidget(m_game_series_label);

    m_type_label = new QLabel(tr("Тип: —"), this);
    details_layout->addWidget(m_type_label);

    m_id_label = new QLabel(tr("ID (Head/Tail): —"), this);
    m_id_label->setStyleSheet(QStringLiteral("color: #718096; font-family: monospace; font-size: 8pt;"));
    details_layout->addWidget(m_id_label);

    m_status_badge = new QLabel(tr("Статус: Ожидание"), this);
    m_status_badge->setStyleSheet(QStringLiteral("color: #a0aec0; font-weight: bold; padding: 2px 6px; background-color: #1a2336; border-radius: 3px;"));
    details_layout->addWidget(m_status_badge);

    // Games compatibility
    auto* games_label = new QLabel(tr("🎮 Поддерживаемые игры на Nintendo Switch:"), this);
    games_label->setStyleSheet(QStringLiteral("font-weight: bold; color: #ffca28; margin-top: 4px;"));
    details_layout->addWidget(games_label);

    m_games_text = new QTextEdit(this);
    m_games_text->setReadOnly(true);
    m_games_text->setPlaceholderText(tr("Список совместимых игр загружается или отсутствует"));
    details_layout->addWidget(m_games_text, 1);

    // Action buttons inside details
    auto* act_btn_layout = new QHBoxLayout();
    act_btn_layout->setSpacing(6);

    m_save_btn = new QPushButton(tr("💾 Сохранить Amiibo (.bin)"), this);
    m_save_btn->setCursor(Qt::PointingHandCursor);
    m_save_btn->setEnabled(false);
    m_save_btn->setStyleSheet(QStringLiteral("background-color: #004d40; border-color: #00e676; color: #ffffff;"));
    connect(m_save_btn, &QPushButton::clicked, this, &AmiiboBrowserDialog::OnSaveAmiiboClicked);
    act_btn_layout->addWidget(m_save_btn);

    m_load_btn = new QPushButton(tr("⚡ Загрузить в игру"), this);
    m_load_btn->setCursor(Qt::PointingHandCursor);
    m_load_btn->setEnabled(false);
    m_load_btn->setStyleSheet(QStringLiteral("background-color: #006064; border-color: #00e5ff; color: #ffffff;"));
    connect(m_load_btn, &QPushButton::clicked, this, &AmiiboBrowserDialog::OnLoadAmiiboClicked);
    act_btn_layout->addWidget(m_load_btn);

    details_layout->addLayout(act_btn_layout);

    right_layout->addWidget(details_group);
    splitter->addWidget(right_container);

    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 6);
    main_layout->addWidget(splitter, 1);

    // Bottom Status & Controls
    auto* bottom_bar = new QHBoxLayout();
    bottom_bar->setContentsMargins(0, 0, 0, 0);

    m_status_label = new QLabel(tr("Подключение к базе Amiibo..."), this);
    m_status_label->setStyleSheet(QStringLiteral("color: #718096; font-size: 8.5pt;"));
    bottom_bar->addWidget(m_status_label, 1);

    m_progress_bar = new QProgressBar(this);
    m_progress_bar->setRange(0, 0); // Busy indicator initially
    m_progress_bar->setFixedWidth(140);
    bottom_bar->addWidget(m_progress_bar);

    m_open_folder_btn = new QPushButton(tr("📁 Открыть папку Amiibo"), this);
    connect(m_open_folder_btn, &QPushButton::clicked, this, &AmiiboBrowserDialog::OnOpenAmiiboFolderClicked);
    bottom_bar->addWidget(m_open_folder_btn);

    auto* close_btn = new QPushButton(tr("Закрыть"), this);
    connect(close_btn, &QPushButton::clicked, this, &QDialog::accept);
    bottom_bar->addWidget(close_btn);

    main_layout->addLayout(bottom_bar);
}

void AmiiboBrowserDialog::FetchAmiiboDatabase() {
    m_status_label->setText(tr("Загрузка каталога Amiibo из сети..."));
    m_progress_bar->setVisible(true);
    m_progress_bar->setRange(0, 0);

    const auto cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir);
    const auto cache_file = cache_dir / "amiibo_cache.json";

    // Try reading cache if available first to show immediately
    if (std::filesystem::exists(cache_file)) {
        QFile file(QString::fromStdString(cache_file.string()));
        if (file.open(QIODevice::ReadOnly)) {
            ParseDatabaseJson(file.readAll());
            file.close();
            m_status_label->setText(tr("Загружено из кэша: %1 Amiibo. Обновление из сети...").arg(m_all_amiibos.size()));
        }
    }

    QUrl url(QStringLiteral("https://www.amiiboapi.com/api/amiibo/"));
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto* reply = m_network_mgr->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, cache_file]() {
        m_progress_bar->setVisible(false);
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            // Cache response to disk
            QFile file(QString::fromStdString(cache_file.string()));
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
            }
            ParseDatabaseJson(data);
            m_status_label->setText(tr("Каталог успешно обновлен. Всего доступно: %1 Amiibo").arg(m_all_amiibos.size()));
        } else {
            if (m_all_amiibos.empty()) {
                m_status_label->setText(tr("Не удалось загрузить каталог Amiibo: %1").arg(reply->errorString()));
            } else {
                m_status_label->setText(tr("Работа в автономном режиме. Доступно из кэша: %1 Amiibo").arg(m_all_amiibos.size()));
            }
        }
        reply->deleteLater();
    });
}

void AmiiboBrowserDialog::ParseDatabaseJson(const QByteArray& json_data) {
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(json_data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray amiibo_array = root.value(QStringLiteral("amiibo")).toArray();
    if (amiibo_array.isEmpty()) {
        return;
    }

    m_all_amiibos.clear();
    m_all_amiibos.reserve(amiibo_array.size());

    for (const auto& val : amiibo_array) {
        QJsonObject obj = val.toObject();
        AmiiboEntry entry;
        entry.character = obj.value(QStringLiteral("character")).toString();
        entry.name = obj.value(QStringLiteral("name")).toString();
        entry.game_series = obj.value(QStringLiteral("gameSeries")).toString();
        entry.amiibo_series = obj.value(QStringLiteral("amiiboSeries")).toString();
        entry.type = obj.value(QStringLiteral("type")).toString();
        entry.head = obj.value(QStringLiteral("head")).toString();
        entry.tail = obj.value(QStringLiteral("tail")).toString();
        entry.image_url = obj.value(QStringLiteral("image")).toString();

        QJsonObject rel = obj.value(QStringLiteral("release")).toObject();
        entry.release_na = rel.value(QStringLiteral("na")).toString();
        entry.release_jp = rel.value(QStringLiteral("jp")).toString();
        entry.release_eu = rel.value(QStringLiteral("eu")).toString();

        QJsonArray switch_games = obj.value(QStringLiteral("gamesSwitch")).toArray();
        for (const auto& g : switch_games) {
            QJsonObject g_obj = g.toObject();
            QString g_name = g_obj.value(QStringLiteral("gameName")).toString();
            QJsonArray usages = g_obj.value(QStringLiteral("amiiboUsage")).toArray();
            QString usage_str;
            for (const auto& u : usages) {
                usage_str += u.toObject().value(QStringLiteral("Usage")).toString() + QStringLiteral("; ");
            }
            if (!usage_str.isEmpty()) {
                entry.switch_games.append(QStringLiteral("• %1: %2").arg(g_name, usage_str));
            } else {
                entry.switch_games.append(QStringLiteral("• %1").arg(g_name));
            }
        }

        m_all_amiibos.push_back(std::move(entry));
    }

    PopulateSeriesFilter();
    ApplyFilters();
}

void AmiiboBrowserDialog::PopulateSeriesFilter() {
    QString current_series = m_series_combo->currentText();
    m_series_combo->blockSignals(true);
    m_series_combo->clear();
    m_series_combo->addItem(tr("Все серии"));

    QSet<QString> series_set;
    for (const auto& a : m_all_amiibos) {
        if (!a.amiibo_series.isEmpty()) {
            series_set.insert(a.amiibo_series);
        }
    }
    QStringList series_list = series_set.values();
    series_list.sort();
    for (const auto& s : series_list) {
        m_series_combo->addItem(s);
    }

    int idx = m_series_combo->findText(current_series);
    if (idx >= 0) {
        m_series_combo->setCurrentIndex(idx);
    }
    m_series_combo->blockSignals(false);
}

void AmiiboBrowserDialog::OnSearchFilterChanged() {
    ApplyFilters();
}

void AmiiboBrowserDialog::ApplyFilters() {
    m_amiibo_list->clear();
    m_filtered_indices.clear();

    const QString search_text = m_search_edit->text().trimmed().toLower();
    const QString series_filter = m_series_combo->currentText();
    const int type_idx = m_type_combo->currentIndex();

    for (size_t i = 0; i < m_all_amiibos.size(); ++i) {
        const auto& a = m_all_amiibos[i];

        if (m_series_combo->currentIndex() > 0 && a.amiibo_series != series_filter) {
            continue;
        }

        if (type_idx == 1 && !a.type.contains(QStringLiteral("Figure"), Qt::CaseInsensitive)) continue;
        if (type_idx == 2 && !a.type.contains(QStringLiteral("Card"), Qt::CaseInsensitive)) continue;
        if (type_idx == 3 && !a.type.contains(QStringLiteral("Yarn"), Qt::CaseInsensitive)) continue;

        if (!search_text.isEmpty()) {
            bool matches = a.name.toLower().contains(search_text) ||
                           a.character.toLower().contains(search_text) ||
                           a.game_series.toLower().contains(search_text) ||
                           a.amiibo_series.toLower().contains(search_text);
            if (!matches) {
                continue;
            }
        }

        m_filtered_indices.push_back(static_cast<int>(i));

        auto* item = new QListWidgetItem(QStringLiteral("%1 [%2]").arg(a.name, a.amiibo_series));
        item->setData(Qt::UserRole, static_cast<int>(i));
        m_amiibo_list->addItem(item);
    }

    if (m_amiibo_list->count() > 0) {
        m_amiibo_list->setCurrentRow(0);
    } else {
        m_name_label->setText(tr("Ничего не найдено"));
        m_series_label->setText(QString());
        m_game_series_label->setText(QString());
        m_type_label->setText(QString());
        m_id_label->setText(QString());
        m_status_badge->setText(tr("Статус: Нет результатов"));
        m_games_text->clear();
        m_image_label->setText(tr("Нет данных"));
        m_save_btn->setEnabled(false);
        m_load_btn->setEnabled(false);
    }
}

void AmiiboBrowserDialog::OnItemSelected(QListWidgetItem* current, QListWidgetItem* previous) {
    if (!current) return;
    int idx = current->data(Qt::UserRole).toInt();
    if (idx >= 0 && idx < static_cast<int>(m_all_amiibos.size())) {
        DisplayAmiiboDetails(m_all_amiibos[idx]);
    }
}

void AmiiboBrowserDialog::DisplayAmiiboDetails(const AmiiboEntry& entry) {
    m_name_label->setText(tr("Имя: %1 (%2)").arg(entry.name, entry.character));
    m_series_label->setText(tr("Серия Amiibo: %1").arg(entry.amiibo_series));
    m_game_series_label->setText(tr("Игровая вселенная: %1").arg(entry.game_series));
    m_type_label->setText(tr("Тип: %1").arg(entry.type));
    m_id_label->setText(tr("Model ID: Head %1 | Tail %2").arg(entry.head, entry.tail));

    // Check if file already exists locally
    const auto amiibo_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::AmiiboDir);
    QString clean_series = entry.amiibo_series;
    clean_series.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
    QString clean_name = entry.name;
    clean_name.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
    auto local_path = amiibo_dir / clean_series.toStdString() / (clean_name.toStdString() + ".bin");

    if (std::filesystem::exists(local_path)) {
        m_status_badge->setText(tr("Статус: ✅ Установлено локально (%1)").arg(clean_name + QStringLiteral(".bin")));
        m_status_badge->setStyleSheet(QStringLiteral("color: #00e676; font-weight: bold; padding: 2px 6px; background-color: #004d40; border-radius: 3px;"));
    } else {
        m_status_badge->setText(tr("Статус: 🌐 Доступно для загрузки"));
        m_status_badge->setStyleSheet(QStringLiteral("color: #00e5ff; font-weight: bold; padding: 2px 6px; background-color: #006064; border-radius: 3px;"));
    }

    // Switch games list
    if (entry.switch_games.isEmpty()) {
        m_games_text->setPlainText(tr("Совместимо с универсальными играми Nintendo Switch (Super Smash Bros., Zelda, Mario Kart 8, и др.)."));
    } else {
        m_games_text->setPlainText(entry.switch_games.join(QStringLiteral("\n\n")));
    }

    // Fetch and display image
    FetchImage(entry.image_url, m_image_label);

    m_save_btn->setEnabled(true);
    m_load_btn->setEnabled(true);
}

void AmiiboBrowserDialog::FetchImage(const QString& image_url, QLabel* target_label) {
    if (image_url.isEmpty()) {
        target_label->setText(tr("Изображение отсутствует"));
        return;
    }

    if (m_image_cache.contains(image_url)) {
        target_label->setPixmap(m_image_cache[image_url].scaled(target_label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return;
    }

    // Check disk cache
    const auto cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "amiibo_images";
    Common::FS::CreateDirs(cache_dir);
    QString filename = QUrl(image_url).fileName();
    if (filename.isEmpty()) filename = QStringLiteral("image.png");
    auto local_img_path = cache_dir / filename.toStdString();

    if (std::filesystem::exists(local_img_path)) {
        QPixmap pixmap(QString::fromStdString(local_img_path.string()));
        if (!pixmap.isNull()) {
            m_image_cache[image_url] = pixmap;
            target_label->setPixmap(pixmap.scaled(target_label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
    }

    target_label->setText(tr("Загрузка артворка..."));

    QUrl url(image_url);
    QNetworkRequest req(url);
    auto* reply = m_network_mgr->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, image_url, local_img_path, target_label]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray img_data = reply->readAll();
            QPixmap pixmap;
            if (pixmap.loadFromData(img_data)) {
                // Save to cache
                QFile file(QString::fromStdString(local_img_path.string()));
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(img_data);
                    file.close();
                }
                m_image_cache[image_url] = pixmap;
                target_label->setPixmap(pixmap.scaled(target_label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
        }
        reply->deleteLater();
    });
}

QString AmiiboBrowserDialog::GenerateAndSaveAmiiboBin(const AmiiboEntry& entry) {
    const auto amiibo_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::AmiiboDir);
    QString clean_series = entry.amiibo_series;
    if (clean_series.isEmpty()) clean_series = QStringLiteral("General");
    clean_series.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));

    QString clean_name = entry.name;
    if (clean_name.isEmpty()) clean_name = QStringLiteral("Amiibo");
    clean_name.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));

    const auto series_folder = amiibo_dir / clean_series.toStdString();
    Common::FS::CreateDirs(series_folder);

    const auto bin_path = series_folder / (clean_name.toStdString() + ".bin");

    // Generate standard 540-byte NTAG215 binary
    std::vector<u8> ntag(540, 0);

    // Randomize tag UID (0x00 to 0x08)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<u32> dis(1, 254);
    ntag[0] = 0x04; // NXP manufacturer prefix
    ntag[1] = static_cast<u8>(dis(gen));
    ntag[2] = static_cast<u8>(dis(gen));
    ntag[3] = ntag[0] ^ ntag[1] ^ ntag[2] ^ 0x88; // BCC0
    ntag[4] = static_cast<u8>(dis(gen));
    ntag[5] = static_cast<u8>(dis(gen));
    ntag[6] = static_cast<u8>(dis(gen));
    ntag[7] = static_cast<u8>(dis(gen));
    ntag[8] = ntag[4] ^ ntag[5] ^ ntag[6] ^ ntag[7]; // BCC1

    // NTAG215 Internal bytes
    ntag[9] = 0x48;
    ntag[10] = 0x0F;
    ntag[11] = 0xE0;
    ntag[12] = 0xF1;

    // Capability Container (CC)
    ntag[13] = 0x11;
    ntag[14] = 0x48;
    ntag[15] = 0x00;
    ntag[16] = 0x00;
    ntag[17] = 0xE1;
    ntag[18] = 0x10;
    ntag[19] = 0x3E;
    ntag[20] = 0x00;

    // Parse Amiibo Model ID (Head & Tail 32-bit hex)
    u32 head_val = entry.head.toUInt(nullptr, 16);
    u32 tail_val = entry.tail.toUInt(nullptr, 16);

    // Write Head (0x54 - 0x57) big endian
    ntag[0x54] = static_cast<u8>((head_val >> 24) & 0xFF);
    ntag[0x55] = static_cast<u8>((head_val >> 16) & 0xFF);
    ntag[0x56] = static_cast<u8>((head_val >> 8) & 0xFF);
    ntag[0x57] = static_cast<u8>(head_val & 0xFF);

    // Write Tail (0x58 - 0x5B) big endian
    ntag[0x58] = static_cast<u8>((tail_val >> 24) & 0xFF);
    ntag[0x59] = static_cast<u8>((tail_val >> 16) & 0xFF);
    ntag[0x5A] = static_cast<u8>((tail_val >> 8) & 0xFF);
    ntag[0x5B] = static_cast<u8>(tail_val & 0xFF);

    // Write Backup Head / Tail at 0x1DC and 0x1E0
    ntag[0x1DC] = ntag[0x54];
    ntag[0x1DD] = ntag[0x55];
    ntag[0x1DE] = ntag[0x56];
    ntag[0x1DF] = ntag[0x57];

    ntag[0x1E0] = ntag[0x58];
    ntag[0x1E1] = ntag[0x59];
    ntag[0x1E2] = ntag[0x5A];
    ntag[0x1E3] = ntag[0x5B];

    // Dynamic Lock Bytes and Config at 0x208
    ntag[0x208] = 0x01;
    ntag[0x209] = 0x00;
    ntag[0x20A] = 0x0F;
    ntag[0x20B] = 0xBD;
    ntag[0x20F] = 0x04;
    ntag[0x210] = 0x5F;
    ntag[0x218] = 0xFF;
    ntag[0x219] = 0xFF;
    ntag[0x21A] = 0xFF;
    ntag[0x21B] = 0xFF;

    // Write to disk
    Common::FS::IOFile file{bin_path, Common::FS::FileAccessMode::Write, Common::FS::FileType::BinaryFile};
    if (file.IsOpen()) {
        file.Write(ntag);
        file.Close();
    }

    return QString::fromStdString(bin_path.string());
}

void AmiiboBrowserDialog::OnSaveAmiiboClicked() {
    auto* cur = m_amiibo_list->currentItem();
    if (!cur) return;
    int idx = cur->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= static_cast<int>(m_all_amiibos.size())) return;

    QString saved_path = GenerateAndSaveAmiiboBin(m_all_amiibos[idx]);
    DisplayAmiiboDetails(m_all_amiibos[idx]);
    QMessageBox::information(this, tr("Amiibo сохранен"),
                             tr("Файл Amiibo успешно создан и сохранен в каталог:\n%1").arg(saved_path));
}

void AmiiboBrowserDialog::OnLoadAmiiboClicked() {
    auto* cur = m_amiibo_list->currentItem();
    if (!cur) return;
    int idx = cur->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= static_cast<int>(m_all_amiibos.size())) return;

    QString saved_path = GenerateAndSaveAmiiboBin(m_all_amiibos[idx]);
    emit AmiiboSelectedForLoading(saved_path);
    DisplayAmiiboDetails(m_all_amiibos[idx]);
    QMessageBox::information(this, tr("Amiibo загружен"),
                             tr("Amiibo «%1» успешно отправлен в виртуальный NFC-считыватель эмулятора!").arg(m_all_amiibos[idx].name));
}

void AmiiboBrowserDialog::OnOpenAmiiboFolderClicked() {
    const auto amiibo_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::AmiiboDir);
    Common::FS::CreateDirs(amiibo_dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(amiibo_dir.string())));
}

void AmiiboBrowserDialog::OnRefreshClicked() {
    FetchAmiiboDatabase();
}

void AmiiboBrowserDialog::OnListLoaded() {}
