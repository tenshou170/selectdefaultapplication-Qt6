#pragma once

#include <QWidget>
#include <QMimeDatabase>
#include <QMultiHash>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QLineEdit>
#include <QSet>
#include <QMenu>
#include "xdgmimeapps.h"

class QFileInfo;
class QTreeWidget;
class QListWidget;
class QPushButton;

class SelectDefaultApplication : public QWidget {
	Q_OBJECT

public:
	SelectDefaultApplication(QWidget *parent, bool isVerbose);
	~SelectDefaultApplication() override;

private slots:
	void onApplicationSelected();
	void onSetDefaultClicked();
	void populateApplicationList(const QString &filter);
	void showHelp();
	void constrictGroup(QAction *action);
	void enableSetDefaultButton();
	void onRemoveDefaultClicked();

private:
	void setDefault(const QString &appName, QSet<QString> &mimetypes);
	void loadIcons(const QString &path);
	void addToMimetypeList(QListWidget *list, const QString &mimetypeName, const bool selected);
	void readCurrentDefaultMimetypes();
	bool applicationHasAnyCorrectMimetype(const QString &appName);
	void onApplicationSelectedLogic(bool allowEnable);

	QSet<QString> getGranularOverwriteConfirmation(const QHash<QString, QString> &warnings, const QString &newApp);
	const QString mimetypeDescription(QString name);

	// Global variable to match selected mimegroup on
	QString m_filterMimegroup;
	// Multi-hashtable with keys as mimetypes and values as application names
	QHash<QString, QString> m_defaultApps;

	bool isVerbose;

	// for preloading icons, because that's (a bit) slooow
	QHash<QString, QIcon> m_mimeTypeIcons;
	QHash<QString, QString> m_iconPaths;

	QMimeDatabase m_mimeDb;

	// XDG MIME Apps specification compliant config manager
	XdgMimeApps m_xdgMimeApps;

	// UI elements
	QListWidget *m_applicationList;
	QListWidget *m_mimetypeList;
	QListWidget *m_currentDefaultApps;
	QLineEdit *m_searchBox;
	QPushButton *m_groupChooser;
	QMenu *m_mimegroupMenu;
	QPushButton *m_setDefaultButton;
	QPushButton *m_removeDefaultButton;
	QToolButton *m_infoButton;
	QLabel *m_middleBanner;
	QLabel *m_rightBanner;
};
