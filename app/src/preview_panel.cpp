#include "preview_panel.h"

#include "job_controller.h"
#include "preview_widget.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QShortcut>
#include <QToolButton>
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
    auto* title = new QLabel(QStringLiteral("Preview"), this);
    title->setObjectName(QStringLiteral("PanelTitle"));

    preview_ = new PreviewWidget(controller_, this);
    preview_->setMinimumSize(320, 240);

    auto make_view_button = [this](const QString& label, PreviewViewMode mode, bool checked) {
        auto* b = new QToolButton(this);
        b->setText(label);
        b->setCheckable(true);
        b->setChecked(checked);
        b->setAutoExclusive(false); // QButtonGroup handles exclusivity
        b->setObjectName(QStringLiteral("ViewModeButton"));
        connect(b, &QToolButton::clicked, this,
                [this, mode] { preview_->set_view_mode(mode); });
        return b;
    };

    view_mode_group_ = new QButtonGroup(this);
    view_mode_group_->setExclusive(true);
    auto* rgb_btn = make_view_button(QStringLiteral("RGB"), PreviewViewMode::RGB, true);
    auto* r_btn = make_view_button(QStringLiteral("R"), PreviewViewMode::R, false);
    auto* g_btn = make_view_button(QStringLiteral("G"), PreviewViewMode::G, false);
    auto* b_btn = make_view_button(QStringLiteral("B"), PreviewViewMode::B, false);
    auto* a_btn = make_view_button(QStringLiteral("A"), PreviewViewMode::A, false);
    view_mode_group_->addButton(rgb_btn);
    view_mode_group_->addButton(r_btn);
    view_mode_group_->addButton(g_btn);
    view_mode_group_->addButton(b_btn);
    view_mode_group_->addButton(a_btn);

    fit_button_ = new QToolButton(this);
    fit_button_->setText(QStringLiteral("Fit"));
    fit_button_->setToolTip(tr("Fit the preview to the window (or double-click the canvas)"));
    connect(fit_button_, &QToolButton::clicked, preview_, &PreviewWidget::fit_to_window);

    one_to_one_button_ = new QToolButton(this);
    one_to_one_button_->setText(QStringLiteral("1:1"));
    one_to_one_button_->setToolTip(tr("Display one image pixel per screen pixel"));
    connect(one_to_one_button_, &QToolButton::clicked, preview_, &PreviewWidget::set_one_to_one);

    checkerboard_check_ = new QCheckBox(QStringLiteral("Checker"), this);
    checkerboard_check_->setChecked(true);
    checkerboard_check_->setToolTip(tr("Show a checkerboard behind translucent regions"));
    connect(checkerboard_check_, &QCheckBox::toggled, preview_, &PreviewWidget::set_checkerboard);

    zoom_label_ = new QLabel(QStringLiteral("100%"), this);
    zoom_label_->setObjectName(QStringLiteral("ZoomLabel"));
    zoom_label_->setMinimumWidth(56);
    zoom_label_->setAlignment(Qt::AlignCenter);
    connect(preview_, &PreviewWidget::zoom_changed, this, [this](float z) {
        zoom_label_->setText(QStringLiteral("%1%").arg(static_cast<int>(z * 100.0f + 0.5f)));
    });

    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(6);
    toolbar->addWidget(new QLabel(QStringLiteral("View:"), this));
    toolbar->addWidget(rgb_btn);
    toolbar->addWidget(r_btn);
    toolbar->addWidget(g_btn);
    toolbar->addWidget(b_btn);
    toolbar->addWidget(a_btn);
    toolbar->addSpacing(12);
    toolbar->addWidget(fit_button_);
    toolbar->addWidget(one_to_one_button_);
    toolbar->addWidget(zoom_label_);
    toolbar->addStretch(1);
    toolbar->addWidget(checkerboard_check_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);
    layout->addWidget(title);
    layout->addLayout(toolbar);
    layout->addWidget(preview_, 1);

    // Local shortcuts. Window-scoped so they don't fire when the user is
    // typing in a spinbox or save dialog.
    auto add_shortcut = [this](QKeySequence key, auto&& fn) {
        auto* s = new QShortcut(key, this);
        s->setContext(Qt::WindowShortcut);
        connect(s, &QShortcut::activated, this, fn);
    };
    add_shortcut(QKeySequence(Qt::Key_F), [this] { preview_->fit_to_window(); });
    add_shortcut(QKeySequence(Qt::Key_1), [this] { preview_->set_one_to_one(); });
    add_shortcut(QKeySequence(Qt::Key_0), [this] { preview_->fit_to_window(); });
    add_shortcut(QKeySequence(Qt::Key_R), [this, r_btn] {
        r_btn->click();
    });
    add_shortcut(QKeySequence(Qt::Key_G), [this, g_btn] { g_btn->click(); });
    add_shortcut(QKeySequence(Qt::Key_B), [this, b_btn] { b_btn->click(); });
    add_shortcut(QKeySequence(Qt::Key_A), [this, a_btn] { a_btn->click(); });
    add_shortcut(QKeySequence(Qt::Key_QuoteLeft), [this, rgb_btn] { rgb_btn->click(); });
}

} // namespace tcp::app
