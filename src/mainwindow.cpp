#include "mainwindow.h"

#include "clipboardwatcher.h"
#include "clipstorage.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QBuffer>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
#include <QFileDevice>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QShortcut>
#include <QStandardPaths>
#include <QStyle>
#include <QSurfaceFormat>
#include <QSysInfo>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QTimer>
#include <QTime>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QUuid>

#include <algorithm>

namespace {
constexpr const char* kAppDisplayName = "QuickQC";
constexpr const char* kFounderName = "Ing Muyleang";
constexpr const char* kReleaseApiUrl = "https://api.github.com/repos/MuyleangIng/quickqc/releases/latest";

QString defaultOpenHotkeyPortable() {
#if defined(Q_OS_MAC)
  return QStringLiteral("Meta+Shift+V");
#else
  return QStringLiteral("Ctrl+Shift+V");
#endif
}

QString normalizeHotkeySetting(const QString& raw) {
  QKeySequence seq = QKeySequence::fromString(raw, QKeySequence::PortableText);
  if (seq.isEmpty()) {
    seq = QKeySequence::fromString(raw, QKeySequence::NativeText);
  }
  if (seq.isEmpty()) {
    seq = QKeySequence::fromString(defaultOpenHotkeyPortable(), QKeySequence::PortableText);
  }
  return seq.toString(QKeySequence::PortableText);
}

QString firstHotkeyStepPortable(const QKeySequence& sequence) {
  const QString portable = sequence.toString(QKeySequence::PortableText);
  const QString firstStep = portable.section(',', 0, 0).trimmed();
  if (firstStep.isEmpty()) {
    return QString();
  }
  return QKeySequence::fromString(firstStep, QKeySequence::PortableText)
      .toString(QKeySequence::PortableText);
}

QString kindText(const ClipKind kind) {
  return kind == ClipKind::Image ? QStringLiteral("image") : QStringLiteral("text");
}

QHash<QString, QPixmap> gImageThumbCache;

QString normalizeVersion(const QString& rawVersion) {
  QString version = rawVersion.trimmed();
  if (version.startsWith(QStringLiteral("v"), Qt::CaseInsensitive)) {
    version.remove(0, 1);
  }

  const int hyphenPos = version.indexOf('-');
  if (hyphenPos > 0) {
    version = version.left(hyphenPos);
  }

  return version;
}

int compareVersions(const QString& lhsRaw, const QString& rhsRaw) {
  const QString lhs = normalizeVersion(lhsRaw);
  const QString rhs = normalizeVersion(rhsRaw);
  const QStringList lhsParts = lhs.split('.', Qt::SkipEmptyParts);
  const QStringList rhsParts = rhs.split('.', Qt::SkipEmptyParts);
  const int maxParts = std::max(lhsParts.size(), rhsParts.size());

  for (int i = 0; i < maxParts; ++i) {
    bool lhsOk = false;
    bool rhsOk = false;

    const int lhsNum = (i < lhsParts.size()) ? lhsParts.at(i).toInt(&lhsOk) : 0;
    const int rhsNum = (i < rhsParts.size()) ? rhsParts.at(i).toInt(&rhsOk) : 0;

    if (!lhsOk && i < lhsParts.size()) {
      return -1;
    }
    if (!rhsOk && i < rhsParts.size()) {
      return 1;
    }

    if (lhsNum < rhsNum) {
      return -1;
    }
    if (lhsNum > rhsNum) {
      return 1;
    }
  }

  return 0;
}

QString releaseAssetNameForCurrentPlatform() {
#if defined(Q_OS_MAC)
  const QString arch = QSysInfo::currentCpuArchitecture().toLower();
  if (arch.contains(QStringLiteral("arm")) || arch.contains(QStringLiteral("aarch64"))) {
    return QStringLiteral("quickqc-macos-arm64.tar.gz");
  }
  return QStringLiteral("quickqc-macos-amd64.tar.gz");
#elif defined(Q_OS_WIN)
  const QString arch = QSysInfo::currentCpuArchitecture().toLower();
  if (arch.contains(QStringLiteral("arm")) || arch.contains(QStringLiteral("aarch64"))) {
    return QStringLiteral("quickqc-windows-arm64.zip");
  }
  return QStringLiteral("quickqc-windows-amd64.zip");
#elif defined(Q_OS_LINUX)
  const QString arch = QSysInfo::currentCpuArchitecture().toLower();
  if (arch.contains(QStringLiteral("arm")) || arch.contains(QStringLiteral("aarch64"))) {
    return QStringLiteral("quickqc-linux-ubuntu-arm64.tar.gz");
  }
  return QStringLiteral("quickqc-linux-ubuntu-amd64.tar.gz");
#else
  return QString();
#endif
}

QString releaseAssetUrlByName(const QJsonObject& releaseObject, const QString& expectedName) {
  if (expectedName.trimmed().isEmpty()) {
    return QString();
  }

  const QJsonArray assets = releaseObject.value(QStringLiteral("assets")).toArray();
  for (const QJsonValue& value : assets) {
    const QJsonObject asset = value.toObject();
    const QString name = asset.value(QStringLiteral("name")).toString().trimmed();
    if (name == expectedName) {
      return asset.value(QStringLiteral("browser_download_url")).toString().trimmed();
    }
  }

  return QString();
}

QString releaseChecksumsAssetName() {
  return QStringLiteral("quickqc-checksums.txt");
}

QString checksumForAssetFromManifest(const QString& manifest, const QString& assetName) {
  if (manifest.trimmed().isEmpty() || assetName.trimmed().isEmpty()) {
    return QString();
  }

  static const QRegularExpression linePattern(
      QStringLiteral("^\\s*([A-Fa-f0-9]{64})\\s+\\*?(.+?)\\s*$"));

  const QStringList lines = manifest.split('\n');
  for (const QString& line : lines) {
    const QRegularExpressionMatch match = linePattern.match(line);
    if (!match.hasMatch()) {
      continue;
    }

    const QString listedFile = match.captured(2).trimmed();
    if (listedFile == assetName) {
      return match.captured(1).toLower();
    }
  }

  return QString();
}

bool downloadTextWithTimeout(
    const QUrl& url,
    const QString& userAgent,
    int timeoutMs,
    QString* responseText,
    QString* errorMessage) {
  if (!url.isValid()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Invalid URL.");
    }
    return false;
  }

  QNetworkAccessManager manager;
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);

  QNetworkReply* reply = manager.get(request);
  QEventLoop loop;
  QTimer timeoutTimer;
  timeoutTimer.setSingleShot(true);
  timeoutTimer.setInterval(std::max(1000, timeoutMs));

  bool timedOut = false;
  QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
    timedOut = true;
    if (reply && reply->isRunning()) {
      reply->abort();
    }
  });
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

  timeoutTimer.start();
  loop.exec();

  const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const QNetworkReply::NetworkError networkError = reply->error();
  const QByteArray payload = reply->readAll();
  reply->deleteLater();

  if (networkError != QNetworkReply::NoError) {
    if (errorMessage) {
      if (timedOut || networkError == QNetworkReply::TimeoutError) {
        *errorMessage = QStringLiteral("Request timed out.");
      } else if (statusCode == 404) {
        *errorMessage = QStringLiteral("Requested file was not found.");
      } else {
        *errorMessage = QStringLiteral("Request failed.");
      }
    }
    return false;
  }

  if (responseText) {
    *responseText = QString::fromUtf8(payload);
  }
  return true;
}

void centerDialogOnScreen(QDialog* dialog, const QWidget* anchor) {
  if (!dialog) {
    return;
  }

  QRect anchorRect;
  if (anchor) {
    anchorRect = anchor->frameGeometry();
  } else {
    const QPoint cursor = QCursor::pos();
    anchorRect = QRect(cursor, QSize(1, 1));
  }

  QScreen* screen = QGuiApplication::screenAt(anchorRect.center());
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }
  if (!screen) {
    return;
  }

  const QRect area = screen->availableGeometry();
  const QSize size = dialog->size();

  int x = anchorRect.center().x() - (size.width() / 2);
  int y = anchorRect.center().y() - (size.height() / 2);

  x = std::clamp(x, area.left(), std::max(area.left(), area.right() - size.width()));
  y = std::clamp(y, area.top(), std::max(area.top(), area.bottom() - size.height()));
  dialog->move(x, y);
}

bool downloadFileWithProgress(
    QWidget* parent,
    const QUrl& url,
    const QString& userAgent,
    const QString& outputPath,
    const QString& expectedSha256,
    QString* errorMessage) {
  if (!url.isValid()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Invalid update download URL.");
    }
    return false;
  }

  QFile output(outputPath);
  if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to create update file at %1.").arg(outputPath);
    }
    return false;
  }

  QNetworkAccessManager manager;
  QNetworkRequest request(url);
  request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
  request.setRawHeader("Accept", "application/octet-stream");

  QNetworkReply* reply = manager.get(request);
  QProgressDialog progress(
      QStringLiteral("Downloading update package..."),
      QStringLiteral("Cancel"),
      0,
      100,
      parent);
  progress.setWindowTitle(QStringLiteral("Updating QuickQC"));
  progress.setWindowModality(Qt::ApplicationModal);
  progress.setMinimumDuration(0);
  progress.setAutoClose(false);
  progress.setAutoReset(false);
  progress.setValue(0);

  QEventLoop loop;
  QTimer timeoutTimer(parent);
  timeoutTimer.setSingleShot(true);
  timeoutTimer.setInterval(180000);

  bool canceled = false;
  bool timedOut = false;
  QCryptographicHash hash(QCryptographicHash::Sha256);

  const QMetaObject::Connection cancelConnection = QObject::connect(&progress, &QProgressDialog::canceled, parent, [reply, &canceled]() {
    canceled = true;
    if (reply && reply->isRunning()) {
      reply->abort();
    }
  });
  QObject::connect(&timeoutTimer, &QTimer::timeout, parent, [reply, &timedOut]() {
    timedOut = true;
    if (reply && reply->isRunning()) {
      reply->abort();
    }
  });
  QObject::connect(reply, &QNetworkReply::readyRead, parent, [reply, &output, &hash]() {
    const QByteArray data = reply->readAll();
    if (!data.isEmpty()) {
      output.write(data);
      hash.addData(data);
    }
  });
  QObject::connect(reply, &QNetworkReply::downloadProgress, parent, [&progress](const qint64 received, const qint64 total) {
    if (total > 0) {
      progress.setRange(0, 100);
      progress.setValue(static_cast<int>((received * 100) / total));
      return;
    }
    progress.setRange(0, 0);
  });
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

  timeoutTimer.start();
  progress.show();
  loop.exec();
  const bool userCanceled = canceled;
  QObject::disconnect(cancelConnection);

  const QByteArray remaining = reply->readAll();
  if (!remaining.isEmpty()) {
    output.write(remaining);
    hash.addData(remaining);
  }

  output.flush();
  output.close();
  progress.hide();

  const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const QNetworkReply::NetworkError networkError = reply->error();
  reply->deleteLater();

  if (userCanceled) {
    QFile::remove(outputPath);
    if (errorMessage) {
      *errorMessage = QStringLiteral("Update download canceled.");
    }
    return false;
  }

  if (networkError != QNetworkReply::NoError) {
    QFile::remove(outputPath);
    if (errorMessage) {
      if (timedOut || networkError == QNetworkReply::TimeoutError) {
        *errorMessage = QStringLiteral("Update download timed out.");
      } else if (statusCode >= 500) {
        *errorMessage = QStringLiteral("Server unavailable while downloading update.");
      } else {
        *errorMessage = QStringLiteral("Failed to download update package.");
      }
    }
    return false;
  }

  const QString expected = expectedSha256.trimmed().toLower();
  if (!expected.isEmpty()) {
    const QString actual = QString::fromLatin1(hash.result().toHex()).toLower();
    if (actual != expected) {
      QFile::remove(outputPath);
      if (errorMessage) {
        *errorMessage = QStringLiteral("Update package checksum mismatch. Install aborted.");
      }
      return false;
    }
  }

  return true;
}

bool extractArchiveToDirectory(const QString& archivePath, const QString& destinationDir, QString* errorMessage) {
#if defined(Q_OS_WIN)
  const QString lowerPath = archivePath.toLower();
  if (lowerPath.endsWith(QStringLiteral(".zip"))) {
    QProcess unzip;
    unzip.start(
        QStringLiteral("powershell"),
        {
            QStringLiteral("-NoProfile"),
            QStringLiteral("-ExecutionPolicy"),
            QStringLiteral("Bypass"),
            QStringLiteral("-Command"),
            QStringLiteral("Expand-Archive -Path $args[0] -DestinationPath $args[1] -Force"),
            archivePath,
            destinationDir,
        });

    if (!unzip.waitForStarted(5000)) {
      if (errorMessage) {
        *errorMessage = QStringLiteral("Failed to start ZIP extraction.");
      }
      return false;
    }

    if (!unzip.waitForFinished(180000)) {
      unzip.kill();
      unzip.waitForFinished();
      if (errorMessage) {
        *errorMessage = QStringLiteral("Timed out while extracting update package.");
      }
      return false;
    }

    if (unzip.exitStatus() != QProcess::NormalExit || unzip.exitCode() != 0) {
      if (errorMessage) {
        const QString stderrOut = QString::fromUtf8(unzip.readAllStandardError()).trimmed();
        const QString stdoutOut = QString::fromUtf8(unzip.readAllStandardOutput()).trimmed();
        const QString detail = !stderrOut.isEmpty() ? stderrOut : stdoutOut;
        *errorMessage = detail.isEmpty()
                            ? QStringLiteral("Failed to extract update package.")
                            : detail;
      }
      return false;
    }

    return true;
  }
#endif

  QProcess tar;
  tar.start(
      QStringLiteral("tar"),
      {QStringLiteral("-xzf"), archivePath, QStringLiteral("-C"), destinationDir});

  if (!tar.waitForStarted(3000)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to start archive extraction.");
    }
    return false;
  }

  if (!tar.waitForFinished(180000)) {
    tar.kill();
    tar.waitForFinished();
    if (errorMessage) {
      *errorMessage = QStringLiteral("Timed out while extracting update package.");
    }
    return false;
  }

  if (tar.exitStatus() != QProcess::NormalExit || tar.exitCode() != 0) {
    if (errorMessage) {
      const QString stderrOut = QString::fromUtf8(tar.readAllStandardError()).trimmed();
      *errorMessage = stderrOut.isEmpty()
                          ? QStringLiteral("Failed to extract update package.")
                          : stderrOut;
    }
    return false;
  }

  return true;
}

bool stageMacUpdate(
    QWidget* parent,
    const QString& assetUrl,
    const QString& userAgent,
    const QString& expectedSha256,
    QString* stagedAppPath,
    QString* errorMessage) {
#if !defined(Q_OS_MAC)
  Q_UNUSED(parent);
  Q_UNUSED(assetUrl);
  Q_UNUSED(userAgent);
  Q_UNUSED(expectedSha256);
  Q_UNUSED(stagedAppPath);
  if (errorMessage) {
    *errorMessage = QStringLiteral("Automatic update installation is supported on macOS only.");
  }
  return false;
#else
  const QString stagingRoot = QDir::temp().filePath(
      QStringLiteral("quickqc-update-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
  if (!QDir().mkpath(stagingRoot)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to create update staging directory.");
    }
    return false;
  }

  const QString archivePath = QDir(stagingRoot).filePath(QStringLiteral("quickqc-update.tar.gz"));
  if (!downloadFileWithProgress(parent, QUrl(assetUrl), userAgent, archivePath, expectedSha256, errorMessage)) {
    return false;
  }

  const QString extractDir = QDir(stagingRoot).filePath(QStringLiteral("extract"));
  if (!QDir().mkpath(extractDir)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to prepare extracted update folder.");
    }
    return false;
  }

  if (!extractArchiveToDirectory(archivePath, extractDir, errorMessage)) {
    return false;
  }

  const QString appPath = QDir(extractDir).filePath(QStringLiteral("quickqc.app"));
  const QFileInfo appInfo(appPath);
  if (!appInfo.exists() || !appInfo.isDir()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Downloaded package does not contain quickqc.app.");
    }
    return false;
  }

  if (stagedAppPath) {
    *stagedAppPath = appPath;
  }
  return true;
#endif
}

bool stageLinuxUpdate(
    QWidget* parent,
    const QString& assetUrl,
    const QString& userAgent,
    const QString& expectedSha256,
    QString* stagedBinaryPath,
    QString* errorMessage) {
#if !defined(Q_OS_LINUX)
  Q_UNUSED(parent);
  Q_UNUSED(assetUrl);
  Q_UNUSED(userAgent);
  Q_UNUSED(expectedSha256);
  Q_UNUSED(stagedBinaryPath);
  if (errorMessage) {
    *errorMessage = QStringLiteral("Automatic update installation is supported on Linux only.");
  }
  return false;
#else
  const QString stagingRoot = QDir::temp().filePath(
      QStringLiteral("quickqc-update-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
  if (!QDir().mkpath(stagingRoot)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to create update staging directory.");
    }
    return false;
  }

  const QString archivePath = QDir(stagingRoot).filePath(QStringLiteral("quickqc-update.tar.gz"));
  if (!downloadFileWithProgress(parent, QUrl(assetUrl), userAgent, archivePath, expectedSha256, errorMessage)) {
    return false;
  }

  const QString extractDir = QDir(stagingRoot).filePath(QStringLiteral("extract"));
  if (!QDir().mkpath(extractDir)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to prepare extracted update folder.");
    }
    return false;
  }

  if (!extractArchiveToDirectory(archivePath, extractDir, errorMessage)) {
    return false;
  }

  const QString binaryPath = QDir(extractDir).filePath(QStringLiteral("quickqc"));
  const QFileInfo binaryInfo(binaryPath);
  if (!binaryInfo.exists() || !binaryInfo.isFile()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Downloaded package does not contain quickqc binary.");
    }
    return false;
  }

  if (stagedBinaryPath) {
    *stagedBinaryPath = binaryPath;
  }
  return true;
#endif
}

bool stageWindowsUpdate(
    QWidget* parent,
    const QString& assetUrl,
    const QString& userAgent,
    const QString& expectedSha256,
    QString* stagedExePath,
    QString* errorMessage) {
#if !defined(Q_OS_WIN)
  Q_UNUSED(parent);
  Q_UNUSED(assetUrl);
  Q_UNUSED(userAgent);
  Q_UNUSED(expectedSha256);
  Q_UNUSED(stagedExePath);
  if (errorMessage) {
    *errorMessage = QStringLiteral("Automatic update installation is supported on Windows only.");
  }
  return false;
#else
  const QString stagingRoot = QDir::temp().filePath(
      QStringLiteral("quickqc-update-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
  if (!QDir().mkpath(stagingRoot)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to create update staging directory.");
    }
    return false;
  }

  const QString archivePath = QDir(stagingRoot).filePath(QStringLiteral("quickqc-update.zip"));
  if (!downloadFileWithProgress(parent, QUrl(assetUrl), userAgent, archivePath, expectedSha256, errorMessage)) {
    return false;
  }

  const QString extractDir = QDir(stagingRoot).filePath(QStringLiteral("extract"));
  if (!QDir().mkpath(extractDir)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to prepare extracted update folder.");
    }
    return false;
  }

  if (!extractArchiveToDirectory(archivePath, extractDir, errorMessage)) {
    return false;
  }

  const QString candidate1 = QDir(extractDir).filePath(QStringLiteral("quickqc.exe"));
  const QString candidate2 = QDir(extractDir).filePath(QStringLiteral("Release/quickqc.exe"));
  QString selected;
  if (QFileInfo::exists(candidate1)) {
    selected = candidate1;
  } else if (QFileInfo::exists(candidate2)) {
    selected = candidate2;
  }

  if (selected.isEmpty()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Downloaded package does not contain quickqc.exe.");
    }
    return false;
  }

  if (stagedExePath) {
    *stagedExePath = selected;
  }
  return true;
#endif
}

QString currentMacAppBundlePath() {
#if !defined(Q_OS_MAC)
  return QString();
#else
  QDir dir = QFileInfo(QCoreApplication::applicationFilePath()).dir();
  if (dir.dirName() == QStringLiteral("MacOS")) {
    if (dir.cdUp() && dir.dirName() == QStringLiteral("Contents") && dir.cdUp()) {
      const QString path = dir.absolutePath();
      if (path.endsWith(QStringLiteral(".app"), Qt::CaseInsensitive)) {
        return path;
      }
    }
  }
  return QStringLiteral("/Applications/quickqc.app");
#endif
}

bool launchMacInstallHelper(
    const QString& stagedAppPath,
    const QString& preferredTargetPath,
    const bool relaunchAfterInstall,
    QString* errorMessage) {
#if !defined(Q_OS_MAC)
  Q_UNUSED(stagedAppPath);
  Q_UNUSED(preferredTargetPath);
  Q_UNUSED(relaunchAfterInstall);
  if (errorMessage) {
    *errorMessage = QStringLiteral("Automatic update installation is supported on macOS only.");
  }
  return false;
#else
  const QFileInfo appInfo(stagedAppPath);
  if (!appInfo.exists() || !appInfo.isDir()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Staged update app is missing.");
    }
    return false;
  }

  const QString scriptPath = QDir(appInfo.absolutePath()).filePath(QStringLiteral("quickqc-install-update.sh"));
  QFile script(scriptPath);
  if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to write update installer helper.");
    }
    return false;
  }

  QTextStream out(&script);
  out << "#!/bin/sh\n";
  out << "set -eu\n";
  out << "PID=\"$1\"\n";
  out << "SRC_APP=\"$2\"\n";
  out << "PREFERRED_DEST=\"$3\"\n";
  out << "RELAUNCH=\"$4\"\n";
  out << "install_to() {\n";
  out << "  DEST=\"$1\"\n";
  out << "  DEST_NEW=\"${DEST}.new\"\n";
  out << "  DEST_BACKUP=\"${DEST}.backup\"\n";
  out << "  mkdir -p \"$(dirname \"$DEST\")\"\n";
  out << "  rm -rf \"$DEST_NEW\"\n";
  out << "  rm -rf \"$DEST_BACKUP\"\n";
  out << "  if cp -R \"$SRC_APP\" \"$DEST_NEW\"; then\n";
  out << "    if [ -d \"$DEST\" ]; then\n";
  out << "      mv \"$DEST\" \"$DEST_BACKUP\" || true\n";
  out << "    fi\n";
  out << "    if mv \"$DEST_NEW\" \"$DEST\"; then\n";
  out << "      rm -rf \"$DEST_BACKUP\"\n";
  out << "      echo \"$DEST\"\n";
  out << "      return 0\n";
  out << "    fi\n";
  out << "    rm -rf \"$DEST\"\n";
  out << "    if [ -d \"$DEST_BACKUP\" ]; then mv \"$DEST_BACKUP\" \"$DEST\" || true; fi\n";
  out << "    rm -rf \"$DEST_NEW\"\n";
  out << "    return 1\n";
  out << "  fi\n";
  out << "  rm -rf \"$DEST_BACKUP\"\n";
  out << "  rm -rf \"$DEST_NEW\"\n";
  out << "  return 1\n";
  out << "}\n";
  out << "while kill -0 \"$PID\" >/dev/null 2>&1; do\n";
  out << "  sleep 0.25\n";
  out << "done\n";
  out << "INSTALLED=\"\"\n";
  out << "if [ -n \"$PREFERRED_DEST\" ] && INSTALLED=$(install_to \"$PREFERRED_DEST\"); then\n";
  out << "  :\n";
  out << "elif INSTALLED=$(install_to \"/Applications/quickqc.app\"); then\n";
  out << "  :\n";
  out << "elif INSTALLED=$(install_to \"$HOME/Applications/quickqc.app\"); then\n";
  out << "  :\n";
  out << "else\n";
  out << "  exit 1\n";
  out << "fi\n";
  out << "if [ \"$RELAUNCH\" = \"1\" ]; then\n";
  out << "  open \"$INSTALLED\"\n";
  out << "fi\n";
  script.flush();
  script.close();

  if (!QFile::setPermissions(
          scriptPath,
          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to make update helper executable.");
    }
    return false;
  }

  const QStringList args = {
      scriptPath,
      QString::number(QCoreApplication::applicationPid()),
      stagedAppPath,
      preferredTargetPath,
      relaunchAfterInstall ? QStringLiteral("1") : QStringLiteral("0"),
  };
  if (!QProcess::startDetached(QStringLiteral("/bin/sh"), args)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to launch update installer helper.");
    }
    return false;
  }

  return true;
#endif
}

bool launchLinuxInstallHelper(
    const QString& stagedBinaryPath,
    const QString& targetBinaryPath,
    const bool relaunchAfterInstall,
    QString* errorMessage) {
#if !defined(Q_OS_LINUX)
  Q_UNUSED(stagedBinaryPath);
  Q_UNUSED(targetBinaryPath);
  Q_UNUSED(relaunchAfterInstall);
  if (errorMessage) {
    *errorMessage = QStringLiteral("Automatic update installation is supported on Linux only.");
  }
  return false;
#else
  const QFileInfo stagedInfo(stagedBinaryPath);
  if (!stagedInfo.exists() || !stagedInfo.isFile()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Staged update binary is missing.");
    }
    return false;
  }

  const QString scriptPath = QDir(stagedInfo.absolutePath()).filePath(QStringLiteral("quickqc-install-update.sh"));
  QFile script(scriptPath);
  if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to write update installer helper.");
    }
    return false;
  }

  QTextStream out(&script);
  out << "#!/bin/sh\n";
  out << "set -eu\n";
  out << "PID=\"$1\"\n";
  out << "SRC_BIN=\"$2\"\n";
  out << "DEST_BIN=\"$3\"\n";
  out << "RELAUNCH=\"$4\"\n";
  out << "BACKUP=\"${DEST_BIN}.backup\"\n";
  out << "while kill -0 \"$PID\" >/dev/null 2>&1; do\n";
  out << "  sleep 0.25\n";
  out << "done\n";
  out << "mkdir -p \"$(dirname \"$DEST_BIN\")\"\n";
  out << "rm -f \"$BACKUP\"\n";
  out << "if [ -f \"$DEST_BIN\" ]; then\n";
  out << "  cp \"$DEST_BIN\" \"$BACKUP\" || true\n";
  out << "fi\n";
  out << "if cp \"$SRC_BIN\" \"$DEST_BIN\"; then\n";
  out << "  chmod 755 \"$DEST_BIN\" || true\n";
  out << "  rm -f \"$BACKUP\"\n";
  out << "else\n";
  out << "  if [ -f \"$BACKUP\" ]; then cp \"$BACKUP\" \"$DEST_BIN\" || true; fi\n";
  out << "  exit 1\n";
  out << "fi\n";
  out << "if [ \"$RELAUNCH\" = \"1\" ]; then\n";
  out << "  \"$DEST_BIN\" >/dev/null 2>&1 &\n";
  out << "fi\n";
  script.flush();
  script.close();

  if (!QFile::setPermissions(
          scriptPath,
          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to make update helper executable.");
    }
    return false;
  }

  const QStringList args = {
      scriptPath,
      QString::number(QCoreApplication::applicationPid()),
      stagedBinaryPath,
      targetBinaryPath,
      relaunchAfterInstall ? QStringLiteral("1") : QStringLiteral("0"),
  };
  if (!QProcess::startDetached(QStringLiteral("/bin/sh"), args)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to launch update installer helper.");
    }
    return false;
  }

  return true;
#endif
}

bool launchWindowsInstallHelper(
    const QString& stagedExePath,
    const QString& targetExePath,
    const bool relaunchAfterInstall,
    QString* errorMessage) {
#if !defined(Q_OS_WIN)
  Q_UNUSED(stagedExePath);
  Q_UNUSED(targetExePath);
  Q_UNUSED(relaunchAfterInstall);
  if (errorMessage) {
    *errorMessage = QStringLiteral("Automatic update installation is supported on Windows only.");
  }
  return false;
#else
  const QFileInfo stagedInfo(stagedExePath);
  if (!stagedInfo.exists() || !stagedInfo.isFile()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Staged update binary is missing.");
    }
    return false;
  }

  const QString scriptPath = QDir(stagedInfo.absolutePath()).filePath(QStringLiteral("quickqc-install-update.ps1"));
  QFile script(scriptPath);
  if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to write update installer helper.");
    }
    return false;
  }

  QTextStream out(&script);
  out << "param(\n";
  out << "  [int]$PidToWait,\n";
  out << "  [string]$SourceExe,\n";
  out << "  [string]$TargetExe,\n";
  out << "  [int]$Relaunch\n";
  out << ")\n";
  out << "$ErrorActionPreference = 'Stop'\n";
  out << "$targetDir = Split-Path -Path $TargetExe -Parent\n";
  out << "if (-not (Test-Path $targetDir)) { New-Item -ItemType Directory -Path $targetDir -Force | Out-Null }\n";
  out << "$backup = \"$TargetExe.backup\"\n";
  out << "for ($i = 0; $i -lt 240; $i++) {\n";
  out << "  if (-not (Get-Process -Id $PidToWait -ErrorAction SilentlyContinue)) { break }\n";
  out << "  Start-Sleep -Milliseconds 250\n";
  out << "}\n";
  out << "try {\n";
  out << "  if (Test-Path $TargetExe) { Copy-Item -Path $TargetExe -Destination $backup -Force }\n";
  out << "  Copy-Item -Path $SourceExe -Destination $TargetExe -Force\n";
  out << "  Remove-Item -Path $backup -Force -ErrorAction SilentlyContinue\n";
  out << "  if ($Relaunch -eq 1) { Start-Process -FilePath $TargetExe }\n";
  out << "} catch {\n";
  out << "  if (Test-Path $backup) { Copy-Item -Path $backup -Destination $TargetExe -Force }\n";
  out << "  exit 1\n";
  out << "}\n";
  script.flush();
  script.close();

  const QStringList args = {
      QStringLiteral("-NoProfile"),
      QStringLiteral("-ExecutionPolicy"),
      QStringLiteral("Bypass"),
      QStringLiteral("-File"),
      scriptPath,
      QString::number(QCoreApplication::applicationPid()),
      stagedExePath,
      targetExePath,
      relaunchAfterInstall ? QStringLiteral("1") : QStringLiteral("0"),
  };

  if (!QProcess::startDetached(QStringLiteral("powershell"), args)) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to launch update installer helper.");
    }
    return false;
  }

  return true;
#endif
}

bool stageUpdateForCurrentPlatform(
    QWidget* parent,
    const QString& assetUrl,
    const QString& userAgent,
    const QString& expectedSha256,
    QString* stagedPayloadPath,
    QString* errorMessage) {
#if defined(Q_OS_MAC)
  return stageMacUpdate(parent, assetUrl, userAgent, expectedSha256, stagedPayloadPath, errorMessage);
#elif defined(Q_OS_WIN)
  return stageWindowsUpdate(parent, assetUrl, userAgent, expectedSha256, stagedPayloadPath, errorMessage);
#elif defined(Q_OS_LINUX)
  return stageLinuxUpdate(parent, assetUrl, userAgent, expectedSha256, stagedPayloadPath, errorMessage);
#else
  Q_UNUSED(parent);
  Q_UNUSED(assetUrl);
  Q_UNUSED(userAgent);
  Q_UNUSED(expectedSha256);
  Q_UNUSED(stagedPayloadPath);
  if (errorMessage) {
    *errorMessage = QStringLiteral("Automatic updater is not available on this platform.");
  }
  return false;
#endif
}

bool launchInstallHelperForCurrentPlatform(
    const QString& stagedPayloadPath,
    const bool relaunchAfterInstall,
    QString* errorMessage) {
#if defined(Q_OS_MAC)
  return launchMacInstallHelper(
      stagedPayloadPath,
      currentMacAppBundlePath(),
      relaunchAfterInstall,
      errorMessage);
#elif defined(Q_OS_WIN)
  return launchWindowsInstallHelper(
      stagedPayloadPath,
      QCoreApplication::applicationFilePath(),
      relaunchAfterInstall,
      errorMessage);
#elif defined(Q_OS_LINUX)
  return launchLinuxInstallHelper(
      stagedPayloadPath,
      QCoreApplication::applicationFilePath(),
      relaunchAfterInstall,
      errorMessage);
#else
  Q_UNUSED(stagedPayloadPath);
  Q_UNUSED(relaunchAfterInstall);
  if (errorMessage) {
    *errorMessage = QStringLiteral("Automatic updater is not available on this platform.");
  }
  return false;
#endif
}

QString manualUpdateHintForCurrentPlatform() {
#if defined(Q_OS_MAC)
  return QStringLiteral("brew update && brew upgrade --cask quickqc");
#elif defined(Q_OS_WIN)
  return QStringLiteral("PowerShell: irm https://raw.githubusercontent.com/MuyleangIng/quickqc/main/scripts/install-windows.ps1 | iex");
#elif defined(Q_OS_LINUX)
  return QStringLiteral("curl -fsSL https://raw.githubusercontent.com/MuyleangIng/quickqc/main/scripts/install-linux.sh | bash");
#else
  return QStringLiteral("Download the latest release from GitHub.");
#endif
}

class GpuImagePreviewWidget final : public QOpenGLWidget {
 public:
  explicit GpuImagePreviewWidget(const QImage& image, QWidget* parent = nullptr)
      : QOpenGLWidget(parent), image_(image) {
    setMinimumSize(300, 220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  }

 protected:
  void paintGL() override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.fillRect(rect(), QColor(10, 14, 20));

    if (image_.isNull()) {
      return;
    }

    const QPixmap pixmap = QPixmap::fromImage(image_);
    const QSize targetSize = pixmap.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect targetRect(
        (width() - targetSize.width()) / 2,
        (height() - targetSize.height()) / 2,
        targetSize.width(),
        targetSize.height());
    painter.drawPixmap(targetRect, pixmap);
  }

 private:
  QImage image_;
};
}

MainWindow::MainWindow(ClipStorage* storage, ClipboardWatcher* watcher, QWidget* parent)
    : QMainWindow(parent),
      storage_(storage),
      watcher_(watcher),
      clipboard_(QGuiApplication::clipboard()),
      searchEdit_(nullptr),
      listWidget_(nullptr),
      importButton_(nullptr),
      settingsButton_(nullptr),
      clearButton_(nullptr),
      openShortcut_(nullptr),
      subtitleLabel_(nullptr),
      storeSummaryLabel_(nullptr),
      cpuIconLabel_(nullptr),
      cpuSummaryLabel_(nullptr),
      toastLabel_(nullptr),
      statusLabel_(nullptr),
      tray_(nullptr),
      searchDebounceTimer_(new QTimer(this)),
      refreshCoalesceTimer_(new QTimer(this)),
      processUsageTimer_(new QTimer(this)),
      toastTimer_(new QTimer(this)),
      settingsStore_(),
      trayMenu_(nullptr),
      trayOpenClipboardAction_(nullptr),
      trayOpenSettingsAction_(nullptr),
      trayAutoCloseAction_(nullptr),
      trayStartupAction_(nullptr),
      trayUpdateAction_(nullptr),
      trayHideAction_(nullptr),
      trayQuitAction_(nullptr),
      startupSilentUpdateCheck_(false) {
  Q_ASSERT(storage_ != nullptr);

  setWindowTitle(QString::fromUtf8(kAppDisplayName));
  setWindowFlag(Qt::WindowStaysOnTopHint, true);
  resize(430, 560);
  setMinimumSize(390, 430);

  loadSettings();
  detectGpuSupport();
  if (appSettings_.gpuPreviewEnabled && !gpuInfo_.supported) {
    appSettings_.gpuPreviewEnabled = false;
    saveSettings();
  }

  searchDebounceTimer_->setSingleShot(true);
  searchDebounceTimer_->setInterval(70);
  connect(searchDebounceTimer_, &QTimer::timeout, this, &MainWindow::refresh);

  refreshCoalesceTimer_->setSingleShot(true);
  refreshCoalesceTimer_->setInterval(35);
  connect(refreshCoalesceTimer_, &QTimer::timeout, this, &MainWindow::refresh);

  // Efficient footer update: slower interval and only while visible.
  processUsageTimer_->setInterval(5000);
  connect(processUsageTimer_, &QTimer::timeout, this, &MainWindow::refreshProcessUsage);

  toastTimer_->setSingleShot(true);
  connect(toastTimer_, &QTimer::timeout, this, [this]() {
    if (toastLabel_) {
      toastLabel_->hide();
    }
  });

  setupUi();
  applyStyles();
  setupTray();
  setupShortcuts();
  refresh();
  refreshProcessUsage();
  processUsageTimer_->start();

  if (appSettings_.autoCheckOnStartup) {
    QTimer::singleShot(2500, this, [this]() {
      if (!appSettings_.autoCheckOnStartup) {
        return;
      }
      startupSilentUpdateCheck_ = true;
      onCheckUpdates();
    });
  }
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
  auto* central = new QWidget(this);
  central->setObjectName(QStringLiteral("root"));

  auto* root = new QVBoxLayout(central);
  root->setContentsMargins(10, 10, 10, 10);
  root->setSpacing(8);

  auto* topRow = new QHBoxLayout();
  topRow->setSpacing(6);

  auto* titleWrap = new QVBoxLayout();
  titleWrap->setSpacing(1);

  auto* title = new QLabel(QString::fromUtf8(kAppDisplayName), central);
  title->setObjectName(QStringLiteral("title"));

  QString openHotkeyLabel = QKeySequence::fromString(
      appSettings_.openHotkey,
      QKeySequence::PortableText).toString(QKeySequence::NativeText);
  if (openHotkeyLabel.trimmed().isEmpty()) {
    openHotkeyLabel = QKeySequence::fromString(
        defaultOpenHotkeyPortable(),
        QKeySequence::PortableText).toString(QKeySequence::NativeText);
  }

  subtitleLabel_ = new QLabel(
      QStringLiteral("Compact clipboard manager • %1 to open • Esc to close")
          .arg(openHotkeyLabel),
      central);
  subtitleLabel_->setObjectName(QStringLiteral("subtitle"));

  titleWrap->addWidget(title);
  titleWrap->addWidget(subtitleLabel_);

  auto* topActions = new QHBoxLayout();
  topActions->setSpacing(5);

  importButton_ = new QPushButton(QStringLiteral("Import"), central);
  importButton_->setObjectName(QStringLiteral("ghostBtn"));

  settingsButton_ = new QPushButton(QStringLiteral("Settings"), central);
  settingsButton_->setObjectName(QStringLiteral("ghostBtn"));

  clearButton_ = new QPushButton(QStringLiteral("Clear"), central);
  clearButton_->setObjectName(QStringLiteral("dangerBtn"));

  topActions->addWidget(importButton_);
  topActions->addWidget(settingsButton_);
  topActions->addWidget(clearButton_);

  topRow->addLayout(titleWrap, 1);
  topRow->addLayout(topActions);

  searchEdit_ = new QLineEdit(central);
  searchEdit_->setObjectName(QStringLiteral("search"));
  searchEdit_->setPlaceholderText(
      QStringLiteral("Search text, image name, tags, or source path..."));
  searchEdit_->setClearButtonEnabled(true);

  listWidget_ = new QListWidget(central);
  listWidget_->setObjectName(QStringLiteral("history"));
  listWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
  listWidget_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
  listWidget_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto* statsRow = new QHBoxLayout();
  statsRow->setSpacing(6);

  storeSummaryLabel_ = new QLabel(QStringLiteral("Store -"), central);
  storeSummaryLabel_->setObjectName(QStringLiteral("topStat"));

  cpuIconLabel_ = new QLabel(central);
  cpuIconLabel_->setObjectName(QStringLiteral("cpuIcon"));
  cpuIconLabel_->setPixmap(style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(14, 14));

  cpuSummaryLabel_ = new QLabel(QStringLiteral("CPU/RAM -"), central);
  cpuSummaryLabel_->setObjectName(QStringLiteral("topStat"));

  statsRow->addWidget(storeSummaryLabel_, 1);
  statsRow->addWidget(cpuIconLabel_);
  statsRow->addWidget(cpuSummaryLabel_);

  toastLabel_ = new QLabel(central);
  toastLabel_->setObjectName(QStringLiteral("copyToast"));
  toastLabel_->setAlignment(Qt::AlignCenter);
  toastLabel_->setVisible(false);
  toastLabel_->setMinimumHeight(26);

  statusLabel_ = new QLabel(QStringLiteral("Ready"), central);
  statusLabel_->setObjectName(QStringLiteral("status"));

  root->addLayout(topRow);
  root->addWidget(searchEdit_);
  root->addWidget(toastLabel_);
  root->addLayout(statsRow);
  root->addWidget(listWidget_, 1);
  root->addWidget(statusLabel_);

  setCentralWidget(central);

  connect(searchEdit_, &QLineEdit::textChanged, this, &MainWindow::onSearchChanged);
  connect(importButton_, &QPushButton::clicked, this, &MainWindow::onImportImage);
  connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::onOpenSettings);
  connect(clearButton_, &QPushButton::clicked, this, &MainWindow::onClearAll);
  connect(listWidget_, &QListWidget::itemDoubleClicked, this, &MainWindow::onItemActivated);
  connect(listWidget_, &QListWidget::currentRowChanged, this, &MainWindow::onCurrentRowChanged);
}

void MainWindow::setupTray() {
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    return;
  }

  tray_ = new QSystemTrayIcon(this);
  tray_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
  tray_->setToolTip(QString::fromUtf8(kAppDisplayName));

  trayMenu_ = new QMenu(this);
  trayOpenClipboardAction_ = trayMenu_->addAction(QStringLiteral("Open Clipboard"));
  trayOpenSettingsAction_ = trayMenu_->addAction(QStringLiteral("Open Settings"));
  trayMenu_->addSeparator();

  trayAutoCloseAction_ = trayMenu_->addAction(QStringLiteral("Auto Close After Copy"));
  trayAutoCloseAction_->setCheckable(true);

  trayStartupAction_ = trayMenu_->addAction(QStringLiteral("Start at Login"));
  trayStartupAction_->setCheckable(true);

  trayUpdateAction_ = trayMenu_->addAction(QStringLiteral("Check Updates"));
  trayMenu_->addSeparator();
  trayHideAction_ = trayMenu_->addAction(QStringLiteral("Hide"));
  trayQuitAction_ = trayMenu_->addAction(QStringLiteral("Quit"));

  // Delay tray actions one event-cycle so the native tray menu fully closes first.
  connect(trayOpenClipboardAction_, &QAction::triggered, this, [this]() {
    QTimer::singleShot(0, this, &MainWindow::showNearCursor);
  });
  connect(trayOpenSettingsAction_, &QAction::triggered, this, [this]() {
    QTimer::singleShot(0, this, &MainWindow::onOpenSettings);
  });

  connect(trayAutoCloseAction_, &QAction::toggled, this, [this](const bool checked) {
    appSettings_.autoCloseOnCopy = checked;
    saveSettings();
    statusLabel_->setText(checked ? QStringLiteral("Auto-close enabled.") : QStringLiteral("Auto-close disabled."));
  });

  connect(trayStartupAction_, &QAction::toggled, this, [this](const bool checked) {
    QString startupError;
    if (!setLaunchAtLogin(checked, &startupError)) {
      QSignalBlocker blocker(trayStartupAction_);
      trayStartupAction_->setChecked(appSettings_.startAtLogin);
      QMessageBox::warning(this, QStringLiteral("Startup"), startupError);
      return;
    }

    appSettings_.startAtLogin = checked;
    saveSettings();
    statusLabel_->setText(checked ? QStringLiteral("Start at login enabled.")
                                  : QStringLiteral("Start at login disabled."));
  });

  connect(trayUpdateAction_, &QAction::triggered, this, [this]() {
    QTimer::singleShot(0, this, &MainWindow::onCheckUpdates);
  });
  connect(trayHideAction_, &QAction::triggered, this, &MainWindow::hide);
  connect(trayQuitAction_, &QAction::triggered, qApp, &QApplication::quit);
  connect(trayMenu_, &QMenu::aboutToShow, this, &MainWindow::rebuildTrayMenu);

  tray_->setContextMenu(trayMenu_);
  connect(tray_, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
  rebuildTrayMenu();
  tray_->show();
}

void MainWindow::rebuildTrayMenu() {
  if (!trayMenu_) {
    return;
  }

  if (trayAutoCloseAction_) {
    QSignalBlocker blocker(trayAutoCloseAction_);
    trayAutoCloseAction_->setChecked(appSettings_.autoCloseOnCopy);
  }

  if (trayStartupAction_) {
    QSignalBlocker blocker(trayStartupAction_);
    trayStartupAction_->setChecked(appSettings_.startAtLogin);
  }
}

void MainWindow::showToast(const QString& message, int durationMs) {
  if (!toastLabel_) {
    return;
  }

  const QString trimmed = message.trimmed();
  if (trimmed.isEmpty()) {
    toastLabel_->hide();
    return;
  }

  toastLabel_->setText(QStringLiteral("✓ %1").arg(trimmed));
  toastLabel_->show();
  toastLabel_->raise();

  if (toastTimer_) {
    toastTimer_->start(std::clamp(durationMs, 500, 6000));
  }
}

void MainWindow::setupShortcuts() {
  openShortcut_ = new QShortcut(this);
  openShortcut_->setContext(Qt::ApplicationShortcut);
  connect(openShortcut_, &QShortcut::activated, this, &MainWindow::showNearCursor);
  applyOpenShortcut();

  auto* escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
  escShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(escShortcut, &QShortcut::activated, this, [this]() { hide(); });

  auto* copyShortcut = new QShortcut(QKeySequence::Copy, this);
  copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(copyShortcut, &QShortcut::activated, this, &MainWindow::onCopySelected);

  auto* findShortcut = new QShortcut(QKeySequence::Find, this);
  findShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(findShortcut, &QShortcut::activated, this, [this]() {
    searchEdit_->setFocus();
    searchEdit_->selectAll();
  });

  auto* editShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+E")), this);
  editShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(editShortcut, &QShortcut::activated, this, &MainWindow::onEditSelected);

  auto* enterShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
  enterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(enterShortcut, &QShortcut::activated, this, &MainWindow::onCopySelected);

  auto* deleteShortcut = new QShortcut(QKeySequence::Delete, this);
  deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(deleteShortcut, &QShortcut::activated, this, [this]() {
    const auto it = selectedClip();
    if (it.has_value()) {
      deleteClipById(it->id);
    }
  });
}

void MainWindow::applyOpenShortcut() {
  if (!openShortcut_) {
    return;
  }

  const QString normalized = normalizeHotkeySetting(appSettings_.openHotkey);
  appSettings_.openHotkey = normalized;
  openShortcut_->setKey(QKeySequence::fromString(normalized, QKeySequence::PortableText));
  updateSubtitleHotkeyLabel();
}

void MainWindow::updateSubtitleHotkeyLabel() {
  if (!subtitleLabel_) {
    return;
  }

  QString openHotkeyLabel = QKeySequence::fromString(
      appSettings_.openHotkey,
      QKeySequence::PortableText).toString(QKeySequence::NativeText);
  if (openHotkeyLabel.trimmed().isEmpty()) {
    openHotkeyLabel = QKeySequence::fromString(
        defaultOpenHotkeyPortable(),
        QKeySequence::PortableText).toString(QKeySequence::NativeText);
  }

  subtitleLabel_->setText(
      QStringLiteral("Compact clipboard manager • %1 to open • Esc to close")
          .arg(openHotkeyLabel));
}

void MainWindow::applyStyles() {
  const bool dark = isDarkTheme();

  const QString darkStyle = QStringLiteral(R"(
QWidget#root { background: #09111d; }
QLabel#title { color: #f5f9ff; font-size: 21px; font-weight: 700; }
QLabel#subtitle { color: #89a2c2; font-size: 11px; }
QLineEdit#search {
  border: 1px solid #22364e; border-radius: 9px; padding: 7px 10px;
  color: #e7f1ff; background: #111c2b; selection-background-color: #356fdd;
}
QLineEdit#search:focus { border-color: #3d8dff; }
QListWidget#history {
  border: 1px solid #1f3248; border-radius: 10px; background: #0f1828;
  padding: 4px; outline: none;
}
QListWidget#history::item { border: 0; margin: 3px 1px; padding: 0; }
QFrame#clipCard { border: 1px solid #283b51; border-radius: 9px; background: #152438; }
QFrame#clipCard[selectedCard="true"] { border-color: #4d94ff; background: #1a2b43; }
QLabel#cardMeta { color: #9cb3d1; font-size: 10px; }
QLabel#cardTitle { color: #eef5ff; font-size: 12px; font-weight: 600; }
QLabel#cardBody { color: #e7f1ff; font-size: 13px; font-weight: 600; }
QLabel#cardLink { color: #8ec3ff; font-size: 10px; }
QLabel#topStat { color: #9cb3d1; font-size: 10px; }
QLabel#cpuIcon { min-width: 14px; max-width: 14px; }
QDialog { background: #0f1828; color: #e6f0ff; }
QDialog QLabel { color: #e6f0ff; }
QDialog QCheckBox { color: #d8e7fc; }
QDialog QDoubleSpinBox, QDialog QComboBox, QDialog QKeySequenceEdit {
  color: #e6f0ff; background: #132034; border: 1px solid #29405b; border-radius: 8px;
}
QDialog QDoubleSpinBox::up-button, QDialog QDoubleSpinBox::down-button {
  width: 16px; border-left: 1px solid #29405b; background: #19314b;
}
QDialog QDoubleSpinBox::up-button:hover, QDialog QDoubleSpinBox::down-button:hover {
  background: #214264;
}
QDialog QDoubleSpinBox::up-arrow, QDialog QDoubleSpinBox::down-arrow {
  width: 8px; height: 8px;
}
QKeySequenceEdit#hotkeyEdit {
  min-width: 180px; padding: 4px 8px; font-weight: 600;
}
QPlainTextEdit#editTextArea, QLineEdit#renameLine {
  border: 1px solid #29405b; border-radius: 9px; background: #132034; color: #e6f0ff; padding: 8px;
}
QPlainTextEdit#editTextArea:focus, QLineEdit#renameLine:focus { border-color: #4d94ff; }
QToolButton#themeChip {
  min-width: 70px; padding: 5px 10px; border: 1px solid #29405b; border-radius: 8px;
  background: #132034; color: #dce9ff; font-size: 12px; font-weight: 600;
}
QToolButton#themeChip:checked { background: #2f67c7; border-color: #4d94ff; color: #ffffff; }
QPushButton {
  border-radius: 7px; padding: 4px 7px; color: #dce9ff;
  border: 1px solid #2a4058; background: #16273c; font-size: 11px;
}
QPushButton:hover { border-color: #4d94ff; }
QPushButton#cardPrimaryBtn { color: #ffffff; background: #2869df; border-color: #3e7ef3; font-weight: 600; }
QPushButton#cardDangerBtn, QPushButton#dangerBtn { color: #ffb4b4; border-color: #6f2f2f; background: #381d1f; }
QLabel#copyToast {
  color: #eef6ff; background: #214c9e; border: 1px solid #3f79dd; border-radius: 8px;
  font-size: 11px; font-weight: 700; padding: 4px 8px;
}
QLabel#status { color: #a9bedd; font-size: 11px; }
)");

  const QString lightStyle = QStringLiteral(R"(
QWidget#root { background: #f2f6fb; }
QLabel#title { color: #1c2a3d; font-size: 21px; font-weight: 700; }
QLabel#subtitle { color: #5b6f89; font-size: 11px; }
QLineEdit#search {
  border: 1px solid #b8c9db; border-radius: 9px; padding: 7px 10px;
  color: #1d2b3c; background: #ffffff; selection-background-color: #75a7ff;
}
QLineEdit#search:focus { border-color: #4a8cff; }
QListWidget#history {
  border: 1px solid #b9cadb; border-radius: 10px; background: #ffffff;
  padding: 4px; outline: none;
}
QListWidget#history::item { border: 0; margin: 3px 1px; padding: 0; }
QFrame#clipCard { border: 1px solid #c6d6e8; border-radius: 9px; background: #f9fbff; }
QFrame#clipCard[selectedCard="true"] { border-color: #4a8cff; background: #eaf2ff; }
QLabel#cardMeta { color: #5b6f89; font-size: 10px; }
QLabel#cardTitle { color: #1f2f44; font-size: 12px; font-weight: 600; }
QLabel#cardBody { color: #162a42; font-size: 13px; font-weight: 600; }
QLabel#cardLink { color: #2f6fd8; font-size: 10px; }
QLabel#topStat { color: #5b6f89; font-size: 10px; }
QLabel#cpuIcon { min-width: 14px; max-width: 14px; }
QDialog { background: #f7faff; color: #1d2f42; }
QDialog QLabel { color: #1d2f42; }
QDialog QCheckBox { color: #243a54; }
QDialog QDoubleSpinBox, QDialog QComboBox, QDialog QKeySequenceEdit {
  color: #1d2f42; background: #ffffff; border: 1px solid #b6c8dd; border-radius: 8px;
}
QDialog QDoubleSpinBox::up-button, QDialog QDoubleSpinBox::down-button {
  width: 16px; border-left: 1px solid #b6c8dd; background: #eef4fc;
}
QDialog QDoubleSpinBox::up-button:hover, QDialog QDoubleSpinBox::down-button:hover {
  background: #dde9f8;
}
QDialog QDoubleSpinBox::up-arrow, QDialog QDoubleSpinBox::down-arrow {
  width: 8px; height: 8px;
}
QKeySequenceEdit#hotkeyEdit {
  min-width: 180px; padding: 4px 8px; font-weight: 600;
}
QPlainTextEdit#editTextArea, QLineEdit#renameLine {
  border: 1px solid #b6c8dd; border-radius: 9px; background: #ffffff; color: #1d2f42; padding: 8px;
}
QPlainTextEdit#editTextArea:focus, QLineEdit#renameLine:focus { border-color: #4a8cff; }
QToolButton#themeChip {
  min-width: 70px; padding: 5px 10px; border: 1px solid #b6c8dd; border-radius: 8px;
  background: #ffffff; color: #1d2f42; font-size: 12px; font-weight: 600;
}
QToolButton#themeChip:checked { background: #2f75ea; border-color: #3f86ff; color: #ffffff; }
QPushButton {
  border-radius: 7px; padding: 4px 7px; color: #1d2f42;
  border: 1px solid #b6c8dd; background: #ffffff; font-size: 11px;
}
QPushButton:hover { border-color: #4a8cff; }
QPushButton#cardPrimaryBtn { color: #ffffff; background: #2f75ea; border-color: #3f86ff; font-weight: 600; }
QPushButton#cardDangerBtn, QPushButton#dangerBtn { color: #a13434; border-color: #d3a9a9; background: #fff4f4; }
QLabel#copyToast {
  color: #11408b; background: #d8e8ff; border: 1px solid #95b9ef; border-radius: 8px;
  font-size: 11px; font-weight: 700; padding: 4px 8px;
}
QLabel#status { color: #5d6f89; font-size: 11px; }
)");

  setStyleSheet(dark ? darkStyle : lightStyle);
}

void MainWindow::loadSettings() {
  appSettings_.autoCloseOnCopy = settingsStore_.value(QStringLiteral("behavior/autoCloseOnCopy"), false).toBool();

  const int delayMs = settingsStore_.value(QStringLiteral("behavior/autoCloseDelayMs"), 0).toInt();
  appSettings_.autoCloseDelayMs = std::clamp(delayMs, 0, 30000);

  appSettings_.themeMode = themeModeFromString(
      settingsStore_.value(QStringLiteral("appearance/themeMode"), QStringLiteral("dark")).toString());

  appSettings_.startAtLogin = settingsStore_.value(QStringLiteral("system/startAtLogin"), false).toBool();
  appSettings_.gpuPreviewEnabled = settingsStore_.value(QStringLiteral("performance/gpuPreviewEnabled"), false).toBool();
  appSettings_.autoCheckOnStartup = settingsStore_.value(QStringLiteral("updates/autoCheckOnStartup"), true).toBool();
  appSettings_.openHotkey = normalizeHotkeySetting(
      settingsStore_.value(QStringLiteral("shortcuts/openHotkey"), defaultOpenHotkeyPortable()).toString());

#if defined(Q_OS_MAC)
  const QFileInfo launchAgent(launchAgentPath());
  if (launchAgent.exists()) {
    appSettings_.startAtLogin = true;
  }
#endif
}

void MainWindow::saveSettings() {
  settingsStore_.setValue(QStringLiteral("behavior/autoCloseOnCopy"), appSettings_.autoCloseOnCopy);
  settingsStore_.setValue(QStringLiteral("behavior/autoCloseDelayMs"), appSettings_.autoCloseDelayMs);
  settingsStore_.setValue(QStringLiteral("appearance/themeMode"), themeModeToString(appSettings_.themeMode));
  settingsStore_.setValue(QStringLiteral("system/startAtLogin"), appSettings_.startAtLogin);
  settingsStore_.setValue(QStringLiteral("performance/gpuPreviewEnabled"), appSettings_.gpuPreviewEnabled);
  settingsStore_.setValue(QStringLiteral("updates/autoCheckOnStartup"), appSettings_.autoCheckOnStartup);
  settingsStore_.setValue(QStringLiteral("shortcuts/openHotkey"), normalizeHotkeySetting(appSettings_.openHotkey));
  settingsStore_.sync();
}

void MainWindow::detectGpuSupport() {
  gpuInfo_ = GpuSupportInfo{};
  gpuInfo_.checked = true;

  QOpenGLContext context;
  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::NoProfile);
  context.setFormat(format);

  if (!context.create()) {
    gpuInfo_.detail = QStringLiteral("OpenGL context creation failed.");
    return;
  }

  QOffscreenSurface surface;
  surface.setFormat(context.format());
  surface.create();
  if (!surface.isValid()) {
    gpuInfo_.detail = QStringLiteral("Offscreen surface creation failed.");
    return;
  }

  if (!context.makeCurrent(&surface)) {
    gpuInfo_.detail = QStringLiteral("Cannot make OpenGL context current.");
    return;
  }

  QOpenGLFunctions* funcs = context.functions();
  if (!funcs) {
    gpuInfo_.detail = QStringLiteral("OpenGL functions unavailable.");
    context.doneCurrent();
    return;
  }

  const auto vendorRaw = funcs->glGetString(GL_VENDOR);
  const auto rendererRaw = funcs->glGetString(GL_RENDERER);
  const auto versionRaw = funcs->glGetString(GL_VERSION);

  gpuInfo_.vendor = vendorRaw ? QString::fromLatin1(reinterpret_cast<const char*>(vendorRaw))
                              : QStringLiteral("Unknown vendor");
  gpuInfo_.renderer = rendererRaw ? QString::fromLatin1(reinterpret_cast<const char*>(rendererRaw))
                                  : QStringLiteral("Unknown renderer");
  gpuInfo_.version = versionRaw ? QString::fromLatin1(reinterpret_cast<const char*>(versionRaw))
                                : QStringLiteral("Unknown version");
  gpuInfo_.supported = true;
  gpuInfo_.detail = QStringLiteral("%1 • %2").arg(gpuInfo_.vendor, gpuInfo_.renderer);

  context.doneCurrent();
}

QString MainWindow::gpuStatusText() const {
  if (!gpuInfo_.checked) {
    return QStringLiteral("GPU support not checked.");
  }

  if (gpuInfo_.supported) {
    return QStringLiteral("GPU supported: %1 (%2)").arg(gpuInfo_.renderer, gpuInfo_.version);
  }

  return QStringLiteral("GPU not supported on this device: %1").arg(gpuInfo_.detail);
}

bool MainWindow::isDarkTheme() const {
  if (appSettings_.themeMode == ThemeMode::Dark) {
    return true;
  }
  if (appSettings_.themeMode == ThemeMode::Light) {
    return false;
  }

  return palette().color(QPalette::Window).lightness() < 128;
}

QString MainWindow::versionString() const {
  const QString appVersion = QCoreApplication::applicationVersion().trimmed();
  return appVersion.isEmpty() ? QStringLiteral("0.1.0") : appVersion;
}

MainWindow::ThemeMode MainWindow::themeModeFromString(const QString& value) {
  const QString v = value.trimmed().toLower();
  if (v == QStringLiteral("light")) {
    return ThemeMode::Light;
  }
  if (v == QStringLiteral("system")) {
    return ThemeMode::System;
  }
  return ThemeMode::Dark;
}

QString MainWindow::themeModeToString(const ThemeMode mode) {
  switch (mode) {
    case ThemeMode::Light:
      return QStringLiteral("light");
    case ThemeMode::System:
      return QStringLiteral("system");
    case ThemeMode::Dark:
    default:
      return QStringLiteral("dark");
  }
}

QString MainWindow::launchAgentPath() const {
  return QDir::home().filePath(QStringLiteral("Library/LaunchAgents/com.muyleang.quickqc.plist"));
}

bool MainWindow::setLaunchAtLogin(const bool enabled, QString* errorMessage) const {
#if defined(Q_OS_MAC)
  const QString plistPath = launchAgentPath();
  const QFileInfo plistInfo(plistPath);

  QDir dir(plistInfo.absolutePath());
  if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
    if (errorMessage) {
      *errorMessage = QStringLiteral("Failed to create LaunchAgents directory.");
    }
    return false;
  }

  if (enabled) {
    QFile f(plistPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
      if (errorMessage) {
        *errorMessage = QStringLiteral("Failed to write startup plist.");
      }
      return false;
    }

    const QString exePath = QCoreApplication::applicationFilePath().toHtmlEscaped();
    QTextStream out(&f);
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
    out << "<plist version=\"1.0\">\n";
    out << "<dict>\n";
    out << "  <key>Label</key><string>com.muyleang.quickqc</string>\n";
    out << "  <key>ProgramArguments</key>\n";
    out << "  <array><string>" << exePath << "</string></array>\n";
    out << "  <key>RunAtLoad</key><true/>\n";
    out << "</dict>\n";
    out << "</plist>\n";
    f.close();

    if (errorMessage) {
      errorMessage->clear();
    }
    return true;
  }

  QProcess::execute(QStringLiteral("launchctl"), {QStringLiteral("unload"), QStringLiteral("-w"), plistPath});
  QFile::remove(plistPath);
  return true;
#else
  if (errorMessage) {
    *errorMessage = QStringLiteral("Start at login is currently implemented for macOS builds only.");
  }
  Q_UNUSED(enabled);
  return false;
#endif
}

void MainWindow::scheduleRefresh() {
  if (!refreshCoalesceTimer_->isActive()) {
    refreshCoalesceTimer_->start();
  }
}

void MainWindow::refresh() {
  if (!storage_) {
    return;
  }

  QString selectedId;
  if (QListWidgetItem* current = listWidget_->currentItem()) {
    selectedId = current->data(Qt::UserRole).toString();
  }

  const QList<ClipItem> items = storage_->history(searchEdit_ ? searchEdit_->text() : QString());
  populateList(items);

  if (!selectedId.isEmpty()) {
    setCurrentById(selectedId);
  }

  if (listWidget_->currentRow() < 0 && listWidget_->count() > 0) {
    listWidget_->setCurrentRow(0);
  }

  refreshStats();
  statusLabel_->setText(QStringLiteral("Showing %1 item(s)").arg(listWidget_->count()));
}

void MainWindow::populateList(const QList<ClipItem>& items) {
  visibleItems_ = items;
  listWidget_->clear();
  if (gImageThumbCache.size() > 300) {
    gImageThumbCache.clear();
  }

  for (const ClipItem& item : visibleItems_) {
    const bool hasSourcePath = item.kind == ClipKind::Image && !item.sourcePath.trimmed().isEmpty();

    auto* row = new QListWidgetItem();
    row->setData(Qt::UserRole, item.id);
    row->setSizeHint(QSize(100, item.kind == ClipKind::Image ? (hasSourcePath ? 180 : 166) : 136));

    listWidget_->addItem(row);
    listWidget_->setItemWidget(row, buildCardWidget(item));
  }

  updateCardSelectionStyles();
}

std::optional<ClipItem> MainWindow::selectedClip() const {
  const int idx = listWidget_->currentRow();
  if (idx < 0 || idx >= visibleItems_.size()) {
    return std::nullopt;
  }

  return visibleItems_.at(idx);
}

std::optional<ClipItem> MainWindow::clipById(const QString& id) const {
  if (!storage_ || id.trimmed().isEmpty()) {
    return std::nullopt;
  }

  if (const auto item = storage_->getClipById(id); item.has_value()) {
    return item;
  }

  for (const ClipItem& item : visibleItems_) {
    if (item.id == id) {
      return item;
    }
  }

  return std::nullopt;
}

void MainWindow::setCurrentById(const QString& id) {
  for (int i = 0; i < listWidget_->count(); ++i) {
    QListWidgetItem* row = listWidget_->item(i);
    if (row && row->data(Qt::UserRole).toString() == id) {
      listWidget_->setCurrentRow(i);
      return;
    }
  }
}

void MainWindow::copyToClipboard(const ClipItem& item) {
  if (!clipboard_) {
    return;
  }

  if (watcher_) {
    watcher_->suppressCaptureForMs(900);
  }

  if (item.kind == ClipKind::Text) {
    clipboard_->setText(item.text, QClipboard::Clipboard);
    statusLabel_->setText(QStringLiteral("Copied text at %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
    showToast(QStringLiteral("Copied text"));
  } else {
    QImage image;
    image.loadFromData(item.imageData, "PNG");
    if (image.isNull()) {
      statusLabel_->setText(QStringLiteral("Selected image is invalid."));
      return;
    }

    auto* mime = new QMimeData();
    mime->setImageData(image);
    if (!item.imageData.isEmpty()) {
      mime->setData(QStringLiteral("image/png"), item.imageData);
    }

    const QString sourcePath = item.sourcePath.trimmed();
    if (!sourcePath.isEmpty() && QFileInfo::exists(sourcePath)) {
      mime->setUrls({QUrl::fromLocalFile(sourcePath)});
    }

    clipboard_->setMimeData(mime, QClipboard::Clipboard);
    statusLabel_->setText(QStringLiteral("Copied image at %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
    showToast(QStringLiteral("Copied image"));
  }

  if (appSettings_.autoCloseOnCopy) {
    const int delayMs = std::max(0, appSettings_.autoCloseDelayMs);
    QTimer::singleShot(delayMs, this, [this]() {
      if (isVisible()) {
        hide();
      }
    });
  }
}

void MainWindow::copyClipById(const QString& id) {
  setCurrentById(id);
  const auto item = clipById(id);
  if (!item.has_value()) {
    statusLabel_->setText(QStringLiteral("Item no longer exists."));
    return;
  }

  copyToClipboard(*item);
}

void MainWindow::editClipById(const QString& id) {
  setCurrentById(id);
  const auto item = clipById(id);
  if (!item.has_value()) {
    statusLabel_->setText(QStringLiteral("Item no longer exists."));
    return;
  }

  if (item->kind == ClipKind::Text) {
    QDialog editDialog(this);
    editDialog.setWindowTitle(QStringLiteral("Edit Text"));
    editDialog.resize(640, 430);

    auto* root = new QVBoxLayout(&editDialog);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("Edit Clipboard Text"), &editDialog);
    title->setObjectName(QStringLiteral("cardTitle"));

    auto* hint = new QLabel(QStringLiteral("Tip: drag title bar to move this editor."), &editDialog);
    hint->setObjectName(QStringLiteral("cardMeta"));

    auto* editor = new QPlainTextEdit(item->text, &editDialog);
    editor->setObjectName(QStringLiteral("editTextArea"));
    editor->setPlaceholderText(QStringLiteral("Type text here..."));
    editor->setMinimumHeight(280);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &editDialog);
    connect(buttons, &QDialogButtonBox::accepted, &editDialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &editDialog, &QDialog::reject);

    root->addWidget(title);
    root->addWidget(hint);
    root->addWidget(editor, 1);
    root->addWidget(buttons);

    if (editDialog.exec() != QDialog::Accepted) {
      return;
    }

    const QString nextText = editor->toPlainText();
    if (!storage_->updateClipText(item->id, nextText)) {
      statusLabel_->setText(QStringLiteral("Failed to update text."));
      return;
    }

    scheduleRefresh();
    statusLabel_->setText(QStringLiteral("Text updated."));
    return;
  }

  QDialog renameDialog(this);
  renameDialog.setWindowTitle(QStringLiteral("Rename Image"));
  renameDialog.resize(440, 170);

  auto* root = new QVBoxLayout(&renameDialog);
  root->setContentsMargins(12, 12, 12, 12);
  root->setSpacing(8);

  auto* title = new QLabel(QStringLiteral("Image Name"), &renameDialog);
  title->setObjectName(QStringLiteral("cardTitle"));

  auto* renameEdit = new QLineEdit(item->imageName, &renameDialog);
  renameEdit->setObjectName(QStringLiteral("renameLine"));
  renameEdit->setPlaceholderText(QStringLiteral("clipboard-image.png"));
  renameEdit->selectAll();

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &renameDialog);
  connect(buttons, &QDialogButtonBox::accepted, &renameDialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &renameDialog, &QDialog::reject);

  root->addWidget(title);
  root->addWidget(renameEdit);
  root->addWidget(buttons);

  if (renameDialog.exec() != QDialog::Accepted) {
    return;
  }

  const QString renamed = renameEdit->text();
  if (!storage_->renameClipImage(item->id, renamed)) {
    statusLabel_->setText(QStringLiteral("Failed to rename image."));
    return;
  }

  scheduleRefresh();
  statusLabel_->setText(QStringLiteral("Image renamed."));
}

void MainWindow::togglePinById(const QString& id) {
  setCurrentById(id);

  if (!storage_->togglePin(id)) {
    statusLabel_->setText(QStringLiteral("Failed to update pin."));
    return;
  }

  scheduleRefresh();
  statusLabel_->setText(QStringLiteral("Pin updated."));
}

void MainWindow::deleteClipById(const QString& id) {
  setCurrentById(id);

  if (!storage_->deleteClip(id)) {
    statusLabel_->setText(QStringLiteral("Failed to delete item."));
    return;
  }

  scheduleRefresh();
  statusLabel_->setText(QStringLiteral("Item deleted."));
}

void MainWindow::previewImageById(const QString& id) {
  setCurrentById(id);
  const auto item = clipById(id);
  if (!item.has_value() || item->kind != ClipKind::Image) {
    statusLabel_->setText(QStringLiteral("Image not found."));
    return;
  }

  QImage image;
  image.loadFromData(item->imageData, "PNG");
  if (image.isNull()) {
    statusLabel_->setText(QStringLiteral("Image is invalid."));
    return;
  }

  QDialog dialog(this);
  dialog.setWindowTitle(item->imageName.trimmed().isEmpty() ? QStringLiteral("Image Preview") : item->imageName);
  dialog.resize(760, 560);

  auto* root = new QVBoxLayout(&dialog);
  root->setContentsMargins(10, 10, 10, 10);
  root->setSpacing(8);

  if (appSettings_.gpuPreviewEnabled && gpuInfo_.supported) {
    auto* gpuView = new GpuImagePreviewWidget(image, &dialog);
    root->addWidget(gpuView, 1);
  } else {
    auto* scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);

    auto* imageLabel = new QLabel(scroll);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setPixmap(QPixmap::fromImage(image));
    imageLabel->setMinimumSize(300, 220);
    scroll->setWidget(imageLabel);

    root->addWidget(scroll, 1);
  }

  if (!item->sourcePath.trimmed().isEmpty()) {
    auto* sourceRow = new QHBoxLayout();
    auto* sourceLabel = new QLabel(QStringLiteral("Source: %1").arg(item->sourcePath), &dialog);
    sourceLabel->setWordWrap(true);

    auto* openBtn = new QPushButton(QStringLiteral("Open Source"), &dialog);
    connect(openBtn, &QPushButton::clicked, &dialog, [source = item->sourcePath]() {
      QDesktopServices::openUrl(QUrl::fromLocalFile(source));
    });

    sourceRow->addWidget(sourceLabel, 1);
    sourceRow->addWidget(openBtn);
    root->addLayout(sourceRow);
  }

  auto* actions = new QHBoxLayout();
  actions->addStretch();

  auto* copyBtn = new QPushButton(QStringLiteral("Copy"), &dialog);
  copyBtn->setObjectName(QStringLiteral("cardPrimaryBtn"));

  auto* closeBtn = new QPushButton(QStringLiteral("Close"), &dialog);

  connect(copyBtn, &QPushButton::clicked, &dialog, [this, item]() {
    if (item.has_value()) {
      copyToClipboard(*item);
    }
  });
  connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

  actions->addWidget(copyBtn);
  actions->addWidget(closeBtn);
  root->addLayout(actions);

  dialog.exec();
}

QWidget* MainWindow::buildCardWidget(const ClipItem& item) {
  auto* card = new QFrame();
  card->setObjectName(QStringLiteral("clipCard"));
  card->setProperty("selectedCard", false);

  auto* root = new QVBoxLayout(card);
  root->setContentsMargins(8, 7, 8, 7);
  root->setSpacing(5);

  auto* headerRow = new QHBoxLayout();
  headerRow->setSpacing(5);

  auto* kindLabel = new QLabel(
      QStringLiteral("%1%2").arg(item.pinned ? QStringLiteral("PIN • ") : QString(), kindText(item.kind)),
      card);
  kindLabel->setObjectName(QStringLiteral("cardMeta"));

  auto* timeLabel = new QLabel(formatTimestamp(item.createdAt), card);
  timeLabel->setObjectName(QStringLiteral("cardMeta"));

  headerRow->addWidget(kindLabel);
  headerRow->addStretch();
  headerRow->addWidget(timeLabel);
  root->addLayout(headerRow);

  if (item.kind == ClipKind::Image) {
    auto* contentRow = new QHBoxLayout();
    contentRow->setSpacing(8);

    auto* imageLabel = new QLabel(card);
    imageLabel->setMinimumSize(90, 60);
    imageLabel->setMaximumSize(90, 60);
    imageLabel->setAlignment(Qt::AlignCenter);

    QPixmap thumb = gImageThumbCache.value(item.id);
    if (thumb.isNull() && storage_) {
      const auto fullItem = storage_->getClipById(item.id);
      if (fullItem.has_value() && fullItem->kind == ClipKind::Image) {
        QImage image;
        image.loadFromData(fullItem->imageData, "PNG");
        if (!image.isNull()) {
          thumb = QPixmap::fromImage(
              image.scaled(imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
          if (!thumb.isNull()) {
            gImageThumbCache.insert(item.id, thumb);
          }
        }
      }
    }

    if (!thumb.isNull()) {
      imageLabel->setPixmap(thumb);
    } else {
      imageLabel->setPixmap(style()->standardIcon(QStyle::SP_FileIcon).pixmap(22, 22));
      imageLabel->setToolTip(QStringLiteral("Image preview available via Preview button"));
    }

    auto* imageInfoWrap = new QVBoxLayout();
    imageInfoWrap->setSpacing(2);

    auto* nameLabel = new QLabel(
        item.imageName.trimmed().isEmpty() ? QStringLiteral("clipboard-image.png") : item.imageName,
        card);
    nameLabel->setObjectName(QStringLiteral("cardTitle"));

    auto* tagsLabel = new QLabel(
        item.tags.isEmpty() ? QStringLiteral("no tags")
                            : QStringLiteral("#") + item.tags.join(QStringLiteral(" #")),
        card);
    tagsLabel->setObjectName(QStringLiteral("cardMeta"));
    tagsLabel->setWordWrap(true);

    imageInfoWrap->addWidget(nameLabel);
    imageInfoWrap->addWidget(tagsLabel);

    if (!item.sourcePath.trimmed().isEmpty()) {
      const QUrl sourceUrl = QUrl::fromLocalFile(item.sourcePath);
      const QString sourceFileName = QFileInfo(item.sourcePath).fileName();

      auto* sourceLink = new QLabel(
          QStringLiteral("<a href=\"%1\">source: %2</a>")
              .arg(sourceUrl.toString(QUrl::FullyEncoded), sourceFileName.toHtmlEscaped()),
          card);
      sourceLink->setObjectName(QStringLiteral("cardLink"));
      sourceLink->setTextFormat(Qt::RichText);
      sourceLink->setTextInteractionFlags(Qt::TextBrowserInteraction);
      sourceLink->setOpenExternalLinks(true);
      sourceLink->setToolTip(item.sourcePath);
      sourceLink->setWordWrap(true);
      imageInfoWrap->addWidget(sourceLink);
    }

    imageInfoWrap->addStretch();

    contentRow->addWidget(imageLabel);
    contentRow->addLayout(imageInfoWrap, 1);
    root->addLayout(contentRow);
  } else {
    auto* textLabel = new QLabel(clipPreview(item), card);
    textLabel->setObjectName(QStringLiteral("cardBody"));
    textLabel->setMinimumHeight(48);
    textLabel->setMaximumHeight(56);
    textLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    textLabel->setToolTip(item.text);
    textLabel->setWordWrap(true);
    root->addWidget(textLabel);

    auto* tagsLabel = new QLabel(
        item.tags.isEmpty() ? QStringLiteral("no tags")
                            : QStringLiteral("#") + item.tags.join(QStringLiteral(" #")),
        card);
    tagsLabel->setObjectName(QStringLiteral("cardMeta"));
    tagsLabel->setWordWrap(true);
    root->addWidget(tagsLabel);
  }

  auto* actions = new QHBoxLayout();
  actions->setSpacing(4);

  auto* copyBtn = new QPushButton(QStringLiteral("Copy"), card);
  copyBtn->setObjectName(QStringLiteral("cardPrimaryBtn"));

  auto* editBtn = new QPushButton(QStringLiteral("Edit"), card);
  auto* pinBtn = new QPushButton(item.pinned ? QStringLiteral("Unpin") : QStringLiteral("Pin"), card);
  auto* deleteBtn = new QPushButton(QStringLiteral("Delete"), card);
  deleteBtn->setObjectName(QStringLiteral("cardDangerBtn"));

  actions->addWidget(copyBtn);
  actions->addWidget(editBtn);
  actions->addWidget(pinBtn);

  if (item.kind == ClipKind::Image) {
    auto* previewBtn = new QPushButton(QStringLiteral("Preview"), card);
    actions->addWidget(previewBtn);
    connect(previewBtn, &QPushButton::clicked, this, [this, id = item.id]() { previewImageById(id); });
  }

  actions->addWidget(deleteBtn);
  actions->addStretch();
  root->addLayout(actions);

  connect(copyBtn, &QPushButton::clicked, this, [this, copyBtn, id = item.id]() {
    copyClipById(id);
    copyBtn->setText(QStringLiteral("Copied"));
    copyBtn->setEnabled(false);
    QTimer::singleShot(850, copyBtn, [copyBtn]() {
      copyBtn->setText(QStringLiteral("Copy"));
      copyBtn->setEnabled(true);
    });
  });
  connect(editBtn, &QPushButton::clicked, this, [this, id = item.id]() { editClipById(id); });
  connect(pinBtn, &QPushButton::clicked, this, [this, id = item.id]() { togglePinById(id); });
  connect(deleteBtn, &QPushButton::clicked, this, [this, id = item.id]() { deleteClipById(id); });

  return card;
}

void MainWindow::updateCardSelectionStyles() {
  const int selected = listWidget_->currentRow();
  for (int i = 0; i < listWidget_->count(); ++i) {
    QListWidgetItem* row = listWidget_->item(i);
    QWidget* card = row ? listWidget_->itemWidget(row) : nullptr;
    if (!card) {
      continue;
    }

    const bool isSelected = i == selected;
    card->setProperty("selectedCard", isSelected);
    card->style()->unpolish(card);
    card->style()->polish(card);
    card->update();
  }
}

void MainWindow::refreshStats() {
  const ClipStorage::StorageStats s = storage_->stats();

  storeSummaryLabel_->setText(
      QStringLiteral("Store %1 (T%2/I%3) • %4")
          .arg(s.totalCount)
          .arg(s.textCount)
          .arg(s.imageCount)
          .arg(formatBytes(s.textBytes + s.imageBytes)));
}

void MainWindow::openSettingsDialog() {
  QDialog dialog(this, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
  dialog.setWindowTitle(QStringLiteral("Settings"));
  dialog.setWindowModality(Qt::ApplicationModal);
  dialog.resize(500, 500);
  centerDialogOnScreen(&dialog, this);

  auto* root = new QVBoxLayout(&dialog);
  root->setContentsMargins(12, 12, 12, 12);
  root->setSpacing(10);

  auto* title = new QLabel(QStringLiteral("%1 Settings").arg(QString::fromUtf8(kAppDisplayName)), &dialog);
  title->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: 700;"));

  auto* autoCloseCheck = new QCheckBox(QStringLiteral("Auto close popup after copy"), &dialog);
  autoCloseCheck->setChecked(appSettings_.autoCloseOnCopy);

  auto* delayRow = new QHBoxLayout();
  auto* delayLabel = new QLabel(QStringLiteral("Auto-close delay (seconds)"), &dialog);
  auto* delaySpin = new QDoubleSpinBox(&dialog);
  delaySpin->setRange(0.0, 30.0);
  delaySpin->setSingleStep(0.1);
  delaySpin->setDecimals(1);
  delaySpin->setValue(appSettings_.autoCloseDelayMs / 1000.0);
  delayRow->addWidget(delayLabel);
  delayRow->addStretch();
  delayRow->addWidget(delaySpin);

  auto* themeRow = new QHBoxLayout();
  auto* themeLabel = new QLabel(QStringLiteral("Theme"), &dialog);
  auto* themeGroupWrap = new QWidget(&dialog);
  auto* themeButtons = new QHBoxLayout(themeGroupWrap);
  themeButtons->setContentsMargins(0, 0, 0, 0);
  themeButtons->setSpacing(6);

  auto* systemBtn = new QToolButton(themeGroupWrap);
  systemBtn->setObjectName(QStringLiteral("themeChip"));
  systemBtn->setText(QStringLiteral("◐ System"));
  systemBtn->setCheckable(true);

  auto* moonBtn = new QToolButton(themeGroupWrap);
  moonBtn->setObjectName(QStringLiteral("themeChip"));
  moonBtn->setText(QStringLiteral("☾ Moon"));
  moonBtn->setCheckable(true);

  auto* sunBtn = new QToolButton(themeGroupWrap);
  sunBtn->setObjectName(QStringLiteral("themeChip"));
  sunBtn->setText(QStringLiteral("☀ Sun"));
  sunBtn->setCheckable(true);

  auto* themeBtnGroup = new QButtonGroup(themeGroupWrap);
  themeBtnGroup->setExclusive(true);
  themeBtnGroup->addButton(systemBtn, static_cast<int>(ThemeMode::System));
  themeBtnGroup->addButton(moonBtn, static_cast<int>(ThemeMode::Dark));
  themeBtnGroup->addButton(sunBtn, static_cast<int>(ThemeMode::Light));

  themeButtons->addWidget(systemBtn);
  themeButtons->addWidget(moonBtn);
  themeButtons->addWidget(sunBtn);

  switch (appSettings_.themeMode) {
    case ThemeMode::System:
      systemBtn->setChecked(true);
      break;
    case ThemeMode::Light:
      sunBtn->setChecked(true);
      break;
    case ThemeMode::Dark:
    default:
      moonBtn->setChecked(true);
      break;
  }

  themeRow->addWidget(themeLabel);
  themeRow->addStretch();
  themeRow->addWidget(themeGroupWrap);

  auto* startupCheck = new QCheckBox(QStringLiteral("Start app when I log in"), &dialog);
  startupCheck->setChecked(appSettings_.startAtLogin);

  auto* startupUpdateCheck = new QCheckBox(QStringLiteral("Check for updates on startup"), &dialog);
  startupUpdateCheck->setChecked(appSettings_.autoCheckOnStartup);

  auto* hotkeyRow = new QHBoxLayout();
  auto* hotkeyLabel = new QLabel(QStringLiteral("Open app hotkey"), &dialog);
  auto* hotkeyEdit = new QKeySequenceEdit(&dialog);
  hotkeyEdit->setObjectName(QStringLiteral("hotkeyEdit"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  hotkeyEdit->setMaximumSequenceLength(1);
#endif
  hotkeyEdit->setKeySequence(QKeySequence::fromString(
      appSettings_.openHotkey,
      QKeySequence::PortableText));
  auto* hotkeyResetBtn = new QPushButton(QStringLiteral("Reset"), &dialog);
  connect(hotkeyResetBtn, &QPushButton::clicked, &dialog, [hotkeyEdit]() {
    hotkeyEdit->setKeySequence(QKeySequence::fromString(
        defaultOpenHotkeyPortable(),
        QKeySequence::PortableText));
  });
  hotkeyRow->addWidget(hotkeyLabel);
  hotkeyRow->addStretch();
  hotkeyRow->addWidget(hotkeyEdit);
  hotkeyRow->addWidget(hotkeyResetBtn);

  auto* hotkeyHint = new QLabel(
      QStringLiteral("Press your keys to record a shortcut. Only one shortcut step is saved."),
      &dialog);
  hotkeyHint->setObjectName(QStringLiteral("cardMeta"));
  hotkeyHint->setWordWrap(true);

  auto* gpuCheck = new QCheckBox(QStringLiteral("Enable GPU image preview (experimental)"), &dialog);
  gpuCheck->setChecked(appSettings_.gpuPreviewEnabled && gpuInfo_.supported);
  gpuCheck->setEnabled(gpuInfo_.supported);

  auto* gpuInfoLabel = new QLabel(gpuStatusText(), &dialog);
  gpuInfoLabel->setObjectName(QStringLiteral("cardMeta"));
  gpuInfoLabel->setWordWrap(true);

  auto* versionFounder = new QLabel(
      QStringLiteral("Founder: %1 • Version %2")
          .arg(QString::fromUtf8(kFounderName), versionString()),
      &dialog);

  auto* updateBtn = new QPushButton(QStringLiteral("Check for Updates"), &dialog);
  connect(updateBtn, &QPushButton::clicked, this, &MainWindow::onCheckUpdates);

  auto* backupBtn = new QPushButton(QStringLiteral("Backup Data"), &dialog);
  auto* restoreBtn = new QPushButton(QStringLiteral("Restore Data"), &dialog);
  auto* backupRow = new QHBoxLayout();
  backupRow->addWidget(backupBtn);
  backupRow->addWidget(restoreBtn);
  backupRow->addStretch();
  connect(backupBtn, &QPushButton::clicked, this, &MainWindow::onBackupData);
  connect(restoreBtn, &QPushButton::clicked, this, &MainWindow::onRestoreData);

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  root->addWidget(title);
  root->addWidget(autoCloseCheck);
  root->addLayout(delayRow);
  root->addLayout(themeRow);
  root->addWidget(startupCheck);
  root->addWidget(startupUpdateCheck);
  root->addLayout(hotkeyRow);
  root->addWidget(hotkeyHint);
  root->addWidget(gpuCheck);
  root->addWidget(gpuInfoLabel);
  root->addWidget(versionFounder);
  root->addWidget(updateBtn);
  root->addLayout(backupRow);
  root->addStretch();
  root->addWidget(buttons);

  if (dialog.exec() != QDialog::Accepted) {
    return;
  }

  AppSettingsData next = appSettings_;
  const QString previousHotkey = appSettings_.openHotkey;
  next.autoCloseOnCopy = autoCloseCheck->isChecked();
  next.autoCloseDelayMs = static_cast<int>(delaySpin->value() * 1000.0);
  if (QAbstractButton* checkedThemeBtn = themeBtnGroup->checkedButton()) {
    next.themeMode = static_cast<ThemeMode>(themeBtnGroup->id(checkedThemeBtn));
  }
  next.autoCheckOnStartup = startupUpdateCheck->isChecked();
  const QString selectedHotkey = firstHotkeyStepPortable(hotkeyEdit->keySequence());
  next.openHotkey = normalizeHotkeySetting(selectedHotkey);
  next.gpuPreviewEnabled = gpuCheck->isChecked() && gpuInfo_.supported;

  const bool requestedStartup = startupCheck->isChecked();
  if (requestedStartup != appSettings_.startAtLogin) {
    QString startupError;
    if (!setLaunchAtLogin(requestedStartup, &startupError)) {
      QMessageBox::warning(this, QStringLiteral("Startup"), startupError);
      next.startAtLogin = appSettings_.startAtLogin;
    } else {
      next.startAtLogin = requestedStartup;
    }
  }

  appSettings_ = next;
  saveSettings();
  applyOpenShortcut();
  if (appSettings_.openHotkey != previousHotkey) {
    emit openHotkeyChanged(appSettings_.openHotkey);
  }
  applyStyles();
  rebuildTrayMenu();
  refreshStats();
  statusLabel_->setText(
      appSettings_.gpuPreviewEnabled
          ? QStringLiteral("Settings saved. GPU preview enabled.")
          : QStringLiteral("Settings saved."));
}

void MainWindow::onBackupData() {
  if (!storage_) {
    QMessageBox::warning(this, QStringLiteral("Backup"), QStringLiteral("Storage is not available."));
    return;
  }

  QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  if (defaultDir.trimmed().isEmpty()) {
    defaultDir = QDir::homePath();
  }
  const QString defaultPath = QDir(defaultDir).filePath(
      QStringLiteral("quickqc-backup-%1.json")
          .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));

  const QString outputPath = QFileDialog::getSaveFileName(
      this,
      QStringLiteral("Backup QuickQC Data"),
      defaultPath,
      QStringLiteral("JSON files (*.json)"));
  if (outputPath.trimmed().isEmpty()) {
    return;
  }

  const QList<ClipItem> clips = storage_->allClipsForBackup();
  QJsonArray clipArray;
  for (const ClipItem& clip : clips) {
    QJsonObject c;
    c.insert(QStringLiteral("id"), clip.id);
    c.insert(QStringLiteral("kind"), clipKindToString(clip.kind));
    c.insert(QStringLiteral("text"), clip.text);
    c.insert(QStringLiteral("imageName"), clip.imageName);
    c.insert(QStringLiteral("sourcePath"), clip.sourcePath);
    c.insert(QStringLiteral("createdAt"), QString::number(clip.createdAt));
    c.insert(QStringLiteral("updatedAt"), QString::number(clip.updatedAt));
    c.insert(QStringLiteral("pinned"), clip.pinned);
    if (clip.kind == ClipKind::Image) {
      c.insert(QStringLiteral("imageDataBase64"), QString::fromLatin1(clip.imageData.toBase64()));
    }

    QJsonArray tags;
    for (const QString& tag : clip.tags) {
      const QString trimmed = tag.trimmed();
      if (!trimmed.isEmpty()) {
        tags.push_back(trimmed);
      }
    }
    c.insert(QStringLiteral("tags"), tags);
    clipArray.push_back(c);
  }

  QJsonObject settingsObj;
  settingsObj.insert(QStringLiteral("autoCloseOnCopy"), appSettings_.autoCloseOnCopy);
  settingsObj.insert(QStringLiteral("autoCloseDelayMs"), appSettings_.autoCloseDelayMs);
  settingsObj.insert(QStringLiteral("themeMode"), themeModeToString(appSettings_.themeMode));
  settingsObj.insert(QStringLiteral("startAtLogin"), appSettings_.startAtLogin);
  settingsObj.insert(QStringLiteral("gpuPreviewEnabled"), appSettings_.gpuPreviewEnabled);
  settingsObj.insert(QStringLiteral("autoCheckOnStartup"), appSettings_.autoCheckOnStartup);
  settingsObj.insert(QStringLiteral("openHotkey"), appSettings_.openHotkey);

  QJsonObject root;
  root.insert(QStringLiteral("format"), QStringLiteral("quickqc-backup-v1"));
  root.insert(QStringLiteral("exportedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
  root.insert(QStringLiteral("appVersion"), versionString());
  root.insert(QStringLiteral("settings"), settingsObj);
  root.insert(QStringLiteral("history"), clipArray);

  QFile file(outputPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QMessageBox::warning(
        this,
        QStringLiteral("Backup"),
        QStringLiteral("Could not write backup file:\n%1").arg(outputPath));
    return;
  }

  const QByteArray payload = QJsonDocument(root).toJson(QJsonDocument::Indented);
  if (file.write(payload) != payload.size()) {
    file.close();
    QMessageBox::warning(
        this,
        QStringLiteral("Backup"),
        QStringLiteral("Failed to save complete backup data."));
    return;
  }
  file.close();

  statusLabel_->setText(QStringLiteral("Backup saved: %1").arg(QFileInfo(outputPath).fileName()));
  QMessageBox::information(
      this,
      QStringLiteral("Backup"),
      QStringLiteral("Backup complete.\nSaved %1 item(s).").arg(clips.size()));
}

void MainWindow::onRestoreData() {
  if (!storage_) {
    QMessageBox::warning(this, QStringLiteral("Restore"), QStringLiteral("Storage is not available."));
    return;
  }

  const QString inputPath = QFileDialog::getOpenFileName(
      this,
      QStringLiteral("Restore QuickQC Data"),
      QString(),
      QStringLiteral("JSON files (*.json)"));
  if (inputPath.trimmed().isEmpty()) {
    return;
  }

  QFile file(inputPath);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(
        this,
        QStringLiteral("Restore"),
        QStringLiteral("Could not open backup file:\n%1").arg(inputPath));
    return;
  }

  QJsonParseError parseError;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
  file.close();
  if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
    QMessageBox::warning(
        this,
        QStringLiteral("Restore"),
        QStringLiteral("Invalid backup JSON format."));
    return;
  }

  const QJsonObject root = doc.object();
  const QString format = root.value(QStringLiteral("format")).toString().trimmed();
  if (!format.startsWith(QStringLiteral("quickqc-backup-"))) {
    QMessageBox::warning(
        this,
        QStringLiteral("Restore"),
        QStringLiteral("This file is not a QuickQC backup."));
    return;
  }

  const QJsonArray historyArray = root.value(QStringLiteral("history")).toArray();
  QList<ClipItem> restoredItems;
  restoredItems.reserve(historyArray.size());

  for (const QJsonValue& entryValue : historyArray) {
    if (!entryValue.isObject()) {
      continue;
    }
    const QJsonObject entry = entryValue.toObject();
    ClipItem item;
    item.id = entry.value(QStringLiteral("id")).toString();
    item.kind = clipKindFromString(entry.value(QStringLiteral("kind")).toString());
    item.text = entry.value(QStringLiteral("text")).toString();
    item.imageName = entry.value(QStringLiteral("imageName")).toString();
    item.sourcePath = entry.value(QStringLiteral("sourcePath")).toString();
    item.createdAt = entry.value(QStringLiteral("createdAt")).toVariant().toLongLong();
    item.updatedAt = entry.value(QStringLiteral("updatedAt")).toVariant().toLongLong();
    item.pinned = entry.value(QStringLiteral("pinned")).toBool(false);
    if (item.kind == ClipKind::Image) {
      item.imageData = QByteArray::fromBase64(
          entry.value(QStringLiteral("imageDataBase64")).toString().toLatin1());
      if (item.imageData.isEmpty()) {
        continue;
      }
    }

    const QJsonArray tags = entry.value(QStringLiteral("tags")).toArray();
    for (const QJsonValue& tag : tags) {
      if (tag.isString()) {
        const QString trimmed = tag.toString().trimmed();
        if (!trimmed.isEmpty()) {
          item.tags.push_back(trimmed);
        }
      }
    }

    restoredItems.push_back(item);
  }

  const auto confirm = QMessageBox::question(
      this,
      QStringLiteral("Restore Backup"),
      QStringLiteral("Restore this backup and replace current clipboard history and settings?\n\nItems in backup: %1")
          .arg(restoredItems.size()),
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::No);
  if (confirm != QMessageBox::Yes) {
    return;
  }

  if (!storage_->replaceAllFromBackup(restoredItems)) {
    QMessageBox::warning(
        this,
        QStringLiteral("Restore"),
        QStringLiteral("Failed to restore clipboard history."));
    return;
  }

  AppSettingsData next = appSettings_;
  const QString previousHotkey = appSettings_.openHotkey;
  const QJsonObject settingsObj = root.value(QStringLiteral("settings")).toObject();
  if (!settingsObj.isEmpty()) {
    if (settingsObj.contains(QStringLiteral("autoCloseOnCopy"))) {
      next.autoCloseOnCopy = settingsObj.value(QStringLiteral("autoCloseOnCopy")).toBool(next.autoCloseOnCopy);
    }
    if (settingsObj.contains(QStringLiteral("autoCloseDelayMs"))) {
      next.autoCloseDelayMs = std::clamp(
          settingsObj.value(QStringLiteral("autoCloseDelayMs")).toInt(next.autoCloseDelayMs),
          0,
          30000);
    }
    if (settingsObj.contains(QStringLiteral("themeMode"))) {
      next.themeMode = themeModeFromString(settingsObj.value(QStringLiteral("themeMode")).toString());
    }
    if (settingsObj.contains(QStringLiteral("gpuPreviewEnabled"))) {
      next.gpuPreviewEnabled = settingsObj.value(QStringLiteral("gpuPreviewEnabled")).toBool(next.gpuPreviewEnabled) && gpuInfo_.supported;
    }
    if (settingsObj.contains(QStringLiteral("autoCheckOnStartup"))) {
      next.autoCheckOnStartup = settingsObj.value(QStringLiteral("autoCheckOnStartup")).toBool(next.autoCheckOnStartup);
    }
    if (settingsObj.contains(QStringLiteral("openHotkey"))) {
      next.openHotkey = normalizeHotkeySetting(settingsObj.value(QStringLiteral("openHotkey")).toString());
    }
    if (settingsObj.contains(QStringLiteral("startAtLogin"))) {
      const bool requestedStartup = settingsObj.value(QStringLiteral("startAtLogin")).toBool(next.startAtLogin);
      if (requestedStartup != appSettings_.startAtLogin) {
        QString startupError;
        if (!setLaunchAtLogin(requestedStartup, &startupError)) {
          QMessageBox::warning(this, QStringLiteral("Restore"), startupError);
          next.startAtLogin = appSettings_.startAtLogin;
        } else {
          next.startAtLogin = requestedStartup;
        }
      }
    }
  }

  appSettings_ = next;
  saveSettings();
  applyOpenShortcut();
  if (appSettings_.openHotkey != previousHotkey) {
    emit openHotkeyChanged(appSettings_.openHotkey);
  }
  applyStyles();
  rebuildTrayMenu();
  refresh();
  statusLabel_->setText(QStringLiteral("Restore completed (%1 item(s)).").arg(restoredItems.size()));
  QMessageBox::information(
      this,
      QStringLiteral("Restore"),
      QStringLiteral("Restore complete.\nLoaded %1 item(s).").arg(restoredItems.size()));
}

void MainWindow::onSearchChanged(const QString&) {
  searchDebounceTimer_->start();
}

void MainWindow::onCopySelected() {
  if (QLineEdit* focusedLine = qobject_cast<QLineEdit*>(focusWidget())) {
    if (!focusedLine->selectedText().isEmpty()) {
      if (watcher_) {
        watcher_->suppressCaptureForMs(700);
      }
      clipboard_->setText(focusedLine->selectedText(), QClipboard::Clipboard);
      statusLabel_->setText(QStringLiteral("Copied selected text."));
      showToast(QStringLiteral("Copied selected text"));
      return;
    }
  }

  const auto item = selectedClip();
  if (!item.has_value()) {
    statusLabel_->setText(QStringLiteral("No item selected."));
    return;
  }

  copyClipById(item->id);
}

void MainWindow::onEditSelected() {
  const auto item = selectedClip();
  if (!item.has_value()) {
    statusLabel_->setText(QStringLiteral("No item selected."));
    return;
  }

  editClipById(item->id);
}

void MainWindow::onClearAll() {
  const auto answer = QMessageBox::question(
      this,
      QStringLiteral("Clear All"),
      QStringLiteral("Delete all clipboard history? This cannot be undone."),
      QMessageBox::Yes | QMessageBox::No,
      QMessageBox::No);

  if (answer != QMessageBox::Yes) {
    return;
  }

  if (!storage_->clearAll()) {
    statusLabel_->setText(QStringLiteral("Failed to clear history."));
    return;
  }

  scheduleRefresh();
  statusLabel_->setText(QStringLiteral("All history cleared."));
}

void MainWindow::onImportImage() {
  const QStringList paths = QFileDialog::getOpenFileNames(
      this,
      QStringLiteral("Import Image"),
      QString(),
      QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tiff *.tif *.heic)"));

  if (paths.isEmpty()) {
    return;
  }

  int imported = 0;
  for (const QString& path : paths) {
    QImage image(path);
    if (image.isNull()) {
      continue;
    }

    QByteArray pngData;
    QBuffer buffer(&pngData);
    if (!buffer.open(QIODevice::WriteOnly)) {
      continue;
    }

    if (!image.save(&buffer, "PNG")) {
      continue;
    }

    if (storage_->insertImage(pngData, QFileInfo(path).fileName(), path)) {
      ++imported;
    }
  }

  if (imported <= 0) {
    statusLabel_->setText(QStringLiteral("No image imported."));
    return;
  }

  scheduleRefresh();
  statusLabel_->setText(QStringLiteral("Imported %1 image(s).").arg(imported));
}

void MainWindow::onOpenSettings() {
  if (!isVisible()) {
    showNearCursor();
  } else {
    if (isMinimized()) {
      showNormal();
    }
    raise();
    activateWindow();
  }
  openSettingsDialog();
}

void MainWindow::onCheckUpdates() {
  const bool startupSilentCheck = startupSilentUpdateCheck_;
  startupSilentUpdateCheck_ = false;

  if (!startupSilentCheck) {
    if (!isVisible()) {
      showNearCursor();
    } else {
      if (isMinimized()) {
        showNormal();
      }
      raise();
      activateWindow();
    }
  }

  enum class UpdateChoice {
    None,
    Close,
    Later,
    DownloadInstall,
  };

  QDialog dialog(this, Qt::Dialog | Qt::WindowTitleHint | Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
  dialog.setWindowTitle(QStringLiteral("Update Check"));
  dialog.setWindowModality(Qt::ApplicationModal);
  dialog.resize(430, 190);
  centerDialogOnScreen(&dialog, this);

  auto* root = new QVBoxLayout(&dialog);
  root->setContentsMargins(14, 14, 14, 14);
  root->setSpacing(10);

  const QString currentVersion = normalizeVersion(versionString());

  auto* titleLabel = new QLabel(QStringLiteral("Checking for updates..."), &dialog);
  titleLabel->setWordWrap(true);

  auto* detailLabel = new QLabel(
      QStringLiteral("Current version: %1").arg(currentVersion),
      &dialog);
  detailLabel->setWordWrap(true);

  auto* progressBar = new QProgressBar(&dialog);
  progressBar->setRange(0, 0);
  progressBar->setTextVisible(false);
  progressBar->setFixedHeight(10);

  auto* buttons = new QDialogButtonBox(&dialog);

  root->addWidget(titleLabel);
  root->addWidget(detailLabel);
  root->addWidget(progressBar);
  root->addWidget(buttons);

  UpdateChoice choice = UpdateChoice::None;
  auto clearDialogButtons = [buttons]() {
    const QList<QAbstractButton*> existing = buttons->buttons();
    for (QAbstractButton* button : existing) {
      buttons->removeButton(button);
      button->deleteLater();
    }
  };
  auto addDialogButton = [&dialog, buttons, &choice](const QString& text, const UpdateChoice mappedChoice, const QDialogButtonBox::ButtonRole role) {
    auto* button = buttons->addButton(text, role);
    QObject::connect(button, &QPushButton::clicked, &dialog, [&dialog, &choice, mappedChoice]() {
      choice = mappedChoice;
      dialog.done(QDialog::Accepted);
    });
  };
  auto waitForDialogClose = [&dialog]() {
    QEventLoop userLoop;
    QObject::connect(&dialog, &QDialog::finished, &userLoop, &QEventLoop::quit);
    userLoop.exec();
  };

  if (!startupSilentCheck) {
    clearDialogButtons();
    addDialogButton(QStringLiteral("Cancel"), UpdateChoice::Close, QDialogButtonBox::RejectRole);
    dialog.show();
    dialog.raise();
    dialog.activateWindow();
    qApp->processEvents();
  }

  QNetworkAccessManager networkManager;
  const QString userAgent = QStringLiteral("quickqc/%1").arg(versionString());
  QNetworkRequest request(QUrl(QString::fromUtf8(kReleaseApiUrl)));
  request.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
  request.setRawHeader("Accept", "application/vnd.github+json");

  QNetworkReply* reply = networkManager.get(request);

  QEventLoop loop;
  QTimer timeoutTimer(&dialog);
  timeoutTimer.setSingleShot(true);
  timeoutTimer.setInterval(12000);
  bool timedOut = false;

  connect(&dialog, &QDialog::finished, &dialog, [reply]() {
    if (reply && reply->isRunning()) {
      reply->abort();
    }
  });
  connect(&timeoutTimer, &QTimer::timeout, &dialog, [reply, &timedOut]() {
    timedOut = true;
    if (reply && reply->isRunning()) {
      reply->abort();
    }
  });
  connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

  timeoutTimer.start();
  loop.exec();
  progressBar->hide();

  if (!startupSilentCheck && !dialog.isVisible()) {
    reply->deleteLater();
    statusLabel_->setText(QStringLiteral("Update check canceled."));
    return;
  }

  const QVariant statusCodeVar = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
  const int statusCode = statusCodeVar.isValid() ? statusCodeVar.toInt() : 0;
  const QByteArray payload = reply->readAll();
  const QNetworkReply::NetworkError replyError = reply->error();
  reply->deleteLater();

  if (replyError != QNetworkReply::NoError) {
    QString message = QStringLiteral("Update check failed. Please try again.");
    if (statusCode == 403) {
      message = QStringLiteral("Update check rate-limited by GitHub. Please retry in a minute.");
    } else if (statusCode >= 500) {
      message = QStringLiteral("GitHub is currently unavailable. Please retry shortly.");
    } else if (timedOut || replyError == QNetworkReply::TimeoutError) {
      message = QStringLiteral("Update check timed out.");
    }

    if (!startupSilentCheck) {
      titleLabel->setText(QStringLiteral("Could not check updates"));
      detailLabel->setText(message);
      clearDialogButtons();
      addDialogButton(QStringLiteral("Close"), UpdateChoice::Close, QDialogButtonBox::RejectRole);
      waitForDialogClose();
    }
    statusLabel_->setText(message);
    return;
  }

  QJsonParseError parseError;
  const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
    const QString message = QStringLiteral("Received invalid update metadata.");
    if (!startupSilentCheck) {
      titleLabel->setText(QStringLiteral("Could not check updates"));
      detailLabel->setText(message);
      clearDialogButtons();
      addDialogButton(QStringLiteral("Close"), UpdateChoice::Close, QDialogButtonBox::RejectRole);
      waitForDialogClose();
    }
    statusLabel_->setText(message);
    return;
  }

  const QJsonObject obj = doc.object();
  const QString latestTag = obj.value(QStringLiteral("tag_name")).toString();
  const QString latestVersion = normalizeVersion(latestTag);
  const QString assetName = releaseAssetNameForCurrentPlatform();
  const QString assetUrl = releaseAssetUrlByName(obj, assetName);
  const QString checksumManifestUrl = releaseAssetUrlByName(obj, releaseChecksumsAssetName());

  if (latestVersion.isEmpty()) {
    const QString message = QStringLiteral("Update metadata is missing a release version.");
    if (!startupSilentCheck) {
      titleLabel->setText(QStringLiteral("Could not check updates"));
      detailLabel->setText(message);
      clearDialogButtons();
      addDialogButton(QStringLiteral("Close"), UpdateChoice::Close, QDialogButtonBox::RejectRole);
      waitForDialogClose();
    }
    statusLabel_->setText(message);
    return;
  }

  const int versionCmp = compareVersions(currentVersion, latestVersion);

  if (versionCmp < 0) {
    if (startupSilentCheck) {
      const QString message = QStringLiteral("Update available: %1.").arg(latestVersion);
      if (tray_) {
        tray_->showMessage(
            QStringLiteral("QuickQC Update"),
            QStringLiteral("Version %1 is available. Open Settings to update.")
                .arg(latestVersion),
            QSystemTrayIcon::Information,
            10000);
      }
      statusLabel_->setText(message);
      return;
    }

    titleLabel->setText(QStringLiteral("Update available"));
    detailLabel->setText(
        QStringLiteral("Latest version: %1\nCurrent version: %2\n\nDownload and install this update now?")
            .arg(latestVersion, currentVersion));
    clearDialogButtons();
    addDialogButton(QStringLiteral("Download && Install"), UpdateChoice::DownloadInstall, QDialogButtonBox::AcceptRole);
    addDialogButton(QStringLiteral("Later"), UpdateChoice::Later, QDialogButtonBox::RejectRole);
    waitForDialogClose();

    if (choice != UpdateChoice::DownloadInstall) {
      statusLabel_->setText(QStringLiteral("Update available: %1.").arg(latestVersion));
      return;
    }

    if (assetUrl.isEmpty()) {
      const QString message = QStringLiteral(
                                  "Automatic update package was not found for this platform.\n\nManual update command:\n%1")
                                  .arg(manualUpdateHintForCurrentPlatform());
      QMessageBox::warning(this, QStringLiteral("Update"), message);
      statusLabel_->setText(message);
      return;
    }

    if (checksumManifestUrl.isEmpty()) {
      const QString message = QStringLiteral(
                                  "Checksum manifest is missing for this release.\n"
                                  "QuickQC will not auto-install unverified packages.\n\n"
                                  "Manual update command:\n%1")
                                  .arg(manualUpdateHintForCurrentPlatform());
      QMessageBox::warning(this, QStringLiteral("Update"), message);
      statusLabel_->setText(QStringLiteral("Update blocked: checksum manifest missing."));
      return;
    }

    QString checksumsText;
    QString checksumsError;
    if (!downloadTextWithTimeout(
            QUrl(checksumManifestUrl),
            userAgent,
            12000,
            &checksumsText,
            &checksumsError)) {
      const QString message = QStringLiteral(
                                  "Could not download checksum manifest: %1\n\n"
                                  "Manual update command:\n%2")
                                  .arg(checksumsError, manualUpdateHintForCurrentPlatform());
      QMessageBox::warning(this, QStringLiteral("Update"), message);
      statusLabel_->setText(QStringLiteral("Update blocked: checksum manifest unavailable."));
      return;
    }

    const QString expectedSha256 = checksumForAssetFromManifest(checksumsText, assetName);
    if (expectedSha256.isEmpty()) {
      const QString message = QStringLiteral(
                                  "Checksum entry was not found for asset: %1\n"
                                  "QuickQC will not auto-install unverified packages.\n\n"
                                  "Manual update command:\n%2")
                                  .arg(assetName, manualUpdateHintForCurrentPlatform());
      QMessageBox::warning(this, QStringLiteral("Update"), message);
      statusLabel_->setText(QStringLiteral("Update blocked: checksum not found for asset."));
      return;
    }

    QString stagedPayloadPath;
    QString stageError;
    if (!stageUpdateForCurrentPlatform(
            this,
            assetUrl,
            userAgent,
            expectedSha256,
            &stagedPayloadPath,
            &stageError)) {
      QMessageBox::warning(this, QStringLiteral("Update"), stageError);
      statusLabel_->setText(stageError);
      return;
    }

    QMessageBox restartPrompt(this);
    restartPrompt.setWindowTitle(QStringLiteral("Update Ready"));
    restartPrompt.setText(
        QStringLiteral("%1 %2 has been downloaded, verified, and prepared.")
            .arg(QString::fromUtf8(kAppDisplayName), latestVersion));
    restartPrompt.setInformativeText(
        QStringLiteral("Restart now to apply immediately, or Later to install automatically when QuickQC exits."));
    auto* restartNowBtn = restartPrompt.addButton(QStringLiteral("Restart Now"), QMessageBox::AcceptRole);
    auto* laterBtn = restartPrompt.addButton(QStringLiteral("Later"), QMessageBox::RejectRole);
    Q_UNUSED(laterBtn);
    restartPrompt.exec();

    const bool restartNow = restartPrompt.clickedButton() == restartNowBtn;

    QString helperError;
    if (!launchInstallHelperForCurrentPlatform(stagedPayloadPath, restartNow, &helperError)) {
      const QString message = QStringLiteral(
                                  "%1\n\nManual update command:\n%2")
                                  .arg(helperError, manualUpdateHintForCurrentPlatform());
      QMessageBox::warning(this, QStringLiteral("Update"), message);
      statusLabel_->setText(helperError);
      return;
    }

    if (restartNow) {
      qApp->quit();
      return;
    }

    const QString message = QStringLiteral("Update downloaded. It will install after you quit QuickQC.");
    QMessageBox::information(this, QStringLiteral("Update Scheduled"), message);
    statusLabel_->setText(message);
    return;
  }

  if (versionCmp == 0) {
    if (startupSilentCheck) {
      statusLabel_->setText(QStringLiteral("Already on latest version (%1).").arg(currentVersion));
      return;
    }

    titleLabel->setText(QStringLiteral("You're up to date"));
    detailLabel->setText(
        QStringLiteral("Current version: %1\nLatest version: %2")
            .arg(currentVersion, latestVersion));
    clearDialogButtons();
    addDialogButton(QStringLiteral("Close"), UpdateChoice::Close, QDialogButtonBox::RejectRole);
    waitForDialogClose();
    statusLabel_->setText(QStringLiteral("Already on latest version (%1).").arg(currentVersion));
    return;
  }

  if (!startupSilentCheck) {
    titleLabel->setText(QStringLiteral("Running newer build"));
    detailLabel->setText(
        QStringLiteral("Current version: %1\nLatest public release: %2")
            .arg(currentVersion, latestVersion));
    clearDialogButtons();
    addDialogButton(QStringLiteral("Close"), UpdateChoice::Close, QDialogButtonBox::RejectRole);
    waitForDialogClose();
  }
  statusLabel_->setText(QStringLiteral("Running newer build (%1).").arg(currentVersion));
}

void MainWindow::onItemActivated(QListWidgetItem* item) {
  if (!item) {
    return;
  }

  copyClipById(item->data(Qt::UserRole).toString());
}

void MainWindow::onTrayActivated(const QSystemTrayIcon::ActivationReason reason) {
  if (!tray_ || !trayMenu_) {
    return;
  }

#if defined(Q_OS_MAC)
  // On macOS, setting `setContextMenu` already shows the tray menu.
  // Manually popping it here causes duplicate stacked menus.
  Q_UNUSED(reason);
  return;
#else
  if (reason == QSystemTrayIcon::Trigger) {
    if (trayMenu_->isVisible()) {
      trayMenu_->hide();
    } else {
      trayMenu_->popup(QCursor::pos());
      trayMenu_->activateWindow();
    }
  }
#endif
}

void MainWindow::onCurrentRowChanged(int) {
  updateCardSelectionStyles();
}

void MainWindow::refreshProcessUsage() {
  if (!isVisible()) {
    return;
  }

#if defined(Q_OS_UNIX)
  QProcess ps;
  ps.start(
      QStringLiteral("ps"),
      {QStringLiteral("-p"), QString::number(QCoreApplication::applicationPid()), QStringLiteral("-o"), QStringLiteral("%cpu=,rss=")});

  if (ps.waitForFinished(120)) {
    const QString output = QString::fromUtf8(ps.readAllStandardOutput()).simplified();
    const QStringList parts = output.split(' ', Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
      bool okCpu = false;
      bool okRss = false;

      QString cpuToken = parts.at(0);
      cpuToken.replace(',', '.');
      const double cpu = cpuToken.toDouble(&okCpu);

      const qint64 rssKb = parts.at(1).toLongLong(&okRss);
      if (okCpu && okRss) {
        cpuSummaryLabel_->setText(
            QStringLiteral("%1% / %2")
                .arg(QString::number(cpu, 'f', 1), formatBytes(rssKb * 1024)));
        return;
      }
    }
  }
#endif

  cpuSummaryLabel_->setText(QStringLiteral("unavailable"));
}

void MainWindow::showNearCursor() {
  refresh();

  const QPoint cursor = QCursor::pos();
  QScreen* screen = QGuiApplication::screenAt(cursor);
  if (!screen) {
    screen = QGuiApplication::primaryScreen();
  }

  const QRect area = screen ? screen->availableGeometry() : QRect(0, 0, 1280, 800);

  const int w = width();
  const int h = height();

  int x = cursor.x() - (w / 2);
  int y = cursor.y() - (h / 3);

  x = std::clamp(x, area.left(), std::max(area.left(), area.right() - w));
  y = std::clamp(y, area.top(), std::max(area.top(), area.bottom() - h));

  move(x, y);
  showNormal();
  raise();
  activateWindow();

  searchEdit_->setFocus();
  searchEdit_->selectAll();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (tray_ && tray_->isVisible()) {
    hide();
    event->ignore();
    return;
  }

  QMainWindow::closeEvent(event);
}

QString MainWindow::clipPreview(const ClipItem& item) {
  if (item.kind == ClipKind::Image) {
    const QString name = item.imageName.trimmed().isEmpty()
                             ? QStringLiteral("clipboard-image.png")
                             : item.imageName.trimmed();
    return QStringLiteral("[Image] %1").arg(name);
  }

  QString oneLine = item.text;
  oneLine.replace('\n', ' ');
  oneLine = oneLine.simplified();

  constexpr int kMaxLen = 118;
  if (oneLine.size() > kMaxLen) {
    oneLine = oneLine.left(kMaxLen - 1) + QStringLiteral("...");
  }

  return oneLine;
}

QString MainWindow::formatTimestamp(const qint64 unixMillis) {
  const QDateTime dt = QDateTime::fromMSecsSinceEpoch(unixMillis);
  return dt.toString(QStringLiteral("MM-dd HH:mm:ss"));
}

QString MainWindow::formatBytes(qint64 bytes) {
  if (bytes < 0) {
    bytes = 0;
  }

  static const char* kUnits[] = {"B", "KB", "MB", "GB", "TB"};

  double value = static_cast<double>(bytes);
  int unitIdx = 0;
  while (value >= 1024.0 && unitIdx < 4) {
    value /= 1024.0;
    ++unitIdx;
  }

  const int precision = unitIdx == 0 ? 0 : 1;
  return QStringLiteral("%1 %2").arg(QString::number(value, 'f', precision), QString::fromUtf8(kUnits[unitIdx]));
}
