#pragma once

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QKeySequence>

#if defined(Q_OS_MAC)
#include <Carbon/Carbon.h>
#endif

class GlobalHotkey final : public QObject, public QAbstractNativeEventFilter {
  Q_OBJECT

 public:
  explicit GlobalHotkey(QObject* parent = nullptr);
  ~GlobalHotkey() override;

  bool registerOpenClipboardHotkey(const QKeySequence& sequence);
  bool registerOpenClipboardHotkey(const QString& hotkeyPortableText);
  void unregisterOpenClipboardHotkey();

  bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

 signals:
  void activated();

 private:
  void triggerActivated();

#if defined(Q_OS_WIN)
  int winHotkeyId_{0x5143};  // "QC"
  bool winRegistered_{false};
#endif

#if defined(Q_OS_MAC)
  EventHotKeyRef macHotkeyRef_{nullptr};
  EventHandlerRef macEventHandler_{nullptr};
  EventHotKeyID macHotkeyId_{};
  static OSStatus macHotkeyHandler(EventHandlerCallRef nextHandler, EventRef event, void* userData);
#endif
};
