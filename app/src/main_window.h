#pragma once

#include <QMainWindow>

#include <array>

namespace tcp::app {

class JobController;
class InputSlotWidget;
class OutputPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void build_central_widget_();
    void build_menus_();
    void wire_status_bar_();

    JobController* controller_ = nullptr;
    std::array<InputSlotWidget*, 4> slot_widgets_{};
    OutputPanel* output_panel_ = nullptr;
};

} // namespace tcp::app
