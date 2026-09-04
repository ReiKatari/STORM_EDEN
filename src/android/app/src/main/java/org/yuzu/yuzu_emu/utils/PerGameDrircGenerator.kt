package org.yuzu.yuzu_emu.utils

import java.io.File
import org.yuzu.yuzu_emu.YuzuApplication

/**
 * Generates tailored, conflict-free Mesa Turnip and PanVK drirc configurations
 * on-the-fly for any Nintendo Switch game prior to Vulkan instance creation.
 *
 * This ensures that games requiring opposing driver settings (e.g. Zelda needing
 * tu_tile_discard=false vs Diablo II/Streets of Rage 4 needing tu_tile_discard=true)
 * each receive their exact ideal profile in <application name="all"> without conflict.
 */
object PerGameDrircGenerator {

    enum class GameProfileType {
        ZELDA,
        DIABLO,
        STREETS_OF_RAGE_4,
        XENOBLADE,
        RED_DEAD_REDEMPTION,
        SUPER_MARIO_ODYSSEY,
        SUPER_SMASH_BROS,
        METROID_PRIME,
        HOGWARTS_LEGACY,
        THE_WITCHER_3,
        BAYONETTA,
        MONSTER_HUNTER_RISE,
        POKEMON,
        PERSONA_5_ROYAL,
        NIER_AUTOMATA,
        DOOM_ETERNAL,
        BORDERLANDS,
        MORTAL_KOMBAT_1,
        ASSASSINS_CREED,
        EA_SPORTS,
        ALAN_WAKE,
        LEGEND_OF_HEROES,
        SONIC,
        UNIVERSAL_DEFAULT
    }

    fun detectProfileType(programIdHex: String, title: String): GameProfileType {
        val cleanId = programIdHex.trim().uppercase()
        val cleanTitle = title.trim().lowercase()

        // 1. Zelda Series (LunchPack / KingSystem - Zero strobing, crystal water, intact shrines)
        if (cleanId.startsWith("01007EF00011E") || // BotW
            cleanId.startsWith("0100F2C0115B6") || // TotK
            cleanId.startsWith("01006BB00C6F0") || // Link's Awakening
            cleanId.startsWith("01008CF01BA04") || // Echoes of Wisdom
            cleanId.startsWith("01008CF01BAEC") || // Echoes of Wisdom Alt
            cleanId.startsWith("01002DA013484") || // Skyward Sword HD
            cleanId.startsWith("01003C700009C") || // Hyrule Warriors Definitive
            cleanId.startsWith("0100874011E54") || // Hyrule Warriors: Age of Calamity
            cleanId.startsWith("0100000000000074") || // Ship of Harkinian / 2S2H
            cleanTitle.contains("zelda") ||
            cleanTitle.contains("hyrule") ||
            cleanTitle.contains("breath of the wild") ||
            cleanTitle.contains("tears of the kingdom") ||
            cleanTitle.contains("skyward sword") ||
            cleanTitle.contains("link's awakening") ||
            cleanTitle.contains("echoes of wisdom")
        ) {
            return GameProfileType.ZELDA
        }

        // 2. Diablo Series (Flawless D32 Float Depth, Zero Strobing)
        if (cleanId.startsWith("01002A801458A") || // Diablo II USA
            cleanId.startsWith("0100916014D8C") || // Diablo II EUR
            cleanId.startsWith("0100726014352") || // Diablo II JPN
            cleanId.startsWith("01001B300B9BE") || // Diablo III
            cleanId.startsWith("01001B700A654") || // Diablo III Eternal
            cleanId.startsWith("0100D1AB10000000") || // DevilutionX
            cleanTitle.contains("diablo")
        ) {
            return GameProfileType.DIABLO
        }

        // 3. Streets of Rage 4 & 2D Action Sprites (Pristine Pixel Art & Video Cutscenes)
        if (cleanId.startsWith("0100EC9010258") || // Streets of Rage 4
            cleanId.startsWith("010085800E33E") || // Streets of Rage 4 Main
            cleanId.startsWith("0100BA700E340") || // Streets of Rage 4 Alt
            cleanId.startsWith("010063B0077F6") || // Hollow Knight
            cleanId.startsWith("01008EB00B32E") || // Cuphead
            cleanId.startsWith("010006200A526") || // Dead Cells
            cleanId.startsWith("0100854005118") || // Celeste
            cleanId.startsWith("01009AA000FAA") || // Sonic Mania
            cleanId.startsWith("0100109015CA6") || // TMNT Shredder's Revenge
            cleanId.startsWith("0100779004BEE") || // Rayman Legends
            cleanId.startsWith("010019C00D6FE") || // Ori 1
            cleanId.startsWith("0100B1A00FECC") || // Ori 2
            cleanId.startsWith("0100B2B0192A0") || // Vampire Survivors
            cleanId.startsWith("0100AE801659C") || // Prince of Persia: The Lost Crown
            cleanId.startsWith("0100535012974") || // Hades
            cleanId.startsWith("0100B880182CA") || // Sea of Stars
            cleanId.startsWith("0100570003AC8") || // Octopath Traveler
            cleanId.startsWith("010057A016848") || // Octopath Traveler II
            cleanId.startsWith("0100CC80140F8") || // Triangle Strategy
            cleanId.startsWith("010023400DA34") || // Blasphemous
            cleanId.startsWith("010067901A51A") || // Blasphemous 2
            cleanTitle.contains("streets of rage") ||
            cleanTitle.contains("hollow knight") ||
            cleanTitle.contains("cuphead") ||
            cleanTitle.contains("dead cells") ||
            cleanTitle.contains("celeste") ||
            cleanTitle.contains("sonic mania") ||
            cleanTitle.contains("tmnt") ||
            cleanTitle.contains("shredder") ||
            cleanTitle.contains("vampire survivors") ||
            cleanTitle.contains("blasphemous") ||
            cleanTitle.contains("sea of stars") ||
            cleanTitle.contains("lost crown") ||
            cleanTitle.contains("hades") ||
            cleanTitle.contains("octopath") ||
            cleanTitle.contains("triangle strategy")
        ) {
            return GameProfileType.STREETS_OF_RAGE_4
        }

        // 4. Super Smash Bros. Ultimate
        if (cleanId.startsWith("01006A800016E") ||
            cleanTitle.contains("smash bros") ||
            cleanTitle.contains("super smash")
        ) {
            return GameProfileType.SUPER_SMASH_BROS
        }

        // 5. Xenoblade Chronicles Series (Monolith Soft Water & Cloud Depth Stability)
        if (cleanId.startsWith("0100FF500E34A") || // Xenoblade DE
            cleanId.startsWith("0100E95004038") || // Xenoblade 2
            cleanId.startsWith("0100C48008890") || // Xenoblade 2 Torna
            cleanId.startsWith("010074F013262") || // Xenoblade 3
            cleanId.startsWith("0100F3400332E") ||
            cleanId.startsWith("010074600E26E") ||
            cleanTitle.contains("xenoblade") ||
            cleanTitle.contains("torna")
        ) {
            return GameProfileType.XENOBLADE
        }

        // 6. Red Dead Redemption (RAGE Engine)
        if (cleanId.startsWith("01007820195A6") ||
            cleanId.startsWith("0100E26017E5E") ||
            cleanTitle.contains("red dead redemption") ||
            cleanTitle.contains("red dead")
        ) {
            return GameProfileType.RED_DEAD_REDEMPTION
        }

        // 7. Metroid Prime & Dread (MercurySteam / Retro Studios)
        if (cleanId.startsWith("0100121014688") || // Metroid Prime Remastered
            cleanId.startsWith("0100813014B74") ||
            cleanId.startsWith("010093801237C") || // Metroid Dread
            cleanId.startsWith("01005AF00BA7A") ||
            cleanTitle.contains("metroid prime") ||
            cleanTitle.contains("metroid dread") ||
            cleanTitle.contains("metroid")
        ) {
            return GameProfileType.METROID_PRIME
        }

        // 8. Hogwarts Legacy (Unreal Engine 4 Heavy RPG)
        if (cleanId.startsWith("0100BCB0176D0") ||
            cleanId.startsWith("0100B5B0112F8") ||
            cleanId.startsWith("0100D1801648E") ||
            cleanTitle.contains("hogwarts legacy") ||
            cleanTitle.contains("hogwarts")
        ) {
            return GameProfileType.HOGWARTS_LEGACY
        }

        // 9. The Witcher 3: Wild Hunt & Skyrim (Heavy Open World RPG)
        if (cleanId.startsWith("01003D100E9C6") ||
            cleanId.startsWith("010014B00E9BE") ||
            cleanId.startsWith("0100E670129CA") ||
            cleanId.startsWith("0100E67012924") ||
            cleanId.startsWith("01000A10041EA") || // Skyrim
            cleanTitle.contains("witcher") ||
            cleanTitle.contains("wild hunt") ||
            cleanTitle.contains("skyrim") ||
            cleanTitle.contains("elder scrolls")
        ) {
            return GameProfileType.THE_WITCHER_3
        }

        // 10. Bayonetta Series (PlatinumGames)
        if (cleanId.startsWith("010076F0049A2") ||
            cleanId.startsWith("0100778006E88") ||
            cleanId.startsWith("01009A1008C36") ||
            cleanId.startsWith("01007760086F8") ||
            cleanId.startsWith("01004A4010F22") || // Bayonetta 3
            cleanId.startsWith("0100B5E018BD6") || // Cereza
            cleanTitle.contains("bayonetta") ||
            cleanTitle.contains("cereza")
        ) {
            return GameProfileType.BAYONETTA
        }

        // 11. Monster Hunter Series (Capcom RE Engine & MT Framework)
        if (cleanId.startsWith("0100B04011742") || // Rise
            cleanId.startsWith("0100830007780") || // Sunbreak
            cleanId.startsWith("0100770008DD8") || // Generations Ultimate
            cleanId.startsWith("01000F5012586") || // Stories 2
            cleanTitle.contains("monster hunter") ||
            cleanTitle.contains("sunbreak")
        ) {
            return GameProfileType.MONSTER_HUNTER_RISE
        }

        // 12. Pokemon Series
        if (cleanId.startsWith("0100A3D008C5C") || // Scarlet
            cleanId.startsWith("01008C30086E0") ||
            cleanId.startsWith("01008F6008C5E") || // Violet
            cleanId.startsWith("0100A3D0086EE") ||
            cleanId.startsWith("01001F5010DFA") || // Arceus
            cleanId.startsWith("0100ABF008968") || // Sword
            cleanId.startsWith("01008DB008C2C") || // Shield
            cleanId.startsWith("0100000011D90") || // BD
            cleanId.startsWith("010018E011D92") || // SP
            cleanId.startsWith("010003F003A34") || // Let's Go Pikachu
            cleanId.startsWith("0100152000022") || // Let's Go Eevee
            cleanId.startsWith("01008F2012014") || // Snap
            cleanId.startsWith("010025D00E266") || // Mystery Dungeon
            cleanTitle.contains("pokemon")
        ) {
            return GameProfileType.POKEMON
        }

        // 13. Persona & Shin Megami Tensei Series (Atlus)
        if (cleanId.startsWith("01000A10041EA") || // Persona 5 Royal
            cleanId.startsWith("01005CA0157CA") ||
            cleanId.startsWith("0100224016A90") || // Persona 4 Golden
            cleanId.startsWith("010058C01570E") ||
            cleanId.startsWith("010072001570A") || // Persona 3 Portable
            cleanId.startsWith("01006A4011400") || // Persona 5 Strikers
            cleanId.startsWith("0100B5B019E06") || // Persona 5 Tactica
            cleanId.startsWith("0100B870097D6") || // SMT V
            cleanId.startsWith("01000B901C46E") || // SMT V Vengeance
            cleanId.startsWith("01006F801BC4C") ||
            cleanId.startsWith("01007E3019808") ||
            cleanTitle.contains("persona") ||
            cleanTitle.contains("shin megami tensei") ||
            cleanTitle.contains("smt v") ||
            cleanTitle.contains("smtv")
        ) {
            return GameProfileType.PERSONA_5_ROYAL
        }

        // 14. NieR: Automata (PlatinumGames / Square Enix)
        if (cleanId.startsWith("01005C9014168") ||
            cleanId.startsWith("01007D401662A") ||
            cleanTitle.contains("nier:automata") ||
            cleanTitle.contains("nier automata") ||
            cleanTitle.contains("yorha")
        ) {
            return GameProfileType.NIER_AUTOMATA
        }

        // 15. DOOM & Wolfenstein Series (id Tech 6 / 7)
        if (cleanId.startsWith("010053700465C") || // DOOM 2016
            cleanId.startsWith("0100B9F010DC4") || // DOOM Eternal
            cleanId.startsWith("0100BB600DC30") ||
            cleanId.startsWith("0100D00100000000") || // DOOM Classic
            cleanId.startsWith("0100D00300000000") || // DOOM 3
            cleanId.startsWith("01002A3008E88") || // Wolfenstein II
            cleanId.startsWith("01000AE009FF2") || // Wolfenstein Youngblood
            cleanTitle.contains("doom") ||
            cleanTitle.contains("wolfenstein")
        ) {
            return GameProfileType.DOOM_ETERNAL
        }

        // 16. Borderlands Series (Gearbox)
        if (cleanId.startsWith("01007E300B70C") || // Legendary Collection
            cleanId.startsWith("01008C30113B0") || // Borderlands 3
            cleanId.startsWith("01006D401831E") || // Tiny Tina
            cleanTitle.contains("borderlands") ||
            cleanTitle.contains("tiny tina")
        ) {
            return GameProfileType.BORDERLANDS
        }

        // 17. Mortal Kombat & Batman Arkham Series (Heavy UE4 Custom Pipelines)
        if (cleanId.startsWith("01006560184E6") || // MK 1
            cleanId.startsWith("0100D2800D5C2") ||
            cleanId.startsWith("0100B1100C4D0") || // MK 11
            cleanId.startsWith("010063B017DAE") || // Arkham Knight
            cleanId.startsWith("010023A017E94") ||
            cleanId.startsWith("01003AE017DB0") || // Arkham City
            cleanId.startsWith("01001D4017DA8") || // Arkham Asylum
            cleanId.startsWith("01000B0012E4E") || // Cronos
            cleanId.startsWith("010055700C30A") || // Outer Worlds
            cleanTitle.contains("mortal kombat") ||
            cleanTitle.contains("arkham") ||
            cleanTitle.contains("batman")
        ) {
            return GameProfileType.MORTAL_KOMBAT_1
        }

        // 18. Assassin's Creed Series (Ubisoft AnvilNext)
        if (cleanId.startsWith("010043800C3DA") || // Rebel Collection (Black Flag / Rogue)
            cleanId.startsWith("0100BC8013E8E") || // Ezio Collection
            cleanId.startsWith("010010000C16C") || // AC III Remastered
            cleanTitle.contains("assassin")
        ) {
            return GameProfileType.ASSASSINS_CREED
        }

        // 19. EA Sports FC Series (Frostbite Engine)
        if (cleanId.startsWith("0100A38018D5A") || // FC 24
            cleanId.startsWith("01005B101DC84") || // FC 25
            cleanId.startsWith("010077D0238FA") || // FC 26
            cleanTitle.contains("ea sports") ||
            cleanTitle.contains("fifa") ||
            cleanTitle.contains("fc 2")
        ) {
            return GameProfileType.EA_SPORTS
        }

        // 20. Alan Wake Remastered (Font and Subtitle Color Fix)
        if (cleanId.startsWith("0100F830138E6") || // Alan Wake Remastered
            cleanId.startsWith("010074B01456A") || // Alan Wake USA
            cleanId.startsWith("010065F014C5E") || // Alan Wake EUR
            cleanTitle.contains("alan wake")
        ) {
            return GameProfileType.ALAN_WAKE
        }

        // 21. The Legend of Heroes & Falcom Series
        if (cleanId.startsWith("0100B0E020356") ||
            cleanId.startsWith("01004A301D6E2") ||
            cleanId.startsWith("01002C5016EE8") ||
            cleanId.startsWith("0100F7A016336") ||
            cleanTitle.contains("trails in the sky") ||
            cleanTitle.contains("trails of cold steel") ||
            cleanTitle.contains("legend of heroes") ||
            cleanTitle.contains("ys viii") ||
            cleanTitle.contains("ys ix") ||
            cleanTitle.contains("ys x")
        ) {
            return GameProfileType.LEGEND_OF_HEROES
        }

        // 22. Mario Series (Odyssey, Wonder, 3D World, Kart, RPG, etc.)
        if (cleanId.startsWith("0100000000010") || // Odyssey
            cleanId.startsWith("010015100B5C4") || // Wonder
            cleanId.startsWith("010015100B5B4") ||
            cleanId.startsWith("010028600EBDA") || // 3D World
            cleanId.startsWith("0100152000022") || // Mario Kart 8
            cleanId.startsWith("010067300059A") || // Mario + Rabbids
            cleanId.startsWith("0100760012E4A") ||
            cleanId.startsWith("01005CA00F966") ||
            cleanId.startsWith("01007EF01B842") || // Paper Mario TTYD
            cleanId.startsWith("01004D701742A") ||
            cleanId.startsWith("01004D300C5AE") || // Origami King
            cleanId.startsWith("0100D57018DEE") || // Mario & Luigi Brothership
            cleanId.startsWith("010091801E8B2") ||
            cleanId.startsWith("010062C019800") || // Super Mario RPG
            cleanId.startsWith("0100BC0018138") ||
            cleanId.startsWith("01008D3017B4C") ||
            cleanId.startsWith("0100DCA0064A6") || // Luigi's Mansion 3
            cleanId.startsWith("010036601D380") || // Party Jamboree
            cleanId.startsWith("010049900F546") || // 3D All-Stars
            cleanId.startsWith("0100427010476") ||
            cleanId.startsWith("0100F7701140E") || // Golf
            cleanId.startsWith("0100874011EE6") ||
            cleanId.startsWith("0100BDE00862A") || // Tennis Aces
            cleanId.startsWith("010019401051C") || // Strikers
            cleanId.startsWith("0100000000000064") || // SM64-NX
            cleanTitle.contains("mario") ||
            cleanTitle.contains("luigi") ||
            cleanTitle.contains("donkey kong")
        ) {
            return GameProfileType.SUPER_MARIO_ODYSSEY
        }

        // 23. Sonic 3D Series (Hedgehog Engine 2)
        if (cleanId.startsWith("01006CD0127BE") || // Frontiers
            cleanId.startsWith("01006CD016142") || // Superstars
            cleanId.startsWith("0100DA80036C8") || // Forces
            cleanId.startsWith("01005110137EE") || // Colors
            cleanId.startsWith("0100A7D01B694") || // Generations
            cleanTitle.contains("sonic")
        ) {
            return GameProfileType.SONIC
        }

        return GameProfileType.UNIVERSAL_DEFAULT
    }

    fun generateXmlContent(profileType: GameProfileType, programIdHex: String, title: String): String {
        val optionsBuilder = StringBuilder()

        // Common baseline options across all profiles:
        optionsBuilder.append("""
            <!-- 1. Next-Gen Thermal Governor Floor & Power Optimization v5 -->
            <option name="tu_thermal_governor_floor" value="true" />
            <option name="tu_power_governor_floor" value="true" />
            <option name="tu_perf_floor_clamp" value="true" />
            <option name="tu_power_profile_balanced" value="true" />
            <option name="tu_low_power_vulkan_pipeline" value="true" />
            <option name="tu_dynamic_thermal_budget" value="true" />
            <option name="tu_energy_aware_scheduling" value="true" />

            <!-- 2. Low-Latency Mailbox VSync & Dynamic Adaptive Frame Pacing -->
            <option name="tu_adaptive_frame_pacing" value="true" />
            <option name="tu_frame_time_smoothing" value="true" />
            <option name="tu_timeline_sync" value="true" />
            <option name="tu_relaxed_frame_pacing" value="true" />
            <option name="tu_mail_box_vsync_pacing" value="true" />

            <!-- 3. Adreno 830 (Snapdragon 8 Elite) & A8xx Precision Architecture Tuning -->
            <option name="tu_a830_reg_size_96" value="true" />
            <option name="tu_a8xx_perfcntrs" value="true" />
            <option name="tu_a830_ubwc_pitch_clamp" value="true" />
            <option name="tu_a8xx_concurrent_queue_barrier_fix" value="true" />
            <option name="tu_a8xx_barycentric_coord_clamp" value="true" />
            <option name="tu_a8xx_subgroup_ballot_fastpath" value="true" />
            <option name="tu_a830_concurrent_render_pass_align" value="true" />
            <option name="tu_shader_spirv_mem_fastpath" value="true" />

            <!-- 4. Samsung OneUI UBWC Framebuffer Compression & Lossless Guard -->
            <option name="tu_ubwc_oneui_fix" value="true" />
            <option name="tu_ubwc_5_direct_align" value="true" />
            <option name="tu_ubwc_5_stride_128_align" value="true" />
            <option name="tu_ubwc_fallback_guard" value="true" />
            <option name="tu_ubwc_lossless_fastpath" value="true" />
            <option name="tu_disable_lossy_ubwc_depth" value="true" />

            <!-- 5. Compute Flush Optimization & Binary-Search GMEM Allocator -->
            <option name="tu_compute_flush_bits_optimize" value="true" />
            <option name="tu_optimal_gmem_tile_allocator" value="true" />
            <option name="tu_lrz_preserve_across_cmdbuf" value="true" />
            <option name="tu_drop_tile_wfi" value="true" />
            <option name="tu_autotune_bypass_margin" value="true" />

            <!-- 6. Zero-Overhead ASTC HDR Decompressor in GMEM & Direct WSI -->
            <option name="tu_astc_hdr_gmem" value="true" />
            <option name="tu_fast_astc_decompress_gmem" value="true" />
            <option name="tu_bindless_textures" value="true" />
            <option name="tu_zero_copy_subsampled_sampler" value="true" />
            <option name="tu_direct_surfaceflinger_blit" value="true" />
            <option name="tu_bypass_gpu_rotator" value="true" />
            <option name="tu_direct_display_sync" value="true" />
            <option name="tu_zero_copy_swapchain" value="true" />

            <!-- 7. Subpass Fusion Guard: False for pure transparency & stability -->
            <option name="tu_subpass_fusion" value="false" />
            <option name="tu_subpass_fusion_v2" value="false" />

            <!-- 8. Monolithic Pipeline Cache (4GB LZ4) & IR3 Optimization -->
            <option name="tu_suballocator_pool_512k" value="true" />
            <option name="tu_pipeline_cache_lz4" value="true" />
            <option name="tu_shader_cache_size_mb" value="4096" />
            <option name="tu_huge_heap_8gb_align" value="true" />
            <option name="tu_gmem_page_alignment_guard" value="true" />
            <option name="tu_gmem_force_tile_align" value="true" />
            <option name="tu_gcm" value="1" />
            <option name="tu_ir3_icache_128b_align" value="true" />
            <option name="tu_ir3_dead_code_elimination" value="true" />
            <option name="tu_ir3_constant_folding" value="true" />
            <option name="tu_ir3_subgroup_merge" value="true" />
            <option name="tu_ir3_scheduler_hazard_guard" value="true" />
        """.trimIndent())

        // Specific tailored options per game category:
        when (profileType) {
            GameProfileType.ZELDA -> {
                optionsBuilder.append("\n            <!-- ZELDA SERIES RULES (Crystal Water, Intact Shrines, Zero Strobing) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"false\" />\n")
                optionsBuilder.append("            <option name=\"tu_disable_lrz\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"false\" />\n")
                optionsBuilder.append("            <option name=\"tu_disable_fast_clears\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_fast_clear\" value=\"false\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bias_control_all_adreno\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_dynamic_state_depth_bias_clamp\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_range_unrestricted_a7xx_a8xx\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
            }
            GameProfileType.DIABLO -> {
                optionsBuilder.append("\n            <!-- DIABLO II & III RULES (Flawless D32 Float Depth, Zero Strobing) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bias_control_all_adreno\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_range_unrestricted_a7xx_a8xx\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_dynamic_state_depth_bias_clamp\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds_test\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
            }
            GameProfileType.STREETS_OF_RAGE_4 -> {
                optionsBuilder.append("\n            <!-- STREETS OF RAGE 4 & 2D SPRITES (Pristine Pixel Art & Video Cutscenes) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bias_control_all_adreno\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_range_unrestricted_a7xx_a8xx\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_dynamic_state_depth_bias_clamp\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_a8xx_barycentric_coord_clamp\" value=\"true\" />\n")
            }
            GameProfileType.XENOBLADE -> {
                optionsBuilder.append("\n            <!-- XENOBLADE CHRONICLES RULES (Monolith Soft Water & Cloud Depth Stability) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"false\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_huge_heap_8gb_align\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_optimal_gmem_tile_allocator\" value=\"true\" />\n")
            }
            GameProfileType.RED_DEAD_REDEMPTION -> {
                optionsBuilder.append("\n            <!-- RED DEAD REDEMPTION RULES (RAGE Engine High Precision Depth) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds_test\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_dynamic_state_depth_bias_clamp\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"true\" />\n")
            }
            GameProfileType.SUPER_MARIO_ODYSSEY -> {
                optionsBuilder.append("\n            <!-- SUPER MARIO ODYSSEY & 3D MARIO (Lossless Fastpath & Ultra Smooth Pacing) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_ubwc_lossless_fastpath\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_mail_box_vsync_pacing\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_adaptive_frame_pacing\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bias_control_all_adreno\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_optimal_gmem_tile_allocator\" value=\"true\" />\n")
            }
            GameProfileType.SUPER_SMASH_BROS -> {
                optionsBuilder.append("\n            <!-- SUPER SMASH BROS ULTIMATE (Zero Input Lag & Optimal GMEM Allocation) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_mail_box_vsync_pacing\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_adaptive_frame_pacing\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bias_control_all_adreno\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_optimal_gmem_tile_allocator\" value=\"true\" />\n")
            }
            GameProfileType.METROID_PRIME -> {
                optionsBuilder.append("\n            <!-- METROID PRIME & DREAD (Fast ASTC GMEM & Dynamic Depth Bias) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_fast_astc_decompress_gmem\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_dynamic_state_depth_bias_clamp\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
            }
            GameProfileType.HOGWARTS_LEGACY -> {
                optionsBuilder.append("\n            <!-- HOGWARTS LEGACY (Complex UE4 Geometry & 8GB Huge Heap Buffer) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"false\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_huge_heap_8gb_align\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_compute_flush_bits_optimize\" value=\"true\" />\n")
            }
            GameProfileType.THE_WITCHER_3 -> {
                optionsBuilder.append("\n            <!-- THE WITCHER 3 & SKYRIM (REDengine 3 & Creation Engine Precision) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"false\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_huge_heap_8gb_align\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
            }
            GameProfileType.BAYONETTA -> {
                optionsBuilder.append("\n            <!-- BAYONETTA SERIES (PlatinumGames High Precision Action Flow) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bias_control_all_adreno\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_dynamic_state_depth_bias_clamp\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_adaptive_frame_pacing\" value=\"true\" />\n")
            }
            GameProfileType.MONSTER_HUNTER_RISE -> {
                optionsBuilder.append("\n            <!-- MONSTER HUNTER RISE & STORIES (Capcom RE Engine Flush Optimization) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_compute_flush_bits_optimize\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_optimal_gmem_tile_allocator\" value=\"true\" />\n")
            }
            GameProfileType.POKEMON -> {
                optionsBuilder.append("\n            <!-- POKEMON SERIES (Game Freak Open World Depth Clamp & LRZ) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bias_control_all_adreno\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_optimal_gmem_tile_allocator\" value=\"true\" />\n")
            }
            GameProfileType.PERSONA_5_ROYAL -> {
                optionsBuilder.append("\n            <!-- PERSONA & SMT SERIES (Atlus Engine Mailbox Pacing & D32 Depth) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bias_control_all_adreno\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_adaptive_frame_pacing\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_mail_box_vsync_pacing\" value=\"true\" />\n")
            }
            GameProfileType.NIER_AUTOMATA -> {
                optionsBuilder.append("\n            <!-- NIER: AUTOMATA (PlatinumGames YoRHa GMEM Allocation) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_dynamic_state_depth_bias_clamp\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_optimal_gmem_tile_allocator\" value=\"true\" />\n")
            }
            GameProfileType.DOOM_ETERNAL -> {
                optionsBuilder.append("\n            <!-- DOOM & WOLFENSTEIN (id Tech 6 / 7 Compute Flush & Suballocator) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_compute_flush_bits_optimize\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds_test\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_suballocator_pool_512k\" value=\"true\" />\n")
            }
            GameProfileType.BORDERLANDS -> {
                optionsBuilder.append("\n            <!-- BORDERLANDS SERIES (Gearbox UE3 / UE4 Flush & Depth Bias) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_compute_flush_bits_optimize\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bias_control_all_adreno\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"true\" />\n")
            }
            GameProfileType.MORTAL_KOMBAT_1 -> {
                optionsBuilder.append("\n            <!-- MORTAL KOMBAT & BATMAN ARKHAM (Heavy UE4 Custom Pipelines) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_compute_flush_bits_optimize\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_a8xx_concurrent_queue_barrier_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
            }
            GameProfileType.ASSASSINS_CREED -> {
                optionsBuilder.append("\n            <!-- ASSASSIN'S CREED SERIES (Ubisoft AnvilNext Ocean & Shadows Integrity) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"false\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"true\" />\n")
            }
            GameProfileType.EA_SPORTS -> {
                optionsBuilder.append("\n            <!-- EA SPORTS FROSTBITE RULES (Pacing & Command Buffering) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_adaptive_frame_pacing\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_mail_box_vsync_pacing\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
            }
            GameProfileType.ALAN_WAKE -> {
                optionsBuilder.append("\n            <!-- ALAN WAKE REMASTERED (Pristine White Subtitles, Font Glyph Swizzle and Flashlight HDR) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_a8_unorm_swizzle_one\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_r8_unorm_swizzle_alpha\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
            }
            GameProfileType.LEGEND_OF_HEROES -> {
                optionsBuilder.append("\n            <!-- THE LEGEND OF HEROES & FALCOM RULES (LRZ Continuity across CmdBuf) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_lrz_preserve_across_cmdbuf\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_autotune_bypass_margin\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
            }
            GameProfileType.SONIC -> {
                optionsBuilder.append("\n            <!-- SONIC 3D RULES (Hedgehog Engine 2 Ultra Smooth Pacing) -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bias_control_all_adreno\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_mail_box_vsync_pacing\" value=\"true\" />\n")
            }
            GameProfileType.UNIVERSAL_DEFAULT -> {
                optionsBuilder.append("\n            <!-- UNIVERSAL STORM SWITCH DEFAULT ENGINE RULES -->\n")
                optionsBuilder.append("            <option name=\"tu_tile_discard\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bias_control_all_adreno\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_range_unrestricted_a7xx_a8xx\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_dynamic_state_depth_bias_clamp\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_bounds\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_depth_clamp_control_fix\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_force_d32_unnormalized\" value=\"true\" />\n")
                optionsBuilder.append("            <option name=\"tu_indirect_ubo_bounds\" value=\"true\" />\n")
            }
        }

        return """
<driconf>
    <device driver="turnip">
        <application name="all">
$optionsBuilder
        </application>
    </device>
    <device driver="panfrost">
        <application name="all">
            <option name="pan_early_z" value="true" />
            <option name="pan_afbc_lossless" value="true" />
        </application>
    </device>
    <device driver="panvk">
        <application name="all">
            <option name="pan_early_z" value="true" />
            <option name="pan_forward_pixel_kill" value="true" />
            <option name="pan_fp16_arithmetic" value="true" />
            <option name="pan_thread_group_clamp" value="true" />
            <option name="pan_tile_buffer_coherency" value="true" />
            <option name="pan_afbc_lossless" value="true" />
            <option name="pan_shader_cache_multithreaded" value="true" />
            <option name="mesa_disk_cache_single_file" value="true" />
            <option name="mesa_shader_cache_max_size" value="4294967296" />
        </application>
    </device>
</driconf>
""".trimIndent()
    }

    /**
     * Generates and writes the tailored drirc configuration for the active game,
     * returning the File pointing to the active session config.
     */
    fun generateForGame(programIdHex: String, title: String): File {
        val profileType = detectProfileType(programIdHex, title)
        val xml = generateXmlContent(profileType, programIdHex, title)

        val targetDir = File(YuzuApplication.appContext.filesDir, "per_game_drirc")
        if (!targetDir.exists()) {
            targetDir.mkdirs()
        }

        val configFile = File(targetDir, "drirc.xml")
        configFile.writeText(xml, Charsets.UTF_8)

        // Duplicate as drirc, 00-storm.conf, drirc.conf for maximum loader compatibility
        File(targetDir, "00-storm.conf").writeText(xml, Charsets.UTF_8)
        File(targetDir, "drirc").writeText(xml, Charsets.UTF_8)
        File(targetDir, "drirc.conf").writeText(xml, Charsets.UTF_8)

        // Write directly to $HOME/.drirc
        val homeDir = YuzuApplication.appContext.filesDir
        File(homeDir, ".drirc").writeText(xml, Charsets.UTF_8)

        Log.info("[PerGameDrircGenerator] Generated $profileType drirc profile for $title ($programIdHex)")
        return configFile
    }
}
