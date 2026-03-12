#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

enum class ClipKind {
  Text,
  Image,
};

struct ClipItem {
  QString id;
  ClipKind kind{ClipKind::Text};
  QString text;
  QByteArray imageData;  // Stored as PNG bytes in SQLite.
  QString imageName;
  QString sourcePath;
  qint64 createdAt{0};
  qint64 updatedAt{0};
  bool pinned{false};
  QStringList tags;
};

inline QString clipKindToString(ClipKind kind) {
  return kind == ClipKind::Image ? QStringLiteral("image") : QStringLiteral("text");
}

inline ClipKind clipKindFromString(const QString& value) {
  return value == QStringLiteral("image") ? ClipKind::Image : ClipKind::Text;
}
