#include "main_window.h"

#include "design_tokens.h"
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
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace tcp::app {

namespace {

// The two button glyphs are painted in C++ at the target pixel size
// rather than loaded from .svg, because Qt's SVG icon engine requires
// linking Qt6Svg (not pulled in by our vcpkg manifest) and Qt's qsvg
// image-format plugin isn't deployed either. The geometry is trivial,
// so doing it in QPainter keeps the dependency footprint flat.

QPixmap make_plus_glyph(int size, const QColor& stroke)
{
    const int dpr = 2; // render at 2x for crisp scaling on hi-dpi displays
    QPixmap pm(size * dpr, size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(stroke);
    pen.setWidthF(1.4);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    const qreal s = size;
    p.drawLine(QPointF(s / 2.0, s * 0.18),
               QPointF(s / 2.0, s * 0.82));
    p.drawLine(QPointF(s * 0.18, s / 2.0),
               QPointF(s * 0.82, s / 2.0));
    return pm;
}

QPixmap make_grid_glyph(int size, const QColor& stroke)
{
    const int dpr = 2;
    QPixmap pm(size * dpr, size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(stroke);
    pen.setWidthF(1.1);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const qreal a = size * 0.18;
    const qreal b = size * 0.82;
    const qreal c = size / 2.0;
    // Outer square.
    p.drawRect(QRectF(a, a, b - a, b - a));
    // Horizontal + vertical interior strokes.
    p.drawLine(QPointF(a, c), QPointF(b, c));
    p.drawLine(QPointF(c, a), QPointF(c, b));
    return pm;
}

QIcon plus_icon(int size, const QColor& stroke)  { return QIcon(make_plus_glyph(size, stroke)); }
QIcon grid_icon(int size, const QColor& stroke)  { return QIcon(make_grid_glyph(size, stroke)); }

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Texture Channel Packer"));
    resize(1320, 820);
    setMinimumSize(1040, 660);
    // Force a fully opaque background — Windows 11's QMainWindow defaults
    // can otherwise let Mica/acrylic pass desktop colors through and tint
    // the canvas a brighter gray than the design tokens specify.
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAutoFillBackground(true);

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
    connect(controller_, &JobController::flip_vertical_y_changed, this,
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

    auto* inputs_header = new QFrame(inputs_frame);
    inputs_header->setAttribute(Qt::WA_TranslucentBackground, true);
    {
        auto* h = new QHBoxLayout(inputs_header);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);
        auto* title = new QLabel(QStringLiteral("INPUTS"), inputs_header);
        title->setObjectName(QStringLiteral("SectionTitle"));
        auto* counter = new QLabel(QStringLiteral("0 of 4"), inputs_header);
        counter->setObjectName(QStringLiteral("MonoCaption"));
        counter->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(title);
        h->addStretch(1);
        h->addWidget(counter);

        connect(controller_, &JobController::populated_slots_changed, counter,
                [counter](int populated) {
                    counter->setText(QStringLiteral("%1 of 4").arg(populated));
                });
    }

    auto* inputs_layout = new QVBoxLayout(inputs_frame);
    inputs_layout->setContentsMargins(14, 14, 14, 14);
    inputs_layout->setSpacing(10);
    inputs_layout->addWidget(inputs_header);

    for (int i = 0; i < slot_count; ++i) {
        auto* slot = new InputSlotWidget(i, controller_, inputs_frame);
        slot_widgets_[i] = slot;
        inputs_layout->addWidget(slot);
    }
    inputs_layout->addStretch(1);

    // Footer: Add input + Presets buttons.
    auto* footer = new QFrame(inputs_frame);
    footer->setAttribute(Qt::WA_TranslucentBackground, true);
    {
        auto* h = new QHBoxLayout(footer);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(8);

        // Buttons use painted vector glyphs rather than unicode characters —
        // the bundled Inter renders literal "+" / "⊞" with weights and metrics
        // that don't match the design's thin vector marks.
        const QColor glyph_color(255, 255, 255, 215);
        auto* add_button = new QPushButton(QStringLiteral("Add input"), footer);
        add_button->setObjectName(QStringLiteral("SecondaryButton"));
        add_button->setIcon(plus_icon(12, glyph_color));
        add_button->setIconSize(QSize(12, 12));
        add_button->setCursor(Qt::PointingHandCursor);
        connect(add_button, &QPushButton::clicked, this, [this] {
            // Find the first empty slot and prompt for a file.
            int target = -1;
            for (int i = 0; i < slot_count; ++i) {
                if (!controller_->job().inputs[i].populated()) {
                    target = i;
                    break;
                }
            }
            if (target < 0) {
                show_toast_(tr("All four slots are already filled — drop on a slot to replace."));
                return;
            }
            const QString filter
                = tr("Textures (*.png *.tga *.jpg *.jpeg *.exr *.tif *.tiff);;All files (*)");
            const QString chosen
                = QFileDialog::getOpenFileName(this, tr("Open texture"), QString{}, filter);
            if (chosen.isEmpty()) {
                return;
            }
            controller_->load_slot(target, chosen);
        });

        auto* presets_button = new QPushButton(QStringLiteral("Presets"), footer);
        presets_button->setObjectName(QStringLiteral("SecondaryButton"));
        presets_button->setIcon(grid_icon(12, glyph_color));
        presets_button->setIconSize(QSize(12, 12));
        presets_button->setCursor(Qt::PointingHandCursor);
        presets_button->setToolTip(tr("Preset packings (coming soon)"));
        connect(presets_button, &QPushButton::clicked, this, [this] {
            show_toast_(tr("Presets are not wired up yet."));
        });

        h->addWidget(add_button, 1);
        h->addWidget(presets_button, 0);
    }
    inputs_layout->addWidget(footer);

    auto* preview = new PreviewPanel(controller_, this);

    output_panel_ = new OutputPanel(controller_, this);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName(QStringLiteral("MainSplitter"));
    splitter->setHandleWidth(12);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(inputs_frame);
    splitter->addWidget(preview);
    splitter->addWidget(output_panel_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({340, 680, 300});

    inputs_frame->setMinimumWidth(320);
    output_panel_->setMinimumWidth(280);

    // Central area must be opaque and explicitly colored so the dark
    // background reads consistently — on Windows 11 a translucent central
    // widget leaks the desktop through any QMainWindow Mica/acrylic and
    // washes the canvas to a brighter mid-gray.
    auto* central = new QFrame(this);
    central->setObjectName(QStringLiteral("CentralArea"));
    central->setAutoFillBackground(true);
    central->setStyleSheet(QStringLiteral(
        "QFrame#CentralArea { background-color: #06070a; }"));
    auto* central_layout = new QHBoxLayout(central);
    central_layout->setContentsMargins(12, 12, 12, 12);
    central_layout->setSpacing(0);
    central_layout->addWidget(splitter);

    setCentralWidget(central);
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
    QString ext;
    QString filter;
    switch (current_format) {
        case tcp::exporter::Format::PNG:
            ext = QStringLiteral("png");
            filter = tr("PNG (*.png)");
            break;
        case tcp::exporter::Format::TGA:
            ext = QStringLiteral("tga");
            filter = tr("Targa (*.tga)");
            break;
        case tcp::exporter::Format::DDS:
            ext = QStringLiteral("dds");
            filter = tr("DDS (*.dds)");
            break;
    }
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
    // QStatusBar honors padding on itself but children added via addWidget
    // are flush with the bar's left/right edges. Pad them in via the
    // layout's contents margins so the mono text doesn't crowd the window
    // frame.
    sb->setContentsMargins(14, 0, 14, 0);
    sb->setSizeGripEnabled(false);

    auto* populated_label = new QLabel(QStringLiteral("0 / 4 inputs · target ARGB"), sb);
    auto* size_label = new QLabel(QStringLiteral("—"), sb);
    auto* srgb_label = new QLabel(QStringLiteral("sRGB"), sb);
    auto* ready_dot = new QLabel(QStringLiteral("●"), sb);
    ready_dot->setObjectName(QStringLiteral("ReadyDot"));
    auto* ready_text = new QLabel(QStringLiteral("ready"), sb);

    toast_label_ = new QLabel(QString{}, sb);
    toast_label_->setObjectName(QStringLiteral("Toast"));

    sb->addWidget(populated_label);
    sb->addWidget(toast_label_, 1);
    sb->addPermanentWidget(srgb_label);
    sb->addPermanentWidget(size_label);
    sb->addPermanentWidget(ready_dot);
    sb->addPermanentWidget(ready_text);

    connect(controller_, &JobController::populated_slots_changed, populated_label,
            [populated_label](int populated) {
                populated_label->setText(
                    QStringLiteral("%1 / 4 inputs · target ARGB").arg(populated));
            });
    connect(controller_, &JobController::target_size_changed, size_label,
            [size_label](int w, int h) {
                if (w <= 0 || h <= 0) {
                    size_label->setText(QStringLiteral("—"));
                } else {
                    size_label->setText(QStringLiteral("%1 × %2").arg(w).arg(h));
                }
            });
}

} // namespace tcp::app
