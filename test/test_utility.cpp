#include "test_utility.h"

#include <memory>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QUrlQuery>
#include <QTemporaryDir>
#include <QTemporaryFile>

#include "networkrequestutility.h"
#include "requestcontext.h"

using namespace QtNetworkRequest;

namespace
{
    // Build a minimal RequestContext carrying a DownloadConfig — the only shape
    // the file-name/dir helpers ever inspect.
    std::unique_ptr<RequestContext> makeDownloadContext(const QString &url,
                                                        const QString &saveDir,
                                                        const QString &saveFileName,
                                                        bool overwrite)
    {
        auto ctx = std::make_unique<RequestContext>();
        ctx->url = url;
        ctx->type = RequestType::Download;
        ctx->downloadConfig = std::make_unique<DownloadConfig>();
        ctx->downloadConfig->saveDir = saveDir;
        ctx->downloadConfig->saveFileName = saveFileName;
        ctx->downloadConfig->overwriteFile = overwrite;
        return ctx;
    }
}

void TestUtility::testGetRequestTypeString()
{
    QCOMPARE(NetworkRequestUtility::getRequestTypeString(RequestType::Get), QString("GET"));
    QCOMPARE(NetworkRequestUtility::getRequestTypeString(RequestType::Post), QString("POST"));
    QCOMPARE(NetworkRequestUtility::getRequestTypeString(RequestType::Put), QString("PUT"));
    QCOMPARE(NetworkRequestUtility::getRequestTypeString(RequestType::Delete), QString("DELETE"));
    QCOMPARE(NetworkRequestUtility::getRequestTypeString(RequestType::Head), QString("HEAD"));
    QCOMPARE(NetworkRequestUtility::getRequestTypeString(RequestType::Patch), QString("PATCH"));
    QCOMPARE(NetworkRequestUtility::getRequestTypeString(RequestType::Options), QString("OPTIONS"));
    QCOMPARE(NetworkRequestUtility::getRequestTypeString(RequestType::Download), QString("Download"));
    QCOMPARE(NetworkRequestUtility::getRequestTypeString(RequestType::MTDownload), QString("MT Download"));
    QCOMPARE(NetworkRequestUtility::getRequestTypeString(RequestType::Upload), QString("Upload"));
    // Unknown maps to an empty string (default switch branch).
    QCOMPARE(NetworkRequestUtility::getRequestTypeString(RequestType::Unknown), QString());
}

void TestUtility::testGetSaveFileNameExplicit()
{
    // An explicit saveFileName always wins, ignoring the URL entirely.
    auto ctx = makeDownloadContext("http://example.com/path/ignored.bin", QString(), "explicit.dat", false);
    QCOMPARE(NetworkRequestUtility::getSaveFileName(ctx.get()), QString("explicit.dat"));
}

void TestUtility::testGetSaveFileNameFromUrl()
{
    // With no explicit name, the trailing URL path segment is used.
    auto ctx = makeDownloadContext("http://example.com/dir/archive.zip", QString(), QString(), false);
    QCOMPARE(NetworkRequestUtility::getSaveFileName(ctx.get()), QString("archive.zip"));
}

void TestUtility::testGetSaveFileNameFromContentDisposition()
{
    // A response-content-disposition query hint takes precedence over the URL
    // path's own file name.
    QUrl u("http://example.com/download");
    QUrlQuery q;
    q.addQueryItem("response-content-disposition", "attachment; filename=report.pdf");
    u.setQuery(q);

    auto ctx = makeDownloadContext(u.toString(), QString(), QString(), false);
    QCOMPARE(NetworkRequestUtility::getSaveFileName(ctx.get()), QString("report.pdf"));
}

void TestUtility::testGetDownloadFileSaveDirEmpty()
{
    // An empty saveDir is a configuration error and yields an empty result.
    auto ctx = makeDownloadContext("http://example.com/a.bin", QString(), "a.bin", false);
    QString err;
    QString dir = NetworkRequestUtility::getDownloadFileSaveDir(ctx.get(), err);
    QVERIFY(dir.isEmpty());
    QVERIFY(!err.isEmpty());
}

void TestUtility::testGetDownloadFileSaveDirCreatesAndAppendsSeparator()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    // Point at a not-yet-existing nested subdirectory: the helper must mkpath it.
    QString target = tmp.path() + "/nested/deep";
    QVERIFY(!QDir(target).exists());

    auto ctx = makeDownloadContext("http://example.com/a.bin", target, "a.bin", false);
    QString err;
    QString dir = NetworkRequestUtility::getDownloadFileSaveDir(ctx.get(), err);

    QVERIFY2(err.isEmpty(), qPrintable(err));
    QVERIFY(!dir.isEmpty());
    QVERIFY2(QDir(target).exists(), "save dir should have been created");
    QVERIFY2(dir.endsWith(QDir::separator()), "returned dir must end with a native separator");
}

void TestUtility::testGetFilePathSuffixWhenNoOverwrite()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    auto ctx = makeDownloadContext("http://example.com/x", tmp.path(), "file.dat", false);

    // Pre-create the target so the helper is forced to disambiguate.
    QString err;
    QString saveDir = NetworkRequestUtility::getDownloadFileSaveDir(ctx.get(), err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QString existing = QDir::toNativeSeparators(saveDir + "file.dat");
    {
        QFile f(existing);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("original");
        f.close();
    }

    QString resolved = NetworkRequestUtility::getFilePath(ctx.get(), err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    // overwriteFile=false + existing file → a _1 suffix is appended.
    QVERIFY2(resolved.endsWith("file.dat_1"),
             qPrintable(QString("expected _1 suffix, got: %1").arg(resolved)));
    // The pre-existing file is left untouched.
    QVERIFY(QFile::exists(existing));
}

void TestUtility::testGetFilePathOverwriteRemovesExisting()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    auto ctx = makeDownloadContext("http://example.com/x", tmp.path(), "file.dat", true);

    QString err;
    QString saveDir = NetworkRequestUtility::getDownloadFileSaveDir(ctx.get(), err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QString existing = QDir::toNativeSeparators(saveDir + "file.dat");
    {
        QFile f(existing);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("original");
        f.close();
    }
    QVERIFY(QFile::exists(existing));

    QString resolved = NetworkRequestUtility::getFilePath(ctx.get(), err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    // overwriteFile=true → original path returned and the stale file removed.
    QCOMPARE(resolved, existing);
    QVERIFY2(!QFile::exists(existing), "overwrite must remove the stale file");
}

void TestUtility::testReadFileContentRoundTrip()
{
    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    const QByteArray payload = "hello utility content 0123456789";
    tmp.write(payload);
    tmp.flush();
    QString path = tmp.fileName();
    tmp.close();

    QByteArray out;
    QString err;
    QVERIFY(NetworkRequestUtility::readFileContent(path, out, err));
    QVERIFY(err.isEmpty());
    QCOMPARE(out, payload);
}

void TestUtility::testReadFileContentMissing()
{
    QByteArray out;
    QString err;
    QString missing = QDir::tempPath() + "/qt_utility_missing_" +
                      QString::number(QCoreApplication::applicationPid()) + ".nope";
    QFile::remove(missing);

    QVERIFY(!NetworkRequestUtility::readFileContent(missing, out, err));
    QVERIFY(!err.isEmpty());
}

void TestUtility::testRemoveFileAndExistence()
{
    QString path = QDir::tempPath() + "/qt_utility_remove_" +
                   QString::number(QCoreApplication::applicationPid()) + ".dat";
    QFile::remove(path);
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("data");
        f.close();
    }

    QFile probe(path);
    QVERIFY(NetworkRequestUtility::isFileExists(&probe));

    QString err;
    QVERIFY(NetworkRequestUtility::removeFile(path, err));
    QVERIFY(err.isEmpty());
    QVERIFY(!QFile::exists(path));

    // Removing an already-absent file is a no-op success.
    QVERIFY(NetworkRequestUtility::removeFile(path, err));
    QVERIFY(err.isEmpty());
}

void TestUtility::testOpenFileMissingAndPresent()
{
    QString missing = QDir::tempPath() + "/qt_utility_open_missing_" +
                      QString::number(QCoreApplication::applicationPid()) + ".dat";
    QFile::remove(missing);

    QString err;
    std::unique_ptr<QFile> none = NetworkRequestUtility::openFile(missing, err);
    QVERIFY(none == nullptr);
    QVERIFY(!err.isEmpty());

    QTemporaryFile tmp;
    QVERIFY(tmp.open());
    tmp.write("payload");
    tmp.flush();
    QString path = tmp.fileName();
    tmp.close();

    std::unique_ptr<QFile> opened = NetworkRequestUtility::openFile(path, err);
    QVERIFY(opened != nullptr);
    QVERIFY(opened->isOpen());
    QVERIFY(err.isEmpty());
    opened->close();
}

void TestUtility::testCreateAndOpenFileConflict()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    // First: overwrite=false against a fresh dir succeeds and opens the file.
    auto ctx = makeDownloadContext("http://example.com/x", tmp.path(), "target.bin", false);
    QString err;
    std::unique_ptr<QFile> created = NetworkRequestUtility::createAndOpenFile(ctx.get(), err);
    QVERIFY2(created != nullptr, qPrintable(err));
    QVERIFY(created->isOpen());
    created->close();

    // Second: same name, overwrite=false, file now exists → File conflict error.
    auto ctx2 = makeDownloadContext("http://example.com/x", tmp.path(), "target.bin", false);
    QString err2;
    std::unique_ptr<QFile> conflict = NetworkRequestUtility::createAndOpenFile(ctx2.get(), err2);
    QVERIFY(conflict == nullptr);
    QVERIFY2(err2.contains("File conflict"), qPrintable(err2));

    // Third: overwrite=true replaces the existing file successfully.
    auto ctx3 = makeDownloadContext("http://example.com/x", tmp.path(), "target.bin", true);
    QString err3;
    std::unique_ptr<QFile> replaced = NetworkRequestUtility::createAndOpenFile(ctx3.get(), err3);
    QVERIFY2(replaced != nullptr, qPrintable(err3));
    QVERIFY(replaced->isOpen());
    replaced->close();
}
