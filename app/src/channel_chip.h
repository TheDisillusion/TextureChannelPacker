#pragma once

#include <QWidget>

namespace tcp::app {

// Small circular badge that displays one channel letter (R/G/B/A/L) using
// the design's channel-chip recipe: the channel color blended into a near
// black, with a soft tinted border and the original channel color as text.
//
// Used at 16px (target rows, preview badge), 22px (default), 24px (input
// slot destination indicator) and 6px (segmented tint dots, with_label=false).
class ChannelChip : public QWidget
{
    Q_OBJECT

public:
    ChannelChip(QChar letter, int diameter, bool with_label, QWidget* parent = nullptr);

    void set_letter(QChar letter);
    [[nodiscard]] QChar letter() const noexcept { return letter_; }

    void set_diameter(int diameter);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QChar letter_;
    int diameter_;
    bool with_label_;
};

} // namespace tcp::app
