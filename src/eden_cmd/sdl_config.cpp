// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// SDL will break our main function in eden-cmd if we don't define this before adding SDL.h
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <fstream>

#include "common/logging.h"
#include "input_common/main.h"
#include "sdl_config.h"

const std::array<int, Settings::NativeButton::NumButtons> SdlConfig::default_buttons = {
    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_T,
    SDL_SCANCODE_G, SDL_SCANCODE_F, SDL_SCANCODE_H, SDL_SCANCODE_Q, SDL_SCANCODE_W,
    SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_B,
};

const std::array<int, Settings::NativeMotion::NumMotions> SdlConfig::default_motions = {
    SDL_SCANCODE_7,
    SDL_SCANCODE_8,
};

const std::array<std::array<int, 4>, Settings::NativeAnalog::NumAnalogs> SdlConfig::default_analogs{
    {
        {
            SDL_SCANCODE_UP,
            SDL_SCANCODE_DOWN,
            SDL_SCANCODE_LEFT,
            SDL_SCANCODE_RIGHT,
        },
        {
            SDL_SCANCODE_I,
            SDL_SCANCODE_K,
            SDL_SCANCODE_J,
            SDL_SCANCODE_L,
        },
    }};

const std::array<int, 2> SdlConfig::default_stick_mod = {
    SDL_SCANCODE_D,
    0,
};

const std::array<int, 2> SdlConfig::default_ringcon_analogs{{
    0,
    0,
}};

extern std::ofstream debug_file;

SdlConfig::SdlConfig(const std::optional<std::string> config_path) {
    debug_file << "Checkpoint SdlConfig constructor started\n";
    debug_file.flush();
    Initialize(config_path);
    debug_file << "Checkpoint SdlConfig Initialize done\n";
    debug_file.flush();
    ReadSdlValues();
    debug_file << "Checkpoint SdlConfig ReadSdlValues done\n";
    debug_file.flush();
    SaveSdlValues();
    debug_file << "Checkpoint SdlConfig constructor done\n";
    debug_file.flush();
}

SdlConfig::~SdlConfig() {
    if (global) {
        SdlConfig::SaveAllValues();
    }
}

void SdlConfig::ReloadAllValues() {
    Reload();
    ReadSdlValues();
    SaveSdlValues();
}

void SdlConfig::SaveAllValues() {
    SaveValues();
    SaveSdlValues();
}

void SdlConfig::ReadSdlValues() {
    ReadSdlControlValues();
}

void SdlConfig::ReadSdlControlValues() {
    debug_file << "Checkpoint ReadSdlControlValues 1: Entering\n";
    debug_file.flush();
    BeginGroup(Settings::TranslateCategory(Settings::Category::Controls));

    debug_file << "Checkpoint ReadSdlControlValues 2: BeginGroup done\n";
    debug_file.flush();
    Settings::values.players.SetGlobal(!IsCustomConfig());
    debug_file << "Checkpoint ReadSdlControlValues 3: SetGlobal done. Players count: " << Settings::values.players.GetValue().size() << "\n";
    debug_file.flush();
    for (std::size_t p = 0; p < Settings::values.players.GetValue().size(); ++p) {
        debug_file << "Checkpoint ReadSdlControlValues 4: Player " << p << " starting\n";
        debug_file.flush();
        ReadSdlPlayerValues(p);
        debug_file << "Checkpoint ReadSdlControlValues 5: Player " << p << " done\n";
        debug_file.flush();
    }
    if (IsCustomConfig()) {
        EndGroup();
        return;
    }
    debug_file << "Checkpoint ReadSdlControlValues 6: DebugControl starting\n";
    debug_file.flush();
    ReadDebugControlValues();
    debug_file << "Checkpoint ReadSdlControlValues 7: DebugControl done\n";
    debug_file.flush();
    ReadHidbusValues();
    debug_file << "Checkpoint ReadSdlControlValues 8: Hidbus done\n";
    debug_file.flush();

    EndGroup();
}

void SdlConfig::ReadSdlPlayerValues(const std::size_t player_index) {
    std::string player_prefix;
    if (type != ConfigType::InputProfile) {
        player_prefix.append("player_").append(ToString(player_index)).append("_");
    }

    auto& player = Settings::values.players.GetValue()[player_index];
    if (IsCustomConfig()) {
        const auto profile_name =
            ReadStringSetting(std::string(player_prefix).append("profile_name"));
        if (profile_name.empty()) {
            // Use the global input config
            player = Settings::values.players.GetValue(true)[player_index];
            player.profile_name = "";
            return;
        }
    }

    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        auto& player_buttons = player.buttons[i];

        player_buttons = ReadStringSetting(
            std::string(player_prefix).append(Settings::NativeButton::mapping[i]), default_param);
        if (player_buttons.empty()) {
            player_buttons = default_param;
        }
    }

    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_stick_mod[i], 0.5f);
        auto& player_analogs = player.analogs[i];

        player_analogs = ReadStringSetting(
            std::string(player_prefix).append(Settings::NativeAnalog::mapping[i]), default_param);
        if (player_analogs.empty()) {
            player_analogs = default_param;
        }
    }

    for (int i = 0; i < Settings::NativeMotion::NumMotions; ++i) {
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_motions[i]);
        auto& player_motions = player.motions[i];

        player_motions = ReadStringSetting(
            std::string(player_prefix).append(Settings::NativeMotion::mapping[i]), default_param);
        if (player_motions.empty()) {
            player_motions = default_param;
        }
    }
}

void SdlConfig::ReadDebugControlValues() {
    debug_file << "Checkpoint ReadDebugControlValues 1: Starting buttons loop. NumButtons=" << Settings::NativeButton::NumButtons << "\n";
    debug_file.flush();
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        debug_file << "Checkpoint ReadDebugControlValues 2: button index=" << i << "\n";
        debug_file.flush();
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        debug_file << "Checkpoint ReadDebugControlValues 3: default_param generated\n";
        debug_file.flush();
        auto& debug_pad_buttons = Settings::values.debug_pad_buttons[i];
        debug_file << "Checkpoint ReadDebugControlValues 4: mapping=" << Settings::NativeButton::mapping[i] << "\n";
        debug_file.flush();
        debug_pad_buttons = ReadStringSetting(
            std::string("debug_pad_").append(Settings::NativeButton::mapping[i]), default_param);
        debug_file << "Checkpoint ReadDebugControlValues 5: ReadStringSetting done\n";
        debug_file.flush();
        if (debug_pad_buttons.empty()) {
            debug_pad_buttons = default_param;
        }
    }
    debug_file << "Checkpoint ReadDebugControlValues 6: Starting analogs loop. NumAnalogs=" << Settings::NativeAnalog::NumAnalogs << "\n";
    debug_file.flush();
    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        debug_file << "Checkpoint ReadDebugControlValues 7: analog index=" << i << "\n";
        debug_file.flush();
        const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_stick_mod[i], 0.5f);
        debug_file << "Checkpoint ReadDebugControlValues 8: default_param generated\n";
        debug_file.flush();
        auto& debug_pad_analogs = Settings::values.debug_pad_analogs[i];
        debug_file << "Checkpoint ReadDebugControlValues 9: mapping=" << Settings::NativeAnalog::mapping[i] << "\n";
        debug_file.flush();
        debug_pad_analogs = ReadStringSetting(
            std::string("debug_pad_").append(Settings::NativeAnalog::mapping[i]), default_param);
        debug_file << "Checkpoint ReadDebugControlValues 10: ReadStringSetting done\n";
        debug_file.flush();
        if (debug_pad_analogs.empty()) {
            debug_pad_analogs = default_param;
        }
    }
    debug_file << "Checkpoint ReadDebugControlValues 11: done\n";
    debug_file.flush();
}

void SdlConfig::ReadHidbusValues() {
    const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
        0, 0, default_ringcon_analogs[0], default_ringcon_analogs[1], 0, 0.05f);
    auto& ringcon_analogs = Settings::values.ringcon_analogs;

    ringcon_analogs = ReadStringSetting(std::string("ring_controller"), default_param);
    if (ringcon_analogs.empty()) {
        ringcon_analogs = default_param;
    }
}

void SdlConfig::SaveSdlValues() {
    debug_file << "Checkpoint SaveSdlValues 1: Entering\n";
    debug_file.flush();
    LOG_DEBUG(Config, "Saving SDL configuration values");
    SaveSdlControlValues();
    debug_file << "Checkpoint SaveSdlValues 2: SaveSdlControlValues done\n";
    debug_file.flush();

    WriteToIni();
    debug_file << "Checkpoint SaveSdlValues 3: WriteToIni done\n";
    debug_file.flush();
}

void SdlConfig::SaveSdlControlValues() {
    debug_file << "Checkpoint SaveSdlControlValues 1: Entering\n";
    debug_file.flush();
    BeginGroup(Settings::TranslateCategory(Settings::Category::Controls));

    Settings::values.players.SetGlobal(!IsCustomConfig());
    debug_file << "Checkpoint SaveSdlControlValues 2: players count=" << Settings::values.players.GetValue().size() << "\n";
    debug_file.flush();
    for (std::size_t p = 0; p < Settings::values.players.GetValue().size(); ++p) {
        debug_file << "Checkpoint SaveSdlControlValues 3: Saving player " << p << " starting\n";
        debug_file.flush();
        SaveSdlPlayerValues(p);
        debug_file << "Checkpoint SaveSdlControlValues 4: Saving player " << p << " done\n";
        debug_file.flush();
    }
    if (IsCustomConfig()) {
        EndGroup();
        return;
    }
    debug_file << "Checkpoint SaveSdlControlValues 5: SaveDebugControlValues starting\n";
    debug_file.flush();
    SaveDebugControlValues();
    debug_file << "Checkpoint SaveSdlControlValues 6: SaveHidbusValues starting\n";
    debug_file.flush();
    SaveHidbusValues();

    EndGroup();
    debug_file << "Checkpoint SaveSdlControlValues 7: done\n";
    debug_file.flush();
}

void SdlConfig::SaveSdlPlayerValues(const std::size_t player_index) {
    std::string player_prefix;
    if (type != ConfigType::InputProfile) {
        player_prefix = std::string("player_").append(ToString(player_index)).append("_");
    }

    const auto& player = Settings::values.players.GetValue()[player_index];
    if (IsCustomConfig() && player.profile_name.empty()) {
        // No custom profile selected
        return;
    }

    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        WriteStringSetting(std::string(player_prefix).append(Settings::NativeButton::mapping[i]),
                           player.buttons[i], std::make_optional(default_param));
    }
    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_stick_mod[i], 0.5f);
        WriteStringSetting(std::string(player_prefix).append(Settings::NativeAnalog::mapping[i]),
                           player.analogs[i], std::make_optional(default_param));
    }
    for (int i = 0; i < Settings::NativeMotion::NumMotions; ++i) {
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_motions[i]);
        WriteStringSetting(std::string(player_prefix).append(Settings::NativeMotion::mapping[i]),
                           player.motions[i], std::make_optional(default_param));
    }
}

void SdlConfig::SaveDebugControlValues() {
    debug_file << "Checkpoint SaveDebugControlValues 1: Entering\n";
    debug_file.flush();
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        const std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        WriteStringSetting(std::string("debug_pad_").append(Settings::NativeButton::mapping[i]),
                           Settings::values.debug_pad_buttons[i],
                           std::make_optional(default_param));
    }
    debug_file << "Checkpoint SaveDebugControlValues 2: buttons done\n";
    debug_file.flush();
    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_stick_mod[i], 0.5f);
        WriteStringSetting(std::string("debug_pad_").append(Settings::NativeAnalog::mapping[i]),
                           Settings::values.debug_pad_analogs[i],
                           std::make_optional(default_param));
    }
    debug_file << "Checkpoint SaveDebugControlValues 3: done\n";
    debug_file.flush();
}

void SdlConfig::SaveHidbusValues() {
    debug_file << "Checkpoint SaveHidbusValues 1: Entering\n";
    debug_file.flush();
    const std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
        0, 0, default_ringcon_analogs[0], default_ringcon_analogs[1], 0, 0.05f);
    WriteStringSetting(std::string("ring_controller"), Settings::values.ringcon_analogs,
                       std::make_optional(default_param));
    debug_file << "Checkpoint SaveHidbusValues 2: done\n";
    debug_file.flush();
}

std::vector<Settings::BasicSetting*>& SdlConfig::FindRelevantList(Settings::Category category) {
    return Settings::values.linkage.by_category[category];
}
