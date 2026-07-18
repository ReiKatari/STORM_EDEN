// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QtConcurrent/QtConcurrent>
#include <mutex>

#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/common_funcs.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/vfs/vfs_buffered.h"
#include "core/loader/loader.h"
#include "qt_common/config/uisettings.h"
#include "eden/compatibility_list.h"
#include "eden/game/game_list.h"
#include "eden/game/game_list_p.h"
#include "eden/game/game_list_worker.h"

namespace {

QString GetGameListCachedObject(const std::string& filename, const std::string& ext,
                                const std::function<QString()>& generator) {
    if (!UISettings::values.cache_game_list || filename == "0000000000000000") {
        return generator();
    }

    const auto path =
        Common::FS::PathToUTF8String(Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) /
                                     "game_list" / fmt::format("{}.{}", filename, ext));

    void(Common::FS::CreateParentDirs(path));

    if (!Common::FS::Exists(path)) {
        const auto str = generator();

        QFile file{QString::fromStdString(path)};
        if (file.open(QFile::WriteOnly)) {
            file.write(str.toUtf8());
        }

        return str;
    }

    QFile file{QString::fromStdString(path)};
    if (file.open(QFile::ReadOnly)) {
        return QString::fromUtf8(file.readAll());
    }

    return generator();
}

std::pair<std::vector<u8>, std::string> GetGameListCachedObject(
    const std::string& filename, const std::string& ext,
    const std::function<std::pair<std::vector<u8>, std::string>()>& generator) {
    if (!UISettings::values.cache_game_list || filename == "0000000000000000") {
        return generator();
    }

    const auto game_list_dir =
        Common::FS::GetEdenPath(Common::FS::EdenPath::CacheDir) / "game_list";
    const auto jpeg_name = fmt::format("{}.jpeg", filename);
    const auto app_name = fmt::format("{}.appname.txt", filename);

    const auto path1 = Common::FS::PathToUTF8String(game_list_dir / jpeg_name);
    const auto path2 = Common::FS::PathToUTF8String(game_list_dir / app_name);

    void(Common::FS::CreateParentDirs(path1));

    if (!Common::FS::Exists(path1) || !Common::FS::Exists(path2)) {
        const auto [icon, nacp] = generator();

        QFile file1{QString::fromStdString(path1)};
        if (!file1.open(QFile::WriteOnly)) {
            LOG_ERROR(Frontend, "Failed to open cache file.");
            return generator();
        }

        if (!file1.resize(icon.size())) {
            LOG_ERROR(Frontend, "Failed to resize cache file to necessary size.");
            return generator();
        }

        if (file1.write(reinterpret_cast<const char*>(icon.data()), icon.size()) !=
            s64(icon.size())) {
            LOG_ERROR(Frontend, "Failed to write data to cache file.");
            return generator();
        }

        QFile file2{QString::fromStdString(path2)};
        if (file2.open(QFile::WriteOnly)) {
            file2.write(nacp.data(), nacp.size());
        }

        return std::make_pair(icon, nacp);
    }

    QFile file1(QString::fromStdString(path1));
    QFile file2(QString::fromStdString(path2));

    if (!file1.open(QFile::ReadOnly)) {
        LOG_ERROR(Frontend, "Failed to open cache file for reading.");
        return generator();
    }

    if (!file2.open(QFile::ReadOnly)) {
        LOG_ERROR(Frontend, "Failed to open cache file for reading.");
        return generator();
    }

    std::vector<u8> vec(file1.size());
    if (file1.read(reinterpret_cast<char*>(vec.data()), vec.size()) !=
        static_cast<s64>(vec.size())) {
        return generator();
    }

    const auto data = file2.readAll();
    return std::make_pair(vec, data.toStdString());
}

void GetMetadataFromControlNCA(const FileSys::PatchManager& patch_manager, const FileSys::NCA& nca, size_t size,
                               std::vector<u8>& icon, std::string& name) {
    std::tie(icon, name) = GetGameListCachedObject(
        fmt::format("{:016X}_{}", patch_manager.GetTitleID(), size), {}, [&patch_manager, &nca] {
            const auto [nacp, icon_f] = patch_manager.ParseControlNCA(nca);
            return std::make_pair(icon_f->ReadAllBytes(), nacp->GetApplicationName());
        });
}

bool HasSupportedFileExtension(const std::string& file_name) {
    const QFileInfo file = QFileInfo(QString::fromStdString(file_name));
    return GameList::supported_file_extensions.contains(file.suffix(), Qt::CaseInsensitive);
}

bool IsExtractedNCAMain(const std::string& file_name) {
    return QFileInfo(QString::fromStdString(file_name)).fileName() == QStringLiteral("main");
}

QString FormatGameName(const std::string& physical_name) {
    const QString physical_name_as_qstring = QString::fromStdString(physical_name);
    const QFileInfo file_info(physical_name_as_qstring);

    if (IsExtractedNCAMain(physical_name)) {
        return file_info.dir().path();
    }

    return physical_name_as_qstring;
}

QString FormatPatchNameVersions(const FileSys::PatchManager& patch_manager,
                                Loader::AppLoader& loader, bool updatable = true) {
    QString out;
    FileSys::VirtualFile update_raw;
    loader.ReadUpdateRaw(update_raw);
    bool has_update = false;
    for (const auto& patch : patch_manager.GetPatches(update_raw)) {
        const bool is_update = patch.name == "Update";
        if (!updatable && is_update) {
            continue;
        }
        if (is_update) {
            has_update = true;
        }

        std::string patch_name = patch.name;
        if (patch_name == "Update") {
            patch_name = "Версия";
        } else if (patch_name == "DLC") {
            patch_name = "Дополнения";
        }

        const QString type =
            QString::fromStdString(patch.enabled ? patch_name : "[D] " + patch_name);

        if (patch.version.empty()) {
            out.append(QStringLiteral("%1\n").arg(type));
        } else {
            auto ver = patch.version;

            // Display container name for packed updates
            if (is_update && ver == "PACKED") {
                std::string control_ver;
                const auto control = patch_manager.GetControlMetadata();
                if (control.first != nullptr) {
                    control_ver = control.first->GetVersionString();
                }
                std::string loader_ver;
                FileSys::NACP nacp;
                if (loader.ReadControlData(nacp) == Loader::ResultStatus::Success) {
                    loader_ver = nacp.GetVersionString();
                }

                if (!control_ver.empty() && control_ver != "1.0.0") {
                    ver = control_ver;
                } else if (!loader_ver.empty()) {
                    ver = loader_ver;
                } else if (!control_ver.empty()) {
                    ver = control_ver;
                }
                
                if (ver == "1.0.0" || ver == "PACKED") {
                    u32 v = 0;
                    if (loader.ReadUpdateVersion(v) == Loader::ResultStatus::Success && v != 0) {
                        std::array<u8, 4> bytes{};
                        bytes[0] = static_cast<u8>(v % 0x100); v /= 0x100;
                        bytes[1] = static_cast<u8>(v % 0x100); v /= 0x100;
                        bytes[2] = static_cast<u8>(v % 0x100); v /= 0x100;
                        bytes[3] = static_cast<u8>(v % 0x100);
                        auto cnmt_ver = fmt::format("{}.{}.{}", bytes[3], bytes[2], bytes[1]);
                        if (cnmt_ver != "1.0.0") {
                            ver = cnmt_ver;
                        }
                    }
                }
                
                if (ver == "1.0.0" || ver == "PACKED") {
                    ver = Loader::GetFileTypeString(loader.GetFileType());
                }
            }

            out.append(QStringLiteral("%1 (%2)\n").arg(type, QString::fromStdString(ver)));
        }
    }

    if (!has_update) {
        const auto control = patch_manager.GetControlMetadata();
        std::string control_ver;
        if (control.first != nullptr) {
            control_ver = control.first->GetVersionString();
        }
        std::string loader_ver;
        FileSys::NACP nacp;
        if (loader.ReadControlData(nacp) == Loader::ResultStatus::Success) {
            loader_ver = nacp.GetVersionString();
        }

        std::string version = "1.0.0";
        if (!control_ver.empty() && control_ver != "1.0.0") {
            version = control_ver;
        } else if (!loader_ver.empty()) {
            version = loader_ver;
        } else if (!control_ver.empty()) {
            version = control_ver;
        }

        if (version == "1.0.0") {
            u32 v = 0;
            if (loader.ReadUpdateVersion(v) == Loader::ResultStatus::Success && v != 0) {
                std::array<u8, 4> bytes{};
                bytes[0] = static_cast<u8>(v % 0x100); v /= 0x100;
                bytes[1] = static_cast<u8>(v % 0x100); v /= 0x100;
                bytes[2] = static_cast<u8>(v % 0x100); v /= 0x100;
                bytes[3] = static_cast<u8>(v % 0x100);
                auto cnmt_ver = fmt::format("{}.{}.{}", bytes[3], bytes[2], bytes[1]);
                if (cnmt_ver != "1.0.0") {
                    version = cnmt_ver;
                }
            }
        }

        if (!version.empty() && version != "1.0.0" && version != "1.0") {
            out.prepend(QStringLiteral("Версия (%1)\n").arg(QString::fromStdString(version)));
        }
    }

    out.chop(1);
    return out;
}

QList<QStandardItem*> MakeGameListEntry(const std::string& path, const std::string& name,
                                        const std::size_t size, const std::vector<u8>& icon,
                                        Loader::AppLoader& loader, u64 program_id,
                                        const CompatibilityList& compatibility_list,
                                        const PlayTime::PlayTimeManager& play_time_manager,
                                        const FileSys::PatchManager& patch) {
    auto const it = FindMatchingCompatibilityEntry(compatibility_list, program_id);
    // The game list uses 99 as compatibility number for untested games
    QString compatibility =
        it != compatibility_list.end() ? it->second.first : QStringLiteral("99");

    auto const file_type = loader.GetFileType();
    auto const file_type_string = QString::fromStdString(Loader::GetFileTypeString(file_type));

    QString patch_versions = GetGameListCachedObject(
        fmt::format("{:016X}_{}", patch.GetTitleID(), size), "pv_v5.txt", [&patch, &loader] {
            return FormatPatchNameVersions(patch, loader, loader.IsRomFSUpdatable());
        });

    u64 play_time = play_time_manager.GetPlayTime(program_id);
    auto list = QList<QStandardItem*>{
        new GameListItemPath(FormatGameName(path), icon, QString::fromStdString(name),
                             file_type_string, program_id, play_time, patch_versions),
        new GameListItem(file_type_string),
        new GameListItemSize(size),
        new GameListItemPlayTime(play_time),
        new GameListItem(patch_versions),
        new GameListItemCompat(compatibility),
    };

    list[1]->setTextAlignment(Qt::AlignCenter);
    list[2]->setTextAlignment(Qt::AlignCenter);
    list[3]->setTextAlignment(Qt::AlignCenter);
    list[4]->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    list[5]->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Apply tabular (monospace) font to numerical columns to prevent jumping
    QFont tabular_font(QStringLiteral("Consolas"));
    tabular_font.setFixedPitch(true);
    tabular_font.setPointSize(9);
    list[2]->setFont(tabular_font);
    list[3]->setFont(tabular_font);

    return list;
}
} // Anonymous namespace

GameListWorker::GameListWorker(FileSys::VirtualFilesystem vfs_,
                               FileSys::ManualContentProvider* provider_,
                               QVector<UISettings::GameDir>& game_dirs_,
                               const CompatibilityList& compatibility_list_,
                               const PlayTime::PlayTimeManager& play_time_manager_,
                               Core::System& system_)
    : vfs{std::move(vfs_)}, provider{provider_}, game_dirs{game_dirs_},
      compatibility_list{compatibility_list_}, play_time_manager{play_time_manager_},
      system{system_} {
    // We want the game list to manage our lifetime.
    setAutoDelete(false);
}

GameListWorker::~GameListWorker() {
    this->disconnect();
    stop_requested.store(true);
    processing_completed.Wait();
}

void GameListWorker::ProcessEvents(GameList* game_list) {
    while (true) {
        std::function<void(GameList*)> func;
        {
            // Lock queue to protect concurrent modification.
            std::scoped_lock lk(lock);

            // If we can't pop a function, return.
            if (queued_events.empty()) {
                return;
            }

            // Pop a function.
            func = std::move(queued_events.back());
            queued_events.pop_back();
        }

        // Run the function.
        func(game_list);
    }
}

template <typename F>
void GameListWorker::RecordEvent(F&& func) {
    {
        // Lock queue to protect concurrent modification.
        std::scoped_lock lk(lock);

        // Add the function into the front of the queue.
        queued_events.emplace_front(std::move(func));
    }

    // Data now available.
    emit DataAvailable();
}

void GameListWorker::AddTitlesToGameList(GameListDir* parent_dir) {
    using namespace FileSys;

    const auto& cache = system.GetContentProviderUnion();

    auto installed_games = cache.ListEntriesFilterOrigin(std::nullopt, TitleType::Application,
                                                         ContentRecordType::Program);

    if (parent_dir->type() == static_cast<int>(GameListItemType::SdmcDir)) {
        installed_games = cache.ListEntriesFilterOrigin(
            ContentProviderUnionSlot::SDMC, TitleType::Application, ContentRecordType::Program);
    } else if (parent_dir->type() == static_cast<int>(GameListItemType::UserNandDir)) {
        installed_games = cache.ListEntriesFilterOrigin(
            ContentProviderUnionSlot::UserNAND, TitleType::Application, ContentRecordType::Program);
    } else if (parent_dir->type() == static_cast<int>(GameListItemType::SysNandDir)) {
        installed_games = cache.ListEntriesFilterOrigin(
            ContentProviderUnionSlot::SysNAND, TitleType::Application, ContentRecordType::Program);
    }

    for (const auto& [slot, game] : installed_games) {
        if (slot == ContentProviderUnionSlot::FrontendManual) {
            continue;
        }

        const auto file = cache.GetEntryUnparsed(game.title_id, game.type);
        std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(system, file);
        if (!loader) {
            continue;
        }

        std::vector<u8> icon;
        std::string name;
        u64 program_id = 0;
        const auto result = loader->ReadProgramId(program_id);

        if (result != Loader::ResultStatus::Success) {
            continue;
        }

        const PatchManager patch{program_id, system.GetFileSystemController(),
                                 system.GetContentProvider()};
        LOG_INFO(Frontend, "PatchManager initiated for id {:X}", program_id);
        const auto control = cache.GetEntry(game.title_id, ContentRecordType::Control);
        if (control != nullptr) {
            GetMetadataFromControlNCA(patch, *control, file->GetSize(), icon, name);
        }

        auto entry = MakeGameListEntry(file->GetFullPath(), name, file->GetSize(), icon, *loader,
                                       program_id, compatibility_list, play_time_manager, patch);
        RecordEvent([=](GameList* game_list) { game_list->AddEntry(entry, parent_dir); });
    }
}

void GameListWorker::ScanFileSystem(ScanTarget target, const std::string& dir_path, bool deep_scan,
                                    GameListDir* parent_dir) {
    std::vector<std::filesystem::path> files;
    std::mutex watch_list_mutex;

    const auto collect_callback = [&](const std::filesystem::path& path) -> bool {
        if (stop_requested) {
            return false;
        }
        
        bool is_dir = Common::FS::IsDir(path);
        bool is_split_file = false;
        
        if (is_dir) {
            std::string name = path.filename().string();
            if (name.ends_with(".nsp") || name.ends_with(".nsz") || name.ends_with(".xci") || name.ends_with(".xcz") || 
                name.ends_with(".NSP") || name.ends_with(".NSZ") || name.ends_with(".XCI") || name.ends_with(".XCZ")) {
                is_split_file = true;
            }
            
            if (!is_split_file) {
                std::scoped_lock lk(watch_list_mutex);
                watch_list.append(QString::fromStdString(Common::FS::PathToUTF8String(path)));
            }
        }
        
        if (!is_dir || is_split_file) {
            files.push_back(path);
        }
        return true;
    };

    if (deep_scan) {
        Common::FS::IterateDirEntriesRecursively(dir_path, collect_callback,
                                                 Common::FS::DirEntryFilter::All);
    } else {
        Common::FS::IterateDirEntries(dir_path, collect_callback, Common::FS::DirEntryFilter::All);
    }

    std::mutex provider_mutex;

    QtConcurrent::blockingMap(files, [&](const std::filesystem::path& path) {
        if (stop_requested) {
            return;
        }

        const auto physical_name = Common::FS::PathToUTF8String(path);

        if (!(HasSupportedFileExtension(physical_name) || IsExtractedNCAMain(physical_name))) {
            return;
        }
        auto file = vfs->OpenFile(physical_name, FileSys::OpenMode::Read);
        if (!file) {
            return;
        }

        file = std::make_shared<FileSys::BufferedVfsFile>(std::move(file), 1024 * 1024);

        auto loader = Loader::GetLoader(system, file);
        if (!loader) {
            return;
        }

        const auto file_type = loader->GetFileType();
        if (file_type == Loader::FileType::Unknown || file_type == Loader::FileType::Error) {
            return;
        }

        if (target == ScanTarget::PopulateGameList &&
            Loader::IsContainerType(file_type) &&
            !Loader::IsBootableGameContainer(file, file_type)) {
            return;
        }

        u64 program_id = 0;
        const auto res2 = loader->ReadProgramId(program_id);

        if (target == ScanTarget::FillManualContentProvider) {
            std::scoped_lock lk(provider_mutex);
            if (res2 == Loader::ResultStatus::Success && file_type == Loader::FileType::NCA) {
                provider->AddEntry(FileSys::TitleType::Application,
                                   FileSys::GetCRTypeFromNCAType(FileSys::NCA{file}.GetType()),
                                   program_id, file);
            } else if (Settings::values.ext_content_from_game_dirs.GetValue() &&
                       Loader::IsContainerType(file_type)) {
                void(provider->AddEntriesFromContainer(file));
            }
        } else {
            std::vector<u64> program_ids;
            loader->ReadProgramIds(program_ids);

            if (res2 == Loader::ResultStatus::Success && program_ids.size() > 1 &&
                Loader::IsContainerType(file_type)) {
                for (const auto id : program_ids) {
                    if ((id & 0xFFF) != 0) {
                        continue;
                    }
                    auto sub_loader = Loader::GetLoader(system, file, id);
                    if (!sub_loader) {
                        continue;
                    }

                    auto size = Common::FS::GetSize(physical_name);
                    std::vector<u8> icon;
                    std::string name = " ";
                    std::tie(icon, name) = GetGameListCachedObject(
                        fmt::format("{:016X}_{}", id, size), {}, [loader_ptr = sub_loader.get()] {
                            std::vector<u8> temp_icon;
                            loader_ptr->ReadIcon(temp_icon);
                            std::string temp_name = " ";
                            loader_ptr->ReadTitle(temp_name);
                            return std::make_pair(temp_icon, temp_name);
                        });

                    const FileSys::PatchManager patch{id, system.GetFileSystemController(),
                                                      system.GetContentProvider()};

                    auto entry = MakeGameListEntry(
                        physical_name, name, size, icon, *sub_loader,
                        id, compatibility_list, play_time_manager, patch);

                    RecordEvent(
                        [=](GameList* game_list) { game_list->AddEntry(entry, parent_dir); });
                }
            } else {
                auto size = Common::FS::GetSize(physical_name);
                std::vector<u8> icon;
                std::string name = " ";
                std::tie(icon, name) = GetGameListCachedObject(
                    fmt::format("{:016X}_{}", program_id, size), {}, [loader_ptr = loader.get()] {
                        std::vector<u8> temp_icon;
                        loader_ptr->ReadIcon(temp_icon);
                        std::string temp_name = " ";
                        loader_ptr->ReadTitle(temp_name);
                        return std::make_pair(temp_icon, temp_name);
                    });

                const FileSys::PatchManager patch{program_id, system.GetFileSystemController(),
                                                  system.GetContentProvider()};

                auto entry = MakeGameListEntry(
                    physical_name, name, size, icon, *loader,
                    program_id, compatibility_list, play_time_manager, patch);

                RecordEvent(
                    [=](GameList* game_list) { game_list->AddEntry(entry, parent_dir); });
            }
        }
    });
}

void GameListWorker::run() {
    try {
        watch_list.clear();
        provider->ClearAllEntries();

        const auto DirEntryReady = [&](GameListDir* game_list_dir) {
            RecordEvent([=](GameList* game_list) { game_list->AddDirEntry(game_list_dir); });
        };

        for (UISettings::GameDir& game_dir : game_dirs) {
            if (stop_requested) {
                break;
            }

            if (game_dir.path == std::string("SDMC")) {
                auto* const game_list_dir = new GameListDir(game_dir, GameListItemType::SdmcDir);
                DirEntryReady(game_list_dir);
                AddTitlesToGameList(game_list_dir);
            } else if (game_dir.path == std::string("UserNAND")) {
                auto* const game_list_dir = new GameListDir(game_dir, GameListItemType::UserNandDir);
                DirEntryReady(game_list_dir);
                AddTitlesToGameList(game_list_dir);
            } else if (game_dir.path == std::string("SysNAND")) {
                auto* const game_list_dir = new GameListDir(game_dir, GameListItemType::SysNandDir);
                DirEntryReady(game_list_dir);
                AddTitlesToGameList(game_list_dir);
            } else {
                const QString qpath = QString::fromStdString(game_dir.path);
                if (QDir(qpath).exists()) {
                    watch_list.append(qpath);
                }
                auto* const game_list_dir = new GameListDir(game_dir);
                DirEntryReady(game_list_dir);
                ScanFileSystem(ScanTarget::FillManualContentProvider, game_dir.path, game_dir.deep_scan,
                               game_list_dir);
                ScanFileSystem(ScanTarget::PopulateGameList, game_dir.path, game_dir.deep_scan,
                               game_list_dir);
            }
        }

        RecordEvent([this](GameList* game_list) { game_list->DonePopulating(watch_list); });
        processing_completed.Set();
    } catch (const std::exception& e) {
        LOG_CRITICAL(Frontend, "CRASH PREVENTED: Exception in GameListWorker::run: {}", e.what());
        // Show the error message dialog safely on the main thread
        RecordEvent([=](GameList* game_list) {
            QMessageBox::critical(game_list, tr("Fatal Error in Game List"),
                                  tr("A fatal C++ exception occurred during game list scanning:\n%1").arg(QString::fromUtf8(e.what())));
            game_list->DonePopulating(watch_list);
        });
        processing_completed.Set();
    } catch (...) {
        LOG_CRITICAL(Frontend, "CRASH PREVENTED: Unknown exception in GameListWorker::run!");
        RecordEvent([=](GameList* game_list) {
            QMessageBox::critical(game_list, tr("Fatal Error in Game List"),
                                  tr("An unknown fatal C++ exception occurred during game list scanning."));
            game_list->DonePopulating(watch_list);
        });
        processing_completed.Set();
    }
}
