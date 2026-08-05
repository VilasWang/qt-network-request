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

#pragma once

#include "collectionmodel.h"
#include "networkrequestglobal.h"

namespace QtNetworkRequest
{
	/// Bidirectional converter between our Collection model and Postman v2.1
	/// collection JSON. The per-request field mapping mirrors the on-disk
	/// single-request schema (method / url / body / headers / auth).
	class NETWORK_EXPORT PostmanConverter
	{
	public:
		/// Collection -> Postman v2.1 collection JSON.
		static QJsonObject toPostmanV21(const Collection &c);
		/// Postman v2.1 collection JSON -> Collection. *ok is set to false on a
		/// structural parse failure (still returns a best-effort Collection).
		static Collection fromPostmanV21(const QJsonObject &doc, bool *ok = nullptr);

	private:
		static QJsonObject itemToPostman(const CollectionItem &item);
		static CollectionItem postmanToItem(const QJsonObject &obj, bool &ok);

		static QJsonObject requestToPostman(const QJsonObject &requestJson);
		static QJsonObject postmanToRequestJson(const QJsonObject &requestObj);
	};
}
