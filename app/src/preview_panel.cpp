#include "preview_panel.h"

#include "channel_chip.h"
#include "design_tokens.h"
#include "job_controller.h"
#include "preview_widget.h"
#include "segmented_control.h"

#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QResizeEvent>
#include <QShortcut>
#include <QShowEvent>
#include <QVBoxLayout>

namespace tcp::app {

PreviewPanel::PreviewPanel(JobController* controller, QWidget* parent)
    : QFrame(parent),
      controller_(controller)
{
    setObjectName(QStringLiteral("PreviewPanel"));
    setFrameShape(QFrame::StyledPanel);
    build_ui_();
}

void PreviewPanel::build_ui_()
{
    auto* title = new QLabel(QStringLiteral("PREVIEW"), this);
    title->setObjectName(QStringLiteral("SectionTitle"));

    view_segment_ = new SegmentedControl(this);
    view_segment_->set_options(QStringList{"RGB", "R", "G", "B", "A"});
    view_segment_->set_channel_tint(true);
    view_segment_->set_cell_height(22);
    view_segment_->set_cell_min_width(28);
    view_segment_->set_value(QStringLiteral("RGB"));

    zoom_segment_ = new SegmentedControl(this);
    zoom_segment_->set_options(QStringList{"Fit", "1:1", "100%"});
    zoom_segment_->set_cell_height(22);
    zoom_segment_->set_cell_min_width(40);
    zoom_segment_->set_value(QStringLiteral("Fit"));

    checker_check_ = new QCheckBox(QStringLiteral("Checker"), this);
    checker_check_->setChecked(true);
    checker_check_->setToolTip(tr("Show a checkerboard behind translucent regions"));

    // The preview surface — frames the OpenGL widget with a 10px radius
    // hairline border and sunken background. The OpenGL widget paints on
    // top; the channel badge floats over the top-left when a single
    // channel is being viewed.
    surface_ = new QFrame(this);
    surface_->setObjectName(QStringLiteral("PreviewSurface"));
    surface_->setMinimumSize(320, 240);
    surface_->installEventFilter(this);

    preview_ = new PreviewWidget(controller_, surface_);
    preview_->setMinimumSize(320, 240);

    auto* surface_layout = new QVBoxLayout(surface_);
    surface_layout->setContentsMargins(1, 1, 1, 1);
    surface_layout->setSpacing(0);
    surface_layout->addWidget(preview_);

    // Channel badge — small pill in the top-left of the surface. Shown
    // only when view mode is not RGB.
    channel_badge_ = new QFrame(surface_);
    channel_badge_->setObjectName(QStringLiteral("ChannelBadge"));
    channel_badge_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    auto* badge_layout = new QHBoxLayout(channel_badge_);
    badge_layout->setContentsMargins(4, 3, 8, 3);
    badge_layout->setSpacing(6);
    channel_badge_dot_ = new ChannelChip(QChar('R'), 14, false, channel_badge_);
    channel_badge_label_ = new QLabel(QStringLiteral("R channel"), channel_badge_);
    channel_badge_label_->setObjectName(QStringLiteral("ChannelBadgeText"));
    badge_layout->addWidget(channel_badge_dot_);
    badge_layout->addWidget(channel_badge_label_);
    channel_badge_->hide();

    // Wire up.
    connect(view_segment_, &SegmentedControl::selectionChanged, this,
            [this](const QString& v) {
                if (v == QStringLiteral("RGB")) apply_view_(PreviewViewMode::RGB);
                else if (v == QStringLiteral("R")) apply_view_(PreviewViewMode::R);
                else if (v == QStringLiteral("G")) apply_view_(PreviewViewMode::G);
                else if (v == QStringLiteral("B")) apply_view_(PreviewViewMode::B);
                else if (v == QStringLiteral("A")) apply_view_(PreviewViewMode::A);
                update_caption_();
            });
    connect(zoom_segment_, &SegmentedControl::selectionChanged, this,
            [this](const QString& v) {
                if (v == QStringLiteral("Fit"))   preview_->fit_to_window();
                else if (v == QStringLiteral("1:1"))  preview_->set_one_to_one();
                else if (v == QStringLiteral("100%")) preview_->set_zoom(1.0f);
                update_caption_();
            });
    connect(checker_check_, &QCheckBox::toggled, preview_, &PreviewWidget::set_checkerboard);
    connect(preview_, &PreviewWidget::zoom_changed, this, [this](float /*z*/) {
        update_caption_();
    });
    connect(controller_, &JobController::target_size_changed, this, [this](int, int) {
        update_caption_();
    });
    connect(controller_, &JobController::populated_slots_changed, this, [this](int) {
        update_caption_();
    });

    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(10);
    toolbar->addWidget(title);
    toolbar->addStretch(1);
    toolbar->addWidget(view_segment_);
    toolbar->addWidget(zoom_segment_);
    toolbar->addWidget(checker_check_);

    // Mono caption below the surface: size · color profile on the left,
    // "viewing X @ Zoom" on the right.
    size_caption_ = new QLabel(QStringLiteral("— · sRGB"), this);
    size_caption_->setObjectName(QStringLiteral("MonoCaption"));
    view_caption_ = new QLabel(QStringLiteral("no source"), this);
    view_caption_->setObjectName(QStringLiteral("MonoCaption"));
    view_caption_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto* caption_row = new QHBoxLayout;
    caption_row->setContentsMargins(0, 0, 0, 0);
    caption_row->setSpacing(8);
    caption_row->addWidget(size_caption_, 1);
    caption_row->addWidget(view_caption_, 0);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(14, 14, 14, 14);
    main_layout->setSpacing(10);
    main_layout->addLayout(toolbar);
    main_layout->addWidget(surface_, 1);
    main_layout->addLayout(caption_row);

    auto add_shortcut = [this](QKeySequence key, auto&& fn) {
        auto* s = new QShortcut(key, this);
        s->setContext(Qt::WindowShortcut);
        connect(s, &QShortcut::activated, this, fn);
    };
    add_shortcut(QKeySequence(Qt::Key_F),  [this] { zoom_segment_->set_value("Fit",   true); });
    add_shortcut(QKeySequence(Qt::Key_1),  [this] { zoom_segment_->set_value("1:1",   true); });
    add_shortcut(QKeySequence(Qt::Key_0),  [this] { zoom_segment_->set_value("Fit",   true); });
    add_shortcut(QKeySequence(Qt::Key_R),  [this] { view_segment_->set_value("R",     true); });
    add_shortcut(QKeySequence(Qt::Key_G),  [this] { view_segment_->set_value("G",     true); });
    add_shortcut(QKeySequence(Qt::Key_B),  [this] { view_segment_->set_value("B",     true); });
    add_shortcut(QKeySequence(Qt::Key_A),  [this] { view_segment_->set_value("A",     true); });
    add_shortcut(QKeySequence(Qt::Key_QuoteLeft), [this] { view_segment_->set_value("RGB", true); });

    update_caption_();
}

bool PreviewPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == surface_ && (event->type() == QEvent::Resize
                                || event->type() == QEvent::Show)) {
        position_badge_();
    }
    return QFrame::eventFilter(watched, event);
}

void PreviewPanel::position_badge_()
{
    if (!channel_badge_ || !surface_) {
        return;
    }
    channel_badge_->adjustSize();
    channel_badge_->move(10, 10);
    channel_badge_->raise();
}

void PreviewPanel::apply_view_(PreviewViewMode mode)
{
    preview_->set_view_mode(mode);
    const bool is_channel = mode != PreviewViewMode::RGB;
    if (is_channel) {
        QChar letter('R');
        switch (mode) {
            case PreviewViewMode::R: letter = QChar('R'); break;
            case PreviewViewMode::G: letter = QChar('G'); break;
            case PreviewViewMode::B: letter = QChar('B'); break;
            case PreviewViewMode::A: letter = QChar('A'); break;
            default: break;
        }
        channel_badge_dot_->set_letter(letter);
        channel_badge_label_->setText(QStringLiteral("%1 channel").arg(letter));
        channel_badge_->show();
        position_badge_();
    } else {
        channel_badge_->hide();
    }
}

void PreviewPanel::update_caption_()
{
    const bool has_data = controller_->any_slot_populated();
    int w = 0, h = 0;
    if (has_data) {
        // Pull the latest known target size from one of the populated slots.
        for (const auto& s : controller_->job().inputs) {
            if (s.populated()) {
                w = s.image->width();
                h = s.image->height();
                break;
            }
        }
    }
    if (has_data && w > 0 && h > 0) {
        size_caption_->setText(QStringLiteral("%1 × %2 · sRGB").arg(w).arg(h));
    } else {
        size_caption_->setText(QStringLiteral("— · sRGB"));
    }

    if (has_data) {
        view_caption_->setText(QStringLiteral("viewing %1 @ %2")
                                   .arg(view_segment_->value(), zoom_segment_->value()));
    } else {
        view_caption_->setText(QStringLiteral("no source"));
    }
}

} // namespace tcp::app
