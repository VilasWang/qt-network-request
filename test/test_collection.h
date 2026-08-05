#ifndef TEST_COLLECTION_H
#define TEST_COLLECTION_H

#include <QObject>
#include <QtTest/QtTest>

// Pure (no-network) unit tests for the Collection tree model.
class CollectionTests : public QObject
{
    Q_OBJECT

private slots:
    void testAddFolderAndRequest();
    void testRemoveNode();
    void testSerializeDeserialize();
};

#endif // TEST_COLLECTION_H
