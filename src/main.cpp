#include "clipboardwatcher.h"
#include "clipstorage.h"
#include "mainwindow.h"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setQuitOnLastWindowClosed(false);

  app.setApplicationName(QStringLiteral("quickqc"));
  app.setOrganizationName(QStringLiteral("quickqc"));
#ifdef QUICKQC_VERSION
  app.setApplicationVersion(QStringLiteral(QUICKQC_VERSION));
#else
  app.setApplicationVersion(QStringLiteral("0.1.0"));
#endif

  const QString instanceServerName = QStringLiteral("com.muyleang.quickqc.instance");

  {
    QLocalSocket socket;
    socket.connectToServer(instanceServerName, QIODevice::WriteOnly);
    if (socket.waitForConnected(150)) {
      socket.write("show");
      socket.flush();
      socket.waitForBytesWritten(150);
      return 0;
    }
  }

  QLocalServer::removeServer(instanceServerName);
  QLocalServer instanceServer;
  if (!instanceServer.listen(instanceServerName)) {
    QMessageBox::warning(
        nullptr,
        QStringLiteral("QuickQC"),
        QStringLiteral("Another QuickQC instance appears to be running."));
    return 0;
  }

  ClipStorage storage;
  if (!storage.open()) {
    QMessageBox::critical(
        nullptr,
        QStringLiteral("QuickQC"),
        QStringLiteral("Failed to initialize database.\nPath: %1").arg(storage.databasePath()));
    return 1;
  }

  ClipboardWatcher watcher(QApplication::clipboard(), &storage);
  watcher.primeFromCurrentClipboard();
  MainWindow window(&storage, &watcher);

  QObject::connect(&watcher, &ClipboardWatcher::historyChanged, &window, &MainWindow::scheduleRefresh);
  QObject::connect(&instanceServer, &QLocalServer::newConnection, &window, [&instanceServer, &window]() {
    while (QLocalSocket* client = instanceServer.nextPendingConnection()) {
      client->readAll();
      client->disconnectFromServer();
      client->deleteLater();
      QMetaObject::invokeMethod(&window, &MainWindow::showNearCursor, Qt::QueuedConnection);
    }
  });

  window.show();

  return app.exec();
}
