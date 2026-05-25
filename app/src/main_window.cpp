#include "main_window.h"

#include "input_slot_widget.h"
#include "job_controller.h"
#include "output_panel.h"
#include "preview_panel.h"

#include "tcp/pack_job.h"
#include "tcp/version.h"

#include <QAction>
#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QString>
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
                 "and Catch2 under the Boost Software License.")
                  .arg(QString::fromUtf8(tcp::version_string().data(),
                                         static_cast<int>(tcp::version_string().size())));
        QMessageBox::about(this, tr("About"), text);
    });
}

void MainWindow::wire_status_bar_()
{
    auto* sb = statusBar();
    auto* populated_label = new QLabel(QStringLiteral("0 / 4 inputs"), sb);
    auto* target_label = new QLabel(QStringLiteral("target —"), sb);
    sb->addWidget(populated_label);
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
