#pragma once

#include <QFrame>

class QCheckBox;
class QLabel;

namespace tcp::app {

class ChannelChip;
class JobController;
class PreviewWidget;
class SegmentedControl;
enum class PreviewViewMode : int;

class PreviewPanel : public QFrame
{
    Q_OBJECT

public:
    explicit PreviewPanel(JobController* controller, QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void build_ui_();
    void update_caption_();
    void apply_view_(PreviewViewMode mode);
    void position_badge_();

    JobController* controller_ = nullptr;
    PreviewWidget* preview_ = nullptr;
    QFrame* surface_ = nullptr;
    SegmentedControl* view_segment_ = nullptr;
    SegmentedControl* zoom_segment_ = nullptr;
    QCheckBox* checker_check_ = nullptr;

    QFrame* channel_badge_ = nullptr;
    ChannelChip* channel_badge_dot_ = nullptr;
    QLabel* channel_badge_label_ = nullptr;

    QLabel* size_caption_ = nullptr;
    QLabel* view_caption_ = nullptr;
};

} // namespace tcp::app
