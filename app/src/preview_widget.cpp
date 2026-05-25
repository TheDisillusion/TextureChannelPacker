#include "preview_widget.h"

#include "job_controller.h"

#include "tcp/channel_ref.h"
#include "tcp/image.h"
#include "tcp/pack_job.h"
#include "tcp/pixel_format.h"

#include <QFile>
#include <QMouseEvent>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QSize>
#include <QString>
#include <QSurfaceFormat>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace tcp::app {

namespace {

// Map tcp::PixelFormat + channel count to the matching QOpenGLTexture format,
// upload pixel format, and upload pixel type.
struct GlFormatTriple
{
    QOpenGLTexture::TextureFormat texture_format;
    QOpenGLTexture::PixelFormat upload_format;
    QOpenGLTexture::PixelType upload_type;
};

GlFormatTriple gl_format_for(PixelFormat pf, int channels)
{
    using TF = QOpenGLTexture::TextureFormat;
    using PF = QOpenGLTexture::PixelFormat;
    using PT = QOpenGLTexture::PixelType;
    switch (pf) {
        case PixelFormat::U8:
            switch (channels) {
                case 1: return {TF::R8_UNorm, PF::Red, PT::UInt8};
                case 2: return {TF::RG8_UNorm, PF::RG, PT::UInt8};
                case 3: return {TF::RGB8_UNorm, PF::RGB, PT::UInt8};
                default: return {TF::RGBA8_UNorm, PF::RGBA, PT::UInt8};
            }
        case PixelFormat::U16:
            switch (channels) {
                case 1: return {TF::R16_UNorm, PF::Red, PT::UInt16};
                case 2: return {TF::RG16_UNorm, PF::RG, PT::UInt16};
                case 3: return {TF::RGB16_UNorm, PF::RGB, PT::UInt16};
                default: return {TF::RGBA16_UNorm, PF::RGBA, PT::UInt16};
            }
        case PixelFormat::F32:
            switch (channels) {
                case 1: return {TF::R32F, PF::Red, PT::Float32};
                case 2: return {TF::RG32F, PF::RG, PT::Float32};
                case 3: return {TF::RGB32F, PF::RGB, PT::Float32};
                default: return {TF::RGBA32F, PF::RGBA, PT::Float32};
            }
    }
    return {TF::RGBA8_UNorm, PF::RGBA, PT::UInt8};
}

QString read_shader_source(const QString& resource_path)
{
    QFile f(resource_path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString{};
    }
    return QString::fromUtf8(f.readAll());
}

constexpr float min_zoom = 0.05f;
constexpr float max_zoom = 32.0f;

} // namespace

PreviewWidget::PreviewWidget(JobController* controller, QWidget* parent)
    : QOpenGLWidget(parent),
      controller_(controller)
{
    setObjectName(QStringLiteral("PreviewWidget"));
    setMouseTracking(true);
    setFocusPolicy(Qt::WheelFocus);

    QSurfaceFormat fmt = format();
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setVersion(3, 3);
    fmt.setDepthBufferSize(0);
    fmt.setStencilBufferSize(0);
    fmt.setSwapInterval(1);
    setFormat(fmt);

    connect(controller_, &JobController::slot_loaded, this, &PreviewWidget::on_slot_loaded_);
    connect(controller_, &JobController::slot_cleared, this, &PreviewWidget::on_slot_cleared_);
    connect(controller_, &JobController::channel_map_changed, this,
            &PreviewWidget::on_channel_map_changed_);
    connect(controller_, &JobController::target_size_changed, this,
            &PreviewWidget::on_target_size_changed_);
}

PreviewWidget::~PreviewWidget()
{
    // GL resources must be destroyed with a current context.
    makeCurrent();
    program_.reset();
    vao_.reset();
    vbo_.reset();
    for (auto& tex : slot_textures_) {
        tex.reset();
    }
    placeholder_texture_.reset();
    doneCurrent();
}

void PreviewWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    program_ = std::make_unique<QOpenGLShaderProgram>();
    const QString vert = read_shader_source(QStringLiteral(":/tcp/shaders/preview.vert"));
    const QString frag = read_shader_source(QStringLiteral(":/tcp/shaders/preview.frag"));
    if (vert.isEmpty() || frag.isEmpty()
        || !program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vert)
        || !program_->addShaderFromSourceCode(QOpenGLShader::Fragment, frag)
        || !program_->link()) {
        // Leave program_ around; paintGL will detect !isLinked and just clear.
        qWarning("PreviewWidget: shader compile/link failed: %s",
                 qUtf8Printable(program_->log()));
    }

    // Fullscreen triangle (cheaper than a quad — single primitive).
    static constexpr float verts[] = {
        -1.0f, -1.0f,
         3.0f, -1.0f,
        -1.0f,  3.0f,
    };

    vao_ = std::make_unique<QOpenGLVertexArrayObject>();
    vao_->create();
    vao_->bind();

    vbo_ = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
    vbo_->create();
    vbo_->bind();
    vbo_->setUsagePattern(QOpenGLBuffer::StaticDraw);
    vbo_->allocate(verts, sizeof(verts));

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    vbo_->release();
    vao_->release();

    ensure_placeholder_texture_();

    // Snapshot the controller's current state in case slots were loaded
    // before our GL context existed.
    for (int i = 0; i < 4; ++i) {
        if (controller_->job().inputs[i].populated()) {
            upload_pending_[i] = true;
        }
    }
    on_target_size_changed_(0, 0);
    const auto target_now = compute_target_size(controller_->job());
    if (target_now.has_value()) {
        image_size_ = QSize(target_now->width, target_now->height);
    }
}

void PreviewWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void PreviewWidget::paintGL()
{
    for (int i = 0; i < 4; ++i) {
        if (upload_pending_[i]) {
            upload_slot_(i);
            upload_pending_[i] = false;
        }
    }

    glClear(GL_COLOR_BUFFER_BIT);

    if (!program_ || !program_->isLinked()) {
        return;
    }

    program_->bind();
    vao_->bind();

    const auto& job = controller_->job();

    for (int slot = 0; slot < 4; ++slot) {
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(slot));
        const bool populated = job.inputs[slot].populated() && slot_textures_[slot];
        if (populated) {
            slot_textures_[slot]->bind();
        } else {
            placeholder_texture_->bind();
        }
        const char* uniform_name = "";
        switch (slot) {
            case 0: uniform_name = "u_slot_0"; break;
            case 1: uniform_name = "u_slot_1"; break;
            case 2: uniform_name = "u_slot_2"; break;
            case 3: uniform_name = "u_slot_3"; break;
        }
        program_->setUniformValue(uniform_name, slot);
    }

    int slot_channels[4] = {1, 1, 1, 1};
    int channel_map_slot[4] = {-1, -1, -1, -1};
    int channel_map_source[4] = {0, 0, 0, 0};
    for (int i = 0; i < 4; ++i) {
        const auto& s = job.inputs[i];
        slot_channels[i] = s.populated() ? s.image->channels() : 1;
        const auto& ref = job.channel_map[i];
        channel_map_slot[i] = (ref.is_set() && ref.slot_index < 4) ? ref.slot_index : -1;
        channel_map_source[i] = static_cast<int>(ref.source);
    }
    program_->setUniformValueArray("u_slot_channels", slot_channels, 4);
    program_->setUniformValueArray("u_channel_map_slot", channel_map_slot, 4);
    program_->setUniformValueArray("u_channel_map_source", channel_map_source, 4);

    const QSize iw = current_image_size_();
    const float image_w = (iw.width() > 0) ? static_cast<float>(iw.width()) : 1.0f;
    const float image_h = (iw.height() > 0) ? static_cast<float>(iw.height()) : 1.0f;
    program_->setUniformValue("u_widget_size", QVector2D(static_cast<float>(width()),
                                                         static_cast<float>(height())));
    program_->setUniformValue("u_image_size", QVector2D(image_w, image_h));
    program_->setUniformValue("u_zoom", zoom_);
    program_->setUniformValue("u_pan_px", QVector2D(static_cast<float>(pan_px_.x()),
                                                    static_cast<float>(pan_px_.y())));
    program_->setUniformValue("u_view_mode", static_cast<int>(view_mode_));
    program_->setUniformValue("u_checkerboard", show_checkerboard_ ? 1 : 0);

    glDrawArrays(GL_TRIANGLES, 0, 3);

    vao_->release();
    program_->release();
}

QSize PreviewWidget::current_image_size_() const noexcept
{
    return image_size_;
}

void PreviewWidget::ensure_placeholder_texture_()
{
    if (placeholder_texture_) {
        return;
    }
    placeholder_texture_ = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    placeholder_texture_->setSize(1, 1);
    placeholder_texture_->setFormat(QOpenGLTexture::RGBA8_UNorm);
    placeholder_texture_->setMinificationFilter(QOpenGLTexture::Linear);
    placeholder_texture_->setMagnificationFilter(QOpenGLTexture::Linear);
    placeholder_texture_->setWrapMode(QOpenGLTexture::ClampToEdge);
    placeholder_texture_->allocateStorage();
    const std::uint8_t transparent_black[4] = {0, 0, 0, 0};
    placeholder_texture_->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8, transparent_black);
}

void PreviewWidget::upload_slot_(int slot)
{
    if (slot < 0 || slot >= 4) {
        return;
    }
    const auto& s = controller_->job().inputs[slot];
    if (!s.populated()) {
        clear_slot_texture_(slot);
        return;
    }
    const auto& img = *s.image;
    const GlFormatTriple gl_fmt = gl_format_for(img.format(), img.channels());

    auto tex = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
    tex->setSize(img.width(), img.height());
    tex->setFormat(gl_fmt.texture_format);
    tex->setMinificationFilter(QOpenGLTexture::Linear);
    tex->setMagnificationFilter(QOpenGLTexture::Linear);
    tex->setWrapMode(QOpenGLTexture::ClampToEdge);
    tex->allocateStorage();
    tex->setData(gl_fmt.upload_format, gl_fmt.upload_type, img.data());
    slot_textures_[slot] = std::move(tex);
}

void PreviewWidget::clear_slot_texture_(int slot)
{
    if (slot < 0 || slot >= 4) {
        return;
    }
    slot_textures_[slot].reset();
}

void PreviewWidget::set_view_mode(PreviewViewMode mode)
{
    if (view_mode_ == mode) {
        return;
    }
    view_mode_ = mode;
    emit view_mode_changed(mode);
    update();
}

void PreviewWidget::set_checkerboard(bool on)
{
    if (show_checkerboard_ == on) {
        return;
    }
    show_checkerboard_ = on;
    emit checkerboard_changed(on);
    update();
}

void PreviewWidget::fit_to_window()
{
    const QSize iw = current_image_size_();
    if (iw.width() <= 0 || iw.height() <= 0 || width() <= 0 || height() <= 0) {
        zoom_ = 1.0f;
    } else {
        const float fx = static_cast<float>(width()) / static_cast<float>(iw.width());
        const float fy = static_cast<float>(height()) / static_cast<float>(iw.height());
        zoom_ = std::clamp(std::min(fx, fy) * 0.95f, min_zoom, max_zoom);
    }
    pan_px_ = QPointF(0, 0);
    emit zoom_changed(zoom_);
    update();
}

void PreviewWidget::set_one_to_one()
{
    set_zoom(1.0f);
    pan_px_ = QPointF(0, 0);
    update();
}

void PreviewWidget::set_zoom(float z)
{
    z = std::clamp(z, min_zoom, max_zoom);
    if (std::abs(z - zoom_) < 1e-6f) {
        return;
    }
    zoom_ = z;
    clamp_pan_();
    emit zoom_changed(zoom_);
    update();
}

void PreviewWidget::clamp_pan_()
{
    // Keep at least a sliver of the image visible by clamping pan to the
    // half-image extent in widget pixels.
    const QSize iw = current_image_size_();
    if (iw.width() <= 0 || iw.height() <= 0) {
        pan_px_ = QPointF(0, 0);
        return;
    }
    const float max_x = (static_cast<float>(iw.width()) * zoom_) * 0.5f
                        + static_cast<float>(width()) * 0.5f;
    const float max_y = (static_cast<float>(iw.height()) * zoom_) * 0.5f
                        + static_cast<float>(height()) * 0.5f;
    pan_px_.setX(std::clamp<double>(pan_px_.x(), -max_x, max_x));
    pan_px_.setY(std::clamp<double>(pan_px_.y(), -max_y, max_y));
}

void PreviewWidget::wheelEvent(QWheelEvent* event)
{
    const float delta = static_cast<float>(event->angleDelta().y());
    if (std::abs(delta) < 1.0f) {
        return;
    }
    const float factor = (delta > 0.0f) ? 1.15f : (1.0f / 1.15f);

    // Zoom around the cursor: keep the image point under the cursor pinned.
    const QPointF cursor = event->position();
    const QPointF widget_center(width() * 0.5, height() * 0.5);
    const QPointF rel = cursor - widget_center - pan_px_;

    const float new_zoom = std::clamp(zoom_ * factor, min_zoom, max_zoom);
    const float ratio = new_zoom / zoom_;
    pan_px_ = pan_px_ + rel * (1.0 - ratio);
    zoom_ = new_zoom;
    clamp_pan_();
    emit zoom_changed(zoom_);
    update();
    event->accept();
}

void PreviewWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton) {
        dragging_ = true;
        last_drag_pos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QOpenGLWidget::mousePressEvent(event);
}

void PreviewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_) {
        const QPoint d = event->pos() - last_drag_pos_;
        last_drag_pos_ = event->pos();
        pan_px_ += QPointF(d.x(), d.y());
        clamp_pan_();
        update();
        event->accept();
        return;
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void PreviewWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (dragging_) {
        dragging_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void PreviewWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        fit_to_window();
        event->accept();
        return;
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void PreviewWidget::on_slot_loaded_(int slot)
{
    upload_pending_[slot] = true;
    update();
}

void PreviewWidget::on_slot_cleared_(int slot)
{
    if (slot < 0 || slot >= 4) {
        return;
    }
    // Defer the texture delete to paintGL where the context is current.
    if (context()) {
        makeCurrent();
        clear_slot_texture_(slot);
        doneCurrent();
    } else {
        slot_textures_[slot].reset();
    }
    update();
}

void PreviewWidget::on_channel_map_changed_(int /*destination*/, tcp::ChannelRef /*ref*/)
{
    update();
}

void PreviewWidget::on_target_size_changed_(int width_px, int height_px)
{
    const QSize previous = image_size_;
    if (width_px > 0 && height_px > 0) {
        image_size_ = QSize(width_px, height_px);
    } else {
        image_size_ = QSize(0, 0);
    }
    if (previous != image_size_ && image_size_.width() > 0) {
        // When the target resolution first appears, auto-fit so the user sees
        // the whole image without scrolling.
        if (previous.width() <= 0) {
            fit_to_window();
        }
    }
    update();
}

} // namespace tcp::app
