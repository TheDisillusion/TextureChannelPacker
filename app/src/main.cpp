#include "tcp/version.h"

#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QString>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Disillusion");
    QCoreApplication::setApplicationName("Texture Channel Packer");
    QCoreApplication::setApplicationVersion(QString::fromUtf8(tcp::version_string().data(),
                                                              static_cast<int>(tcp::version_string().size())));

    QMainWindow window;
    window.setWindowTitle("Texture Channel Packer");
    window.resize(1280, 800);

    auto* placeholder = new QLabel(QStringLiteral("Texture Channel Packer — Phase 0 bootstrap"));
    placeholder->setAlignment(Qt::AlignCenter);
    window.setCentralWidget(placeholder);

    window.show();
    return QApplication::exec();
}
