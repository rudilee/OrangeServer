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
    void forceLogoutUsers();

    int circulateWorkerIndex();

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
    void onClientUserLoggedIn(QString username);
    void onClientUserLoggedOut(QString username);

private slots:
    void openDatabase();
};

#endif // SERVICE_H
