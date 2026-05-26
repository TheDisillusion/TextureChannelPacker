#pragma once

#include <QFrame>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;

namespace tcp::app {

class JobController;

class OutputPanel : public QFrame
{
    Q_OBJECT

public:
    explicit OutputPanel(JobController* controller, QWidget* parent = nullptr);

private slots:
    void on_format_changed_(int combo_index);
    void on_bit_depth_changed_(int combo_index);
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

private:
    void build_ui_();
    void apply_resize_mode_visibility_();

    JobController* controller_ = nullptr;

    QComboBox* format_combo_ = nullptr;
    QComboBox* bit_depth_combo_ = nullptr;
    QComboBox* bc_variant_combo_ = nullptr;
    class QLabel* bc_variant_label_ = nullptr;
    QCheckBox* flip_vertical_check_ = nullptr;
    QComboBox* resize_mode_combo_ = nullptr;
    QComboBox* filter_combo_ = nullptr;
    QSpinBox* custom_width_ = nullptr;
    QSpinBox* custom_height_ = nullptr;
    QLabel* target_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QPushButton* export_button_ = nullptr;
};

} // namespace tcp::app
