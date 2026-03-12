#include "globalhotkey.h"

#include <QCoreApplication>
#include <QKeyCombination>
#include <QMetaObject>

#include <cstdint>

#if defined(Q_OS_WIN)
#include <windows.h>
#endif

#if defined(Q_OS_WIN)
UINT qtKeyToVirtualKey(const Qt::Key key) {
  if (key >= Qt::Key_A && key <= Qt::Key_Z) {
    return static_cast<UINT>('A' + (key - Qt::Key_A));
  }
  if (key >= Qt::Key_0 && key <= Qt::Key_9) {
    return static_cast<UINT>('0' + (key - Qt::Key_0));
  }
  if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
    return static_cast<UINT>(VK_F1 + (key - Qt::Key_F1));
  }

  switch (key) {
    case Qt::Key_Space:
      return VK_SPACE;
    case Qt::Key_Tab:
      return VK_TAB;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      return VK_RETURN;
    case Qt::Key_Escape:
      return VK_ESCAPE;
    case Qt::Key_Backspace:
      return VK_BACK;
    case Qt::Key_Delete:
      return VK_DELETE;
    case Qt::Key_Insert:
      return VK_INSERT;
    case Qt::Key_Home:
      return VK_HOME;
    case Qt::Key_End:
      return VK_END;
    case Qt::Key_PageUp:
      return VK_PRIOR;
    case Qt::Key_PageDown:
      return VK_NEXT;
    case Qt::Key_Left:
      return VK_LEFT;
    case Qt::Key_Right:
      return VK_RIGHT;
    case Qt::Key_Up:
      return VK_UP;
    case Qt::Key_Down:
      return VK_DOWN;
    default:
      return 0;
  }
}
#endif

#if defined(Q_OS_MAC)
UInt32 qtKeyToCarbonKeyCode(const Qt::Key key) {
  if (key >= Qt::Key_A && key <= Qt::Key_Z) {
    static const UInt32 letterMap[] = {
        kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E, kVK_ANSI_F, kVK_ANSI_G,
        kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J, kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N,
        kVK_ANSI_O, kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T, kVK_ANSI_U,
        kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y, kVK_ANSI_Z,
    };
    return letterMap[key - Qt::Key_A];
  }

  if (key >= Qt::Key_0 && key <= Qt::Key_9) {
    static const UInt32 digitMap[] = {
        kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
        kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9,
    };
    return digitMap[key - Qt::Key_0];
  }

  if (key >= Qt::Key_F1 && key <= Qt::Key_F20) {
    static const UInt32 functionMap[] = {
        kVK_F1,  kVK_F2,  kVK_F3,  kVK_F4,  kVK_F5,  kVK_F6,  kVK_F7,  kVK_F8,  kVK_F9,  kVK_F10,
        kVK_F11, kVK_F12, kVK_F13, kVK_F14, kVK_F15, kVK_F16, kVK_F17, kVK_F18, kVK_F19, kVK_F20,
    };
    return functionMap[key - Qt::Key_F1];
  }

  switch (key) {
    case Qt::Key_Space:
      return kVK_Space;
    case Qt::Key_Tab:
      return kVK_Tab;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      return kVK_Return;
    case Qt::Key_Escape:
      return kVK_Escape;
    case Qt::Key_Backspace:
      return kVK_Delete;
    case Qt::Key_Delete:
      return kVK_ForwardDelete;
    case Qt::Key_Left:
      return kVK_LeftArrow;
    case Qt::Key_Right:
      return kVK_RightArrow;
    case Qt::Key_Up:
      return kVK_UpArrow;
    case Qt::Key_Down:
      return kVK_DownArrow;
    case Qt::Key_Home:
      return kVK_Home;
    case Qt::Key_End:
      return kVK_End;
    case Qt::Key_PageUp:
      return kVK_PageUp;
    case Qt::Key_PageDown:
      return kVK_PageDown;
    default:
      return UINT32_MAX;
  }
}
#endif

GlobalHotkey::GlobalHotkey(QObject* parent)
    : QObject(parent) {}

GlobalHotkey::~GlobalHotkey() {
  unregisterOpenClipboardHotkey();
}

bool GlobalHotkey::registerOpenClipboardHotkey(const QString& hotkeyPortableText) {
  QKeySequence sequence = QKeySequence::fromString(hotkeyPortableText, QKeySequence::PortableText);
  if (sequence.isEmpty()) {
    sequence = QKeySequence::fromString(hotkeyPortableText, QKeySequence::NativeText);
  }
  return registerOpenClipboardHotkey(sequence);
}

bool GlobalHotkey::registerOpenClipboardHotkey(const QKeySequence& sequence) {
  unregisterOpenClipboardHotkey();

  if (sequence.isEmpty()) {
    return false;
  }

  const QKeyCombination combination = sequence[0];
  const Qt::Key key = static_cast<Qt::Key>(combination.key());
  if (key == Qt::Key_unknown) {
    return false;
  }

#if defined(Q_OS_WIN)
  UINT modifiers = MOD_NOREPEAT;
  const Qt::KeyboardModifiers qtMods = combination.keyboardModifiers();
  if (qtMods & Qt::ControlModifier) {
    modifiers |= MOD_CONTROL;
  }
  if (qtMods & Qt::ShiftModifier) {
    modifiers |= MOD_SHIFT;
  }
  if (qtMods & Qt::AltModifier) {
    modifiers |= MOD_ALT;
  }
  if (qtMods & Qt::MetaModifier) {
    modifiers |= MOD_WIN;
  }

  const UINT virtualKey = qtKeyToVirtualKey(key);
  if (virtualKey == 0) {
    return false;
  }

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

  UInt32 modifiers = 0;
  const Qt::KeyboardModifiers qtMods = combination.keyboardModifiers();
  if (qtMods & Qt::ShiftModifier) {
    modifiers |= shiftKey;
  }
  if (qtMods & Qt::ControlModifier) {
    modifiers |= controlKey;
  }
  if (qtMods & Qt::AltModifier) {
    modifiers |= optionKey;
  }
  if (qtMods & Qt::MetaModifier) {
    modifiers |= cmdKey;
  }

  const UInt32 keyCode = qtKeyToCarbonKeyCode(key);
  if (keyCode == UINT32_MAX) {
    if (macEventHandler_) {
      RemoveEventHandler(macEventHandler_);
      macEventHandler_ = nullptr;
    }
    return false;
  }

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
