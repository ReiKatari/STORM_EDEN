// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <regex>
#include "common/android/android_common.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/file_sys/fs_filesystem.h"
#include "core/file_sys/patch_manager.h"
#include "core/loader/loader.h"
#include "core/loader/nro.h"
#include "native.h"

struct RomMetadata {
    std::string title;
    u64 programId;
    std::string developer;
    std::string version;
    std::string internal_version;
    int addon_count{0};
    std::vector<u8> icon;
    bool isHomebrew;
};
static ankerl::unordered_dense::map<std::string, RomMetadata> m_rom_metadata_cache;
static ankerl::unordered_dense::map<u64, int> m_aoc_count_cache;
static bool m_aoc_cache_valid = false;

static RomMetadata CacheRomMetadata(const std::string& path) {
    auto& instance = EmulationSession::GetInstance();
    const auto file = Core::GetGameFileFromPath(instance.System().GetFilesystem(), path);
    if (auto loader = Loader::GetLoader(instance.System(), file, 0, 0); loader) {
        RomMetadata entry;
        loader->ReadTitle(entry.title);
        loader->ReadProgramId(entry.programId);
        loader->ReadIcon(entry.icon);

        const FileSys::PatchManager pm{
            entry.programId,
            instance.System().GetFileSystemController(),
            instance.System().GetContentProvider()
        };
        const auto control = pm.GetControlMetadata();
        const auto game_version = pm.GetGameVersion();

        if (control.first != nullptr) {
            entry.developer = control.first->GetDeveloperName();
            entry.version = control.first->GetVersionString();
        } else {
            FileSys::NACP nacp{};
            if (loader->ReadControlData(nacp) == Loader::ResultStatus::Success) {
                entry.developer = nacp.GetDeveloperName();
                entry.version = nacp.GetVersionString();
            } else {
                entry.developer = "";
                entry.version = "1.0.0";
            }
        }

        // Check if filename contains paired display version and internal version (e.g. "(1.5.1 - 262144 - ...)")
        std::regex pair_ver_regex(R"(\(([0-9]+\.[0-9]+(?:\.[0-9]+)*)\s*-\s*([0-9]+))");
        std::smatch pair_match;
        if (std::regex_search(path, pair_match, pair_ver_regex) && pair_match.size() > 2) {
            entry.version = pair_match[1].str();
        }

        // Clean version string: remove leading 'v' / 'V'
        while (entry.version.starts_with('v') || entry.version.starts_with('V')) {
            entry.version = entry.version.substr(1);
        }
        if (entry.version.empty()) {
            entry.version = "1.0.0";
        }

        // Accurate internal version: numeric string without 'v' (e.g. 65536, 393216, 0)
        u32 internal_ver = 0;
        if (game_version.has_value() && *game_version > 0) {
            internal_ver = *game_version;
        } else {
            internal_ver = instance.System().GetContentProvider().GetEntryVersion(entry.programId).value_or(0);
        }

        // Check if filename contains version tag like [v65536], (v131072), [v0], _v393216
        if (internal_ver == 0) {
            std::regex ver_tag_regex(R"([\[\(_]v(\d+)[\]\)])", std::regex::icase);
            std::smatch match;
            if (std::regex_search(path, match, ver_tag_regex) && match.size() > 1) {
                try {
                    internal_ver = static_cast<u32>(std::stoul(match[1].str()));
                } catch (...) {}
            }
        }

        // If internal_ver is still 0, but display version string is e.g. "1.2.0" or "1.1.0" or "2.0.0"
        if (internal_ver == 0 && !entry.version.empty() && entry.version != "1.0.0" && entry.version != "1.0") {
            int major = 1, minor = 0, patch = 0;
            if (std::sscanf(entry.version.c_str(), "%d.%d.%d", &major, &minor, &patch) >= 2) {
                if (major >= 1) {
                    internal_ver = static_cast<u32>((major - 1) * 655360 + minor * 65536 + (patch * 65536) / 10);
                }
            }
        }

        entry.internal_version = std::to_string(internal_ver);

        // Count DLC / Addons for this game from ContentProvider
        int aoc_count = 0;
        const u64 base_tid = FileSys::GetBaseTitleID(entry.programId);
        const auto dlc_entries = instance.System().GetContentProvider().ListEntriesFilter(
            FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);
        for (const auto& dlc : dlc_entries) {
            if (FileSys::GetBaseTitleID(dlc.title_id) == base_tid) {
                aoc_count++;
            }
        }
        if (aoc_count == 0) {
            auto prev_it = m_rom_metadata_cache.find(path);
            if (prev_it != m_rom_metadata_cache.end() && prev_it->second.addon_count > 0) {
                aoc_count = prev_it->second.addon_count;
            }
        }
        entry.addon_count = aoc_count;

        if (loader->GetFileType() == Loader::FileType::NRO) {
            auto loader_nro = reinterpret_cast<Loader::AppLoader_NRO*>(loader.get());
            entry.isHomebrew = loader_nro->IsHomebrew();
        } else {
            entry.isHomebrew = false;
        }
        m_rom_metadata_cache[path] = entry;
        return entry;
    }
    return {};
}

static RomMetadata GetRomMetadata(const std::string& path, bool reload = false) {
    if (reload)
        return CacheRomMetadata(path);
    if (auto it = m_rom_metadata_cache.find(path); it != m_rom_metadata_cache.end())
        return it->second;
    return CacheRomMetadata(path);
}

extern "C" {

jboolean Java_org_yuzu_yuzu_1emu_utils_GameMetadata_getIsValid(JNIEnv* env, jobject obj, jstring jpath) {
    const std::string path_str = Common::Android::GetJString(env, jpath);
    const auto l_path = Common::ToLower(path_str);
    if (l_path.ends_with(".part") || l_path.ends_with(".tmp") ||
        l_path.ends_with(".crdownload") || l_path.ends_with(".downloading") ||
        l_path.ends_with(".incomplete") || l_path.ends_with(".!ut")) {
        return false;
    }

    if (auto const file = EmulationSession::GetInstance().System().GetFilesystem()->OpenFile(path_str, FileSys::OpenMode::Read); file) {
        if (file->GetSize() == 0) {
            return false;
        }
        if (auto loader = Loader::GetLoader(EmulationSession::GetInstance().System(), file); loader) {
            auto const file_type = loader->GetFileType();
            if (file_type == Loader::FileType::Unknown || file_type == Loader::FileType::Error)
                return false;
            if ((file_type == Loader::FileType::NSP || file_type == Loader::FileType::XCI ||
                 file_type == Loader::FileType::NSZ || file_type == Loader::FileType::XCZ) &&
                !Loader::IsBootableGameContainer(file, file_type))
                return false;
            u64 program_id = 0;
            if (loader->ReadProgramId(program_id) != Loader::ResultStatus::Success || program_id == 0)
                return false;
            if ((program_id & 0xFFF) != 0)
                return false; // Exclude standalone DLCs and Updates
            return true;
        }
    }
    return false;
}

jstring Java_org_yuzu_yuzu_1emu_utils_GameMetadata_getTitle(JNIEnv* env, jobject obj, jstring jpath) {
    return Common::Android::ToJString(env, GetRomMetadata(Common::Android::GetJString(env, jpath)).title);
}

jstring Java_org_yuzu_yuzu_1emu_utils_GameMetadata_getProgramId(JNIEnv* env, jobject obj, jstring jpath) {
    return Common::Android::ToJString(env, std::to_string(GetRomMetadata(Common::Android::GetJString(env, jpath)).programId));
}

jstring Java_org_yuzu_yuzu_1emu_utils_GameMetadata_getDeveloper(JNIEnv* env, jobject obj, jstring jpath) {
    return Common::Android::ToJString(env, GetRomMetadata(Common::Android::GetJString(env, jpath)).developer);
}

jstring Java_org_yuzu_yuzu_1emu_utils_GameMetadata_getVersion(JNIEnv* env, jobject obj, jstring jpath, jboolean jreload) {
    return Common::Android::ToJString(env, GetRomMetadata(Common::Android::GetJString(env, jpath), jreload).version);
}

jstring Java_org_yuzu_yuzu_1emu_utils_GameMetadata_getInternalVersion(JNIEnv* env, jobject obj, jstring jpath) {
    return Common::Android::ToJString(env, GetRomMetadata(Common::Android::GetJString(env, jpath)).internal_version);
}

jint Java_org_yuzu_yuzu_1emu_utils_GameMetadata_getAddonCount(JNIEnv* env, jobject obj, jstring jpath) {
    return jint(GetRomMetadata(Common::Android::GetJString(env, jpath)).addon_count);
}

jbyteArray Java_org_yuzu_yuzu_1emu_utils_GameMetadata_getIcon(JNIEnv* env, jobject obj, jstring jpath) {
    auto icon_data = GetRomMetadata(Common::Android::GetJString(env, jpath)).icon;
    jbyteArray icon = env->NewByteArray(jsize(icon_data.size()));
    env->SetByteArrayRegion(icon, 0, env->GetArrayLength(icon), reinterpret_cast<jbyte*>(icon_data.data()));
    return icon;
}

jboolean Java_org_yuzu_yuzu_1emu_utils_GameMetadata_getIsHomebrew(JNIEnv* env, jobject obj, jstring jpath) {
    return jboolean(GetRomMetadata(Common::Android::GetJString(env, jpath)).isHomebrew);
}

void Java_org_yuzu_yuzu_1emu_utils_GameMetadata_resetMetadata(JNIEnv* env, jobject obj) {
    m_rom_metadata_cache.clear();
    m_aoc_count_cache.clear();
    m_aoc_cache_valid = false;
}

} // extern "C"
