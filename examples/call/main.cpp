#include <glib-object.h>
#include <gst/gst.h>

#include <TelepathyQt4/Debug>
#include <TelepathyQt4/Constants>
#include <TelepathyQt4/Types>

#include <QDebug>
#include <QtGui>

#include "call-window.h"
#include "farsight-channel.h"

int main(int argc, char **argv)
{
    g_type_init();
    gst_init(&argc, &argv);

    QApplication app(argc, argv);

    if (argc < 3) {
        qDebug() << "usage: call username password";
        return 1;
    }

    app.setAttribute(Qt::AA_NativeWindows);

    Tp::registerTypes();
    Tp::enableDebug(true);
    Tp::enableWarnings(true);
    qRegisterMetaType<Tp::FarsightChannel::Status>();

    QString username = QLatin1String(argv[1]);
    QString password = QLatin1String(argv[2]);
    CallWindow w(username, password);
    w.show();

    return app.exec();
}
