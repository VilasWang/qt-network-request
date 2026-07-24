#ifndef TEST_QTDOWNLOADER_H
#define TEST_QTDOWNLOADER_H

#include <QObject>
#include <QtTest/QtTest>

// Pure (no-network, no-GUI) unit tests for the NetworkDownloader sample's
// NetworkDownloadTask value type. The struct is header-only and packed with
// deterministic formatting/state helpers, so it is exercised in its own
// lightweight executable.
class TestQtDownloader : public QObject
{
    Q_OBJECT

private slots:
    void testDefaultConstructor();
    void testUrlConstructor();
    void testExtractFileName();
    void testStateRoundTrip();
    void testStateFromUnknown();
    void testFormatFileSize();
    void testFormatSpeed();
    void testFormatDuration();
    void testFormatTimeRunningEta();
    void testFormatTimeCompletedElapsed();
    void testIsValid();
};

#endif // TEST_QTDOWNLOADER_H
