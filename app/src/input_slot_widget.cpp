#include "input_slot_widget.h"

#include "job_controller.h"

#include "tcp/channel_ref.h"
#include "tcp/image.h"

#include <QComboBox>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>

namespace tcp::app {

namespace {

constexpr std::array<const char*, output_channel_count> destination_names{"R", "G", "B", "A"};
constexpr std::array<const char*, output_channel_count> destination_colors{
    "#e85b5b", // R
    "#5be877", // G
    "#5b8be8", // B
    "#cccccc"  // A
};

constexpr int thumbnail_extent = 56;

// Build an RGBA8 thumbnail directly from the in-memory tcp::Image. Used as a
// fallback when Qt's QImage(path) returns null — which happens with 16-bit
// PNGs, certain ICC profiles, EXR/TGA without the right plugins, or just any
// time the Qt image plugins misbehave. Slow path (per-pixel sampling), but
// it's only run at thumbnail size and only on slot-load.
QImage thumbnail_from_tcp_image(const tcp::Image& img, int max_extent)
{
    if (img.empty() || max_extent <= 0) {
        return QImage{};
    }

    int dst_w = 0;
    int dst_h = 0;
    if (img.width() >= img.height()) {
        dst_w = max_extent;
        dst_h = std::max(1, static_cast<int>(std::lround(
                                static_cast<double>(max_extent) * img.height() / img.width())));
    } else {
        dst_h = max_extent;
        dst_w = std::max(1, static_cast<int>(std::lround(
                                static_cast<double>(max_extent) * img.width() / img.height())));
    }

    QImage out(dst_w, dst_h, QImage::Format_RGBA8888);
    const int channels = img.channels();
    for (int y = 0; y < dst_h; ++y) {
        auto* row = out.scanLine(y);
        const int sy = std::min(img.height() - 1,
                                static_cast<int>(static_cast<double>(y) / dst_h * img.height()));
        for (int x = 0; x < dst_w; ++x) {
            const int sx = std::min(img.width() - 1,
                                    static_cast<int>(static_cast<double>(x) / dst_w * img.width()));
            auto clamp_u8 = [](float v) {
                v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
                return static_cast<unsigned char>(v * 255.0f + 0.5f);
            };
            const unsigned char r = clamp_u8(img.sample_linear(sx, sy, 0));
            const unsigned char g = channels > 1 ? clamp_u8(img.sample_linear(sx, sy, 1)) : r;
            const unsigned char b = channels > 2 ? clamp_u8(img.sample_linear(sx, sy, 2)) : r;
            const unsigned char a = channels > 3 ? clamp_u8(img.sample_linear(sx, sy, 3)) : 255;
            row[x * 4 + 0] = r;
            row[x * 4 + 1] = g;
            row[x * 4 + 2] = b;
            row[x * 4 + 3] = a;
        }
    }
    return out;
}

} // namespace

InputSlotWidget::InputSlotWidget(int slot_index, JobController* controller, QWidget* parent)
    : QFrame(parent),
      slot_index_(slot_index),
      controller_(controller)
{
    setObjectName(QStringLiteral("InputSlot"));
    setFrameShape(QFrame::StyledPanel);
    setAcceptDrops(true);
    setFocusPolicy(Qt::ClickFocus);

    build_ui_();

    connect(controller_, &JobController::slot_loading, this, &InputSlotWidget::on_slot_loading_);
    connect(controller_, &JobController::slot_loaded, this, &InputSlotWidget::on_slot_loaded_);
    connect(controller_, &JobController::slot_load_failed, this,
            &InputSlotWidget::on_slot_load_failed_);
    connect(controller_, &JobController::slot_cleared, this, &InputSlotWidget::on_slot_cleared_);

    refresh_state_for_(slot_index_);
}

void InputSlotWidget::build_ui_()
{
    destination_badge_ = new QLabel(QString::fromUtf8(destination_names[slot_index_]), this);
    destination_badge_->setObjectName(QStringLiteral("DestinationBadge"));
    destination_badge_->setAlignment(Qt::AlignCenter);
    destination_badge_->setFixedSize(28, 28);
    destination_badge_->setStyleSheet(
        QStringLiteral("QLabel#DestinationBadge { background-color: %1; color: #1a1a1a; "
                       "border-radius: 14px; font-weight: 700; }")
            .arg(QString::fromUtf8(destination_colors[slot_index_])));

    thumbnail_ = new QLabel(this);
    thumbnail_->setObjectName(QStringLiteral("Thumbnail"));
    thumbnail_->setFixedSize(thumbnail_extent, thumbnail_extent);
    thumbnail_->setAlignment(Qt::AlignCenter);
    thumbnail_->setText(QStringLiteral("Drop"));

    filename_label_ = new QLabel(QStringLiteral("— empty —"), this);
    filename_label_->setObjectName(QStringLiteral("FilenameLabel"));
    filename_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    status_label_ = new QLabel(QStringLiteral("Drag a texture here or click to browse"), this);
    status_label_->setObjectName(QStringLiteral("StatusLabel"));

    source_channel_combo_ = new QComboBox(this);
    source_channel_combo_->setObjectName(QStringLiteral("SourceChannelCombo"));
    source_channel_combo_->addItem(QStringLiteral("Source: R"), static_cast<int>(SourceChannel::R));
    source_channel_combo_->addItem(QStringLiteral("Source: G"), static_cast<int>(SourceChannel::G));
    source_channel_combo_->addItem(QStringLiteral("Source: B"), static_cast<int>(SourceChannel::B));
    source_channel_combo_->addItem(QStringLiteral("Source: A"), static_cast<int>(SourceChannel::A));
    source_channel_combo_->addItem(QStringLiteral("Source: Luminance"),
                                   static_cast<int>(SourceChannel::Luminance));
    // Default selection mirrors the identity channel map set up in JobController.
    source_channel_combo_->setCurrentIndex(slot_index_);
    connect(source_channel_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &InputSlotWidget::on_source_channel_changed_);

    clear_button_ = new QToolButton(this);
    clear_button_->setObjectName(QStringLiteral("ClearButton"));
    clear_button_->setText(QStringLiteral("×")); // multiplication sign
    clear_button_->setToolTip(tr("Clear this slot"));
    clear_button_->setEnabled(false);
    connect(clear_button_, &QToolButton::clicked, this, &InputSlotWidget::on_clear_);

    auto* details_layout = new QVBoxLayout;
    details_layout->setSpacing(2);
    details_layout->setContentsMargins(0, 0, 0, 0);
    details_layout->addWidget(filename_label_);
    details_layout->addWidget(status_label_);
    details_layout->addWidget(source_channel_combo_);

    auto* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(10, 8, 10, 8);
    main_layout->setSpacing(10);
    main_layout->addWidget(destination_badge_, 0, Qt::AlignTop);
    main_layout->addWidget(thumbnail_, 0, Qt::AlignTop);
    main_layout->addLayout(details_layout, 1);
    main_layout->addWidget(clear_button_, 0, Qt::AlignTop);
}

void InputSlotWidget::refresh_state_for_(int slot)
{
    if (slot != slot_index_) {
        return;
    }
    const auto& s = controller_->job().inputs[slot];
    const bool populated = s.populated();
    clear_button_->setEnabled(populated);
    if (populated) {
        const auto& ref = controller_->job().channel_map[slot_index_];
        if (ref.slot_index == slot_index_) {
            source_channel_combo_->setCurrentIndex(static_cast<int>(ref.source));
        }
        filename_label_->setText(
            QFileInfo(QString::fromStdU16String(s.path.u16string())).fileName());
        const auto& img = *s.image;
        set_status_text_(QStringLiteral("%1 × %2 · %3 ch · %4")
                             .arg(img.width())
                             .arg(img.height())
                             .arg(img.channels())
                             .arg(QString::fromUtf8(std::string(to_string(img.format())).c_str())));
        set_thumbnail_from_(QString::fromStdU16String(s.path.u16string()));
    } else {
        filename_label_->setText(QStringLiteral("— empty —"));
        set_status_text_(QStringLiteral("Drag a texture here or click to browse"));
        thumbnail_->setPixmap(QPixmap{});
        thumbnail_->setText(QStringLiteral("Drop"));
    }
}

void InputSlotWidget::set_thumbnail_from_(const QString& path)
{
    // Try Qt's image decoder first — fast for common formats.
    QImage img(path);

    // Fallback: build the thumbnail from the tcp::Image OIIO already
    // decoded. Covers 16-bit / float PNG, EXR, TGA, and any case where
    // Qt's imageformats plugins aren't doing the job.
    if (img.isNull()) {
        const auto& slot = controller_->job().inputs[slot_index_];
        if (slot.populated()) {
            img = thumbnail_from_tcp_image(*slot.image, thumbnail_extent);
        }
    }

    if (img.isNull()) {
        thumbnail_->setPixmap(QPixmap{});
        thumbnail_->setText(QStringLiteral("?"));
        return;
    }

    const QPixmap pm = QPixmap::fromImage(
        img.scaled(thumbnail_extent, thumbnail_extent,
                   Qt::KeepAspectRatio, Qt::SmoothTransformation));
    thumbnail_->setPixmap(pm);
    thumbnail_->setText(QString{});
}

void InputSlotWidget::set_status_text_(const QString& text)
{
    status_label_->setText(text);
}

void InputSlotWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        setProperty("dragOver", true);
        style()->unpolish(this);
        style()->polish(this);
        event->acceptProposedAction();
    }
}

void InputSlotWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    setProperty("dragOver", false);
    style()->unpolish(this);
    style()->polish(this);
    event->accept();
}

void InputSlotWidget::dropEvent(QDropEvent* event)
{
    setProperty("dragOver", false);
    style()->unpolish(this);
    style()->polish(this);
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) {
        return;
    }
    const QString local = urls.first().toLocalFile();
    if (local.isEmpty()) {
        return;
    }
    current_path_ = local;
    controller_->load_slot(slot_index_, local);
    event->acceptProposedAction();
}

void InputSlotWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const QPoint pos = event->pos();
        if (thumbnail_->geometry().contains(pos) || filename_label_->geometry().contains(pos)
            || status_label_->geometry().contains(pos)) {
            on_browse_();
            return;
        }
    }
    QFrame::mousePressEvent(event);
}

void InputSlotWidget::on_browse_()
{
    const QString filter = tr("Textures (*.png *.tga *.jpg *.jpeg *.exr *.tif *.tiff);;All files (*)");
    const QString chosen = QFileDialog::getOpenFileName(this, tr("Open texture"), current_path_,
                                                        filter);
    if (chosen.isEmpty()) {
        return;
    }
    current_path_ = chosen;
    controller_->load_slot(slot_index_, chosen);
}

void InputSlotWidget::on_clear_()
{
    controller_->clear_slot(slot_index_);
}

void InputSlotWidget::on_source_channel_changed_(int combo_index)
{
    if (combo_index < 0) {
        return;
    }
    const SourceChannel ch = static_cast<SourceChannel>(
        source_channel_combo_->itemData(combo_index).toInt());
    controller_->set_slot_source_channel(slot_index_, ch);
}

void InputSlotWidget::on_slot_loading_(int slot, const QString& path)
{
    if (slot != slot_index_) {
        return;
    }
    set_status_text_(tr("Loading %1…").arg(QFileInfo(path).fileName()));
}

void InputSlotWidget::on_slot_loaded_(int slot)
{
    refresh_state_for_(slot);
}

void InputSlotWidget::on_slot_load_failed_(int slot, const QString& error)
{
    if (slot != slot_index_) {
        return;
    }
    set_status_text_(tr("Load failed: %1").arg(error));
}

void InputSlotWidget::on_slot_cleared_(int slot)
{
    if (slot != slot_index_) {
        return;
    }
    current_path_.clear();
    refresh_state_for_(slot);
}

} // namespace tcp::app
