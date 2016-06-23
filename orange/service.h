#ifndef SERVICE_H
#define SERVICE_H

#include <QtService>
#include <QSettings>
#include <QSqlDatabase>
#include <QTcpServer>

#include "asterisk.h"
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

    void setupSettings();
    void setupServer();
    void startServer();
    void setupAsterisk();
    void createWorkers();
    void stopWorkers();
    void setupDatabase();

    int circulateWorkerIndex();

    void forceLogoutUsers();
    void broadcastAgentStatus(Client *client);

private:
    QSettings *settings;
    QTcpServer server;
    QSqlDatabase database;
    Asterisk *asterisk;
    QList<Worker *> workers;
    QHash<QString, Client *> addressClientMap; // key: IP Address
    QHash<QString, QString> usernameAddressMap; // key: Username, value: IP Address
    QHash<QString, QString> extensionUsernameMap; // key: Extension, value: Username
    QStringList agents, supervisors, managers;
    int workerCount, currentWorkerIndex;

    bool checkGroupIntersected(Client *superior, Client *subordinate);

protected slots:
    void onServerNewConnection();

    void onAsteriskEventReceived(QString event, QVariantHash headers);

    void onWorkerFinished();

    void onClientSocketDisconnected();
    void onClientUserLoggedIn();
    void onClientUserLoggedOut();
    void onClientUserExtensionChanged(QString extension);
    void onClientUserStatusChanged(Client::Status status);
    void onClientPhoneStatusChanged(QString status);
    void onClientAskDialAuthorization(QString destination, QString customerId, QString campaign);
    void onClientSpyAgentPhone(QString agentUsername);
    void onClientChangeAgentStatus(Client::Status status, bool outbound, QString extension);

private slots:
    void openDatabase();
    void connectToAsterisk();
};

#endif // SERVICE_H
