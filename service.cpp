#include <QDebug>

#include "service.h"

Service::Service(int &argc, char **argv) : QtService<QCoreApplication>(argc, argv, "Orange Server")
{
    qDebug() << "Service initiated";
}

Service::~Service()
{
    qDebug() << "Service destroyed";
}

void Service::createApplication(int &argc, char **argv)
{
    QtService::createApplication(argc, argv);

    qDebug() << "Application created";
}

int Service::executeApplication()
{
    qDebug() << "Application executing";

    return QtService::executeApplication();
}

void Service::processCommand(int code)
{
    qDebug() << "Received command code:" << code;
}

void Service::start()
{
    qDebug() << "Service started";
}

void Service::stop()
{
    qDebug() << "Service stopped";
}
