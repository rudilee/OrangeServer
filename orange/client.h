#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTcpSocket>

class Client : public QObject
{
    Q_OBJECT

public:
    explicit Client(QObject *parent = 0);
    ~Client();

    void setSocket(QTcpSocket *socket);

private:
    QTcpSocket *socket;

protected slots:
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onSocketReadyRead();

signals:
    void socketDisconnected();

    void userLoggedIn(QString username);
    void userLoggedOut(QString username);
};

#endif // CLIENT_H
