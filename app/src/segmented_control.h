#pragma once

#include <QStringList>
#include <QWidget>

#include <vector>

namespace tcp::app {

// Pill-shaped exclusive segmented control. Holds N text options; one is
// active at a time. Replaces small dropdowns and tool-button groups across
// the redesigned UI (per-slot source channel picker, preview view-mode
// picker, zoom picker, bit-depth picker).
//
// The control owns its visual rendering (rounded track, active pill, hover
// tints, optional channel-tint dots) via paintEvent rather than QSS because
// the active-pill needs to translate cleanly across an even subdivision of
// the track regardless of label widths.
class SegmentedControl : public QWidget
{
    Q_OBJECT

public:
    explicit SegmentedControl(QWidget* parent = nullptr);

    // Replace the options. Each option is a short label (usually one letter
    // or a short word). The control resizes itself to its preferred width.
    void set_options(const QStringList& options);

    // Enable a small 6px channel-tint dot before each label. The dot color
    // is derived from the option letter (R/G/B/A/L); options whose first
    // char isn't a channel letter render without a dot.
    void set_channel_tint(bool on);

    // Pixel size for an individual cell. Defaults to height 22, min cell
    // width based on font metrics.
    void set_cell_height(int h);
    void set_cell_min_width(int w);

    [[nodiscard]] QString value() const noexcept;
    void set_value(const QString& value, bool emit_signal = false);

    [[nodiscard]] int index_of(const QString& value) const noexcept;

signals:
    void selectionChanged(const QString& value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    [[nodiscard]] QRectF cell_rect_(int index) const noexcept;
    [[nodiscard]] int cell_at_(const QPoint& pos) const noexcept;

    QStringList options_;
    int active_index_ = 0;
    int hover_index_ = -1;
    bool channel_tint_ = false;
    int cell_height_ = 22;
    int cell_min_width_ = 26;
    int track_padding_ = 2;
};

} // namespace tcp::app
