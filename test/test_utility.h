#ifndef TEST_UTILITY_H
#define TEST_UTILITY_H

#include <QObject>
#include <QtTest/QtTest>

// Pure (no-network) unit tests for NetworkRequestUtils — the internal helper
// that resolves download file names/dirs, performs file I/O, and maps request
// types to human-readable strings. These functions are deterministic and
// filesystem-only, so they live in their own executable alongside the builder
// contract tests.
class TestUtility : public QObject
{
    Q_OBJECT

private slots:
    void testGetRequestTypeString();
    void testGetSaveFileNameExplicit();
    void testGetSaveFileNameFromUrl();
    void testGetSaveFileNameFromContentDisposition();
    void testGetDownloadFileSaveDirEmpty();
    void testGetDownloadFileSaveDirCreatesAndAppendsSeparator();
    void testGetFilePathSuffixWhenNoOverwrite();
    void testGetFilePathOverwriteRemovesExisting();
    void testReadFileContentRoundTrip();
    void testReadFileContentMissing();
    void testRemoveFileAndExistence();
    void testOpenFileMissingAndPresent();
    void testCreateAndOpenFileConflict();
};

#endif // TEST_UTILITY_H
