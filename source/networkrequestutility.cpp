#include "networkrequestutility.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <QUrlQuery>
#include <QList>
#include <QPair>
#include <QDir>
#include <QDebug>
#include <QFile>
#include "networkrequestdefs.h"

using namespace QtNetworkRequest;

std::unique_ptr<QFile> NetworkRequestUtility::createAndOpenFile(const RequestContext *context, QString &strError)
{
    std::unique_ptr<QFile> pFile;
    strError.clear();

    Q_ASSERT(context && nullptr != context->downloadConfig);
    // Get download file save directory
    const QString &saveDir = getDownloadFileSaveDir(context, strError);
    if (saveDir.isEmpty())
    {
        return pFile;
    }

    // Get download save filename
    const QString &strFileName = getSaveFileName(context);
    if (strFileName.isEmpty())
    {
        strError = QString("Invalid request: File name cannot be empty");
        qWarning() << strError;
        return pFile;
    }

    // If file exists and bReplaceFileIfExist is set, close file and remove
    const QString &strFilePath = QDir::toNativeSeparators(saveDir + strFileName);
    if (QFile::exists(strFilePath))
    {
        if (context->downloadConfig->overwriteFile)
        {
            QString strFileErr;
            if (!removeFile(strFilePath, strFileErr))
            {
                strError = QString("File operation failed: Unable to remove existing file '%1' - %2").arg(strFilePath).arg(strFileErr);
                qWarning() << strError;
                return pFile;
            }
        }
        else
        {
            strError = QString("File conflict: Target file already exists at '%1'").arg(strFilePath);
            qWarning() << strError;
            return pFile;
        }
    }

    // Create and open file
    pFile = std::make_unique<QFile>(strFilePath);
    if (!pFile->open(QIODevice::WriteOnly))
    {
        strError = QString("File operation failed: Unable to open file '%1' for writing - %2").arg(strFilePath).arg(pFile->errorString());
        qWarning() << strError;
        pFile.reset();
        return pFile;
    }
    return pFile;
}

bool NetworkRequestUtility::readFileContent(const QString &strFilePath, QByteArray &bytes, QString &strError)
{
    strError.clear();
    if (QFile::exists(strFilePath))
    {
        QFile file(strFilePath);
        if (file.open(QIODevice::ReadOnly))
        {
            bytes = file.readAll();
            file.close();
            return true;
        }
        strError = QString("File operation failed: Unable to open file '%1' for reading - %2").arg(strFilePath).arg(file.errorString());
    }
    else
    {
        strError = QString("File not found: The specified file '%1' does not exist").arg(strFilePath);
    }
    qDebug() << "[QMultiThreadNetwork]" << strError;
    return false;
}

QString NetworkRequestUtility::getFilePath(const RequestContext* context, QString &strError)
{
    QString filePath;
    strError.clear();

    Q_ASSERT(context && nullptr != context->downloadConfig);
    // Get download file save directory
    const QString &saveDir = getDownloadFileSaveDir(context, strError);
    if (saveDir.isEmpty())
    {
        return filePath;
    }

    // Get download save filename
    const QString &strFileName = getSaveFileName(context);
    if (saveDir.isEmpty())
    {
        strError = QString("Invalid request: File name cannot be empty");
        qWarning() << strError;
        return filePath;
    }

    filePath = QDir::toNativeSeparators(saveDir + strFileName);
    if (!QFile::exists(filePath))
        return filePath;

    // If file exists and bOverwriteFile is set, close file and remove
    if (context->downloadConfig->overwriteFile)
    {
        QString strFileErr;
        if (!removeFile(filePath, strFileErr))
        {
            strError = QString("File operation failed: Unable to remove existing file '%1' - %2").arg(filePath).arg(strFileErr);
            qWarning() << strError;
        }
        else
        {
            return filePath;
        }
    }

    // If bOverwriteFile is not set, add suffix to filename, _1, _2, ...
    for (int i = 1; i < 100; ++i)
    {
        QString strFileName = strFileName + QString("_%1").arg(i);
        filePath = QDir::toNativeSeparators(saveDir + strFileName);
        if (!QFile::exists(filePath))
            return filePath;
    }

    return filePath;
}

QString NetworkRequestUtility::getSaveFileName(const RequestContext* context)
{
    Q_ASSERT(context && nullptr != context->downloadConfig);
    if (!context->downloadConfig->saveFileName.isEmpty())
    {
        return context->downloadConfig->saveFileName;
    }

    const QUrl &url = QUrl(context->url);
    if (!url.isValid())
    {
        return QString();
    }

    QString strFileName;
    // Extract filename from url, format like: response-content-disposition=attachment; filename=test.exe
    QUrlQuery query(url.query(QUrl::FullyDecoded));
    const QList<QPair<QString, QString>> &querys = query.queryItems();
    foreach (auto pair, querys)
    {
        if (pair.first.compare("response-content-disposition", Qt::CaseInsensitive) == 0 || pair.first.compare("content-disposition", Qt::CaseInsensitive) == 0)
        {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
            const QStringList &strlist = pair.second.split(";", QString::SkipEmptyParts);
#else
            const QStringList &strlist = pair.second.split(";", QString::SkipEmptyParts);
#endif
            foreach (QString str, strlist)
            {
                str = str.trimmed();
                if (str.startsWith(QString("filename="), Qt::CaseInsensitive))
                {
                    strFileName = str.right(str.size() - QString("filename=").size());

                    // Filename cannot contain \/|":<> symbols
                    QStringList strlst;
                    strlst << "\"" << ":" << "<" << ">" << "|" << "/" << "\\";
                    for (auto &str : strlst)
                    {
                        int index = strFileName.indexOf(str);
                        while (-1 != index)
                        {
                            strFileName.remove(index, str.length());
                            index = strFileName.indexOf(str);
                        }
                    }
                    break;
                }
            }
            break;
        }
    }
    if (strFileName.isEmpty())
    {
        strFileName = url.fileName();
    }

    return strFileName;
}

QString NetworkRequestUtility::getDownloadFileSaveDir(const RequestContext* context, QString &strError)
{
    strError.clear();
    Q_ASSERT(context && nullptr != context->downloadConfig);
    QString saveDir = QDir::toNativeSeparators(context->downloadConfig->saveDir);
    if (!saveDir.isEmpty())
    {
        QDir dir;
        if (!dir.exists(saveDir) && !dir.mkpath(saveDir))
        {
            strError = QString("File system error: Failed to create directory path - %1").arg(saveDir);
            qWarning() << strError;
            return QString();
        }
    }
    else
    {
        strError = QString("Configuration error: Request task save directory cannot be empty");
        qWarning() << strError;
        return QString();
    }
    if (!saveDir.endsWith("\\"))
    {
        saveDir.append("\\");
    }
    return saveDir;
}

bool NetworkRequestUtility::isFileExists(QFile *pFile)
{
    return (nullptr != pFile && pFile->exists());
}

bool NetworkRequestUtility::isFileOpened(QFile *pFile)
{
    return (nullptr != pFile && pFile->exists() && pFile->isOpen());
}

bool NetworkRequestUtility::removeFile(const QString &strFilePath, QString &errMessage)
{
    QFile file(strFilePath);
    if (isFileExists(&file))
    {
        file.close();
        if (!file.remove())
        {
            errMessage = file.errorString();
            return false;
        }
    }
    return true;
}

const QString NetworkRequestUtility::getRequestTypeString(const RequestType eType)
{
    QString strType;
    switch (eType)
    {
    case RequestType::Download:
    {
        strType = QString("Download");
    }
    break;
    case RequestType::MTDownload:
    {
        strType = QString("MT Download");
    }
    break;
    case RequestType::Upload:
    {
        strType = QString("Upload");
    }
    break;
    case RequestType::Get:
    {
        strType = QString("GET");
    }
    break;
    case RequestType::Post:
    {
        strType = QString("POST");
    }
    break;
    case RequestType::Put:
    {
        strType = QString("PUT");
    }
    break;
    case RequestType::Delete:
    {
        strType = QString("DELETE");
    }
    break;
    case RequestType::Head:
    {
        strType = QString("HEAD");
    }
    break;
    default:
        break;
    }
    return strType;
}

std::unique_ptr<QFile> NetworkRequestUtility::openFile(const QString& strFilePath, QString& errMessage)
{
    errMessage.clear();
    auto pFile = std::make_unique<QFile>(strFilePath);
    if (pFile->exists())
    {
        if (pFile->open(QIODevice::ReadOnly))
        {
            return pFile;
        }
        errMessage = QString("File operation failed: Unable to open file '%1' for reading - %2").arg(strFilePath).arg(pFile->errorString());
    }
    else
    {
        errMessage = QString("File not found: The specified file '%1' does not exist").arg(strFilePath);
    }
    qDebug() << "[QMultiThreadNetwork]" << errMessage;
    return nullptr;
}
