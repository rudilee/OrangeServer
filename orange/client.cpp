#include <QMetaEnum>
#include <QStringList>
#include <QSqlError>
#include <QDateTime>
#include <QHostAddress>
#include <QDebug>

#include "common.h"
#include "client.h"

Client::Client(QObject *parent) :
    QObject(parent),
    settings(new QSettings(CONFIG_FILE, QSettings::IniFormat)),
    socket(NULL),
    heartbeatTimerId(0),
    agentId(0),
    agentExtenMapId(0),
    agentSessionId(0)
{
    socketOut.setAutoFormatting(true);

    qDebug("Client initialized");
}

Client::~Client()
{
    settings->deleteLater();

    qDebug("Client destroyed");
}

void Client::setSocket(QTcpSocket *socket)
{
    this->socket = socket;
    this->socket->setParent(this);

    socketIn.setDevice(socket);
    socketOut.setDevice(socket);

    connect(socket, SIGNAL(disconnected()), SLOT(onSocketDisconnected()));
    connect(socket, SIGNAL(disconnected()), SIGNAL(socketDisconnected()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(onSocketError(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(readyRead()), SLOT(onSocketReadyRead()));

    initiateHandshake();
    resetHeartbeatTimer();
}

void Client::forceLogout()
{
    socketOut.writeStartElement("authentication");
    socketOut.writeAttribute("id", "force-logout");
    socketOut.writeTextElement("status", "server stop services");
    socketOut.writeEndElement();

    socket->write("\n");
}

void Client::timerEvent(QTimerEvent *event)
{
    socket->write("-ERR Timeout");
    socket->waitForBytesWritten(1000);
    socket->disconnectFromHost();
}

void Client::logFailedQuery(QSqlQuery *query, QString queryTitle)
{
    qWarning() << "Database query for " << queryTitle << " failed, error:" << query->lastError();
}

QString Client::getCurrentTime()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

QVariant Client::getLastInsertId(QString table, QString id)
{
    QSqlQuery retrieveId;
    QString query = QString("SELECT currval(pg_get_serial_sequence('%1', '%2'))").arg(table, id);

    if (retrieveId.exec(query)) {
        if (retrieveId.next())
            return retrieveId.value(0);
    } else {
        logFailedQuery(&retrieveId, "retrieving current id");
    }

    return QVariant();
}

void Client::initiateHandshake()
{
    if (settings->value("orange/single_quote_handshake", false).toBool())
        socket->write("<?xml version='1.0' encoding='UTF-8'?>");
    else
        socketOut.writeStartDocument();

    socketOut.writeStartElement("stream");

    socketOut.writeStartElement("welcome");
    socketOut.writeAttribute("name", "CTI Server v1.0");
    socketOut.writeTextElement("note", "Send <quit /> to close connection");
    socketOut.writeEndElement();

    socketOut.writeStartElement("authentication");
    socketOut.writeAttribute("id", "prompt");

    socketOut.writeStartElement("type");
    socketOut.writeAttribute("id", "plain");
    socketOut.writeTextElement("note", "send authentication using plain text");
    socketOut.writeEndElement();

    socketOut.writeStartElement("type");
    socketOut.writeAttribute("id", "encrypted");
    socketOut.writeTextElement("note", "send authentication encrypted");
    socketOut.writeEndElement();

    socketOut.writeEndElement();

    socket->write("\n");
}

void Client::retrieveExtension()
{
    QSqlQuery retrieveExtension;
    retrieveExtension.prepare("SELECT acd_agent_exten_map_id, extension"
                              "FROM acd_agent_exten_map "
                              "WHERE ip_address = :ip_address");

    retrieveExtension.bindValue(":ip_address", socket->peerAddress().toString());

    if (retrieveExtension.exec()) {
        if (retrieveExtension.next()) {
            QString extension = retrieveExtension.value(1).toString();

            agentExtenMapId = retrieveExtension.value(0).toUInt();

            if (!extension.isEmpty())
                socketOut.writeTextElement("extension", extension);
        }
    } else {
        logFailedQuery(&retrieveExtension, "retrieving extension");
    }
}

void Client::retrieveSkills()
{
    QSqlQuery retrieveSkills;
    retrieveSkills.prepare("SELECT acd_s.name, acd_as.acd_skill_id "
                           "FROM acd_agent_skill acd_as "
                           "LEFT JOIN acd_skill acd_s ON acd_as.acd_skill_id = acd_s.acd_skill_id "
                           "WHERE acd_as.acd_agent_id = :agent_id");

    retrieveSkills.bindValue(":agent_id", agentId);

    if (retrieveSkills.exec()) {
        socketOut.writeStartElement("transfer");

        while (retrieveSkills.next()) {
            socketOut.writeEmptyElement("skill");
            socketOut.writeAttribute("name", retrieveSkills.value(0).toString());
            socketOut.writeAttribute("id", retrieveSkills.value(1).toString());
            socketOut.writeEndElement();
        }

        socketOut.writeEndElement();
    } else {
        logFailedQuery(&retrieveSkills, "retrieving user's skills");
    }
}

void Client::startSession()
{
    QSqlQuery insertSession;
    insertSession.prepare("INSERT INTO acd_log_agent_session "
                         "(acd_agent_id, acd_agent_exten_map_id, login_time) "
                         "VALUES :agent_id, :agent_exten_map_id, :login_time");

    insertSession.bindValue(":agent_id", agentId);
    insertSession.bindValue(":agent_exten_map_id", agentExtenMapId);
    insertSession.bindValue(":login_time", getCurrentTime());

    if (insertSession.exec())
        agentSessionId = getLastInsertId("acd_log_agent_session", "acd_log_agent_session_id").toULongLong();
    else
        logFailedQuery(&insertSession, "inserting session");
}

void Client::endSession()
{
    if (agentSessionId <= 0)
        return;

    QSqlQuery updateSession;
    updateSession.prepare("UPDATE acd_log_agent_session "
                          "SET logout_time = :logout_time "
                          "WHERE acd_log_agent_session_id = :agent_session_id");

    if (!updateSession.exec())
        logFailedQuery(&updateSession, "updating session");

}

void Client::resetHeartbeatTimer()
{
    if (heartbeatTimerId > 0)
        killTimer(heartbeatTimerId);

    heartbeatTimerId = startTimer(15000);
}

void Client::checkAuthentication(QString authentication, bool encrypted)
{
    QString status = "failed",
            message = QString();

    if (encrypted)
        authentication = QByteArray::fromBase64(authentication.toLatin1());

    QStringList usernamePassword = QString(authentication).split(":");

    socketOut.writeStartElement("authentication");
    socketOut.writeAttribute("id", "status");

    QSqlQuery retrieveUser;
    retrieveUser.prepare("SELECT acd_agent_id, username, password, level "
                         "FROM acd_agent "
                         "WHERE name = :username AND password = :password");

    retrieveUser.bindValue(":username", usernamePassword[0]);
    retrieveUser.bindValue(":password", usernamePassword[1]);

    if (retrieveUser.exec()) {
        if (retrieveUser.next()) {
            agentId = retrieveUser.value(0).toUInt();

            socketOut.writeTextElement("level", retrieveUser.value(3).toString());
            socketOut.writeTextElement("login", getCurrentTime());

            retrieveExtension();
            retrieveSkills();
            startSession();
        } else {
            message = "Username/Password incorrect";
        }
    } else {
        message = "Retrieve user query error";

        logFailedQuery(&retrieveUser, "retrieving user");
    }

    socketOut.writeTextElement("status", status);

    if (!message.isEmpty())
        socketOut.writeTextElement("message", message);

    socketOut.writeEndElement();

    socket->write("\n");
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
    while (!socketIn.atEnd()) {
        QXmlStreamReader::TokenType tokenType = socketIn.readNext();

        switch (tokenType) {
        case QXmlStreamReader::StartDocument:
            break;
        case QXmlStreamReader::StartElement: {
            QString elementName = socketIn.name().toString();
            QXmlStreamAttributes attributes = socketIn.attributes();

            if (elementName == "beat") {
                socketIn.readElementText();

                resetHeartbeatTimer();
            } else if (elementName == "authentication") {
                QString authentication = socketIn.readElementText();
                bool encrypted = attributes.value("type").toString() == "encrypted";

                checkAuthentication(authentication, encrypted);
            }

            break;
        }
        case QXmlStreamReader::EndElement:
            if (socketIn.name() == "stream")
                socket->disconnectFromHost();

            break;
        case QXmlStreamReader::Invalid:
            qDebug() << socketIn.text() << socketIn.errorString();

            break;
        default: qDebug() << "Token:" << tokenType;
            break;
        }
    }
}
