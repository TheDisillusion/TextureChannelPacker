#include "main_window.h"

#include "input_slot_widget.h"
#include "job_controller.h"
#include "output_panel.h"
#include "preview_panel.h"

#include "tcp/exporter.h"
#include "tcp/pack_job.h"
#include "tcp/project.h"
#include "tcp/version.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace tcp::app {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Texture Channel Packer"));
    resize(1280, 800);
    setMinimumSize(960, 640);

    controller_ = new JobController(this);

    build_central_widget_();
    build_menus_();
    wire_status_bar_();

    // Anything that changes the active job marks the project dirty.
    connect(controller_, &JobController::slot_loaded, this, &MainWindow::on_mark_dirty_);
    connect(controller_, &JobController::slot_cleared, this, &MainWindow::on_mark_dirty_);
    connect(controller_, &JobController::channel_map_changed, this, &MainWindow::on_mark_dirty_);
    connect(controller_, &JobController::resize_mode_changed, this, &MainWindow::on_mark_dirty_);
    connect(controller_, &JobController::resize_filter_changed, this, &MainWindow::on_mark_dirty_);
    connect(controller_, &JobController::custom_size_changed, this, &MainWindow::on_mark_dirty_);
    connect(controller_, &JobController::output_format_changed, this, &MainWindow::on_mark_dirty_);
    connect(controller_, &JobController::output_format_kind_changed, this,
            &MainWindow::on_mark_dirty_);
    connect(controller_, &JobController::slot_treat_as_srgb_changed, this,
            &MainWindow::on_mark_dirty_);

    // Surface async export status as toasts in the main window — keeps the
    // success path quiet and the failure path obvious.
    connect(controller_, &JobController::export_finished, this,
            [this](const QString& path) {
                show_toast_(tr("Exported %1").arg(QFileInfo(path).fileName()));
            });
    connect(controller_, &JobController::export_failed, this,
            [this](const QString& path, const QString& error) {
                show_toast_(tr("Export failed: %1").arg(error), 8000);
                Q_UNUSED(path);
            });
    connect(controller_, &JobController::slot_load_failed, this,
            [this](int slot, const QString& error) {
                show_toast_(tr("Slot %1: %2").arg(slot).arg(error), 8000);
            });

    update_window_title_();
}

void MainWindow::build_central_widget_()
{
    auto* inputs_frame = new QFrame(this);
    inputs_frame->setObjectName(QStringLiteral("InputsPanel"));
    inputs_frame->setFrameShape(QFrame::StyledPanel);

    auto* inputs_title = new QLabel(QStringLiteral("Inputs"), inputs_frame);
    inputs_title->setObjectName(QStringLiteral("PanelTitle"));

    auto* inputs_layout = new QVBoxLayout(inputs_frame);
    inputs_layout->setContentsMargins(14, 14, 14, 14);
    inputs_layout->setSpacing(10);
    inputs_layout->addWidget(inputs_title);

    for (int i = 0; i < slot_count; ++i) {
        auto* slot = new InputSlotWidget(i, controller_, inputs_frame);
        slot_widgets_[i] = slot;
        inputs_layout->addWidget(slot);
    }
    inputs_layout->addStretch(1);

    auto* preview = new PreviewPanel(controller_, this);

    output_panel_ = new OutputPanel(controller_, this);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("MainSplitter"));
    splitter->addWidget(inputs_frame);
    splitter->addWidget(preview);
    splitter->addWidget(output_panel_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({320, 640, 280});

    setCentralWidget(splitter);
}

void MainWindow::build_menus_()
{
    auto* file_menu = menuBar()->addMenu(tr("&File"));

    auto* new_act = file_menu->addAction(tr("&New project"));
    new_act->setShortcut(QKeySequence::New);
    connect(new_act, &QAction::triggered, this, &MainWindow::on_new_project_);

    auto* open_act = file_menu->addAction(tr("&Open project…"));
    open_act->setShortcut(QKeySequence::Open);
    connect(open_act, &QAction::triggered, this, &MainWindow::on_open_project_);

    file_menu->addSeparator();

    auto* save_act = file_menu->addAction(tr("&Save project"));
    save_act->setShortcut(QKeySequence::Save);
    connect(save_act, &QAction::triggered, this, [this] { on_save_project_(); });

    auto* save_as_act = file_menu->addAction(tr("Save project &as…"));
    save_as_act->setShortcut(QKeySequence::SaveAs);
    connect(save_as_act, &QAction::triggered, this, [this] { on_save_project_as_(); });

    file_menu->addSeparator();

    auto* export_act = file_menu->addAction(tr("&Export packed texture…"));
    export_act->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(export_act, &QAction::triggered, this, &MainWindow::on_export_);

    file_menu->addSeparator();

    auto* quit = file_menu->addAction(tr("E&xit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    auto* help_menu = menuBar()->addMenu(tr("&Help"));
    auto* about = help_menu->addAction(tr("&About"));
    connect(about, &QAction::triggered, this, [this] {
        const QString text
            = tr("Texture Channel Packer %1\n\n"
                 "Native desktop tool for packing R/G/B/A channels of texture maps.\n\n"
                 "Uses Qt 6 under the LGPLv3, OpenImageIO under the BSD 3-Clause license, "
                 "nlohmann/json under the MIT License, and Catch2 under the Boost Software "
                 "License.")
                  .arg(QString::fromUtf8(tcp::version_string().data(),
                                         static_cast<int>(tcp::version_string().size())));
        QMessageBox::about(this, tr("About"), text);
    });
}

void MainWindow::on_new_project_()
{
    if (!maybe_save_if_dirty_()) {
        return;
    }
    controller_->reset();
    project_path_.clear();
    dirty_ = false;
    update_window_title_();
    show_toast_(tr("New project"));
}

void MainWindow::on_open_project_()
{
    if (!maybe_save_if_dirty_()) {
        return;
    }
    const QString default_dir = project_path_.isEmpty()
                                    ? QStandardPaths::writableLocation(
                                          QStandardPaths::DocumentsLocation)
                                    : QFileInfo(project_path_).path();
    const QString chosen = QFileDialog::getOpenFileName(
        this, tr("Open project"), default_dir, tr("Texture Channel Packer (*.tcpproj);;All files (*)"));
    if (chosen.isEmpty()) {
        return;
    }
    auto result = tcp::project::load(std::filesystem::path(chosen.toStdU16String()));
    if (!result.ok()) {
        QMessageBox::warning(this, tr("Open project"),
                             tr("Could not load %1:\n\n%2")
                                 .arg(chosen, QString::fromStdString(result.error)));
        return;
    }
    controller_->apply_project(*result.project);
    project_path_ = chosen;
    dirty_ = false;
    update_window_title_();
    show_toast_(tr("Opened %1").arg(QFileInfo(chosen).fileName()));
}

bool MainWindow::on_save_project_()
{
    if (project_path_.isEmpty()) {
        return on_save_project_as_();
    }
    return save_to_(project_path_);
}

bool MainWindow::on_save_project_as_()
{
    const QString default_dir = project_path_.isEmpty()
                                    ? QStandardPaths::writableLocation(
                                          QStandardPaths::DocumentsLocation)
                                    : QFileInfo(project_path_).path();
    const QString default_name = project_path_.isEmpty()
                                     ? tr("untitled.tcpproj")
                                     : QFileInfo(project_path_).fileName();
    const QString chosen = QFileDialog::getSaveFileName(
        this, tr("Save project"), default_dir + QStringLiteral("/") + default_name,
        tr("Texture Channel Packer (*.tcpproj)"));
    if (chosen.isEmpty()) {
        return false;
    }
    return save_to_(chosen);
}

bool MainWindow::save_to_(const QString& path)
{
    auto result = tcp::project::save(controller_->snapshot_project(),
                                     std::filesystem::path(path.toStdU16String()));
    if (!result.ok) {
        QMessageBox::warning(this, tr("Save project"),
                             tr("Could not save %1:\n\n%2")
                                 .arg(path, QString::fromStdString(result.error)));
        return false;
    }
    project_path_ = path;
    dirty_ = false;
    update_window_title_();
    show_toast_(tr("Saved %1").arg(QFileInfo(path).fileName()));
    return true;
}

void MainWindow::on_export_()
{
    if (!controller_->any_slot_populated()) {
        show_toast_(tr("Load at least one input slot before exporting"));
        return;
    }
    const auto current_format = controller_->output_format_kind();
    const QString ext = (current_format == tcp::exporter::Format::PNG)
                            ? QStringLiteral("png")
                            : QStringLiteral("tga");
    const QString filter = (current_format == tcp::exporter::Format::PNG)
                               ? tr("PNG (*.png)")
                               : tr("Targa (*.tga)");
    const QString default_dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString suggested = default_dir + QStringLiteral("/packed.") + ext;
    const QString path = QFileDialog::getSaveFileName(this, tr("Export packed texture"),
                                                      suggested, filter);
    if (path.isEmpty()) {
        return;
    }
    controller_->export_to(path, current_format);
}

void MainWindow::on_mark_dirty_()
{
    if (dirty_) {
        return;
    }
    dirty_ = true;
    update_window_title_();
}

void MainWindow::update_window_title_()
{
    QString name = project_path_.isEmpty() ? tr("Untitled") : QFileInfo(project_path_).fileName();
    if (dirty_) {
        name = QStringLiteral("* ") + name;
    }
    setWindowTitle(QStringLiteral("%1 — Texture Channel Packer").arg(name));
}

bool MainWindow::maybe_save_if_dirty_()
{
    if (!dirty_) {
        return true;
    }
    const auto reply = QMessageBox::question(
        this, tr("Unsaved changes"),
        tr("The current project has unsaved changes. Save before continuing?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (reply == QMessageBox::Cancel) {
        return false;
    }
    if (reply == QMessageBox::Discard) {
        return true;
    }
    return on_save_project_();
}

void MainWindow::show_toast_(const QString& text, int duration_ms)
{
    if (!toast_label_) {
        return;
    }
    toast_label_->setText(text);
    toast_label_->show();
    QTimer::singleShot(duration_ms, this, [this] {
        if (toast_label_) {
            toast_label_->clear();
        }
    });
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (maybe_save_if_dirty_()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::wire_status_bar_()
{
    auto* sb = statusBar();
    auto* populated_label = new QLabel(QStringLiteral("0 / 4 inputs"), sb);
    auto* target_label = new QLabel(QStringLiteral("target —"), sb);
    toast_label_ = new QLabel(QString{}, sb);
    toast_label_->setObjectName(QStringLiteral("Toast"));
    sb->addWidget(populated_label);
    sb->addWidget(toast_label_, 1);
    sb->addPermanentWidget(target_label);

    connect(controller_, &JobController::populated_slots_changed, populated_label,
            [populated_label](int populated) {
                populated_label->setText(QStringLiteral("%1 / 4 inputs").arg(populated));
            });
    connect(controller_, &JobController::target_size_changed, target_label,
            [target_label](int w, int h) {
                if (w <= 0 || h <= 0) {
                    target_label->setText(QStringLiteral("target —"));
                } else {
                    target_label->setText(QStringLiteral("target %1×%2").arg(w).arg(h));
                }
            });
}

} // namespace tcp::app
