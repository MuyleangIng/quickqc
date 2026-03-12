#pragma once

#include "clipitem.h"

#include <QList>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include <optional>

class QSqlQuery;

class ClipStorage {
 public:
  struct StorageStats {
    qint64 totalCount{0};
    qint64 textCount{0};
    qint64 imageCount{0};
    qint64 textBytes{0};
    qint64 imageBytes{0};
    qint64 dbFileBytes{0};
  };

  explicit ClipStorage(QString appName = QStringLiteral("quickqc"));
  ~ClipStorage();

  bool open();
  bool isOpen() const;
  QString databasePath() const;

  QList<ClipItem> history(const QString& query, int limit = 80) const;
  std::optional<ClipItem> getClipById(const QString& id) const;
  bool insertText(const QString& text);
  bool insertImage(const QByteArray& pngData, const QString& imageName, const QString& sourcePath = QString());

  bool deleteClip(const QString& id);
  bool clearAll();
  bool togglePin(const QString& id);
  bool setTags(const QString& id, const QStringList& tags);
  bool updateClipText(const QString& id, const QString& text);
  bool renameClipImage(const QString& id, const QString& imageName);
  StorageStats stats() const;

 private:
  QString appName_;
  QString connectionName_;
  QString dbPath_;
  QSqlDatabase db_;

  bool initSchema();
  static QString generateId();
  ClipItem rowToClip(const QSqlQuery& query) const;
  void logQueryError(const QSqlQuery& query, const QString& context) const;
};
