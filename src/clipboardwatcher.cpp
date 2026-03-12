#include "clipboardwatcher.h"

#include "clipstorage.h"

#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QMimeData>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

#include <algorithm>

namespace {
QString imageFingerprint(const QImage& image) {
  if (image.isNull()) {
    return QString();
  }

  const qsizetype totalSize = image.sizeInBytes();
  const qsizetype sampleSize = std::min<qsizetype>(totalSize, 512);

  quint16 checksum = 0;
  if (sampleSize > 0 && image.constBits() != nullptr) {
    const QByteArrayView sample(
        reinterpret_cast<const char*>(image.constBits()),
        sampleSize);
    checksum = qChecksum(sample);
  }

  return QStringLiteral("%1x%2:%3:%4:%5")
      .arg(image.width())
      .arg(image.height())
      .arg(static_cast<int>(image.format()))
      .arg(totalSize)
      .arg(checksum, 4, 16, QChar('0'));
}
}

ClipboardWatcher::ClipboardWatcher(QClipboard* clipboard, ClipStorage* storage, QObject* parent)
    : QObject(parent),
      clipboard_(clipboard),
      storage_(storage),
      lastImageProbeMs_(0),
      suppressUntilMs_(0),
      pollTimer_(new QTimer(this)) {
  Q_ASSERT(clipboard_ != nullptr);
  Q_ASSERT(storage_ != nullptr);

  connect(clipboard_, &QClipboard::changed, this, &ClipboardWatcher::onClipboardChanged);

  // Keep event-driven capture as primary path; poll is a low-frequency fallback.
  pollTimer_->setInterval(900);
  connect(pollTimer_, &QTimer::timeout, this, &ClipboardWatcher::onPollTick);
  pollTimer_->start();
}

void ClipboardWatcher::suppressCaptureForMs(const int durationMs) {
  const int safeMs = std::max(120, durationMs);
  suppressUntilMs_ = QDateTime::currentMSecsSinceEpoch() + safeMs;
}

void ClipboardWatcher::primeFromCurrentClipboard() {
  if (!clipboard_) {
    return;
  }

  const QMimeData* mimeData = clipboard_->mimeData(QClipboard::Clipboard);
  if (!mimeData) {
    return;
  }

  rememberClipboardState(mimeData);
}

void ClipboardWatcher::onClipboardChanged(const QClipboard::Mode mode) {
  if (mode != QClipboard::Clipboard) {
    return;
  }

  scanClipboard();
}

void ClipboardWatcher::onPollTick() {
  scanClipboard();
}

void ClipboardWatcher::scanClipboard() {
  if (!storage_ || !clipboard_) {
    return;
  }

  const QMimeData* mimeData = clipboard_->mimeData(QClipboard::Clipboard);
  if (!mimeData) {
    return;
  }

  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  if (mimeData->hasImage() &&
      !mimeData->hasText() &&
      !mimeData->hasUrls() &&
      lastClipboardSignature_.startsWith(QStringLiteral("image:")) &&
      (now - lastImageProbeMs_) < 5000) {
    return;
  }

  const QString signature = clipboardSignature(mimeData);
  if (mimeData->hasImage()) {
    lastImageProbeMs_ = now;
  }

  if (!signature.isEmpty() && signature == lastClipboardSignature_) {
    return;
  }

  if (now <= suppressUntilMs_) {
    rememberClipboardState(mimeData);
    lastClipboardSignature_ = signature;
    return;
  }

  // Support "copy file in Finder/Explorer" for image files first.
  bool inserted = captureImageFromUrls(mimeData);
  if (!inserted) {
    inserted = captureText(mimeData);
  }
  if (!inserted) {
    inserted = captureImage(mimeData);
  }

  if (inserted) {
    emit historyChanged();
  }

  if (!signature.isEmpty()) {
    lastClipboardSignature_ = signature;
  }
}

QString ClipboardWatcher::clipboardSignature(const QMimeData* mimeData) const {
  if (!mimeData) {
    return {};
  }

  if (mimeData->hasUrls()) {
    QStringList paths;
    const QList<QUrl> urls = mimeData->urls();
    paths.reserve(urls.size());
    for (const QUrl& url : urls) {
      if (url.isLocalFile()) {
        paths.push_back(url.toLocalFile());
      } else {
        paths.push_back(url.toString());
      }
    }

    if (!paths.isEmpty()) {
      return QStringLiteral("urls:%1").arg(paths.join(QStringLiteral("|")));
    }
  }

  if (mimeData->hasText()) {
    const QString text = mimeData->text().trimmed();
    if (!text.isEmpty()) {
      return QStringLiteral("text:%1").arg(text);
    }
  }

  if (mimeData->hasImage()) {
    const QImage image = qvariant_cast<QImage>(mimeData->imageData());
    const QString fingerprint = imageFingerprint(image);
    if (!fingerprint.isEmpty()) {
      return QStringLiteral("image:%1").arg(fingerprint);
    }
  }

  return QStringLiteral("formats:%1").arg(mimeData->formats().join(QStringLiteral("|")));
}

void ClipboardWatcher::rememberClipboardState(const QMimeData* mimeData) {
  if (!mimeData) {
    return;
  }

  if (mimeData->hasText()) {
    lastText_ = mimeData->text().trimmed();
  } else {
    lastText_.clear();
  }

  if (mimeData->hasImage()) {
    const QImage image = qvariant_cast<QImage>(mimeData->imageData());
    lastImageFingerprint_ = imageFingerprint(image);
  } else {
    lastImageFingerprint_.clear();
  }
}

bool ClipboardWatcher::captureImageFromUrls(const QMimeData* mimeData) {
  if (!mimeData->hasUrls()) {
    return false;
  }

  const QList<QUrl> urls = mimeData->urls();
  for (const QUrl& url : urls) {
    if (!url.isLocalFile()) {
      continue;
    }

    const QString path = url.toLocalFile();
    QImage image(path);
    if (image.isNull()) {
      continue;
    }

    const QString fingerprint = imageFingerprint(image);
    if (!fingerprint.isEmpty() && fingerprint == lastImageFingerprint_) {
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

    const QString fileName = QFileInfo(path).fileName();
    if (storage_->insertImage(pngData, fileName, path)) {
      lastImageFingerprint_ = fingerprint;
      return true;
    }
  }

  return false;
}

bool ClipboardWatcher::captureText(const QMimeData* mimeData) {
  if (!mimeData->hasText()) {
    return false;
  }

  const QString text = mimeData->text().trimmed();
  if (text.isEmpty() || text == lastText_) {
    return false;
  }

  if (storage_->insertText(text)) {
    lastText_ = text;
    return true;
  }

  return false;
}

bool ClipboardWatcher::captureImage(const QMimeData* mimeData) {
  if (!mimeData->hasImage()) {
    return false;
  }

  const QImage image = qvariant_cast<QImage>(mimeData->imageData());
  if (image.isNull()) {
    return false;
  }

  const QString fingerprint = imageFingerprint(image);
  if (!fingerprint.isEmpty() && fingerprint == lastImageFingerprint_) {
    return false;
  }

  QByteArray pngData;
  QBuffer buffer(&pngData);
  if (!buffer.open(QIODevice::WriteOnly)) {
    return false;
  }

  if (!image.save(&buffer, "PNG")) {
    return false;
  }

  if (storage_->insertImage(pngData, QStringLiteral("clipboard-image.png"))) {
    lastImageFingerprint_ = fingerprint;
    return true;
  }

  return false;
}
