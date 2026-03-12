#pragma once

#include <QClipboard>
#include <QObject>

class QMimeData;
class QTimer;

class ClipStorage;

class ClipboardWatcher : public QObject {
  Q_OBJECT

 public:
  ClipboardWatcher(QClipboard* clipboard, ClipStorage* storage, QObject* parent = nullptr);

  void suppressCaptureForMs(int durationMs = 700);
  void primeFromCurrentClipboard();

 signals:
  void historyChanged();

 private slots:
  void onClipboardChanged(QClipboard::Mode mode);
  void onPollTick();

 private:
  void scanClipboard();
  QString clipboardSignature(const QMimeData* mimeData) const;
  void rememberClipboardState(const QMimeData* mimeData);

  bool captureImageFromUrls(const QMimeData* mimeData);
  bool captureText(const QMimeData* mimeData);
  bool captureImage(const QMimeData* mimeData);

  QClipboard* clipboard_;
  ClipStorage* storage_;
  QString lastText_;
  QString lastImageFingerprint_;
  QString lastClipboardSignature_;
  qint64 lastImageProbeMs_;
  qint64 suppressUntilMs_;
  QTimer* pollTimer_;
};
