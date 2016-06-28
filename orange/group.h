#ifndef GROUP_H
#define GROUP_H

#include <QObject>

#include "client.h"

class Group : public QObject
{
    Q_OBJECT

public:
    explicit Group(QString queue, QObject *parent = 0);
    ~Group();

    void addMember(Client *client);

private:
    QString queue;
    QHash<QString, Client *> members; // key: Username

    void sendAgentStatus(Client *sender, Client *receiver);
    void broadcastAgentStatus(Client *client);
    void retrieveAgentStatuses(Client *client);

private slots:
    void onClientUserLoggedOut();
    void onClientPhoneStatusChanged(QString status);
};

#endif // GROUP_H
