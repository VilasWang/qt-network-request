#include "test_collection.h"
#include "collectionmodel.h"

#include <QFile>
#include <QDir>
#include <QJsonArray>

using namespace QtNetworkRequest;

namespace
{
QJsonObject makeRequestJson()
{
    QJsonObject req;
    req["method"] = "POST";
    req["url"]    = "http://example.com/api";
    req["body"]   = "{\"k\":\"v\"}";
    QJsonArray headers;
    QJsonObject h;
    h["key"]   = "Content-Type";
    h["value"] = "application/json";
    headers.append(h);
    req["headers"] = headers;
    return req;
}
}

void CollectionTests::testAddFolderAndRequest()
{
    Collection c("My Collection");
    const QString fid = c.addFolder("", "Folder1");
    QVERIFY(!fid.isEmpty());
    const QString rid = c.addRequest(fid, "Req1", makeRequestJson());
    QVERIFY(!rid.isEmpty());

    CollectionItem *folder = c.findById(fid);
    QVERIFY(folder);
    QCOMPARE(folder->type, CollectionItemType::Folder);
    QCOMPARE(folder->name, QString("Folder1"));

    CollectionItem *req = c.findById(rid);
    QVERIFY(req);
    QCOMPARE(req->type, CollectionItemType::Request);
    QCOMPARE(req->name, QString("Req1"));
    QCOMPARE(req->requestJson["method"].toString(), QString("POST"));
}

void CollectionTests::testRemoveNode()
{
    Collection c("C");
    const QString fid = c.addFolder("", "F");
    const QString rid = c.addRequest(fid, "R", makeRequestJson());
    QVERIFY(c.findById(rid));

    c.removeItem(rid);
    QVERIFY(!c.findById(rid));
    QVERIFY(c.findById(fid));   // sibling folder survives

    c.removeItem(fid);
    QVERIFY(!c.findById(fid));
}

void CollectionTests::testSerializeDeserialize()
{
    Collection c("Persisted");
    const QString fid = c.addFolder("", "F1");
    c.addRequest(fid, "R1", makeRequestJson());

    const QString path = QDir::temp().absoluteFilePath("qtmtnetwork_collection_test.json");
    QVERIFY(c.save(path));

    Collection c2;
    QVERIFY(c2.load(path));
    QCOMPARE(c2.name(), QString("Persisted"));
    QCOMPARE(c2.root().children.size(), 1);

    const CollectionItem *folder = c2.findById(fid);
    QVERIFY(folder);
    QCOMPARE(folder->name, QString("F1"));
    QCOMPARE(folder->children.size(), 1);
    QCOMPARE(folder->children.first().requestJson["method"].toString(), QString("POST"));
    QCOMPARE(folder->children.first().requestJson["url"].toString(), QString("http://example.com/api"));

    QFile::remove(path);
}
