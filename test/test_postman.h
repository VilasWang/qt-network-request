#ifndef TEST_POSTMAN_H
#define TEST_POSTMAN_H

#include <QObject>
#include <QtTest/QtTest>

// Pure (no-network) unit tests for the Postman v2.1 converter.
class PostmanTests : public QObject
{
	Q_OBJECT

private slots:
	void test_postmanExportRoundTrip();
	void test_postmanImportBasic();
	void test_postmanImportNestedFolders();
};

#endif // TEST_POSTMAN_H
