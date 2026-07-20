// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dump_settings.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QListView>
#include <QMenu>
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>
#include <QThreadPool>
#include <QToolButton>
#include <QVariantAnimation>
#include <fmt/ranges.h>
#include <qnamespace.h>
#include <fstream>
#include "common/common_types.h"
#include "common/logging.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "game/game_card.h"
#include "qt_common/config/uisettings.h"
#include "qt_common/qt_common.h"
#include "qt_common/util/game.h"
#include "eden/compatibility_list.h"
#include "eden/game/game_list.h"
#include "eden/game/game_list_p.h"
#include "eden/game/game_list_worker.h"
#include "eden/main_window.h"
#include "eden/util/controller_navigation.h"

#include <QStyledItemDelegate>
#include <QPainter>

class GameListDelegate : public QStyledItemDelegate {
public:
    explicit GameListDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        // Draw background explicitly if set via CustomBackgroundRole
        QVariant background = index.data(GameListItem::CustomBackgroundRole);
        if (background.isValid()) {
            painter->fillRect(opt.rect, background.value<QBrush>());
        }

        // Draw default item contents (icon, text) without a stylesheet interfering
        QStyledItemDelegate::paint(painter, opt, index);

        // Draw the right border line using the system palette mid color
        painter->setPen(opt.palette.color(QPalette::Mid));
        painter->drawLine(opt.rect.topRight(), opt.rect.bottomRight());
    }
};

class GameListTreeView : public QTreeView {
public:
    explicit GameListTreeView(QWidget* parent = nullptr) : QTreeView(parent) {}
protected:
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        // Let standard row drawing occur
        QTreeView::drawRow(painter, option, index);
        
        // Draw bottom separator across the ENTIRE row width (completely reaching the left viewport edge)
        painter->setPen(palette().color(QPalette::Mid));
        painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
        
        // Draw absolute leftmost vertical boundary line
        painter->drawLine(option.rect.topLeft(), option.rect.bottomLeft());
    }
};

GameListSearchField::KeyReleaseEater::KeyReleaseEater(GameList* gamelist_, QObject* parent)
    : QObject(parent), gamelist{gamelist_} {}

// EventFilter in order to process systemkeys while editing the searchfield
bool GameListSearchField::KeyReleaseEater::eventFilter(QObject* obj, QEvent* event) {
    // If it isn't a KeyRelease event then continue with standard event processing
    if (event->type() != QEvent::KeyRelease)
        return QObject::eventFilter(obj, event);

    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
    QString edit_filter_text = gamelist->search_field->edit_filter->text().toLower();

    // If the searchfield's text hasn't changed special function keys get checked
    // If no function key changes the searchfield's text the filter doesn't need to get reloaded
    if (edit_filter_text == edit_filter_text_old) {
        switch (keyEvent->key()) {
        // Escape: Resets the searchfield
        case Qt::Key_Escape: {
            if (edit_filter_text_old.isEmpty()) {
                return QObject::eventFilter(obj, event);
            } else {
                gamelist->search_field->edit_filter->clear();
                edit_filter_text.clear();
            }
            break;
        }
        // Return and Enter
        // If the enter key gets pressed first checks how many and which entry is visible
        // If there is only one result launch this game
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            if (gamelist->search_field->visible == 1) {
                const QString file_path = gamelist->GetLastFilterResultItem();

                // To avoid loading error dialog loops while confirming them using enter
                // Also users usually want to run a different game after closing one
                gamelist->search_field->edit_filter->clear();
                edit_filter_text.clear();
                emit gamelist->GameChosen(file_path);
            } else {
                return QObject::eventFilter(obj, event);
            }
            break;
        }
        default:
            return QObject::eventFilter(obj, event);
        }
    }
    edit_filter_text_old = edit_filter_text;
    return QObject::eventFilter(obj, event);
}

void GameListSearchField::setFilterResult(int visible_, int total_) {
    visible = visible_;
    total = total_;

    label_filter_result->setText(tr("%1 of %n result(s)", "", total).arg(visible));
}

QString GameListSearchField::filterText() const {
    return edit_filter->text();
}

QString GameList::GetLastFilterResultItem() const {
    QString file_path;

    for (int i = 1; i < item_model->rowCount() - 1; ++i) {
        const QStandardItem* folder = item_model->item(i, 0);
        const QModelIndex folder_index = folder->index();
        const int children_count = folder->rowCount();

        for (int j = 0; j < children_count; ++j) {
            if (tree_view->isRowHidden(j, folder_index)) {
                continue;
            }

            const QStandardItem* child = folder->child(j, 0);
            file_path = child->data(GameListItemPath::FullPathRole).toString();
        }
    }

    return file_path;
}

void GameListSearchField::clear() {
    edit_filter->clear();
}

void GameListSearchField::setFocus() {
    if (edit_filter->isVisible()) {
        edit_filter->setFocus();
    }
}

GameListSearchField::GameListSearchField(GameList* parent) : QWidget{parent} {
    auto* const key_release_eater = new KeyReleaseEater(parent, this);
    layout_filter = new QHBoxLayout;
    layout_filter->setContentsMargins(8, 8, 8, 8);
    label_filter = new QLabel;
    edit_filter = new QLineEdit;
    edit_filter->clear();
    edit_filter->installEventFilter(key_release_eater);
    edit_filter->setClearButtonEnabled(true);
    connect(edit_filter, &QLineEdit::textChanged, parent, &GameList::OnTextChanged);
    label_filter_result = new QLabel;
    button_filter_close = new QToolButton(this);
    button_filter_close->setText(QStringLiteral("X"));
    button_filter_close->setCursor(Qt::ArrowCursor);
    button_filter_close->setStyleSheet(
        QStringLiteral("QToolButton{ border: none; padding: 0px; color: "
                       "#000000; font-weight: bold; background: #F0F0F0; }"
                       "QToolButton:hover{ border: none; padding: 0px; color: "
                       "#EEEEEE; font-weight: bold; background: #E81123}"));
    connect(button_filter_close, &QToolButton::clicked, parent, &GameList::OnFilterCloseClicked);
    layout_filter->setSpacing(10);
    layout_filter->addWidget(label_filter);
    layout_filter->addWidget(edit_filter);
    layout_filter->addWidget(label_filter_result);
    layout_filter->addWidget(button_filter_close);
    setLayout(layout_filter);
    RetranslateUI();
}

/**
 * Checks if all words separated by spaces are contained in another string
 * This offers a word order insensitive search function
 *
 * @param haystack String that gets checked if it contains all words of the userinput string
 * @param userinput String containing all words getting checked
 * @return true if the haystack contains all words of userinput
 */
static bool ContainsAllWords(const QString& haystack, const QString& userinput) {
    const QStringList userinput_split = userinput.split(QLatin1Char{' '}, Qt::SkipEmptyParts);

    return std::all_of(userinput_split.begin(), userinput_split.end(),
                       [&haystack](const QString& s) { return haystack.contains(s); });
}

// Syncs the expanded state of Game Directories with settings to persist across sessions
void GameList::OnItemExpanded(const QModelIndex& item) {
    const auto type = item.data(GameListItem::TypeRole).value<GameListItemType>();
    const bool is_dir = type == GameListItemType::CustomDir || type == GameListItemType::SdmcDir ||
                        type == GameListItemType::UserNandDir ||
                        type == GameListItemType::SysNandDir;
    const bool is_fave = type == GameListItemType::Favorites;
    if (!is_dir && !is_fave) {
        return;
    }
    const bool is_expanded = tree_view->isExpanded(item);
    if (is_fave) {
        UISettings::values.favorites_expanded = is_expanded;
        return;
    }
    const int item_dir_index = item.data(GameListDir::GameDirRole).toInt();
    UISettings::values.game_dirs[item_dir_index].expanded = is_expanded;
}

// Event in order to filter the gamelist after editing the searchfield
void GameList::OnTextChanged(const QString& new_text) {
    QString edit_filter_text = new_text.toLower();
    QStandardItem* folder;
    int children_total = 0;
    int result_count = 0;

    auto hide = [this](int row, bool hidden, QModelIndex index = QModelIndex()) {
        if (m_isTreeMode) {
            tree_view->setRowHidden(row, index, hidden);
        } else {
            list_view->setRowHidden(row, hidden);
        }
    };

    // If the searchfield is empty every item is visible
    // Otherwise the filter gets applied

    // TODO(crueter) dedupe
    if (!m_isTreeMode) {
        int row_count = item_model->rowCount();

        for (int i = 0; i < row_count; ++i) {
            QStandardItem* item = item_model->item(i, 0);
            if (!item)
                continue;

            children_total++;

            const QString file_path =
                item->data(GameListItemPath::FullPathRole).toString().toLower();
            const QString file_title = item->data(GameListItemPath::TitleRole).toString().toLower();
            const QString file_name = file_path.mid(file_path.lastIndexOf(QLatin1Char{'/'}) + 1) +
                                      QLatin1Char{' '} + file_title;

            if (edit_filter_text.isEmpty() || ContainsAllWords(file_name, edit_filter_text)) {
                hide(i, false);
                result_count++;
            } else {
                hide(i, true);
            }
        }
        search_field->setFilterResult(result_count, children_total);
    } else if (edit_filter_text.isEmpty()) {
        hide(0, UISettings::values.favorited_ids.size() == 0,
             item_model->invisibleRootItem()->index());
        for (int i = 1; i < item_model->rowCount() - 1; ++i) {
            folder = item_model->item(i, 0);
            const QModelIndex folder_index = folder->index();
            const int children_count = folder->rowCount();
            for (int j = 0; j < children_count; ++j) {
                ++children_total;
                hide(j, false, folder_index);
            }
        }
        search_field->setFilterResult(children_total, children_total);
    } else {
        hide(0, true, item_model->invisibleRootItem()->index());
        for (int i = 1; i < item_model->rowCount() - 1; ++i) {
            folder = item_model->item(i, 0);
            const QModelIndex folder_index = folder->index();
            const int children_count = folder->rowCount();
            for (int j = 0; j < children_count; ++j) {
                ++children_total;

                const QStandardItem* child = folder->child(j, 0);

                const auto program_id = child->data(GameListItemPath::ProgramIdRole).toULongLong();

                const QString file_path =
                    child->data(GameListItemPath::FullPathRole).toString().toLower();
                const QString file_title =
                    child->data(GameListItemPath::TitleRole).toString().toLower();
                const QString file_program_id =
                    QStringLiteral("%1").arg(program_id, 16, 16, QLatin1Char{'0'});

                // Only items which filename in combination with its title contains all words
                // that are in the searchfield will be visible in the gamelist
                // The search is case insensitive because of toLower()
                // I decided not to use Qt::CaseInsensitive in containsAllWords to prevent
                // multiple conversions of edit_filter_text for each game in the gamelist
                const QString file_name =
                    file_path.mid(file_path.lastIndexOf(QLatin1Char{'/'}) + 1) + QLatin1Char{' '} +
                    file_title;
                if (ContainsAllWords(file_name, edit_filter_text) ||
                    (file_program_id.size() == 16 && file_program_id.contains(edit_filter_text))) {
                    hide(j, false, folder_index);
                    ++result_count;
                } else {
                    hide(j, true, folder_index);
                }
            }
        }
        search_field->setFilterResult(result_count, children_total);
    }
}

void GameList::OnUpdateThemedIcons() {
    for (int i = 0; i < item_model->invisibleRootItem()->rowCount(); i++) {
        QStandardItem* child = item_model->invisibleRootItem()->child(i);

        const int icon_size = UISettings::values.folder_icon_size.GetValue();

        switch (child->data(GameListItem::TypeRole).value<GameListItemType>()) {
        case GameListItemType::SdmcDir:
            child->setData(
                QIcon::fromTheme(QStringLiteral("sd_card"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            break;
        case GameListItemType::UserNandDir:
            child->setData(
                QIcon::fromTheme(QStringLiteral("chip"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            break;
        case GameListItemType::SysNandDir:
            child->setData(
                QIcon::fromTheme(QStringLiteral("chip"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            break;
        case GameListItemType::CustomDir: {
            const UISettings::GameDir& game_dir =
                UISettings::values.game_dirs[child->data(GameListDir::GameDirRole).toInt()];
            const QString icon_name = QFileInfo::exists(QString::fromStdString(game_dir.path))
                                          ? QStringLiteral("folder")
                                          : QStringLiteral("bad_folder");
            child->setData(
                QIcon::fromTheme(icon_name).pixmap(icon_size).scaled(
                    icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            break;
        }
        case GameListItemType::AddDir:
            child->setData(
                QIcon::fromTheme(QStringLiteral("list-add"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            break;
        case GameListItemType::Favorites:
            child->setData(
                QIcon::fromTheme(QStringLiteral("star"))
                    .pixmap(icon_size)
                    .scaled(icon_size, icon_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation),
                Qt::DecorationRole);
            break;
        default:
            break;
        }
    }
}

void GameList::OnFilterCloseClicked() {
    main_window->filterBarSetChecked(false);
}

GameList::GameList(FileSys::VirtualFilesystem vfs_, FileSys::ManualContentProvider* provider_,
                   PlayTime::PlayTimeManager& play_time_manager_, Core::System& system_,
                   MainWindow* parent)
    : QWidget{parent}, vfs{std::move(vfs_)}, provider{provider_},
      play_time_manager{play_time_manager_}, system{system_} {
    {
    }
    {
    }
    watcher = new QFileSystemWatcher(this);
    {
    }
    connect(watcher, &QFileSystemWatcher::directoryChanged, this, &GameList::RefreshGameDirectory);

    {
    }
    external_watcher = new QFileSystemWatcher(this);
    {
    }
    ResetExternalWatcher();
    {
    }
    connect(external_watcher, &QFileSystemWatcher::directoryChanged, this,
            &GameList::RefreshExternalContent);
    {
    }

    this->main_window = parent;
    layout = new QVBoxLayout;
    tree_view = new GameListTreeView(this);
    list_view = new QListView(this);
    m_gameCard = new GameCard(this);

    list_view->setItemDelegate(m_gameCard);
    {
    }

    {
    }
    controller_navigation = new ControllerNavigation(system.HIDCore(), this);
    {
    }
    search_field = new GameListSearchField(this);
    item_model = new QStandardItemModel(tree_view);
    tree_view->setModel(item_model);
    list_view->setModel(item_model);
    {
    }

    SetupScrollAnimation();

    // tree
    tree_view->setAlternatingRowColors(true);
    tree_view->setSelectionMode(QHeaderView::SingleSelection);
    tree_view->setSelectionBehavior(QHeaderView::SelectRows);
    tree_view->setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setSortingEnabled(true);
    tree_view->setEditTriggers(QHeaderView::NoEditTriggers);
    tree_view->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_view->setAttribute(Qt::WA_AcceptTouchEvents, true);
    tree_view->setStyleSheet(QStringLiteral(
        "QTreeView { border: none; show-decoration-selected: 0; }\n"
        "QTreeView::branch { border: none; background: transparent; }\n"
        "QHeaderView { border-top: 1px solid palette(mid); border-bottom: 1px solid palette(mid); background: transparent; }\n"
        "QHeaderView::section { font-weight: bold; border: none; border-right: 1px solid palette(mid); border-left: 1px solid palette(mid); padding: 4px; }\n"
    ));
    tree_view->header()->setDefaultAlignment(Qt::AlignCenter);
    
    // Interactive Headers: Context menu to toggle column visibility
    tree_view->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_view->header(), &QHeaderView::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu;
        for (int i = 1; i < COLUMN_COUNT; ++i) { // Skip NAME column
            QAction* action = menu.addAction(item_model->headerData(i, Qt::Horizontal).toString());
            action->setCheckable(true);
            action->setChecked(!tree_view->isColumnHidden(i));
            connect(action, &QAction::triggered, this, [this, i](bool checked) {
                tree_view->setColumnHidden(i, !checked);
                // Save layout changes to settings
                SaveInterfaceLayout();
            });
        }
        menu.exec(tree_view->header()->mapToGlobal(pos));
    });

    // list view setup
    list_view->setViewMode(QListView::ListMode);
    list_view->setResizeMode(QListView::Fixed);
    list_view->setUniformItemSizes(true);
    list_view->setSelectionMode(QAbstractItemView::SingleSelection);
    list_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    list_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    // Forcefully disable scroll bar, prevents thing where game list items
    // will start clamping prematurely.
    list_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    list_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    list_view->setContextMenuPolicy(Qt::CustomContextMenu);
    list_view->setGridSize(QSize(140, 160));
    m_gameCard->setSize(list_view->gridSize(), 0, 4);

    list_view->setSpacing(10);
    list_view->setWordWrap(true);
    list_view->setTextElideMode(Qt::ElideRight);
    list_view->setFlow(QListView::LeftToRight);
    list_view->setWrapping(true);

    item_model->insertColumns(0, COLUMN_COUNT);
    
    // Set custom delegate for TreeView grid lines
    auto* delegate = new GameListDelegate(this);
    tree_view->setItemDelegate(delegate);
    
    RetranslateUI();

    tree_view->setColumnHidden(COLUMN_ADD_ONS, !UISettings::values.show_add_ons);
    tree_view->setColumnHidden(COLUMN_COMPATIBILITY, !UISettings::values.show_compat);
    tree_view->setColumnHidden(COLUMN_PLAY_TIME, !UISettings::values.show_play_time);
    item_model->setSortRole(GameListItemPath::SortRole);

    connect(main_window, &MainWindow::UpdateThemedIcons, this, &GameList::OnUpdateThemedIcons);

    connect(tree_view, &QTreeView::activated, this, &GameList::ValidateEntry);
    connect(tree_view, &QTreeView::customContextMenuRequested, this, &GameList::PopupContextMenu);

    connect(list_view, &QListView::activated, this, &GameList::ValidateEntry);
    connect(list_view, &QListView::customContextMenuRequested, this, &GameList::PopupContextMenu);

    connect(tree_view, &QTreeView::expanded, this, &GameList::OnItemExpanded);
    connect(tree_view, &QTreeView::collapsed, this, &GameList::OnItemExpanded);
    connect(controller_navigation, &ControllerNavigation::TriggerKeyboardEvent, this,
            [this](Qt::Key key) {
                // Avoid pressing buttons while playing
                if (system.IsPoweredOn()) {
                    return;
                }
                if (!this->isActiveWindow()) {
                    return;
                }
                QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier);

                QCoreApplication::postEvent(m_currentView, event);
            });

    // We must register all custom types with the Qt Automoc system so that we are able to use
    // it with signals/slots. In this case, QList falls under the umbrella of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(tree_view);
    layout->addWidget(list_view);
    layout->addWidget(search_field);
    setLayout(layout);

    ResetViewMode();
    LoadCache();
}

void GameList::UnloadController() {
    controller_navigation->UnloadController();
}

bool GameList::IsTreeMode() {
    return m_isTreeMode;
}

void GameList::ResetViewMode() {
    auto& setting = UISettings::values.game_list_mode;
    bool newTreeMode = false;

    switch (setting.GetValue()) {
    case Settings::GameListMode::TreeView:
        m_currentView = tree_view;
        newTreeMode = true;

        tree_view->setVisible(true);
        list_view->setVisible(false);
        break;
    case Settings::GameListMode::GridView:
        m_currentView = list_view;
        newTreeMode = false;

        list_view->setVisible(true);
        tree_view->setVisible(false);
        break;
    default:
        break;
    }

    auto view = m_currentView->viewport();
    view->installEventFilter(this);

    // touch gestures
    view->grabGesture(Qt::SwipeGesture);
    view->grabGesture(Qt::PanGesture);

    // TODO: touch?
    QScroller::grabGesture(view, QScroller::LeftMouseButtonGesture);

    auto scroller = QScroller::scroller(view);
    QScrollerProperties props;
    props.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                          QScrollerProperties::OvershootAlwaysOff);
    props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
                          QScrollerProperties::OvershootAlwaysOff);
    scroller->setScrollerProperties(props);

    if (m_isTreeMode != newTreeMode) {
        m_isTreeMode = newTreeMode;

        RefreshGameDirectory();
    }
}

GameList::~GameList() {
    UnloadController();
}

void GameList::SetFilterFocus() {
    if (tree_view->model()->rowCount() > 0) {
        search_field->setFocus();
    }
}

void GameList::SetFilterVisible(bool visibility) {
    search_field->setVisible(visibility);
}

void GameList::ClearFilter() {
    search_field->clear();
}

void GameList::WorkerEvent() {
    current_worker->ProcessEvents(this);
}

void GameList::AddDirEntry(GameListDir* entry_items) {
    if (m_isTreeMode) {
        QStandardItem* root = item_model->invisibleRootItem();
        GameListDir* existing_dir = nullptr;
        const QString dir_path = QString::fromStdString(UISettings::values.game_dirs[entry_items->data(GameListDir::GameDirRole).toInt()].path);
        
        for (int i = 0; i < root->rowCount(); ++i) {
            QStandardItem* child = root->child(i, 0);
            if (!child) continue;
            auto type = child->data(GameListItem::TypeRole).value<GameListItemType>();
            if (type == GameListItemType::CustomDir || type == GameListItemType::SdmcDir ||
                type == GameListItemType::UserNandDir || type == GameListItemType::SysNandDir) {
                int dir_idx = child->data(GameListDir::GameDirRole).toInt();
                if (dir_idx >= 0 && dir_idx < UISettings::values.game_dirs.size()) {
                    if (QString::fromStdString(UISettings::values.game_dirs[dir_idx].path) == dir_path) {
                        existing_dir = static_cast<GameListDir*>(child);
                        break;
                    }
                }
            }
        }

        if (existing_dir) {
            m_dir_map[entry_items] = existing_dir;
        } else {
            root->appendRow(entry_items);
            tree_view->setFirstColumnSpanned(entry_items->row(), tree_view->rootIndex(), true);
            tree_view->setExpanded(
                entry_items->index(),
                UISettings::values.game_dirs[entry_items->data(GameListDir::GameDirRole).toInt()]
                    .expanded);
            m_dir_map[entry_items] = entry_items;
        }
    }
}

void GameList::AddEntry(const QList<QStandardItem*>& entry_items, GameListDir* parent) {
    GameListDir* real_parent = m_isTreeMode ? m_dir_map.value(parent, parent) : nullptr;
    QStandardItem* root = m_isTreeMode ? real_parent : item_model->invisibleRootItem();
    
    if (!root) {
        root = item_model->invisibleRootItem();
    }

    QStandardItem* existing_first_item = nullptr;
    int existing_row_index = -1;
    QString new_path = entry_items[0]->data(GameListItemPath::FullPathRole).toString();
    m_scanned_paths.insert(new_path);

    for (int i = 0; i < root->rowCount(); ++i) {
        QStandardItem* child = root->child(i, 0);
        if (child && child->data(GameListItemPath::FullPathRole).toString() == new_path) {
            existing_first_item = child;
            existing_row_index = i;
            break;
        }
    }

    if (existing_first_item) {
        for (int col = 0; col < entry_items.size(); ++col) {
            root->setChild(existing_row_index, col, entry_items[col]);
        }
    } else {
        if (!m_isTreeMode)
            item_model->invisibleRootItem()->appendRow(entry_items);
        else
            real_parent->appendRow(entry_items);
    }
}

void GameList::ValidateEntry(const QModelIndex& item) {
    const auto selected = item.sibling(item.row(), 0);

    switch (selected.data(GameListItem::TypeRole).value<GameListItemType>()) {
    case GameListItemType::Game: {
        const QString file_path = selected.data(GameListItemPath::FullPathRole).toString();
        if (file_path.isEmpty())
            return;
        const QFileInfo file_info(file_path);
        if (!file_info.exists())
            return;

        if (file_info.isDir()) {
            const QDir dir{file_path};
            const QStringList matching_main = dir.entryList({QStringLiteral("main")}, QDir::Files);
            if (matching_main.size() == 1) {
                emit GameChosen(dir.path() + QDir::separator() + matching_main[0]);
            }
            return;
        }

        const auto title_id = selected.data(GameListItemPath::ProgramIdRole).toULongLong();

        // Users usually want to run a different game after closing one
        search_field->clear();
        emit GameChosen(file_path, title_id);
        break;
    }
    case GameListItemType::AddDir:
        emit AddDirectory();
        break;
    default:
        break;
    }
}

bool GameList::IsEmpty() const {
    for (int i = 0; i < item_model->rowCount(); i++) {
        const QStandardItem* child = item_model->invisibleRootItem()->child(i);
        const auto type = static_cast<GameListItemType>(child->type());

        if (!child->hasChildren() &&
            (type == GameListItemType::SdmcDir || type == GameListItemType::UserNandDir ||
             type == GameListItemType::SysNandDir)) {
            item_model->invisibleRootItem()->removeRow(child->row());
            i--;
        }
    }

    return !item_model->invisibleRootItem()->hasChildren();
}

void GameList::DonePopulating(const QStringList& watch_list) {
    // Remove any games that were not found in the scan
    QStandardItem* root_item = item_model->invisibleRootItem();
    for (int i = 0; i < root_item->rowCount(); ++i) {
        QStandardItem* child = root_item->child(i, 0);
        if (!child) continue;
        auto type = child->data(GameListItem::TypeRole).value<GameListItemType>();
        if (type == GameListItemType::Game) {
            QString path = child->data(GameListItemPath::FullPathRole).toString();
            if (!m_scanned_paths.contains(path)) {
                root_item->removeRow(i);
                --i;
            }
        } else if (type == GameListItemType::CustomDir || type == GameListItemType::SdmcDir ||
                   type == GameListItemType::UserNandDir || type == GameListItemType::SysNandDir) {
            for (int j = 0; j < child->rowCount(); ++j) {
                QStandardItem* grand_child = child->child(j, 0);
                if (grand_child && grand_child->data(GameListItem::TypeRole).value<GameListItemType>() == GameListItemType::Game) {
                    QString path = grand_child->data(GameListItemPath::FullPathRole).toString();
                    if (!m_scanned_paths.contains(path)) {
                        child->removeRow(j);
                        --j;
                    }
                }
            }
        }
    }

    // Free up unused directories created by the worker
    for (auto* key : m_dir_map.keys()) {
        auto* val = m_dir_map.value(key);
        if (key != val) {
            delete key;
        }
    }
    m_dir_map.clear();
    m_scanned_paths.clear();

    // Save current games list to the offline cache
    SaveCache();

    emit ShowList(!IsEmpty());

    // Add favorites row
    if (m_isTreeMode) {
        item_model->invisibleRootItem()->appendRow(new GameListAddDir());

        item_model->invisibleRootItem()->insertRow(0, new GameListFavorites());
        tree_view->setRowHidden(0, item_model->invisibleRootItem()->index(),
                                UISettings::values.favorited_ids.size() == 0);
        tree_view->setExpanded(item_model->invisibleRootItem()->child(0)->index(),
                               UISettings::values.favorites_expanded.GetValue());
        for (const auto id : std::as_const(UISettings::values.favorited_ids)) {
            AddFavorite(id);
        }
        
        tree_view->setFirstColumnSpanned(0, item_model->invisibleRootItem()->index(), true);
        tree_view->setFirstColumnSpanned(item_model->rowCount() - 1, item_model->invisibleRootItem()->index(), true);
    }

    // Clear out the old directories to watch for changes and add the new ones
    auto watch_dirs = watcher->directories();
    if (!watch_dirs.isEmpty()) {
        watcher->removePaths(watch_dirs);
    }
    // Workaround: Add the watch paths in chunks to allow the gui to refresh
    // This prevents the UI from stalling when a large number of watch paths are added
    // Also artificially caps the watcher to a certain number of directories
    constexpr int LIMIT_WATCH_DIRECTORIES = 5000;
    constexpr int SLICE_SIZE = 25;
    int len = (std::min)(static_cast<int>(watch_list.size()), LIMIT_WATCH_DIRECTORIES);

    // Block signals to prevent the watcher from triggering a refresh while we are adding paths.
    // This fixes a refresh loop on macOS.
#ifdef __APPLE__
    const bool old_signals_blocked = watcher->blockSignals(true);
#endif

    for (int i = 0; i < len; i += SLICE_SIZE) {
        auto chunk = watch_list.mid(i, SLICE_SIZE);
        if (!chunk.isEmpty()) {
            watcher->addPaths(chunk);
        }
        QCoreApplication::processEvents();
    }

#ifdef __APPLE__
    watcher->blockSignals(old_signals_blocked);
#endif
    m_currentView->setEnabled(true);

    int children_total = 0;
    for (int i = 1; i < item_model->rowCount() - 1; ++i) {
        children_total += item_model->item(i, 0)->rowCount();
        if (m_isTreeMode) {
            tree_view->setFirstColumnSpanned(i, item_model->invisibleRootItem()->index(), true);
        }
    }
    search_field->setFilterResult(children_total, children_total);
    if (children_total > 0) {
        search_field->setFocus();
    }
    item_model->sort(tree_view->header()->sortIndicatorSection(),
                     tree_view->header()->sortIndicatorOrder());

    emit PopulatingCompleted();
}

void GameList::PopupContextMenu(const QPoint& menu_location) {
    QModelIndex item = m_currentView->indexAt(menu_location);
    if (!item.isValid()) {
        if (m_isTreeMode)
            return;

        QMenu blank_menu;
        QAction* addGameDirAction = blank_menu.addAction(tr("&Add New Game Directory"));

        connect(addGameDirAction, &QAction::triggered, this, &GameList::AddDirectory);
        blank_menu.exec(m_currentView->viewport()->mapToGlobal(menu_location));
        return;
    }

    const auto selected = item.sibling(item.row(), 0);
    QMenu context_menu;
    switch (selected.data(GameListItem::TypeRole).value<GameListItemType>()) {
    case GameListItemType::Game:
        AddGamePopup(context_menu, selected.data(GameListItemPath::ProgramIdRole).toULongLong(),
                     selected.data(GameListItemPath::FullPathRole).toString().toStdString());
        break;
    case GameListItemType::CustomDir:
        AddPermDirPopup(context_menu, selected);
        AddCustomDirPopup(context_menu, selected);
        break;
    case GameListItemType::SdmcDir:
    case GameListItemType::UserNandDir:
    case GameListItemType::SysNandDir:
        AddPermDirPopup(context_menu, selected);
        break;
    case GameListItemType::Favorites:
        AddFavoritesPopup(context_menu);
        break;
    default:
        break;
    }
    context_menu.exec(m_currentView->viewport()->mapToGlobal(menu_location));
}

void GameList::AddGamePopup(QMenu& context_menu, u64 program_id, const std::string& path) {
    // TODO(crueter): Refactor this and make it less bad
    QAction* favorite = context_menu.addAction(tr("Favorite"));
    context_menu.addSeparator();
    QAction* start_game = context_menu.addAction(tr("Start Game"));
    QAction* start_game_global =
        context_menu.addAction(tr("Start Game without Custom Configuration"));
    context_menu.addSeparator();
    QAction* open_save_location = context_menu.addAction(tr("Open Save Data Location"));
    QAction* open_mod_location = context_menu.addAction(tr("Open Mod Data Location"));
    QAction* open_transferable_shader_cache =
        context_menu.addAction(tr("Open Transferable Pipeline Cache"));
    QAction* ryujinx = context_menu.addAction(tr("Link to Ryujinx"));
    context_menu.addSeparator();
    QMenu* remove_menu = context_menu.addMenu(tr("Remove"));
    QAction* remove_update = remove_menu->addAction(tr("Remove Installed Update"));
    QAction* remove_dlc = remove_menu->addAction(tr("Remove All Installed DLC"));
    QAction* remove_custom_config = remove_menu->addAction(tr("Remove Custom Configuration"));
    QAction* remove_cache_storage = remove_menu->addAction(tr("Remove Cache Storage"));
    QAction* remove_gl_shader_cache = remove_menu->addAction(tr("Remove OpenGL Pipeline Cache"));
    QAction* remove_vk_shader_cache = remove_menu->addAction(tr("Remove Vulkan Pipeline Cache"));
    remove_menu->addSeparator();
    QAction* remove_shader_cache = remove_menu->addAction(tr("Remove All Pipeline Caches"));
    QAction* remove_all_content = remove_menu->addAction(tr("Remove All Installed Contents"));
    QMenu* play_time_menu = context_menu.addMenu(tr("Manage Play Time"));
    QAction* set_play_time = play_time_menu->addAction(tr("Edit Play Time Data"));
    QAction* remove_play_time_data = play_time_menu->addAction(tr("Remove Play Time Data"));
    QMenu* dump_romfs_menu = context_menu.addMenu(tr("Dump RomFS"));
    QAction* dump_romfs = dump_romfs_menu->addAction(tr("Dump RomFS"));
    QAction* dump_romfs_sdmc = dump_romfs_menu->addAction(tr("Dump RomFS to SDMC"));
    QAction* verify_integrity = context_menu.addAction(tr("Verify Integrity"));
    QAction* copy_tid = context_menu.addAction(tr("Copy Title ID to Clipboard"));
    QAction* navigate_to_gamedb_entry = context_menu.addAction(tr("Navigate to GameDB entry"));
// TODO: Implement shortcut creation for macOS
#if !defined(__APPLE__)
    QMenu* shortcut_menu = context_menu.addMenu(tr("Create Shortcut"));
    QAction* create_desktop_shortcut = shortcut_menu->addAction(tr("Add to Desktop"));
    QAction* create_applications_menu_shortcut =
        shortcut_menu->addAction(tr("Add to Applications Menu"));
#endif
    context_menu.addSeparator();
    QAction* properties = context_menu.addAction(tr("Configure Game"));

    favorite->setVisible(program_id != 0);
    favorite->setCheckable(true);
    favorite->setChecked(UISettings::values.favorited_ids.contains(program_id));
    open_save_location->setVisible(program_id != 0);
    open_mod_location->setVisible(program_id != 0);
    open_transferable_shader_cache->setVisible(program_id != 0);
    remove_update->setVisible(program_id != 0);
    remove_dlc->setVisible(program_id != 0);
    remove_gl_shader_cache->setVisible(program_id != 0);
    remove_vk_shader_cache->setVisible(program_id != 0);
    remove_shader_cache->setVisible(program_id != 0);
    remove_all_content->setVisible(program_id != 0);
    auto it = FindMatchingCompatibilityEntry(compatibility_list, program_id);
    navigate_to_gamedb_entry->setVisible(it != compatibility_list.end() && program_id != 0);

    connect(favorite, &QAction::triggered, this,
            [this, program_id]() { ToggleFavorite(program_id); });
    connect(open_save_location, &QAction::triggered, this, [this, program_id, path]() {
        emit OpenFolderRequested(program_id, GameListOpenTarget::SaveData, path);
    });
    connect(start_game, &QAction::triggered, this,
            [this, path]() { emit BootGame(QString::fromStdString(path), StartGameType::Normal); });
    connect(start_game_global, &QAction::triggered, this,
            [this, path]() { emit BootGame(QString::fromStdString(path), StartGameType::Global); });
    connect(open_mod_location, &QAction::triggered, this, [this, program_id, path]() {
        emit OpenFolderRequested(program_id, GameListOpenTarget::ModData, path);
    });
    connect(open_transferable_shader_cache, &QAction::triggered, this,
            [this, program_id]() { emit OpenTransferableShaderCacheRequested(program_id); });
    connect(remove_all_content, &QAction::triggered, this, [this, program_id]() {
        emit RemoveInstalledEntryRequested(program_id, QtCommon::Game::InstalledEntryType::Game);
    });
    connect(remove_update, &QAction::triggered, this, [this, program_id]() {
        emit RemoveInstalledEntryRequested(program_id, QtCommon::Game::InstalledEntryType::Update);
    });
    connect(remove_dlc, &QAction::triggered, this, [this, program_id]() {
        emit RemoveInstalledEntryRequested(program_id,
                                           QtCommon::Game::InstalledEntryType::AddOnContent);
    });
    connect(remove_gl_shader_cache, &QAction::triggered, this, [this, program_id, path]() {
        emit RemoveFileRequested(program_id, QtCommon::Game::GameListRemoveTarget::GlShaderCache,
                                 path);
    });
    connect(remove_vk_shader_cache, &QAction::triggered, this, [this, program_id, path]() {
        emit RemoveFileRequested(program_id, QtCommon::Game::GameListRemoveTarget::VkShaderCache,
                                 path);
    });
    connect(remove_shader_cache, &QAction::triggered, this, [this, program_id, path]() {
        emit RemoveFileRequested(program_id, QtCommon::Game::GameListRemoveTarget::AllShaderCache,
                                 path);
    });
    connect(remove_custom_config, &QAction::triggered, this, [this, program_id, path]() {
        emit RemoveFileRequested(program_id,
                                 QtCommon::Game::GameListRemoveTarget::CustomConfiguration, path);
    });
    connect(set_play_time, &QAction::triggered, this,
            [this, program_id]() { emit SetPlayTimeRequested(program_id); });
    connect(remove_play_time_data, &QAction::triggered, this,
            [this, program_id]() { emit RemovePlayTimeRequested(program_id); });
    connect(remove_cache_storage, &QAction::triggered, this, [this, program_id, path] {
        emit RemoveFileRequested(program_id, QtCommon::Game::GameListRemoveTarget::CacheStorage,
                                 path);
    });
    connect(dump_romfs, &QAction::triggered, this, [this, program_id, path]() {
        emit DumpRomFSRequested(program_id, path, DumpRomFSTarget::Normal);
    });
    connect(dump_romfs_sdmc, &QAction::triggered, this, [this, program_id, path]() {
        emit DumpRomFSRequested(program_id, path, DumpRomFSTarget::SDMC);
    });
    connect(verify_integrity, &QAction::triggered, this,
            [this, path]() { emit VerifyIntegrityRequested(path); });
    connect(copy_tid, &QAction::triggered, this,
            [this, program_id]() { emit CopyTIDRequested(program_id); });
    connect(navigate_to_gamedb_entry, &QAction::triggered, this, [this, program_id]() {
        emit NavigateToGamedbEntryRequested(program_id, compatibility_list);
    });
// TODO: Implement shortcut creation for macOS
#if !defined(__APPLE__)
    connect(create_desktop_shortcut, &QAction::triggered, this, [this, program_id, path]() {
        emit CreateShortcut(program_id, path, QtCommon::Game::ShortcutTarget::Desktop);
    });
    connect(create_applications_menu_shortcut, &QAction::triggered, this,
            [this, program_id, path]() {
                emit CreateShortcut(program_id, path, QtCommon::Game::ShortcutTarget::Applications);
            });
#endif
    connect(properties, &QAction::triggered, this,
            [this, program_id, path]() { emit OpenPerGameGeneralRequested(program_id, path); });

    connect(ryujinx, &QAction::triggered, this,
            [this, program_id]() { emit LinkToRyujinxRequested(program_id); });
};

void GameList::AddCustomDirPopup(QMenu& context_menu, QModelIndex selected) {
    UISettings::GameDir& game_dir =
        UISettings::values.game_dirs[selected.data(GameListDir::GameDirRole).toInt()];

    QAction* deep_scan = context_menu.addAction(tr("Scan Subfolders"));
    QAction* delete_dir = context_menu.addAction(tr("Remove Game Directory"));

    deep_scan->setCheckable(true);
    deep_scan->setChecked(game_dir.deep_scan);

    connect(deep_scan, &QAction::triggered, this, [this, &game_dir] {
        game_dir.deep_scan = !game_dir.deep_scan;
        PopulateAsync(UISettings::values.game_dirs);
    });
    connect(delete_dir, &QAction::triggered, this, [this, &game_dir, selected] {
        UISettings::values.game_dirs.removeOne(game_dir);
        item_model->invisibleRootItem()->removeRow(selected.row());
        OnTextChanged(search_field->filterText());
    });
}

void GameList::AddPermDirPopup(QMenu& context_menu, QModelIndex selected) {
    const int game_dir_index = selected.data(GameListDir::GameDirRole).toInt();

    QAction* move_up = context_menu.addAction(tr("\u25B2 Move Up"));
    QAction* move_down = context_menu.addAction(tr("\u25bc Move Down"));
    QAction* open_directory_location = context_menu.addAction(tr("Open Directory Location"));

    const int row = selected.row();

    move_up->setEnabled(row > 1);
    move_down->setEnabled(row < item_model->rowCount() - 2);

    connect(move_up, &QAction::triggered, this, [this, selected, row, game_dir_index] {
        const int other_index = selected.sibling(row - 1, 0).data(GameListDir::GameDirRole).toInt();
        // swap the items in the settings
        std::swap(UISettings::values.game_dirs[game_dir_index],
                  UISettings::values.game_dirs[other_index]);
        // swap the indexes held by the QVariants
        item_model->setData(selected, QVariant(other_index), GameListDir::GameDirRole);
        item_model->setData(selected.sibling(row - 1, 0), QVariant(game_dir_index),
                            GameListDir::GameDirRole);
        // move the treeview items
        QList<QStandardItem*> item = item_model->takeRow(row);
        item_model->invisibleRootItem()->insertRow(row - 1, item);
        tree_view->setExpanded(selected.sibling(row - 1, 0),
                               UISettings::values.game_dirs[other_index].expanded);
    });

    connect(move_down, &QAction::triggered, this, [this, selected, row, game_dir_index] {
        const int other_index = selected.sibling(row + 1, 0).data(GameListDir::GameDirRole).toInt();
        // swap the items in the settings
        std::swap(UISettings::values.game_dirs[game_dir_index],
                  UISettings::values.game_dirs[other_index]);
        // swap the indexes held by the QVariants
        item_model->setData(selected, QVariant(other_index), GameListDir::GameDirRole);
        item_model->setData(selected.sibling(row + 1, 0), QVariant(game_dir_index),
                            GameListDir::GameDirRole);
        // move the treeview items
        const QList<QStandardItem*> item = item_model->takeRow(row);
        item_model->invisibleRootItem()->insertRow(row + 1, item);
        tree_view->setExpanded(selected.sibling(row + 1, 0),
                               UISettings::values.game_dirs[other_index].expanded);
    });

    connect(open_directory_location, &QAction::triggered, this, [this, game_dir_index] {
        emit OpenDirectory(
            QString::fromStdString(UISettings::values.game_dirs[game_dir_index].path));
    });
}

void GameList::AddFavoritesPopup(QMenu& context_menu) {
    QAction* clear = context_menu.addAction(tr("Clear"));

    connect(clear, &QAction::triggered, this, [this] {
        for (const auto id : std::as_const(UISettings::values.favorited_ids)) {
            RemoveFavorite(id);
        }
        UISettings::values.favorited_ids.clear();
        tree_view->setRowHidden(0, item_model->invisibleRootItem()->index(), true);
    });
}

void GameList::LoadCompatibilityList() {
    QFile compat_list{QStringLiteral(":compatibility_list/compatibility_list.json")};

    if (!compat_list.open(QFile::ReadOnly | QFile::Text)) {
        LOG_ERROR(Frontend, "Unable to open game compatibility list");
        return;
    }

    if (compat_list.size() == 0) {
        LOG_WARNING(Frontend, "Game compatibility list is empty");
        return;
    }

    const QByteArray content = compat_list.readAll();
    if (content.isEmpty()) {
        LOG_ERROR(Frontend, "Unable to completely read game compatibility list");
        return;
    }

    const QJsonDocument json = QJsonDocument::fromJson(content);
    const QJsonArray arr = json.array();

    for (const QJsonValue& value : arr) {
        const QJsonObject game = value.toObject();
        const QString compatibility_key = QStringLiteral("compatibility");

        if (!game.contains(compatibility_key) || !game[compatibility_key].isDouble()) {
            continue;
        }

        const int compatibility = game[compatibility_key].toInt();
        const QString directory = game[QStringLiteral("directory")].toString();
        const QJsonArray ids = game[QStringLiteral("releases")].toArray();

        for (const QJsonValue& id_ref : ids) {
            const QJsonObject id_object = id_ref.toObject();
            const QString id = id_object[QStringLiteral("id")].toString();

            compatibility_list.emplace(id.toUpper().toStdString(),
                                       std::make_pair(QString::number(compatibility), directory));
        }
    }
}

void GameList::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void GameList::RetranslateUI() {
    item_model->setHeaderData(COLUMN_NAME, Qt::Horizontal, tr("Name").toUpper());
    item_model->setHeaderData(COLUMN_COMPATIBILITY, Qt::Horizontal, tr("Compatibility").toUpper());
    item_model->setHeaderData(COLUMN_ADD_ONS, Qt::Horizontal, tr("Add-ons").toUpper());
    item_model->setHeaderData(COLUMN_FILE_TYPE, Qt::Horizontal, tr("File type").toUpper());
    item_model->setHeaderData(COLUMN_SIZE, Qt::Horizontal, tr("Size").toUpper());
    item_model->setHeaderData(COLUMN_PLAY_TIME, Qt::Horizontal, tr("Play time").toUpper());
    
    item_model->setHeaderData(COLUMN_NAME, Qt::Horizontal, Qt::AlignCenter, Qt::TextAlignmentRole);
    item_model->setHeaderData(COLUMN_COMPATIBILITY, Qt::Horizontal, Qt::AlignCenter, Qt::TextAlignmentRole);
    item_model->setHeaderData(COLUMN_ADD_ONS, Qt::Horizontal, Qt::AlignCenter, Qt::TextAlignmentRole);
    item_model->setHeaderData(COLUMN_FILE_TYPE, Qt::Horizontal, Qt::AlignCenter, Qt::TextAlignmentRole);
    item_model->setHeaderData(COLUMN_SIZE, Qt::Horizontal, Qt::AlignCenter, Qt::TextAlignmentRole);
    item_model->setHeaderData(COLUMN_PLAY_TIME, Qt::Horizontal, Qt::AlignCenter, Qt::TextAlignmentRole);
}

void GameListSearchField::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void GameListSearchField::RetranslateUI() {
    label_filter->setText(tr("Filter:"));
    edit_filter->setPlaceholderText(tr("Enter pattern to filter"));
}

QStandardItemModel* GameList::GetModel() const {
    return item_model;
}

void GameList::UpdateIconSize() {
    // Update sizes and stuff for the list view
    const u32 icon_size = UISettings::values.game_icon_size.GetValue();

    int heightMargin = 0;
    int widthMargin = 80;

    if (UISettings::values.show_game_name) {
        // the scaling on the card is kinda abysmal.
        // TODO(crueter): refactor
        switch (icon_size) {
        case 128:
            heightMargin = 65;
            break;
        case 0:
            widthMargin = 120;
            heightMargin = 120;
            break;
        case 64:
            heightMargin = 77;
            break;
        case 32:
        case 256:
            heightMargin = 81;
            break;
        }
    } else {
        widthMargin = 24;
        heightMargin = 24;
    }

    // "auto" resize //
    const int view_width = list_view->viewport()->width();

    // Tiny space padding to prevent the list view from forcing its own resize operation.
    const double spacing = 0.01;
    const int min_item_width = icon_size + widthMargin;

    // And now stretch it a bit to fill out remaining space.
    // Not perfect but works well enough for now
    int columns = std::max(1, (view_width - 16) / min_item_width);
    int stretched_width = ((view_width) - (spacing * (columns - 1))) / columns;

    // only updates things if grid size is changed
    QSize grid_size(stretched_width, icon_size + heightMargin);
    if (list_view->gridSize() != grid_size) {
        list_view->setUpdatesEnabled(false);

        list_view->setGridSize(grid_size);
        m_gameCard->setSize(grid_size, stretched_width - min_item_width, columns);

        list_view->setUpdatesEnabled(true);
    }
}

void GameList::PopulateAsync(QVector<UISettings::GameDir>& game_dirs) {
    m_currentView->setEnabled(false);

    // Update the columns in case UISettings has changed
    tree_view->setColumnHidden(COLUMN_ADD_ONS, !UISettings::values.show_add_ons);
    tree_view->setColumnHidden(COLUMN_COMPATIBILITY, !UISettings::values.show_compat);
    tree_view->setColumnHidden(COLUMN_FILE_TYPE, !UISettings::values.show_types);
    tree_view->setColumnHidden(COLUMN_SIZE, !UISettings::values.show_size);
    tree_view->setColumnHidden(COLUMN_PLAY_TIME, !UISettings::values.show_play_time);

    if (!m_isTreeMode)
        UpdateIconSize();

    // Cancel any existing worker.
    current_worker.reset();

    // Remove Favorites and AddDir rows before scanning so we don't duplicate them
    QStandardItem* root = item_model->invisibleRootItem();
    for (int i = 0; i < root->rowCount(); ++i) {
        QStandardItem* child = root->child(i, 0);
        if (child) {
            auto type = child->data(GameListItem::TypeRole).value<GameListItemType>();
            if (type == GameListItemType::Favorites || type == GameListItemType::AddDir) {
                root->removeRow(i);
                --i;
            }
        }
    }

    m_scanned_paths.clear();
    m_dir_map.clear();

    search_field->clear();

    current_worker = std::make_unique<GameListWorker>(vfs, provider, game_dirs, compatibility_list,
                                                      play_time_manager, system);

    // Get events from the worker as data becomes available
    connect(current_worker.get(), &GameListWorker::DataAvailable, this, &GameList::WorkerEvent,
            Qt::QueuedConnection);

    QThreadPool::globalInstance()->start(current_worker.get());
}

void GameList::SaveInterfaceLayout() {
    UISettings::values.gamelist_header_state = tree_view->header()->saveState();
}

void GameList::LoadInterfaceLayout() {
    auto* header = tree_view->header();

    if (header->restoreState(UISettings::values.gamelist_header_state))
        return;

    // We are using the name column to display icons and titles
    // so make it as large as possible as default.

    // TODO(crueter): width() is not initialized yet, so use a sane default value
    header->resizeSection(COLUMN_NAME, 840);
}

const QStringList GameList::supported_file_extensions = {
    QStringLiteral("nso"), QStringLiteral("nro"), QStringLiteral("nca"),
    QStringLiteral("xci"), QStringLiteral("nsp"), QStringLiteral("kip"),
    QStringLiteral("ncz"), QStringLiteral("nsz"), QStringLiteral("xcz")};

void GameList::RefreshGameDirectory() {
    if (system.IsPoweredOn()) {
        LOG_INFO(Frontend, "Game list refresh ignored because emulator is running.");
        return;
    }

    // Reset the externals watcher whenever the game list is reloaded,
    // primarily ensures that new titles and external dirs are caught.
    ResetExternalWatcher();

    if (!UISettings::values.game_dirs.empty() && current_worker != nullptr) {
        LOG_INFO(Frontend, "Change detected in the games directory. Reloading game list.");
        QtCommon::system->GetFileSystemController().CreateFactories(*QtCommon::vfs);
        PopulateAsync(UISettings::values.game_dirs);
    }
}

void GameList::RefreshExternalContent() {
    if (system.IsPoweredOn()) {
        LOG_INFO(Frontend, "External content refresh ignored because emulator is running.");
        return;
    }

    // TODO: Explore the possibility of only resetting the metadata cache for that specific game.
    if (!UISettings::values.game_dirs.empty() && current_worker != nullptr) {
        LOG_INFO(Frontend, "External content directory changed. Clearing metadata cache.");
        QtCommon::Game::ResetMetadata(false);
        QtCommon::system->GetFileSystemController().CreateFactories(*QtCommon::vfs);
        PopulateAsync(UISettings::values.game_dirs);
    }
}

void GameList::ResetExternalWatcher() {
    {
    }
    auto watch_dirs = external_watcher->directories();
    if (!watch_dirs.isEmpty()) {
        external_watcher->removePaths(watch_dirs);
    }
    {
    }
    for (const std::string& dir : Settings::values.external_content_dirs) {
        external_watcher->addPath(QString::fromStdString(dir));
    }
    {
    }
}

void GameList::ToggleFavorite(u64 program_id) {
    if (!UISettings::values.favorited_ids.contains(program_id)) {
        tree_view->setRowHidden(0, item_model->invisibleRootItem()->index(),
                                !search_field->filterText().isEmpty());
        UISettings::values.favorited_ids.append(program_id);
        AddFavorite(program_id);
        item_model->sort(tree_view->header()->sortIndicatorSection(),
                         tree_view->header()->sortIndicatorOrder());
    } else {
        UISettings::values.favorited_ids.removeOne(program_id);
        RemoveFavorite(program_id);
        if (UISettings::values.favorited_ids.size() == 0) {
            tree_view->setRowHidden(0, item_model->invisibleRootItem()->index(), true);
        }
    }
    emit SaveConfig();
}

void GameList::AddFavorite(u64 program_id) {
    auto* favorites_row = item_model->item(0);

    for (int i = 1; i < item_model->rowCount() - 1; i++) {
        const auto* folder = item_model->item(i);
        for (int j = 0; j < folder->rowCount(); j++) {
            if (folder->child(j)->data(GameListItemPath::ProgramIdRole).toULongLong() ==
                program_id) {
                QList<QStandardItem*> list;
                for (int k = 0; k < COLUMN_COUNT; k++) {
                    list.append(folder->child(j, k)->clone());
                }
                list[0]->setData(folder->child(j)->data(GameListItem::SortRole),
                                 GameListItem::SortRole);
                list[0]->setText(folder->child(j)->data(Qt::DisplayRole).toString());

                favorites_row->appendRow(list);
                return;
            }
        }
    }
}

void GameList::RemoveFavorite(u64 program_id) {
    auto* favorites_row = item_model->item(0);

    for (int i = 0; i < favorites_row->rowCount(); i++) {
        const auto* game = favorites_row->child(i);
        if (game->data(GameListItemPath::ProgramIdRole).toULongLong() == program_id) {
            favorites_row->removeRow(i);
            return;
        }
    }
}

GameListPlaceholder::GameListPlaceholder(MainWindow* parent) : QWidget{parent} {
    connect(parent, &MainWindow::UpdateThemedIcons, this,
            &GameListPlaceholder::onUpdateThemedIcons);

    layout = new QVBoxLayout;
    image = new QLabel;
    text = new QLabel;
    layout->setAlignment(Qt::AlignCenter);
    image->setPixmap(QIcon::fromTheme(QStringLiteral("plus_folder")).pixmap(200));

    RetranslateUI();
    QFont font = text->font();
    font.setPointSize(20);
    text->setFont(font);
    text->setAlignment(Qt::AlignHCenter);
    image->setAlignment(Qt::AlignHCenter);

    layout->addWidget(image);
    layout->addWidget(text);
    setLayout(layout);
}

GameListPlaceholder::~GameListPlaceholder() = default;

void GameListPlaceholder::onUpdateThemedIcons() {
    image->setPixmap(QIcon::fromTheme(QStringLiteral("plus_folder")).pixmap(200));
}

void GameListPlaceholder::mouseDoubleClickEvent(QMouseEvent* event) {
    emit GameListPlaceholder::AddDirectory();
}

void GameList::SetupScrollAnimation() {
    auto setup = [this](QVariantAnimation* anim, QScrollBar* bar) {
        // animation handles moving the bar instead of Qt's built in crap
        anim->setEasingCurve(QEasingCurve::OutCubic);
        anim->setDuration(200);
        connect(anim, &QVariantAnimation::valueChanged, this,
                [bar](const QVariant& value) { bar->setValue(value.toInt()); });
    };

    vertical_scroll = new QVariantAnimation(this);
    horizontal_scroll = new QVariantAnimation(this);

    setup(vertical_scroll, tree_view->verticalScrollBar());
    setup(horizontal_scroll, tree_view->horizontalScrollBar());

    setup(vertical_scroll, list_view->verticalScrollBar());
    setup(horizontal_scroll, list_view->horizontalScrollBar());
}

bool GameList::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_currentView->viewport() && event->type() == QEvent::Wheel) {
        QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);

        bool horizontal = wheelEvent->modifiers() & Qt::ShiftModifier;

        int deltaX = wheelEvent->angleDelta().x();
        int deltaY = wheelEvent->angleDelta().y();

        // if shift is held do a horizontal scroll
        if (horizontal && deltaY != 0 && deltaX == 0) {
            deltaX = deltaY;
            deltaY = 0;
        }

        // TODO(crueter): dedup this
        if (deltaY != 0) {
            if (vertical_scroll->state() == QAbstractAnimation::Stopped)
                vertical_scroll_target = m_currentView->verticalScrollBar()->value();

            vertical_scroll_target -= deltaY;
            vertical_scroll_target =
                qBound(0, vertical_scroll_target, m_currentView->verticalScrollBar()->maximum());

            vertical_scroll->stop();
            vertical_scroll->setStartValue(m_currentView->verticalScrollBar()->value());
            vertical_scroll->setEndValue(vertical_scroll_target);
            vertical_scroll->start();
        }

        if (deltaX != 0) {
            if (horizontal_scroll->state() == QAbstractAnimation::Stopped)
                horizontal_scroll_target = m_currentView->horizontalScrollBar()->value();

            horizontal_scroll_target -= deltaX;
            horizontal_scroll_target = qBound(0, horizontal_scroll_target,
                                              m_currentView->horizontalScrollBar()->maximum());

            horizontal_scroll->stop();
            horizontal_scroll->setStartValue(m_currentView->horizontalScrollBar()->value());
            horizontal_scroll->setEndValue(horizontal_scroll_target);
            horizontal_scroll->start();
        }

        return true;
    }

    if (obj == m_currentView->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);

        // if the user clicks outside of the list, deselect the current item.
        QModelIndex index = m_currentView->indexAt(mouseEvent->pos());
        if (!index.isValid()) {
            m_currentView->selectionModel()->clearSelection();
            m_currentView->setCurrentIndex(QModelIndex());
        }
    }

    if (obj == list_view->viewport() && event->type() == QEvent::Resize) {
        UpdateIconSize();
        return true;
    }

    return QWidget::eventFilter(obj, event);
}

void GameListPlaceholder::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void GameListPlaceholder::RetranslateUI() {
    text->setText(tr("Double-click to add a new folder to the game list"));
}

void GameList::SaveCache() {
    const auto cache_path = Common::FS::PathToUTF8String(
        Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "game_list_cache.dat");
    QFile file(QString::fromStdString(cache_path));
    if (!file.open(QIODevice::WriteOnly)) return;

    QDataStream out(&file);
    out << qint32(1); // Version

    QList<QList<QVariant>> games_data;
    QStandardItem* root = item_model->invisibleRootItem();
    
    auto collect_game = [&](QStandardItem* item, const QString& dir_path) {
        if (item->data(GameListItem::TypeRole).value<GameListItemType>() == GameListItemType::Game) {
            const int row = item->row();
            QStandardItem* parent = item->parent();
            auto get_item = [&](int col) -> QStandardItem* {
                return parent ? parent->child(row, col) : item_model->item(row, col);
            };

            QStandardItem* item_path = get_item(0);
            QStandardItem* item_type = get_item(1);
            QStandardItem* item_size = get_item(2);
            QStandardItem* item_time = get_item(3);
            QStandardItem* item_addon = get_item(4);
            QStandardItem* item_compat = get_item(5);

            QList<QVariant> game_data;
            game_data << (item_path ? item_path->data(GameListItemPath::FullPathRole) : QVariant())
                      << (item_path ? item_path->data(GameListItemPath::TitleRole) : QVariant())
                      << (item_type ? item_type->text() : QString())
                      << (item_size ? item_size->data(GameListItemSize::SizeRole) : QVariant())
                      << (item_time ? item_time->data(GameListItemPlayTime::PlayTimeRole) : QVariant())
                      << (item_addon ? item_addon->text() : QString())
                      << (item_path ? item_path->data(GameListItemPath::ProgramIdRole) : QVariant())
                      << (item_compat ? item_compat->data(GameListItemCompat::CompatNumberRole) : QVariant())
                      << dir_path
                      << (item_path ? item_path->data(Qt::DecorationRole) : QVariant()); // Icon
            games_data.append(game_data);
        }
    };

    for (int i = 0; i < root->rowCount(); ++i) {
        QStandardItem* child = root->child(i, 0);
        if (!child) continue;
        auto type = child->data(GameListItem::TypeRole).value<GameListItemType>();
        if (type == GameListItemType::Game) {
            collect_game(child, QString());
        } else if (type == GameListItemType::CustomDir || type == GameListItemType::SdmcDir ||
                   type == GameListItemType::UserNandDir || type == GameListItemType::SysNandDir) {
            QString dir_path = QString::fromStdString(UISettings::values.game_dirs[child->data(GameListDir::GameDirRole).toInt()].path);
            for (int j = 0; j < child->rowCount(); ++j) {
                QStandardItem* grand_child = child->child(j, 0);
                if (grand_child) {
                    collect_game(grand_child, dir_path);
                }
            }
        }
    }

    out << games_data;
}

void GameList::LoadCache() {
    const auto cache_path = Common::FS::PathToUTF8String(
        Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "game_list_cache.dat");
    QFile file(QString::fromStdString(cache_path));
    if (!file.open(QIODevice::ReadOnly)) return;

    QDataStream in(&file);
    qint32 version;
    in >> version;
    if (version != 1) return;

    QList<QList<QVariant>> games_data;
    in >> games_data;

    QStandardItem* root = item_model->invisibleRootItem();
    QMap<QString, GameListDir*> dir_map;
    
    for (int i = 0; i < UISettings::values.game_dirs.size(); ++i) {
        auto& game_dir = UISettings::values.game_dirs[i];
        if (m_isTreeMode) {
            GameListItemType type = GameListItemType::CustomDir;
            if (game_dir.path == "SDMC") type = GameListItemType::SdmcDir;
            else if (game_dir.path == "UserNAND") type = GameListItemType::UserNandDir;
            else if (game_dir.path == "SysNAND") type = GameListItemType::SysNandDir;
            
            auto* const game_list_dir = new GameListDir(game_dir, type);
            root->appendRow(game_list_dir);
            tree_view->setFirstColumnSpanned(game_list_dir->row(), tree_view->rootIndex(), true);
            tree_view->setExpanded(game_list_dir->index(), game_dir.expanded);
            dir_map[QString::fromStdString(game_dir.path)] = game_list_dir;
        }
    }

    for (const auto& game_data : games_data) {
        if (game_data.size() < 10) continue;
        
        QString full_path = game_data[0].toString();
        QString name = game_data[1].toString();
        QString file_type = game_data[2].toString();
        u64 size = game_data[3].toULongLong();
        u64 play_time = game_data[4].toULongLong();
        QString addons = game_data[5].toString();
        u64 program_id = game_data[6].toULongLong();
        QString compat = game_data[7].toString();
        QString dir_path = game_data[8].toString();
        QPixmap picture = game_data[9].value<QPixmap>();

        auto list = QList<QStandardItem*>{
            new GameListItemPath(full_path, picture, name, file_type, program_id, play_time, addons, size),
            new GameListItem(file_type),
            new GameListItemSize(size),
            new GameListItemPlayTime(play_time),
            new GameListItem(addons),
            new GameListItemCompat(compat),
        };

        list[1]->setTextAlignment(Qt::AlignCenter);
        list[2]->setTextAlignment(Qt::AlignCenter);
        list[3]->setTextAlignment(Qt::AlignCenter);
        list[4]->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        list[5]->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        QFont tabular_font(QStringLiteral("Consolas"));
        tabular_font.setFixedPitch(true);
        tabular_font.setPointSize(9);
        list[2]->setFont(tabular_font);
        list[3]->setFont(tabular_font);

        if (m_isTreeMode && dir_map.contains(dir_path)) {
            dir_map[dir_path]->appendRow(list);
        } else {
            root->appendRow(list);
        }
    }
}


