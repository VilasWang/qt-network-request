/*
@Brief:			Qt multi-threaded network request module – Postman v2.1 converter

MIT License

Copyright (c) 2025 Lucas Wang

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "postmanconverter.h"

#include <QJsonArray>
#include <QJsonObject>

namespace QtNetworkRequest
{
	QJsonObject PostmanConverter::requestToPostman(const QJsonObject &requestJson)
	{
		QJsonObject req;
		req["method"] = requestJson["method"].toString("GET");

		QJsonObject url;
		url["raw"] = requestJson["url"].toString();
		req["url"] = url;

		QJsonArray headers;
		for (const auto &h : requestJson["headers"].toArray())
		{
			const QJsonObject ho = h.toObject();
			QJsonObject oh;
			oh["key"]   = ho["key"];
			oh["value"] = ho["value"];
			headers.append(oh);
		}
		if (!headers.isEmpty())
			req["header"] = headers;

		const QString body = requestJson["body"].toString();
		if (!body.isEmpty())
		{
			QJsonObject b;
			b["mode"] = "raw";
			b["raw"]  = body;
			req["body"] = b;
		}

		// (R2) round-trip auth so authenticated requests survive import/export.
		if (requestJson.contains("auth"))
			req["auth"] = requestJson["auth"];

		return req;
	}

	QJsonObject PostmanConverter::postmanToRequestJson(const QJsonObject &requestObj)
	{
		QJsonObject out;
		out["method"] = requestObj["method"].toString("GET");

		const QJsonValue urlv = requestObj["url"];
		if (urlv.isObject())
			out["url"] = urlv.toObject()["raw"].toString();
		else
			out["url"] = urlv.toString();

		QJsonArray headers;
		const QJsonValue hv = requestObj["header"];
		QJsonArray ha = hv.isArray() ? hv.toArray()
		                 : (hv.isObject() ? QJsonArray{ hv.toObject() } : QJsonArray{});
		for (const auto &h : ha)
		{
			const QJsonObject ho = h.toObject();
			QJsonObject oh;
			oh["key"]   = ho["key"];
			oh["value"] = ho["value"];
			headers.append(oh);
		}
		out["headers"] = headers;

		const QJsonValue bv = requestObj["body"];
		if (bv.isObject())
		{
			const QString rawBody = bv.toObject()["raw"].toString();
			if (!rawBody.isEmpty())
				out["body"] = rawBody;
		}

		// (R2) restore auth if present.
		if (requestObj.contains("auth"))
			out["auth"] = requestObj["auth"];

		return out;
	}

	QJsonObject PostmanConverter::itemToPostman(const CollectionItem &item)
	{
		QJsonObject obj;
		obj["name"] = item.name;

		if (item.type == CollectionItemType::Folder)
		{
			QJsonArray children;
			for (const auto &child : item.children)
				children.append(itemToPostman(child));
			obj["item"] = children;
		}
		else
		{
			obj["request"] = requestToPostman(item.requestJson);
		}
		return obj;
	}

	CollectionItem PostmanConverter::postmanToItem(const QJsonObject &obj, bool &ok)
	{
		CollectionItem item;
		item.id   = obj["id"].toString();
		if (item.id.isEmpty())
			item.id = Collection::genId();   // Postman items may lack a stable id
		item.name = obj["name"].toString();

		if (item.name.isEmpty())
		{
			qWarning("PostmanConverter: skipping item with empty name");
			ok = false;
			return item;
		}

		if (obj.contains("item"))   // folder
		{
			item.type = CollectionItemType::Folder;
			const QJsonArray children = obj["item"].toArray();
			item.children.reserve(children.size());
			for (const auto &c : children)
			{
				bool childOk = false;
				CollectionItem child = postmanToItem(c.toObject(), childOk);
				if (childOk)
					item.children.append(child);
			}
		}
		else if (obj.contains("request"))   // request
		{
			item.type = CollectionItemType::Request;
			item.requestJson = postmanToRequestJson(obj["request"].toObject());

			// Validate required fields: method and url must be present
			if (!item.requestJson.contains("method") || !item.requestJson.contains("url"))
			{
				qWarning("PostmanConverter: skipping request item '%s' — missing method or url",
				         qPrintable(item.name));
				ok = false;
				return item;
			}
		}
		else
		{
			ok = false;
			return item;
		}

		ok = true;
		return item;
	}

	QJsonObject PostmanConverter::toPostmanV21(const Collection &c)
	{
		QJsonObject info;
		info["name"] = c.name();
		info["schema"] = "https://schema.getpostman.com/json/collection/v2.1.0/collection.json";

		QJsonArray items;
		for (const auto &child : c.root().children)
			items.append(itemToPostman(child));

		QJsonObject doc;
		doc["info"] = info;
		doc["item"] = items;
		return doc;
	}

	Collection PostmanConverter::fromPostmanV21(const QJsonObject &doc, bool *ok)
	{
		Collection c;
		if (doc.contains("info"))
			c.setName(doc["info"].toObject()["name"].toString());

		const QJsonArray items = doc["item"].toArray();
		bool allOk = true;
		c.root().children.reserve(items.size());
		for (const auto &it : items)
		{
			bool itemOk = false;
			CollectionItem child = postmanToItem(it.toObject(), itemOk);
			if (itemOk)
				c.root().children.append(child);
			else
				allOk = false;
		}

		if (ok)
			*ok = allOk;
		return c;
	}
}
