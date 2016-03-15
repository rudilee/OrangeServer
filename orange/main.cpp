#include <iostream>

#include <QDateTime>

#include "common.h"
#include "terminal.h"
#include "service.h"

void messageLogger(QtMsgType type, const char *message)
{
    QHash<QtMsgType, QString> logTypes;
    logTypes[QtDebugMsg] = "INFO";
    logTypes[QtCriticalMsg] = "CRITICAL";
    logTypes[QtWarningMsg] = "WARNING";
    logTypes[QtFatalMsg] = "FATAL";

    QHash<QtMsgType, QString> logColors;
    logColors[QtDebugMsg] = GREEN;
    logColors[QtCriticalMsg] = RED;
    logColors[QtWarningMsg] = YELLOW;
    logColors[QtFatalMsg] = BOLD RED;

    std::cout << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss:zzz").toLatin1().data()
              << QString(" [%1%2" RESET "] ").arg(logColors.value(type), logTypes.value(type)).toLatin1().data()
              << QString(message).replace("\"", "").trimmed().toLatin1().data()
              << std::endl;
}

int main(int argc, char *argv[])
{
    qInstallMsgHandler(messageLogger);

    QCoreApplication::setOrganizationDomain(ORGANIZATION_DOMAIN);
    QCoreApplication::setOrganizationName(ORGANIZATION_NAME);
    QCoreApplication::setApplicationName(APPLICATION_NAME);
    QCoreApplication::setApplicationVersion(APPLICATION_VERSION);

    return Service(argc, argv).exec();
}
