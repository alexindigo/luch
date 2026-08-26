#include <LayerShellQt/Shell>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QUrl>

#include "pickerwindow.h"

int main(int argc, char *argv[])
{
#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    LayerShellQt::Shell::useLayerShell();
#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Luch"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("luch"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Luch — link router: pick which browser opens a URL."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("url"),
                                 QStringLiteral("URL to route to a browser."));
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        parser.showHelp(2);
    }

    const QUrl url(args.first());
    if (!url.isValid()
        || (url.scheme() != QLatin1String("http")
            && url.scheme() != QLatin1String("https"))) {
        qCritical().noquote()
            << QStringLiteral("luch: not a routable http(s) URL:")
            << args.first();
        return 2;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("incomingUrl"),
                                             url.toString());
    engine.load(QUrl(QStringLiteral("qrc:/Luch/ui/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "luch: QML root failed to load";
        return 1;
    }

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) {
        qCritical() << "luch: QML root is not a Window";
        return 1;
    }

    Luch::PickerWindow picker(window);
    QObject::connect(&picker, &Luch::PickerWindow::dismissed, &app,
                     [&app](int exitCode) { app.exit(exitCode); });
    window->show();

    return app.exec();
}
