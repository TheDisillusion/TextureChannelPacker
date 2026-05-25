#pragma once

#include <QFrame>

class QButtonGroup;
class QCheckBox;
class QLabel;
class QToolButton;

namespace tcp::app {

class JobController;
class PreviewWidget;
enum class PreviewViewMode : int;

class PreviewPanel : public QFrame
{
    Q_OBJECT

public:
    explicit PreviewPanel(JobController* controller, QWidget* parent = nullptr);

private:
    void build_ui_();

    JobController* controller_ = nullptr;
    PreviewWidget* preview_ = nullptr;
    QButtonGroup* view_mode_group_ = nullptr;
    QToolButton* fit_button_ = nullptr;
    QToolButton* one_to_one_button_ = nullptr;
    QCheckBox* checkerboard_check_ = nullptr;
    QLabel* zoom_label_ = nullptr;
};

} // namespace tcp::app
