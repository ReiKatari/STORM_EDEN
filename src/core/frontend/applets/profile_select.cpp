// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "core/frontend/applets/profile_select.h"
#include "core/hle/service/acc/profile_manager.h"

namespace Core::Frontend {

ProfileSelectApplet::~ProfileSelectApplet() = default;

void DefaultProfileSelectApplet::Close() const {}

void DefaultProfileSelectApplet::SelectProfile(SelectProfileCallback callback,
                                               const ProfileSelectParameters& parameters) const {
    Service::Account::ProfileManager manager;
    auto user = manager.GetUser(Settings::values.current_user.GetValue());
    if (user && user->IsValid()) {
        LOG_INFO(Service_ACC, "called, selecting configured user {} instead of prompting...", user->FormattedString());
        callback(*user);
        return;
    }
    auto last_opened = manager.GetLastOpenedUser();
    if (last_opened.IsValid()) {
        LOG_INFO(Service_ACC, "called, selecting last opened user {} instead of prompting...", last_opened.FormattedString());
        callback(last_opened);
        return;
    }
    const auto all_users = manager.GetAllUsers();
    for (const auto& u : all_users) {
        if (u.IsValid()) {
            LOG_INFO(Service_ACC, "called, selecting user {} instead of prompting...", u.FormattedString());
            callback(u);
            return;
        }
    }
    auto default_user = manager.GetUser(0).value_or(Common::UUID{});
    LOG_INFO(Service_ACC, "called, selecting fallback user {} instead of prompting...", default_user.FormattedString());
    callback(default_user);
}

} // namespace Core::Frontend
