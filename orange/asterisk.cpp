#include <QUuid>
#include <QDebug>

#include "terminal.h"
#include "asterisk.h"

Asterisk::Asterisk(QObject *parent, QString host, quint16 port) :
    QObject(parent),
    host(host),
    port(port)
{
    connect(&socket, SIGNAL(disconnected()), SLOT(onSocketDisconnected()));
    connect(&socket, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(onSocketError(QAbstractSocket::SocketError)));
    connect(&socket, SIGNAL(readyRead()), SLOT(onSocketReadyRead()));

    qDebug("Asterisk Manager initialized");
}

Asterisk::~Asterisk()
{
    qDebug("Asterisk Manager destroyed");
}

QVariantHash Asterisk::login(QString username, QString secret)
{
    this->username = username;
    this->secret = secret;

    if (socket.state() != QTcpSocket::ConnectedState) {
        socket.connectToHost(host, port);

        if (socket.waitForConnected()) {
            socket.blockSignals(true);

            if (socket.waitForReadyRead())
                qDebug() << "Asterisk Manager connected:" BOLD BLUE << socket.readLine() << RESET;

            socket.blockSignals(false);

            QVariantHash headers;
            headers["Username"] = username;
            headers["Secret"] = secret;

            return sendPacket("Login", headers);
        }
    }
}

QVariantHash Asterisk::logout()
{
    return sendPacket("Logout");
}

QVariantHash Asterisk::coreShowChannels()
{
    return sendPacket("CoreShowChannels");
}

QVariantHash Asterisk::sipPeers()
{
    return sendPacket("SIPpeers");
}

QVariantHash Asterisk::originate(QString channel,
                                 QString exten,
                                 QString context,
                                 uint priority,
                                 QString application,
                                 QString data,
                                 uint timeout,
                                 QString callerId,
                                 QVariantHash variables,
                                 QString account,
                                 bool earlyMedia,
                                 bool async,
                                 QStringList codecs)
{
    QVariantHash headers;
    headers["Channel"] = channel;
    headers["EarlyMedia"] = earlyMedia;
    headers["Async"] = async;

    insertNotEmpty(&headers, "Timeout", timeout);
    insertNotEmpty(&headers, "CallerID", callerId);
    insertNotEmpty(&headers, "Account", account);
    insertNotEmpty(&headers, "Codecs", codecs.join(","));

    if (!exten.isEmpty() && !context.isEmpty() && priority > 0) {
        headers["Exten"] = exten;
        headers["Context"] = context;
        headers["Priority"] = priority;
    }

    if (!application.isEmpty()) {
        headers["Application"] = application;

        insertNotEmpty(&headers, "Data", data);
    }

    if (!variables.isEmpty()) {
        QHashIterator<QString, QVariant> variable(variables);
        while (variable.hasNext()) {
            variable.next();

            headers.insertMulti("Variable", QString("%1=%2").arg(variable.key(), encodeValue(variable.value())));
        }
    }

    return sendPacket("Originate", headers);
}

QVariantHash Asterisk::playDtmf(QString channel, QChar digit)
{
    QVariantHash headers;
    headers["Channel"] = channel;
    headers["Digit"] = digit;

    return sendPacket("PlayDTMF", headers);
}

QVariantHash Asterisk::hangup(QString channel, uint cause)
{
    QVariantHash headers;
    headers["Channel"] = channel;

    insertNotEmpty(&headers, "Cause", cause);

    return sendPacket("Hangup", headers);
}

QVariantHash Asterisk::redirect(QString channel, QString exten, QString context, uint priority, QString extraChannel, QString extraExten, QString extraContext, uint extraPriority)
{
    QVariantHash headers;
    headers["Channel"] = channel;
    headers["Exten"] = exten;
    headers["Context"] = context;
    headers["Priority"] = priority;

    insertNotEmpty(&headers, "ExtraChannel",  extraChannel);
    insertNotEmpty(&headers, "ExtraExten", extraExten);
    insertNotEmpty(&headers, "ExtraContext", extraContext);
    insertNotEmpty(&headers, "ExtraPriority", extraPriority);

    return sendPacket("Redirect", headers);
}

void Asterisk::insertNotEmpty(QVariantHash *headers, QString key, QVariant value)
{
    bool isEmpty = false;

    switch (value.type()) {
    case QMetaType::UInt: isEmpty = value.toUInt() == 0;
        break;
    default: isEmpty = value.isNull();
        break;
    }

    if (!isEmpty)
        headers->insert(key, value);
}

QString Asterisk::encodeValue(QVariant value)
{
    if ((QMetaType::Type) value.type() == QMetaType::Bool)
        return value.toBool() ? "true" : "false";

    return value.toString();
}

QVariant Asterisk::decodeValue(QString string)
{
    QVariant value(string);

    if (string == "true" || string == "false")
        value.setValue(string == "true");
    else
        value.setValue(string);

    return value;
}

QVariantHash Asterisk::sendPacket(QString action, QVariantHash headers)
{
    QString actionId = QUuid::createUuid().toString();

    if (socket.state() == QTcpSocket::ConnectedState) {
        headers["Action"] = action;
        headers["ActionID"] = actionId;

        QHashIterator<QString, QVariant> header(headers);
        while (header.hasNext()) {
            header.next();

            socket.write(QString("%1: %2\r\n").arg(header.key(), encodeValue(header.value())).toLatin1());
        }

        socket.write("\r\n");
        socket.flush();
        socket.waitForReadyRead();
    }

    return responses.take(actionId);
}

void Asterisk::onSocketDisconnected()
{
    ;
}

void Asterisk::onSocketError(QAbstractSocket::SocketError socketError)
{
    ;
}

void Asterisk::onSocketReadyRead()
{
//    qDebug("<ready-read>");

    QVariantHash headers;

    while (socket.canReadLine()) {
        QByteArray line = socket.readLine();

//        qDebug() << "Line:" << line;

        if (line != "\r\n") {
            QStringList header = QString(line.trimmed()).split(':');

            headers.insertMulti(header[0], decodeValue(header[1].trimmed()));
        } else {
            if (headers.contains("Response"))
                responses.insert(headers.take("ActionID").toString(), headers);
            else if (headers.contains("Event"))
                emit eventReceived(headers.take("Event").toString(), headers);

            headers.clear();
        }
    }

//    qDebug("</ready-read>");
}
