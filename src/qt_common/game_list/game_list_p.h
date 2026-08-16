// SPDX-FileCopyrightText: Copyright 2026 STORM EDEN Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <map>
#include <string>
#include <utility>

#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QObject>
#include <QPainter>
#include <QPen>
#include <QRegularExpression>
#include <QStandardItem>
#include <QString>

#include "common/common_types.h"
#include "common/logging.h"
#include "common/string_util.h"
#include "frontend_common/play_time_manager.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/qt_common.h"

enum class GameListItemType {
    Game = QStandardItem::UserType + 1,
    CustomDir = QStandardItem::UserType + 2,
    SdmcDir = QStandardItem::UserType + 3,
    UserNandDir = QStandardItem::UserType + 4,
    SysNandDir = QStandardItem::UserType + 5,
    AddDir = QStandardItem::UserType + 6,
    Favorites = QStandardItem::UserType + 7,
};

Q_DECLARE_METATYPE(GameListItemType);

static QPixmap GetDefaultIcon(u32 size) {
    if (size == 0) size = 32;
    QPixmap icon(size, size);
    icon.fill(Qt::transparent);
    QPainter p(&icon);
    p.setRenderHint(QPainter::Antialiasing);

    // Cartridge / Card Base
    QLinearGradient bg(0, 0, size, size);
    bg.setColorAt(0.0, QColor(24, 28, 44));
    bg.setColorAt(1.0, QColor(10, 12, 20));

    p.setPen(QPen(QColor(0, 242, 254), 1.5));
    p.setBrush(bg);
    p.drawRoundedRect(2, 2, size - 4, size - 4, 4, 4);

    // Joy-Con Red & Blue top accents
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 60, 90)); // Neon Red
    p.drawRoundedRect(4, 4, (size - 8) / 2, size / 3, 2, 2);

    p.setBrush(QColor(0, 220, 255)); // Neon Blue
    p.drawRoundedRect(4 + (size - 8) / 2 + 1, 4, (size - 8) / 2 - 1, size / 3, 2, 2);

    // Subtle controller circles
    p.setPen(QPen(QColor(255, 255, 255, 220), 1.2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(size / 4, size / 2, size / 5, size / 5);
    p.drawEllipse(size * 7 / 12, size * 7 / 12, size / 5, size / 5);

    return icon;
}

static QPixmap ThemeIcon(const char* name) {
    const int size = std::max(20, static_cast<int>(UISettings::values.folder_icon_size.GetValue()));

    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    if (std::string(name) == "folder" || std::string(name) == "bad_folder") {
        // Vibrant solid golden-yellow 3D folder with cyan tab & crisp outline
        QLinearGradient grad(0, 0, 0, size);
        grad.setColorAt(0.0, QColor(255, 238, 0));
        grad.setColorAt(1.0, QColor(255, 175, 0));

        // Back tab (Cyan)
        p.setPen(QPen(QColor(0, 240, 255), 1.5));
        p.setBrush(QColor(0, 240, 255));
        p.drawRoundedRect(1, 1, size / 2, 6, 2, 2);

        // Front folder body (Solid Yellow/Orange Gradient)
        p.setPen(QPen(QColor(0, 0, 0), 1.5));
        p.setBrush(grad);
        p.drawRoundedRect(1, 4, size - 2, size - 5, 3, 3);

        // Top glossy highlight
        p.setPen(QPen(QColor(255, 255, 255, 220), 1));
        p.drawLine(3, 6, size - 4, 6);
    } else if (std::string(name) == "star") {
        p.setPen(QPen(QColor(0, 0, 0), 1));
        p.setBrush(QColor(255, 215, 0));
        p.drawEllipse(2, 2, size - 4, size - 4);
    } else if (std::string(name) == "list-add") {
        p.setPen(QPen(QColor(0, 240, 255), 1.5));
        p.setBrush(QColor(18, 22, 36));
        p.drawRoundedRect(2, 2, size - 4, size - 4, 3, 3);
        p.setPen(QPen(QColor(0, 240, 255), 2));
        p.drawLine(size / 2, 5, size / 2, size - 5);
        p.drawLine(5, size / 2, size - 5, size / 2);
    } else {
        QIcon icon = QIcon::fromTheme(QLatin1String(name));
        if (!icon.isNull()) {
            return icon.pixmap(size, size);
        }
        p.setPen(QPen(QColor(0, 240, 255), 1.5));
        p.setBrush(QColor(0, 240, 255));
        p.drawRoundedRect(2, 2, size - 4, size - 4, 3, 3);
    }
    return pix;
}

class GameListItem : public QStandardItem {

public:
    // used to access type from item index
    static constexpr int TypeRole = Qt::UserRole + 1;
    static constexpr int SortRole = Qt::UserRole + 2;
    GameListItem() = default;
    explicit GameListItem(const QString& string) : QStandardItem(string) {
        setData(string, SortRole);
    }
};

/**
 * A specialization of GameListItem for path values.
 * This class ensures that for every full path value it holds, a correct string representation
 * of just the filename (with no extension) will be displayed to the user.
 * If this class receives valid title metadata, it will also display game icons and titles.
 */
class GameListItemPath : public GameListItem {
public:
    static constexpr int TitleRole = SortRole + 1;
    static constexpr int FullPathRole = SortRole + 2;
    static constexpr int ProgramIdRole = SortRole + 3;
    static constexpr int FileTypeRole = SortRole + 4;

    GameListItemPath() = default;
    GameListItemPath(const QString& game_path, const std::vector<u8>& picture_data,
                     const QString& game_name, const QString& game_type, u64 program_id,
                     u64 play_time, const QString& patch_versions, u64 size_bytes = 0,
                     const QString& base_version = QStringLiteral("1.0.0")) {
        setData(type(), TypeRole);
        setData(game_path, FullPathRole);
        setData(game_name, TitleRole);
        setData(qulonglong(program_id), ProgramIdRole);
        setData(game_type, FileTypeRole);

        const auto readable_play_time =
            play_time > 0 ? QString::fromStdString(
                                PlayTime::PlayTimeManager::GetReadablePlayTime(play_time))
                          : QObject::tr("Не запускалась");

        QString version_num = base_version.trimmed().isEmpty() ? QStringLiteral("1.0.0") : base_version.trimmed();
        int addon_count = 0;
        const QStringList lines = patch_versions.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        const QRegularExpression update_regex{QStringLiteral(R"(Update\s*\(v?([0-9a-zA-Z\.\-]+)\))"), QRegularExpression::CaseInsensitiveOption};

        for (const QString& line : lines) {
            const auto match = update_regex.match(line);
            if (match.hasMatch() && match.hasCaptured(1)) {
                version_num = match.captured(1);
            } else if (line.contains(QStringLiteral("Update"), Qt::CaseInsensitive)) {
                QString clean = line;
                clean.remove(QStringLiteral("Update"), Qt::CaseInsensitive);
                clean.remove(QLatin1Char('('));
                clean.remove(QLatin1Char(')'));
                clean = clean.trimmed();
                if (!clean.isEmpty()) {
                    version_num = clean;
                }
            } else {
                QString clean = line.trimmed();
                clean.replace(QStringLiteral("DLC "), QStringLiteral(""), Qt::CaseInsensitive);
                clean.replace(QStringLiteral("DLC"), QStringLiteral(""), Qt::CaseInsensitive);
                clean.remove(QLatin1Char('('));
                clean.remove(QLatin1Char(')'));
                clean = clean.trimmed();
                if (!clean.isEmpty()) {
                    const auto parts = clean.split(QLatin1Char(','), Qt::SkipEmptyParts);
                    addon_count += parts.isEmpty() ? 1 : parts.size();
                } else {
                    addon_count++;
                }
            }
        }

        if (addon_count == 0) {
            static const QRegularExpression fn_dlc_regex{QStringLiteral(R"(\+([0-9]+)D\b)"), QRegularExpression::CaseInsensitiveOption};
            const auto dm = fn_dlc_regex.match(game_path);
            if (dm.hasMatch() && dm.hasCaptured(1)) {
                addon_count = dm.captured(1).toInt();
            }
        }

        if (!base_version.isEmpty() && base_version != QStringLiteral("1.0.0")) {
            version_num = base_version;
        } else if (version_num == QStringLiteral("1.0.0") || version_num.isEmpty()) {
            static const QRegularExpression fn_ver_regex{QStringLiteral(R"((?:[\(\[\s]v?|\b)([0-9]+\.[0-9]+(?:\.[0-9]+)*)(?!\s*(?:GB|MB|KB|TB|ГБ|МБ|КБ|Б|B)\b))")};
            const auto m = fn_ver_regex.match(game_path);
            if (m.hasMatch() && m.hasCaptured(1)) {
                version_num = m.captured(1);
            }
        }

        const QString addons_info = addon_count > 0
                                        ? QStringLiteral("%1").arg(addon_count)
                                        : QObject::tr("Нет");

        const u32 size = UISettings::values.game_icon_size.GetValue();

        QPixmap picture;
        if (!picture.loadFromData(picture_data.data(), static_cast<u32>(picture_data.size()))) {
            picture = GetDefaultIcon(size);
        }
        picture = picture.scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        setData(picture, Qt::DecorationRole);

        QPixmap tip_pix;
        if (!tip_pix.loadFromData(picture_data.data(), static_cast<u32>(picture_data.size()))) {
            tip_pix = GetDefaultIcon(96);
        }
        tip_pix = tip_pix.scaled(96, 96, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

        QByteArray bArray;
        QBuffer buffer(&bArray);
        buffer.open(QIODevice::WriteOnly);
        tip_pix.save(&buffer, "PNG");
        const QString icon_base64 = QString::fromLatin1(bArray.toBase64());

        const QString formatted_title_id =
            QString::fromStdString(fmt::format("{:016X}", program_id));
        const QString formatted_size =
            size_bytes > 0 ? QtCommon::ReadableByteSize(size_bytes) : QStringLiteral("-");

        const QString rich_tooltip = QStringLiteral(
            R"(<table style="border-collapse: collapse; width: 640px;">)"
            R"(<tr>)"
            R"(<td style="vertical-align: middle; width: 110px; padding-right: 14px; text-align: center;">)"
            R"(<img src="data:image/png;base64,%1" width="96" height="96" style="border-radius: 8px;" />)"
            R"(</td>)"
            R"(<td style="vertical-align: middle;">)"
            R"(<div style="font-size: 11.5pt; font-weight: bold; color: #ffee00; line-height: 1.25; margin-bottom: 2px;">%2</div>)"
            R"(<div style="font-size: 8.5pt; font-weight: bold; color: #00f0ff; margin-bottom: 6px;">[%3]</div>)"
            R"(<table style="font-size: 8.5pt; width: 100%%; border-collapse: collapse;">)"
            R"(<tr><td style="color: #8fa2b8; padding: 1px 6px 1px 0; width: 130px;"><b>ID приложения:</b></td><td style="color: #00f2fe; font-family: monospace;">%4</td></tr>)"
            R"(<tr><td style="color: #8fa2b8; padding: 1px 6px 1px 0;"><b>Версия:</b></td><td><b style="color: #00ffaa;">%5</b></td></tr>)"
            R"(<tr><td style="color: #8fa2b8; padding: 1px 6px 1px 0;"><b>Размер:</b></td><td style="color: #ffffff;">%6</td></tr>)"
            R"(<tr><td style="color: #8fa2b8; padding: 1px 6px 1px 0;"><b>Время игры:</b></td><td style="color: #ffffff;">%7</td></tr>)"
            R"(<tr><td style="color: #8fa2b8; padding: 1px 6px 1px 0;"><b>Дополнения:</b></td><td style="color: #ffffff;">%8</td></tr>)"
            R"(</table>)"
            R"(</td>)"
            R"(</tr>)"
            R"(<tr>)"
            R"(<td colspan="2" style="padding-top: 6px; border-top: 1px solid rgba(255, 255, 255, 0.12); font-size: 8pt; color: #8fa2b8; word-break: break-all;">)"
            R"(<b>Путь к файлу:</b> <span style="color: #cbd5e1; font-family: monospace;">%9</span>)"
            R"(</td>)"
            R"(</tr>)"
            R"(</table>)"
        ).arg(icon_base64, game_name.toHtmlEscaped(), game_type.toHtmlEscaped(), formatted_title_id, version_num, formatted_size, readable_play_time, addons_info, game_path.toHtmlEscaped());

        setData(rich_tooltip, Qt::ToolTipRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    QVariant data(int role) const override {
        if (role == Qt::DisplayRole || role == SortRole) {
            std::string filename;
            Common::SplitPath(data(FullPathRole).toString().toStdString(), nullptr, &filename,
                              nullptr);

            QString display_title = data(TitleRole).toString().trimmed();
            if (display_title.isEmpty()) {
                display_title = QString::fromStdString(filename);
            }

            const std::array<QString, 4> row_data{{
                QString::fromStdString(filename),
                data(FileTypeRole).toString(),
                QString::fromStdString(fmt::format("{:#016x}", data(ProgramIdRole).toULongLong())),
                display_title,
            }};

            const auto& row1 = row_data.at(UISettings::values.row_1_text_id.GetValue());
            // don't show row 2 on grid view
            switch (UISettings::values.game_list_mode.GetValue()) {

            case Settings::GameListMode::TreeView: {
                const int row2_id = UISettings::values.row_2_text_id.GetValue();

                if (role == SortRole) {
                    return row1.toLower();
                }

                // None
                if (row2_id == 4) {
                    return row1;
                }

                const auto& row2 = row_data.at(row2_id);

                if (row1 == row2) {
                    return row1;
                }

                return QStringLiteral("%1\n    %2").arg(row1, row2);
            }
            case Settings::GameListMode::GridView:
            case Settings::GameListMode::CarouselView:
                return row1;
            default:
                break;
            }
        }

        return GameListItem::data(role);
    }
};

class GameListItemCompat : public GameListItem {
    Q_DECLARE_TR_FUNCTIONS(GameListItemCompat)
public:
    static constexpr int CompatNumberRole = SortRole;
    GameListItemCompat() = default;
    explicit GameListItemCompat(const QString& compatibility) {
        setData(type(), TypeRole);

        struct CompatStatus {
            QString color;
            const char* text;
            const char* tooltip;
        };
        // clang-format off
        const auto ingame_status =
                       CompatStatus{QStringLiteral("#f2d624"), QT_TR_NOOP("Ingame"),     QT_TR_NOOP("Game starts, but crashes or major glitches prevent it from being completed.")};
        static const std::map<QString, CompatStatus> status_data = {
            {QStringLiteral("0"),  {QStringLiteral("#5c93ed"), QT_TR_NOOP("Perfect"),    QT_TR_NOOP("Game can be played without issues.")}},
            {QStringLiteral("1"),  {QStringLiteral("#47d35c"), QT_TR_NOOP("Playable"),   QT_TR_NOOP("Game functions with minor graphical or audio glitches and is playable from start to finish.")}},
            {QStringLiteral("2"),  ingame_status},
            {QStringLiteral("3"),  ingame_status}, // Fallback for the removed "Okay" category
            {QStringLiteral("4"),  {QStringLiteral("#FF0000"), QT_TR_NOOP("Intro/Menu"), QT_TR_NOOP("Game loads, but is unable to progress past the Start Screen.")}},
            {QStringLiteral("5"),  {QStringLiteral("#828282"), QT_TR_NOOP("Won't Boot"), QT_TR_NOOP("The game crashes when attempting to startup.")}},
            {QStringLiteral("99"), {QStringLiteral("#000000"), QT_TR_NOOP("Not Tested"), QT_TR_NOOP("The game has not yet been tested.")}},
        };
        // clang-format on

        auto iterator = status_data.find(compatibility);
        if (iterator == status_data.end()) {
            LOG_WARNING(Frontend, "Invalid compatibility number {}", compatibility.toStdString());
            return;
        }
        const CompatStatus& status = iterator->second;
        setData(compatibility, CompatNumberRole);
        setText(tr(status.text));
        setToolTip(tr(status.tooltip));
        setData(QtCommon::CreateCirclePixmapFromColor(status.color), Qt::DecorationRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    bool operator<(const QStandardItem& other) const override {
        return data(CompatNumberRole).value<QString>() <
               other.data(CompatNumberRole).value<QString>();
    }
};

class GameListItemCentered : public GameListItem {
public:
    GameListItemCentered() = default;
    explicit GameListItemCentered(const QString& string) : GameListItem(string) {}

    QVariant data(int role) const override {
        if (role == Qt::TextAlignmentRole) {
            return Qt::AlignCenter;
        }
        return GameListItem::data(role);
    }
};

class GameListItemSize : public GameListItem {
public:
    static constexpr int SizeRole = SortRole;

    GameListItemSize() = default;
    explicit GameListItemSize(const qulonglong size_bytes) {
        setData(type(), TypeRole);
        setData(size_bytes, SizeRole);
    }

    void setData(const QVariant& value, int role) override {
        if (role == SizeRole) {
            qulonglong size_bytes = value.toULongLong();
            GameListItem::setData(QtCommon::ReadableByteSize(size_bytes), Qt::DisplayRole);
            GameListItem::setData(value, SizeRole);
        } else {
            GameListItem::setData(value, role);
        }
    }

    QVariant data(int role) const override {
        if (role == Qt::TextAlignmentRole) {
            return Qt::AlignCenter;
        }
        return GameListItem::data(role);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    bool operator<(const QStandardItem& other) const override {
        return data(SizeRole).toULongLong() < other.data(SizeRole).toULongLong();
    }
};

class GameListItemPlayTime : public GameListItem {
public:
    static constexpr int PlayTimeRole = SortRole;

    GameListItemPlayTime() = default;
    explicit GameListItemPlayTime(const qulonglong time_seconds) {
        setData(time_seconds, PlayTimeRole);
    }

    void setData(const QVariant& value, int role) override {
        qulonglong time_seconds = value.toULongLong();
        GameListItem::setData(
            QString::fromStdString(PlayTime::PlayTimeManager::GetReadablePlayTime(time_seconds)),
            Qt::DisplayRole);
        GameListItem::setData(value, PlayTimeRole);
    }

    QVariant data(int role) const override {
        if (role == Qt::TextAlignmentRole) {
            return Qt::AlignCenter;
        }
        return GameListItem::data(role);
    }

    bool operator<(const QStandardItem& other) const override {
        return data(PlayTimeRole).toULongLong() < other.data(PlayTimeRole).toULongLong();
    }
};

class GameListDir : public GameListItem {
public:
    static constexpr int GameDirRole = Qt::UserRole + 2;

    explicit GameListDir(UISettings::GameDir& directory,
                         GameListItemType dir_type_ = GameListItemType::CustomDir)
        : dir_type{dir_type_} {
        setData(type(), TypeRole);

        UISettings::GameDir* game_dir = &directory;
        setData(QVariant(UISettings::values.game_dirs.indexOf(directory)), GameDirRole);

        const char* icon_name = nullptr;

        switch (dir_type) {
        case GameListItemType::SdmcDir:
            icon_name = "sd_card";
            setData(QObject::tr("Установленные на SD игры"), Qt::DisplayRole);
            break;
        case GameListItemType::UserNandDir:
            icon_name = "chip";
            setData(QObject::tr("Установленные в NAND игры"), Qt::DisplayRole);
            break;
        case GameListItemType::SysNandDir:
            icon_name = "chip";
            setData(QObject::tr("Системные программы"), Qt::DisplayRole);
            break;
        case GameListItemType::CustomDir: {
            const QString path = QString::fromStdString(game_dir->path);
            icon_name = QFileInfo::exists(path) ? "folder" : "bad_folder";
            setData(path, Qt::DisplayRole);
            break;
        }
        default:
            break;
        }

        if (icon_name != nullptr)
            setData(ThemeIcon(icon_name), Qt::DecorationRole);
    }

    int type() const override {
        return static_cast<int>(dir_type);
    }

    QVariant data(int role) const override {
        if (role == Qt::FontRole) {
            QFont font;
            font.setFamily(QStringLiteral("Segoe UI"));
            font.setBold(true);
            font.setPointSizeF(9.5);
            return font;
        }
        return GameListItem::data(role);
    }

    bool operator<(const QStandardItem& other) const override {
        return false;
    }

private:
    GameListItemType dir_type;
};

class GameListAddDir : public GameListItem {
public:
    explicit GameListAddDir() {
        setData(type(), TypeRole);

        setData(ThemeIcon("list-add"), Qt::DecorationRole);
        setData(QObject::tr("Добавить новую папку с играми"), Qt::DisplayRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::AddDir);
    }

    QVariant data(int role) const override {
        if (role == Qt::FontRole) {
            QFont font;
            font.setFamily(QStringLiteral("Segoe UI"));
            font.setBold(true);
            font.setPointSizeF(9.5);
            return font;
        }
        return GameListItem::data(role);
    }

    bool operator<(const QStandardItem& other) const override {
        return false;
    }
};

class GameListFavorites : public GameListItem {
public:
    explicit GameListFavorites() {
        setData(type(), TypeRole);

        setData(ThemeIcon("star"), Qt::DecorationRole);
        setData(QObject::tr("Избранное"), Qt::DisplayRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Favorites);
    }

    QVariant data(int role) const override {
        if (role == Qt::FontRole) {
            QFont font;
            font.setFamily(QStringLiteral("Segoe UI"));
            font.setBold(true);
            font.setPointSizeF(9.5);
            return font;
        }
        return GameListItem::data(role);
    }

    bool operator<(const QStandardItem& other) const override {
        return false;
    }
};
