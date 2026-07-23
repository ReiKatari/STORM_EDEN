// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <map>
#include <string>
#include <utility>

#include <QCoreApplication>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QStandardItem>
#include <QString>
#include <QBuffer>
#include <QWidget>

#include "common/common_types.h"
#include "common/logging.h"
#include "common/string_util.h"
#include "frontend_common/play_time_manager.h"
#include "qt_common/config/uisettings.h"
#include "eden/util/util.h"

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

/**
 * Gets the default icon (for games without valid title metadata)
 * @param size The desired width and height of the default icon.
 * @return QPixmap default icon
 */
static QPixmap GetDefaultIcon(u32 size) {
    QIcon icon(QStringLiteral(":/img/eden.ico"));
    if (!icon.isNull()) {
        return icon.pixmap(size, size);
    }
    QPixmap default_icon(size, size);
    default_icon.fill(Qt::transparent);
    return default_icon;
}

class GameListItem : public QStandardItem {

public:
    // used to access type from item index
    static constexpr int TypeRole = Qt::UserRole + 1;
    static constexpr int SortRole = Qt::UserRole + 2;
    static constexpr int CustomBackgroundRole = Qt::UserRole + 42;
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
static QString BuildGameTooltip(const QString& game_name, const QString& game_path,
                                const QString& game_type, u64 program_id, u64 play_time,
                                const QString& patch_versions, u64 size_bytes, const QPixmap& picture) {
    QString base64_icon;
    if (!picture.isNull()) {
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        picture.save(&buffer, "PNG");
        base64_icon = QString::fromLatin1(ba.toBase64());
    } else {
        QPixmap default_pic = GetDefaultIcon(128);
        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        default_pic.save(&buffer, "PNG");
        base64_icon = QString::fromLatin1(ba.toBase64());
    }

    const auto readable_play_time =
        play_time > 0 ? QString::fromStdString(
                                PlayTime::PlayTimeManager::GetReadablePlayTime(play_time))
                      : QStringLiteral("Нет");

    const auto enabled_update = [patch_versions]() -> QString {
        const QStringList lines = patch_versions.split(QLatin1Char('\n'));
        const QRegularExpression regex{QStringLiteral(R"(^(Update|Версия|Version).*\(([^)]+)\))")};
        for (const QString& line : std::as_const(lines)) {
            const auto match = regex.match(line);
            if (match.hasMatch() && match.hasCaptured(2))
                return match.captured(2);
        }
        return QStringLiteral("1.0.0");
    }();

    QString readable_size = size_bytes > 0 ? ReadableByteSize(size_bytes) : QObject::tr("Unknown Size");

    QStringList dlc_list;
    const QStringList lines = patch_versions.split(QLatin1Char('\n'));
    const QRegularExpression dlc_regex{QStringLiteral(R"(^(DLC|Дополнения).*\(([^)]+)\))")};
    for (const QString& line : std::as_const(lines)) {
        const auto match = dlc_regex.match(line);
        if (match.hasMatch() && match.hasCaptured(2)) {
            dlc_list << match.captured(2);
        }
    }
    QString dlc_str = dlc_list.isEmpty() ? QString::fromUtf8("Нет") : dlc_list.join(QStringLiteral(", "));

    QString html = QStringLiteral(
        "<html><body style=\"margin: 0; padding: 0;\">"
        "<table width=\"100%\" height=\"100%\" bgcolor=\"#121214\" style=\"border-collapse: collapse; font-family: 'Segoe UI', Arial, sans-serif; font-size: 11pt; color: #E0E0E0;\" cellpadding=\"8\">"
        "<tr><td>"
        "<table style=\"border-collapse: collapse; margin: 0px;\" cellpadding=\"8\">"
        "<tr>"
        "  <td valign=\"top\" style=\"padding-right: 14px;\">"
        "    <div style=\"background-color: #1e1e24; border: 2px solid #32323e; border-radius: 12px; padding: 4px; box-shadow: 0px 4px 10px rgba(0, 0, 0, 0.4);\">"
        "      <img src=\"data:image/png;base64,%1\" width=\"112\" height=\"112\" style=\"border-radius: 8px;\" />"
        "    </div>"
        "  </td>"
        "  <td valign=\"top\" style=\"min-width: 280px;\">"
        "    <div style=\"font-size: 13pt; font-weight: bold; color: #00FFCC; margin-bottom: 6px; line-height: 1.2;\">%2</div>"
        "    <table style=\"font-size: 10pt; color: #D0D0DB;\" cellpadding=\"2\">"
        "      <tr><td><b>%3:</b></td><td><span style=\"background-color: #2a2a35; border: 1px solid #444455; border-radius: 4px; padding: 1px 6px; font-family: 'Consolas', monospace; font-weight: bold; color: #FFFFFF;\">0x%4</span></td></tr>"
        "      <tr><td><b>%5:</b></td><td><span style=\"background-color: #2a2a35; border: 1px solid #444455; border-radius: 4px; padding: 1px 6px; font-weight: bold; color: #FFFFFF;\">%6</span></td></tr>"
        "      <tr><td><b>%7:</b></td><td><span style=\"background-color: #1e2d2f; border: 1px solid #1f5f5b; border-radius: 4px; padding: 1px 6px; font-weight: bold; color: #00FFCC;\">%8</span></td></tr>"
        "      <tr><td><b>%9:</b></td><td><span style=\"background-color: #2a2a35; border: 1px solid #444455; border-radius: 4px; padding: 1px 6px; font-weight: bold; color: #FFFFFF;\">%10</span></td></tr>"
        "      <tr><td><b>%11:</b></td><td><span style=\"background-color: #2a2a35; border: 1px solid #444455; border-radius: 4px; padding: 1px 6px; font-weight: bold; color: #FFFFFF;\">%12</span></td></tr>"
        "      <tr><td><b>%13:</b></td><td><span style=\"background-color: #2a2a35; border: 1px solid #444455; border-radius: 4px; padding: 1px 6px; font-weight: bold; color: #FFFFFF;\">%14</span></td></tr>"
        "    </table>"
        "  </td>"
        "</tr>"
        "</table>"
        "<div style=\"font-size: 8.5pt; color: #6E6E7A; border-top: 1px solid #222228; padding: 6px 12px; word-break: break-all;\"><b style=\"color: #FFFFFF;\">%15:</b> %16</div>"
        "</td></tr></table>"
        "</body></html>"
    )
    .arg(base64_icon)
    .arg(game_name.isEmpty() ? QString::fromUtf8("Неизвестное название") : game_name)
    .arg(QString::fromUtf8("ID Игры"))
    .arg(QString::fromStdString(fmt::format("{:016X}", program_id)))
    .arg(QString::fromUtf8("Формат"))
    .arg(game_type)
    .arg(QString::fromUtf8("Версия"))
    .arg(enabled_update)
    .arg(QString::fromUtf8("Размер"))
    .arg(readable_size)
    .arg(QString::fromUtf8("Время в игре"))
    .arg(readable_play_time)
    .arg(QString::fromUtf8("DLC"))
    .arg(dlc_str)
    .arg(QString::fromUtf8("Путь"))
    .arg(game_path);

    return html;
}

class GameListItemPath : public GameListItem {
public:
    static constexpr int FullPathRole = SortRole + 1;
    static constexpr int TitleRole = SortRole + 2;
    static constexpr int ProgramIdRole = SortRole + 3;
    static constexpr int FileTypeRole = SortRole + 4;

    GameListItemPath() = default;
    GameListItemPath(const QString& game_path, const QPixmap& picture,
                     const QString& game_name, const QString& game_type, u64 program_id,
                     u64 play_time, const QString& patch_versions, u64 size_bytes = 0) {
        setData(type(), TypeRole);
        setData(game_path, FullPathRole);
        setData(game_name, TitleRole);
        setData(qulonglong(program_id), ProgramIdRole);
        setData(game_type, FileTypeRole);

        const auto tooltip = BuildGameTooltip(game_name, game_path, game_type, program_id, play_time, patch_versions, size_bytes, picture);
        setData(tooltip, Qt::ToolTipRole);

        const u32 icon_size = UISettings::values.game_icon_size.GetValue();
        QPixmap list_icon = picture.scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        setData(list_icon, Qt::DecorationRole);
    }
    GameListItemPath(const QString& game_path, const std::vector<u8>& picture_data,
                     const QString& game_name, const QString& game_type, u64 program_id,
                     u64 play_time, const QString& patch_versions, u64 size_bytes = 0) {
        setData(type(), TypeRole);
        setData(game_path, FullPathRole);
        setData(game_name, TitleRole);
        setData(qulonglong(program_id), ProgramIdRole);
        setData(game_type, FileTypeRole);

        QPixmap picture;
        if (!picture.loadFromData(picture_data.data(), static_cast<u32>(picture_data.size()))) {
            picture = GetDefaultIcon(128);
        }

        const auto tooltip = BuildGameTooltip(game_name, game_path, game_type, program_id, play_time, patch_versions, size_bytes, picture);
        setData(tooltip, Qt::ToolTipRole);

        const u32 icon_size = UISettings::values.game_icon_size.GetValue();
        QPixmap list_icon = picture.scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        setData(list_icon, Qt::DecorationRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    QVariant data(int role) const override {
        if (role == Qt::DisplayRole || role == SortRole) {
            std::string filename;
            Common::SplitPath(data(FullPathRole).toString().toStdString(), nullptr, &filename,
                              nullptr);

            const std::array<QString, 4> row_data{{
                QString::fromStdString(filename),
                data(FileTypeRole).toString(),
                QString::fromStdString(fmt::format("0x{:016X}", data(ProgramIdRole).toULongLong())),
                data(TitleRole).toString(),
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
            {QStringLiteral("99"), {QStringLiteral("#a0a0b0"), QT_TR_NOOP("Not Tested"), QT_TR_NOOP("The game has not yet been tested.")}},
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
        setData(CreateCirclePixmapFromColor(status.color), Qt::DecorationRole);
        setData(static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter), Qt::TextAlignmentRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    bool operator<(const QStandardItem& other) const override {
        return data(CompatNumberRole).value<QString>() <
               other.data(CompatNumberRole).value<QString>();
    }
};

/**
 * A specialization of GameListItem for size values.
 * This class ensures that for every numerical size value it holds (in bytes), a correct
 * human-readable string representation will be displayed to the user.
 */
class GameListItemSize : public GameListItem {
public:
    static constexpr int SizeRole = SortRole;

    GameListItemSize() = default;
    explicit GameListItemSize(const qulonglong size_bytes) {
        setData(type(), TypeRole);
        setData(size_bytes, SizeRole);
    }

    void setData(const QVariant& value, int role) override {
        // By specializing setData for SizeRole, we can ensure that the numerical and string
        // representations of the data are always accurate and in the correct format.
        if (role == SizeRole) {
            qulonglong size_bytes = value.toULongLong();
            GameListItem::setData(ReadableByteSize(size_bytes), Qt::DisplayRole);
            GameListItem::setData(value, SizeRole);
        } else {
            GameListItem::setData(value, role);
        }
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    /**
     * This operator is, in practice, only used by the TreeView sorting systems.
     * Override it so that it will correctly sort by numerical value instead of by string
     * representation.
     */
    bool operator<(const QStandardItem& other) const override {
        return data(SizeRole).toULongLong() < other.data(SizeRole).toULongLong();
    }
};

/**
 * GameListItem for Play Time values.
 * This object stores the play time of a game in seconds, and its readable
 * representation in minutes/hours
 */
class GameListItemPlayTime : public GameListItem {
public:
    static constexpr int PlayTimeRole = SortRole;

    GameListItemPlayTime() = default;
    explicit GameListItemPlayTime(const qulonglong time_seconds) {
        setData(time_seconds, PlayTimeRole);
    }

    void setData(const QVariant& value, int role) override {
        if (role == PlayTimeRole) {
            qulonglong time_seconds = value.toULongLong();
            GameListItem::setData(
                QString::fromStdString(PlayTime::PlayTimeManager::GetReadablePlayTime(time_seconds)),
                Qt::DisplayRole);
            GameListItem::setData(value, PlayTimeRole);
        } else {
            GameListItem::setData(value, role);
        }
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
        setData(QBrush(QColor(100, 100, 100, 120)), CustomBackgroundRole); // Stronger pale highlight

        UISettings::GameDir* game_dir = &directory;
        setData(QVariant(UISettings::values.game_dirs.indexOf(directory)), GameDirRole);

        const int icon_size = UISettings::values.folder_icon_size.GetValue();
        switch (dir_type) {
        case GameListItemType::SdmcDir:
            setData(
                QIcon::fromTheme(QStringLiteral("sd_card"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            setData(QObject::tr("Installed SD Titles"), Qt::DisplayRole);
            break;
        case GameListItemType::UserNandDir:
            setData(
                QIcon::fromTheme(QStringLiteral("chip"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            setData(QObject::tr("Installed NAND Titles"), Qt::DisplayRole);
            break;
        case GameListItemType::SysNandDir:
            setData(
                QIcon::fromTheme(QStringLiteral("chip"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            setData(QObject::tr("System Titles"), Qt::DisplayRole);
            break;
        case GameListItemType::CustomDir: {
            const QString path = QString::fromStdString(game_dir->path);
            const QString icon_name =
                QFileInfo::exists(path) ? QStringLiteral("folder") : QStringLiteral("bad_folder");
            setData(QIcon::fromTheme(icon_name).pixmap(icon_size).scaled(
                        icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                    Qt::DecorationRole);
            setData(path, Qt::DisplayRole);
            break;
        }
        default:
            break;
        }
    }

    int type() const override {
        return static_cast<int>(dir_type);
    }

    /**
     * Override to prevent automatic sorting between folders and the addDir button.
     */
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
        setData(QBrush(QColor(100, 100, 100, 120)), CustomBackgroundRole); // Stronger pale highlight

        const int icon_size = UISettings::values.folder_icon_size.GetValue();

        setData(QIcon::fromTheme(QStringLiteral("list-add"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
        setData(QObject::tr("Add New Game Directory"), Qt::DisplayRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::AddDir);
    }

    bool operator<(const QStandardItem& other) const override {
        return false;
    }
};

class GameListFavorites : public GameListItem {
public:
    explicit GameListFavorites() {
        setData(type(), TypeRole);
        setData(QBrush(QColor(80, 120, 150, 100)), CustomBackgroundRole); // Distinct blueish pale highlight for Add New Directory

        const int icon_size = UISettings::values.folder_icon_size.GetValue();

        setData(QIcon::fromTheme(QStringLiteral("star"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
        setData(QObject::tr("Favorites"), Qt::DisplayRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Favorites);
    }

    bool operator<(const QStandardItem& other) const override {
        return false;
    }
};

class GameList;
class QHBoxLayout;
class QTreeView;
class QLabel;
class QLineEdit;
class QToolButton;

class GameListSearchField : public QWidget {
    Q_OBJECT

public:
    explicit GameListSearchField(GameList* parent = nullptr);

    QString filterText() const;
    void setFilterResult(int visible_, int total_);

    void clear();
    void setFocus();

private:
    void changeEvent(QEvent*) override;
    void RetranslateUI();

    class KeyReleaseEater : public QObject {
    public:
        explicit KeyReleaseEater(GameList* gamelist_, QObject* parent = nullptr);

    private:
        GameList* gamelist = nullptr;
        QString edit_filter_text_old;

    protected:
        // EventFilter in order to process systemkeys while editing the searchfield
        bool eventFilter(QObject* obj, QEvent* event) override;
    };
    int visible;
    int total;

    QHBoxLayout* layout_filter = nullptr;
    QTreeView* tree_view = nullptr;
    QLabel* label_filter = nullptr;
    QLineEdit* edit_filter = nullptr;
    QLabel* label_filter_result = nullptr;
    QToolButton* button_filter_close = nullptr;
};
