// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <vector>
#include <QDialog>
#include <QImage>
#include <QString>
#include <QRect>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

class QLabel;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QComboBox;
class QCheckBox;
class QSlider;
class QSpinBox;
class QTabWidget;
class QListWidget;
class QPaintEvent;
class QMouseEvent;

namespace Core {
class System;
}

// Interactive screenshot widget for drawing and selecting ROI (Regions of Interest)
class ROIPreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit ROIPreviewWidget(QWidget* parent = nullptr);
    void SetImage(const QImage& image);
    void SetZones(const std::vector<QRect>& zones);
    std::vector<QRect> GetZones() const { return m_zones; }
    void AddZone(const QRect& rect);
    void ClearZones();
    void SetActiveZoneIndex(int index);

signals:
    void ZonesChanged();
    void ZoneSelected(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRect GetImageDrawRect() const;
    QPoint ScreenToImageCoords(const QPoint& pt) const;
    QRect ScreenToImageRect(const QRect& r) const;
    QRect ImageToScreenRect(const QRect& r) const;

    QImage m_image;
    std::vector<QRect> m_zones;
    int m_active_zone{-1};
    bool m_is_drawing{false};
    QPoint m_start_pt;
    QRect m_current_rect;
};

// Floating HUD Subtitle Overlay for in-game live translation display
class TranslatorHUDOverlay : public QWidget {
    Q_OBJECT
public:
    explicit TranslatorHUDOverlay(QWidget* parent = nullptr);
    void SetSubtitleText(const QString& text);
    void SetFontSize(int pt);
    void SetTextColor(const QColor& color);
    void SetBackgroundOpacity(int opacityPercent);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    QString m_text;
    int m_font_size{20};
    QColor m_text_color{0x00, 0xE5, 0xFF};
    int m_bg_opacity{75};
    QPoint m_drag_position;
};

struct SpeechSegment {
    enum class Role {
        Default,
        Speaker1,
        Speaker2,
        Narrator
    };
    Role role{Role::Default};
    QString speaker_name;
    QString text;
};

class GameTranslator : public QDialog {
    Q_OBJECT

public:
    explicit GameTranslator(Core::System& system, QWidget* parent = nullptr);
    ~GameTranslator() override;

    void TranslateFrame(const QImage& frame);
    void SpeakText(const QString& text);
    void StopSpeech();

private slots:
    void OnTranslateClicked();
    void OnSpeakClicked();
    void OnCopyClicked();
    void OnClearClicked();
    void OnProviderChanged(int index);
    void OnTestTTSClicked();
    void OnToggleHUDClicked(bool checked);
    void OnAutoIntervalToggled(bool checked);
    void OnAutoIntervalTimerTimeout();
    void OnAddZoneClicked();
    void OnRemoveZoneClicked();
    void OnClearZonesClicked();
    void OnPresetSelected(int index);
    void OnZoneListSelectionChanged();
    void OnZonesUpdatedInPreview();
    void OnZoneSpinChanged();

private:
    void LoadSettings();
    void SaveSettings();
    void PopulateTTSVoices();
    void PerformOnlineTranslation(const QString& text, const QString& src_lang, const QString& tgt_lang);
    void PerformAIProviderTranslation(const QString& text, const QString& provider, const QString& src_lang, const QString& tgt_lang);
    void ExtractTextFromImageAndTranslate(const QImage& frame);
    std::vector<SpeechSegment> ParseDialogueSegments(const QString& text);
    void SpeakSingleSegment(const SpeechSegment& segment);
    QString GetCurrentTitleIdString() const;

    Core::System& m_system;
    QNetworkAccessManager* m_network_mgr{nullptr};
    QImage m_captured_frame;

    // Tabs
    QTabWidget* m_tab_widget{nullptr};

    // Tab 1: Translation & AI Hub
    QComboBox* m_provider_combo{nullptr};
    QLineEdit* m_api_key_edit{nullptr};
    QComboBox* m_ocr_engine_combo{nullptr};
    QLineEdit* m_system_prompt_edit{nullptr};
    QComboBox* m_src_lang_combo{nullptr};
    QComboBox* m_tgt_lang_combo{nullptr};
    QCheckBox* m_auto_speak_check{nullptr};
    QLabel* m_status_label{nullptr};
    QTextEdit* m_original_edit{nullptr};
    QTextEdit* m_translated_edit{nullptr};
    QPushButton* m_translate_btn{nullptr};
    QPushButton* m_speak_btn{nullptr};
    QPushButton* m_copy_btn{nullptr};
    QPushButton* m_clear_btn{nullptr};

    // Tab 2: ROI Editor
    ROIPreviewWidget* m_roi_preview{nullptr};
    QListWidget* m_zone_list{nullptr};
    QComboBox* m_preset_combo{nullptr};
    QSpinBox* m_zone_x_spin{nullptr};
    QSpinBox* m_zone_y_spin{nullptr};
    QSpinBox* m_zone_w_spin{nullptr};
    QSpinBox* m_zone_h_spin{nullptr};
    QPushButton* m_add_zone_btn{nullptr};
    QPushButton* m_remove_zone_btn{nullptr};
    QPushButton* m_clear_zones_btn{nullptr};
    QCheckBox* m_save_per_game_check{nullptr};

    // Tab 3: Monitoring & HUD Overlay
    QCheckBox* m_auto_interval_check{nullptr};
    QSpinBox* m_interval_spin{nullptr};
    QTimer* m_auto_interval_timer{nullptr};
    QCheckBox* m_show_hud_check{nullptr};
    QCheckBox* m_enable_floating_btn_check{nullptr};
    QSlider* m_hud_opacity_slider{nullptr};
    QSpinBox* m_hud_font_size_spin{nullptr};
    QComboBox* m_hud_color_combo{nullptr};
    TranslatorHUDOverlay* m_hud_overlay{nullptr};

    // Tab 4: TTS Configuration
    QCheckBox* m_multi_voice_check{nullptr};
    QComboBox* m_tts_voice_combo{nullptr};
    QComboBox* m_speaker1_voice_combo{nullptr};
    QComboBox* m_speaker2_voice_combo{nullptr};
    QComboBox* m_narrator_voice_combo{nullptr};
    QSlider* m_tts_volume_slider{nullptr};
    QSlider* m_tts_rate_slider{nullptr};
    QPushButton* m_test_tts_btn{nullptr};

#ifdef _WIN32
    struct ISpVoice* m_sapi_voice{nullptr};
#endif
};
