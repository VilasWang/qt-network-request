#include "test_qtdownloader.h"

#include <QUrl>
#include "downloadtask.h"

using namespace QtNetworkRequest;
using State = NetworkDownloadTask::State;

void TestQtDownloader::testDefaultConstructor()
{
    NetworkDownloadTask t;
    QCOMPARE(t.totalBytes, qint64(-1));
    QCOMPARE(t.downloadedBytes, qint64(0));
    QCOMPARE(t.progress, 0);
    QCOMPARE(t.speed, qint64(0));
    QCOMPARE(t.elapsedMillis, qint64(0));
    QVERIFY(t.state == State::Waiting);
    QVERIFY(t.id.isEmpty());
}

void TestQtDownloader::testUrlConstructor()
{
    QUrl url("http://example.com/files/report.pdf");
    NetworkDownloadTask t(url, "C:/downloads");
    QVERIFY(!t.id.isEmpty());                 // a UUID is assigned
    QCOMPARE(t.url, url);
    QCOMPARE(t.fileName, QString("report.pdf"));
    QCOMPARE(t.savePath, QString("C:/downloads"));
    QVERIFY(t.state == State::Waiting);
}

void TestQtDownloader::testExtractFileName()
{
    QCOMPARE(NetworkDownloadTask::extractFileName(QUrl("http://h/a/b/file.zip")), QString("file.zip"));
    // No path or root path falls back to a generic name.
    QCOMPARE(NetworkDownloadTask::extractFileName(QUrl("http://h")), QString("download"));
    QCOMPARE(NetworkDownloadTask::extractFileName(QUrl("http://h/")), QString("download"));
    // Trailing slash → empty file component → fallback.
    QCOMPARE(NetworkDownloadTask::extractFileName(QUrl("http://h/dir/")), QString("download"));
}

void TestQtDownloader::testStateRoundTrip()
{
    // stateToString/stateFromString must round-trip. Note Running maps to the
    // user-facing label "Downloading".
    const State states[] = { State::Waiting, State::Running, State::Paused,
                             State::Completed, State::Error };
    NetworkDownloadTask t;
    for (State s : states)
    {
        t.state = s;
        QVERIFY(NetworkDownloadTask::stateFromString(t.stateToString()) == s);
    }
    t.state = State::Running;
    QCOMPARE(t.stateToString(), QString("Downloading"));
}

void TestQtDownloader::testStateFromUnknown()
{
    // Any unrecognised label degrades to Waiting.
    QVERIFY(NetworkDownloadTask::stateFromString("gibberish") == State::Waiting);
    QVERIFY(NetworkDownloadTask::stateFromString(QString()) == State::Waiting);
}

void TestQtDownloader::testFormatFileSize()
{
    NetworkDownloadTask t;
    QCOMPARE(t.formatFileSize(-1), QString("--"));
    QCOMPARE(t.formatFileSize(512), QString("512 B"));
    QCOMPARE(t.formatFileSize(2048), QString("2 KB"));          // integer KB
    QCOMPARE(t.formatFileSize(5 * 1024 * 1024), QString("5.00 MB"));
    QCOMPARE(t.formatFileSize(qint64(3) * 1024 * 1024 * 1024), QString("3.00 GB"));
}

void TestQtDownloader::testFormatSpeed()
{
    NetworkDownloadTask t;
    t.speed = 2048;
    QCOMPARE(t.formatSpeed(), QString("2 KB/s"));
}

void TestQtDownloader::testFormatDuration()
{
    QCOMPARE(NetworkDownloadTask::formatDuration(-5), QString("0s"));
    QCOMPARE(NetworkDownloadTask::formatDuration(45), QString("45s"));
    QCOMPARE(NetworkDownloadTask::formatDuration(90), QString("1m 30s"));
    QCOMPARE(NetworkDownloadTask::formatDuration(3661), QString("1h 1m 1s"));
}

void TestQtDownloader::testFormatTimeRunningEta()
{
    NetworkDownloadTask t;
    t.state = State::Running;
    t.totalBytes = 1000;
    t.downloadedBytes = 200;
    t.speed = 100;                 // 800 bytes remaining / 100 Bps = 8s
    QCOMPARE(t.formatTime(), QString("8s"));

    // Missing rate information yields the placeholder.
    t.speed = 0;
    QCOMPARE(t.formatTime(), QString("--"));
}

void TestQtDownloader::testFormatTimeCompletedElapsed()
{
    NetworkDownloadTask t;
    t.state = State::Completed;
    t.elapsedMillis = 90 * 1000;   // 90s → "1m 30s"
    QCOMPARE(t.formatTime(), QString("1m 30s"));

    t.elapsedMillis = 0;           // nothing recorded → placeholder
    QCOMPARE(t.formatTime(), QString("--"));
}

void TestQtDownloader::testIsValid()
{
    NetworkDownloadTask ok(QUrl("http://example.com/a.bin"));
    QVERIFY(ok.isValid());

    NetworkDownloadTask empty;
    QVERIFY(!empty.isValid());     // default-constructed url is empty/invalid
}
