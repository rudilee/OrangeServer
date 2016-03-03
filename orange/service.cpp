#include <QTcpSocket>
#include <QDebug>

#include "service.h"

Service::Service(int &argc, char **argv) :
    QObject(),
    QtService<QCoreApplication>(argc, argv, "Orange Server"),
    workerCount(1),
    currentWorkerIndex(0)
{
    qDebug("Service initialized");
}

Service::~Service()
{
    qDebug("Service destroyed");
}

void Service::createApplication(int &argc, char **argv)
{
    QtService::createApplication(argc, argv);

    setupServer();

    qDebug("Application created");
}

void Service::processCommand(int code)
{
    qDebug() << "Received command code:" << code;
}

void Service::start()
{
    startServer();
    createWorkers();

    qDebug("Service started");
}

void Service::stop()
{
    stopWorkers();

    qDebug("Service stopped");
}

void Service::setupServer()
{
    connect(&server, SIGNAL(newConnection()), SLOT(onServerNewConnection()));

    qDebug("Server was setup");
}

void Service::startServer()
{
    quint16 port = settings.value("orange/port", 12345).toUInt();

    server.listen(QHostAddress::Any, port);

    qDebug() << "Server started, listening on port:" << port;
}

void Service::createWorkers()
{
    workerCount = QThread::idealThreadCount();

    if (workerCount > 1)
        workerCount--;

    for (int i = 0; i < workerCount; ++i) {
        Worker *worker = new Worker(i);
        worker->start();

        workers.append(worker);

        connect(worker, SIGNAL(finished()), SLOT(onWorkerFinished()));
    }

    qDebug("Workers created");
}

void Service::stopWorkers()
{
    foreach (Worker *worker, workers) {
        if (worker->isRunning())
            worker->quit();
    }
}

int Service::circulateWorkerIndex()
{
    currentWorkerIndex = (currentWorkerIndex + 1) % workerCount;

    return currentWorkerIndex;
}

void Service::onServerNewConnection()
{
    if (server.hasPendingConnections()) {
        QTcpSocket *newSocket = server.nextPendingConnection();
        QString clientAddress = newSocket->peerAddress().toString();

        Client *client = new Client;
        client->setSocket(newSocket);
        client->moveToThread(workers.at(circulateWorkerIndex()));

        clientAddressMap.insert(clientAddress, client);

        connect(client, SIGNAL(socketDisconnected()), SLOT(onClientSocketDisconnected()));
        connect(client, SIGNAL(userLoggedIn(QString)), SLOT(onClientUserLoggedIn(QString)));
        connect(client, SIGNAL(userLoggedOut(QString)), SLOT(onClientUserLoggedOut(QString)));

        qDebug() << "Client connected from:" << clientAddress;
    }
}

void Service::onWorkerFinished()
{
    Worker *worker = (Worker *) sender();
    worker->deleteLater();

    workers.removeAll(worker);

    workerCount = workers.count();
    currentWorkerIndex = 0;
}

void Service::onClientSocketDisconnected()
{
    Client *client = (Client *) sender();
    QString clientAddress = clientAddressMap.key(client);

    if (!clientAddress.isEmpty())
        clientAddressMap.remove(clientAddress);

    disconnect(client);

    client->deleteLater();
}

void Service::onClientUserLoggedIn(QString username)
{

}

void Service::onClientUserLoggedOut(QString username)
{

}
