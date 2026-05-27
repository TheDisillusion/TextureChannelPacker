#include "main_window.h"

#include "tcp/version.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QPixmap>
#include <QString>
#include <QStringList>
#include <QStyleFactory>
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

// Set the global UI font. Inter is the design's primary choice; we fall
// back to Segoe UI on Windows boxes that don't have it installed. The mono
// family is selected per-widget in the QSS / custom paint code.
// Register the bundled Inter and JetBrains Mono variable fonts. Bundling
// them avoids relying on whatever (possibly broken) Inter the local Windows
// machine has installed — earlier iterations of this app rendered every
// styled label in italics because the system's Inter install only included
// an italic style face. Both fonts ship under SIL OFL 1.1, which permits
// embedding and redistribution; the upstream license texts live alongside
// the .ttf files for attribution.
//
// Returns the resolved family name (Inter / JetBrains Mono) on success, or
// an empty string if Qt rejected the resource.
QString register_bundled_font_(const QString& resource_path)
{
    const int id = QFontDatabase::addApplicationFont(resource_path);
    if (id < 0) {
        qWarning("could not register bundled font %s", qUtf8Printable(resource_path));
        return {};
    }
    const QStringList families = QFontDatabase::applicationFontFamilies(id);
    if (families.isEmpty()) {
        qWarning("bundled font %s registered but exposed no families",
                 qUtf8Printable(resource_path));
        return {};
    }
    qInfo() << "registered bundled font" << resource_path << "->" << families.first();
    return families.first();
}

void apply_fonts(QApplication& app)
{
    const QString ui_family = register_bundled_font_(
        QStringLiteral(":/tcp/fonts/InterVariable.ttf"));
    const QString mono_family = register_bundled_font_(
        QStringLiteral(":/tcp/fonts/JetBrainsMono.ttf"));

    // Set the global UI font. The bundled "Inter" resolves first; Segoe UI
    // is the fallback for any environment where the application resource
    // fails to register.
    QFont f;
    QStringList families;
    if (!ui_family.isEmpty()) {
        families << ui_family;
    }
    families << QStringLiteral("Inter")
             << QStringLiteral("Segoe UI")
             << QStringLiteral("Arial");
    f.setFamilies(families);
    f.setPointSizeF(10.0);
    f.setStyle(QFont::StyleNormal);
    f.setItalic(false);
    f.setWeight(QFont::Medium);
    app.setFont(f);
    Q_UNUSED(mono_family);
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

// Install the branded app icon. The .ico bundles multiple sizes (16, 32,
// 48, 64, 128, 256) so Qt picks the right frame for the title bar, the
// Alt-Tab popup, and the taskbar.
//
// Setting a non-null icon also avoids a historical Format_Mono assert path:
// when QApplication::windowIcon() is null, Qt lazily constructs an HICON
// from a null QBitmap which trips an assert in Debug builds and returns a
// NULL HICON in Release.
void install_default_window_icon(QApplication& app)
{
    QIcon icon(QStringLiteral(":/tcp/icon/TextureChannelPacker.ico"));
    if (!icon.isNull()) {
        app.setWindowIcon(icon);
        return;
    }
    qWarning("could not load app icon — falling back to placeholder");
    QPixmap pm(16, 16);
    pm.fill(QColor(0x1a, 0x1b, 0x1f));
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

    // Pin the widget style to Fusion before anything else paints. Qt's
    // default Windows 11 style enables Mica/acrylic on the main window,
    // which leaks the desktop wallpaper through the canvas and washes the
    // dark palette to a brighter gray than the design tokens specify. It
    // also routes combo box / spin box text through native rendering paths
    // that ignore QSS font-style hints. Fusion is a flat platform-neutral
    // style that renders predictably and honors our stylesheet end-to-end.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

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

    apply_fonts(app);
    apply_dark_theme(app);
    install_default_window_icon(app);

    tcp::app::MainWindow window;
    window.show();
    return QApplication::exec();
}
