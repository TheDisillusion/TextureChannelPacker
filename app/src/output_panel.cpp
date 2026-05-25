#include "output_panel.h"

#include "job_controller.h"

#include "tcp/exporter.h"
#include "tcp/pack_job.h"
#include "tcp/pixel_format.h"
#include "tcp/resize.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace tcp::app {

namespace {

int max_custom_size = 16384;

} // namespace

OutputPanel::OutputPanel(JobController* controller, QWidget* parent)
    : QFrame(parent),
      controller_(controller)
{
    setObjectName(QStringLiteral("OutputPanel"));
    setFrameShape(QFrame::StyledPanel);

    build_ui_();

    connect(controller_, &JobController::target_size_changed, this,
            &OutputPanel::on_target_size_changed_);
    connect(controller_, &JobController::populated_slots_changed, this,
            &OutputPanel::on_populated_slots_changed_);
    connect(controller_, &JobController::export_started, this, &OutputPanel::on_export_started_);
    connect(controller_, &JobController::export_finished, this, &OutputPanel::on_export_finished_);
    connect(controller_, &JobController::export_failed, this, &OutputPanel::on_export_failed_);

    apply_resize_mode_visibility_();
}

void OutputPanel::build_ui_()
{
    auto* title = new QLabel(QStringLiteral("Output"), this);
    title->setObjectName(QStringLiteral("PanelTitle"));

    format_combo_ = new QComboBox(this);
    format_combo_->addItem(QStringLiteral("PNG"), static_cast<int>(exporter::Format::PNG));
    format_combo_->addItem(QStringLiteral("TGA"), static_cast<int>(exporter::Format::TGA));
    connect(format_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &OutputPanel::on_format_changed_);

    bit_depth_combo_ = new QComboBox(this);
    bit_depth_combo_->addItem(QStringLiteral("8-bit"), static_cast<int>(PixelFormat::U8));
    bit_depth_combo_->addItem(QStringLiteral("16-bit"), static_cast<int>(PixelFormat::U16));
    connect(bit_depth_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &OutputPanel::on_bit_depth_changed_);

    resize_mode_combo_ = new QComboBox(this);
    resize_mode_combo_->addItem(QStringLiteral("Largest input"),
                                static_cast<int>(ResizeMode::Largest));
    resize_mode_combo_->addItem(QStringLiteral("Smallest input"),
                                static_cast<int>(ResizeMode::Smallest));
    resize_mode_combo_->addItem(QStringLiteral("First populated"),
                                static_cast<int>(ResizeMode::FirstPopulated));
    resize_mode_combo_->addItem(QStringLiteral("Custom"), static_cast<int>(ResizeMode::Custom));
    connect(resize_mode_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &OutputPanel::on_resize_mode_changed_);

    filter_combo_ = new QComboBox(this);
    filter_combo_->addItem(QStringLiteral("Mitchell-Netravali"),
                           static_cast<int>(ResizeFilter::Mitchell));
    filter_combo_->addItem(QStringLiteral("Lanczos"), static_cast<int>(ResizeFilter::Lanczos));
    filter_combo_->addItem(QStringLiteral("Bilinear"), static_cast<int>(ResizeFilter::Bilinear));
    filter_combo_->addItem(QStringLiteral("Nearest"), static_cast<int>(ResizeFilter::Nearest));
    connect(filter_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &OutputPanel::on_filter_changed_);

    custom_width_ = new QSpinBox(this);
    custom_width_->setRange(1, max_custom_size);
    custom_width_->setValue(1024);
    custom_width_->setSuffix(QStringLiteral(" px"));
    custom_height_ = new QSpinBox(this);
    custom_height_->setRange(1, max_custom_size);
    custom_height_->setValue(1024);
    custom_height_->setSuffix(QStringLiteral(" px"));
    connect(custom_width_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &OutputPanel::on_custom_size_changed_);
    connect(custom_height_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &OutputPanel::on_custom_size_changed_);

    target_label_ = new QLabel(QStringLiteral("(no inputs)"), this);
    target_label_->setObjectName(QStringLiteral("TargetLabel"));

    status_label_ = new QLabel(QString{}, this);
    status_label_->setObjectName(QStringLiteral("StatusLabel"));
    status_label_->setWordWrap(true);

    export_button_ = new QPushButton(QStringLiteral("Export…"), this);
    export_button_->setObjectName(QStringLiteral("ExportButton"));
    export_button_->setEnabled(false);
    connect(export_button_, &QPushButton::clicked, this, &OutputPanel::on_export_clicked_);

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setSpacing(8);
    form->addRow(QStringLiteral("Format"), format_combo_);
    form->addRow(QStringLiteral("Bit depth"), bit_depth_combo_);
    form->addRow(QStringLiteral("Resize mode"), resize_mode_combo_);
    form->addRow(QStringLiteral("Filter"), filter_combo_);

    auto* custom_row = new QHBoxLayout;
    custom_row->addWidget(custom_width_);
    custom_row->addWidget(new QLabel(QStringLiteral("×"), this));
    custom_row->addWidget(custom_height_);
    form->addRow(QStringLiteral("Custom size"), custom_row);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(14, 14, 14, 14);
    main_layout->setSpacing(10);
    main_layout->addWidget(title);
    main_layout->addLayout(form);
    main_layout->addSpacing(6);
    main_layout->addWidget(new QLabel(QStringLiteral("Target"), this));
    main_layout->addWidget(target_label_);
    main_layout->addStretch(1);
    main_layout->addWidget(status_label_);
    main_layout->addWidget(export_button_);
}

void OutputPanel::apply_resize_mode_visibility_()
{
    const bool is_custom = resize_mode_combo_->currentData().toInt()
                           == static_cast<int>(ResizeMode::Custom);
    custom_width_->setEnabled(is_custom);
    custom_height_->setEnabled(is_custom);
}

void OutputPanel::on_format_changed_(int combo_index)
{
    if (combo_index < 0) {
        return;
    }
    const auto fmt = static_cast<exporter::Format>(format_combo_->itemData(combo_index).toInt());
    // TGA only supports 8-bit; keep the UI in sync without nagging the user.
    if (fmt == exporter::Format::TGA) {
        bit_depth_combo_->setCurrentIndex(0);
        bit_depth_combo_->setEnabled(false);
    } else {
        bit_depth_combo_->setEnabled(true);
    }
}

void OutputPanel::on_bit_depth_changed_(int combo_index)
{
    if (combo_index < 0) {
        return;
    }
    const auto pf = static_cast<PixelFormat>(bit_depth_combo_->itemData(combo_index).toInt());
    controller_->set_output_format(pf);
}

void OutputPanel::on_resize_mode_changed_(int combo_index)
{
    if (combo_index < 0) {
        return;
    }
    const auto mode = static_cast<ResizeMode>(resize_mode_combo_->itemData(combo_index).toInt());
    controller_->set_resize_mode(mode);
    apply_resize_mode_visibility_();
}

void OutputPanel::on_filter_changed_(int combo_index)
{
    if (combo_index < 0) {
        return;
    }
    const auto filter = static_cast<ResizeFilter>(filter_combo_->itemData(combo_index).toInt());
    controller_->set_resize_filter(filter);
}

void OutputPanel::on_custom_size_changed_()
{
    controller_->set_custom_size(custom_width_->value(), custom_height_->value());
}

void OutputPanel::on_target_size_changed_(int width, int height)
{
    if (width <= 0 || height <= 0) {
        target_label_->setText(QStringLiteral("(no inputs)"));
    } else {
        target_label_->setText(QStringLiteral("%1 × %2").arg(width).arg(height));
    }
}

void OutputPanel::on_populated_slots_changed_(int populated)
{
    export_button_->setEnabled(populated > 0);
}

void OutputPanel::on_export_clicked_()
{
    const auto current_format = static_cast<exporter::Format>(
        format_combo_->currentData().toInt());
    const QString ext = (current_format == exporter::Format::PNG) ? QStringLiteral("png")
                                                                  : QStringLiteral("tga");
    const QString filter = (current_format == exporter::Format::PNG)
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

void OutputPanel::on_export_started_(const QString& path)
{
    export_button_->setEnabled(false);
    status_label_->setText(tr("Exporting to %1…").arg(path));
}

void OutputPanel::on_export_finished_(const QString& path)
{
    export_button_->setEnabled(controller_->any_slot_populated());
    status_label_->setText(tr("Saved %1").arg(path));
}

void OutputPanel::on_export_failed_(const QString& path, const QString& error)
{
    export_button_->setEnabled(controller_->any_slot_populated());
    status_label_->setText(tr("Export failed: %1").arg(error));
    QMessageBox::warning(this, tr("Export failed"),
                         tr("Could not save %1:\n\n%2").arg(path, error));
}

} // namespace tcp::app
