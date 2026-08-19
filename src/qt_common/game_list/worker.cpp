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
#include <QSettings>

#include "common/cityhash.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/nca_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/submission_package.h"
#include "core/loader/loader.h"

#include "qt_common/config/uisettings.h"
#include "qt_common/qt_common.h"

#include "qt_common/game_list/game_list_p.h"

#include "qt_common/game_list/model.h"
#include "qt_common/game_list/worker.h"

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

void GetMetadataFromControlNCA(const FileSys::PatchManager& patch_manager, const FileSys::NCA& nca,
                               std::vector<u8>& icon, std::string& name) {
    std::tie(icon, name) = GetGameListCachedObject(
        fmt::format("{:016X}", patch_manager.GetTitleID()), {}, [&patch_manager, &nca] {
            const auto [nacp, icon_f] = patch_manager.ParseControlNCA(nca);
            return std::make_pair(icon_f->ReadAllBytes(), nacp->GetApplicationName());
        });
}

bool HasSupportedFileExtension(const std::string& file_name) {
    const QFileInfo file = QFileInfo(QString::fromStdString(file_name));
    return QtCommon::supported_file_extensions.contains(file.suffix(), Qt::CaseInsensitive);
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
    for (const auto& patch : patch_manager.GetPatches(update_raw)) {
        const bool is_update = patch.name == "Update";
        if (!updatable && is_update) {
            continue;
        }

        const QString type =
            QString::fromStdString(patch.enabled ? patch.name : "[D] " + patch.name);

        if (patch.version.empty()) {
            out.append(QStringLiteral("%1\n").arg(type));
        } else {
            auto ver = patch.version;

            // Display container name for packed updates
            if (is_update && ver == "PACKED") {
                ver = Loader::GetFileTypeString(loader.GetFileType());
            }

            out.append(QStringLiteral("%1 (%2)\n").arg(type, QString::fromStdString(ver)));
        }
    }

    out.chop(1);
    return out;
}

QString FormatAddonsColumnText(const QString& patch_versions, const QString& base_version = QStringLiteral("1.0.0")) {
    QString version_num = base_version.trimmed();
    while (version_num.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        version_num.remove(0, 1);
    }
    version_num = version_num.trimmed();
    if (version_num.isEmpty() || version_num == QStringLiteral("0")) {
        version_num = QStringLiteral("1.0.0");
    }

    int dlc_count = 0;
    const QStringList lines = patch_versions.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        if (!line.contains(QStringLiteral("Update"), Qt::CaseInsensitive) &&
            !line.contains(QStringLiteral("Обновление"), Qt::CaseInsensitive)) {
            QString clean = line.trimmed();
            clean.replace(QStringLiteral("DLC "), QStringLiteral(""), Qt::CaseInsensitive);
            clean.replace(QStringLiteral("DLC"), QStringLiteral(""), Qt::CaseInsensitive);
            clean.remove(QLatin1Char('('));
            clean.remove(QLatin1Char(')'));
            clean = clean.trimmed();
            if (!clean.isEmpty()) {
                const auto parts = clean.split(QLatin1Char(','), Qt::SkipEmptyParts);
                dlc_count += parts.isEmpty() ? 1 : parts.size();
            }
        }
    }

    QString result = QObject::tr("Версия: %1").arg(version_num);
    if (dlc_count > 0) {
        const QString addons_word = dlc_count == 1 ? QObject::tr("Дополнение") : QObject::tr("Дополнения");
        result.append(QStringLiteral("\n%1: %2").arg(addons_word, QString::number(dlc_count)));
    }
    return result;
}

QList<QStandardItem*> MakeGameListEntry(const std::string& path, const std::string& name,
                                        const std::size_t size, const std::vector<u8>& icon,
                                        Loader::AppLoader& loader, u64 program_id,
                                        const PlayTime::PlayTimeManager& play_time_manager,
                                        const FileSys::PatchManager& patch) {
    auto const file_type = loader.GetFileType();
    QString file_type_string = QString::fromStdString(Loader::GetFileTypeString(file_type));
    const auto ext = Common::ToLower(std::string(Common::FS::GetExtensionFromFilename(path)));
    if (ext == "nsz") {
        file_type_string = QStringLiteral("NSZ");
    } else if (ext == "xcz") {
        file_type_string = QStringLiteral("XCZ");
    }

    const u64 file_path_hash = Common::CityHash64(path.data(), path.size());
    QString patch_versions = GetGameListCachedObject(
        fmt::format("{:016X}_{:016X}", program_id, file_path_hash), "pv.txt", [&patch, &loader] {
            return FormatPatchNameVersions(patch, loader, loader.IsRomFSUpdatable());
        });

    u64 play_time = play_time_manager.GetPlayTime(program_id);

    // Determine the exact version for this specific file
    QString file_version;

    // 1. Try filename regex for paired version (e.g. "(1.5.1 - 262144 - ...)" or "(1.6.15.13 - 1310720 - ...)")
    static const QRegularExpression fn_pair_ver_regex{QStringLiteral(R"(\(([0-9]+\.[0-9]+(?:\.[0-9]+)*)\s*-\s*([0-9]+))")};
    const auto fm = fn_pair_ver_regex.match(QString::fromStdString(path));
    if (fm.hasMatch() && !fm.captured(1).isEmpty()) {
        file_version = fm.captured(1);
    }

    // 2. Try PatchManager control metadata (which prioritizes updates)
    if (file_version.isEmpty() || file_version == QStringLiteral("1.0.0") || file_version == QStringLiteral("0")) {
        if (const auto nacp = patch.GetControlMetadata().first; nacp != nullptr) {
            const auto ver = nacp->GetVersionString();
            if (!ver.empty() && ver != "0") {
                file_version = QString::fromStdString(ver);
            }
        }
    }

    // 3. Try reading control data directly from loader
    if (file_version.isEmpty() || file_version == QStringLiteral("1.0.0") || file_version == QStringLiteral("0")) {
        FileSys::NACP file_nacp;
        if (loader.ReadControlData(file_nacp) == Loader::ResultStatus::Success) {
            auto ver = file_nacp.GetVersionString();
            if (!ver.empty() && ver != "0") {
                file_version = QString::fromStdString(ver);
            }
        }
    }

    // 4. Fallback to general filename version regex
    if (file_version.isEmpty() || file_version == QStringLiteral("1.0.0") || file_version == QStringLiteral("0")) {
        static const QRegularExpression fn_ver_regex{QStringLiteral(R"((?:[\(\[\s]v?|\b)([0-9]+\.[0-9]+(?:\.[0-9]+)*)(?!\s*(?:GB|MB|KB|TB|ГБ|МБ|КБ|Б|B)\b))")};
        const auto m = fn_ver_regex.match(QString::fromStdString(path));
        if (m.hasMatch() && m.hasCaptured(1)) {
            const QString parsed_fn_ver = m.captured(1);
            if (!parsed_fn_ver.isEmpty()) {
                file_version = parsed_fn_ver;
            }
        }
    }

    if (file_version.isEmpty() || file_version == QStringLiteral("1.0.0") || file_version == QStringLiteral("0")) {
        static const QRegularExpression fn_vnum_regex{QStringLiteral(R"(\[v([0-9]+)\])")};
        const auto vm = fn_vnum_regex.match(QString::fromStdString(path));
        if (vm.hasMatch()) {
            const u32 vnum = vm.captured(1).toUInt();
            if (vnum > 0) {
                file_version = QString::number(vnum);
            }
        }
    }

    while (file_version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        file_version.remove(0, 1);
    }
    file_version = file_version.trimmed();

    if (file_version.isEmpty() || file_version == QStringLiteral("0")) {
        file_version = QStringLiteral("1.0.0");
    }

    QString addons_text = FormatAddonsColumnText(patch_versions, file_version);

    return QList<QStandardItem*>{
        new GameListItemPath(FormatGameName(path), icon, QString::fromStdString(name),
                             file_type_string, program_id, play_time, patch_versions, size, file_version),
        new GameListItemCentered(file_type_string),
        new GameListItemSize(size),
        new GameListItemPlayTime(play_time),
        new GameListItem(addons_text),
    };
}
} // Anonymous namespace

GameListWorker::GameListWorker(FileSys::VirtualFilesystem vfs_,
                               FileSys::ManualContentProvider* provider_,
                               QVector<UISettings::GameDir>& game_dirs_,
                               const PlayTime::PlayTimeManager& play_time_manager_,
                               Core::System& system_)
    : vfs{std::move(vfs_)}, provider{provider_}, game_dirs{game_dirs_},
      play_time_manager{play_time_manager_},
      system{system_} {
    // We want the game list to manage our lifetime.
    setAutoDelete(false);
}

GameListWorker::~GameListWorker() {
    this->disconnect();
    stop_requested.store(true);
    processing_completed.Wait();
}

void GameListWorker::ProcessEvents(GameListModel* model) {
    while (true) {
        std::function<void(GameListModel*)> func;
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
        func(model);
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
            GetMetadataFromControlNCA(patch, *control, icon, name);
        }

        auto entry = MakeGameListEntry(file->GetFullPath(), name, file->GetSize(), icon, *loader,
                                       program_id, play_time_manager, patch);
        RecordEvent([=](GameListModel* model) { model->AddEntry(entry, parent_dir); });
    }
}

void GameListWorker::ScanFileSystem(ScanTarget target, const std::string& dir_path, bool deep_scan,
                                    GameListDir* parent_dir) {
    const auto callback = [this, target, parent_dir](const std::filesystem::path& path) -> bool {
        if (stop_requested) {
            // Breaks the callback loop.
            return false;
        }

        const auto physical_name = Common::FS::PathToUTF8String(path);
        const auto is_dir = Common::FS::IsDir(path);

        if (!is_dir &&
            (HasSupportedFileExtension(physical_name) || IsExtractedNCAMain(physical_name))) {
            try {
                const auto file = vfs->OpenFile(physical_name, FileSys::OpenMode::Read);
                if (!file) {
                    return true;
                }

                auto loader = Loader::GetLoader(system, file);
                if (!loader) {
                    return true;
                }

                const auto file_type = loader->GetFileType();
                if (file_type == Loader::FileType::Unknown || file_type == Loader::FileType::Error) {
                    return true;
                }

                if (target == ScanTarget::PopulateGameList &&
                    (file_type == Loader::FileType::XCI || file_type == Loader::FileType::XCZ ||
                     file_type == Loader::FileType::NSP || file_type == Loader::FileType::NSZ)) {
                    if (!Loader::IsBootableGameContainer(file, file_type)) {
                        if (file->GetSize() < 0x100000) {
                            return true;
                        }
                    }
                }

                u64 program_id = 0;
                const auto res2 = loader->ReadProgramId(program_id);

                if (target == ScanTarget::FillManualContentProvider) {
                    if (res2 == Loader::ResultStatus::Success && file_type == Loader::FileType::NCA) {
                        provider->AddEntry(FileSys::TitleType::Application,
                                           FileSys::GetCRTypeFromNCAType(FileSys::NCA{file}.GetType()),
                                           program_id, file);
                    } else if (Settings::values.ext_content_from_game_dirs.GetValue() &&
                               (file_type == Loader::FileType::XCI || file_type == Loader::FileType::XCZ ||
                                file_type == Loader::FileType::NSP || file_type == Loader::FileType::NSZ)) {
                        void(provider->AddEntriesFromContainer(file));
                    }
                } else {
                    std::vector<u64> program_ids;
                    loader->ReadProgramIds(program_ids);

                    const auto addEntry = [this, physical_name,
                                           parent_dir](std::unique_ptr<Loader::AppLoader>& app_loader,
                                                       const u64 id) {
                        const FileSys::PatchManager patch{id, system.GetFileSystemController(),
                                                          system.GetContentProvider()};

                        std::vector<u8> icon;
                        [[maybe_unused]] const auto res1 = app_loader->ReadIcon(icon);
                        if (icon.empty()) {
                            const auto control = patch.GetControlMetadata();
                            if (control.second != nullptr) {
                                icon = control.second->ReadAllBytes();
                            }
                        }

                        std::string name = " ";
                        [[maybe_unused]] const auto res3 = app_loader->ReadTitle(name);
                        if (name.empty() || name == " ") {
                            const auto control = patch.GetControlMetadata();
                            if (control.first != nullptr) {
                                name = control.first->GetApplicationName();
                            }
                        }
                        if (name.empty() || name == " ") {
                            const std::string filename_str = std::filesystem::path(physical_name).stem().string();
                            if (!filename_str.empty()) {
                                name = filename_str;
                            }
                        }

                        auto entry = MakeGameListEntry(
                            physical_name, name, Common::FS::GetSize(physical_name), icon, *app_loader,
                            id, play_time_manager, patch);

                        RecordEvent([=](GameListModel* model) { model->AddEntry(entry, parent_dir); });
                    };

                    if (res2 == Loader::ResultStatus::Success && program_ids.size() > 1 &&
                        (file_type == Loader::FileType::XCI || file_type == Loader::FileType::XCZ ||
                         file_type == Loader::FileType::NSP || file_type == Loader::FileType::NSZ)) {
                        for (const auto id : program_ids) {
                            // dravee suggested this, only viable way to
                            // not show sub-games in qlaunch for now.
                            if ((id & 0xFFF) != 0) {
                                continue;
                            }
                            loader = Loader::GetLoader(system, file, id);
                            if (!loader) {
                                continue;
                            }

                            addEntry(loader, id);
                        }
                    } else {
                        addEntry(loader, program_id);
                    }
                }
            } catch (const std::exception& e) {
                LOG_WARNING(Frontend, "Exception while scanning file {}: {}", physical_name, e.what());
                return true;
            } catch (...) {
                LOG_WARNING(Frontend, "Unknown exception while scanning file {}", physical_name);
                return true;
            }
        } else if (is_dir) {
            watch_list.append(QString::fromStdString(physical_name));
        }

        return true;
    };

    if (deep_scan) {
        Common::FS::IterateDirEntriesRecursively(dir_path, callback,
                                                 Common::FS::DirEntryFilter::All);
    } else {
        Common::FS::IterateDirEntries(dir_path, callback, Common::FS::DirEntryFilter::File);
    }
}

void GameListWorker::run() {
    watch_list.clear();
    provider->ClearAllEntries();

    const auto DirEntryReady = [&](GameListDir* game_list_dir) {
        RecordEvent([=](GameListModel* model) { model->AddDirEntry(game_list_dir); });
    };

    for (UISettings::GameDir& game_dir : game_dirs) {
        if (stop_requested) {
            break;
        }

        GameListDir* game_list_dir;
        bool scan = false;

        if (game_dir.path == std::string("SDMC")) {
            game_list_dir = new GameListDir(game_dir, GameListItemType::SdmcDir);
        } else if (game_dir.path == std::string("UserNAND")) {
            game_list_dir = new GameListDir(game_dir, GameListItemType::UserNandDir);
        } else if (game_dir.path == std::string("SysNAND")) {
            game_list_dir = new GameListDir(game_dir, GameListItemType::SysNandDir);
        } else {
            const QString qpath = QString::fromStdString(game_dir.path);
            if (QDir(qpath).exists()) {
                watch_list.append(qpath);
            }

            game_list_dir = new GameListDir(game_dir);
            scan = true;
        }

        DirEntryReady(game_list_dir);
        if (scan) {
            ScanFileSystem(ScanTarget::FillManualContentProvider, game_dir.path, game_dir.deep_scan,
                           game_list_dir);
            ScanFileSystem(ScanTarget::PopulateGameList, game_dir.path, game_dir.deep_scan,
                           game_list_dir);
        } else {
            AddTitlesToGameList(game_list_dir);
        }
    }

    RecordEvent([this](GameListModel* model) { model->DonePopulating(watch_list); });
    processing_completed.Set();
}
