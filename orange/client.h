#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QSettings>
#include <QTcpSocket>
#include <QDateTime>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QSqlQuery>

class Client : public QObject
{
    Q_OBJECT

public:
    enum Level {
        Agent,
        Supervisor,
        Manager
    };

    enum Status {
        Login = 1,
        Ready,
        NotReady,
        Logout,
        AUX,
        ACW
    };

    struct Phone {
        QDateTime time;
        QString status;
        QString channel;
        bool active;
        bool outbound;
        QString dnis;
    };

    explicit Client(QObject *parent = 0);
    ~Client();

    void setSocket(QTcpSocket *socket);
    void forceLogout();

    void sendAgentStatus(QString username = QString(),
                         QString fullname = QString(),
                         int handle = 0,
                         int abandoned = 0,
                         Phone phone = Phone(),
                         QString group = QString());

protected:
    void timerEvent(QTimerEvent *event);

    void logFailedQuery(QSqlQuery *query, QString queryTitle);

    QVariant getLastInsertId(QString table, QString column);

    void initiateHandshake();

    void retrieveExtension();
    void retrieveSkills();
    void startSession();
    void endSession();
    void startStatus(Status status);
    void changeStatus(Status status);
    void endStatus();
    void endLogging();
    void changePhoneStatus(QString status, bool outbound);

    void resetHeartbeatTimer();
    void checkAuthentication(QString authentication, bool encrypted);
    void dispatchAction(QString actionType, QXmlStreamAttributes attributes);

private:
    QSettings *settings;
    QTcpSocket *socket;
    QXmlStreamReader socketIn;
    QXmlStreamWriter socketOut;
    QHash<QString, Status> statusText;

    int heartbeatTimerId;
    quint32 agentId, agentExtenMapId;
    quint64 agentLogSessionId, agentLogStatusId;

    QString username, fullname, group;

    Level level;
    Status status;
    Phone phone;
    int handle, abandoned;

protected slots:
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onSocketReadyRead();

signals:
    void socketDisconnected();

    void userLoggedIn(QString username);
    void userLoggedOut(QString username);
    void userStatusChanged(QString username, Status status);
    void phoneStatusChanged(QString username, QString status);
};

#endif // CLIENT_H
