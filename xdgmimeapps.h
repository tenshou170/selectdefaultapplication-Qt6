#pragma once

#include <QHash>
#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QMultiHash>
#include <QSet>
#include <QString>
#include <QStringList>

/**
 * @brief Manages default application associations per XDG MIME Apps Specification.
 *
 * This class handles both the parsing of mimeapps.list files and the discovery
 * of .desktop files to provide a complete view of MIME associations.
 */
class XdgMimeApps {
public:
	XdgMimeApps();

	/**
	 * @brief Load all mimeapps.list files in XDG precedence order.
	 */
	void loadAllConfigs(bool verbose = false);

	/**
	 * @brief Discover and parse all .desktop files from standard XDG locations.
	 * @param verbose Enable debug logging
	 */
	void loadApplications(bool verbose = false);

	/**
	 * @brief Get the default application for a MIME type.
	 */
	QString getDefaultApp(const QString &mimeType) const;

	/**
	 * @brief Get associated applications for a MIME type.
	 */
	QStringList getAssociatedApps(const QString &mimeType) const;

	/**
	 * @brief Check if a MIME type has an explicit user-set default.
	 */
	bool hasUserDefault(const QString &mimeType) const;

	/**
	 * @brief Set the default application for the given MIME types in the user's mimeapps.list.
	 * 
	 * @param appFile The .desktop file name (e.g. "org.kde.kate.desktop")
	 * @param mimeTypes Set of MIME types to associate
	 */
	void setDefaults(const QString &appFile, const QSet<QString> &mimeTypes);

	/**
	 * @brief Remove the default application association for the given MIME types from the user's mimeapps.list.
	 * 
	 * @param mimeTypes Set of MIME types to remove associations for
	 */
	void removeDefaults(const QSet<QString> &mimeTypes);

	// Data accessors for UI
	const QHash<QString, QHash<QString, QString> > &getApps() const
	{
		return m_apps;
	}
	const QHash<QString, QString> &getApplicationIcons() const
	{
		return m_applicationIcons;
	}
	const QMultiHash<QString, QString> &getChildMimeTypes() const
	{
		return m_childMimeTypes;
	}
	const QSet<QString> &getMimeGroups() const
	{
		return m_mimegroups;
	}

	/**
	 * @brief Utility to normalize MIME type names and handle aliases.
	 */
	QString normalizeMimeType(const QString &name);

	static QStringList getCurrentDesktops();
	QStringList getMimeAppsListPaths() const;

private:
	void parseMimeAppsList(const QString &filePath, bool desktopSpecific, bool verbose);
	void loadDesktopFile(const QString &filePath, bool verbose);

	QStringList m_desktops;
	QHash<QString, QString> m_defaults;
	QMultiHash<QString, QString> m_addedAssociations;
	QMultiHash<QString, QString> m_removedAssociations;
	QSet<QString> m_userDefaults;

	// Application data
	QHash<QString, QHash<QString, QString> > m_apps;
	QHash<QString, QString> m_applicationIcons;
	QMultiHash<QString, QString> m_childMimeTypes;
	QSet<QString> m_mimegroups;

	QMimeDatabase m_mimeDb;
};

Q_DECLARE_LOGGING_CATEGORY(sdaLog);
