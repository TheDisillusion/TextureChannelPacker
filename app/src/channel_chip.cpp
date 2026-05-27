#include "channel_chip.h"

#include "design_tokens.h"

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>

namespace tcp::app {

ChannelChip::ChannelChip(QChar letter, int diameter, bool with_label, QWidget* parent)
    : QWidget(parent),
      letter_(letter),
      diameter_(diameter),
      with_label_(with_label)
{
    setFixedSize(diameter_, diameter_);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::NoFocus);
}

void ChannelChip::set_letter(QChar letter)
{
    if (letter_ == letter) {
        return;
    }
    letter_ = letter;
    update();
}

void ChannelChip::set_diameter(int diameter)
{
    if (diameter_ == diameter) {
        return;
    }
    diameter_ = diameter;
    setFixedSize(diameter_, diameter_);
    update();
}

void ChannelChip::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QColor base = tokens::channel_color(letter_);
    const QColor fill = tokens::srgb_mix(base, 0.30, tokens::chip_ink);
    QColor border = base;
    border.setAlphaF(0.55f);

    const QRectF r(0.5, 0.5, diameter_ - 1.0, diameter_ - 1.0);

    p.setPen(QPen(border, 0.5));
    p.setBrush(fill);
    p.drawEllipse(r);

    if (with_label_ && !letter_.isNull()) {
        QFont f = font();
        f.setFamilies(tokens::mono_font_families);
        f.setPointSizeF(qMax(6.0, diameter_ * 0.36));
        f.setWeight(QFont::Bold);
        f.setItalic(false);
        f.setStyle(QFont::StyleNormal);
        p.setFont(f);
        p.setPen(base);
        p.drawText(rect(), Qt::AlignCenter, QString(letter_.toUpper()));
    }
}

} // namespace tcp::app
