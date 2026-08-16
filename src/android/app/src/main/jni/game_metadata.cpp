// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/android/android_common.h"
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

        // Clean version string: remove leading 'v' / 'V'
        while (entry.version.starts_with('v') || entry.version.starts_with('V')) {
            entry.version = entry.version.substr(1);
        }
        if (entry.version.empty()) {
            entry.version = "1.0.0";
        }

        // Accurate internal version: numeric string without 'v' (e.g. 65536, 393216, 0)
        u32 internal_ver = 0;
        if (game_version.has_value()) {
            internal_ver = *game_version;
        } else {
            internal_ver = instance.System().GetContentProvider().GetEntryVersion(entry.programId).value_or(0);
        }
        entry.internal_version = std::to_string(internal_ver);

        // Count DLC / Addons for this game
        const auto dlc_entries = instance.System().GetContentProvider().ListEntriesFilter(
            FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);
        int dlc_count = 0;
        for (const auto& dlc : dlc_entries) {
            if (FileSys::GetBaseTitleID(dlc.title_id) == entry.programId) {
                ++dlc_count;
            }
        }
        entry.addon_count = dlc_count;

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
    if (auto const file = EmulationSession::GetInstance().System().GetFilesystem()->OpenFile(Common::Android::GetJString(env, jpath), FileSys::OpenMode::Read); file) {
        if (auto loader = Loader::GetLoader(EmulationSession::GetInstance().System(), file); loader) {
            auto const file_type = loader->GetFileType();
            if (file_type == Loader::FileType::Unknown || file_type == Loader::FileType::Error)
                return false;
            if ((file_type == Loader::FileType::NSP || file_type == Loader::FileType::XCI) && !Loader::IsBootableGameContainer(file, file_type))
                return false;
            u64 program_id = 0;
            return loader->ReadProgramId(program_id) == Loader::ResultStatus::Success;
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
}

} // extern "C"
