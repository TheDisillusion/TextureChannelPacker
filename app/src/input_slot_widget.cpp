#include "input_slot_widget.h"

#include "channel_chip.h"
#include "design_tokens.h"
#include "job_controller.h"
#include "segmented_control.h"

#include "tcp/channel_ref.h"
#include "tcp/image.h"

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
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QSizePolicy>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>

namespace tcp::app {

namespace {

constexpr std::array<QChar, output_channel_count> destination_letters{
    QChar('R'), QChar('G'), QChar('B'), QChar('A')
};

constexpr int thumbnail_extent = 48;

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

// ─────────────────────────────────────────────────────────────────────────
// ThumbnailWell
// ─────────────────────────────────────────────────────────────────────────

ThumbnailWell::ThumbnailWell(QWidget* parent)
    : QLabel(parent)
{
    setObjectName(QStringLiteral("Thumbnail"));
    setFixedSize(thumbnail_extent, thumbnail_extent);
    setAlignment(Qt::AlignCenter);
    set_empty(true);
}

void ThumbnailWell::set_empty(bool empty)
{
    if (empty_ == empty) {
        return;
    }
    empty_ = empty;
    setProperty("empty", empty_);
    style()->unpolish(this);
    style()->polish(this);
    if (empty_) {
        setPixmap(QPixmap{});
    }
    update();
}

void ThumbnailWell::paintEvent(QPaintEvent* event)
{
    if (!empty_) {
        // Populated: defer to QLabel's pixmap rendering, but clip into a
        // rounded rect so the corners match the design's 8px radius.
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath path;
        path.addRoundedRect(QRectF(0.5, 0.5, width() - 1.0, height() - 1.0), 8.0, 8.0);
        p.setClipPath(path);
        QLabel::paintEvent(event);
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Diagonal stripe pattern (very subtle).
    const QRectF inside(0.5, 0.5, width() - 1.0, height() - 1.0);
    QPainterPath clip;
    clip.addRoundedRect(inside, 8.0, 8.0);
    p.setClipPath(clip);

    QColor stripe_a(255, 255, 255, 8);
    QColor stripe_b(255, 255, 255, 3);
    const qreal step = 6.0;
    p.setPen(Qt::NoPen);
    for (qreal d = -height(); d < width() + height(); d += step * 2.0) {
        QPainterPath stripe;
        stripe.moveTo(d, 0);
        stripe.lineTo(d + step, 0);
        stripe.lineTo(d + step + height(), height());
        stripe.lineTo(d + height(), height());
        stripe.closeSubpath();
        p.fillPath(stripe, stripe_a);
        QPainterPath stripe2;
        stripe2.moveTo(d + step, 0);
        stripe2.lineTo(d + step * 2.0, 0);
        stripe2.lineTo(d + step * 2.0 + height(), height());
        stripe2.lineTo(d + step + height(), height());
        stripe2.closeSubpath();
        p.fillPath(stripe2, stripe_b);
    }

    p.setClipping(false);

    // Dashed border.
    QPen border(QColor(255, 255, 255, 38));
    border.setStyle(Qt::DashLine);
    border.setWidthF(1.0);
    border.setDashPattern({3.0, 3.0});
    p.setPen(border);
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(inside, 8.0, 8.0);

    // Down-arrow glyph centered.
    QPen glyph(QColor(255, 255, 255, 90));
    glyph.setWidthF(1.2);
    glyph.setCapStyle(Qt::RoundCap);
    glyph.setJoinStyle(Qt::RoundJoin);
    p.setPen(glyph);
    const qreal cx = width() / 2.0;
    const qreal cy = height() / 2.0;
    p.drawLine(QPointF(cx, cy - 5.0), QPointF(cx, cy + 3.0));
    p.drawLine(QPointF(cx - 3.0, cy), QPointF(cx, cy + 3.0));
    p.drawLine(QPointF(cx + 3.0, cy), QPointF(cx, cy + 3.0));
    p.drawLine(QPointF(cx - 5.0, cy + 6.0), QPointF(cx + 5.0, cy + 6.0));
}

// ─────────────────────────────────────────────────────────────────────────
// InputSlotWidget
// ─────────────────────────────────────────────────────────────────────────

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
    destination_chip_ = new ChannelChip(destination_letters[slot_index_], 24, true, this);

    thumbnail_ = new ThumbnailWell(this);

    filename_label_ = new QLabel(QStringLiteral("Drop texture or click to browse"), this);
    filename_label_->setObjectName(QStringLiteral("FilenameLabel"));
    filename_label_->setProperty("empty", true);
    filename_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    filename_label_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    from_label_ = new QLabel(QStringLiteral("FROM"), this);
    from_label_->setObjectName(QStringLiteral("FromLabel"));

    source_picker_ = new SegmentedControl(this);
    source_picker_->set_options(QStringList{"R", "G", "B", "A", "L"});
    source_picker_->set_channel_tint(true);
    source_picker_->set_cell_height(20);
    source_picker_->set_cell_min_width(22);
    // Default selection mirrors the identity channel map set up in JobController.
    source_picker_->set_value(QString(destination_letters[slot_index_]));
    connect(source_picker_, &SegmentedControl::selectionChanged, this,
            &InputSlotWidget::on_source_segment_changed_);

    clear_button_ = new QToolButton(this);
    clear_button_->setObjectName(QStringLiteral("ClearButton"));
    clear_button_->setText(QStringLiteral("×"));
    clear_button_->setToolTip(tr("Clear this slot"));
    clear_button_->setEnabled(false);
    connect(clear_button_, &QToolButton::clicked, this, &InputSlotWidget::on_clear_);

    auto* from_row = new QHBoxLayout;
    from_row->setContentsMargins(0, 0, 0, 0);
    from_row->setSpacing(8);
    from_row->addWidget(from_label_, 0, Qt::AlignVCenter);
    from_row->addWidget(source_picker_, 0, Qt::AlignVCenter);
    from_row->addStretch(1);

    auto* meta = new QVBoxLayout;
    meta->setSpacing(6);
    meta->setContentsMargins(0, 0, 0, 0);
    meta->addWidget(filename_label_);
    meta->addLayout(from_row);

    auto* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(12);
    main_layout->addWidget(destination_chip_, 0, Qt::AlignVCenter);
    main_layout->addWidget(thumbnail_, 0, Qt::AlignVCenter);
    main_layout->addLayout(meta, 1);
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
            source_picker_->set_value(
                QString::fromUtf8(std::string(to_string(ref.source)).c_str()));
        }
        const QString file = QFileInfo(QString::fromStdU16String(s.path.u16string())).fileName();
        filename_label_->setText(file);
        filename_label_->setProperty("empty", false);
        style()->unpolish(filename_label_);
        style()->polish(filename_label_);

        const auto& img = *s.image;
        set_status_tooltip_(QStringLiteral("%1 × %2 · %3 ch · %4")
                                .arg(img.width())
                                .arg(img.height())
                                .arg(img.channels())
                                .arg(QString::fromUtf8(
                                    std::string(to_string(img.format())).c_str())));
        set_thumbnail_from_(QString::fromStdU16String(s.path.u16string()));
    } else {
        filename_label_->setText(QStringLiteral("Drop texture or click to browse"));
        filename_label_->setProperty("empty", true);
        style()->unpolish(filename_label_);
        style()->polish(filename_label_);
        set_status_tooltip_(tr("Drag a texture here or click to browse"));
        thumbnail_->set_empty(true);
    }
}

void InputSlotWidget::set_thumbnail_from_(const QString& path)
{
    QImage img(path);

    if (img.isNull()) {
        const auto& slot = controller_->job().inputs[slot_index_];
        if (slot.populated()) {
            img = thumbnail_from_tcp_image(*slot.image, thumbnail_extent);
        }
    }

    if (img.isNull()) {
        thumbnail_->set_empty(true);
        return;
    }

    // Qt's image plugins may return formats other than 32-bit ARGB
    // (Format_Mono for 1-bit PNGs, Format_Grayscale8, Format_Indexed8, etc.).
    // Normalize so the pixmap is guaranteed to be in a well-supported state
    // regardless of what the source file looked like.
    QImage normalized = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (normalized.isNull()) {
        thumbnail_->set_empty(true);
        return;
    }

    const QImage scaled = normalized.scaled(thumbnail_extent, thumbnail_extent,
                                            Qt::KeepAspectRatioByExpanding,
                                            Qt::SmoothTransformation);
    if (scaled.isNull()) {
        thumbnail_->set_empty(true);
        return;
    }

    // Crop to thumbnail extent (center) so the rounded corners render
    // edge-to-edge.
    const int crop_x = std::max(0, (scaled.width() - thumbnail_extent) / 2);
    const int crop_y = std::max(0, (scaled.height() - thumbnail_extent) / 2);
    const QImage cropped = scaled.copy(crop_x, crop_y, thumbnail_extent, thumbnail_extent);

    thumbnail_->set_empty(false);
    thumbnail_->setPixmap(QPixmap::fromImage(cropped));
}

void InputSlotWidget::set_status_tooltip_(const QString& text)
{
    thumbnail_->setToolTip(text);
    filename_label_->setToolTip(text);
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
        if (thumbnail_->geometry().contains(pos) || filename_label_->geometry().contains(pos)) {
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

void InputSlotWidget::on_source_segment_changed_(const QString& value)
{
    SourceChannel ch = SourceChannel::R;
    if (value == QStringLiteral("G")) ch = SourceChannel::G;
    else if (value == QStringLiteral("B")) ch = SourceChannel::B;
    else if (value == QStringLiteral("A")) ch = SourceChannel::A;
    else if (value == QStringLiteral("L")) ch = SourceChannel::Luminance;
    controller_->set_slot_source_channel(slot_index_, ch);
}

void InputSlotWidget::on_slot_loading_(int slot, const QString& path)
{
    if (slot != slot_index_) {
        return;
    }
    set_status_tooltip_(tr("Loading %1…").arg(QFileInfo(path).fileName()));
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
    set_status_tooltip_(tr("Load failed: %1").arg(error));
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
