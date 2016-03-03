#ifndef SERVICE_H
#define SERVICE_H

#include <QtService>
#include <QSettings>
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

    int circulateWorkerIndex();

private:
    QSettings settings;
    QTcpServer server;
    QList<Worker *> workers;
    QHash<QString, Client *> clientAddressMap; // key: IP Address
    int workerCount, currentWorkerIndex;

protected slots:
    void onServerNewConnection();

    void onWorkerFinished();

    void onClientSocketDisconnected();
    void onClientUserLoggedIn(QString username);
    void onClientUserLoggedOut(QString username);
};

#endif // SERVICE_H
