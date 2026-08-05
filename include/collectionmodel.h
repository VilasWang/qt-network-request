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

#pragma once

#include <QString>
#include <QJsonObject>
#include <QList>
#include "networkrequestglobal.h"

namespace QtNetworkRequest
{
	enum class CollectionItemType
	{
		Folder,
		Request
	};

	/// A node in a Collection tree. Folders contain children; Requests carry a
	/// `requestJson` object whose fields match the on-disk single-request schema
	/// (method/url/body/bodyType/rawType/timestamp/params[]/headers[]/auth).
	struct CollectionItem
	{
		CollectionItemType type{ CollectionItemType::Folder };
		QString id;                 // stable id for tree mapping / lookup
		QString name;
		QJsonObject requestJson;    // for Request items
		QList<CollectionItem> children;  // for Folder items
	};

	/// In-memory Collection (folders + requests) with JSON (de)serialization.
	/// Backs both the GUI tree and the Postman v2.1 converter.
	class NETWORK_EXPORT Collection
	{
	public:
		Collection() = default;
		explicit Collection(const QString &name) : m_name(name) {}

		const QString &name() const { return m_name; }
		void setName(const QString &name) { m_name = name; }

		const CollectionItem &root() const { return m_root; }
		CollectionItem &root() { return m_root; }

		/// Load/save the whole collection from/to a JSON file.
		bool load(const QString &path);
		bool save(const QString &path) const;

		/// Mutators return the new item's id (empty string on failure).
		QString addFolder(const QString &parentId, const QString &name);
		QString addRequest(const QString &parentId, const QString &name,
		                   const QJsonObject &requestJson);
		/// Remove the item with the given id anywhere in the tree.
		void removeItem(const QString &id);

		/// Lookup by id (navigates from root). Returns nullptr if not found.
		CollectionItem *findById(const QString &id);
		const CollectionItem *findById(const QString &id) const;

		/// Generate a stable unique id (UUID without braces).
		static QString genId();

		/// (De)serialize a single item (shared by Collection + PostmanConverter).
		static QJsonObject toJson(const CollectionItem &item);
		static CollectionItem fromJson(const QJsonObject &obj, bool *ok = nullptr);

	private:
		CollectionItem *findById(CollectionItem &node, const QString &id);
		const CollectionItem *findById(const CollectionItem &node, const QString &id) const;
		bool removeFrom(CollectionItem &parent, const QString &id);

		QString m_name;
		CollectionItem m_root{ CollectionItemType::Folder, QString(), QStringLiteral("root") };
	};
}
