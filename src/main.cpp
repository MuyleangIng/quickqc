#include "clipboardwatcher.h"
#include "clipstorage.h"
#include "mainwindow.h"

#include <QApplication>
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

  window.show();

  return app.exec();
}
