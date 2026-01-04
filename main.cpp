#include "selectdefaultapplication.h"
#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QString>

int main(int argc, char *argv[])
{
	// Check for help/version flags to avoid loading QWidget/Gui logic for CLI tasks
	bool isGui = true;
	for (int i = 1; i < argc; ++i) {
		QString arg = QString::fromLocal8Bit(argv[i]);
		if (arg == "-h" || arg == "--help" || arg == "--help-all" || arg == "-v" || arg == "--version") {
			isGui = false;
			break;
		}
	}

	if (!isGui) {
		QCoreApplication a(argc, argv);
		a.setApplicationVersion("2.0");
		a.setApplicationName("Select Default Application"); // Console apps use Name usually

		QCommandLineParser parser;
		parser.setApplicationDescription(QCoreApplication::translate(
			"main", "A simple application to manage default MIME type associations on Linux."));
		parser.addHelpOption();
		parser.addVersionOption();

		QCommandLineOption verbose({ "V", "verbose" },
					   QCoreApplication::translate(
						   "main",
						   "Print verbose information about how the desktop files are parsed"));
		parser.addOption(verbose);
		parser.parse(a.arguments());
		if (parser.isSet("help")) {
			puts(qPrintable(parser.helpText()));
			return 0;
		}
		if (parser.isSet("version")) {
			printf("%s %s\n", qPrintable(a.applicationName()), qPrintable(a.applicationVersion()));
			return 0;
		}
		// Fallback if loop detected help/version but parser didn't see it (unlikely)
		return 0;
	}

	QApplication a(argc, argv);
	a.setApplicationVersion("2.0");
	a.setApplicationDisplayName("Select Default Application");

	QCommandLineParser parser;
	parser.setApplicationDescription(QCoreApplication::translate(
		"main", "A simple application to manage default MIME type associations on Linux."));
	parser.addHelpOption();
	parser.addVersionOption();

	QCommandLineOption verbose({ "V", "verbose" },
				   QCoreApplication::translate(
					   "main", "Print verbose information about how the desktop files are parsed"));

	parser.addOption(verbose);
	parser.process(a);

	if (parser.isSet(verbose)) {
		QLoggingCategory::setFilterRules(QStringLiteral("sda.log.debug=true"));
	}

	SelectDefaultApplication w(nullptr, parser.isSet(verbose));
	w.show();

	return a.exec();
}
