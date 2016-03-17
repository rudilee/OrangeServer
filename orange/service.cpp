#include <QTcpSocket>
#include <QTimer>
#include <QDebug>

#include "common.h"
#include "terminal.h"
#include "service.h"

Service::Service(int &argc, char **argv) :
    QObject(),
    QtService<QCoreApplication>(argc, argv, APPLICATION_NAME),
    settings(new QSettings(CONFIG_FILE, QSettings::IniFormat)),
    workerCount(1),
    currentWorkerIndex(0)
{
    qDebug("Service initialized");
}

Service::~Service()
{
    settings->deleteLater();

    qDebug("Service destroyed");
}

void Service::createApplication(int &argc, char **argv)
{
    QtService::createApplication(argc, argv);

    setupServer();
    setupDatabase();

    qDebug("Application created");
}

void Service::processCommand(int code)
{
    qDebug() << "Received command code:" BOLD BLUE << code << RESET;
}

void Service::start()
{
    startServer();
    createWorkers();
    openDatabase();

    qDebug("Service started");
}

void Service::stop()
{
    forceLogoutUsers();
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
    quint16 port = settings->value("orange/port", 18279).toUInt();

    server.listen(QHostAddress::Any, port);

    qDebug() << "Server started, listening on port:" BOLD BLUE << port << RESET;
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

void Service::setupDatabase()
{
    QString host = settings->value("database/host", "localhost").toString(),
            name = settings->value("database/name", "icentra").toString(),
            username = settings->value("database/username", "icentra").toString(),
            password = settings->value("database/password").toString();

    int port = settings->value("database/port", 5432).toInt();

    database = QSqlDatabase::addDatabase("QPSQL");
    database.setHostName(host);
    database.setPort(port);
    database.setDatabaseName(name);
    database.setUserName(username);

    if (!password.isEmpty())
        database.setPassword(password);
}

int Service::circulateWorkerIndex()
{
    currentWorkerIndex = (currentWorkerIndex + 1) % workerCount;

    return currentWorkerIndex;
}

void Service::forceLogoutUsers()
{
    QHashIterator<QString, Client *> clientAddress(clientAddressMap);

    while (clientAddress.hasNext()) {
        clientAddress.next();
        clientAddress.value()->forceLogout();
    }
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
        connect(client, SIGNAL(userLoggedIn()), SLOT(onClientUserLoggedIn()));
        connect(client, SIGNAL(userLoggedOut()), SLOT(onClientUserLoggedOut()));
        connect(client, SIGNAL(userStatusChanged(Client::Status)), SLOT(onClientUserStatusChanged(Client::Status)));
        connect(client, SIGNAL(phoneStatusChanged(QString)), SLOT(onClientPhoneStatusChanged(QString)));
        connect(client, SIGNAL(askDialAuthorization(QString,QString,QString)), SLOT(onClientAskDialAuthorization(QString,QString,QString)));
        connect(client, SIGNAL(changeAgentStatus(Client::Status,QString)), SLOT(onClientChangeAgentStatus(Client::Status,QString)));

        qDebug() << "Client connected from:" BOLD BLUE << clientAddress << RESET;
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

void Service::onClientUserLoggedIn()
{
    Client *client = (Client *) sender();

    clientUserMap.insert(client->getUsername(), client);
}

void Service::onClientUserLoggedOut()
{
    Client *client = (Client *) sender();
    QString username = client->getUsername();

    if (clientUserMap.contains(username))
        clientUserMap.remove(username);
}

void Service::onClientUserStatusChanged(Client::Status status)
{
    ;
}

void Service::onClientPhoneStatusChanged(QString status)
{
    ;
}

void Service::onClientAskDialAuthorization(QString destination, QString customerId, QString campaign)
{
    ;
}

void Service::onClientChangeAgentStatus(Client::Status status, QString extension)
{
    ;
}

void Service::openDatabase()
{
    if (!database.isOpen()) {
        if (!database.open()) {
            qWarning("Database connection failed, reconnecting in 15 seconds");

            QTimer::singleShot(15000, this, SLOT(openDatabase()));
        } else {
            qDebug("Database connected");
        }
    }
}
