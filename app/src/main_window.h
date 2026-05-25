#pragma once

#include <QMainWindow>
#include <QString>

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

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void on_new_project_();
    void on_open_project_();
    bool on_save_project_();
    bool on_save_project_as_();
    void on_export_();
    void on_mark_dirty_();

private:
    void build_central_widget_();
    void build_menus_();
    void wire_status_bar_();
    void update_window_title_();
    void show_toast_(const QString& text, int duration_ms = 5000);
    bool maybe_save_if_dirty_();
    bool save_to_(const QString& path);

    JobController* controller_ = nullptr;
    std::array<InputSlotWidget*, 4> slot_widgets_{};
    OutputPanel* output_panel_ = nullptr;

    QString project_path_;
    bool dirty_ = false;

    class QLabel* toast_label_ = nullptr;
};

} // namespace tcp::app
