#include <QMetaEnum>
#include <QHostAddress>
#include <QDebug>

#include "client.h"

Client::Client(QObject *parent) :
    QObject(parent),
    socket(NULL)
{
    qDebug("Client initialized");
}

Client::~Client()
{
    qDebug("Client destroyed");
}

void Client::setSocket(QTcpSocket *socket)
{
    this->socket = socket;
    this->socket->setParent(this);

    connect(socket, SIGNAL(disconnected()), SLOT(onSocketDisconnected()));
    connect(socket, SIGNAL(disconnected()), SIGNAL(socketDisconnected()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(onSocketError(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(readyRead()), SLOT(onSocketReadyRead()));
}

void Client::onSocketDisconnected()
{
    disconnect(socket);

    socket->deleteLater();

    qDebug("Client disconnected");
}

void Client::onSocketError(QAbstractSocket::SocketError socketError)
{
    int indexOfSocketError = QAbstractSocket::staticMetaObject.indexOfEnumerator("SocketError");
    QString socketErrorKey = QAbstractSocket::staticMetaObject.enumerator(indexOfSocketError).key(socketError);

    qDebug() << "Client connection error:" << socketErrorKey;
}

void Client::onSocketReadyRead()
{
    qDebug() << "Client received:" << socket->readAll();
}
