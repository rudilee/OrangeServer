#include <QMetaEnum>
#include <QStringList>
#include <QSqlError>
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
    agentLogStatusId(0),
    handle(0),
    abandoned(0)
{
    qRegisterMetaType<Client::Status>("Client::Status");

    socketOut.setAutoFormatting(true);

    statusText["ready"] = Ready;
    statusText["not-ready"] = NotReady;
    statusText["acw"] = ACW;
    statusText["aux"] = AUX;

    statusText["free"] = Free;
    statusText["initiate"] = Initiate;
    statusText["originate"] = Originate;
    statusText["ringing"] = Ringing;
    statusText["busy"] = Busy;
    statusText["release-callee"] = ReleaseCallee;

    qDebug("Client initialized");
}

Client::~Client()
{
    settings->deleteLater();

    qDebug("Client destroyed");
}

QString Client::getIpAddress()
{
    return socket->peerAddress().toString();
}

QString Client::getUsername()
{
    return username;
}

QString Client::getFullname()
{
    return fullname;
}

Client::Level Client::getLevel()
{
    return level;
}

Client::Phone Client::getPhone()
{
    return phone;
}

QStringList Client::getGroups()
{
    return groups;
}

int Client::getHandle()
{
    return handle;
}

void Client::setHandle(int handle)
{
    if (handle > 0)
        this->handle = handle;
}

int Client::getAbandoned()
{
    return abandoned;
}

void Client::setAbandoned(int abandoned)
{
    if (abandoned > 0)
        this->abandoned = abandoned;
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

QString Client::getExtension()
{
    return extension;
}

void Client::setExtension(QString extension)
{
    this->extension = extension;

    emit userExtensionChanged(extension);
}

void Client::forceLogout(QString status)
{
    if (agentId <= 0)
        return;

    socketOut.writeStartElement("authentication");
    socketOut.writeAttribute("id", "force-logout");
    socketOut.writeTextElement("status", status);
    socketOut.writeEndElement();

    socket->write("\n");
    socket->flush();
    socket->disconnectFromHost();
}

void Client::changeStatus(Client::Status status)
{
    endStatus();
    startStatus(status);

    emit userStatusChanged(status);
}

void Client::changePhoneStatus(QString status, bool outbound)
{
    phone.time = QDateTime::currentDateTime();
    phone.status = status;
    phone.outbound = outbound;

    sendAgentStatus(username, fullname, phone, handle, abandoned);

    emit phoneStatusChanged(status);

    qDebug() << "Phone status of" BOLD BLUE << username << RESET "changed to:" BOLD BLUE << status << RESET;
}

void Client::sendAgentStatus(QString username, QString fullname, Client::Phone phone, int handle, int abandoned, QString group, QDateTime login, QString address, QString extension)
{
    bool groupEmpty = group.isEmpty();

    socketOut.writeStartElement("agent");

    socketOut.writeTextElement("username", username);
    socketOut.writeTextElement("fullname", fullname);

    if (!groupEmpty)
        socketOut.writeTextElement("group", group);

    socketOut.writeTextElement("handle", QString::number(handle));
    socketOut.writeTextElement("abandoned", QString::number(abandoned));

    if (login.isValid())
        socketOut.writeTextElement("login", login.toString("yyyy-MM-dd HH:mm:ss"));

    socketOut.writeTextElement("time", phone.time.toString("yyyy-MM-dd HH:mm:ss"));

    if (!address.isEmpty())
        socketOut.writeTextElement("address", address);

    if (!extension.isEmpty())
        socketOut.writeTextElement("extension", extension);

    socketOut.writeStartElement("phone");
    socketOut.writeAttribute("status", phone.status);
    socketOut.writeAttribute("outbound", phone.outbound ? "true" : "false");

    if (!groupEmpty)
        socketOut.writeAttribute("group", group);

    if (!phone.channel.isEmpty()) {
        socketOut.writeAttribute(phone.active ? "activechannel" : "passivechannel", phone.channel);
    }

    if (!phone.dnis.isEmpty()) {
        socketOut.writeEmptyElement(phone.active ? "callee" : "caller");
        socketOut.writeAttribute("dnis", phone.dnis);
    }

    socketOut.writeEndElement(); // phone

    socketOut.writeEndElement(); // agent

    socket->write("\n");
}

void Client::sendAgentLogout(QString username, QString extension, QString group, QString address)
{
    socketOut.writeStartElement("agent");
    socketOut.writeTextElement("username", username);
    socketOut.writeTextElement("extension", extension);
    socketOut.writeTextElement("group", group);
    socketOut.writeTextElement("address", address);
    socketOut.writeEmptyElement("logout");
    socketOut.writeEndElement(); // logout
    socketOut.writeEndElement(); // agent
}

void Client::sendDialerResponse(QString formattedNumber)
{
    socketOut.writeStartElement("dialer");
    socketOut.writeAttribute("formatted-number", formattedNumber);
    socketOut.writeEndElement();

    socket->write("\n");
}

void Client::timerEvent(QTimerEvent *event)
{
    socket->write("-ERR Timeout\n");
    socket->flush();
    socket->disconnectFromHost();

    Q_UNUSED(event)
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
            agentExtenMapId = retrieveExtension.value(0).toUInt();

            setExtension(retrieveExtension.value(1).toString());

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
        }

        socketOut.writeEndElement(); // transfer
    } else {
        logFailedQuery(&retrieveSkills, "retrieving user's skills");
    }
}

void Client::retrieveGroups()
{
    QSqlQuery retrieveGroups;
    retrieveGroups.prepare("SELECT name "
                           "FROM acd_agent_group acd_ag "
                           "LEFT JOIN acd_queue acd_q ON acd_ag.acd_queue_id = acd_q.acd_queue_id "
                           "WHERE acd_ag.acd_agent_id = :agent_id");

    retrieveGroups.bindValue(":agent_id", agentId);

    if (retrieveGroups.exec()) {
        groups.clear();

        while (retrieveGroups.next()) {
            groups << retrieveGroups.value(0).toString();
        }
    } else {
        logFailedQuery(&retrieveGroups, "retrieving user's groups");
    }
}

void Client::startSession()
{
    QSqlQuery insertSession;
    insertSession.prepare("INSERT INTO acd_log_agent_session (acd_agent_id, acd_agent_exten_map_id, login_time) "
                          "VALUES (:agent_id, :agent_exten_map_id, :login_time)");

    insertSession.bindValue(":agent_id", agentId);
    insertSession.bindValue(":agent_exten_map_id", agentExtenMapId <= 0 ? QVariant() : agentExtenMapId);
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

    heartbeatTimerId = startTimer(20000);
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
    retrieveUser.prepare("SELECT acd_agent_id, name, password, fullname, level "
                         "FROM acd_agent "
                         "WHERE name = :username AND password = :password");

    retrieveUser.bindValue(":username", usernamePassword[0]);
    retrieveUser.bindValue(":password", hashedPassword);

    if (retrieveUser.exec()) {
        if (retrieveUser.next()) {
            username = usernamePassword[0];
            fullname = retrieveUser.value(3).toString();
            level = (Level) retrieveUser.value(4).toUInt();
            agentId = retrieveUser.value(0).toUInt();
            status = "ok";

            socketOut.writeTextElement("level", QString::number(level));
            socketOut.writeTextElement("login", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));

            if (!extension.isEmpty())
                socketOut.writeTextElement("extension", extension);
            else
                retrieveExtension();

            retrieveSkills();
            retrieveGroups();
            startSession();
            startStatus(Login);

            emit userLoggedIn();
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

    socketOut.writeEndElement(); // authentication

    socket->write("\n");
}

void Client::dispatchAction(QString actionType, QXmlStreamAttributes attributes)
{
    if (actionType == "ready") {
        bool outbound = attributes.value("outbound").toString() == "true",
             ready = attributes.value("value").toString() == "true";

        QString status = ready ? "ready" : attributes.value("mode").toString();

        changeStatus(statusText.value(status));
        changePhoneStatus(status, outbound);
    } else if (actionType == "ask-dial-authorization") {
        QString customerId = attributes.value("customerid").toString(),
                destination = attributes.value("destination").toString(),
                campaign = attributes.value("campaign").toString();

        emit askDialAuthorization(destination, customerId, campaign);
    } else if (actionType == "spy") {
        QString agent = attributes.value("agent").toString();

        emit spyAgentPhone(agent);
    } else if (actionType == "status") {
        bool outbound = attributes.value("outbound").toString() == "true",
             ready = attributes.value("ready").toString() == "true";

        QString group = attributes.value("group").toString(),
                extension = attributes.value("extension").toString();

        emit changeAgentStatus(ready ? Ready : NotReady, outbound, extension);

        Q_UNUSED(group)
    }
}

void Client::onSocketDisconnected()
{
    disconnect(socket);

    socket->deleteLater();

    if (!username.isEmpty()) {
        endLogging();

        emit userLoggedOut();
    }

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
                QString actionType = attributes.value("type").toString();

                if (socketIn.readNextStartElement()) {
                    if (socketIn.name().toString() == actionType)
                        dispatchAction(actionType, socketIn.attributes());
                }
            }

            break;
        }
        case QXmlStreamReader::EndElement:
            if (socketIn.name() == "stream")
                socket->disconnectFromHost();

            break;
        case QXmlStreamReader::Invalid:
//            qDebug() << socketIn.errorString();
            break;
        default:
//            qDebug() << "Token:" << tokenType;
            break;
        }
    }
}
