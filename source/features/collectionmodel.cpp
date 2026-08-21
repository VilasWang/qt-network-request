/*
@Brief:			Qt multi-threaded network request module – request Collection model

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

#include "collectionmodel.h"

#include "qtcompat.h"

#include <QUuid>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

namespace QtNetworkRequest
{
	QString Collection::genId()
	{
		return QtCompat::createUuidString();
	}

	QJsonObject Collection::toJson(const CollectionItem &item)
	{
		QJsonObject obj;
		obj["type"] = (item.type == CollectionItemType::Folder) ? "folder" : "request";
		obj["id"]   = item.id;
		obj["name"] = item.name;

		if (item.type == CollectionItemType::Folder)
		{
			QJsonArray children;
			for (const auto &child : item.children)
				children.append(toJson(child));
			obj["children"] = children;
		}
		else
		{
			obj["request"] = item.requestJson;
		}
		return obj;
	}

	CollectionItem Collection::fromJson(const QJsonObject &obj, bool *ok)
	{
		CollectionItem item;
		const QString type = obj["type"].toString();
		item.id   = obj["id"].toString();
		item.name = obj["name"].toString();

		if (type == "folder")
		{
			item.type = CollectionItemType::Folder;
			const QJsonArray children = obj["children"].toArray();
			QtCompat::reserveList(item.children, children.size());
			for (const auto &c : children)
			{
				bool childOk = false;
				CollectionItem child = fromJson(c.toObject(), &childOk);
				if (childOk)
					item.children.append(child);
			}
		}
		else if (type == "request")
		{
			item.type = CollectionItemType::Request;
			item.requestJson = obj["request"].toObject();
		}
		else
		{
			// Unknown type -> treat as request if it has a "request" object.
			if (obj.contains("request"))
			{
				item.type = CollectionItemType::Request;
				item.requestJson = obj["request"].toObject();
			}
			else if (ok)
			{
				*ok = false;
				return item;
			}
		}

		if (ok)
			*ok = true;
		return item;
	}

	bool Collection::load(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly))
			return false;

		const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
		if (doc.isNull() || !doc.isObject())
			return false;

		const QJsonObject root = doc.object();
		m_name = root["name"].toString();

		const QJsonArray items = root["items"].toArray();
		m_root.children.clear();
		QtCompat::reserveList(m_root.children, items.size());
		for (const auto &it : items)
		{
			bool itemOk = false;
			CollectionItem child = fromJson(it.toObject(), &itemOk);
			if (itemOk)
				m_root.children.append(child);
		}
		return true;
	}

	bool Collection::save(const QString &path) const
	{
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly))
			return false;

		QJsonObject root;
		root["name"] = m_name;
		QJsonArray items;
		for (const auto &child : m_root.children)
			items.append(toJson(child));
		root["items"] = items;

		file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
		return true;
	}

	QString Collection::addFolder(const QString &parentId, const QString &name)
	{
		CollectionItem *parent = parentId.isEmpty() ? &m_root : findById(parentId);
		if (!parent)
			return QString();

		CollectionItem folder;
		folder.type = CollectionItemType::Folder;
		folder.id   = genId();
		folder.name = name;
		parent->children.append(folder);
		return folder.id;
	}

	QString Collection::addRequest(const QString &parentId, const QString &name,
	                               const QJsonObject &requestJson)
	{
		CollectionItem *parent = parentId.isEmpty() ? &m_root : findById(parentId);
		if (!parent)
			return QString();

		CollectionItem request;
		request.type       = CollectionItemType::Request;
		request.id         = genId();
		request.name       = name;
		request.requestJson = requestJson;
		parent->children.append(request);
		return request.id;
	}

	void Collection::removeItem(const QString &id)
	{
		removeFrom(m_root, id);
	}

	bool Collection::removeFrom(CollectionItem &parent, const QString &id)
	{
		for (int i = 0; i < parent.children.size(); ++i)
		{
			CollectionItem &child = parent.children[i];
			if (child.id == id)
			{
				parent.children.removeAt(i);
				return true;
			}
			if (removeFrom(child, id))
				return true;
		}
		return false;
	}

	CollectionItem *Collection::findById(const QString &id)
	{
		return findById(m_root, id);
	}

	const CollectionItem *Collection::findById(const QString &id) const
	{
		return findById(m_root, id);
	}

	CollectionItem *Collection::findById(CollectionItem &node, const QString &id)
	{
		if (node.id == id)
			return &node;
		for (auto &child : node.children)
		{
			if (CollectionItem *found = findById(child, id))
				return found;
		}
		return nullptr;
	}

	const CollectionItem *Collection::findById(const CollectionItem &node, const QString &id) const
	{
		if (node.id == id)
			return &node;
		for (const auto &child : node.children)
		{
			if (const CollectionItem *found = findById(child, id))
				return found;
		}
		return nullptr;
	}
}
