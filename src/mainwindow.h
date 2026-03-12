#pragma once

#include "clipitem.h"

#include <QMainWindow>
#include <QSettings>
#include <QSystemTrayIcon>

#include <optional>

class ClipboardWatcher;
class ClipStorage;
class QClipboard;
class QCloseEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QAction;
class QMenu;
class QTimer;
class QWidget;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(ClipStorage* storage, ClipboardWatcher* watcher, QWidget* parent = nullptr);
  ~MainWindow() override;

 public slots:
  void refresh();
  void scheduleRefresh();
  void showNearCursor();

 private slots:
  void onSearchChanged(const QString& text);
  void onCopySelected();
  void onEditSelected();
  void onClearAll();
  void onImportImage();
  void onOpenSettings();
  void onCheckUpdates();
  void onItemActivated(QListWidgetItem* item);
  void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
  void onCurrentRowChanged(int currentRow);
  void refreshProcessUsage();

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  enum class ThemeMode {
    System,
    Dark,
    Light,
  };

  struct AppSettingsData {
    bool autoCloseOnCopy{false};
    int autoCloseDelayMs{0};
    ThemeMode themeMode{ThemeMode::Dark};
    bool startAtLogin{false};
    bool gpuPreviewEnabled{false};
  };

  struct GpuSupportInfo {
    bool checked{false};
    bool supported{false};
    QString vendor;
    QString renderer;
    QString version;
    QString detail;
  };

  void setupUi();
  void setupTray();
  void setupShortcuts();
  void applyStyles();
  void populateList(const QList<ClipItem>& items);
  void refreshStats();
  void openSettingsDialog();
  void rebuildTrayMenu();
  void showToast(const QString& message, int durationMs = 1200);

  void loadSettings();
  void saveSettings();
  void detectGpuSupport();
  QString gpuStatusText() const;
  bool isDarkTheme() const;
  QString versionString() const;

  static ThemeMode themeModeFromString(const QString& value);
  static QString themeModeToString(ThemeMode mode);

  bool setLaunchAtLogin(bool enabled, QString* errorMessage = nullptr) const;
  QString launchAgentPath() const;

  std::optional<ClipItem> selectedClip() const;
  std::optional<ClipItem> clipById(const QString& id) const;
  void setCurrentById(const QString& id);

  void copyToClipboard(const ClipItem& item);
  void copyClipById(const QString& id);
  void editClipById(const QString& id);
  void togglePinById(const QString& id);
  void deleteClipById(const QString& id);
  void previewImageById(const QString& id);

  QWidget* buildCardWidget(const ClipItem& item);
  void updateCardSelectionStyles();

  static QString clipPreview(const ClipItem& item);
  static QString formatTimestamp(qint64 unixMillis);
  static QString formatBytes(qint64 bytes);

  ClipStorage* storage_;
  ClipboardWatcher* watcher_;
  QClipboard* clipboard_;

  QLineEdit* searchEdit_;
  QListWidget* listWidget_;

  QPushButton* importButton_;
  QPushButton* settingsButton_;
  QPushButton* clearButton_;

  QLabel* storeSummaryLabel_;
  QLabel* cpuIconLabel_;
  QLabel* cpuSummaryLabel_;
  QLabel* toastLabel_;
  QLabel* statusLabel_;

  QSystemTrayIcon* tray_;
  QTimer* searchDebounceTimer_;
  QTimer* refreshCoalesceTimer_;
  QTimer* processUsageTimer_;
  QTimer* toastTimer_;

  QSettings settingsStore_;
  AppSettingsData appSettings_;
  GpuSupportInfo gpuInfo_;

  QMenu* trayMenu_;
  QAction* trayOpenClipboardAction_;
  QAction* trayOpenSettingsAction_;
  QAction* trayAutoCloseAction_;
  QAction* trayStartupAction_;
  QAction* trayUpdateAction_;
  QAction* trayHideAction_;
  QAction* trayQuitAction_;

  QList<ClipItem> visibleItems_;
};
