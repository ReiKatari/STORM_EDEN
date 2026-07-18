// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.view.LayoutInflater
import android.view.ViewGroup
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.ListItemAddonBinding
import org.yuzu.yuzu_emu.model.Patch
import org.yuzu.yuzu_emu.model.PatchType
import org.yuzu.yuzu_emu.model.AddonViewModel
import org.yuzu.yuzu_emu.viewholder.AbstractViewHolder

class AddonAdapter(val addonViewModel: AddonViewModel) :
    AbstractDiffAdapter<Patch, AddonAdapter.AddonViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AddonViewHolder {
        ListItemAddonBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return AddonViewHolder(it) }
    }

    inner class AddonViewHolder(val binding: ListItemAddonBinding) :
        AbstractViewHolder<Patch>(binding) {
        override fun bind(model: Patch) {
            val isActive = model.source != Patch.SOURCE_UNKNOWN
            binding.addonSwitch.isEnabled = isActive
            binding.addonCard.isEnabled = isActive

            binding.addonCard.setOnClickListener {
                if (isActive) {
                    binding.addonSwitch.performClick()
                }
            }

            binding.addonCard.setOnLongClickListener {
                val clipBoard = it.context.getSystemService(android.content.Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager
                val clip = android.content.ClipData.newPlainText(model.name, model.name)
                clipBoard.setPrimaryClip(clip)
                android.widget.Toast.makeText(
                    it.context,
                    R.string.copied_to_clipboard,
                    android.widget.Toast.LENGTH_SHORT
                ).show()
                true
            }

            binding.title.text = model.name
            binding.version.text = model.version
            binding.addonSwitch.isChecked = model.enabled

            binding.addonSwitch.setOnCheckedChangeListener { _, checked ->
                if (isActive) {
                    if (PatchType.from(model.type) == PatchType.Update && checked) {
                        addonViewModel.enableOnlyThisUpdate(model)
                        notifyDataSetChanged()
                    } else {
                        model.enabled = checked
                    }
                }
            }

            val canDelete = model.isRemovable && isActive
            binding.deleteCard.isEnabled = canDelete
            binding.buttonDelete.isEnabled = canDelete
            binding.deleteCard.alpha = if (canDelete) 1f else 0.38f

            if (canDelete) {
                val deleteAction = {
                    addonViewModel.setAddonToDelete(model)
                }
                binding.deleteCard.setOnClickListener { deleteAction() }
                binding.buttonDelete.setOnClickListener { deleteAction() }
            } else {
                binding.deleteCard.setOnClickListener(null)
                binding.buttonDelete.setOnClickListener(null)
            }
        }
    }
}
