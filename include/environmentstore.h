/*
@Brief:			Qt multi-threaded network request module – named environment store

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
#include <QStringList>
#include <QMap>
#include "networkrequestglobal.h"

namespace QtNetworkRequest
{
	/// Manages multiple named environments (dev/staging/prod) with key-value
	/// variable maps. One JSON file on disk: { "active": "dev", "environments": [...] }.
	/// The active environment's variables are feed into RequestContext::environment
	/// so the library's substitution pipeline replaces {{host}}, {{apiKey}}, etc.
	class NETWORK_EXPORT EnvironmentStore
	{
	public:
		EnvironmentStore() = default;

		/// Load environments from the JSON file at the given path.
		/// If the file doesn't exist the store is left empty (no error).
		bool load(const QString &path);

		/// Persist all environments + active name to disk.
		bool save(const QString &path) const;

		/// Names of all registered environments (order from JSON).
		QStringList environmentNames() const;

		/// Which environment is currently active.
		/// Returns an empty string when "No Environment" is selected.
		const QString &activeName() const { return m_activeName; }

		/// Switch the active environment. An empty name means "No Environment"
		/// (activeVariables() returns an empty map).
		void setActiveName(const QString &name);

		/// Return the variables of the active environment (or empty map).
		QMap<QString, QString> activeVariables() const;

		/// Insert or update an environment with the given name and variables.
		void upsert(const QString &name, const QMap<QString, QString> &vars);

		/// Remove an environment by name. If it was the active one the active
		/// name is cleared.
		void remove(const QString &name);

	private:
		struct EnvEntry
		{
			QString name;
			QMap<QString, QString> variables;
		};

		QString m_activeName;
		QList<EnvEntry> m_environments;
	};
}
