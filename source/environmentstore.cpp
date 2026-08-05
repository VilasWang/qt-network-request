/*
@Brief:			Qt multi-threaded network request module – EnvironmentStore implementation

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

#include "environmentstore.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace QtNetworkRequest
{
	bool EnvironmentStore::load(const QString &path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly))
			return false;

		const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
		if (!doc.isObject())
			return false;

		const QJsonObject root = doc.object();
		m_activeName = root["active"].toString();
		m_environments.clear();

		const QJsonArray envs = root["environments"].toArray();
		m_environments.reserve(envs.size());
		for (const auto &ev : envs)
		{
			const QJsonObject eo = ev.toObject();
			EnvEntry entry;
			entry.name = eo["name"].toString();

			const QJsonObject vars = eo["variables"].toObject();
			for (auto it = vars.begin(); it != vars.end(); ++it)
				entry.variables.insert(it.key(), it.value().toString());

			m_environments.append(entry);
		}
		return true;
	}

	bool EnvironmentStore::save(const QString &path) const
	{
		QJsonObject root;
		root["active"] = m_activeName;

		QJsonArray envs;
		for (const auto &entry : m_environments)
		{
			QJsonObject eo;
			eo["name"] = entry.name;

			QJsonObject vars;
			for (auto it = entry.variables.begin(); it != entry.variables.end(); ++it)
				vars.insert(it.key(), it.value());
			eo["variables"] = vars;

			envs.append(eo);
		}
		root["environments"] = envs;

		QFile file(path);
		if (!file.open(QIODevice::WriteOnly))
			return false;

		file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
		return true;
	}

	QStringList EnvironmentStore::environmentNames() const
	{
		QStringList names;
		names.reserve(m_environments.size());
		for (const auto &e : m_environments)
			names.append(e.name);
		return names;
	}

	void EnvironmentStore::setActiveName(const QString &name)
	{
		m_activeName = name;
	}

	QMap<QString, QString> EnvironmentStore::activeVariables() const
	{
		for (const auto &e : m_environments)
		{
			if (e.name == m_activeName)
				return e.variables;
		}
		return {};
	}

	void EnvironmentStore::upsert(const QString &name, const QMap<QString, QString> &vars)
	{
		for (auto &e : m_environments)
		{
			if (e.name == name)
			{
				e.variables = vars;
				return;
			}
		}
		m_environments.append({ name, vars });
	}

	void EnvironmentStore::remove(const QString &name)
	{
		for (int i = 0; i < m_environments.size(); ++i)
		{
			if (m_environments[i].name == name)
			{
				m_environments.removeAt(i);
				if (m_activeName == name)
					m_activeName.clear();
				return;
			}
		}
	}
}
