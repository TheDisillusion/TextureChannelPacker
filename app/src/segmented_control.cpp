#include "segmented_control.h"

#include "design_tokens.h"

#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>

namespace tcp::app {

namespace {

constexpr int dot_extent = 6;
constexpr int cell_hpad = 10;
constexpr int dot_gap = 6;

} // namespace

SegmentedControl::SegmentedControl(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setAttribute(Qt::WA_Hover, true);
    QFont f = font();
    f.setFamilies(tokens::mono_font_families);
    f.setPointSizeF(10.0);
    f.setWeight(QFont::Bold);
    f.setItalic(false);
    f.setStyle(QFont::StyleNormal);
    setFont(f);
}

void SegmentedControl::set_options(const QStringList& options)
{
    options_ = options;
    if (active_index_ >= options_.size()) {
        active_index_ = options_.isEmpty() ? 0 : 0;
    }
    updateGeometry();
    update();
}

void SegmentedControl::set_channel_tint(bool on)
{
    if (channel_tint_ == on) {
        return;
    }
    channel_tint_ = on;
    updateGeometry();
    update();
}

void SegmentedControl::set_cell_height(int h)
{
    if (cell_height_ == h) {
        return;
    }
    cell_height_ = qMax(16, h);
    updateGeometry();
    update();
}

void SegmentedControl::set_cell_min_width(int w)
{
    if (cell_min_width_ == w) {
        return;
    }
    cell_min_width_ = qMax(16, w);
    updateGeometry();
    update();
}

QString SegmentedControl::value() const noexcept
{
    if (active_index_ < 0 || active_index_ >= options_.size()) {
        return {};
    }
    return options_.at(active_index_);
}

void SegmentedControl::set_value(const QString& value, bool emit_signal)
{
    const int idx = index_of(value);
    if (idx < 0 || idx == active_index_) {
        return;
    }
    active_index_ = idx;
    update();
    if (emit_signal) {
        emit selectionChanged(value);
    }
}

int SegmentedControl::index_of(const QString& value) const noexcept
{
    for (int i = 0; i < options_.size(); ++i) {
        if (options_.at(i).compare(value, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

QSize SegmentedControl::sizeHint() const
{
    if (options_.isEmpty()) {
        return QSize(0, cell_height_ + 2 * track_padding_);
    }
    const QFontMetrics fm(font());
    int total = 2 * track_padding_;
    for (const auto& o : options_) {
        int w = fm.horizontalAdvance(o) + 2 * cell_hpad;
        if (channel_tint_) {
            w += dot_extent + dot_gap;
        }
        total += qMax(w, cell_min_width_);
    }
    return QSize(total, cell_height_ + 2 * track_padding_);
}

QSize SegmentedControl::minimumSizeHint() const
{
    return sizeHint();
}

QRectF SegmentedControl::cell_rect_(int index) const noexcept
{
    const QFontMetrics fm(font());
    qreal x = track_padding_;
    for (int i = 0; i < options_.size(); ++i) {
        int w = fm.horizontalAdvance(options_.at(i)) + 2 * cell_hpad;
        if (channel_tint_) {
            w += dot_extent + dot_gap;
        }
        w = qMax(w, cell_min_width_);
        if (i == index) {
            return QRectF(x, track_padding_, w, cell_height_);
        }
        x += w;
    }
    return {};
}

int SegmentedControl::cell_at_(const QPoint& pos) const noexcept
{
    for (int i = 0; i < options_.size(); ++i) {
        if (cell_rect_(i).contains(pos)) {
            return i;
        }
    }
    return -1;
}

void SegmentedControl::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF track(0.5, 0.5, width() - 1.0, height() - 1.0);
    const qreal radius = track.height() / 2.0;

    // Sunken track with hairline border.
    p.setPen(QPen(tokens::hairline, 0.5));
    p.setBrush(tokens::sunken);
    p.drawRoundedRect(track, radius, radius);

    // Active pill.
    if (active_index_ >= 0 && active_index_ < options_.size()) {
        QRectF pill = cell_rect_(active_index_);
        // Inset 0.5px to keep the inner highlight crisp against the track.
        pill.adjust(0.5, 0.5, -0.5, -0.5);
        const qreal pr = pill.height() / 2.0;
        QColor border = tokens::raised.lighter(115);
        border.setAlphaF(0.18f);
        p.setPen(QPen(border, 0.5));
        p.setBrush(tokens::raised);
        p.drawRoundedRect(pill, pr, pr);
        // Inner top highlight.
        QColor highlight(255, 255, 255, 18);
        p.setPen(QPen(highlight, 0.5));
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(pill.left() + pr * 0.5, pill.top() + 0.5),
                   QPointF(pill.right() - pr * 0.5, pill.top() + 0.5));
    }

    // Labels.
    const QFontMetrics fm(font());
    for (int i = 0; i < options_.size(); ++i) {
        const QRectF cell = cell_rect_(i);
        const bool active = i == active_index_;
        const bool hover = !active && i == hover_index_;

        QColor text = active ? tokens::text : (hover ? tokens::text : tokens::text_dim);

        qreal text_left = cell.left() + cell_hpad;

        if (channel_tint_) {
            // 6px dot before the label.
            const QChar letter = options_.at(i).isEmpty() ? QChar{} : options_.at(i).at(0);
            const QColor base = tokens::channel_color(letter);
            QColor fill = tokens::srgb_mix(base, 0.30, tokens::chip_ink);
            if (!active) {
                // Inactive: 50% mix between the active fill and transparent.
                fill.setAlphaF(fill.alphaF() * 0.55f);
            }
            const QPointF dot_center(cell.left() + cell_hpad + dot_extent / 2.0,
                                     cell.center().y());
            QColor dot_border = base;
            dot_border.setAlphaF(active ? 0.55f : 0.30f);
            p.setPen(QPen(dot_border, 0.5));
            p.setBrush(fill);
            p.drawEllipse(dot_center, dot_extent / 2.0, dot_extent / 2.0);
            text_left += dot_extent + dot_gap;
        }

        const QRectF text_rect(text_left, cell.top(),
                               cell.right() - text_left - cell_hpad * 0.5, cell.height());
        p.setPen(text);
        p.drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft, options_.at(i));
    }
}

void SegmentedControl::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    const int idx = cell_at_(event->pos());
    if (idx < 0 || idx == active_index_) {
        return;
    }
    active_index_ = idx;
    update();
    emit selectionChanged(options_.at(idx));
}

void SegmentedControl::mouseMoveEvent(QMouseEvent* event)
{
    const int idx = cell_at_(event->pos());
    if (idx != hover_index_) {
        hover_index_ = idx;
        update();
    }
}

void SegmentedControl::leaveEvent(QEvent* /*event*/)
{
    if (hover_index_ != -1) {
        hover_index_ = -1;
        update();
    }
}

} // namespace tcp::app
