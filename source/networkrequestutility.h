#pragma once

#include <memory>
#include <QString>
#include "networkrequestdefs.h"

class QFile;
class QUrl;

namespace QtNetworkRequest
{
    class NetworkRequestUtility
    {
    public:
        // Create and open file
        static std::unique_ptr<QFile> createAndOpenFile(const RequestContext* context, QString &errMessage);

        // Create shared read/write file
        static QString getFilePath(const RequestContext* context, QString &errMessage);

        // Read file content
        static bool readFileContent(const QString &strFilePath, QByteArray &bytes, QString &errMessage);

        // Open file
        static std::unique_ptr<QFile> openFile(const QString& strFilePath, QString& errMessage);

        // Get save filename for download file
        static QString getSaveFileName(const RequestContext* context);
        // Get save directory for download file
        static QString getDownloadFileSaveDir(const RequestContext* context, QString &errMessage);

        static bool isFileExists(QFile *pFile);
        static bool isFileOpened(QFile *pFile);
        static bool removeFile(const QString &strFilePath, QString &errMessage);

        static const QString getRequestTypeString(const RequestType eType);

    private:
        NetworkRequestUtility() {}
        virtual ~NetworkRequestUtility() {}
        NetworkRequestUtility(const NetworkRequestUtility &) = delete;
        NetworkRequestUtility &operator=(const NetworkRequestUtility &) = delete;
    };
}
