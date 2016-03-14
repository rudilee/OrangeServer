#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QSettings>
#include <QTcpSocket>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QSqlQuery>

class Client : public QObject
{
    Q_OBJECT

public:
    explicit Client(QObject *parent = 0);
    ~Client();

    void setSocket(QTcpSocket *socket);
    void forceLogout();

protected:
    void timerEvent(QTimerEvent *event);

    void logFailedQuery(QSqlQuery *query, QString queryTitle);

    QString getCurrentTime();
    QVariant getLastInsertId(QString table, QString id);

    void initiateHandshake();

    void retrieveExtension();
    void retrieveSkills();

    void startSession();
    void endSession();

    void resetHeartbeatTimer();
    void checkAuthentication(QString authentication, bool encrypted);

private:
    QSettings *settings;
    QTcpSocket *socket;
    QXmlStreamReader socketIn;
    QXmlStreamWriter socketOut;
    int heartbeatTimerId;
    quint32 agentId, agentExtenMapId;
    quint64 agentSessionId;

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
