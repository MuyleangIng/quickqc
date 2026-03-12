#include "clipboardwatcher.h"
#include "clipstorage.h"
#include "globalhotkey.h"
#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QKeySequence>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QSettings>

namespace {
QString defaultOpenHotkeyPortable() {
#if defined(Q_OS_MAC)
  return QStringLiteral("Meta+Shift+V");
#else
  return QStringLiteral("Ctrl+Shift+V");
#endif
}
}

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setQuitOnLastWindowClosed(false);

  app.setApplicationName(QStringLiteral("quickqc"));
  app.setOrganizationName(QStringLiteral("quickqc"));
#ifdef QUICKQC_VERSION
  app.setApplicationVersion(QStringLiteral(QUICKQC_VERSION));
#else
  app.setApplicationVersion(QStringLiteral("0.2.4"));
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
  GlobalHotkey hotkey(&app);
  QSettings settings;
  const QString configuredHotkey = settings.value(
      QStringLiteral("shortcuts/openHotkey"),
      defaultOpenHotkeyPortable()).toString();
  if (!hotkey.registerOpenClipboardHotkey(configuredHotkey)) {
    qWarning() << "QuickQC global open hotkey registration failed; in-window shortcut remains available.";
  }
  QObject::connect(&window, &MainWindow::openHotkeyChanged, &hotkey, [&hotkey](const QString& hotkeyPortableText) {
    if (!hotkey.registerOpenClipboardHotkey(hotkeyPortableText)) {
      qWarning() << "QuickQC failed to apply updated global hotkey.";
    }
  });
  QObject::connect(&hotkey, &GlobalHotkey::activated, &window, [&window]() {
    QMetaObject::invokeMethod(&window, &MainWindow::showNearCursor, Qt::QueuedConnection);
  });
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
