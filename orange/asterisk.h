#ifndef ASTERISK_H
#define ASTERISK_H

#include <QObject>
#include <QTcpSocket>
#include <QStringList>

class Asterisk : public QObject
{
    Q_OBJECT

public:
    explicit Asterisk(QObject *parent = 0, QString host = "localhost", quint16 port = 5038);
    ~Asterisk();

    QVariantHash login(QString username, QString secret);
    QVariantHash logout();

    QVariantHash coreShowChannels();
    QVariantHash sipPeers();

    QVariantHash originate(QString channel,
                           QString exten = QString(),
                           QString context = QString(),
                           uint priority = 0,
                           QString application = QString(),
                           QString data = QString(),
                           uint timeout = 0,
                           QString callerId = QString(),
                           QVariantHash variables = QVariantHash(),
                           QString account = QString(),
                           bool earlyMedia = false,
                           bool async = false,
                           QStringList codecs = QStringList());

    QVariantHash playDtmf(QString channel, QChar digit);
    QVariantHash hangup(QString channel, uint cause = 0);

    QVariantHash redirect(QString channel,
                          QString exten,
                          QString context,
                          uint priority,
                          QString extraChannel = QString(),
                          QString extraExten = QString(),
                          QString extraContext = QString(),
                          uint extraPriority = 0);

private:
    QTcpSocket socket;
    QString host, username, secret;
    quint16 port;
    QHash<QString, QVariantHash> responses;

    void insertNotEmpty(QVariantHash *fields, QString key, QVariant value);
    QString encodeValue(QVariant value);
    QVariant decodeValue(QString string);

    QVariantHash sendPacket(QString action, QVariantHash headers = QVariantHash());

private slots:
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onSocketReadyRead();

signals:
    void eventReceived(QString event, QVariantHash headers);
};

#endif // ASTERISK_H
