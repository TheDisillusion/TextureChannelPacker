#pragma once

#include "tcp/channel_ref.h"

#include <QFrame>
#include <QLabel>
#include <QString>

class QToolButton;

namespace tcp::app {

class ChannelChip;
class JobController;
class SegmentedControl;

// Thumbnail well — a 48×48 box that shows a pixmap when populated or a
// dashed border with a diagonal-stripe pattern when empty. Owns its own
// paint so the stripe + drop arrow don't fight QSS.
class ThumbnailWell : public QLabel
{
    Q_OBJECT
public:
    explicit ThumbnailWell(QWidget* parent = nullptr);
    void set_empty(bool empty);
    [[nodiscard]] bool is_empty() const noexcept { return empty_; }
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    bool empty_ = true;
};

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
    void on_source_segment_changed_(const QString& value);
    void on_slot_loading_(int slot, const QString& path);
    void on_slot_loaded_(int slot);
    void on_slot_load_failed_(int slot, const QString& error);
    void on_slot_cleared_(int slot);

private:
    void build_ui_();
    void refresh_state_for_(int slot);
    void set_thumbnail_from_(const QString& path);
    void set_status_tooltip_(const QString& text);

    int slot_index_;
    JobController* controller_;
    QString current_path_;

    ChannelChip* destination_chip_ = nullptr;
    ThumbnailWell* thumbnail_ = nullptr;
    QLabel* filename_label_ = nullptr;
    QLabel* from_label_ = nullptr;
    SegmentedControl* source_picker_ = nullptr;
    QToolButton* clear_button_ = nullptr;
};

} // namespace tcp::app
