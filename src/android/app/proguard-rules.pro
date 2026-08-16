# SPDX-FileCopyrightText: 2023 yuzu Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# To get usable stack traces
-dontobfuscate

# Prevents crashing when using Wini
-keep class org.ini4j.spi.IniParser
-keep class org.ini4j.spi.IniBuilder
-keep class org.ini4j.spi.IniFormatter

# Keep all JNI-referenced classes and members
-keep class org.yuzu.yuzu_emu.NativeLibrary { *; }
-keepclassmembers class org.yuzu.yuzu_emu.NativeLibrary { *; }
-keep class org.yuzu.yuzu_emu.disk_shader_cache.** { *; }
-keep class org.yuzu.yuzu_emu.model.** { *; }
-keep class org.yuzu.yuzu_emu.overlay.model.** { *; }
-keep class org.yuzu.yuzu_emu.features.input.** { *; }
-keep class org.yuzu.yuzu_emu.features.settings.** { *; }
-keep class org.yuzu.yuzu_emu.utils.** { *; }
-keep class kotlin.Pair { *; }

# Suppress warnings for R8
-dontwarn org.bouncycastle.jsse.BCSSLParameters
-dontwarn org.bouncycastle.jsse.BCSSLSocket
-dontwarn org.bouncycastle.jsse.provider.BouncyCastleJsseProvider
-dontwarn org.conscrypt.Conscrypt$Version
-dontwarn org.conscrypt.Conscrypt
-dontwarn org.conscrypt.ConscryptHostnameVerifier
-dontwarn org.openjsse.javax.net.ssl.SSLParameters
-dontwarn org.openjsse.javax.net.ssl.SSLSocket
-dontwarn org.openjsse.net.ssl.OpenJSSE
-dontwarn java.beans.Introspector
-dontwarn java.beans.VetoableChangeListener
-dontwarn java.beans.VetoableChangeSupport
