#include <QTcpSocket>
#include <QTimer>
#include <QDebug>

#include "common.h"
#include "terminal.h"
#include "service.h"

Service::Service(int &argc, char **argv) :
    QObject(),
    QtService<QCoreApplication>(argc, argv, APPLICATION_NAME),
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

    setupSettings();
    setupServer();
    setupDatabase();
    setupAsterisk();
    createWorkers();

    qDebug("Application created");
}

void Service::processCommand(int code)
{
    qDebug() << "Received command code:" BOLD BLUE << code << RESET;
}

void Service::start()
{
    startServer();
    openDatabase();
    connectToAsterisk();

    qDebug("Service started");
}

void Service::stop()
{
    forceLogoutUsers();
//    stopWorkers();

    settings->deleteLater();
    asterisk->deleteLater();

    qDebug("Service stopped");
}

void Service::setupSettings()
{
    settings = new QSettings(CONFIG_FILE, QSettings::IniFormat);
}

void Service::setupServer()
{
    connect(&server, SIGNAL(newConnection()), SLOT(onServerNewConnection()));

    qDebug("Server has been setup");
}

void Service::startServer()
{
    quint16 port = settings->value("orange/port", 18279).toUInt();

    server.listen(QHostAddress::Any, port);

    qDebug() << "Server started, listening on port:" BOLD BLUE << port << RESET;
}

void Service::setupAsterisk()
{
    QString host = settings->value("asterisk/host", "localhost").toString();
    quint16 port = settings->value("asterisk/port", 5038).toUInt();

    asterisk = new Asterisk(this, host, port);

    connect(asterisk, SIGNAL(eventReceived(QString,QVariantHash)), SLOT(onAsteriskEventReceived(QString,QVariantHash)));
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
    QHashIterator<QString, Client *> clientAddress(addressClientMap);

    while (clientAddress.hasNext()) {
        clientAddress.next();
        clientAddress.value()->forceLogout();
    }
}

void Service::broadcastAgentStatus(Client *client)
{
    ;
}

bool Service::checkGroupIntersected(Client *superior, Client *subordinate)
{
    return !superior->getGroups().toSet().intersect(subordinate->getGroups().toSet()).isEmpty();
}

void Service::onServerNewConnection()
{
    if (server.hasPendingConnections()) {
        QTcpSocket *newSocket = server.nextPendingConnection();
        QString clientAddress = newSocket->peerAddress().toString();

        Client *client = new Client;
        client->setSocket(newSocket);
        client->moveToThread(workers.at(circulateWorkerIndex()));

        addressClientMap.insert(clientAddress, client);

        connect(client, SIGNAL(socketDisconnected()), SLOT(onClientSocketDisconnected()));
        connect(client, SIGNAL(userLoggedIn()), SLOT(onClientUserLoggedIn()));
        connect(client, SIGNAL(userLoggedOut()), SLOT(onClientUserLoggedOut()));
        connect(client, SIGNAL(askDialAuthorization(QString,QString,QString)), SLOT(onClientAskDialAuthorization(QString,QString,QString)));
        connect(client, SIGNAL(spyAgentPhone(QString)), SLOT(onClientSpyAgentPhone(QString)));
        connect(client, SIGNAL(changeAgentStatus(Client::Status,bool,QString)), SLOT(onClientChangeAgentStatus(Client::Status,bool,QString)));

        qDebug() << "Client connected from:" BOLD BLUE << clientAddress << RESET;
    }
}

void Service::onAsteriskEventReceived(QString event, QVariantHash headers)
{
    if (event == "FullyBooted") {
        asterisk->sipPeers();
        asterisk->coreShowChannels();
    } else if (event == "PeerEntry" || event == "Registry") {
        ;
    } else if (event == "CoreShowChannel" || event == "Newchannel") {
        ;
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
    QString clientAddress = addressClientMap.key(client);

    if (!clientAddress.isEmpty())
        addressClientMap.remove(clientAddress);

    disconnect(client);

    client->deleteLater();
}

void Service::onClientUserLoggedIn()
{
    Client *client = (Client *) sender();
    QString username = client->getUsername();

    if (usernameAddressMap.contains(username)) {
        client->forceLogout("same user login");

        return;
    }

    usernameAddressMap.insert(username, client->getIpAddress());

    foreach (QString group, client->getGroups()) {
        if (!groups.contains(group))
            groups.insert(group, new Group(group, this));

        groups.value(group)->addMember(client);
    }
}

void Service::onClientUserLoggedOut()
{
    Client *client = (Client *) sender();
    QString username = client->getUsername();

    if (usernameAddressMap.contains(username))
        usernameAddressMap.remove(username);
}

void Service::onClientUserExtensionChanged(QString extension)
{
    Client *client = (Client *) sender();

    extensionUsernameMap.insert(extension, client->getUsername());
}

void Service::onClientAskDialAuthorization(QString destination, QString customerId, QString campaign)
{
    Client *client = (Client *) sender();
    client->sendDialerResponse(destination);

    qDebug() << "User" BOLD BLUE << client->getUsername() << RESET "dialing" BOLD BLUE << destination << RESET;
}

void Service::onClientSpyAgentPhone(QString agentUsername)
{
    ;
}

void Service::onClientChangeAgentStatus(Client::Status status, bool outbound, QString extension)
{
    Client *client = (Client *) sender(),
           *target = addressClientMap.value(usernameAddressMap.value(extensionUsernameMap.value(extension)));

    if (target != NULL) {
        if (checkGroupIntersected(client, target)) {
            target->changeStatus(status);
            target->changePhoneStatus(status == Client::Ready ? "ready" : "aux", outbound);
        }
    }
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

void Service::connectToAsterisk()
{
    QString username = settings->value("asterisk/username").toString(),
            secret = settings->value("asterisk/secret").toString();

    QVariantHash response = asterisk->login(username, secret);
    QString message = response["Message"].toString();

    if (response["Response"].toString() == "Success")
        qDebug() << "Asterisk login " BOLD GREEN "Succeed" RESET << message;
    else
        qDebug() << "Asterisk login " BOLD RED "Failed" RESET << message;
}
