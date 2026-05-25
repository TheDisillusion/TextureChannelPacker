#include "main_window.h"

#include "tcp/version.h"

#include <QApplication>
#include <QFile>
#include <QString>
#include <QTextStream>

namespace {

void apply_dark_theme(QApplication& app)
{
    QFile qss(QStringLiteral(":/tcp/theme/dark.qss"));
    if (!qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&qss);
    app.setStyleSheet(stream.readAll());
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Disillusion"));
    QCoreApplication::setApplicationName(QStringLiteral("Texture Channel Packer"));
    QCoreApplication::setApplicationVersion(
        QString::fromUtf8(tcp::version_string().data(),
                          static_cast<int>(tcp::version_string().size())));

    apply_dark_theme(app);

    tcp::app::MainWindow window;
    window.show();
    return QApplication::exec();
}
