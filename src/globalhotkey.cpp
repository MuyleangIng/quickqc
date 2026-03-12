#include "globalhotkey.h"

#include <QCoreApplication>
#include <QMetaObject>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

GlobalHotkey::GlobalHotkey(QObject* parent)
    : QObject(parent) {}

GlobalHotkey::~GlobalHotkey() {
  unregisterOpenClipboardHotkey();
}

bool GlobalHotkey::registerOpenClipboardHotkey() {
  unregisterOpenClipboardHotkey();

#if defined(Q_OS_WIN)
  const UINT modifiers = MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT;
  const UINT virtualKey = 'V';
  winRegistered_ = RegisterHotKey(nullptr, winHotkeyId_, modifiers, virtualKey);
  if (winRegistered_) {
    qApp->installNativeEventFilter(this);
  }
  return winRegistered_;
#elif defined(Q_OS_MAC)
  const EventTypeSpec eventType = {kEventClassKeyboard, kEventHotKeyPressed};
  if (InstallEventHandler(
          GetApplicationEventTarget(),
          &GlobalHotkey::macHotkeyHandler,
          1,
          &eventType,
          this,
          &macEventHandler_) != noErr) {
    macEventHandler_ = nullptr;
    return false;
  }

  macHotkeyId_.signature = 'QKQC';
  macHotkeyId_.id = 1;

  const UInt32 modifiers = cmdKey | shiftKey;
  const UInt32 keyCode = kVK_ANSI_V;
  const OSStatus status = RegisterEventHotKey(
      keyCode,
      modifiers,
      macHotkeyId_,
      GetApplicationEventTarget(),
      0,
      &macHotkeyRef_);

  if (status != noErr || !macHotkeyRef_) {
    if (macEventHandler_) {
      RemoveEventHandler(macEventHandler_);
      macEventHandler_ = nullptr;
    }
    return false;
  }

  return true;
#else
  return false;
#endif
}

void GlobalHotkey::unregisterOpenClipboardHotkey() {
#if defined(Q_OS_WIN)
  if (winRegistered_) {
    UnregisterHotKey(nullptr, winHotkeyId_);
    winRegistered_ = false;
    qApp->removeNativeEventFilter(this);
  }
#elif defined(Q_OS_MAC)
  if (macHotkeyRef_) {
    UnregisterEventHotKey(macHotkeyRef_);
    macHotkeyRef_ = nullptr;
  }
  if (macEventHandler_) {
    RemoveEventHandler(macEventHandler_);
    macEventHandler_ = nullptr;
  }
#endif
}

bool GlobalHotkey::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
  Q_UNUSED(result);

#if defined(Q_OS_WIN)
  if ((eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") && message) {
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_HOTKEY && static_cast<int>(msg->wParam) == winHotkeyId_) {
      triggerActivated();
      return true;
    }
  }
#else
  Q_UNUSED(eventType);
  Q_UNUSED(message);
#endif

  return false;
}

void GlobalHotkey::triggerActivated() {
  emit activated();
}

#if defined(Q_OS_MAC)
OSStatus GlobalHotkey::macHotkeyHandler(EventHandlerCallRef nextHandler, EventRef event, void* userData) {
  Q_UNUSED(nextHandler);

  if (!userData || !event) {
    return eventNotHandledErr;
  }

  auto* self = static_cast<GlobalHotkey*>(userData);
  EventHotKeyID hotkeyId{};
  if (GetEventParameter(
          event,
          kEventParamDirectObject,
          typeEventHotKeyID,
          nullptr,
          sizeof(EventHotKeyID),
          nullptr,
          &hotkeyId) != noErr) {
    return eventNotHandledErr;
  }

  if (hotkeyId.signature == self->macHotkeyId_.signature && hotkeyId.id == self->macHotkeyId_.id) {
    QMetaObject::invokeMethod(self, [self]() { self->triggerActivated(); }, Qt::QueuedConnection);
    return noErr;
  }

  return eventNotHandledErr;
}
#endif

