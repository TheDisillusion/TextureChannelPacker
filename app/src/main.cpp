#include "main_window.h"

#include "tcp/version.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QtMessageHandler>

#include <cstdio>

namespace {

// ---------------------------------------------------------------------------
// File-backed log
// ---------------------------------------------------------------------------
// Qt's default qWarning destination on a Win32 GUI subsystem binary is
// OutputDebugString, which is invisible unless a debugger is attached. We
// route every Qt message to a tcp.log next to the executable so users can
// share it when something goes wrong.
QFile* g_log_file = nullptr;
QtMessageHandler g_prev_handler = nullptr;

const char* level_tag(QtMsgType t) noexcept
{
    switch (t) {
        case QtDebugMsg: return "DEBUG";
        case QtInfoMsg: return "INFO ";
        case QtWarningMsg: return "WARN ";
        case QtCriticalMsg: return "CRIT ";
        case QtFatalMsg: return "FATAL";
    }
    return "?    ";
}

void tcp_message_handler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    if (g_log_file && g_log_file->isOpen()) {
        const QString stamp = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
        const QString line = QStringLiteral("[%1] [%2] %3\n").arg(stamp, QString::fromLatin1(level_tag(type)), msg);
        g_log_file->write(line.toUtf8());
        g_log_file->flush();
    }
    if (g_prev_handler) {
        g_prev_handler(type, ctx, msg);
    }
}

void install_log_handler()
{
    const QString dir = QCoreApplication::applicationDirPath();
    const QString path = QDir(dir).filePath(QStringLiteral("tcp.log"));
    auto* f = new QFile(path);
    if (!f->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        delete f;
        return;
    }
    g_log_file = f;
    g_log_file->write("--- session start ---\n");
    g_log_file->flush();
    g_prev_handler = qInstallMessageHandler(tcp_message_handler);
}

void apply_dark_theme(QApplication& app)
{
    QFile qss(QStringLiteral(":/tcp/theme/dark.qss"));
    if (!qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("could not open :/tcp/theme/dark.qss — UI will use the platform default style");
        return;
    }
    QTextStream stream(&qss);
    app.setStyleSheet(stream.readAll());
}

// Pre-populate QApplication::windowIcon() with a non-null QPixmap so Qt does
// not lazily construct an HICON from a null QBitmap when the OS asks for the
// taskbar / Alt-Tab icon. That lazy path tripped a Format_Mono assert in the
// Debug build (qpixmap_win.cpp:200) and silently returned a NULL HICON in
// Release, which in turn could disturb downstream UI rendering. A plain
// 16×16 filled pixmap is enough to keep that path off the stack entirely.
void install_default_window_icon(QApplication& app)
{
    QPixmap pm(16, 16);
    pm.fill(QColor(0x1e, 0x1e, 0x1e)); // matches dark theme background
    app.setWindowIcon(QIcon(pm));
}

} // namespace

int main(int argc, char* argv[])
{
    // ----- OpenGL surface format -----
    // Must be set BEFORE QApplication so every QOpenGLWidget surface uses it.
    // Setting it later (e.g. from PreviewWidget's constructor) is too late on
    // some drivers — Qt has already chosen a profile and the GLSL 330 shader
    // fails to compile against a 2.x context.
    QSurfaceFormat gl;
    gl.setRenderableType(QSurfaceFormat::OpenGL);
    gl.setProfile(QSurfaceFormat::CoreProfile);
    gl.setMajorVersion(3);
    gl.setMinorVersion(3);
    gl.setDepthBufferSize(0);
    gl.setStencilBufferSize(0);
    gl.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(gl);

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Disillusion"));
    QCoreApplication::setApplicationName(QStringLiteral("Texture Channel Packer"));
    QCoreApplication::setApplicationVersion(
        QString::fromUtf8(tcp::version_string().data(),
                          static_cast<int>(tcp::version_string().size())));

    install_log_handler();
    qInfo() << "Texture Channel Packer"
            << QString::fromUtf8(tcp::version_string().data(),
                                 static_cast<int>(tcp::version_string().size()))
            << "starting";

    apply_dark_theme(app);
    install_default_window_icon(app);

    tcp::app::MainWindow window;
    window.show();
    return QApplication::exec();
}
