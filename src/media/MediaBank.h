#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class MediaBank : public QObject
{
    Q_OBJECT

public:
    explicit MediaBank(QObject *parent = nullptr);

    bool setFolder(const QString &path);
    QString folder() const { return m_folder; }
    QStringList files() const { return m_files; }
    void refresh();

    static bool isSupportedMediaFile(const QString &path);

signals:
    void changed();

private:
    QString m_folder;
    QStringList m_files;
};
