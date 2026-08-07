#pragma once

#include <memory>
#include <QString>
#include <QNetworkReply>
#include "requestcontext.h"
#include "networkerror.h"

class QFile;
class QUrl;

namespace QtNetworkRequest
{
    class NetworkRequestUtils
    {
    public:
        // Create and open file
        static std::unique_ptr<QFile> createAndOpenFile(const RequestContext* context, QString &errorMessage);

        // Create shared read/write file
        static QString getFilePath(const RequestContext* context, QString &errorMessage);

        // Read file content
        static bool readFileContent(const QString &filePath, QByteArray &bytes, QString &errorMessage);

        // Open file
        static std::unique_ptr<QFile> openFile(const QString& filePath, QString& errorMessage);

        // Get save filename for download file
        static QString getSaveFileName(const RequestContext* context);
        // Get save directory for download file
        static QString getDownloadFileSaveDir(const RequestContext* context, QString &errorMessage);

        static bool isFileExists(QFile *pFile);
        static bool isFileOpened(QFile *pFile);
        static bool removeFile(const QString &filePath, QString &errorMessage);

        static const QString getRequestTypeString(const RequestType eType);

    private:
        NetworkRequestUtils() {}
        virtual ~NetworkRequestUtils() {}
        NetworkRequestUtils(const NetworkRequestUtils &) = delete;
        NetworkRequestUtils &operator=(const NetworkRequestUtils &) = delete;
    };

    // 将 Qt 网络错误码映射为结构化错误 (内部使用, 依赖 Qt 类型)
    ErrorInfo makeNetworkError(QNetworkReply::NetworkError code, const QString& message);
}
