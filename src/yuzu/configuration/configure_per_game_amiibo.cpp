// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "yuzu/configuration/configure_per_game_amiibo.h"

#include <filesystem>
#include <fstream>
#include <random>

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

#include "common/fs/fs.h"
#include "common/fs/fs_paths.h"
#include "common/fs/path_util.h"
#include "common/logging.h"
#include "core/core.h"

ConfigurePerGameAmiibo::ConfigurePerGameAmiibo(Core::System& system, u64 title_id,
                                               const QString& file_name, QWidget* parent)
    : QWidget(parent), m_system(system), m_title_id(title_id), m_file_name(file_name),
      m_network_mgr(new QNetworkAccessManager(this)) {
    SetupUi();
    LoadAmiiboDatabase();
}

ConfigurePerGameAmiibo::~ConfigurePerGameAmiibo() = default;

void ConfigurePerGameAmiibo::SetupUi() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(10, 10, 10, 10);
    main_layout->setSpacing(8);

    // Top Filter Bar
    auto* top_filter_box = new QWidget(this);
    auto* filter_layout = new QHBoxLayout(top_filter_box);
    filter_layout->setContentsMargins(0, 0, 0, 0);
    filter_layout->setSpacing(8);

    m_search_edit = new QLineEdit(this);
    m_search_edit->setPlaceholderText(tr("🔍 Поиск Amiibo для этой игры..."));
    m_search_edit->setClearButtonEnabled(true);
    connect(m_search_edit, &QLineEdit::textChanged, this, &ConfigurePerGameAmiibo::OnSearchFilterChanged);
    filter_layout->addWidget(m_search_edit, 3);

    m_filter_scope_combo = new QComboBox(this);
    m_filter_scope_combo->addItems({tr("⭐ Рекомендуемые для игры"), tr("🌐 Все доступные Amiibo")});
    connect(m_filter_scope_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConfigurePerGameAmiibo::OnSearchFilterChanged);
    filter_layout->addWidget(m_filter_scope_combo, 2);

    m_series_combo = new QComboBox(this);
    m_series_combo->addItem(tr("Все серии"));
    connect(m_series_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ConfigurePerGameAmiibo::OnSearchFilterChanged);
    filter_layout->addWidget(m_series_combo, 2);

    m_refresh_btn = new QPushButton(tr("🔄 Обновить"), this);
    connect(m_refresh_btn, &QPushButton::clicked, this, &ConfigurePerGameAmiibo::OnRefreshClicked);
    filter_layout->addWidget(m_refresh_btn, 0);

    main_layout->addWidget(top_filter_box);

    m_progress_bar = new QProgressBar(this);
    m_progress_bar->setRange(0, 0);
    m_progress_bar->setTextVisible(false);
    m_progress_bar->setFixedHeight(4);
    m_progress_bar->setVisible(false);
    main_layout->addWidget(m_progress_bar);

    // Splitter with List (left) and Inspector (right)
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(4);

    // Left List
    auto* left_container = new QWidget(this);
    auto* left_layout = new QVBoxLayout(left_container);
    left_layout->setContentsMargins(0, 0, 0, 0);
    left_layout->setSpacing(4);

    auto* list_hint = new QLabel(tr("Отметьте галочками нужные Amiibo для автоматической загрузки и привязки к игре:"), this);
    list_hint->setStyleSheet(QStringLiteral("color: #a0aec0; font-size: 9pt;"));
    left_layout->addWidget(list_hint);

    m_amiibo_list = new QListWidget(this);
    connect(m_amiibo_list, &QListWidget::currentItemChanged, this, &ConfigurePerGameAmiibo::OnItemSelected);
    connect(m_amiibo_list, &QListWidget::itemChanged, this, &ConfigurePerGameAmiibo::OnItemCheckChanged);
    left_layout->addWidget(m_amiibo_list);

    splitter->addWidget(left_container);

    // Right Details
    auto* right_container = new QWidget(this);
    auto* right_layout = new QVBoxLayout(right_container);
    right_layout->setContentsMargins(8, 0, 0, 0);
    right_layout->setSpacing(8);

    auto* details_group = new QGroupBox(tr("Информация и статус Amiibo"), this);
    auto* details_layout = new QVBoxLayout(details_group);
    details_layout->setContentsMargins(10, 14, 10, 10);
    details_layout->setSpacing(6);

    m_image_label = new QLabel(this);
    m_image_label->setMinimumSize(160, 160);
    m_image_label->setMaximumHeight(200);
    m_image_label->setAlignment(Qt::AlignCenter);
    m_image_label->setStyleSheet(QStringLiteral("background-color: #0c1018; border: 1px solid rgba(255,255,255,0.08); border-radius: 8px;"));
    m_image_label->setText(tr("Выберите фигурку"));
    details_layout->addWidget(m_image_label);

    m_name_label = new QLabel(tr("Имя: —"), this);
    m_name_label->setStyleSheet(QStringLiteral("font-size: 11pt; font-weight: bold; color: #00f0ff;"));
    details_layout->addWidget(m_name_label);

    m_series_label = new QLabel(tr("Серия: —"), this);
    details_layout->addWidget(m_series_label);

    m_type_label = new QLabel(tr("Тип: —"), this);
    details_layout->addWidget(m_type_label);

    m_status_badge = new QLabel(tr("Статус: Ожидание"), this);
    m_status_badge->setStyleSheet(QStringLiteral("color: #a0aec0; font-weight: bold; padding: 2px 6px; background-color: #1a2336; border-radius: 3px;"));
    details_layout->addWidget(m_status_badge);

    auto* games_label = new QLabel(tr("🎮 Эффект и совместимость в этой игре:"), this);
    games_label->setStyleSheet(QStringLiteral("font-weight: bold; color: #ffca28; margin-top: 4px;"));
    details_layout->addWidget(games_label);

    m_games_text = new QTextEdit(this);
    m_games_text->setReadOnly(true);
    m_games_text->setPlaceholderText(tr("Описание совместимости"));
    details_layout->addWidget(m_games_text, 1);

    auto* btn_layout = new QHBoxLayout();
    m_install_btn = new QPushButton(tr("⚡ Включить / Установить"), this);
    m_install_btn->setStyleSheet(QStringLiteral("background-color: #004d40; border-color: #00e676; color: #ffffff; padding: 6px 12px; font-weight: bold;"));
    m_install_btn->setEnabled(false);
    connect(m_install_btn, &QPushButton::clicked, this, [this]() {
        auto* cur = m_amiibo_list->currentItem();
        if (cur) {
            bool is_checked = cur->checkState() == Qt::Checked;
            cur->setCheckState(is_checked ? Qt::Unchecked : Qt::Checked);
        }
    });
    btn_layout->addWidget(m_install_btn);

    m_open_folder_btn = new QPushButton(tr("📁 Папка Amiibo игры"), this);
    connect(m_open_folder_btn, &QPushButton::clicked, this, &ConfigurePerGameAmiibo::OnOpenAmiiboFolderClicked);
    btn_layout->addWidget(m_open_folder_btn);

    details_layout->addLayout(btn_layout);
    right_layout->addWidget(details_group);
    splitter->addWidget(right_container);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    main_layout->addWidget(splitter);
}

QString ConfigurePerGameAmiibo::GetGameAmiiboFolder() const {
    const auto amiibo_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::AmiiboDir);
    QString clean_title = QStringLiteral("%1").arg(m_title_id, 16, 16, QLatin1Char('0')).toUpper();
    if (!m_file_name.isEmpty()) {
        QFileInfo fi(m_file_name);
        QString base = fi.completeBaseName();
        base.replace(QRegularExpression(QStringLiteral(R"(\[[^\]]*\]|\([^\)]*\))")), QString());
        base = base.trimmed();
        if (!base.isEmpty()) {
            clean_title = base;
        }
    }
    clean_title.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
    auto game_folder = amiibo_dir / clean_title.toStdString();
    Common::FS::CreateDirs(game_folder);
    return QString::fromStdString(game_folder.string());
}

bool ConfigurePerGameAmiibo::IsAmiiboInstalled(const AmiiboEntry& entry) const {
    QString folder = GetGameAmiiboFolder();
    QString clean_name = entry.name;
    clean_name.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
    QString path1 = folder + QStringLiteral("/") + clean_name + QStringLiteral(".bin");

    return QFile::exists(path1);
}

void ConfigurePerGameAmiibo::LoadAmiiboDatabase() {
    const auto cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir);
    auto cache_file = cache_dir / "amiibo_database.json";

    if (std::filesystem::exists(cache_file)) {
        QFile file(QString::fromStdString(cache_file.string()));
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray file_bytes = file.readAll();
            file.close();
            ParseDatabaseJson(file_bytes);
            if (!m_all_amiibos.empty()) {
                PopulateSeriesFilter();
                ApplyFilters();
                return;
            }
        }
    }

    OnRefreshClicked();
}

void ConfigurePerGameAmiibo::OnRefreshClicked() {
    m_progress_bar->setVisible(true);

    const QStringList urls = {
        QStringLiteral("https://www.amiiboapi.com/api/amiibo/"),
        QStringLiteral("https://cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master/database/amiibo.json"),
        QStringLiteral("https://raw.githubusercontent.com/N3evin/AmiiboAPI/master/database/amiibo.json"),
    };

    auto tryFetchUrl = [this, urls](auto&& self, int index) -> void {
        if (index >= urls.size()) {
            m_progress_bar->setVisible(false);
            return;
        }

        QUrl url(urls[index]);
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 STORM-EDEN/4.0.1"));
        req.setRawHeader("Accept", "application/json, text/plain, */*");

        auto* reply = m_network_mgr->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, index, self]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray db_raw_data = reply->readAll();
                const auto cache_dir = Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir);
                Common::FS::CreateDirs(cache_dir);
                auto cache_file = cache_dir / "amiibo_database.json";
                QFile file(QString::fromStdString(cache_file.string()));
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(db_raw_data);
                    file.close();
                }
                ParseDatabaseJson(db_raw_data);
                PopulateSeriesFilter();
                ApplyFilters();
                m_progress_bar->setVisible(false);
            } else {
                self(self, index + 1);
            }
            reply->deleteLater();
        });
    };

    tryFetchUrl(tryFetchUrl, 0);
}

void ConfigurePerGameAmiibo::ParseDatabaseJson(const QByteArray& json_data) {
    QJsonParseError err{};
    QJsonDocument doc = QJsonDocument::fromJson(json_data, &err);
    if (err.error != QJsonParseError::NoError) return;

    std::vector<AmiiboEntry> parsed_list;

    auto populateSwitchGames = [](AmiiboEntry& entry) {
        if (!entry.switch_games.isEmpty()) return;
        if (entry.game_series.contains(QStringLiteral("Zelda"), Qt::CaseInsensitive) ||
            entry.amiibo_series.contains(QStringLiteral("Zelda"), Qt::CaseInsensitive)) {
            entry.switch_games.append(QStringLiteral("• The Legend of Zelda: Tears of the Kingdom: Эксклюзивная ткань параплана, оружие, ресурсы"));
            entry.switch_games.append(QStringLiteral("• The Legend of Zelda: Breath of the Wild: Доспехи, оружие, Эпона, Волк Линк, сундуки"));
            entry.switch_games.append(QStringLiteral("• The Legend of Zelda: Echoes of Wisdom: Уникальные костюмы, аксессуары и ресурсы"));
            entry.switch_games.append(QStringLiteral("• Super Smash Bros. Ultimate: Обучаемый боец FP (Figure Player)"));
        } else if (entry.game_series.contains(QStringLiteral("Mario"), Qt::CaseInsensitive) ||
                   entry.amiibo_series.contains(QStringLiteral("Mario"), Qt::CaseInsensitive)) {
            entry.switch_games.append(QStringLiteral("• Super Mario Odyssey: Уникальные костюмы для Марио и подсказки Лун энергии"));
            entry.switch_games.append(QStringLiteral("• Super Mario 3D World + Bowser's Fury: Костюм Белого Тануки Неуязвимости, суперзвезды"));
            entry.switch_games.append(QStringLiteral("• Super Smash Bros. Ultimate: Боец FP с прокачкой 1-50 ур."));
            entry.switch_games.append(QStringLiteral("• Mario Kart 8 Deluxe: Гоночный костюм Mii"));
        } else if (entry.game_series.contains(QStringLiteral("Splatoon"), Qt::CaseInsensitive) ||
                   entry.amiibo_series.contains(QStringLiteral("Splatoon"), Qt::CaseInsensitive)) {
            entry.switch_games.append(QStringLiteral("• Splatoon 3 / 2: Эксклюзивные наборы экипировки и фотосессии"));
        } else if (entry.game_series.contains(QStringLiteral("Metroid"), Qt::CaseInsensitive) ||
                   entry.amiibo_series.contains(QStringLiteral("Metroid"), Qt::CaseInsensitive)) {
            entry.switch_games.append(QStringLiteral("• Metroid Dread: Дополнительный контейнер энергии (Energy Tank) и пополнение ракет"));
        } else if (entry.game_series.contains(QStringLiteral("Monster Hunter"), Qt::CaseInsensitive) ||
                   entry.amiibo_series.contains(QStringLiteral("Monster Hunter"), Qt::CaseInsensitive)) {
            entry.switch_games.append(QStringLiteral("• Monster Hunter Rise: Многослойная броня и ежедневная лотерея"));
        } else if (entry.game_series.contains(QStringLiteral("Animal Crossing"), Qt::CaseInsensitive) ||
                   entry.amiibo_series.contains(QStringLiteral("Animal Crossing"), Qt::CaseInsensitive)) {
            entry.switch_games.append(QStringLiteral("• Animal Crossing: New Horizons: Плакаты, кемпинг жителя, фотосессия"));
        } else if (entry.game_series.contains(QStringLiteral("Fire Emblem"), Qt::CaseInsensitive) ||
                   entry.amiibo_series.contains(QStringLiteral("Fire Emblem"), Qt::CaseInsensitive)) {
            entry.switch_games.append(QStringLiteral("• Fire Emblem Engage: Музыкальные треки и билеты на наряды"));
        } else if (entry.game_series.contains(QStringLiteral("Xenoblade"), Qt::CaseInsensitive) ||
                   entry.amiibo_series.contains(QStringLiteral("Xenoblade"), Qt::CaseInsensitive)) {
            entry.switch_games.append(QStringLiteral("• Xenoblade Chronicles 3: Облик Меча Монадо и полезные расходники"));
        } else {
            entry.switch_games.append(QStringLiteral("• Super Smash Bros. Ultimate: Обучаемый боец FP или получение бонусов/духов"));
            entry.switch_games.append(QStringLiteral("• Mario Kart 8 Deluxe: Гоночный костюм Mii"));
            entry.switch_games.append(QStringLiteral("• Универсальная поддержка: Совместимо со всеми Switch играми с поддержкой Amiibo"));
        }
    };

    auto parseObject = [&populateSwitchGames](const QJsonObject& obj) -> AmiiboEntry {
        AmiiboEntry entry;
        entry.character = obj.value(QStringLiteral("character")).toString();
        entry.name = obj.value(QStringLiteral("name")).toString();
        entry.game_series = obj.value(QStringLiteral("gameSeries")).toString();
        entry.amiibo_series = obj.value(QStringLiteral("amiiboSeries")).toString();
        entry.type = obj.value(QStringLiteral("type")).toString();
        entry.head = obj.value(QStringLiteral("head")).toString();
        entry.tail = obj.value(QStringLiteral("tail")).toString();
        entry.image_url = obj.value(QStringLiteral("image")).toString();

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
        populateSwitchGames(entry);
        return entry;
    };

    if (doc.isObject()) {
        QJsonObject root = doc.object();
        if (root.contains(QStringLiteral("amiibo")) && root.value(QStringLiteral("amiibo")).isArray()) {
            QJsonArray arr = root.value(QStringLiteral("amiibo")).toArray();
            for (const auto& val : arr) {
                if (val.isObject()) {
                    AmiiboEntry e = parseObject(val.toObject());
                    if (e.image_url.isEmpty() && !e.head.isEmpty() && !e.tail.isEmpty()) {
                        e.image_url = QStringLiteral("https://cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master/images/icon_%1-%2.png")
                                          .arg(e.head.toLower(), e.tail.toLower());
                    }
                    parsed_list.push_back(std::move(e));
                }
            }
        } else if (root.contains(QStringLiteral("amiibos")) && root.value(QStringLiteral("amiibos")).isObject()) {
            QJsonObject dict = root.value(QStringLiteral("amiibos")).toObject();
            QJsonObject amiibo_series_map = root.value(QStringLiteral("amiibo_series")).toObject();
            QJsonObject game_series_map = root.value(QStringLiteral("game_series")).toObject();
            QJsonObject types_map = root.value(QStringLiteral("types")).toObject();
            QJsonObject characters_map = root.value(QStringLiteral("characters")).toObject();

            for (auto it = dict.begin(); it != dict.end(); ++it) {
                if (it.value().isObject()) {
                    AmiiboEntry e = parseObject(it.value().toObject());
                    QString key = it.key();
                    if (key.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) key = key.mid(2);
                    if (key.length() >= 16) {
                        e.head = key.left(8);
                        e.tail = key.mid(8, 8);
                    }

                    if (e.amiibo_series.isEmpty() && e.tail.length() >= 4) {
                        QString series_id = QStringLiteral("0x") + e.tail.mid(2, 2).toLower();
                        if (amiibo_series_map.contains(series_id)) {
                            e.amiibo_series = amiibo_series_map.value(series_id).toString();
                        }
                    }
                    if (e.amiibo_series.isEmpty()) e.amiibo_series = QStringLiteral("Others");

                    if (e.type.isEmpty() && e.tail.length() >= 8) {
                        QString type_id = QStringLiteral("0x") + e.tail.mid(6, 2).toLower();
                        if (types_map.contains(type_id)) {
                            e.type = types_map.value(type_id).toString();
                        }
                    }
                    if (e.type.isEmpty()) e.type = QStringLiteral("Figure");

                    if (e.game_series.isEmpty() && e.head.length() >= 3) {
                        QString g_id = QStringLiteral("0x") + e.head.left(3).toLower();
                        if (game_series_map.contains(g_id)) {
                            e.game_series = game_series_map.value(g_id).toString();
                        }
                    }
                    if (e.game_series.isEmpty()) e.game_series = QStringLiteral("Nintendo");

                    if (e.character.isEmpty() && e.head.length() >= 4) {
                        QString c_id = QStringLiteral("0x") + e.head.left(4).toLower();
                        if (characters_map.contains(c_id)) {
                            e.character = characters_map.value(c_id).toString();
                        }
                    }
                    if (e.character.isEmpty()) e.character = e.name;

                    if (e.image_url.isEmpty() && !e.head.isEmpty() && !e.tail.isEmpty()) {
                        e.image_url = QStringLiteral("https://cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master/images/icon_%1-%2.png")
                                          .arg(e.head.toLower(), e.tail.toLower());
                    }

                    populateSwitchGames(e);
                    parsed_list.push_back(std::move(e));
                }
            }
        }
    }

    if (!parsed_list.empty()) {
        m_all_amiibos = std::move(parsed_list);
    }
}

void ConfigurePerGameAmiibo::PopulateSeriesFilter() {
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
    if (idx >= 0) m_series_combo->setCurrentIndex(idx);
    m_series_combo->blockSignals(false);
}

void ConfigurePerGameAmiibo::OnSearchFilterChanged() {
    ApplyFilters();
}

void ConfigurePerGameAmiibo::ApplyFilters() {
    m_is_updating_ui = true;
    m_amiibo_list->clear();
    m_filtered_indices.clear();

    const QString search_text = m_search_edit->text().trimmed().toLower();
    const QString series_filter = m_series_combo->currentText();
    const bool recommended_only = (m_filter_scope_combo->currentIndex() == 0);

    // Determine keywords for current game
    QString title_str = QStringLiteral("%1").arg(m_title_id, 16, 16, QLatin1Char('0')).toUpper();
    QString game_keyword;
    if (!m_file_name.isEmpty()) {
        QFileInfo fi(m_file_name);
        game_keyword = fi.completeBaseName().toLower();
    }

    for (size_t i = 0; i < m_all_amiibos.size(); ++i) {
        const auto& a = m_all_amiibos[i];

        if (m_series_combo->currentIndex() > 0 && a.amiibo_series != series_filter) {
            continue;
        }

        if (recommended_only) {
            bool matches_game = false;
            if (game_keyword.contains(QStringLiteral("zelda")) &&
                (a.game_series.contains(QStringLiteral("zelda"), Qt::CaseInsensitive) || a.amiibo_series.contains(QStringLiteral("zelda"), Qt::CaseInsensitive))) {
                matches_game = true;
            } else if (game_keyword.contains(QStringLiteral("mario")) &&
                       (a.game_series.contains(QStringLiteral("mario"), Qt::CaseInsensitive) || a.amiibo_series.contains(QStringLiteral("mario"), Qt::CaseInsensitive))) {
                matches_game = true;
            } else if (game_keyword.contains(QStringLiteral("smash")) || game_keyword.contains(QStringLiteral("ssbu"))) {
                matches_game = true;
            } else if (game_keyword.contains(QStringLiteral("splatoon")) &&
                       (a.game_series.contains(QStringLiteral("splatoon"), Qt::CaseInsensitive) || a.amiibo_series.contains(QStringLiteral("splatoon"), Qt::CaseInsensitive))) {
                matches_game = true;
            } else if (game_keyword.contains(QStringLiteral("metroid")) &&
                       (a.game_series.contains(QStringLiteral("metroid"), Qt::CaseInsensitive) || a.amiibo_series.contains(QStringLiteral("metroid"), Qt::CaseInsensitive))) {
                matches_game = true;
            } else if (game_keyword.contains(QStringLiteral("pokemon")) &&
                       (a.game_series.contains(QStringLiteral("pokemon"), Qt::CaseInsensitive) || a.amiibo_series.contains(QStringLiteral("pokemon"), Qt::CaseInsensitive))) {
                matches_game = true;
            } else if (game_keyword.contains(QStringLiteral("kirby")) &&
                       (a.game_series.contains(QStringLiteral("kirby"), Qt::CaseInsensitive) || a.amiibo_series.contains(QStringLiteral("kirby"), Qt::CaseInsensitive))) {
                matches_game = true;
            } else if (game_keyword.contains(QStringLiteral("fire emblem")) &&
                       (a.game_series.contains(QStringLiteral("fire emblem"), Qt::CaseInsensitive) || a.amiibo_series.contains(QStringLiteral("fire emblem"), Qt::CaseInsensitive))) {
                matches_game = true;
            } else if (game_keyword.contains(QStringLiteral("xenoblade")) &&
                       (a.game_series.contains(QStringLiteral("xenoblade"), Qt::CaseInsensitive) || a.amiibo_series.contains(QStringLiteral("xenoblade"), Qt::CaseInsensitive))) {
                matches_game = true;
            } else if (game_keyword.contains(QStringLiteral("monster hunter")) &&
                       (a.game_series.contains(QStringLiteral("monster hunter"), Qt::CaseInsensitive) || a.amiibo_series.contains(QStringLiteral("monster hunter"), Qt::CaseInsensitive))) {
                matches_game = true;
            } else if (game_keyword.contains(QStringLiteral("animal crossing")) &&
                       (a.game_series.contains(QStringLiteral("animal crossing"), Qt::CaseInsensitive) || a.amiibo_series.contains(QStringLiteral("animal crossing"), Qt::CaseInsensitive))) {
                matches_game = true;
            }

            if (!matches_game) {
                // If game is unknown, match Smash or Mario or Zelda as top Switch amiibos
                if (a.amiibo_series.contains(QStringLiteral("Smash"), Qt::CaseInsensitive) ||
                    a.amiibo_series.contains(QStringLiteral("Zelda"), Qt::CaseInsensitive) ||
                    a.amiibo_series.contains(QStringLiteral("Mario"), Qt::CaseInsensitive)) {
                    matches_game = true;
                }
            }

            if (!matches_game) continue;
        }

        if (!search_text.isEmpty()) {
            bool matches = a.name.toLower().contains(search_text) ||
                           a.character.toLower().contains(search_text) ||
                           a.game_series.toLower().contains(search_text) ||
                           a.amiibo_series.toLower().contains(search_text);
            if (!matches) continue;
        }

        m_filtered_indices.push_back(static_cast<int>(i));

        bool installed = IsAmiiboInstalled(a);
        auto* item = new QListWidgetItem(QStringLiteral("%1 [%2] %3")
                                             .arg(a.name, a.amiibo_series, installed ? QStringLiteral("✅") : QString()));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(installed ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, static_cast<int>(i));
        m_amiibo_list->addItem(item);
    }

    m_is_updating_ui = false;

    if (m_amiibo_list->count() > 0) {
        m_amiibo_list->setCurrentRow(0);
    } else {
        m_name_label->setText(tr("Ничего не найдено"));
        m_series_label->setText(QString());
        m_type_label->setText(QString());
        m_status_badge->setText(tr("Статус: Нет результатов"));
        m_games_text->clear();
        m_image_label->setText(tr("Нет данных"));
        m_install_btn->setEnabled(false);
    }
}

void ConfigurePerGameAmiibo::OnItemSelected(QListWidgetItem* current, QListWidgetItem* previous) {
    if (!current) return;
    int idx = current->data(Qt::UserRole).toInt();
    if (idx >= 0 && idx < static_cast<int>(m_all_amiibos.size())) {
        DisplayAmiiboDetails(m_all_amiibos[idx]);
    }
}

void ConfigurePerGameAmiibo::OnItemCheckChanged(QListWidgetItem* item) {
    if (m_is_updating_ui || !item) return;

    int idx = item->data(Qt::UserRole).toInt();
    if (idx < 0 || idx >= static_cast<int>(m_all_amiibos.size())) return;

    const auto& entry = m_all_amiibos[idx];
    if (item->checkState() == Qt::Checked) {
        QString saved = GenerateAndSaveAmiiboBin(entry);
        item->setText(QStringLiteral("%1 [%2] ✅").arg(entry.name, entry.amiibo_series));
        DisplayAmiiboDetails(entry);
    } else {
        QString folder = GetGameAmiiboFolder();
        QString clean_name = entry.name;
        clean_name.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
        QString path = folder + QStringLiteral("/") + clean_name + QStringLiteral(".bin");
        QFile::remove(path);
        item->setText(QStringLiteral("%1 [%2]").arg(entry.name, entry.amiibo_series));
        DisplayAmiiboDetails(entry);
    }
}

void ConfigurePerGameAmiibo::DisplayAmiiboDetails(const AmiiboEntry& entry) {
    m_name_label->setText(tr("Имя: %1 (%2)").arg(entry.name, entry.character));
    m_series_label->setText(tr("Серия Amiibo: %1 | Вселенная: %2").arg(entry.amiibo_series, entry.game_series));
    m_type_label->setText(tr("Тип: %1 | ID: %2 %3").arg(entry.type, entry.head, entry.tail));

    bool installed = IsAmiiboInstalled(entry);
    if (installed) {
        m_status_badge->setText(tr("Статус: ✅ Включено и установлено для этой игры"));
        m_status_badge->setStyleSheet(QStringLiteral("color: #00e676; font-weight: bold; padding: 3px 8px; background-color: #004d40; border-radius: 4px;"));
        m_install_btn->setText(tr("❌ Отключить для игры"));
    } else {
        m_status_badge->setText(tr("Статус: 🌐 Нажмите галочку для скачивания и включения"));
        m_status_badge->setStyleSheet(QStringLiteral("color: #00e5ff; font-weight: bold; padding: 3px 8px; background-color: #006064; border-radius: 4px;"));
        m_install_btn->setText(tr("⚡ Скачать и включить"));
    }

    if (entry.switch_games.isEmpty()) {
        m_games_text->setPlainText(tr("Совместимо с Nintendo Switch."));
    } else {
        m_games_text->setPlainText(entry.switch_games.join(QStringLiteral("\n\n")));
    }

    FetchImage(entry.image_url, m_image_label);
    m_install_btn->setEnabled(true);
}

void ConfigurePerGameAmiibo::FetchImage(const QString& image_url, QLabel* target_label) {
    if (image_url.isEmpty()) {
        target_label->setText(tr("Изображение отсутствует"));
        return;
    }

    if (m_image_cache.contains(image_url)) {
        target_label->setPixmap(m_image_cache[image_url].scaled(target_label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        return;
    }

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

    QStringList mirror_urls;
    mirror_urls << image_url;
    QString fastly_url = image_url;
    fastly_url.replace(QStringLiteral("cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master"), QStringLiteral("fastly.jsdelivr.net/gh/N3evin/AmiiboAPI@master"));
    fastly_url.replace(QStringLiteral("raw.githubusercontent.com/N3evin/AmiiboAPI/master"), QStringLiteral("fastly.jsdelivr.net/gh/N3evin/AmiiboAPI@master"));
    mirror_urls << fastly_url;

    QString gh_url = image_url;
    gh_url.replace(QStringLiteral("cdn.jsdelivr.net/gh/N3evin/AmiiboAPI@master"), QStringLiteral("raw.githubusercontent.com/N3evin/AmiiboAPI/master"));
    mirror_urls << gh_url;

    QString proxy_url = QStringLiteral("https://ghproxy.net/") + gh_url;
    mirror_urls << proxy_url;
    mirror_urls.removeDuplicates();

    auto downloadWithFallback = [this, target_label, local_img_path, image_url, mirror_urls](int mirror_index, auto&& self) -> void {
        if (mirror_index >= mirror_urls.size()) {
            target_label->setText(tr("Изображение отсутствует"));
            return;
        }

        QUrl url(mirror_urls[mirror_index]);
        QNetworkRequest req(url);
        req.setTransferTimeout(2500);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 STORM-EDEN/4.0.2"));
        req.setRawHeader("Accept", "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8");

        auto* reply = m_network_mgr->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, image_url, local_img_path, target_label, mirror_index, mirror_urls, self]() {
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray img_data = reply->readAll();
                QPixmap pixmap;
                if (pixmap.loadFromData(img_data)) {
                    QFile file(QString::fromStdString(local_img_path.string()));
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(img_data);
                        file.close();
                    }
                    m_image_cache[image_url] = pixmap;
                    target_label->setPixmap(pixmap.scaled(target_label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    reply->deleteLater();
                    return;
                }
            }
            reply->deleteLater();
            self(mirror_index + 1, self);
        });
    };

    downloadWithFallback(0, downloadWithFallback);
}

QString ConfigurePerGameAmiibo::GenerateAndSaveAmiiboBin(const AmiiboEntry& entry) {
    QString folder = GetGameAmiiboFolder();
    QString clean_name = entry.name;
    if (clean_name.isEmpty()) clean_name = QStringLiteral("Amiibo");
    clean_name.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));

    QString bin_path_str = folder + QStringLiteral("/") + clean_name + QStringLiteral(".bin");
    std::filesystem::path bin_path = bin_path_str.toStdString();

    std::vector<u8> bin_data(540, 0);

    // UID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<u32> dis(0x10, 0xEF);
    bin_data[0] = static_cast<u8>(dis(gen));
    bin_data[1] = static_cast<u8>(dis(gen));
    bin_data[2] = static_cast<u8>(dis(gen));
    bin_data[3] = 0x88;
    bin_data[4] = static_cast<u8>(dis(gen));
    bin_data[5] = static_cast<u8>(dis(gen));
    bin_data[6] = static_cast<u8>(dis(gen));
    bin_data[7] = static_cast<u8>(dis(gen));

    // Nickname
    QString name_to_write = entry.name.left(10);
    for (int i = 0; i < name_to_write.length() && i < 10; ++i) {
        ushort unicode = name_to_write.at(i).unicode();
        bin_data[0x38 + i * 2] = static_cast<u8>((unicode >> 8) & 0xFF);
        bin_data[0x38 + i * 2 + 1] = static_cast<u8>(unicode & 0xFF);
    }

    // Model ID
    bool ok_head = false, ok_tail = false;
    u32 head_val = entry.head.toUInt(&ok_head, 16);
    u32 tail_val = entry.tail.toUInt(&ok_tail, 16);

    bin_data[0x54] = static_cast<u8>((head_val >> 24) & 0xFF);
    bin_data[0x55] = static_cast<u8>((head_val >> 16) & 0xFF);
    bin_data[0x56] = static_cast<u8>((head_val >> 8) & 0xFF);
    bin_data[0x57] = static_cast<u8>(head_val & 0xFF);

    bin_data[0x58] = static_cast<u8>((tail_val >> 24) & 0xFF);
    bin_data[0x59] = static_cast<u8>((tail_val >> 16) & 0xFF);
    bin_data[0x5A] = static_cast<u8>((tail_val >> 8) & 0xFF);
    bin_data[0x5B] = static_cast<u8>(tail_val & 0xFF);

    // Save to disk
    std::ofstream out(bin_path, std::ios::binary);
    if (out.is_open()) {
        out.write(reinterpret_cast<const char*>(bin_data.data()), bin_data.size());
        out.close();
    }

    return bin_path_str;
}

void ConfigurePerGameAmiibo::OnOpenAmiiboFolderClicked() {
    QString folder = GetGameAmiiboFolder();
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void ConfigurePerGameAmiibo::ApplyConfiguration() {
    // Explicitly synchronize current checkbox states with game folder
    QString folder = GetGameAmiiboFolder();
    for (int i = 0; i < m_amiibo_list->count(); ++i) {
        auto* item = m_amiibo_list->item(i);
        if (!item) continue;
        int idx = item->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= static_cast<int>(m_all_amiibos.size())) continue;
        const auto& entry = m_all_amiibos[idx];

        QString clean_name = entry.name;
        if (clean_name.isEmpty()) clean_name = QStringLiteral("Amiibo");
        clean_name.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
        QString bin_path = folder + QStringLiteral("/") + clean_name + QStringLiteral(".bin");

        if (item->checkState() == Qt::Checked) {
            if (!QFile::exists(bin_path)) {
                GenerateAndSaveAmiiboBin(entry);
            }
        } else {
            if (QFile::exists(bin_path)) {
                QFile::remove(bin_path);
            }
        }
    }
}
