// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.content.Context
import android.content.res.ColorStateList
import android.content.res.Configuration
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.os.Bundle
import android.view.Gravity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.DialogFragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogTouchOverlayStyleBinding
import org.yuzu.yuzu_emu.databinding.ItemTouchOverlayThemeBinding
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.overlay.model.OverlayRenderStyle
import org.yuzu.yuzu_emu.overlay.model.OverlayTheme
import org.yuzu.yuzu_emu.utils.NativeConfig

class TouchOverlayStyleDialogFragment : DialogFragment() {

    private var _binding: DialogTouchOverlayStyleBinding? = null
    private val binding get() = _binding!!

    private var onThemeChangedCallback: (() -> Unit)? = null

    companion object {
        const val TAG = "TouchOverlayStyleDialogFragment"

        fun newInstance(onThemeChanged: (() -> Unit)? = null): TouchOverlayStyleDialogFragment {
            return TouchOverlayStyleDialogFragment().apply {
                this.onThemeChangedCallback = onThemeChanged
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(STYLE_NO_TITLE, 0)
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val dialog = super.onCreateDialog(savedInstanceState)
        dialog.requestWindowFeature(android.view.Window.FEATURE_NO_TITLE)
        dialog.window?.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        return dialog
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = DialogTouchOverlayStyleBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.btnClose.setOnClickListener {
            dismiss()
        }

        val themes = OverlayTheme.values().toList()
        val currentThemeId = IntSetting.OVERLAY_SKIN_THEME.getInt()

        val adapter = ThemeAdapter(requireContext(), themes, currentThemeId) { selectedTheme ->
            IntSetting.OVERLAY_SKIN_THEME.setInt(selectedTheme.id)
            NativeConfig.saveGlobalConfig()
            onThemeChangedCallback?.invoke()
            dismiss()
        }

        binding.listOverlayThemes.layoutManager = LinearLayoutManager(requireContext())
        binding.listOverlayThemes.adapter = adapter

        val initialPosition = themes.indexOfFirst { it.id == currentThemeId }.coerceAtLeast(0)
        binding.listOverlayThemes.scrollToPosition(initialPosition)
    }

    override fun onStart() {
        super.onStart()
        dialog?.window?.let { window ->
            val dm = resources.displayMetrics
            val isLandscape = resources.configuration.orientation == Configuration.ORIENTATION_LANDSCAPE
            val isInGame = activity is org.yuzu.yuzu_emu.activities.EmulationActivity
            val width = if (isInGame || isLandscape) {
                (dm.widthPixels * 0.85).toInt().coerceIn(480, 850)
            } else {
                (dm.widthPixels * 0.94).toInt()
            }
            val height = if (isInGame || isLandscape) {
                (dm.heightPixels * 0.88).toInt().coerceIn(360, 800)
            } else {
                (dm.heightPixels * 0.75).toInt()
            }
            window.setLayout(width, height)
            window.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
            window.setGravity(Gravity.CENTER)
            val lp = window.attributes
            lp.width = width
            lp.height = height
            lp.gravity = Gravity.CENTER
            window.attributes = lp
        }
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private class ThemeAdapter(
        private val context: Context,
        private val themes: List<OverlayTheme>,
        private var selectedId: Int,
        private val onSelected: (OverlayTheme) -> Unit
    ) : RecyclerView.Adapter<ThemeAdapter.ViewHolder>() {

        class ViewHolder(val binding: ItemTouchOverlayThemeBinding) : RecyclerView.ViewHolder(binding.root)

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val binding = ItemTouchOverlayThemeBinding.inflate(
                LayoutInflater.from(parent.context),
                parent,
                false
            )
            return ViewHolder(binding)
        }

        override fun getItemCount(): Int = themes.size

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            val theme = themes[position]
            val isSelected = (theme.id == selectedId)

            holder.binding.textThemeTitle.text = context.getString(theme.titleResId)
            holder.binding.textThemeStyle.text = getStyleDescription(theme)

            // Volumetric dual-color preview
            holder.binding.viewColorPreviewBg.backgroundTintList = ColorStateList.valueOf(theme.colorDefault)
            holder.binding.viewColorPressedDot.backgroundTintList = ColorStateList.valueOf(theme.colorPressed)

            val activeColor = Color.parseColor("#00F0FF")
            val defaultOutline = Color.parseColor("#374151")

            holder.binding.cardTheme.strokeColor = if (isSelected) activeColor else defaultOutline
            holder.binding.cardTheme.strokeWidth = if (isSelected) 5 else 2
            holder.binding.cardTheme.cardElevation = if (isSelected) 8f else 2f
            holder.binding.radioThemeSelected.isChecked = isSelected

            holder.binding.cardTheme.setOnClickListener {
                selectedId = theme.id
                notifyDataSetChanged()
                onSelected(theme)
            }
        }

        private fun getStyleDescription(theme: OverlayTheme): String {
            return when (theme.renderStyle) {
                OverlayRenderStyle.NEON_GLOW -> "Неоновое кибер-свечение (3D)"
                OverlayRenderStyle.SWITCH_JOYCON -> "Оригинальный стиль Joy-Con (3D)"
                OverlayRenderStyle.FROST_GLASS_3D -> "Матовое стекло Frost Glass (3D)"
                OverlayRenderStyle.TITANIUM_MECHA -> "Титан и кибернетика (3D)"
                OverlayRenderStyle.ARCADE_CANDY_3D -> "Объемные аркадные кнопки (3D)"
                OverlayRenderStyle.FLAT_MINIMAL -> "Минималистичный контур (3D)"
                OverlayRenderStyle.RETRO_CLASSIC -> "Ретро классика Nintendo (3D)"
                OverlayRenderStyle.GOLDEN_ORNATE -> "Королевское золото (3D)"
            }
        }
    }
}
