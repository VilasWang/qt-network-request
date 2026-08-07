#include "networkrequestutils.h"
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
#include "responseresult.h"
#include "qtcompat.h"

using namespace QtNetworkRequest;

std::unique_ptr<QFile> NetworkRequestUtils::createAndOpenFile(const RequestContext *context, QString &errorMessage)
{
    std::unique_ptr<QFile> pFile;
    errorMessage.clear();

    Q_ASSERT(context && nullptr != context->downloadConfig);
    // Get download file save directory
    const QString &saveDir = getDownloadFileSaveDir(context, errorMessage);
    if (saveDir.isEmpty())
    {
        return pFile;
    }

    // Get download save filename
    const QString &strFileName = getSaveFileName(context);
    if (strFileName.isEmpty())
    {
        errorMessage = QString("Invalid request: File name cannot be empty");
        qWarning() << errorMessage;
        return pFile;
    }

    // If file exists and bReplaceFileIfExist is set, close file and remove
    const QString &filePath = QDir::toNativeSeparators(saveDir + strFileName);
    if (QFile::exists(filePath))
    {
        if (context->downloadConfig->overwriteFile)
        {
            QString strFileErr;
            if (!removeFile(filePath, strFileErr))
            {
                errorMessage = QString("File operation failed: Unable to remove existing file '%1' - %2").arg(filePath).arg(strFileErr);
                qWarning() << errorMessage;
                return pFile;
            }
        }
        else
        {
            errorMessage = QString("File conflict: Target file already exists at '%1'").arg(filePath);
            qWarning() << errorMessage;
            return pFile;
        }
    }

    // Create and open file
    pFile = std::make_unique<QFile>(filePath);
    if (!pFile->open(QIODevice::WriteOnly))
    {
        errorMessage = QString("File operation failed: Unable to open file '%1' for writing - %2").arg(filePath).arg(pFile->errorString());
        qWarning() << errorMessage;
        pFile.reset();
        return pFile;
    }
    return pFile;
}

bool NetworkRequestUtils::readFileContent(const QString &filePath, QByteArray &bytes, QString &errorMessage)
{
    errorMessage.clear();
    if (QFile::exists(filePath))
    {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly))
        {
            bytes = file.readAll();
            file.close();
            return true;
        }
        errorMessage = QString("File operation failed: Unable to open file '%1' for reading - %2").arg(filePath).arg(file.errorString());
    }
    else
    {
        errorMessage = QString("File not found: The specified file '%1' does not exist").arg(filePath);
    }
    qDebug() << "[QMultiThreadNetwork]" << errorMessage;
    return false;
}

QString NetworkRequestUtils::getFilePath(const RequestContext* context, QString &errorMessage)
{
    QString filePath;
    errorMessage.clear();

    Q_ASSERT(context && nullptr != context->downloadConfig);
    // Get download file save directory
    const QString &saveDir = getDownloadFileSaveDir(context, errorMessage);
    if (saveDir.isEmpty())
    {
        return filePath;
    }

    // Get download save filename
    const QString &strFileName = getSaveFileName(context);
    if (saveDir.isEmpty())
    {
        errorMessage = QString("Invalid request: File name cannot be empty");
        qWarning() << errorMessage;
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
            errorMessage = QString("File operation failed: Unable to remove existing file '%1' - %2").arg(filePath).arg(strFileErr);
            qWarning() << errorMessage;
        }
        else
        {
            return filePath;
        }
    }

    // If bOverwriteFile is not set, add suffix to filename, _1, _2, ...
    for (int i = 1; i < 100; ++i)
    {
        QString newFileName = strFileName + QString("_%1").arg(i);
        filePath = QDir::toNativeSeparators(saveDir + newFileName);
        if (!QFile::exists(filePath))
            return filePath;
    }

    return filePath;
}

QString NetworkRequestUtils::getSaveFileName(const RequestContext* context)
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
            const QStringList &strlist = pair.second.split(";", QtCompat::kSkipEmptyParts);
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

QString NetworkRequestUtils::getDownloadFileSaveDir(const RequestContext* context, QString &errorMessage)
{
    errorMessage.clear();
    Q_ASSERT(context && nullptr != context->downloadConfig);
    QString saveDir = QDir::toNativeSeparators(context->downloadConfig->saveDir);
    if (!saveDir.isEmpty())
    {
        QDir dir;
        if (!dir.exists(saveDir) && !dir.mkpath(saveDir))
        {
            errorMessage = QString("File system error: Failed to create directory path - %1").arg(saveDir);
            qWarning() << errorMessage;
            return QString();
        }
    }
    else
    {
        errorMessage = QString("Configuration error: Request task save directory cannot be empty");
        qWarning() << errorMessage;
        return QString();
    }
    if (!saveDir.endsWith(QDir::separator()))
    {
        saveDir.append(QDir::separator());
    }
    return saveDir;
}

bool NetworkRequestUtils::isFileExists(QFile *pFile)
{
    return (nullptr != pFile && pFile->exists());
}

bool NetworkRequestUtils::isFileOpened(QFile *pFile)
{
    return (nullptr != pFile && pFile->exists() && pFile->isOpen());
}

bool NetworkRequestUtils::removeFile(const QString &filePath, QString &errorMessage)
{
    QFile file(filePath);
    if (isFileExists(&file))
    {
        file.close();
        if (!file.remove())
        {
            errorMessage = file.errorString();
            return false;
        }
    }
    return true;
}

const QString NetworkRequestUtils::getRequestTypeString(const RequestType eType)
{
    QString requestTypeString;
    switch (eType)
    {
    case RequestType::Download:
    {
        requestTypeString = QString("Download");
    }
    break;
    case RequestType::MTDownload:
    {
        requestTypeString = QString("MT Download");
    }
    break;
    case RequestType::Upload:
    {
        requestTypeString = QString("Upload");
    }
    break;
    case RequestType::Get:
    {
        requestTypeString = QString("GET");
    }
    break;
    case RequestType::Post:
    {
        requestTypeString = QString("POST");
    }
    break;
    case RequestType::Put:
    {
        requestTypeString = QString("PUT");
    }
    break;
    case RequestType::Delete:
    {
        requestTypeString = QString("DELETE");
    }
    break;
    case RequestType::Head:
    {
        requestTypeString = QString("HEAD");
    }
    break;
    case RequestType::Patch:
    {
        requestTypeString = QString("PATCH");
    }
    break;
    case RequestType::Options:
    {
        requestTypeString = QString("OPTIONS");
    }
    break;
    default:
        break;
    }
    return requestTypeString;
}

std::unique_ptr<QFile> NetworkRequestUtils::openFile(const QString& filePath, QString& errorMessage)
{
    errorMessage.clear();
    auto pFile = std::make_unique<QFile>(filePath);
    if (pFile->exists())
    {
        if (pFile->open(QIODevice::ReadOnly))
        {
            return pFile;
        }
        errorMessage = QString("File operation failed: Unable to open file '%1' for reading - %2").arg(filePath).arg(pFile->errorString());
    }
    else
    {
        errorMessage = QString("File not found: The specified file '%1' does not exist").arg(filePath);
    }
    qDebug() << "[QMultiThreadNetwork]" << errorMessage;
    return nullptr;
}

namespace QtNetworkRequest
{
    ErrorInfo makeNetworkError(QNetworkReply::NetworkError code, const QString& message)
    {
        ErrorInfo info;
        info.nativeCode = static_cast<int>(code);
        info.message = message;

        switch (code)
        {
        case QNetworkReply::NoError:
            info.category = ErrorCategory::None;
            info.code = ErrorCode::NoError;
            break;
        case QNetworkReply::OperationCanceledError:
            info.category = ErrorCategory::Cancelled;
            info.code = ErrorCode::OperationCancelled;
            break;
        case QNetworkReply::TimeoutError:
            info.category = ErrorCategory::Timeout;
            info.code = ErrorCode::TimeoutTransfer;
            break;
        case QNetworkReply::ConnectionRefusedError:
            info.category = ErrorCategory::Network;
            info.code = ErrorCode::ConnectionRefused;
            break;
        case QNetworkReply::HostNotFoundError:
            info.category = ErrorCategory::Network;
            info.code = ErrorCode::HostNotFound;
            break;
        case QNetworkReply::RemoteHostClosedError:
            info.category = ErrorCategory::Network;
            info.code = ErrorCode::RemoteHostClosed;
            break;
        case QNetworkReply::TemporaryNetworkFailureError:
            info.category = ErrorCategory::Network;
            info.code = ErrorCode::TemporaryNetworkFailure;
            break;
        case QNetworkReply::ProxyConnectionRefusedError:
        case QNetworkReply::ProxyConnectionClosedError:
        case QNetworkReply::ProxyNotFoundError:
        case QNetworkReply::ProxyTimeoutError:
        case QNetworkReply::ProxyAuthenticationRequiredError:
        case QNetworkReply::UnknownProxyError:
            info.category = ErrorCategory::Network;
            info.code = ErrorCode::ProxyError;
            break;
        case QNetworkReply::TooManyRedirectsError:
            info.category = ErrorCategory::Network;
            info.code = ErrorCode::TooManyRedirects;
            break;
        case QNetworkReply::SslHandshakeFailedError:
            info.category = ErrorCategory::Ssl;
            info.code = ErrorCode::SslHandshakeFailed;
            break;
        default:
            info.category = ErrorCategory::Network;
            info.code = ErrorCode::Unknown;
            break;
        }
        return info;
    }

    ErrorInfo makeHttpError(int statusCode, const QString& message)
    {
        ErrorInfo info;
        if (statusCode >= 500)
        {
            info.category = ErrorCategory::Http;
            info.code = ErrorCode::HttpServerError;
        }
        else if (statusCode >= 400)
        {
            info.category = ErrorCategory::Http;
            info.code = ErrorCode::HttpClientError;
        }
        else
        {
            info.category = ErrorCategory::Unknown;
            info.code = ErrorCode::Unknown;
        }
        info.message = message.isEmpty()
            ? QString("HTTP error: status code %1").arg(statusCode)
            : message;
        return info;
    }

    const char* toString(ErrorCategory category)
    {
        switch (category)
        {
        case ErrorCategory::None: return "None";
        case ErrorCategory::Cancelled: return "Cancelled";
        case ErrorCategory::Timeout: return "Timeout";
        case ErrorCategory::Network: return "Network";
        case ErrorCategory::Ssl: return "Ssl";
        case ErrorCategory::Http: return "Http";
        case ErrorCategory::FileIo: return "FileIo";
        case ErrorCategory::Protocol: return "Protocol";
        case ErrorCategory::Configuration: return "Configuration";
        case ErrorCategory::Unknown: return "Unknown";
        }
        return "Unknown";
    }

    const char* toString(ErrorCode code)
    {
        switch (code)
        {
        case ErrorCode::NoError: return "NoError";
        case ErrorCode::OperationCancelled: return "OperationCancelled";
        case ErrorCode::TimeoutTotal: return "TimeoutTotal";
        case ErrorCode::TimeoutIdle: return "TimeoutIdle";
        case ErrorCode::TimeoutTransfer: return "TimeoutTransfer";
        case ErrorCode::ConnectionRefused: return "ConnectionRefused";
        case ErrorCode::HostNotFound: return "HostNotFound";
        case ErrorCode::RemoteHostClosed: return "RemoteHostClosed";
        case ErrorCode::TemporaryNetworkFailure: return "TemporaryNetworkFailure";
        case ErrorCode::ProxyError: return "ProxyError";
        case ErrorCode::TooManyRedirects: return "TooManyRedirects";
        case ErrorCode::SslHandshakeFailed: return "SslHandshakeFailed";
        case ErrorCode::HttpClientError: return "HttpClientError";
        case ErrorCode::HttpServerError: return "HttpServerError";
        case ErrorCode::InvalidUrl: return "InvalidUrl";
        case ErrorCode::UnsupportedProtocol: return "UnsupportedProtocol";
        case ErrorCode::UnsupportedRequestType: return "UnsupportedRequestType";
        case ErrorCode::InvalidReply: return "InvalidReply";
        case ErrorCode::ContentLengthMissing: return "ContentLengthMissing";
        case ErrorCode::FileOpenFailed: return "FileOpenFailed";
        case ErrorCode::FileWriteFailed: return "FileWriteFailed";
        case ErrorCode::FileReadFailed: return "FileReadFailed";
        case ErrorCode::MemoryMappingFailed: return "MemoryMappingFailed";
        case ErrorCode::FileRenameFailed: return "FileRenameFailed";
        case ErrorCode::TerminatedWithoutResponse: return "TerminatedWithoutResponse";
        case ErrorCode::Unknown: return "Unknown";
        }
        return "Unknown";
    }
}
