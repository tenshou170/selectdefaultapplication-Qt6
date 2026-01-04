#include "xdgmimeapps.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QStandardPaths>
#include <QMimeType>
#include <QTextStream>
#include <QString>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(sdaLog, "sda.log")

XdgMimeApps::XdgMimeApps()
{
	m_desktops = getCurrentDesktops();
}

QStringList XdgMimeApps::getCurrentDesktops()
{
	QStringList desktops;
	const QString xdgCurrentDesktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
	if (!xdgCurrentDesktop.isEmpty()) {
		const QStringList parts = xdgCurrentDesktop.split(':', Qt::SkipEmptyParts);
		for (const QString &part : parts) {
			desktops.append(part.toLower());
		}
	}
	return desktops;
}

QStringList XdgMimeApps::getMimeAppsListPaths() const
{
	QStringList paths;

	// XDG_CONFIG_HOME (defaults to ~/.config)
	const QString configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);

	// XDG_CONFIG_DIRS (defaults to /etc/xdg)
	QStringList configDirs = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation);
	// Remove configHome from configDirs to avoid duplicates
	configDirs.removeOne(configHome);

	// XDG_DATA_HOME (defaults to ~/.local/share)
	const QString dataHome = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);

	// XDG_DATA_DIRS (defaults to /usr/local/share:/usr/share)
	QStringList dataDirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
	dataDirs.removeOne(dataHome);

	// Build paths in precedence order (highest first)
	// 1. Desktop-specific in config home
	for (const QString &desktop : m_desktops) {
		paths.append(QDir(configHome).absoluteFilePath(desktop + "-mimeapps.list"));
	}
	// 2. Generic in config home
	paths.append(QDir(configHome).absoluteFilePath("mimeapps.list"));

	// 3. Desktop-specific in config dirs
	for (const QString &configDir : configDirs) {
		for (const QString &desktop : m_desktops) {
			paths.append(QDir(configDir).absoluteFilePath(desktop + "-mimeapps.list"));
		}
		paths.append(QDir(configDir).absoluteFilePath("mimeapps.list"));
	}

	// 4. Desktop-specific in data home applications
	const QString dataHomeApps = QDir(dataHome).absoluteFilePath("applications");
	for (const QString &desktop : m_desktops) {
		paths.append(QDir(dataHomeApps).absoluteFilePath(desktop + "-mimeapps.list"));
	}
	// 5. Generic in data home applications
	paths.append(QDir(dataHomeApps).absoluteFilePath("mimeapps.list"));

	// 6. Data dirs applications
	for (const QString &dataDir : dataDirs) {
		const QString dataDirApps = QDir(dataDir).absoluteFilePath("applications");
		for (const QString &desktop : m_desktops) {
			paths.append(QDir(dataDirApps).absoluteFilePath(desktop + "-mimeapps.list"));
		}
		paths.append(QDir(dataDirApps).absoluteFilePath("mimeapps.list"));
	}

	return paths;
}

void XdgMimeApps::loadAllConfigs(bool verbose)
{
	m_defaults.clear();
	m_addedAssociations.clear();
	m_removedAssociations.clear();
	m_userDefaults.clear();

	const QString configHome = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
	const QStringList paths = getMimeAppsListPaths();

	for (const QString &path : paths) {
		QFileInfo fileInfo(path);
		if (!fileInfo.exists()) {
			continue;
		}

		// Desktop-specific files can only set defaults, not add/remove associations
		const bool isDesktopSpecific = fileInfo.fileName().contains("-mimeapps.list");
		const bool isUserConfig = path.startsWith(configHome);

		parseMimeAppsList(path, isDesktopSpecific, verbose);

		// Track user-level defaults for UI indication
		if (isUserConfig && !isDesktopSpecific) {
			// Re-parse to track which are user defaults
			QFile file(path);
			if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
				bool inDefaultApps = false;
				while (!file.atEnd()) {
					const QString line = QString::fromUtf8(file.readLine()).trimmed();
					if (line.startsWith('[')) {
						inDefaultApps = (line == "[Default Applications]");
						continue;
					}
					if (inDefaultApps && line.contains('=')) {
						const QString mimeType = line.section('=', 0, 0).trimmed();
						m_userDefaults.insert(mimeType);
					}
				}
			}
		}
	}
}

void XdgMimeApps::parseMimeAppsList(const QString &filePath, bool desktopSpecific, bool verbose)
{
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		if (verbose) {
			qCDebug(sdaLog) << "XdgMimeApps: Could not open" << filePath;
		}
		return;
	}

	if (verbose) {
		qCDebug(sdaLog) << "XdgMimeApps: Parsing" << filePath;
	}

	enum Section { None, DefaultApplications, AddedAssociations, RemovedAssociations };
	Section currentSection = None;

	while (!file.atEnd()) {
		const QString line = QString::fromUtf8(file.readLine()).trimmed();

		if (line.isEmpty() || line.startsWith('#')) {
			continue;
		}

		if (line.startsWith('[')) {
			if (line == "[Default Applications]") {
				currentSection = DefaultApplications;
			} else if (line == "[Added Associations]") {
				currentSection = AddedAssociations;
			} else if (line == "[Removed Associations]") {
				currentSection = RemovedAssociations;
			} else {
				currentSection = None;
			}
			continue;
		}

		if (!line.contains('=')) {
			continue;
		}

		const QString mimeType = line.section('=', 0, 0).trimmed();
		const QString value = line.section('=', 1, -1).trimmed();
		const QStringList desktopIds = value.split(';', Qt::SkipEmptyParts);

		switch (currentSection) {
		case DefaultApplications:
			// First entry wins - only insert if not already present
			if (!m_defaults.contains(mimeType)) {
				// Take the first valid desktop ID from the list
				for (const QString &desktopId : desktopIds) {
					const QString trimmedId = desktopId.trimmed();
					if (!trimmedId.isEmpty()) {
						m_defaults.insert(mimeType, trimmedId);
						break;
					}
				}
			}
			break;

		case AddedAssociations:
			// Desktop-specific files cannot add associations per spec
			if (!desktopSpecific) {
				for (const QString &desktopId : desktopIds) {
					const QString trimmedId = desktopId.trimmed();
					if (!trimmedId.isEmpty()) {
						m_addedAssociations.insert(mimeType, trimmedId);
					}
				}
			}
			break;

		case RemovedAssociations:
			// Desktop-specific files cannot remove associations per spec
			if (!desktopSpecific) {
				for (const QString &desktopId : desktopIds) {
					const QString trimmedId = desktopId.trimmed();
					if (!trimmedId.isEmpty()) {
						m_removedAssociations.insert(mimeType, trimmedId);
					}
				}
			}
			break;

		case None:
			break;
		}
	}
}

QString XdgMimeApps::getDefaultApp(const QString &mimeType) const
{
	return m_defaults.value(mimeType, QString());
}

QStringList XdgMimeApps::getAssociatedApps(const QString &mimeType) const
{
	QStringList result;
	const QStringList removed = m_removedAssociations.values(mimeType);

	// Add associations that aren't removed
	const QStringList added = m_addedAssociations.values(mimeType);
	for (const QString &app : added) {
		if (!removed.contains(app) && !result.contains(app)) {
			result.append(app);
		}
	}

	return result;
}

bool XdgMimeApps::hasUserDefault(const QString &mimeType) const
{
	return m_userDefaults.contains(mimeType);
}

void XdgMimeApps::loadApplications(bool verbose)
{
	m_apps.clear();
	m_applicationIcons.clear();
	m_childMimeTypes.clear();
	m_mimegroups.clear();

	const QStringList appDirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
	for (const QString &dirPath : appDirs) {
		if (verbose) {
			qCDebug(sdaLog) << "XdgMimeApps: Loading applications from" << dirPath;
		}
		QDir applicationsDir(dirPath);
		const QFileInfoList files = applicationsDir.entryInfoList({ "*.desktop" }, QDir::Files);
		for (const QFileInfo &file : files) {
			loadDesktopFile(file.absoluteFilePath(), verbose);
		}
	}
}

void XdgMimeApps::loadDesktopFile(const QString &filePath, bool verbose)
{
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		if (verbose) {
			qCWarning(sdaLog) << "XdgMimeApps: Failed to open" << filePath;
		}
		return;
	}

	QFileInfo fileInfo(filePath);
	const QString appFile = fileInfo.fileName();
	QString appName;
	QString appIcon;
	QStringList mimetypes;

	QTextStream in(&file);
	bool inDesktopEntry = false;

	while (!in.atEnd()) {
		QString line = in.readLine().trimmed();
		if (line.isEmpty() || line.startsWith('#'))
			continue;

		if (line.startsWith('[')) {
			inDesktopEntry = (line == "[Desktop Entry]");
			if (!inDesktopEntry && !appName.isEmpty())
				break; // Done with Desktop Entry section
			continue;
		}

		if (!inDesktopEntry)
			continue;

		const int eqPos = line.indexOf('=');
		if (eqPos <= 0)
			continue;

		const QStringView key = QStringView(line).left(eqPos).trimmed();
		const QStringView value = QStringView(line).mid(eqPos + 1).trimmed();

		if (key == "Name") {
			appName = value.toString();
		} else if (key == "MimeType") {
			mimetypes = value.toString().split(';', Qt::SkipEmptyParts);
		} else if (key == "Icon") {
			appIcon = value.toString();
		}
	}

	if (appName.isEmpty()) {
		appName = fileInfo.baseName();
	}

	if (!appIcon.isEmpty() && m_applicationIcons[appName].isEmpty()) {
		m_applicationIcons[appName] = appIcon;
	}

	if (mimetypes.isEmpty())
		return;

	for (const QString &readMimeName : mimetypes) {
		const QString mimetypeName = normalizeMimeType(readMimeName);
		if (mimetypeName.isEmpty())
			continue;

		const QMimeType mimetype = m_mimeDb.mimeTypeForName(mimetypeName);
		const QStringList parents = mimetype.parentMimeTypes();
		for (const QString &parent : parents) {
			if (parent != "application/octet-stream") {
				m_childMimeTypes.insert(parent, mimetypeName);
			}
		}

		if (mimetypeName.contains('/')) {
			m_mimegroups.insert(mimetypeName.section('/', 0, 0));
		}

		// Higher priority directories are scanned first
		if (!m_apps[appName].contains(mimetypeName)) {
			m_apps[appName][mimetypeName] = appFile;
		}
	}
}

QString XdgMimeApps::normalizeMimeType(const QString &name)
{
	static const QString X_SCHEME_HANDLER = "x-scheme-handler/";

	if (name.startsWith(X_SCHEME_HANDLER)) {
		return name;
	}

	QMimeType mimetype = m_mimeDb.mimeTypeForName(name);
	if (!mimetype.isValid()) {
		return QString();
	}

	QString mimetypeName = mimetype.name();
	// Workaround for QTBUG-99509
	if (mimetypeName == "application/pkcs12") {
		mimetypeName = "application/x-pkcs12";
	}
	return mimetypeName;
}

void XdgMimeApps::setDefaults(const QString &appFile, const QSet<QString> &mimeTypes)
{
	if (mimeTypes.isEmpty()) {
		return;
	}

	const QString filePath =
		QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).absoluteFilePath("mimeapps.list");
	QFile file(filePath);

	// Read in existing mimeapps.list, skipping the lines for the mimetypes we're updating
	QList<QByteArray> existingContent;
	QList<QByteArray> existingAssociations;

	if (file.open(QIODevice::ReadOnly)) {
		bool inCorrectGroup = false;
		while (!file.atEnd()) {
			const QByteArray line = file.readLine().trimmed();

			if (line.isEmpty()) {
				continue;
			}

			if (line.startsWith('[')) {
				inCorrectGroup = (line == "[Default Applications]");
				if (!inCorrectGroup) {
					existingContent.append(line);
				}
				continue;
			}

			if (!inCorrectGroup) {
				existingContent.append(line);
				continue;
			}

			if (!line.contains('=')) {
				existingAssociations.append(line);
				continue;
			}

			const QString mimetype =
				normalizeMimeType(QString::fromUtf8(line.split('=').first().trimmed()));
			// If we aren't setting this mimetype, leave any entry
			if (!mimeTypes.contains(mimetype)) {
				existingAssociations.append(line);
				continue;
			}
		}
		file.close();
	}

	// Write the file
	if (!file.open(QIODevice::WriteOnly)) {
		qWarning() << "XdgMimeApps: Failed to write to" << filePath << file.errorString();
		return;
	}

	for (const QByteArray &line : existingContent) {
		file.write(line + '\n');
	}
	file.write("\n[Default Applications]\n");
	for (const QByteArray &line : existingAssociations) {
		file.write(line + '\n');
	}

	QStringList selectedMimetypes = mimeTypes.values();
	selectedMimetypes.sort();
	for (const QString &mimetype : selectedMimetypes) {
		file.write(QString(mimetype + '=' + appFile + '\n').toUtf8());
		qCDebug(sdaLog) << "XdgMimeApps: Writing setting:" << mimetype << "=" << appFile;
	}
	file.close();
}

void XdgMimeApps::removeDefaults(const QSet<QString> &mimeTypes)
{
	if (mimeTypes.isEmpty()) {
		return;
	}

	const QString filePath =
		QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).absoluteFilePath("mimeapps.list");
	QFile file(filePath);

	QList<QByteArray> existingContent;

	if (file.open(QIODevice::ReadOnly)) {
		bool inDefaultSection = false;
		bool inAddedSection = false;
		while (!file.atEnd()) {
			QByteArray rawLine = file.readLine().trimmed();
			if (rawLine.isEmpty())
				continue;

			QString line = QString::fromUtf8(rawLine);

			if (line.startsWith('[')) {
				inDefaultSection = (line == "[Default Applications]");
				inAddedSection = (line == "[Added Associations]");
				existingContent.append(rawLine);
				continue;
			}

			// If we are in neither section, keep the line
			if (!inDefaultSection && !inAddedSection) {
				existingContent.append(rawLine);
				continue;
			}

			if (!line.contains('=')) {
				existingContent.append(rawLine);
				continue;
			}

			const QString mimetype = normalizeMimeType(line.section('=', 0, 0).trimmed());
			if (mimeTypes.contains(mimetype)) {
				if (mimeTypes.contains(mimetype)) {
					// Skip this line (remove it)
					qCDebug(sdaLog) << "XdgMimeApps: Removing association for" << mimetype;
					continue;
				}
			}
			existingContent.append(rawLine);
		}
		file.close();
	}

	if (!file.open(QIODevice::WriteOnly)) {
		qWarning() << "XdgMimeApps: Failed to write to" << filePath << file.errorString();
		return;
	}

	for (const QByteArray &line : existingContent) {
		file.write(line + '\n');
	}
	file.close();
}
