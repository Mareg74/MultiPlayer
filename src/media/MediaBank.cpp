#include "media/MediaBank.h"

#include <QDir>
#include <QFileInfo>

MediaBank::MediaBank(QObject *parent)
    : QObject(parent)
{
}

bool MediaBank::isSupportedMediaFile(const QString &path)
{
    static const QStringList exts = {
        QStringLiteral("mp4"),  QStringLiteral("mov"),  QStringLiteral("mkv"),
        QStringLiteral("avi"),  QStringLiteral("webm"), QStringLiteral("m4v"),
        QStringLiteral("mpg"),  QStringLiteral("mpeg"), QStringLiteral("wmv"),
        QStringLiteral("hap"),  QStringLiteral("png"),  QStringLiteral("jpg"),
        QStringLiteral("jpeg")};
    const QString ext = QFileInfo(path).suffix().toLower();
    return !ext.isEmpty() && exts.contains(ext);
}

bool MediaBank::setFolder(const QString &path)
{
    QDir dir(path);
    if (!dir.exists())
        return false;
    m_folder = dir.absolutePath();
    refresh();
    return true;
}

void MediaBank::refresh()
{
    m_files.clear();
    if (m_folder.isEmpty()) {
        emit changed();
        return;
    }

    QDir dir(m_folder);
    const QStringList filters = {
        QStringLiteral("*.mp4"),  QStringLiteral("*.mov"), QStringLiteral("*.mkv"),
        QStringLiteral("*.avi"),  QStringLiteral("*.webm"), QStringLiteral("*.m4v"),
        QStringLiteral("*.mpg"),  QStringLiteral("*.mpeg"), QStringLiteral("*.wmv"),
        QStringLiteral("*.hap"),  QStringLiteral("*.png"), QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg")};
    const QFileInfoList entries =
        dir.entryInfoList(filters, QDir::Files | QDir::Readable, QDir::Name);
    for (const QFileInfo &fi : entries)
        m_files.append(fi.absoluteFilePath());
    emit changed();
}
