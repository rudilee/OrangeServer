#ifndef SERVICE_H
#define SERVICE_H

#include <QtService>
#include <QSettings>
#include <QSqlDatabase>
#include <QTcpServer>

#include "worker.h"
#include "client.h"

class Service : public QObject, public QtService<QCoreApplication>
{
    Q_OBJECT

public:
    Service(int &argc, char **argv);
    ~Service();

protected:
    void createApplication(int &argc, char **argv);
    void processCommand(int code);
    void start();
    void stop();

    void setupServer();
    void startServer();
    void createWorkers();
    void stopWorkers();
    void setupDatabase();

    int circulateWorkerIndex();

    void forceLogoutUsers();

private:
    QSettings *settings;
    QTcpServer server;
    QSqlDatabase database;
    QList<Worker *> workers;
    QHash<QString, Client *> clientAddressMap; // key: IP Address
    QHash<QString, Client *> clientUserMap; // key: Username
    int workerCount, currentWorkerIndex;

protected slots:
    void onServerNewConnection();

    void onWorkerFinished();

    void onClientSocketDisconnected();
    void onClientUserLoggedIn();
    void onClientUserLoggedOut();
    void onClientUserStatusChanged(Client::Status status);
    void onClientPhoneStatusChanged(QString status);
    void onClientAskDialAuthorization(QString destination, QString customerId, QString campaign);
    void onClientChangeAgentStatus(Client::Status status, QString extension);

private slots:
    void openDatabase();
};

#endif // SERVICE_H
