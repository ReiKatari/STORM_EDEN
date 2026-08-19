package org.yuzu.yuzu_emu.overlay.model

import org.yuzu.yuzu_emu.R

enum class OverlayRenderStyle {
    NEON_GLOW,
    SWITCH_JOYCON,
    FROST_GLASS_3D,
    TITANIUM_MECHA,
    ARCADE_CANDY_3D,
    FLAT_MINIMAL,
    RETRO_CLASSIC,
    GOLDEN_ORNATE
}

enum class OverlayTheme(
    val id: Int,
    val titleResId: Int,
    val colorDefault: Int,
    val colorPressed: Int,
    val renderStyle: OverlayRenderStyle
) {
    CYBERPUNK_NEON(0, R.string.skin_cyberpunk_neon, 0xFF00F2FE.toInt(), 0xFFFF007F.toInt(), OverlayRenderStyle.NEON_GLOW),
    CLASSIC_SWITCH(1, R.string.skin_classic_switch, 0xFF00C3E3.toInt(), 0xFFFF3C28.toInt(), OverlayRenderStyle.SWITCH_JOYCON),
    FROST_GLASS_3D(2, R.string.skin_frost_glass_3d, 0xFFE0F2FE.toInt(), 0xFF38BDF8.toInt(), OverlayRenderStyle.FROST_GLASS_3D),
    TITANIUM_MECHA(3, R.string.skin_titanium_mecha, 0xFF94A3B8.toInt(), 0xFF00E5FF.toInt(), OverlayRenderStyle.TITANIUM_MECHA),
    ARCADE_CANDY_3D(4, R.string.skin_arcade_candy_3d, 0xFFF43F5E.toInt(), 0xFFFBBF24.toInt(), OverlayRenderStyle.ARCADE_CANDY_3D),
    FLAT_MINIMAL(5, R.string.skin_flat_minimal, 0xFFF1F5F9.toInt(), 0xFF00E5FF.toInt(), OverlayRenderStyle.FLAT_MINIMAL),
    STORM_DS_CLASSIC(6, R.string.skin_storm_ds_classic, 0xFFE2E8F0.toInt(), 0xFF00E5FF.toInt(), OverlayRenderStyle.TITANIUM_MECHA),
    DS_LITE_WHITE(7, R.string.skin_ds_lite_white, 0xFFF8FAFC.toInt(), 0xFF38BDF8.toInt(), OverlayRenderStyle.FROST_GLASS_3D),
    DSI_XL_BLUE(8, R.string.skin_dsi_xl_blue, 0xFF2563EB.toInt(), 0xFF67E8F9.toInt(), OverlayRenderStyle.TITANIUM_MECHA),
    CRIMSON_RED(9, R.string.skin_crimson_red, 0xFFEF4444.toInt(), 0xFFF59E0B.toInt(), OverlayRenderStyle.ARCADE_CANDY_3D),
    N3DS_AQUA(10, R.string.skin_n3ds_aqua, 0xFF06B6D4.toInt(), 0xFF10B981.toInt(), OverlayRenderStyle.NEON_GLOW),
    OLED_STEALTH(11, R.string.skin_oled_stealth, 0xFF64748B.toInt(), 0xFFFFFFFF.toInt(), OverlayRenderStyle.FLAT_MINIMAL),
    GOLDEN_ZELDA(12, R.string.skin_golden_zelda, 0xFFF59E0B.toInt(), 0xFFFB923C.toInt(), OverlayRenderStyle.GOLDEN_ORNATE),
    GAMEBOY_COLOR_PURPLE(13, R.string.skin_gameboy_color_purple, 0xFFA855F7.toInt(), 0xFFEC4899.toInt(), OverlayRenderStyle.FROST_GLASS_3D),
    GAMEBOY_DMG_RETRO(14, R.string.skin_gameboy_dmg_retro, 0xFF84CC16.toInt(), 0xFF991B1B.toInt(), OverlayRenderStyle.RETRO_CLASSIC);

    companion object {
        fun fromId(id: Int): OverlayTheme = values().firstOrNull { it.id == id } ?: CYBERPUNK_NEON
    }
}
