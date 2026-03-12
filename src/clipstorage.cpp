#include "clipstorage.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <QVariant>

#include <algorithm>

namespace {
constexpr auto kDbFileName = "quickqc.sqlite";
constexpr auto kHistorySelectColumns =
    "id, kind, text, CAST(NULL AS BLOB) AS imageData, imageName, sourcePath, createdAt, updatedAt, pinned, tagsJson";
constexpr auto kFullSelectColumns =
    "id, kind, text, imageData, imageName, sourcePath, createdAt, updatedAt, pinned, tagsJson";

bool ensureColumn(QSqlDatabase& db, const QString& table, const QString& column, const QString& alterSql) {
  QSqlQuery info(db);
  if (!info.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
    return false;
  }

  while (info.next()) {
    if (info.value(1).toString() == column) {
      return true;
    }
  }

  QSqlQuery alter(db);
  return alter.exec(alterSql);
}
}

ClipStorage::ClipStorage(QString appName)
    : appName_(std::move(appName)) {}

ClipStorage::~ClipStorage() {
  if (db_.isOpen()) {
    db_.close();
  }

  const QString name = connectionName_;
  db_ = QSqlDatabase();

  if (!name.isEmpty()) {
    QSqlDatabase::removeDatabase(name);
  }
}

bool ClipStorage::open() {
  if (db_.isOpen()) {
    return true;
  }

  const QString appDataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir dir(appDataRoot);
  if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
    qWarning() << "Failed to create app data path:" << appDataRoot;
    return false;
  }

  dbPath_ = dir.filePath(QString::fromUtf8(kDbFileName));
  connectionName_ = QStringLiteral("quickqc-conn-%1")
                        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

  db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
  db_.setDatabaseName(dbPath_);

  if (!db_.open()) {
    qWarning() << "Failed to open SQLite DB:" << db_.lastError().text();
    return false;
  }

  return initSchema();
}

bool ClipStorage::isOpen() const {
  return db_.isOpen();
}

QString ClipStorage::databasePath() const {
  return dbPath_;
}

QList<ClipItem> ClipStorage::history(const QString& query, const int limit) const {
  QList<ClipItem> items;
  if (!db_.isOpen()) {
    return items;
  }

  const int safeLimit = std::clamp(limit, 1, 500);
  QSqlQuery q(db_);

  const QString trimmed = query.trimmed();
  if (trimmed.isEmpty()) {
    q.prepare(QStringLiteral(
        "SELECT %1 "
        "FROM clips "
        "ORDER BY pinned DESC, createdAt DESC "
        "LIMIT :limit")
                  .arg(QString::fromUtf8(kHistorySelectColumns)));
    q.bindValue(QStringLiteral(":limit"), safeLimit);
  } else if (trimmed.compare(QStringLiteral("image"), Qt::CaseInsensitive) == 0) {
    q.prepare(QStringLiteral(
        "SELECT %1 "
        "FROM clips "
        "WHERE kind = 'image' "
        "ORDER BY pinned DESC, createdAt DESC "
        "LIMIT :limit")
                  .arg(QString::fromUtf8(kHistorySelectColumns)));
    q.bindValue(QStringLiteral(":limit"), safeLimit);
  } else {
    q.prepare(QStringLiteral(
        "SELECT %1 "
        "FROM clips "
        "WHERE lower(COALESCE(text, '')) LIKE :needle "
        "   OR lower(COALESCE(imageName, '')) LIKE :needle "
        "   OR lower(COALESCE(sourcePath, '')) LIKE :needle "
        "   OR lower(COALESCE(tagsJson, '')) LIKE :needle "
        "ORDER BY pinned DESC, createdAt DESC "
        "LIMIT :limit")
                  .arg(QString::fromUtf8(kHistorySelectColumns)));

    q.bindValue(QStringLiteral(":needle"), QStringLiteral("%") + trimmed.toLower() + QStringLiteral("%"));
    q.bindValue(QStringLiteral(":limit"), safeLimit);
  }

  if (!q.exec()) {
    logQueryError(q, QStringLiteral("history"));
    return items;
  }

  while (q.next()) {
    items.push_back(rowToClip(q));
  }

  return items;
}

std::optional<ClipItem> ClipStorage::getClipById(const QString& id) const {
  if (!db_.isOpen() || id.trimmed().isEmpty()) {
    return std::nullopt;
  }

  QSqlQuery q(db_);
  q.prepare(QStringLiteral(
      "SELECT %1 "
      "FROM clips "
      "WHERE id = :id "
      "LIMIT 1")
                .arg(QString::fromUtf8(kFullSelectColumns)));
  q.bindValue(QStringLiteral(":id"), id);

  if (!q.exec()) {
    logQueryError(q, QStringLiteral("getClipById"));
    return std::nullopt;
  }

  if (!q.next()) {
    return std::nullopt;
  }

  return rowToClip(q);
}

bool ClipStorage::insertText(const QString& text) {
  const QString clipped = text.trimmed();
  if (clipped.isEmpty() || !db_.isOpen()) {
    return false;
  }

  QSqlQuery q(db_);
  q.prepare(
      "INSERT INTO clips(id, kind, text, createdAt, pinned, tagsJson) "
      "VALUES(:id, 'text', :text, :createdAt, 0, '[]')");

  q.bindValue(QStringLiteral(":id"), generateId());
  q.bindValue(QStringLiteral(":text"), clipped);
  q.bindValue(QStringLiteral(":createdAt"), QDateTime::currentMSecsSinceEpoch());

  if (!q.exec()) {
    logQueryError(q, QStringLiteral("insertText"));
    return false;
  }

  return true;
}

bool ClipStorage::insertImage(const QByteArray& pngData, const QString& imageName, const QString& sourcePath) {
  if (pngData.isEmpty() || !db_.isOpen()) {
    return false;
  }

  QSqlQuery q(db_);
  q.prepare(
      "INSERT INTO clips(id, kind, imageData, imageName, sourcePath, createdAt, pinned, tagsJson) "
      "VALUES(:id, 'image', :imageData, :imageName, :sourcePath, :createdAt, 0, '[]')");

  q.bindValue(QStringLiteral(":id"), generateId());
  q.bindValue(QStringLiteral(":imageData"), pngData);
  q.bindValue(QStringLiteral(":imageName"), imageName.trimmed().isEmpty() ? QStringLiteral("clipboard-image.png")
                                                                            : imageName.trimmed());
  q.bindValue(QStringLiteral(":sourcePath"), sourcePath.trimmed());
  q.bindValue(QStringLiteral(":createdAt"), QDateTime::currentMSecsSinceEpoch());

  if (!q.exec()) {
    logQueryError(q, QStringLiteral("insertImage"));
    return false;
  }

  return true;
}

bool ClipStorage::deleteClip(const QString& id) {
  if (!db_.isOpen() || id.isEmpty()) {
    return false;
  }

  QSqlQuery q(db_);
  q.prepare("DELETE FROM clips WHERE id = :id");
  q.bindValue(QStringLiteral(":id"), id);

  if (!q.exec()) {
    logQueryError(q, QStringLiteral("deleteClip"));
    return false;
  }

  return q.numRowsAffected() > 0;
}

bool ClipStorage::clearAll() {
  if (!db_.isOpen()) {
    return false;
  }

  QSqlQuery q(db_);
  if (!q.exec(QStringLiteral("DELETE FROM clips"))) {
    logQueryError(q, QStringLiteral("clearAll"));
    return false;
  }

  return true;
}

bool ClipStorage::togglePin(const QString& id) {
  if (!db_.isOpen() || id.isEmpty()) {
    return false;
  }

  QSqlQuery readQ(db_);
  readQ.prepare("SELECT pinned FROM clips WHERE id = :id");
  readQ.bindValue(QStringLiteral(":id"), id);

  if (!readQ.exec() || !readQ.next()) {
    logQueryError(readQ, QStringLiteral("togglePin/select"));
    return false;
  }

  const int nextPinned = readQ.value(0).toInt() == 0 ? 1 : 0;

  QSqlQuery writeQ(db_);
  writeQ.prepare("UPDATE clips SET pinned = :pinned WHERE id = :id");
  writeQ.bindValue(QStringLiteral(":pinned"), nextPinned);
  writeQ.bindValue(QStringLiteral(":id"), id);

  if (!writeQ.exec()) {
    logQueryError(writeQ, QStringLiteral("togglePin/update"));
    return false;
  }

  return writeQ.numRowsAffected() > 0;
}

bool ClipStorage::setTags(const QString& id, const QStringList& tags) {
  if (!db_.isOpen() || id.isEmpty()) {
    return false;
  }

  QJsonArray arr;
  for (const QString& tag : tags) {
    const QString trimmed = tag.trimmed();
    if (!trimmed.isEmpty()) {
      arr.append(trimmed);
    }
  }

  QSqlQuery q(db_);
  q.prepare("UPDATE clips SET tagsJson = :tagsJson, updatedAt = :updatedAt WHERE id = :id");
  q.bindValue(QStringLiteral(":tagsJson"), QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
  q.bindValue(QStringLiteral(":updatedAt"), QDateTime::currentMSecsSinceEpoch());
  q.bindValue(QStringLiteral(":id"), id);

  if (!q.exec()) {
    logQueryError(q, QStringLiteral("setTags"));
    return false;
  }

  return q.numRowsAffected() > 0;
}

bool ClipStorage::updateClipText(const QString& id, const QString& text) {
  if (!db_.isOpen() || id.isEmpty()) {
    return false;
  }

  QSqlQuery q(db_);
  q.prepare("UPDATE clips SET text = :text, updatedAt = :updatedAt WHERE id = :id AND kind = 'text'");
  q.bindValue(QStringLiteral(":text"), text.trimmed());
  q.bindValue(QStringLiteral(":updatedAt"), QDateTime::currentMSecsSinceEpoch());
  q.bindValue(QStringLiteral(":id"), id);

  if (!q.exec()) {
    logQueryError(q, QStringLiteral("updateClipText"));
    return false;
  }

  return q.numRowsAffected() > 0;
}

bool ClipStorage::renameClipImage(const QString& id, const QString& imageName) {
  if (!db_.isOpen() || id.isEmpty()) {
    return false;
  }

  QSqlQuery q(db_);
  q.prepare(
      "UPDATE clips "
      "SET imageName = :imageName, updatedAt = :updatedAt "
      "WHERE id = :id AND kind = 'image'");

  const QString safeName = imageName.trimmed().isEmpty() ? QStringLiteral("clipboard-image.png")
                                                          : imageName.trimmed();
  q.bindValue(QStringLiteral(":imageName"), safeName);
  q.bindValue(QStringLiteral(":updatedAt"), QDateTime::currentMSecsSinceEpoch());
  q.bindValue(QStringLiteral(":id"), id);

  if (!q.exec()) {
    logQueryError(q, QStringLiteral("renameClipImage"));
    return false;
  }

  return q.numRowsAffected() > 0;
}

ClipStorage::StorageStats ClipStorage::stats() const {
  StorageStats s;
  if (!db_.isOpen()) {
    return s;
  }

  QSqlQuery q(db_);
  q.prepare(
      "SELECT "
      "COUNT(*) AS totalCount, "
      "SUM(CASE WHEN kind = 'text' THEN 1 ELSE 0 END) AS textCount, "
      "SUM(CASE WHEN kind = 'image' THEN 1 ELSE 0 END) AS imageCount, "
      "SUM(CASE WHEN kind = 'text' THEN length(CAST(COALESCE(text, '') AS BLOB)) ELSE 0 END) AS textBytes, "
      "SUM(CASE WHEN kind = 'image' THEN length(COALESCE(imageData, X'')) ELSE 0 END) AS imageBytes "
      "FROM clips");

  if (!q.exec() || !q.next()) {
    logQueryError(q, QStringLiteral("stats"));
    return s;
  }

  s.totalCount = q.value(0).toLongLong();
  s.textCount = q.value(1).toLongLong();
  s.imageCount = q.value(2).toLongLong();
  s.textBytes = q.value(3).toLongLong();
  s.imageBytes = q.value(4).toLongLong();

  if (!dbPath_.isEmpty()) {
    const QFileInfo info(dbPath_);
    if (info.exists()) {
      s.dbFileBytes = info.size();
    }
  }

  return s;
}

bool ClipStorage::initSchema() {
  if (!db_.isOpen()) {
    return false;
  }

  QSqlQuery q(db_);

  const QStringList ddl = {
      QStringLiteral(
          "CREATE TABLE IF NOT EXISTS clips ("
          "id TEXT PRIMARY KEY,"
          "kind TEXT NOT NULL,"
          "text TEXT,"
          "imageData BLOB,"
          "imageName TEXT,"
          "sourcePath TEXT,"
          "createdAt INTEGER NOT NULL,"
          "updatedAt INTEGER,"
          "pinned INTEGER NOT NULL DEFAULT 0,"
          "tagsJson TEXT NOT NULL DEFAULT '[]'"
          ")"),
      QStringLiteral("CREATE INDEX IF NOT EXISTS idx_clips_pinned_created ON clips(pinned DESC, createdAt DESC)"),
      QStringLiteral("CREATE INDEX IF NOT EXISTS idx_clips_created ON clips(createdAt DESC)")};

  for (const QString& statement : ddl) {
    if (!q.exec(statement)) {
      logQueryError(q, QStringLiteral("initSchema"));
      return false;
    }
  }

  if (!ensureColumn(
          db_,
          QStringLiteral("clips"),
          QStringLiteral("sourcePath"),
          QStringLiteral("ALTER TABLE clips ADD COLUMN sourcePath TEXT"))) {
    qWarning() << "Failed to ensure sourcePath column";
  }

  return true;
}

QString ClipStorage::generateId() {
  return QStringLiteral("%1_%2")
      .arg(QString::number(QDateTime::currentMSecsSinceEpoch()),
           QUuid::createUuid().toString(QUuid::WithoutBraces));
}

ClipItem ClipStorage::rowToClip(const QSqlQuery& query) const {
  ClipItem item;
  item.id = query.value(0).toString();
  item.kind = clipKindFromString(query.value(1).toString());
  item.text = query.value(2).toString();
  item.imageData = query.value(3).toByteArray();
  item.imageName = query.value(4).toString();
  item.sourcePath = query.value(5).toString();
  item.createdAt = query.value(6).toLongLong();
  item.updatedAt = query.value(7).toLongLong();
  item.pinned = query.value(8).toInt() == 1;

  const auto tagsJson = query.value(9).toString().toUtf8();
  const QJsonDocument tagsDoc = QJsonDocument::fromJson(tagsJson);
  if (tagsDoc.isArray()) {
    for (const auto& v : tagsDoc.array()) {
      if (v.isString()) {
        item.tags.push_back(v.toString());
      }
    }
  }

  return item;
}

void ClipStorage::logQueryError(const QSqlQuery& query, const QString& context) const {
  qWarning() << "SQLite error in" << context << ":" << query.lastError().text();
}
