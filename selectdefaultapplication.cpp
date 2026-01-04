#include "selectdefaultapplication.h"
#include <QLoggingCategory>
#include <QCheckBox>
#include <QDialog>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTreeWidget>

SelectDefaultApplication::SelectDefaultApplication(QWidget *parent, bool isVerbose)
	: QWidget(parent), isVerbose(isVerbose)
{
	m_xdgMimeApps.loadApplications(isVerbose);
	m_xdgMimeApps.loadAllConfigs(isVerbose);

	readCurrentDefaultMimetypes();

	// Now that m_apps is populated, sync human-readable names with XDG defaults
	readCurrentDefaultMimetypes();

	// Preload icons up front, so it doesn't get sluggish when selecting applications
	// supporting a lot
	const QIcon unknownIcon = QIcon::fromTheme("unknown");

	// TODO: check if QT_QPA_PLATFORMTHEME is set to plasma or sandsmark,
	// if so just use the functioning QIcon::fromTheme()
	// We do this manually because non-Plasma-platforms icon loading is extremely
	// slow (I blame GTK and its crappy icon cache)
	for (const QString &searchPath : (QIcon::themeSearchPaths() + QIcon::fallbackSearchPaths())) {
		loadIcons(searchPath + QIcon::themeName());
		loadIcons(searchPath);
	}

	// Set m_mimeTypeIcons[mimetypeName] to an appropriate icon
	const auto &apps = m_xdgMimeApps.getApps();
	const auto &appIcons = m_xdgMimeApps.getApplicationIcons();
	for (const QHash<QString, QString> &application_associations : apps.values()) {
		for (const QString &mimetypeName : application_associations.keys()) {
			if (m_mimeTypeIcons.contains(mimetypeName)) {
				continue;
			}
			// Here we actually want to use the real mimetype, because we need to access its iconName
			const QMimeType mimetype = m_mimeDb.mimeTypeForName(mimetypeName);

			QString iconName = mimetype.iconName();
			QIcon icon(m_iconPaths.value(iconName));
			if (!icon.isNull()) {
				m_mimeTypeIcons[mimetypeName] = icon;
				continue;
			}
			icon = QIcon(m_iconPaths.value(mimetype.genericIconName()));
			if (!icon.isNull()) {
				m_mimeTypeIcons[mimetypeName] = icon;
				continue;
			}
			int split = iconName.lastIndexOf('+');
			if (split != -1) {
				iconName.truncate(split);
				icon = QIcon(m_iconPaths.value(iconName));
				if (!icon.isNull()) {
					m_mimeTypeIcons[mimetypeName] = icon;
					continue;
				}
			}
			split = iconName.lastIndexOf('-');
			if (split != -1) {
				iconName.truncate(split);
				icon = QIcon(m_iconPaths.value(iconName));
				if (!icon.isNull()) {
					m_mimeTypeIcons[mimetypeName] = icon;
					continue;
				}
			}
			icon = QIcon(m_iconPaths.value(mimetype.genericIconName()));
			if (!icon.isNull()) {
				m_mimeTypeIcons[mimetypeName] = icon;
				continue;
			}

			m_mimeTypeIcons[mimetypeName] = unknownIcon;
		}
	}

	// The rest of this constructor sets up the GUI
	// Left section
	m_applicationList = new QListWidget;
	m_applicationList->setSelectionMode(QAbstractItemView::SingleSelection);
	populateApplicationList("");

	m_searchBox = new QLineEdit;
	m_searchBox->setPlaceholderText(tr("Search for Application"));

	m_groupChooser = new QPushButton;
	m_groupChooser->setText(tr("All"));

	m_mimegroupMenu = new QMenu(m_groupChooser);
	m_mimegroupMenu->addAction(tr("All"));
	QStringList sorted_mimegroups = m_xdgMimeApps.getMimeGroups().values();
	std::sort(sorted_mimegroups.begin(), sorted_mimegroups.end());
	for (const QString &mimegroup : sorted_mimegroups) {
		m_mimegroupMenu->addAction(mimegroup);
	}
	m_groupChooser->setMenu(m_mimegroupMenu);

	// Help button
	m_infoButton = new QToolButton();
	m_infoButton->setText("?");
	m_infoButton->setFixedSize(24, 24);

	QHBoxLayout *filterHolder = new QHBoxLayout;
	filterHolder->addWidget(m_searchBox);
	filterHolder->addWidget(m_groupChooser);
	filterHolder->addWidget(m_infoButton);

	QVBoxLayout *leftLayout = new QVBoxLayout;
	leftLayout->addLayout(filterHolder);
	leftLayout->addWidget(m_applicationList);

	// Middle section
	m_middleBanner = new QLabel(tr("Select an application to see its defaults."));
	m_middleBanner->setWordWrap(true);
	m_middleBanner->setMinimumHeight(40);
	m_middleBanner->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	m_mimetypeList = new QListWidget;
	m_mimetypeList->setUniformItemSizes(true);
	m_mimetypeList->setSelectionMode(QAbstractItemView::ExtendedSelection);

	m_setDefaultButton = new QPushButton(tr("Add association(s)"));
	m_setDefaultButton->setEnabled(false);

	QVBoxLayout *middleLayout = new QVBoxLayout;
	middleLayout->addWidget(m_middleBanner);
	middleLayout->addWidget(m_mimetypeList);
	middleLayout->addWidget(m_setDefaultButton);

	// Right section
	m_rightBanner = new QLabel("");
	m_rightBanner->setWordWrap(true);
	m_rightBanner->setMinimumHeight(40);
	m_rightBanner->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	m_currentDefaultApps = new QListWidget;
	m_currentDefaultApps->setSelectionMode(QAbstractItemView::SingleSelection);

	m_removeDefaultButton = new QPushButton(tr("Remove association(s)"));
	m_removeDefaultButton->setEnabled(false);

	QVBoxLayout *rightLayout = new QVBoxLayout;
	rightLayout->addWidget(m_rightBanner);
	rightLayout->addWidget(m_currentDefaultApps);
	rightLayout->addWidget(m_removeDefaultButton);

	// Main layout and connections
	QHBoxLayout *mainLayout = new QHBoxLayout;
	setLayout(mainLayout);
	mainLayout->addLayout(leftLayout, 1);
	mainLayout->addLayout(middleLayout, 1);
	mainLayout->addLayout(rightLayout, 1);

	connect(m_applicationList, &QListWidget::itemSelectionChanged, this,
		&SelectDefaultApplication::onApplicationSelected);
	connect(m_mimetypeList, &QListWidget::itemActivated, this, &SelectDefaultApplication::enableSetDefaultButton);
	connect(m_currentDefaultApps, &QListWidget::itemSelectionChanged, this,
		&SelectDefaultApplication::enableSetDefaultButton);
	connect(m_setDefaultButton, &QPushButton::clicked, this, &SelectDefaultApplication::onSetDefaultClicked);
	connect(m_removeDefaultButton, &QPushButton::clicked, this, &SelectDefaultApplication::onRemoveDefaultClicked);
	connect(m_infoButton, &QToolButton::clicked, this, &SelectDefaultApplication::showHelp);
	connect(m_searchBox, &QLineEdit::textEdited, this, &SelectDefaultApplication::populateApplicationList);
	connect(m_mimegroupMenu, &QMenu::triggered, this, &SelectDefaultApplication::constrictGroup);

	// Set a reasonable default window size
	resize(1000, 600);
}

SelectDefaultApplication::~SelectDefaultApplication()
{
}

/**
 * Populates the middle and right side of the screen.
 * Selects all the mimetypes that application can natively support for the middle, and all currently selected for right
 * Filters mimetypes based on if they start with m_filterMimegroup
 * Extra function is needed for Qt slots, which is in turn needed to stop the screen from flashing, which is unfortunate
 */
void SelectDefaultApplication::onApplicationSelected()
{
	onApplicationSelectedLogic(true);
}
void SelectDefaultApplication::onApplicationSelectedLogic(bool allowEnabled)
{
	m_setDefaultButton->setEnabled(false);
	m_mimetypeList->clear();

	QList<QListWidgetItem *> selectedItems = m_applicationList->selectedItems();
	if (selectedItems.count() != 1) {
		return;
	}

	const QListWidgetItem *item = selectedItems.first();
	const QString appName = item->data(0).toString();

	// Set banners and right widget
	m_middleBanner->setText(appName + tr(" can open:"));
	m_rightBanner->setText(appName + tr(" currently opens:"));
	m_currentDefaultApps->clear();

	QStringList currentMimes = m_defaultApps.keys(appName);
	qCDebug(sdaLog) << "SelectDefaultApplication: Application" << appName << "currently opens"
			<< currentMimes.count() << "file types";

	for (const QString &mimetype : currentMimes) {
		addToMimetypeList(m_currentDefaultApps, mimetype, false);
	}

	const auto &apps = m_xdgMimeApps.getApps();
	const auto &childMimeTypes = m_xdgMimeApps.getChildMimeTypes();
	const QHash<QString, QString> &officiallySupported = apps.value(appName);

	// E. g. kwrite and kate only indicate support for "text/plain", but they're nice for things like C source files.
	QSet<QString> impliedSupported;
	for (const QString &mimetype : officiallySupported.keys()) {
		for (const QString &child : childMimeTypes.values(mimetype)) {
			// Ensure that the officially supported keys don't contain this value
			if (!officiallySupported.contains(child)) {
				impliedSupported.insert(child);
			}
		}
	}

	for (const QString &mimetype : officiallySupported.keys()) {
		if (mimetype.startsWith(m_filterMimegroup)) {
			addToMimetypeList(m_mimetypeList, mimetype, true);
		}
	}
	for (const QString &mimetype : impliedSupported) {
		if (mimetype.startsWith(m_filterMimegroup)) {
			addToMimetypeList(m_mimetypeList, mimetype, false);
		}
	}

	m_setDefaultButton->setEnabled(allowEnabled && m_mimetypeList->count() > 0);
	m_removeDefaultButton->setEnabled(false);
}
void SelectDefaultApplication::addToMimetypeList(QListWidget *list, const QString &mimetypeName, const bool selected)
{
	QString description = mimetypeDescription(mimetypeName);
	QListWidgetItem *item = new QListWidgetItem(description);
	item->setData(Qt::UserRole, mimetypeName);
	item->setIcon(m_mimeTypeIcons[mimetypeName]);
	list->addItem(item);
	item->setSelected(selected);
}

void SelectDefaultApplication::onSetDefaultClicked()
{
	QList<QListWidgetItem *> selectedItems = m_applicationList->selectedItems();
	if (selectedItems.count() != 1) {
		return;
	}

	const QListWidgetItem *item = selectedItems.first();

	const QString application = item->data(0).toString();
	if (application.isEmpty()) {
		return;
	}

	QSet<QString> unselected;
	QSet<QString> selected;
	for (int i = 0; i < m_mimetypeList->count(); i++) {
		QListWidgetItem *item = m_mimetypeList->item(i);
		const QString name = item->data(Qt::UserRole).toString();
		if (item->isSelected()) {
			selected.insert(name);
		} else {
			unselected.insert(name);
		}
	}

	setDefault(application, selected);
}

// chunk 1 removed
// rest of function removed

// Removes values from mimetypes if warnings exist and the user requests to do a non-destructive change
void SelectDefaultApplication::setDefault(const QString &appName, QSet<QString> &mimetypes)
{
	const QString filePath =
		QDir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)).absoluteFilePath("mimeapps.list");
	QFile file(filePath);

	// Read in existing mimeapps.list, skipping the lines for the mimetypes we're updating
	QList<QByteArray> existingContent;
	QList<QByteArray> existingAssociations;
	QHash<QString, QString> warnings;
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

			const QString mimetype = m_xdgMimeApps.normalizeMimeType(line.split('=').first().trimmed());
			// If we aren't setting this mimetype, leave any entry, even others that this application owns
			if (!mimetypes.contains(mimetype)) {
				existingAssociations.append(line);
				continue;
			}

			// Ensure that if a mimetype is selected and is set as default for a different application, we warn about it
			const QString handlingAppFile = line.split('=')[1].trimmed();
			const auto &apps = m_xdgMimeApps.getApps();
			const QString appFile = apps[appName].value(mimetype);
			if (appFile != handlingAppFile && apps[appName].contains(mimetype)) {
				warnings[mimetype] = handlingAppFile;
			}
		}

		file.close();
	} else {
		qWarning() << "Unable to open file for reading" << file.errorString();
		// TODO If we can't open the file for reading, we better stop before opening for writing and deleting it
	}

	// Display warnings and get user confirmation that we should proceed
	if (!warnings.isEmpty()) {
		QSet<QString> mimesToOverwrite = getGranularOverwriteConfirmation(warnings, appName);
		if (mimesToOverwrite.isEmpty()) {
			return; // User canceled
		}

		// Remove mimes that user chose NOT to overwrite
		for (const QString &warningType : warnings.keys()) {
			if (!mimesToOverwrite.contains(warningType)) {
				// Keep the existing association
				const QString warning = warningType + '=' + warnings[warningType];
				existingAssociations.append(warning.toUtf8());
				// Remove from values to set
				mimetypes.remove(warningType);
			}
		}
	}

	// Write the file
	QHash<QString, QSet<QString> > fileToMimes;
	const auto &apps = m_xdgMimeApps.getApps();
	for (const QString &mime : mimetypes) {
		QString file = apps[appName].value(mime);
		if (!file.isEmpty()) {
			fileToMimes[file].insert(mime);
		}
	}

	for (auto it = fileToMimes.begin(); it != fileToMimes.end(); ++it) {
		m_xdgMimeApps.setDefaults(it.key(), it.value());
	}

	// Refresh everything from disk to ensure UI is in sync
	readCurrentDefaultMimetypes();
	// Redraw and make the button unclickable so there is always user feedback
	onApplicationSelectedLogic(false);
}

void SelectDefaultApplication::readCurrentDefaultMimetypes()
{
	qCDebug(sdaLog) << "SelectDefaultApplication: Refreshing current default mimetypes...";
	// Load all mimeapps.list files in XDG precedence order
	m_xdgMimeApps.loadAllConfigs();

	// Sync human-readable app names with their desktop file defaults
	m_defaultApps.clear();

	const auto &apps = m_xdgMimeApps.getApps();
	if (apps.isEmpty()) {
		qCDebug(sdaLog)
			<< "SelectDefaultApplication: Applications not loaded yet, skipping human-readable name sync";
		return;
	}

	int syncCount = 0;
	// We iterate over all applications and their supported mimetypes
	// If the application's desktop file matches the XDG default for that mimetype,
	// record it in m_defaultApps so the UI shows it as "currently opens".
	for (auto it = apps.begin(); it != apps.end(); ++it) {
		const QString &appName = it.key();
		const QHash<QString, QString> &appMimetypes = it.value();
		for (auto mit = appMimetypes.begin(); mit != appMimetypes.end(); ++mit) {
			const QString &mimetype = mit.key();
			const QString &appFileId = mit.value();

			if (m_xdgMimeApps.getDefaultApp(mimetype) == appFileId) {
				m_defaultApps[mimetype] = appName;
				syncCount++;
			}
		}
	}
	qCDebug(sdaLog) << "SelectDefaultApplication: Sync-ed" << syncCount << "associations to UI";
}

void SelectDefaultApplication::populateApplicationList(const QString &filter)
{
	m_applicationList->clear();
	const auto &apps = m_xdgMimeApps.getApps();
	const auto &appIcons = m_xdgMimeApps.getApplicationIcons();
	QStringList sorted_app_names = apps.keys();
	std::sort(sorted_app_names.begin(), sorted_app_names.end());

	for (const QString &appName : sorted_app_names) {
		if (!filter.isEmpty() && !appName.contains(filter, Qt::CaseInsensitive)) {
			continue;
		}

		if (!m_filterMimegroup.isEmpty() && !applicationHasAnyCorrectMimetype(appName)) {
			continue;
		}

		QListWidgetItem *item = new QListWidgetItem(appName);
		item->setData(Qt::UserRole, appName);

		QString iconName = appIcons.value(appName);
		if (!iconName.isEmpty()) {
			if (m_iconPaths.contains(iconName)) {
				item->setIcon(QIcon(m_iconPaths.value(iconName)));
			} else {
				item->setIcon(QIcon::fromTheme(iconName));
			}
		} else {
			// Fallback if no icon name (though XDG usually provides one)
			item->setIcon(QIcon::fromTheme("application-x-executable"));
		}

		m_applicationList->addItem(item);
	}
}

void SelectDefaultApplication::loadIcons(const QString &path)
{
	QFileInfo icon_file(path);
	if (!icon_file.exists() || !icon_file.isDir()) {
		return;
	}
	// TODO: avoid hardcoding
	QStringList imageTypes({ "*.svg", "*.svgz", "*.png", "*.xpm" });
	QDirIterator iter(path, imageTypes, QDir::Files, QDirIterator::Subdirectories);

	while (iter.hasNext()) {
		iter.next();
		icon_file = iter.fileInfo();

		const QString name = icon_file.completeBaseName();
		if (m_iconPaths.contains(name)) {
			continue;
		}
		m_iconPaths[name] = icon_file.filePath();
	}
}

void SelectDefaultApplication::constrictGroup(QAction *action)
{
	m_groupChooser->setText(action->text());
	m_filterMimegroup = (action->text() == tr("All")) ? "" : action->text();
	m_searchBox->clear();
	populateApplicationList("");
	onApplicationSelected();
}

void SelectDefaultApplication::enableSetDefaultButton()
{
	m_setDefaultButton->setEnabled(m_mimetypeList->selectedItems().count() > 0);
	m_removeDefaultButton->setEnabled(m_currentDefaultApps->selectedItems().count() > 0);
}

void SelectDefaultApplication::onRemoveDefaultClicked()
{
	QList<QListWidgetItem *> selectedItems = m_applicationList->selectedItems();
	if (selectedItems.count() != 1) {
		return;
	}

	QList<QListWidgetItem *> mimetypesToRemove = m_currentDefaultApps->selectedItems();
	if (mimetypesToRemove.isEmpty()) {
		return;
	}

	QSet<QString> mimesToRemove;
	for (QListWidgetItem *item : mimetypesToRemove) {
		mimesToRemove.insert(item->data(Qt::UserRole).toString());
	}
	m_xdgMimeApps.removeDefaults(mimesToRemove);

	// Refresh everything from disk
	readCurrentDefaultMimetypes();
	// Update UI panels
	onApplicationSelectedLogic(false);
}

void SelectDefaultApplication::showHelp()
{
	QMessageBox *dialog = new QMessageBox(this);
	dialog->setText(tr("<h3>Help about Select Default Application</h3>"));
	dialog->setInformativeText(tr(
		"<html><body>"
		"<p><b>To use this program:</b>"
		"<ul>"
		"<li>Select any application on the <b>left panel</b>.</li>"
		"<li>Select or deselect any mimetypes in the <b>center</b> that you want this application to open.</li>"
		"<li>Using defaults is usually best; it will choose all mimetypes the application has explicit support for.</li>"
		"<li>Press <b>Add association(s)</b> at the bottom to apply changes.</li>"
		"</ul>"
		"<p>You can see your changes on the <b>right panel</b>.</p>"
		"<hr>"
		"<p><b>How this works:</b></p>"
		"<p>FreeDesktop environments utilize <b>Desktop Entries</b> (<code>.desktop</code> files) to tell launchers how to run programs.</p>"
		"<p>The tool <code>xdg-open</code> uses these entries to determine which application handles a file type, reading from system locations like <code>/usr/share/applications/</code> and user config at <code>~/.config/mimeapps.list</code>.</p>"
		"<p>This program parses these files to visualize current associations. When you apply changes, it writes to your <code>mimeapps.list</code>, ensuring your preferences take precedence.</p>"
		"</body></html>"));
	dialog->exec();
}

QSet<QString> SelectDefaultApplication::getGranularOverwriteConfirmation(const QHash<QString, QString> &warnings,
									 const QString &newApp)
{
	QDialog *dialog = new QDialog(this);
	dialog->setWindowTitle(tr("Conflicting Associations Detected"));
	dialog->setMinimumWidth(500);

	QVBoxLayout *mainLayout = new QVBoxLayout(dialog);

	// Description label
	QLabel *descLabel = new QLabel(tr("The following MIME types are already assigned to other applications.\n"
					  "Select which ones you want to overwrite:"));
	descLabel->setWordWrap(true);
	mainLayout->addWidget(descLabel);

	// Create checkboxes for each conflict
	QHash<QString, QCheckBox *> checkboxes;
	for (auto it = warnings.begin(); it != warnings.end(); ++it) {
		const QString &mimetype = it.key();
		const QString &currentApp = it.value();

		QCheckBox *checkbox = new QCheckBox();
		checkbox->setChecked(true); // Default to overwrite

		// Format: "application/x-msi (currently: Bottles)"
		QString labelText = QString("%1\n  Currently: %2").arg(mimetype, currentApp);
		checkbox->setText(labelText);

		checkboxes[mimetype] = checkbox;
		mainLayout->addWidget(checkbox);
	}

	// Buttons
	QHBoxLayout *buttonLayout = new QHBoxLayout();
	buttonLayout->addStretch();

	QPushButton *applyButton = new QPushButton(tr("Apply Selected"));
	QPushButton *cancelButton = new QPushButton(tr("Cancel"));

	buttonLayout->addWidget(applyButton);
	buttonLayout->addWidget(cancelButton);
	mainLayout->addLayout(buttonLayout);

	connect(applyButton, &QPushButton::clicked, dialog, &QDialog::accept);
	connect(cancelButton, &QPushButton::clicked, dialog, &QDialog::reject);

	QSet<QString> result;
	if (dialog->exec() == QDialog::Accepted) {
		// Collect checked MIME types
		for (auto it = checkboxes.begin(); it != checkboxes.end(); ++it) {
			if (it.value()->isChecked()) {
				result.insert(it.key());
			}
		}
	}

	delete dialog;
	return result;
}

bool SelectDefaultApplication::applicationHasAnyCorrectMimetype(const QString &appName)
{
	const QString &filter = m_filterMimegroup;
	const auto &apps = m_xdgMimeApps.getApps();
	const auto &childMimeTypes = m_xdgMimeApps.getChildMimeTypes();

	if (!apps.contains(appName)) {
		return false;
	}

	const QHash<QString, QString> &appMimetypes = apps.value(appName);
	for (auto it = appMimetypes.keyBegin(); it != appMimetypes.keyEnd(); ++it) {
		if (it->startsWith(filter)) {
			return true;
		}
		// Also check if any of the child mimetypes match
		// E.g. if we have text/plain, we also match text/x-csrc
		const QStringList children = childMimeTypes.values(*it);
		for (const QString &child : children) {
			if (child.startsWith(filter)) {
				return true;
			}
		}
	}
	return false;
}

const char *X_SCHEME_HANDLER = "x-scheme-handler/";
// Returns the value of m_mimeDb.mimeTypeForName(name) but
// mimeTypeForName(application/x-pkcs12) always returns application/x-pkcs12 instead of application/pkcs12
// If starts with x-scheme-handler, instead just returns the argument

const QString SelectDefaultApplication::mimetypeDescription(QString name)
{
	if (name.startsWith(X_SCHEME_HANDLER)) {
		// x-scheme-handler is not a valid mimetype for a file, but we do want to be able to set applications as the default handlers for it.
		// Assumes all x-scheme-handler/* is valid
		return "Handles " + name.mid(strlen(X_SCHEME_HANDLER)) + ":// URIs\n" + name;
	}
	// There appears to be a bug in Qt https://bugreports.qt.io/browse/QTBUG-99509, hack around it
	if (name == "application/pkcs12") {
		name = "application/x-pkcs12";
	}
	const QMimeType mimetype = m_mimeDb.mimeTypeForName(name);
	QString desc = mimetype.filterString().trimmed();
	if (desc.isEmpty()) {
		desc = mimetype.comment().trimmed();
	}
	if (!desc.isEmpty()) {
		desc += '\n';
	}
	return desc + name;
}
