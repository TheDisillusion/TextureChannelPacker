#include "output_panel.h"

#include "channel_chip.h"
#include "design_tokens.h"
#include "job_controller.h"
#include "segmented_control.h"

#include "tcp/exporter.h"
#include "tcp/pack_job.h"
#include "tcp/pixel_format.h"
#include "tcp/resize.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStyle>
#include <QVBoxLayout>

namespace tcp::app {

namespace {

constexpr int max_custom_size = 16384;

constexpr std::array<QChar, 4> dst_letters{
    QChar('R'), QChar('G'), QChar('B'), QChar('A')
};

QChar source_letter(SourceChannel ch) noexcept
{
    switch (ch) {
        case SourceChannel::R: return QChar('R');
        case SourceChannel::G: return QChar('G');
        case SourceChannel::B: return QChar('B');
        case SourceChannel::A: return QChar('A');
        case SourceChannel::Luminance: return QChar('L');
    }
    return QChar('R');
}

// Build the small "src pill" used in the target-channel rows.
QLabel* make_src_pill(QChar letter, QWidget* parent)
{
    auto* l = new QLabel(QString(letter), parent);
    QColor base = tokens::channel_color(letter);
    QColor bg = base;
    bg.setAlphaF(0.16f);
    l->setStyleSheet(QStringLiteral(
        "QLabel { background-color: %1; color: %2;"
        " border-radius: 4px; padding: 1px 6px;"
        " font-family: \"JetBrains Mono\",\"Consolas\",monospace;"
        " font-size: 10px; font-weight: 700; font-style: normal; }")
        .arg(bg.name(QColor::HexArgb), base.name()));
    l->setAlignment(Qt::AlignCenter);
    return l;
}

QFrame* make_field_label(const QString& text, QWidget* parent)
{
    Q_UNUSED(parent);
    auto* l = new QLabel(text.toUpper());
    l->setObjectName(QStringLiteral("FieldLabel"));
    auto* host = new QFrame;
    host->setAttribute(Qt::WA_TranslucentBackground, true);
    auto* lay = new QVBoxLayout(host);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(l);
    return host;
}

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
    connect(controller_, &JobController::slot_loaded, this, &OutputPanel::on_slot_loaded_);
    connect(controller_, &JobController::slot_cleared, this, &OutputPanel::on_slot_cleared_);
    connect(controller_, &JobController::channel_map_changed, this,
            &OutputPanel::on_channel_map_changed_);

    connect(controller_, &JobController::output_format_kind_changed, this,
            [this](tcp::exporter::Format fmt) {
                const int idx = format_combo_->findData(static_cast<int>(fmt));
                if (idx >= 0) {
                    QSignalBlocker block(format_combo_);
                    format_combo_->setCurrentIndex(idx);
                    on_format_changed_(idx);
                }
            });
    connect(controller_, &JobController::output_format_changed, this,
            [this](tcp::PixelFormat pf) {
                const QString seg = pf == PixelFormat::U16 ? QStringLiteral("16")
                                                            : QStringLiteral("8");
                bit_depth_segment_->set_value(seg);
            });
    connect(controller_, &JobController::resize_mode_changed, this,
            [this](tcp::ResizeMode mode) {
                const int idx = resize_mode_combo_->findData(static_cast<int>(mode));
                if (idx >= 0) {
                    QSignalBlocker block(resize_mode_combo_);
                    resize_mode_combo_->setCurrentIndex(idx);
                    apply_resize_mode_visibility_();
                }
            });
    connect(controller_, &JobController::resize_filter_changed, this,
            [this](tcp::ResizeFilter f) {
                const int idx = filter_combo_->findData(static_cast<int>(f));
                if (idx >= 0) {
                    QSignalBlocker block(filter_combo_);
                    filter_combo_->setCurrentIndex(idx);
                }
            });
    connect(controller_, &JobController::custom_size_changed, this,
            [this](int w, int h) {
                QSignalBlocker bw(custom_width_);
                QSignalBlocker bh(custom_height_);
                custom_width_->setValue(w);
                custom_height_->setValue(h);
            });
    connect(controller_, &JobController::bc_variant_changed, this,
            [this](tcp::exporter::BcVariant v) {
                const int idx = bc_variant_combo_->findData(static_cast<int>(v));
                if (idx >= 0) {
                    QSignalBlocker block(bc_variant_combo_);
                    bc_variant_combo_->setCurrentIndex(idx);
                }
            });
    connect(controller_, &JobController::flip_vertical_y_changed, this,
            [this](bool flip) {
                QSignalBlocker block(flip_vertical_check_);
                flip_vertical_check_->setChecked(flip);
            });

    apply_resize_mode_visibility_();
    on_format_changed_(format_combo_->currentIndex());

    for (int i = 0; i < 4; ++i) {
        refresh_target_row_(i);
    }
}

void OutputPanel::build_ui_()
{
    auto* title = new QLabel(QStringLiteral("OUTPUT"), this);
    title->setObjectName(QStringLiteral("SectionTitle"));

    format_combo_ = new QComboBox(this);
    format_combo_->addItem(QStringLiteral("PNG"), static_cast<int>(exporter::Format::PNG));
    format_combo_->addItem(QStringLiteral("TGA"), static_cast<int>(exporter::Format::TGA));
    format_combo_->addItem(QStringLiteral("DDS"), static_cast<int>(exporter::Format::DDS));
    connect(format_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &OutputPanel::on_format_changed_);

    bit_depth_segment_ = new SegmentedControl(this);
    bit_depth_segment_->set_options(QStringList{"8", "16"});
    bit_depth_segment_->set_cell_height(22);
    bit_depth_segment_->set_cell_min_width(30);
    bit_depth_segment_->set_value(QStringLiteral("8"));
    connect(bit_depth_segment_, &SegmentedControl::selectionChanged, this,
            &OutputPanel::on_bit_depth_segment_);

    bc_variant_combo_ = new QComboBox(this);
    bc_variant_combo_->addItem(QStringLiteral("BC7 (highest quality)"),
                               static_cast<int>(exporter::BcVariant::BC7));
    bc_variant_combo_->addItem(QStringLiteral("BC3 / DXT5 (color + alpha)"),
                               static_cast<int>(exporter::BcVariant::BC3));
    bc_variant_combo_->addItem(QStringLiteral("BC1 / DXT1 (opaque color)"),
                               static_cast<int>(exporter::BcVariant::BC1));
    bc_variant_combo_->addItem(QStringLiteral("BC5 (two-channel)"),
                               static_cast<int>(exporter::BcVariant::BC5));
    bc_variant_combo_->addItem(QStringLiteral("Uncompressed (RGBA8)"),
                               static_cast<int>(exporter::BcVariant::Uncompressed));
    connect(bc_variant_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &OutputPanel::on_bc_variant_changed_);

    flip_vertical_check_ = new QCheckBox(QStringLiteral("Flip Y (Unity / OpenGL)"), this);
    flip_vertical_check_->setToolTip(tr(
        "Write the DDS bottom-up so engines that don't re-orient on import "
        "(Unity, other GL-UV consumers) display it right-side-up. Leave off "
        "for D3D-native tools and Unreal."));
    flip_vertical_check_->setChecked(controller_->flip_vertical_y());
    connect(flip_vertical_check_, &QCheckBox::toggled, this, [this](bool checked) {
        controller_->set_flip_vertical_y(checked);
    });

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
    custom_width_->setValue(2048);
    custom_width_->setSuffix(QStringLiteral(" px"));
    custom_height_ = new QSpinBox(this);
    custom_height_->setRange(1, max_custom_size);
    custom_height_->setValue(2048);
    custom_height_->setSuffix(QStringLiteral(" px"));
    connect(custom_width_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &OutputPanel::on_custom_size_changed_);
    connect(custom_height_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &OutputPanel::on_custom_size_changed_);

    export_button_ = new QPushButton(QStringLiteral("↓  Export"), this);
    export_button_->setObjectName(QStringLiteral("ExportButton"));
    export_button_->setCursor(Qt::PointingHandCursor);
    connect(export_button_, &QPushButton::clicked, this, &OutputPanel::on_export_clicked_);

    // Subtle accent glow under the button. Disabled by default — the glow
    // is enabled / disabled in sync with the button's enabled state (see
    // on_populated_slots_changed_) so a disabled button reads as flat, not
    // a glowing-but-dim affordance.
    export_glow_ = new QGraphicsDropShadowEffect(this);
    export_glow_->setBlurRadius(12.0);
    export_glow_->setOffset(0.0, 0.0);
    QColor glow = tokens::accent;
    glow.setAlphaF(0.35f);
    export_glow_->setColor(glow);
    export_glow_->setEnabled(false);
    export_button_->setGraphicsEffect(export_glow_);
    export_button_->setEnabled(false);

    // ------------------------------------------------------------------
    // Field helpers — build a [Label + control] vertical group.
    // ------------------------------------------------------------------
    auto make_field = [this](const QString& label_text, QWidget* control) {
        auto* container = new QFrame(this);
        container->setAttribute(Qt::WA_TranslucentBackground, true);
        auto* lay = new QVBoxLayout(container);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(5);
        auto* label = new QLabel(label_text.toUpper(), container);
        label->setObjectName(QStringLiteral("FieldLabel"));
        lay->addWidget(label);
        lay->addWidget(control);
        return container;
    };

    // Custom-size row: spinbox × spinbox.
    auto* custom_row = new QFrame(this);
    custom_row->setAttribute(Qt::WA_TranslucentBackground, true);
    {
        auto* row = new QHBoxLayout(custom_row);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(8);
        row->addWidget(custom_width_, 1);
        auto* x = new QLabel(QStringLiteral("×"), custom_row);
        x->setStyleSheet(QStringLiteral(
            "QLabel { color: rgba(255,255,255,110); font-family: \"JetBrains Mono\", monospace;"
            " font-style: normal; font-weight: 500; }"));
        row->addWidget(x, 0, Qt::AlignVCenter);
        row->addWidget(custom_height_, 1);
    }

    // BC variant + Flip Y row, only visible for DDS.
    auto* dds_block = new QFrame(this);
    dds_block->setAttribute(Qt::WA_TranslucentBackground, true);
    {
        auto* lay = new QVBoxLayout(dds_block);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(8);
        bc_variant_label_ = new QLabel(QStringLiteral("BC VARIANT"), dds_block);
        bc_variant_label_->setObjectName(QStringLiteral("FieldLabel"));
        lay->addWidget(bc_variant_label_);
        lay->addWidget(bc_variant_combo_);
        lay->addWidget(flip_vertical_check_);
    }

    // Target channels — four rows.
    auto* targets_label = new QLabel(QStringLiteral("TARGET CHANNELS"), this);
    targets_label->setObjectName(QStringLiteral("FieldLabel"));

    auto* targets_container = new QFrame(this);
    targets_container->setAttribute(Qt::WA_TranslucentBackground, true);
    auto* targets_layout = new QVBoxLayout(targets_container);
    targets_layout->setContentsMargins(0, 0, 0, 0);
    targets_layout->setSpacing(4);

    for (int i = 0; i < 4; ++i) {
        auto& row = target_rows_[i];
        auto* row_frame = new QFrame(targets_container);
        row_frame->setAttribute(Qt::WA_TranslucentBackground, true);
        auto* h = new QHBoxLayout(row_frame);
        h->setContentsMargins(2, 4, 2, 4);
        h->setSpacing(8);

        row.dst_chip = new ChannelChip(dst_letters[i], 16, false, row_frame);

        row.dst_letter = new QLabel(QString(dst_letters[i]), row_frame);
        row.dst_letter->setStyleSheet(QStringLiteral(
            "QLabel { color: rgba(255,255,255,160); font-size: 11px; font-weight: 700; font-style: normal; }"));

        row.arrow = new QLabel(QStringLiteral("←"), row_frame);
        row.arrow->setStyleSheet(QStringLiteral(
            "QLabel { color: rgba(255,255,255,140); font-size: 11px; font-weight: 500; font-style: normal; }"));

        row.src_pill = make_src_pill(dst_letters[i], row_frame);

        row.filename = new QLabel(QStringLiteral("—"), row_frame);
        row.filename->setStyleSheet(QStringLiteral(
            "QLabel { color: rgba(255,255,255,140); font-size: 11px; font-weight: 500; font-style: normal; }"));
        row.filename->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

        h->addWidget(row.dst_chip, 0, Qt::AlignVCenter);
        h->addWidget(row.dst_letter, 0, Qt::AlignVCenter);
        h->addWidget(row.arrow, 0, Qt::AlignVCenter);
        h->addWidget(row.src_pill, 0, Qt::AlignVCenter);
        h->addWidget(row.filename, 1, Qt::AlignVCenter);

        targets_layout->addWidget(row_frame);
    }

    // Hairline divider between numeric fields and target summary.
    auto* divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("HairlineDivider"));
    divider->setFrameShape(QFrame::HLine);

    // Bit-depth and resize-mode controls need a custom-width wrapper because
    // SegmentedControl's natural sizeHint is its content width and we want it
    // to take a minimum column width for visual balance.
    auto* bit_depth_holder = new QFrame(this);
    bit_depth_holder->setAttribute(Qt::WA_TranslucentBackground, true);
    {
        auto* row = new QHBoxLayout(bit_depth_holder);
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);
        row->addWidget(bit_depth_segment_, 0, Qt::AlignLeft);
        row->addStretch(1);
    }

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(14, 14, 14, 14);
    main_layout->setSpacing(12);
    main_layout->addWidget(title);
    main_layout->addWidget(make_field(QStringLiteral("Format"), format_combo_));
    main_layout->addWidget(make_field(QStringLiteral("Bit depth"), bit_depth_holder));
    main_layout->addWidget(dds_block);
    main_layout->addWidget(make_field(QStringLiteral("Resize mode"), resize_mode_combo_));
    main_layout->addWidget(make_field(QStringLiteral("Filter"), filter_combo_));
    main_layout->addWidget(make_field(QStringLiteral("Custom size"), custom_row));
    main_layout->addWidget(divider);
    main_layout->addWidget(targets_label);
    main_layout->addWidget(targets_container);
    main_layout->addStretch(1);
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
    controller_->set_output_format_kind(fmt);

    // TGA and DDS only support 8-bit in this revision.
    if (fmt == exporter::Format::TGA || fmt == exporter::Format::DDS) {
        bit_depth_segment_->set_value(QStringLiteral("8"));
        bit_depth_segment_->setEnabled(false);
    } else {
        bit_depth_segment_->setEnabled(true);
    }

    const bool is_dds = (fmt == exporter::Format::DDS);
    bc_variant_combo_->setVisible(is_dds);
    if (bc_variant_label_) {
        bc_variant_label_->setVisible(is_dds);
    }
    if (flip_vertical_check_) {
        flip_vertical_check_->setVisible(is_dds);
    }
}

void OutputPanel::on_bit_depth_segment_(const QString& value)
{
    const PixelFormat pf = (value == QStringLiteral("16")) ? PixelFormat::U16 : PixelFormat::U8;
    controller_->set_output_format(pf);
}

void OutputPanel::on_bc_variant_changed_(int combo_index)
{
    if (combo_index < 0) {
        return;
    }
    const auto v = static_cast<exporter::BcVariant>(
        bc_variant_combo_->itemData(combo_index).toInt());
    controller_->set_bc_variant(v);
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

void OutputPanel::on_target_size_changed_(int /*width*/, int /*height*/)
{
    // Size summary is rendered in the main-window status bar.
}

void OutputPanel::on_populated_slots_changed_(int populated)
{
    const bool enabled = populated > 0;
    export_button_->setEnabled(enabled);
    if (export_glow_) {
        export_glow_->setEnabled(enabled);
    }
}

void OutputPanel::on_slot_loaded_(int slot)
{
    refresh_target_row_(slot);
}

void OutputPanel::on_slot_cleared_(int slot)
{
    refresh_target_row_(slot);
}

void OutputPanel::on_channel_map_changed_(int destination, tcp::ChannelRef ref)
{
    Q_UNUSED(ref);
    if (destination < 0 || destination >= static_cast<int>(target_rows_.size())) {
        return;
    }
    refresh_target_row_(destination);
}

void OutputPanel::refresh_target_row_(int destination)
{
    if (destination < 0 || destination >= static_cast<int>(target_rows_.size())) {
        return;
    }
    const auto& ref = controller_->job().channel_map[destination];
    auto& row = target_rows_[destination];

    QChar src = source_letter(ref.source);
    // Replace the src pill — easiest way to apply the new tint colors.
    auto* parent = row.src_pill->parentWidget();
    auto* layout = qobject_cast<QHBoxLayout*>(parent ? parent->layout() : nullptr);
    if (layout) {
        const int idx = layout->indexOf(row.src_pill);
        delete row.src_pill;
        row.src_pill = make_src_pill(src, parent);
        layout->insertWidget(idx, row.src_pill, 0, Qt::AlignVCenter);
    }

    QString file_text = QStringLiteral("—");
    bool has_file = false;
    if (ref.is_set() && ref.slot_index >= 0 && ref.slot_index < 4) {
        const auto& slot = controller_->job().inputs[ref.slot_index];
        if (slot.populated()) {
            file_text = QFileInfo(QString::fromStdU16String(slot.path.u16string())).fileName();
            has_file = true;
        }
    }
    row.filename->setText(file_text);
    QColor color = has_file ? tokens::text : tokens::text_mute;
    row.filename->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; font-weight: 500; font-style: normal; }")
        .arg(color.name(QColor::HexArgb)));
}

void OutputPanel::on_export_clicked_()
{
    const auto current_format = controller_->output_format_kind();
    QString ext;
    QString filter;
    switch (current_format) {
        case exporter::Format::PNG:
            ext = QStringLiteral("png");
            filter = tr("PNG (*.png)");
            break;
        case exporter::Format::TGA:
            ext = QStringLiteral("tga");
            filter = tr("Targa (*.tga)");
            break;
        case exporter::Format::DDS:
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

void OutputPanel::on_export_started_(const QString& /*path*/)
{
    export_button_->setEnabled(false);
    if (export_glow_) {
        export_glow_->setEnabled(false);
    }
}

void OutputPanel::on_export_finished_(const QString& /*path*/)
{
    const bool enabled = controller_->any_slot_populated();
    export_button_->setEnabled(enabled);
    if (export_glow_) {
        export_glow_->setEnabled(enabled);
    }
}

void OutputPanel::on_export_failed_(const QString& path, const QString& error)
{
    const bool enabled = controller_->any_slot_populated();
    export_button_->setEnabled(enabled);
    if (export_glow_) {
        export_glow_->setEnabled(enabled);
    }
    QMessageBox::warning(this, tr("Export failed"),
                         tr("Could not save %1:\n\n%2").arg(path, error));
}

bool OutputPanel::event(QEvent* event)
{
    if (export_glow_ && export_button_) {
        if (event->type() == QEvent::HoverEnter || event->type() == QEvent::Enter) {
            // No-op: hover is tracked by export_button_ itself via its own
            // hover events. We intercept here so derived effects could be
            // staged in the future without an extra event filter install.
        }
    }
    return QFrame::event(event);
}

} // namespace tcp::app
