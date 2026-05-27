#pragma once

#include "tcp/channel_ref.h"

#include <QFrame>
#include <QString>

#include <array>

class QCheckBox;
class QComboBox;
class QGraphicsDropShadowEffect;
class QLabel;
class QPushButton;
class QSpinBox;

namespace tcp::app {

class ChannelChip;
class JobController;
class SegmentedControl;

class OutputPanel : public QFrame
{
    Q_OBJECT

public:
    explicit OutputPanel(JobController* controller, QWidget* parent = nullptr);

protected:
    bool event(QEvent* event) override;

private slots:
    void on_format_changed_(int combo_index);
    void on_bit_depth_segment_(const QString& value);
    void on_bc_variant_changed_(int combo_index);
    void on_resize_mode_changed_(int combo_index);
    void on_filter_changed_(int combo_index);
    void on_custom_size_changed_();
    void on_export_clicked_();

    void on_target_size_changed_(int width, int height);
    void on_populated_slots_changed_(int populated);
    void on_export_started_(const QString& path);
    void on_export_finished_(const QString& path);
    void on_export_failed_(const QString& path, const QString& error);

    void on_slot_loaded_(int slot);
    void on_slot_cleared_(int slot);
    void on_channel_map_changed_(int destination, tcp::ChannelRef ref);

private:
    void build_ui_();
    void apply_resize_mode_visibility_();
    void refresh_target_row_(int destination);

    JobController* controller_ = nullptr;

    QComboBox* format_combo_ = nullptr;
    SegmentedControl* bit_depth_segment_ = nullptr;
    QComboBox* bc_variant_combo_ = nullptr;
    QLabel* bc_variant_label_ = nullptr;
    QCheckBox* flip_vertical_check_ = nullptr;
    QComboBox* resize_mode_combo_ = nullptr;
    QComboBox* filter_combo_ = nullptr;
    QSpinBox* custom_width_ = nullptr;
    QSpinBox* custom_height_ = nullptr;
    QLabel* custom_size_label_ = nullptr;
    QPushButton* export_button_ = nullptr;
    QGraphicsDropShadowEffect* export_glow_ = nullptr;

    // Target-channel summary rows (R, G, B, A).
    struct TargetRow {
        ChannelChip* dst_chip = nullptr;
        QLabel* dst_letter = nullptr;
        QLabel* arrow = nullptr;
        QLabel* src_pill = nullptr;
        QLabel* filename = nullptr;
    };
    std::array<TargetRow, 4> target_rows_{};
};

} // namespace tcp::app
