// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>
#include "frozen/map.h"
#include "frozen/string.h"

/// Small helper to look up enums.
/// res = the result code
/// base = the base matching value in the StringKey table
#define LOOKUP_ENUM(res, base)                                                                     \
    QtCommon::StringLookup::Lookup(                                                                \
        QtCommon::StringLookup::StringKey((int)res + (int)QtCommon::StringLookup::base))

namespace QtCommon::StringLookup {

Q_NAMESPACE

// TODO(crueter): QML interface
enum StringKey {
    DataManagerSavesTooltip,
    DataManagerShadersTooltip,
    DataManagerUserNandTooltip,
    DataManagerSysNandTooltip,
    DataManagerModsTooltip,

    // Key install results
    KeyInstallSuccess,
    KeyInstallInvalidDir,
    KeyInstallErrorFailedCopy,
    KeyInstallErrorWrongFilename,
    KeyInstallErrorFailedInit,

    // Firmware install results
    FwInstallSuccess,
    FwInstallNoNCAs,
    FwInstallFailedDelete,
    FwInstallFailedCopy,
    FwInstallFailedCorrupted,

    // Firmware Check results
    FwCheckErrorFirmwareMissing,
    FwCheckErrorFirmwareCorrupted,

    // user data migrator
    MigrationPromptPrefix,
    MigrationPrompt,
    MigrationTooltipClearShader,
    MigrationTooltipKeepOld,
    MigrationTooltipClearOld,
    MigrationTooltipLinkOld,

    // ryujinx
    KvdbNonexistent,
    KvdbNoHeader,
    KvdbInvalidMagic,
    KvdbMisaligned,
    KvdbNoImens,
    RyujinxNoSaveId,
};

// NB: the constexpr check always succeeds (in clangd at least) if size arg < size
// always triple-check the size arg
static const constexpr frozen::map<StringKey, frozen::string, 29> strings = {
    // 0-4
    {DataManagerSavesTooltip,
     QT_TR_NOOP("Содержит файлы сохранений игр. НЕ УДАЛЯЙТЕ, ЕСЛИ НЕ УВЕРЕНЫ В СВОИХ ДЕЙСТВИЯХ!")},
    {DataManagerShadersTooltip,
     QT_TR_NOOP("Содержит кэш шейдеров Vulkan и OpenGL. Безопасно для очистки.")},
    {DataManagerUserNandTooltip, QT_TR_NOOP("Содержит установленные обновления и DLC для игр.")},
    {DataManagerSysNandTooltip, QT_TR_NOOP("Содержит файлы прошивки и системных апплетов.")},
    {DataManagerModsTooltip, QT_TR_NOOP("Содержит модификации, патчи и чит-коды для игр.")},

    // Key install
    // 5-9
    {KeyInstallSuccess, QT_TR_NOOP("Ключи дешифрования успешно установлены")},
    {KeyInstallInvalidDir, QT_TR_NOOP("Не удалось прочитать папку с ключами, операция отменена")},
    {KeyInstallErrorFailedCopy, QT_TR_NOOP("Не удалось скопировать один или несколько файлов ключей.")},
    {KeyInstallErrorWrongFilename,
     QT_TR_NOOP("Убедитесь, что файл ключей имеет расширение .keys, и повторите попытку.")},
    {KeyInstallErrorFailedInit,
     QT_TR_NOOP(
         "Не удалось инициализировать ключи. Проверьте актуальность дампера и повторите дамп ключей.")},

    // fw install
    // 10-14
    {FwInstallSuccess, QT_TR_NOOP("Успешно установлена версия прошивки %1")},
    {FwInstallNoNCAs, QT_TR_NOOP("Не удалось обнаружить файлы прошивки (.nca)")},
    {FwInstallFailedDelete, QT_TR_NOOP("Не удалось удалить старые файлы прошивки.")},
    {FwInstallFailedCopy, QT_TR_NOOP("Не удалось скопировать файлы прошивки в NAND.")},
    {FwInstallFailedCorrupted,
     QT_TR_NOOP(
         "Установка прошивки отменена. Перезапустите STORM EDEN или повторите установку прошивки.")},

    {FwCheckErrorFirmwareMissing,
     QT_TR_NOOP(
         "Прошивка отсутствует. Прошивка требуется для работы некоторых игр и Home Menu.")},
    {FwCheckErrorFirmwareCorrupted,
     QT_TR_NOOP(
         "Прошивка обнаружена, но не может быть прочитана. Проверьте ключи дешифрования и повторите установку.")},

    // migrator
    // 17-22
    {MigrationPromptPrefix, QT_TR_NOOP("STORM EDEN обнаружил данные следующих эмуляторов:")},
    {MigrationPrompt,
     QT_TR_NOOP("Хотите перенести данные для использования в STORM EDEN?\n"
                "Выберите эмулятор для переноса данных.\n"
                "Этот процесс может занять некоторое время.")},
    {MigrationTooltipClearShader, QT_TR_NOOP("Рекомендуется очистить кэш шейдеров.\nНе снимайте отметку, если не уверены.")},
    {MigrationTooltipKeepOld,
     QT_TR_NOOP("Сохранить старую папку данных. Рекомендуется при наличии свободного места на диске.")},
    {MigrationTooltipClearOld, QT_TR_NOOP("Удалить старую папку данных для экономии места на диске.")},
    {MigrationTooltipLinkOld,
     QT_TR_NOOP("Создать символическую ссылку между папками для общего доступа к данным.")},

    // 23-28
    {KvdbNonexistent, QT_TR_NOOP("База данных игр Ryujinx не найдена.")},
    {KvdbNoHeader, QT_TR_NOOP("Неверный заголовок базы данных Ryujinx.")},
    {KvdbInvalidMagic, QT_TR_NOOP("Неверная сигнатура базы данных Ryujinx.")},
    {KvdbMisaligned, QT_TR_NOOP("Ошибка выравнивания в базе данных Ryujinx.")},
    {KvdbNoImens, QT_TR_NOOP("Элементы в базе данных Ryujinx не найдены.")},
    {RyujinxNoSaveId, QT_TR_NOOP("Игра %1 не найдена в базе данных Ryujinx.")},
};

static inline const QString Lookup(StringKey key) {
    return QObject::tr(strings.at(key).data());
}

} // namespace QtCommon::StringLookup
