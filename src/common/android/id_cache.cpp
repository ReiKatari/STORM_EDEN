// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <jni.h>

#include "applets/software_keyboard.h"
#include "applets/web_browser.h"
#include "common/android/id_cache.h"
#include "common/assert.h"
#include "common/fs/fs_android.h"
#include "video_core/rasterizer_interface.h"
#include "common/android/multiplayer/multiplayer.h"
#include <network/network.h>


static JavaVM *s_java_vm;
static jclass s_native_library_class;
static jclass s_disk_cache_progress_class;
static jclass s_load_callback_stage_class;
static jclass s_game_dir_class;
static jmethodID s_game_dir_constructor;
static jmethodID s_exit_emulation_activity;
static jmethodID s_disk_cache_load_progress;
static jmethodID s_on_emulation_started;
static jmethodID s_on_emulation_stopped;
static jmethodID s_on_program_changed;
static jmethodID s_copy_to_storage;
static jmethodID s_file_exists;
static jmethodID s_file_extension;

static jclass s_game_class;
static jmethodID s_game_constructor;
static jfieldID s_game_title_field;
static jfieldID s_game_path_field;
static jfieldID s_game_program_id_field;
static jfieldID s_game_developer_field;
static jfieldID s_game_version_field;
static jfieldID s_game_is_homebrew_field;

static jclass s_string_class;
static jclass s_pair_class;
static jmethodID s_pair_constructor;
static jfieldID s_pair_first_field;
static jfieldID s_pair_second_field;

static jclass s_overlay_control_data_class;
static jmethodID s_overlay_control_data_constructor;
static jfieldID s_overlay_control_data_id_field;
static jfieldID s_overlay_control_data_enabled_field;
static jfieldID s_overlay_control_data_individual_scale_field;
static jfieldID s_overlay_control_data_landscape_position_field;
static jfieldID s_overlay_control_data_portrait_position_field;
static jfieldID s_overlay_control_data_foldable_position_field;

static jclass s_patch_class;
static jmethodID s_patch_constructor;
static jfieldID s_patch_enabled_field;
static jfieldID s_patch_name_field;
static jfieldID s_patch_version_field;
static jfieldID s_patch_type_field;
static jfieldID s_patch_program_id_field;
static jfieldID s_patch_title_id_field;

static jclass s_double_class;
static jmethodID s_double_constructor;
static jfieldID s_double_value_field;

static jclass s_integer_class;
static jmethodID s_integer_constructor;
static jfieldID s_integer_value_field;

static jclass s_boolean_class;
static jmethodID s_boolean_constructor;
static jfieldID s_boolean_value_field;

static jclass s_player_input_class;
static jmethodID s_player_input_constructor;
static jfieldID s_player_input_connected_field;
static jfieldID s_player_input_buttons_field;
static jfieldID s_player_input_analogs_field;
static jfieldID s_player_input_motions_field;
static jfieldID s_player_input_vibration_enabled_field;
static jfieldID s_player_input_vibration_strength_field;
static jfieldID s_player_input_body_color_left_field;
static jfieldID s_player_input_body_color_right_field;
static jfieldID s_player_input_button_color_left_field;
static jfieldID s_player_input_button_color_right_field;
static jfieldID s_player_input_profile_name_field;
static jfieldID s_player_input_use_system_vibrator_field;

static jclass s_yuzu_input_device_interface;
static jmethodID s_yuzu_input_device_get_name;
static jmethodID s_yuzu_input_device_get_guid;
static jmethodID s_yuzu_input_device_get_port;
static jmethodID s_yuzu_input_device_get_supports_vibration;
static jmethodID s_yuzu_input_device_vibrate;
static jmethodID s_yuzu_input_device_get_axes;
static jmethodID s_yuzu_input_device_has_keys;

static jmethodID s_add_netplay_message;
static jmethodID s_clear_chat;

static constexpr jint JNI_VERSION = JNI_VERSION_1_6;

namespace Common::Android {

    JNIEnv *GetEnvForThread() {
        thread_local static struct OwnedEnv {
            OwnedEnv() {
                status = s_java_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
                if (status == JNI_EDETACHED)
                    s_java_vm->AttachCurrentThread(&env, nullptr);
            }

            ~OwnedEnv() {
                if (status == JNI_EDETACHED)
                    s_java_vm->DetachCurrentThread();
            }

            int status;
            JNIEnv *env = nullptr;
        } owned;
        return owned.env;
    }

    jclass GetNativeLibraryClass() {
        return s_native_library_class;
    }

    jclass GetDiskCacheProgressClass() {
        return s_disk_cache_progress_class;
    }

    jclass GetDiskCacheLoadCallbackStageClass() {
        return s_load_callback_stage_class;
    }

    jclass GetGameDirClass() {
        return s_game_dir_class;
    }

    jmethodID GetGameDirConstructor() {
        return s_game_dir_constructor;
    }

    jmethodID GetExitEmulationActivity() {
        return s_exit_emulation_activity;
    }

    jmethodID GetDiskCacheLoadProgress() {
        return s_disk_cache_load_progress;
    }

    jmethodID GetCopyToStorage() {
        return s_copy_to_storage;
    }

    jmethodID GetFileExists() {
        return s_file_exists;
    }

    jmethodID GetFileExtension() {
        return s_file_extension;
    }

    jmethodID GetOnEmulationStarted() {
        return s_on_emulation_started;
    }

    jmethodID GetOnEmulationStopped() {
        return s_on_emulation_stopped;
    }

    jmethodID GetOnProgramChanged() {
        return s_on_program_changed;
    }

    jclass GetGameClass() {
        return s_game_class;
    }

    jmethodID GetGameConstructor() {
        return s_game_constructor;
    }

    jfieldID GetGameTitleField() {
        return s_game_title_field;
    }

    jfieldID GetGamePathField() {
        return s_game_path_field;
    }

    jfieldID GetGameProgramIdField() {
        return s_game_program_id_field;
    }

    jfieldID GetGameDeveloperField() {
        return s_game_developer_field;
    }

    jfieldID GetGameVersionField() {
        return s_game_version_field;
    }

    jfieldID GetGameIsHomebrewField() {
        return s_game_is_homebrew_field;
    }

    jclass GetStringClass() {
        return s_string_class;
    }

    jclass GetPairClass() {
        return s_pair_class;
    }

    jmethodID GetPairConstructor() {
        return s_pair_constructor;
    }

    jfieldID GetPairFirstField() {
        return s_pair_first_field;
    }

    jfieldID GetPairSecondField() {
        return s_pair_second_field;
    }

    jclass GetOverlayControlDataClass() {
        return s_overlay_control_data_class;
    }

    jmethodID GetOverlayControlDataConstructor() {
        return s_overlay_control_data_constructor;
    }

    jfieldID GetOverlayControlDataIdField() {
        return s_overlay_control_data_id_field;
    }

    jfieldID GetOverlayControlDataEnabledField() {
        return s_overlay_control_data_enabled_field;
    }

    jfieldID GetOverlayControlDataIndividualScaleField() {
        return s_overlay_control_data_individual_scale_field;
    }

    jfieldID GetOverlayControlDataLandscapePositionField() {
        return s_overlay_control_data_landscape_position_field;
    }

    jfieldID GetOverlayControlDataPortraitPositionField() {
        return s_overlay_control_data_portrait_position_field;
    }

    jfieldID GetOverlayControlDataFoldablePositionField() {
        return s_overlay_control_data_foldable_position_field;
    }

    jclass GetPatchClass() {
        return s_patch_class;
    }

    jmethodID GetPatchConstructor() {
        return s_patch_constructor;
    }

    jfieldID GetPatchEnabledField() {
        return s_patch_enabled_field;
    }

    jfieldID GetPatchNameField() {
        return s_patch_name_field;
    }

    jfieldID GetPatchVersionField() {
        return s_patch_version_field;
    }

    jfieldID GetPatchTypeField() {
        return s_patch_type_field;
    }

    jfieldID GetPatchProgramIdField() {
        return s_patch_program_id_field;
    }

    jfieldID GetPatchTitleIdField() {
        return s_patch_title_id_field;
    }

    jclass GetDoubleClass() {
        return s_double_class;
    }

    jmethodID GetDoubleConstructor() {
        return s_double_constructor;
    }

    jfieldID GetDoubleValueField() {
        return s_double_value_field;
    }

    jclass GetIntegerClass() {
        return s_integer_class;
    }

    jmethodID GetIntegerConstructor() {
        return s_integer_constructor;
    }

    jfieldID GetIntegerValueField() {
        return s_integer_value_field;
    }

    jclass GetBooleanClass() {
        return s_boolean_class;
    }

    jmethodID GetBooleanConstructor() {
        return s_boolean_constructor;
    }

    jfieldID GetBooleanValueField() {
        return s_boolean_value_field;
    }

    jclass GetPlayerInputClass() {
        return s_player_input_class;
    }

    jmethodID GetPlayerInputConstructor() {
        return s_player_input_constructor;
    }

    jfieldID GetPlayerInputConnectedField() {
        return s_player_input_connected_field;
    }

    jfieldID GetPlayerInputButtonsField() {
        return s_player_input_buttons_field;
    }

    jfieldID GetPlayerInputAnalogsField() {
        return s_player_input_analogs_field;
    }

    jfieldID GetPlayerInputMotionsField() {
        return s_player_input_motions_field;
    }

    jfieldID GetPlayerInputVibrationEnabledField() {
        return s_player_input_vibration_enabled_field;
    }

    jfieldID GetPlayerInputVibrationStrengthField() {
        return s_player_input_vibration_strength_field;
    }

    jfieldID GetPlayerInputBodyColorLeftField() {
        return s_player_input_body_color_left_field;
    }

    jfieldID GetPlayerInputBodyColorRightField() {
        return s_player_input_body_color_right_field;
    }

    jfieldID GetPlayerInputButtonColorLeftField() {
        return s_player_input_button_color_left_field;
    }

    jfieldID GetPlayerInputButtonColorRightField() {
        return s_player_input_button_color_right_field;
    }

    jfieldID GetPlayerInputProfileNameField() {
        return s_player_input_profile_name_field;
    }

    jfieldID GetPlayerInputUseSystemVibratorField() {
        return s_player_input_use_system_vibrator_field;
    }

    jclass GetYuzuInputDeviceInterface() {
        return s_yuzu_input_device_interface;
    }

    jmethodID GetYuzuDeviceGetName() {
        return s_yuzu_input_device_get_name;
    }

    jmethodID GetYuzuDeviceGetGUID() {
        return s_yuzu_input_device_get_guid;
    }

    jmethodID GetYuzuDeviceGetPort() {
        return s_yuzu_input_device_get_port;
    }

    jmethodID GetYuzuDeviceGetSupportsVibration() {
        return s_yuzu_input_device_get_supports_vibration;
    }

    jmethodID GetYuzuDeviceVibrate() {
        return s_yuzu_input_device_vibrate;
    }

    jmethodID GetYuzuDeviceGetAxes() {
        return s_yuzu_input_device_get_axes;
    }

    jmethodID GetYuzuDeviceHasKeys() {
        return s_yuzu_input_device_has_keys;
    }

    jmethodID GetAddNetPlayMessage() {
        return s_add_netplay_message;
    }

    jmethodID ClearChat() {
        return s_clear_chat;
    }

#ifdef __cplusplus
    extern "C" {
#endif

    jint InitFFmpegOnLoad(JavaVM *vm);

#ifdef __ANDROID__
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#include <android/log.h>
#include <cstdio>
#include <cstring>
#include <unwind.h>
#include <dlfcn.h>
#include <exception>
#include <typeinfo>

struct BacktraceState {
    void** current;
    void** end;
};

static _Unwind_Reason_Code UnwindCallback(struct _Unwind_Context* context, void* arg) {
    auto* state = static_cast<BacktraceState*>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current >= state->end) {
            return _URC_END_OF_STACK;
        }
        *state->current++ = reinterpret_cast<void*>(pc);
    }
    return _URC_NO_REASON;
}

static size_t CaptureBacktrace(void** buffer, size_t max) {
    BacktraceState state = {buffer, buffer + max};
    _Unwind_Backtrace(UnwindCallback, &state);
    return static_cast<size_t>(state.current - buffer);
}

static void AppendBacktrace(char* out_buf, size_t out_buf_size, void** buffer, size_t count) {
    size_t offset = strlen(out_buf);
    for (size_t i = 0; i < count; ++i) {
        const void* addr = buffer[i];
        const char* symbol = "";
        const char* fname = "";
        ptrdiff_t sym_offset = 0;

        Dl_info info;
        if (dladdr(addr, &info)) {
            if (info.dli_sname) {
                symbol = info.dli_sname;
                sym_offset = reinterpret_cast<ptrdiff_t>(addr) - reinterpret_cast<ptrdiff_t>(info.dli_saddr);
            }
            if (info.dli_fname) {
                fname = info.dli_fname;
                if (!info.dli_sname) {
                    sym_offset = reinterpret_cast<ptrdiff_t>(addr) - reinterpret_cast<ptrdiff_t>(info.dli_fbase);
                }
            }
        }

        char line[256];
        if (symbol && symbol[0] != '\0') {
            snprintf(line, sizeof(line), "#%02zu  %p  %s (%s+%td)\n", i, addr, fname, symbol, sym_offset);
        } else {
            snprintf(line, sizeof(line), "#%02zu  %p  %s\n", i, addr, fname);
        }

        size_t len = strlen(line);
        if (offset + len < out_buf_size - 1) {
            memcpy(out_buf + offset, line, len);
            offset += len;
            out_buf[offset] = '\0';
        }
    }
}

static void WriteCrashReportToDisk(const char* text) {
    const char* paths[] = {
        "/storage/emulated/0/Download/STORM_EDEN_CRASH.txt",
        "/sdcard/Download/STORM_EDEN_CRASH.txt",
        "/data/data/dev.eden.eden_emulator/files/STORM_EDEN_CRASH.txt",
        "/data/data/org.yuzu.yuzu_emu/files/STORM_EDEN_CRASH.txt",
        "/data/user/0/dev.eden.eden_emulator/files/STORM_EDEN_CRASH.txt",
        "/data/user/0/org.yuzu.yuzu_emu/files/STORM_EDEN_CRASH.txt",
        "/sdcard/Android/data/dev.eden.eden_emulator/files/STORM_EDEN_CRASH.txt",
        "/sdcard/Android/data/org.yuzu.yuzu_emu/files/STORM_EDEN_CRASH.txt"
    };

    for (const auto& path : paths) {
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd >= 0) {
            write(fd, text, strlen(text));
            close(fd);
        }
    }
}

static void NativeCrashSignalHandler(int sig, siginfo_t* info, void* ucontext) {
    const char* sig_name = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: sig_name = "SIGSEGV (Segmentation Fault)"; break;
        case SIGABRT: sig_name = "SIGABRT (Abort)"; break;
        case SIGBUS:  sig_name = "SIGBUS (Bus Error)"; break;
        case SIGILL:  sig_name = "SIGILL (Illegal Instruction)"; break;
        case SIGFPE:  sig_name = "SIGFPE (Floating Point Exception)"; break;
        case SIGSYS:  sig_name = "SIGSYS (Bad System Call)"; break;
    }

    void* frames[48];
    size_t count = CaptureBacktrace(frames, 48);

    char buf[4096];
    snprintf(buf, sizeof(buf),
        "=======================================================\n"
        "STORM EDEN Android - NATIVE CRASH REPORT\n"
        "=======================================================\n"
        "Signal: %d (%s)\n"
        "Fault Address: %p\n"
        "Signal Code: %d\n\n"
        "--- NATIVE BACKTRACE ---\n",
        sig, sig_name, info ? info->si_addr : nullptr, info ? info->si_code : 0);

    AppendBacktrace(buf, sizeof(buf), frames, count);

    __android_log_print(ANDROID_LOG_FATAL, "STORM_EDEN_CRASH", "%s", buf);
    WriteCrashReportToDisk(buf);

    signal(sig, SIG_DFL);
    raise(sig);
}

static void CustomTerminateHandler() {
    char exc_buf[512] = "C++ Exception: unknown\n";
    try {
        auto cur = ::std::current_exception();
        if (cur) {
            ::std::rethrow_exception(cur);
        }
    } catch (const ::std::exception& e) {
        snprintf(exc_buf, sizeof(exc_buf), "C++ std::exception: %s\n", e.what());
    } catch (...) {
        snprintf(exc_buf, sizeof(exc_buf), "C++ Exception: unknown non-std exception\n");
    }

    void* frames[48];
    size_t count = CaptureBacktrace(frames, 48);

    char buf[4096];
    snprintf(buf, sizeof(buf),
        "=======================================================\n"
        "STORM EDEN Android - UNCAUGHT C++ EXCEPTION REPORT\n"
        "=======================================================\n"
        "%s\n"
        "--- NATIVE BACKTRACE ---\n", exc_buf);

    AppendBacktrace(buf, sizeof(buf), frames, count);

    __android_log_print(ANDROID_LOG_FATAL, "STORM_EDEN_CRASH", "%s", buf);
    WriteCrashReportToDisk(buf);

    abort();
}

static void InstallNativeCrashHandler() {
    ::std::set_terminate(CustomTerminateHandler);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = NativeCrashSignalHandler;
    sa.sa_flags = static_cast<int>(SA_SIGINFO | SA_ONSTACK | SA_RESETHAND);
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
    sigaction(SIGILL, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
    sigaction(SIGSYS, &sa, nullptr);
}

static jclass FindClassSafe(JNIEnv* env, const char* name) {
    jclass local = env->FindClass(name);
    if (env->ExceptionCheck() || !local) {
        __android_log_print(ANDROID_LOG_ERROR, "STORM_EDEN_CRASH", "[JNI] FindClass FAILED for: %s", name);
        env->ExceptionClear();
        return nullptr;
    }
    jclass global = reinterpret_cast<jclass>(env->NewGlobalRef(local));
    env->DeleteLocalRef(local);
    return global;
}

static jmethodID GetMethodIDSafe(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    if (!clazz) return nullptr;
    jmethodID id = env->GetMethodID(clazz, name, sig);
    if (env->ExceptionCheck() || !id) {
        __android_log_print(ANDROID_LOG_ERROR, "STORM_EDEN_CRASH", "[JNI] GetMethodID FAILED for: %s %s", name, sig);
        env->ExceptionClear();
        return nullptr;
    }
    return id;
}

static jmethodID GetStaticMethodIDSafe(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    if (!clazz) return nullptr;
    jmethodID id = env->GetStaticMethodID(clazz, name, sig);
    if (env->ExceptionCheck() || !id) {
        __android_log_print(ANDROID_LOG_ERROR, "STORM_EDEN_CRASH", "[JNI] GetStaticMethodID FAILED for: %s %s", name, sig);
        env->ExceptionClear();
        return nullptr;
    }
    return id;
}

static jfieldID GetFieldIDSafe(JNIEnv* env, jclass clazz, const char* name, const char* sig) {
    if (!clazz) return nullptr;
    jfieldID id = env->GetFieldID(clazz, name, sig);
    if (env->ExceptionCheck() || !id) {
        __android_log_print(ANDROID_LOG_ERROR, "STORM_EDEN_CRASH", "[JNI] GetFieldID FAILED for: %s %s", name, sig);
        env->ExceptionClear();
        return nullptr;
    }
    return id;
}
#endif

    jint JNI_OnLoad(JavaVM *vm, void *reserved) {
        s_java_vm = vm;
#ifdef __ANDROID__
        InstallNativeCrashHandler();
#endif
        InitFFmpegOnLoad(vm);

        JNIEnv *env;
        if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION) != JNI_OK)
            return JNI_ERR;

#ifdef __ANDROID__
        // Initialize Java classes safely
        s_native_library_class = FindClassSafe(env, "org/yuzu/yuzu_emu/NativeLibrary");
        s_disk_cache_progress_class = FindClassSafe(env, "org/yuzu/yuzu_emu/disk_shader_cache/DiskShaderCacheProgress");
        s_load_callback_stage_class = FindClassSafe(env, "org/yuzu/yuzu_emu/disk_shader_cache/DiskShaderCacheProgress$LoadCallbackStage");

        s_game_dir_class = FindClassSafe(env, "org/yuzu/yuzu_emu/model/GameDir");
        s_game_dir_constructor = GetMethodIDSafe(env, s_game_dir_class, "<init>", "(Ljava/lang/String;Z)V");

        // Initialize methods
        s_exit_emulation_activity = GetStaticMethodIDSafe(env, s_native_library_class, "exitEmulationActivity", "(I)V");
        s_disk_cache_load_progress = GetStaticMethodIDSafe(env, s_disk_cache_progress_class, "loadProgress", "(III)V");
        s_copy_to_storage = GetStaticMethodIDSafe(env, s_native_library_class, "copyFileToStorage", "(Ljava/lang/String;Ljava/lang/String;)Z");
        s_file_exists = GetStaticMethodIDSafe(env, s_native_library_class, "exists", "(Ljava/lang/String;)Z");
        s_file_extension = GetStaticMethodIDSafe(env, s_native_library_class, "getFileExtension", "(Ljava/lang/String;)Ljava/lang/String;");
        s_on_emulation_started = GetStaticMethodIDSafe(env, s_native_library_class, "onEmulationStarted", "()V");
        s_on_emulation_stopped = GetStaticMethodIDSafe(env, s_native_library_class, "onEmulationStopped", "(I)V");
        s_on_program_changed = GetStaticMethodIDSafe(env, s_native_library_class, "onProgramChanged", "(I)V");

        s_game_class = FindClassSafe(env, "org/yuzu/yuzu_emu/model/Game");
        s_game_constructor = GetMethodIDSafe(env, s_game_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)V");
        s_game_title_field = GetFieldIDSafe(env, s_game_class, "title", "Ljava/lang/String;");
        s_game_path_field = GetFieldIDSafe(env, s_game_class, "path", "Ljava/lang/String;");
        s_game_program_id_field = GetFieldIDSafe(env, s_game_class, "programId", "Ljava/lang/String;");
        s_game_developer_field = GetFieldIDSafe(env, s_game_class, "developer", "Ljava/lang/String;");
        s_game_version_field = GetFieldIDSafe(env, s_game_class, "version", "Ljava/lang/String;");
        s_game_is_homebrew_field = GetFieldIDSafe(env, s_game_class, "isHomebrew", "Z");

        s_string_class = FindClassSafe(env, "java/lang/String");

        s_pair_class = FindClassSafe(env, "kotlin/Pair");
        s_pair_constructor = GetMethodIDSafe(env, s_pair_class, "<init>", "(Ljava/lang/Object;Ljava/lang/Object;)V");
        s_pair_first_field = GetFieldIDSafe(env, s_pair_class, "first", "Ljava/lang/Object;");
        s_pair_second_field = GetFieldIDSafe(env, s_pair_class, "second", "Ljava/lang/Object;");

        s_overlay_control_data_class = FindClassSafe(env, "org/yuzu/yuzu_emu/overlay/model/OverlayControlData");
        s_overlay_control_data_constructor = GetMethodIDSafe(env, s_overlay_control_data_class, "<init>", "(Ljava/lang/String;ZLkotlin/Pair;Lkotlin/Pair;Lkotlin/Pair;F)V");
        s_overlay_control_data_id_field = GetFieldIDSafe(env, s_overlay_control_data_class, "id", "Ljava/lang/String;");
        s_overlay_control_data_enabled_field = GetFieldIDSafe(env, s_overlay_control_data_class, "enabled", "Z");
        s_overlay_control_data_landscape_position_field = GetFieldIDSafe(env, s_overlay_control_data_class, "landscapePosition", "Lkotlin/Pair;");
        s_overlay_control_data_portrait_position_field = GetFieldIDSafe(env, s_overlay_control_data_class, "portraitPosition", "Lkotlin/Pair;");
        s_overlay_control_data_foldable_position_field = GetFieldIDSafe(env, s_overlay_control_data_class, "foldablePosition", "Lkotlin/Pair;");
        s_overlay_control_data_individual_scale_field = GetFieldIDSafe(env, s_overlay_control_data_class, "individualScale", "F");

        s_patch_class = FindClassSafe(env, "org/yuzu/yuzu_emu/model/Patch");
        s_patch_constructor = GetMethodIDSafe(env, s_patch_class, "<init>", "(ZLjava/lang/String;Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;JI)V");
        s_patch_enabled_field = GetFieldIDSafe(env, s_patch_class, "enabled", "Z");
        s_patch_name_field = GetFieldIDSafe(env, s_patch_class, "name", "Ljava/lang/String;");
        s_patch_version_field = GetFieldIDSafe(env, s_patch_class, "version", "Ljava/lang/String;");
        s_patch_type_field = GetFieldIDSafe(env, s_patch_class, "type", "I");
        s_patch_program_id_field = GetFieldIDSafe(env, s_patch_class, "programId", "Ljava/lang/String;");
        s_patch_title_id_field = GetFieldIDSafe(env, s_patch_class, "titleId", "Ljava/lang/String;");

        s_double_class = FindClassSafe(env, "java/lang/Double");
        s_double_constructor = GetMethodIDSafe(env, s_double_class, "<init>", "(D)V");
        s_double_value_field = GetFieldIDSafe(env, s_double_class, "value", "D");

        s_integer_class = FindClassSafe(env, "java/lang/Integer");
        s_integer_constructor = GetMethodIDSafe(env, s_integer_class, "<init>", "(I)V");
        s_integer_value_field = GetFieldIDSafe(env, s_integer_class, "value", "I");

        s_boolean_class = FindClassSafe(env, "java/lang/Boolean");
        s_boolean_constructor = GetMethodIDSafe(env, s_boolean_class, "<init>", "(Z)V");
        s_boolean_value_field = GetFieldIDSafe(env, s_boolean_class, "value", "Z");

        s_player_input_class = FindClassSafe(env, "org/yuzu/yuzu_emu/features/input/model/PlayerInput");
        s_player_input_constructor = GetMethodIDSafe(env, s_player_input_class, "<init>", "(Z[Ljava/lang/String;[Ljava/lang/String;[Ljava/lang/String;ZIJJJJLjava/lang/String;Z)V");
        s_player_input_connected_field = GetFieldIDSafe(env, s_player_input_class, "connected", "Z");
        s_player_input_buttons_field = GetFieldIDSafe(env, s_player_input_class, "buttons", "[Ljava/lang/String;");
        s_player_input_analogs_field = GetFieldIDSafe(env, s_player_input_class, "analogs", "[Ljava/lang/String;");
        s_player_input_motions_field = GetFieldIDSafe(env, s_player_input_class, "motions", "[Ljava/lang/String;");
        s_player_input_vibration_enabled_field = GetFieldIDSafe(env, s_player_input_class, "vibrationEnabled", "Z");
        s_player_input_vibration_strength_field = GetFieldIDSafe(env, s_player_input_class, "vibrationStrength", "I");
        s_player_input_body_color_left_field = GetFieldIDSafe(env, s_player_input_class, "bodyColorLeft", "J");
        s_player_input_body_color_right_field = GetFieldIDSafe(env, s_player_input_class, "bodyColorRight", "J");
        s_player_input_button_color_left_field = GetFieldIDSafe(env, s_player_input_class, "buttonColorLeft", "J");
        s_player_input_button_color_right_field = GetFieldIDSafe(env, s_player_input_class, "buttonColorRight", "J");
        s_player_input_profile_name_field = GetFieldIDSafe(env, s_player_input_class, "profileName", "Ljava/lang/String;");
        s_player_input_use_system_vibrator_field = GetFieldIDSafe(env, s_player_input_class, "useSystemVibrator", "Z");

        s_yuzu_input_device_interface = FindClassSafe(env, "org/yuzu/yuzu_emu/features/input/YuzuInputDevice");
        s_yuzu_input_device_get_name = GetMethodIDSafe(env, s_yuzu_input_device_interface, "getName", "()Ljava/lang/String;");
        s_yuzu_input_device_get_guid = GetMethodIDSafe(env, s_yuzu_input_device_interface, "getGUID", "()Ljava/lang/String;");
        s_yuzu_input_device_get_port = GetMethodIDSafe(env, s_yuzu_input_device_interface, "getPort", "()I");
        s_yuzu_input_device_get_supports_vibration = GetMethodIDSafe(env, s_yuzu_input_device_interface, "getSupportsVibration", "()Z");
        s_yuzu_input_device_vibrate = GetMethodIDSafe(env, s_yuzu_input_device_interface, "vibrate", "(F)V");
        s_yuzu_input_device_get_axes = GetMethodIDSafe(env, s_yuzu_input_device_interface, "getAxes", "()[Ljava/lang/Integer;");
        s_yuzu_input_device_has_keys = GetMethodIDSafe(env, s_yuzu_input_device_interface, "hasKeys", "([I)[Z");

        s_add_netplay_message = GetStaticMethodIDSafe(env, s_native_library_class, "addNetPlayMessage", "(ILjava/lang/String;)V");
        s_clear_chat = GetStaticMethodIDSafe(env, s_native_library_class, "clearChat", "()V");

        // Initialize Android Storage
        Common::FS::Android::RegisterCallbacks(env, s_native_library_class);

        // Initialize applets
        Common::Android::SoftwareKeyboard::InitJNI(env);
        Common::Android::WebBrowser::InitJNI(env);
#endif

        return JNI_VERSION;
    }

    void JNI_OnUnload(JavaVM *vm, void *reserved) {
        JNIEnv *env;
        if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION) != JNI_OK) {
            return;
        }

        // UnInitialize Android Storage
        Common::FS::Android::UnRegisterCallbacks();
        env->DeleteGlobalRef(s_native_library_class);
        env->DeleteGlobalRef(s_disk_cache_progress_class);
        env->DeleteGlobalRef(s_load_callback_stage_class);
        env->DeleteGlobalRef(s_game_dir_class);
        env->DeleteGlobalRef(s_game_class);
        env->DeleteGlobalRef(s_string_class);
        env->DeleteGlobalRef(s_pair_class);
        env->DeleteGlobalRef(s_overlay_control_data_class);
        env->DeleteGlobalRef(s_patch_class);
        env->DeleteGlobalRef(s_double_class);
        env->DeleteGlobalRef(s_integer_class);
        env->DeleteGlobalRef(s_boolean_class);
        env->DeleteGlobalRef(s_player_input_class);
        env->DeleteGlobalRef(s_yuzu_input_device_interface);

        // UnInitialize applets
        SoftwareKeyboard::CleanupJNI(env);
        WebBrowser::CleanupJNI(env);

        AndroidMultiplayer::NetworkShutdown();
    }

#ifdef __cplusplus
    }
#endif

} // namespace Common::Android
