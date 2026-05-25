#pragma once

#include "tcp/channel_ref.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QPoint>
#include <QPointF>

#include <array>
#include <memory>

class QOpenGLBuffer;
class QOpenGLShaderProgram;
class QOpenGLTexture;
class QOpenGLVertexArrayObject;

namespace tcp {
class Image;
}

namespace tcp::app {

class JobController;

enum class PreviewViewMode : int {
    RGB = 0,
    R = 1,
    G = 2,
    B = 3,
    A = 4,
};

class PreviewWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    explicit PreviewWidget(JobController* controller, QWidget* parent = nullptr);
    ~PreviewWidget() override;

    void set_view_mode(PreviewViewMode mode);
    void set_checkerboard(bool on);

    void fit_to_window();
    void set_one_to_one();
    void set_zoom(float zoom_px_per_image_px);
    [[nodiscard]] float zoom() const noexcept { return zoom_; }

signals:
    void zoom_changed(float zoom);
    void view_mode_changed(PreviewViewMode mode);
    void checkerboard_changed(bool on);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private slots:
    void on_slot_loaded_(int slot);
    void on_slot_cleared_(int slot);
    void on_channel_map_changed_(int destination, tcp::ChannelRef ref);
    void on_target_size_changed_(int width, int height);

private:
    void upload_slot_(int slot);
    void clear_slot_texture_(int slot);
    void ensure_placeholder_texture_();
    [[nodiscard]] QSize current_image_size_() const noexcept;
    void clamp_pan_();

    JobController* controller_ = nullptr;

    // GL resources — created in initializeGL, destroyed in destructor with a
    // current context. We hold them as unique_ptr so the destructors are
    // explicit and ordered.
    std::unique_ptr<QOpenGLShaderProgram> program_;
    std::unique_ptr<QOpenGLVertexArrayObject> vao_;
    std::unique_ptr<QOpenGLBuffer> vbo_;
    std::array<std::unique_ptr<QOpenGLTexture>, 4> slot_textures_;
    std::unique_ptr<QOpenGLTexture> placeholder_texture_;

    // Camera state.
    float zoom_ = 1.0f;
    QPointF pan_px_{0.0f, 0.0f};
    QPoint last_drag_pos_;
    bool dragging_ = false;

    // Target image size in pixels (the resolution the packer would use).
    QSize image_size_{0, 0};

    PreviewViewMode view_mode_ = PreviewViewMode::RGB;
    bool show_checkerboard_ = true;

    // Pending re-uploads queued while no GL context is current.
    std::array<bool, 4> upload_pending_{};
};

} // namespace tcp::app
