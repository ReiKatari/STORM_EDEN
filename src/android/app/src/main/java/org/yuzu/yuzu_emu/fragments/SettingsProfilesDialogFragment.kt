// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.graphics.Color
import android.graphics.drawable.ColorDrawable
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.DialogFragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogSettingsProfilesBinding
import org.yuzu.yuzu_emu.databinding.ItemSettingsProfileBinding
import org.yuzu.yuzu_emu.features.settings.utils.SettingsProfile
import org.yuzu.yuzu_emu.features.settings.utils.SettingsProfileManager

class SettingsProfilesDialogFragment : DialogFragment() {

    private var _binding: DialogSettingsProfilesBinding? = null
    private val binding get() = _binding!!

    private var profilesList = mutableListOf<SettingsProfile>()
    private var adapter: ProfilesAdapter? = null

    private val importProfileLauncher = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) {
            try {
                requireContext().contentResolver.openInputStream(uri)?.use { stream ->
                    val imported = SettingsProfileManager.importProfileFromStream(stream)
                    if (imported != null) {
                        Toast.makeText(requireContext(), getString(R.string.profile_import_success, imported.name), Toast.LENGTH_SHORT).show()
                        refreshProfiles()
                    } else {
                        Toast.makeText(requireContext(), getString(R.string.profile_import_failed), Toast.LENGTH_SHORT).show()
                    }
                }
            } catch (e: Exception) {
                Toast.makeText(requireContext(), getString(R.string.profile_import_failed), Toast.LENGTH_SHORT).show()
            }
        }
    }

    companion object {
        const val TAG = "SettingsProfilesDialogFragment"

        fun newInstance(): SettingsProfilesDialogFragment {
            return SettingsProfilesDialogFragment()
        }
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val dialog = super.onCreateDialog(savedInstanceState)
        dialog.window?.setBackgroundDrawable(ColorDrawable(Color.TRANSPARENT))
        return dialog
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = DialogSettingsProfilesBinding.inflate(inflater, container, false)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        binding.btnCloseProfiles.setOnClickListener { dismiss() }

        binding.btnSaveCurrentProfile.setOnClickListener {
            showSaveProfileDialog()
        }

        binding.btnImportProfile.setOnClickListener {
            importProfileLauncher.launch(arrayOf("*/*"))
        }

        binding.listProfiles.layoutManager = LinearLayoutManager(requireContext())
        adapter = ProfilesAdapter(
            profiles = profilesList,
            onApply = { profile ->
                val success = SettingsProfileManager.applyProfile(profile)
                if (success) {
                    Toast.makeText(requireContext(), getString(R.string.profile_applied_toast, profile.name), Toast.LENGTH_SHORT).show()
                    dismiss()
                } else {
                    Toast.makeText(requireContext(), getString(R.string.profile_apply_failed), Toast.LENGTH_SHORT).show()
                }
            },
            onShare = { profile ->
                SettingsProfileManager.shareProfile(requireContext(), profile)
            },
            onRename = { profile ->
                showRenameProfileDialog(profile)
            },
            onDelete = { profile ->
                showDeleteProfileDialog(profile)
            }
        )
        binding.listProfiles.adapter = adapter

        refreshProfiles()
    }

    private fun refreshProfiles() {
        profilesList.clear()
        profilesList.addAll(SettingsProfileManager.getAllProfiles())
        adapter?.notifyDataSetChanged()
    }

    private fun showSaveProfileDialog() {
        val editText = EditText(requireContext()).apply {
            hint = getString(R.string.profile_name_hint)
            setPadding(40, 20, 40, 20)
        }
        MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.profile_create_title)
            .setMessage(R.string.profile_create_msg)
            .setView(editText)
            .setPositiveButton(R.string.save) { _, _ ->
                val name = editText.text.toString().trim()
                if (name.isNotEmpty()) {
                    val saved = SettingsProfileManager.saveCurrentSettingsAsProfile(name)
                    if (saved != null) {
                        Toast.makeText(requireContext(), getString(R.string.profile_saved_toast, name), Toast.LENGTH_SHORT).show()
                        refreshProfiles()
                    }
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private fun showRenameProfileDialog(profile: SettingsProfile) {
        val editText = EditText(requireContext()).apply {
            setText(profile.name)
            setPadding(40, 20, 40, 20)
        }
        MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.profile_rename_title)
            .setView(editText)
            .setPositiveButton(R.string.save) { _, _ ->
                val newName = editText.text.toString().trim()
                if (newName.isNotEmpty() && SettingsProfileManager.renameProfile(profile, newName)) {
                    refreshProfiles()
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    private fun showDeleteProfileDialog(profile: SettingsProfile) {
        MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.profile_delete_title)
            .setMessage(getString(R.string.profile_delete_confirmation, profile.name))
            .setPositiveButton(R.string.delete) { _, _ ->
                if (SettingsProfileManager.deleteProfile(profile)) {
                    refreshProfiles()
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private class ProfilesAdapter(
        private val profiles: List<SettingsProfile>,
        private val onApply: (SettingsProfile) -> Unit,
        private val onShare: (SettingsProfile) -> Unit,
        private val onRename: (SettingsProfile) -> Unit,
        private val onDelete: (SettingsProfile) -> Unit
    ) : RecyclerView.Adapter<ProfilesAdapter.ViewHolder>() {

        class ViewHolder(val binding: ItemSettingsProfileBinding) : RecyclerView.ViewHolder(binding.root)

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
            val binding = ItemSettingsProfileBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            return ViewHolder(binding)
        }

        override fun onBindViewHolder(holder: ViewHolder, position: Int) {
            val profile = profiles[position]
            holder.binding.textProfileName.text = profile.name
            holder.binding.textProfileDesc.text = profile.description

            if (profile.isPreset) {
                holder.binding.imageProfileIcon.setImageResource(R.drawable.ic_settings)
                holder.binding.layoutCustomActions.visibility = View.GONE
            } else {
                holder.binding.imageProfileIcon.setImageResource(R.drawable.ic_folder_open)
                holder.binding.layoutCustomActions.visibility = View.VISIBLE
            }

            holder.binding.btnProfileApply.setOnClickListener { onApply(profile) }
            holder.binding.btnProfileShare.setOnClickListener { onShare(profile) }
            holder.binding.btnProfileRename.setOnClickListener { onRename(profile) }
            holder.binding.btnProfileDelete.setOnClickListener { onDelete(profile) }
        }

        override fun getItemCount(): Int = profiles.size
    }
}