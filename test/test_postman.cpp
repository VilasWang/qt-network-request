#include "test_postman.h"
#include "collectionmodel.h"
#include "postmanconverter.h"

#include <QJsonArray>

using namespace QtNetworkRequest;

namespace
{
// A representative single-request payload. Includes an auth block so the
// Postman round-trip also exercises R2 (auth must survive export/import).
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

	QJsonObject auth;
	auth["type"] = "bearer";
	QJsonObject bearer;
	bearer["token"] = "abc123";
	auth["bearer"] = bearer;
	req["auth"] = auth;
	return req;
}

// Recursive structural comparison. Internal ids are intentionally dropped on
// Postman export (Postman assigns its own), so we compare type / name /
// request fields / auth rather than ids.
bool compareItems(const CollectionItem &a, const CollectionItem &b)
{
	if (a.type != b.type)
		return false;
	if (a.name != b.name)
		return false;
	if (a.type == CollectionItemType::Request)
	{
		const QJsonObject ra = a.requestJson;
		const QJsonObject rb = b.requestJson;
		if (ra["method"].toString() != rb["method"].toString())
			return false;
		if (ra["url"].toString() != rb["url"].toString())
			return false;
		if (ra["body"].toString() != rb["body"].toString())
			return false;
		if (ra["auth"] != rb["auth"])   // R2: auth round-trips
			return false;
		const QJsonArray ha = ra["headers"].toArray();
		const QJsonArray hb = rb["headers"].toArray();
		if (ha.size() != hb.size())
			return false;
		for (int i = 0; i < ha.size(); ++i)
		{
			if (ha[i].toObject()["key"].toString() != hb[i].toObject()["key"].toString())
				return false;
			if (ha[i].toObject()["value"].toString() != hb[i].toObject()["value"].toString())
				return false;
		}
	}
	if (a.children.size() != b.children.size())
		return false;
	for (int i = 0; i < a.children.size(); ++i)
		if (!compareItems(a.children[i], b.children[i]))
			return false;
	return true;
}
}

void PostmanTests::test_postmanExportRoundTrip()
{
	Collection c("ExportTest");
	const QString fid = c.addFolder("", "Folder1");
	c.addRequest(fid, "Req1", makeRequestJson());
	const QString fid2 = c.addFolder(fid, "SubFolder");
	c.addRequest(fid2, "Req2", makeRequestJson());

	const QJsonObject pm = PostmanConverter::toPostmanV21(c);
	QCOMPARE(pm["info"].toObject()["name"].toString(), QString("ExportTest"));
	QVERIFY(pm["item"].isArray());

	bool ok = false;
	Collection back = PostmanConverter::fromPostmanV21(pm, &ok);
	QVERIFY(ok);
	QCOMPARE(back.name(), QString("ExportTest"));
	QVERIFY(compareItems(c.root(), back.root()));
}

void PostmanTests::test_postmanImportBasic()
{
	QJsonObject reqObj;
	reqObj["method"] = "POST";
	QJsonObject url;
	url["raw"] = "http://example.com/api";
	reqObj["url"] = url;
	QJsonArray headers;
	QJsonObject h;
	h["key"] = "Content-Type";
	h["value"] = "application/json";
	headers.append(h);
	reqObj["header"] = headers;
	QJsonObject body;
	body["mode"] = "raw";
	body["raw"]  = "{\"k\":\"v\"}";
	reqObj["body"] = body;

	QJsonObject item;
	item["name"] = "BasicReq";
	item["request"] = reqObj;

	QJsonObject info;
	info["name"] = "Imported";
	QJsonObject doc;
	doc["info"] = info;
	QJsonArray items;
	items.append(item);
	doc["item"] = items;

	bool ok = false;
	Collection c = PostmanConverter::fromPostmanV21(doc, &ok);
	QVERIFY(ok);
	QCOMPARE(c.name(), QString("Imported"));
	QCOMPARE(c.root().children.size(), 1);

	const CollectionItem *r = c.findById(c.root().children.first().id);
	QVERIFY(r);
	QCOMPARE(r->type, CollectionItemType::Request);
	QCOMPARE(r->name, QString("BasicReq"));
	QCOMPARE(r->requestJson["method"].toString(), QString("POST"));
	QCOMPARE(r->requestJson["url"].toString(), QString("http://example.com/api"));
	QCOMPARE(r->requestJson["body"].toString(), QString("{\"k\":\"v\"}"));
	QCOMPARE(r->requestJson["headers"].toArray().size(), 1);
	QCOMPARE(r->requestJson["headers"].toArray().first().toObject()["key"].toString(),
	         QString("Content-Type"));
	QCOMPARE(r->requestJson["headers"].toArray().first().toObject()["value"].toString(),
	         QString("application/json"));
}

void PostmanTests::test_postmanImportNestedFolders()
{
	QJsonObject reqObj;
	reqObj["method"] = "GET";
	QJsonObject url;
	url["raw"] = "http://x/y";
	reqObj["url"] = url;
	QJsonObject req;
	req["name"] = "R";
	req["request"] = reqObj;

	QJsonObject f2;
	f2["name"] = "F2";
	QJsonArray f2items;
	f2items.append(req);
	f2["item"] = f2items;

	QJsonObject f1;
	f1["name"] = "F1";
	QJsonArray f1items;
	f1items.append(f2);
	f1["item"] = f1items;

	QJsonObject info;
	info["name"] = "N";
	QJsonObject doc;
	doc["info"] = info;
	QJsonArray items;
	items.append(f1);
	doc["item"] = items;

	bool ok = false;
	Collection c = PostmanConverter::fromPostmanV21(doc, &ok);
	QVERIFY(ok);
	QCOMPARE(c.root().children.size(), 1);

	const CollectionItem *f1p = c.findById(c.root().children.first().id);
	QVERIFY(f1p);
	QCOMPARE(f1p->type, CollectionItemType::Folder);
	QCOMPARE(f1p->name, QString("F1"));
	QCOMPARE(f1p->children.size(), 1);

	const CollectionItem *f2p = c.findById(f1p->children.first().id);
	QVERIFY(f2p);
	QCOMPARE(f2p->name, QString("F2"));
	QCOMPARE(f2p->children.size(), 1);

	const CollectionItem *rp = c.findById(f2p->children.first().id);
	QVERIFY(rp);
	QCOMPARE(rp->type, CollectionItemType::Request);
	QCOMPARE(rp->name, QString("R"));
	QCOMPARE(rp->requestJson["url"].toString(), QString("http://x/y"));
}
