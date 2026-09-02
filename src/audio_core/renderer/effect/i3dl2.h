// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <vector>

#include "audio_core/common/common.h"
#include "audio_core/renderer/effect/effect_info_base.h"
#include "common/common_types.h"
#include "common/fixed_point.h"

namespace AudioCore::Renderer {

class I3dl2ReverbInfo : public EffectInfoBase {
public:
    struct ParameterVersion1 {
        /* 0x00 */ std::array<s8, MaxChannels> inputs;
        /* 0x06 */ std::array<s8, MaxChannels> outputs;
        /* 0x0C */ u16 channel_count_max;
        /* 0x0E */ u16 channel_count;
        /* 0x10 */ char unk10[0x4];
        /* 0x14 */ u32 sample_rate;
        /* 0x18 */ f32 room_HF_gain;
        /* 0x1C */ f32 reference_HF;
        /* 0x20 */ f32 late_reverb_decay_time;
        /* 0x24 */ f32 late_reverb_HF_decay_ratio;
        /* 0x28 */ f32 room_gain;
        /* 0x2C */ f32 reflection_gain;
        /* 0x30 */ f32 reverb_gain;
        /* 0x34 */ f32 late_reverb_diffusion;
        /* 0x38 */ f32 reflection_delay;
        /* 0x3C */ f32 late_reverb_delay_time;
        /* 0x40 */ f32 late_reverb_density;
        /* 0x44 */ f32 dry_gain;
        /* 0x48 */ ParameterState state;
        /* 0x49 */ char unk49[0x3];
    };
    static_assert(sizeof(ParameterVersion1) <= sizeof(EffectInfoBase::InParameterVersion1),
                  "I3dl2ReverbInfo::ParameterVersion1 has the wrong size!");

    struct ParameterVersion2 {
        /* 0x00 */ std::array<s8, MaxChannels> inputs;
        /* 0x06 */ std::array<s8, MaxChannels> outputs;
        /* 0x0C */ u16 channel_count_max;
        /* 0x0E */ u16 channel_count;
        /* 0x10 */ char unk10[0x4];
        /* 0x14 */ u32 sample_rate;
        /* 0x18 */ f32 room_HF_gain;
        /* 0x1C */ f32 reference_HF;
        /* 0x20 */ f32 late_reverb_decay_time;
        /* 0x24 */ f32 late_reverb_HF_decay_ratio;
        /* 0x28 */ f32 room_gain;
        /* 0x2C */ f32 reflection_gain;
        /* 0x30 */ f32 reverb_gain;
        /* 0x34 */ f32 late_reverb_diffusion;
        /* 0x38 */ f32 reflection_delay;
        /* 0x3C */ f32 late_reverb_delay_time;
        /* 0x40 */ f32 late_reverb_density;
        /* 0x44 */ f32 dry_gain;
        /* 0x48 */ ParameterState state;
        /* 0x49 */ char unk49[0x3];
    };
    static_assert(sizeof(ParameterVersion2) <= sizeof(EffectInfoBase::InParameterVersion2),
                  "I3dl2ReverbInfo::ParameterVersion2 has the wrong size!");

    static constexpr u32 MaxDelayLines = 4;
    static constexpr u32 MaxDelayTaps = 20;

    struct I3dl2DelayLine {
        void Initialize(const s32 delay_time) {
            max_delay = delay_time;
            buffer.assign(static_cast<size_t>(delay_time) + 1, 0);
            buffer_end = buffer.data() + buffer.size();
            output = buffer.data();
            SetDelay(delay_time);
            wet_gain = 0.0f;
        }

        void SetDelay(const s32 delay_time) {
            if (buffer.empty() || buffer.data() == nullptr || buffer_end == nullptr || max_delay < delay_time || max_delay < 0) {
                return;
            }
            delay = delay_time;
            const auto output_offset =
                (output && output >= buffer.data() && output < buffer_end) ? (output - buffer.data()) : 0;
            const size_t target_idx = static_cast<size_t>((output_offset + delay) % (max_delay + 1));
            if (target_idx < buffer.size()) {
                input = &buffer[target_idx];
            } else {
                input = buffer.data();
            }
        }

        Common::FixedPoint<50, 14> Tick(const Common::FixedPoint<50, 14> sample) {
            if (buffer.empty() || output == nullptr || input == nullptr || buffer.data() == nullptr || buffer_end == nullptr) {
                return sample;
            }
            Write(sample);

            auto out_sample{Read()};

            output++;
            if (output >= buffer_end || output < buffer.data()) {
                output = buffer.data();
            }

            return out_sample;
        }

        Common::FixedPoint<50, 14> Read() const {
            if (buffer.empty() || output == nullptr || buffer.data() == nullptr || buffer_end == nullptr) {
                return 0;
            }
            if (output < buffer.data() || output >= buffer_end) {
                return *buffer.data();
            }
            return *output;
        }

        void Write(const Common::FixedPoint<50, 14> sample) {
            if (buffer.empty() || input == nullptr || buffer.data() == nullptr || buffer_end == nullptr) {
                return;
            }
            if (input < buffer.data() || input >= buffer_end) {
                input = buffer.data();
            }
            *input = sample;
            input++;
            if (input >= buffer_end) {
                input = buffer.data();
            }
        }

        Common::FixedPoint<50, 14> TapOut(const s32 index) const {
            if (buffer.empty() || input == nullptr || buffer.data() == nullptr || buffer_end == nullptr) {
                return 0;
            }
            if (input < buffer.data() || input >= buffer_end) {
                return 0;
            }
            auto out{input - (index + 1)};
            if (out < buffer.data()) {
                out += max_delay + 1;
            }
            if (out >= buffer_end || out < buffer.data()) {
                return 0;
            }
            return *out;
        }

        std::vector<Common::FixedPoint<50, 14>> buffer{};
        Common::FixedPoint<50, 14>* buffer_end{};
        s32 max_delay{};
        Common::FixedPoint<50, 14>* input{};
        Common::FixedPoint<50, 14>* output{};
        s32 delay{};
        f32 wet_gain{};
    };

    struct State {
        f32 lowpass_0;
        f32 lowpass_1;
        f32 lowpass_2;
        I3dl2DelayLine early_delay_line;
        std::array<s32, MaxDelayTaps> early_tap_steps;
        f32 early_gain;
        f32 late_gain;
        s32 early_to_late_taps;
        std::array<I3dl2DelayLine, MaxDelayLines> fdn_delay_lines;
        std::array<I3dl2DelayLine, MaxDelayLines> decay_delay_lines0;
        std::array<I3dl2DelayLine, MaxDelayLines> decay_delay_lines1;
        f32 last_reverb_echo;
        I3dl2DelayLine center_delay_line;
        std::array<std::array<f32, 3>, MaxDelayLines> lowpass_coeff;
        std::array<f32, MaxDelayLines> shelf_filter;
        f32 dry_gain;
    };
    static_assert(sizeof(State) <= sizeof(EffectInfoBase::State),
                  "I3dl2ReverbInfo::State is too large!");

    /**
     * Update the info with new parameters, version 1.
     *
     * @param error_info  - Used to write call result code.
     * @param in_params   - New parameters to update the info with.
     * @param pool_mapper - Pool for mapping buffers.
     */
    void Update(BehaviorInfo::ErrorInfo& error_info, const InParameterVersion1& in_params,
                const PoolMapper& pool_mapper) override;

    /**
     * Update the info with new parameters, version 2.
     *
     * @param error_info  - Used to write call result code.
     * @param in_params   - New parameters to update the info with.
     * @param pool_mapper - Pool for mapping buffers.
     */
    void Update(BehaviorInfo::ErrorInfo& error_info, const InParameterVersion2& in_params,
                const PoolMapper& pool_mapper) override;

    /**
     * Update the info after command generation. Usually only changes its state.
     */
    void UpdateForCommandGeneration() override;

    /**
     * Initialize a new result state. Version 2 only, unused.
     *
     * @param result_state - Result state to initialize.
     */
    void InitializeResultState(EffectResultState& result_state) override;

    /**
     * Update the host-side state with the ADSP-side state. Version 2 only, unused.
     *
     * @param cpu_state - Host-side result state to update.
     * @param dsp_state - AudioRenderer-side result state to update from.
     */
    void UpdateResultState(EffectResultState& cpu_state, EffectResultState& dsp_state) override;

    /**
     * Get a workbuffer assigned to this effect with the given index.
     *
     * @param index - Workbuffer index.
     * @return Address of the buffer.
     */
    CpuAddr GetWorkbuffer(s32 index) override;
};

} // namespace AudioCore::Renderer
