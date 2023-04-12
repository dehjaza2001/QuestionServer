#include "server.h"

Server::Server()
{
    players = 0;
    numDead = 0;
    inTurn = 0;
    currQuestion = 0;
    question = generateQuestions("question.txt");
    // Start the server

}

void Server::startServer(){
    m_server = new QTcpServer();
    if(m_server->listen(QHostAddress::Any, 8080))
        {
           connect(this, &MainWindow::newName, this, &MainWindow::checkName);
           connect(this, &MainWindow::newMessage, this, &MainWindow::displayMessage);
           connect(this, &MainWindow::newMessage, this, &MainWindow::checkAnswer);
           connect(this, &MainWindow::newMessage, this, &MainWindow::updateStatus);
           connect(this, &MainWindow::goOutQuestion, this, &MainWindow::endGame);
           connect(m_server, &QTcpServer::newConnection, this, &MainWindow::newConnection);
        }
    else
        {
           QMessageBox::critical(this,"QTCPServer",QString("Unable to start the server: %1.").arg(m_server->errorString()));
           exit(EXIT_FAILURE);
        }
}

Server::~Server()
{
        for(int i = 0 ; i < connection_set.size();i++){
            connection_set[i]->close();
            connection_set[i]->deleteLater();
        }
        m_server->close();
        m_server->deleteLater();
}

QStringList Server::generateQuestions(const std::string& filename){
   QStringList question;
   std::ifstream infile(filename);

   if(!infile.is_open()){
       qDebug() << "can not open file";
       return question;
   }

   std::string line;
   while(std::getline(infile,line)){
       question.append(QString::fromStdString(line));
   }

   infile.close();
   qDebug() << QString("Loaded %1 questions").arg(question.size());
   return question;
}

void Server::newConnection()
{
    while(m_server->hasPendingConnections()){
        appendToSocketList(m_server->nextPendingConnection());
    }
}

void Server::appendToSocketList(QTcpSocket* socket)
{
    connection_set.push_back(socket);
    isDead[socket] = false;
    connect(socket, &QTcpSocket::readyRead, this, &Server::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, &Server::discardSocket);
    connect(socket, &QAbstractSocket::errorOccurred, this, &Server::displayError);

    // Show num of player:
    players++;
    //ui->label->setText(QString("Number of players: %1").arg(players));
    displayMessage(socket, QString("INFO :: Client with sockd:%1 has just entered the room").arg(socket->socketDescriptor()));
}

//void Server::displayMessage(const QString& str)
//{
//    ui->textBrowser->append(str);
//}

void Server::sendMessage(QTcpSocket* socket, QString str, QString fileType)
{
    if(socket)
    {
        if(socket->isOpen())
        {
            qDebug() << "Go in Socket" <<  str << fileType;
            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            QByteArray header;
            header.prepend(QString("fileType:%1,fileName:null,fileSize:%2;").arg(fileType).arg(str.size()).toUtf8());
            header.resize(128);

            QByteArray byteArray = str.toUtf8();
            byteArray.prepend(header);

            socketStream.setVersion(QDataStream::Qt_5_15);
            socketStream << byteArray;
            qDebug() << "sent message" << str << fileType;
            QCoreApplication::processEvents();
            while (socket->bytesToWrite() > 0)
            {
                QCoreApplication::processEvents();
                QThread::msleep(10);
            }
        }
        else
            QMessageBox::critical(this,"QuestionServer","Socket doesn't seem to be opened");
    }
    else
        QMessageBox::critical(this,"QuestionServer","Not connected");
}

void Server::sendMessageToAll(QString str, QString fileType)
{

    for(int i = 0 ; i < connection_set.size(); i++){
        if(isDead[connection_set[i]] == false)
            sendMessage(connection_set[i],str, fileType);
    }
}

void Server::playGame()
{
    ui->pushButton->setDisabled(true);
    updateStatus();
    while(currQuestion < nQuestion || numDead < players){
        if(inTurn == players){
            inTurn = 0;
            currQuestion++;
        }

        QString ques = question[currQuestion].split(";")[0];
        QString ans = question[currQuestion].split(";")[1];

        qDebug() << ques << " : " << ans;

        sendMessage(connection_set[inTurn],ques,QString("question"));
        waitForAnswer(connection_set[inTurn]);
    }

    emit goOutQuestion();
    return;
}

void Server::waitForAnswer(QTcpSocket* socket){
    while(socket->bytesAvailable() < 2) {
            if (!socket->waitForReadyRead()) {
                //qDebug() << "Error: waitForReadyRead failed";
                return;
            }
        }
}

void Server::checkAnswer(QTcpSocket* socket, const QString& str)
{
    qDebug() << "Go checkAnswer";
    qDebug() << "client_ans: " << str;

//    if (client_ans == "TimeOut") {
//        qDebug() << "player time out " << isDead[socket];
//        isDead[socket] = true;
//        sendMessage(socket,QString("DEAD"),("result"));
//        return;
//    }

    QString ans = question[currQuestion].split(";")[1];
    qDebug() << "correct ans " << ans;
    if(str == QString("SKIP") || str == ans){
        sendMessage(socket,QString("CORRECT"),("result"));
    } else {
        qDebug() << "dead: " << isDead[socket];
        isDead[socket] = true;
        sendMessage(socket,QString("WRONG"),("result"));
    }
    inTurn++;
}

void Server::endGame()
{
    // sort the vector by value
    std::vector<std::pair<QTcpSocket*, bool>> status_list(isDead.begin(), isDead.end());
    QString winner = "";
    for(int i = 0 ; i < status_list.size(); i++){
        winner += " ";
        winner += nameMap[status_list[i].first];
    }

    QString message = QString("%1 are the millionare").arg(winner);
    sendMessageToAll(message,QString("status"));
}

void Server::updateStatus()
{
    // sort the vector by value
    QString message = "";
    for(int i = 0 ; i < connection_set.size(); i++){
        QTcpSocket* socket = connection_set[i];
        if(isDead[socket]){
             message += QString("<s>Player: %1</s> \n").arg(nameMap[socket]);
        } else if (i == inTurn){
            message += QString("Player: %1 (in turn) \n").arg(nameMap[socket]);
        } else {
            message += QString("Player: %1 \n").arg(nameMap[socket]);
        }
    }

    qDebug ()<< message;

    sendMessageToAll(message, QString("status"));
}

void Server::readSocket()
{
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());

    QByteArray buffer;

    QDataStream socketStream(socket);
    socketStream.setVersion(QDataStream::Qt_5_15);

    socketStream.startTransaction();
    socketStream >> buffer;

    if(!socketStream.commitTransaction())
    {
        QString message = QString("%1 :: Waiting for more data to come..").arg(socket->socketDescriptor());
        emit newMessage(socket, message);
        return;
    }

    QString header = buffer.mid(0,128);
    QString fileType = header.split(",")[0].split(":")[1];

    buffer = buffer.mid(128);

    QString message = QString::fromStdString(buffer.toStdString());

    if(fileType=="answer"){
        emit newMessage(socket, message);
    } else if(fileType == "name"){
        emit newName(socket,message);
    }
}

void Server::checkName(QTcpSocket *socket, const QString &playername){
    qDebug() << "Go check Name";
    qDebug() << "player name " << playername;
    auto its = std::find_if(nameMap.begin(), nameMap.end(),
           [playername](const auto& p) { return p.second == playername; });

   if(its != nameMap.end()){
       sendMessage(socket,QString("NOK"),QString("message"));
   } else {
       nameMap[socket] = playername;
       auto it = std::find(connection_set.begin(), connection_set.end(), socket);
       int id = distance(connection_set.begin(), it);
       sendMessage(socket,QString("OK;%1").arg(id + 1),QString("message"));
       for (const auto& [key, value] : nameMap) {
              qDebug() << key << " : " << value;
          }
       ui->textBrowser->append(QString("%1 was registered").arg(playername));
   }

}

void Server::discardSocket()
{
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());
    //displayMessage(socket, QString("INFO :: A client has just left the room").arg(socket->socketDescriptor()));
    connection_set.erase(std::remove(connection_set.begin(),connection_set.end(),socket),connection_set.end());
    players--;
    nameMap.erase(socket);
    isDead.erase(socket);
    ui->label->setText(QString("Number of players: %1").arg(players));
    socket->deleteLater();
}

void Server::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
        case QAbstractSocket::RemoteHostClosedError:
        break;
        case QAbstractSocket::HostNotFoundError:
            QMessageBox::information(this, "QuestionServer", "The host was not found. Please check the host name and port settings.");
        break;
        case QAbstractSocket::ConnectionRefusedError:
            QMessageBox::information(this, "QuestionServer", "The connection was refused by the peer. Make sure QTCPServer is running, and check that the host name and port settings are correct.");
        break;
        default:
            QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
            QMessageBox::information(this, "QuestionServer", QString("The following error occurred: %1.").arg(socket->errorString()));
        break;
    }
}

