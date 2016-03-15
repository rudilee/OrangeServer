#include <QMetaEnum>
#include <QStringList>
#include <QSqlError>
#include <QDateTime>
#include <QHostAddress>
#include <QCryptographicHash>
#include <QDebug>

#include "common.h"
#include "terminal.h"
#include "client.h"

Client::Client(QObject *parent) :
    QObject(parent),
    settings(new QSettings(CONFIG_FILE, QSettings::IniFormat)),
    socket(NULL),
    heartbeatTimerId(0),
    agentId(0),
    agentExtenMapId(0),
    agentLogSessionId(0),
    agentLogStatusId(0)
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
    if (agentId <= 0)
        return;

    socketOut.writeStartElement("authentication");
    socketOut.writeAttribute("id", "force-logout");
    socketOut.writeTextElement("status", "server stop services");
    socketOut.writeEndElement();

    socket->write("\n");

    endLogging();
}

void Client::timerEvent(QTimerEvent *event)
{
    socket->write("-ERR Timeout");
    socket->waitForBytesWritten(1000);
    socket->disconnectFromHost();
}

void Client::logFailedQuery(QSqlQuery *query, QString queryTitle)
{
    qCritical() << "Database query for" << queryTitle << "failed, error:" BOLD CYAN << query->lastError() << RESET;
}

QVariant Client::getLastInsertId(QString table, QString column)
{
    QSqlQuery retrieveId;
    QString query = QString("SELECT currval(pg_get_serial_sequence('%1', '%2'))").arg(table, column);

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
    retrieveExtension.prepare("SELECT acd_agent_exten_map_id, extension "
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
    insertSession.prepare("INSERT INTO acd_log_agent_session (acd_agent_id, acd_agent_exten_map_id, login_time) "
                          "VALUES (:agent_id, :agent_exten_map_id, :login_time)");

    insertSession.bindValue(":agent_id", agentId);
    insertSession.bindValue(":agent_exten_map_id", agentExtenMapId);
    insertSession.bindValue(":login_time", QDateTime::currentDateTime());

    if (insertSession.exec())
        agentLogSessionId = getLastInsertId("acd_log_agent_session", "acd_log_agent_session_id").toULongLong();
    else
        logFailedQuery(&insertSession, "inserting session log");
}

void Client::endSession()
{
    if (agentLogSessionId <= 0)
        return;

    QSqlQuery updateSession;
    updateSession.prepare("UPDATE acd_log_agent_session "
                          "SET logout_time = :logout_time "
                          "WHERE acd_log_agent_session_id = :agent_log_session_id");

    updateSession.bindValue(":logout_time", QDateTime::currentDateTime());
    updateSession.bindValue(":agent_log_session_id", agentLogSessionId);

    if (updateSession.exec())
        agentLogSessionId = 0;
    else
        logFailedQuery(&updateSession, "updating session log");
}

void Client::startStatus(Status status)
{
    this->status = status;

    QSqlQuery insertStatus;
    insertStatus.prepare("INSERT INTO acd_log_agent_status (acd_log_agent_session_id, acd_agent_status_id, start) "
                         "VALUES (:agent_log_session_id, :status, :start)");

    insertStatus.bindValue(":agent_log_session_id", agentLogSessionId);
    insertStatus.bindValue(":status", (quint16) status);
    insertStatus.bindValue(":start", QDateTime::currentDateTime());

    if (insertStatus.exec())
        agentLogStatusId = getLastInsertId("acd_log_agent_status", "acd_log_agent_status_id").toULongLong();
    else
        logFailedQuery(&insertStatus, "inserting status log");
}

void Client::changeStatus(Client::Status status)
{
    endStatus();
    startStatus(status);
}

void Client::endStatus()
{
    if (agentLogStatusId <= 0)
        return;

    QSqlQuery updateStatus;
    updateStatus.prepare("UPDATE acd_log_agent_status "
                         "SET finish = :finish "
                         "WHERE acd_log_agent_status_id = :agent_log_status_id");

    updateStatus.bindValue(":finish", QDateTime::currentDateTime());
    updateStatus.bindValue(":agent_log_status_id", agentLogStatusId);

    if (updateStatus.exec())
        agentLogStatusId = 0;
    else
        logFailedQuery(&updateStatus, "updating status log");
}

void Client::endLogging()
{
    if (status != Logout)
        changeStatus(Logout);

    endSession();
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
    QString hashedPassword = QCryptographicHash::hash(usernamePassword[1].toLatin1(), QCryptographicHash::Md5).toHex();

    socketOut.writeStartElement("authentication");
    socketOut.writeAttribute("id", "status");

    QSqlQuery retrieveUser;
    retrieveUser.prepare("SELECT acd_agent_id, name, password, level "
                         "FROM acd_agent "
                         "WHERE name = :username AND password = :password");

    retrieveUser.bindValue(":username", usernamePassword[0]);
    retrieveUser.bindValue(":password", hashedPassword);

    if (retrieveUser.exec()) {
        if (retrieveUser.next()) {
            username = usernamePassword[0];
            level = (Level) retrieveUser.value(3).toUInt();
            agentId = retrieveUser.value(0).toUInt();
            status = "ok";

            socketOut.writeTextElement("level", QString::number(level));
            socketOut.writeTextElement("login", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

            retrieveExtension();
            retrieveSkills();
            startSession();
            startStatus(Login);
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

    endLogging();

    qDebug("Client disconnected");
}

void Client::onSocketError(QAbstractSocket::SocketError socketError)
{
    int indexOfSocketError = QAbstractSocket::staticMetaObject.indexOfEnumerator("SocketError");
    QString socketErrorKey = QAbstractSocket::staticMetaObject.enumerator(indexOfSocketError).key(socketError);

    qWarning() << "Client connection error:" BOLD CYAN << socketErrorKey << RESET;
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
            } else if (elementName == "action") {
                // TODO: proses action dan children tagnya
            }

            break;
        }
        case QXmlStreamReader::EndElement:
            if (socketIn.name() == "stream") {
                socket->disconnectFromHost();

                endLogging();
            }

            break;
        case QXmlStreamReader::Invalid:
            qDebug() << socketIn.text() << socketIn.errorString();

            break;
        default: qDebug() << "Token:" << tokenType;
            break;
        }
    }
}
