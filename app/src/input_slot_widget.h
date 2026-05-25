#pragma once

#include "tcp/channel_ref.h"

#include <QFrame>
#include <QString>

class QComboBox;
class QLabel;
class QToolButton;

namespace tcp::app {

class JobController;

// One row of the left-hand panel — represents one destination channel
// (R/G/B/A) and the input texture feeding it.
class InputSlotWidget : public QFrame
{
    Q_OBJECT

public:
    InputSlotWidget(int slot_index, JobController* controller, QWidget* parent = nullptr);

    [[nodiscard]] int slot_index() const noexcept { return slot_index_; }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void on_browse_();
    void on_clear_();
    void on_source_channel_changed_(int combo_index);
    void on_slot_loading_(int slot, const QString& path);
    void on_slot_loaded_(int slot);
    void on_slot_load_failed_(int slot, const QString& error);
    void on_slot_cleared_(int slot);

private:
    void build_ui_();
    void refresh_state_for_(int slot);
    void set_thumbnail_from_(const QString& path);
    void set_status_text_(const QString& text);

    int slot_index_;
    JobController* controller_;
    QString current_path_;

    QLabel* destination_badge_ = nullptr;
    QLabel* thumbnail_ = nullptr;
    QLabel* filename_label_ = nullptr;
    QLabel* status_label_ = nullptr;
    QComboBox* source_channel_combo_ = nullptr;
    QToolButton* clear_button_ = nullptr;
};

} // namespace tcp::app
