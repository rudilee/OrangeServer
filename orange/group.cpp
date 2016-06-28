#include <QDebug>

#include "terminal.h"
#include "group.h"

Group::Group(QString queue, QObject *parent) :
    QObject(parent),
    queue(queue)
{
    qDebug() << "Group" BOLD BLUE << queue << RESET "initialized";
}

Group::~Group()
{
    qDebug() << "Group" BOLD BLUE << queue << RESET "destroyed";
}

void Group::addMember(Client *client)
{
    if (members.contains(client->getUsername()))
        return;

    members.insert(client->getUsername(), client);

    connect(client, SIGNAL(userLoggedOut()), SLOT(onClientUserLoggedOut()));
    connect(client, SIGNAL(phoneStatusChanged(QString)), SLOT(onClientPhoneStatusChanged(QString)));

    broadcastAgentStatus(client);
    retrieveAgentStatuses(client);

    qDebug() << "Adding" BOLD BLUE << client->getUsername() << RESET "to group" BOLD BLUE << queue << RESET;
}

void Group::sendAgentStatus(Client *sender, Client *receiver)
{
    if (receiver != sender && receiver->getLevel() > sender->getLevel()) {
        receiver->sendAgentStatus(sender->getUsername(),
                                  sender->getFullname(),
                                  sender->getPhone(),
                                  sender->getHandle(),
                                  sender->getAbandoned(),
                                  sender->getGroups().first(),
                                  QDateTime::currentDateTime(),
                                  sender->getIpAddress(),
                                  sender->getExtension());
    }
}

void Group::broadcastAgentStatus(Client *client)
{
    QHashIterator<QString, Client *> member(members);
    while (member.hasNext()) {
        member.next();

        if (member.value()->getLevel() > Client::Agent)
            sendAgentStatus(client, member.value());
    }
}

void Group::retrieveAgentStatuses(Client *client)
{
    QHashIterator<QString, Client *> member(members);
    while (member.hasNext()) {
        member.next();

        sendAgentStatus(member.value(), client);
    }
}

void Group::onClientUserLoggedOut()
{
    Client *client = (Client *) sender();

    members.remove(client->getUsername());

    QHashIterator<QString, Client *> member(members);
    while (member.hasNext()) {
        member.next();

        if (member.value()->getLevel() > Client::Agent) {
            if (member.value()->getLevel() > client->getLevel()) {
                member.value()->sendAgentLogout(client->getUsername(),
                                                client->getExtension(),
                                                client->getGroups().first(),
                                                client->getIpAddress());
            }
        }
    }
}

void Group::onClientPhoneStatusChanged(QString status)
{
    broadcastAgentStatus((Client *) sender());

    Q_UNUSED(status)
}

