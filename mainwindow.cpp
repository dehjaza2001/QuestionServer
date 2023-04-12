#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{

    ui->setupUi(this);
//    nQuestion = totalPlayers;
//    question = generateQuestions(QString("C:/Users/Luong Thien Tri/Documents/QuestionServer/questions.txt"),nQuestion);
    players = 0;
    numDead = 0;
    inTurn = 0;
    currQuestion = 0;
    ui->textBrowser->append(QString("Please input number of players:"));
   // startServer();
}

MainWindow::~MainWindow()
{

    for(int i = 0 ; i < connection_set.size();i++){
        connection_set[i]->close();
        connection_set[i]->deleteLater();
    }
    m_server->close();
    m_server->deleteLater();
    delete ui;
}

QStringList MainWindow::generateQuestions(const QString& filename, int num){
   QStringList question;
   QStringList allQuestions;
   QFile file(filename);
   if (!file.open(QIODevice::ReadOnly | QIODevice::Text)){
       qDebug() << "can not open file";
       displayMessage(QString("Can not open file, please try again"));
       return question;

   }
   QTextStream in(&file);
   while (!in.atEnd()) {
       QString line = in.readLine();
       allQuestions.append(line);
   }

   file.close();

   QRandomGenerator generator(QTime::currentTime().msec());
   std::unordered_set<int> chosenIndices; // Set to store chosen indices
   while (chosenIndices.size() < num) {
       int randomIndex = generator.bounded(allQuestions.size()); // Generate random index
       chosenIndices.insert(randomIndex); // Add index to set
   }

   for (int index : chosenIndices) {
       question.append(allQuestions[index]); // Add randomly chosen questions to list
       }
    qDebug() << QString("Loaded %1 questions").arg(question.size());
   return question;
}

void MainWindow::startServer(){
    m_server = new QTcpServer();

     if(m_server->listen(QHostAddress::Any, 8080))
        {
           connect(this, &MainWindow::newName, this, &MainWindow::checkName);
           //connect(this, &MainWindow::newMessage, this, &MainWindow::displayMessage);
           connect(this, &MainWindow::newMessage, this, &MainWindow::checkAnswer);
           connect(this, &MainWindow::newMessage, this, &MainWindow::updateStatus);
           connect(this, &MainWindow::goOutQuestion, this, &MainWindow::endGame);
           connect(m_server, &QTcpServer::newConnection, this, &MainWindow::newConnection);
           ui->statusbar->showMessage("Server is listening...");
        }
        else
        {
            QMessageBox::critical(this,"QTCPServer",QString("Unable to start the server: %1.").arg(m_server->errorString()));
            exit(EXIT_FAILURE);
        }

}

void MainWindow::newRound(){
    numDead = 0;
    inTurn = 0;
    for(auto it = isDead.begin(); it != isDead.end() ; ++it){
        it->second = false;
    }
    currQuestion = 0;
    nQuestion = 3;
    question = generateQuestions(QString("questions.txt"),nQuestion);
}


void MainWindow::displayMessage(const QString& str)
{
    ui->textBrowser->append(str);
}

void MainWindow::newConnection()
{
    while(m_server->hasPendingConnections()){
        appendToSocketList(m_server->nextPendingConnection());
    }
}

void MainWindow::appendToSocketList(QTcpSocket* socket)
{
    connection_set.push_back(socket);
    //isDead.insert(std::make_pair(socket, false));
    isDead[socket] = false;
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::discardSocket);
    connect(socket, &QAbstractSocket::errorOccurred, this, &MainWindow::displayError);

    // Show num of player:
    players++;
    ui->label->setText(QString("Number of players: %1").arg(players));
    displayMessage(QString("INFO :: New players has just entered the room"));
}

void MainWindow::readSocket()
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

void MainWindow::checkName(QTcpSocket *socket, const QString &playername){
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
       sendMessage(socket,QString("OK;%1;%2").arg(id + 1).arg(nQuestion),QString("message"));
       for (const auto& [key, value] : nameMap) {
              qDebug() << key << " : " << value;
          }
       ui->textBrowser->append(QString("%1 was registered").arg(playername));
   }
   if(connection_set.size() == nameMap.size()){
       displayMessage(QString("All players has registered their name"));
   }
}

void MainWindow::discardSocket()
{
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());
    displayMessage(QString("INFO :: A client has just left the room"));
    connection_set.erase(std::remove(connection_set.begin(),connection_set.end(),socket),connection_set.end());
    players--;
    inTurn = inTurn < players ? inTurn : inTurn -1;
    nameMap.erase(socket);
    isDead.erase(socket);
    ui->label->setText(QString("Number of players: %1").arg(players));
    socket->deleteLater();
}

void MainWindow::displayError(QAbstractSocket::SocketError socketError)
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

void MainWindow::on_startButton_clicked()
{
    gameplay();
}

void MainWindow::sendMessage(QTcpSocket* socket, QString str, QString fileType)
{
    if(socket)
    {
        if(socket->isOpen())
        {
            //qDebug() << "Go in Socket" <<  str << fileType;
            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            QByteArray header;
            header.prepend(QString("fileType:%1,fileName:null,fileSize:%2;").arg(fileType).arg(str.size()).toUtf8());
            header.resize(128);

            QByteArray byteArray = str.toUtf8();
            byteArray.prepend(header);

            socketStream.setVersion(QDataStream::Qt_5_15);
            socketStream << byteArray;

            displayMessage(QString("Server sent: %1").arg(str));
            qDebug() << "Sent" <<  str << fileType;
            QCoreApplication::processEvents();
            while (socket->bytesToWrite() > 0)
            {
                QCoreApplication::processEvents();
                QThread::msleep(20);
            }

        }
        else
            QMessageBox::critical(this,"QuestionServer","Socket doesn't seem to be opened");
    }
    else
        QMessageBox::critical(this,"QuestionServer","Not connected");
}

void MainWindow::sendMessageToAll(QString str, QString fileType)
{
    for(int i = 0 ; i < connection_set.size(); i++){
        if(isDead[connection_set[i]] == false)
            sendMessage(connection_set[i],str, fileType);
    }
}

void MainWindow::gameplay()
{
    if(connection_set.size() == 1 || connection_set.size() != nameMap.size() || players < totalPlayers){
        QMessageBox::warning(this,"Invalid","Can start the game now");
        return;
    }

    ui->startButton->setDisabled(true);
    sendMessageToAll(QString("go"),QString("start"));
    iniStatus();
    //
    while(currQuestion < nQuestion){
        for(int i = 0 ; i < connection_set.size() ; i++){
            if(numDead == players - 1){
                emit goOutQuestion();
                return;
            }
            if(!isDead[connection_set[i]]){
                inTurn = i + 1;
                QString ques = question[currQuestion].split(";")[0];
                sendMessage(connection_set[i],ques,QString("question"));
                displayMessage(QString("Sent question %1 : %2").arg(currQuestion).arg(ques));
                // wait for answer
                waitForAnswer();
            }
        }
        currQuestion++;
        qDebug() << "current question" << currQuestion;
    }
    qDebug() << "out of question";
    emit goOutQuestion();
}

void MainWindow::waitForAnswer(){
    QEventLoop loop;
    // connect a slot to a signal that will resume the event loop
    connect(this, MainWindow::continueSend, &loop, &QEventLoop::quit);
    // start the event loop
    loop.exec();
}

void MainWindow::checkAnswer(QTcpSocket* socket, const QString& str)
{
    qDebug() << "Go checkAnswer";
    qDebug() << "client_ans: " << str;

    QString ans = question[currQuestion].split(";")[1];
    displayMessage(QString("Player %1 answered : %2").arg(nameMap[socket]).arg(str));
    if(str == QString("SKIP")){
         sendMessage(socket,QString("CORRECT"),("result"));
         displayMessage(QString("Player %1 used SKIP").arg(nameMap[socket]));
    } else if (str == ans){
        sendMessage(socket,QString("CORRECT"),("result"));
        displayMessage(QString("Correct"));
    } else  {
        numDead++;
        qDebug() << "dead: " << isDead[socket];
        isDead[socket] = true;
        sendMessage(socket,QString("WRONG"),("result"));
        displayMessage(QString("Player %1 is wrong, eliminated").arg(nameMap[socket]));
    }
}

void MainWindow::endGame()
{
    // sort the vector by value

    QString winner = "";
    for(int i = 0 ; i < connection_set.size() ; i++){
        if(!isDead[connection_set[i]]){
            winner += nameMap[connection_set[i]];
            winner += " ";
        }
    }

    displayMessage(winner);
    ui->startButton->setDisabled(false);
    sendMessageToAll(winner,QString("winner"));
    newRound();
}

void MainWindow::updateStatus()
{
    // sort the vector by value

    QString message = "";

    for(int i = 0 ; i < connection_set.size(); i++){
        QTcpSocket* socket = connection_set[i];
        if(isDead[socket]){
             message += QString("%2.<s>Player: %1</s> <br>").arg(nameMap[socket]).arg(i+1);
        } else if (i == (inTurn % players)){
            message += QString("%2.Player: %1 (in turn) <br>").arg(nameMap[socket]).arg(i+1);
        } else {
            message += QString("%2.Player: %1 <br>").arg(nameMap[socket]).arg(i+1);
        }
    }

    qDebug ()<< message;
    sendMessageToAll(message, QString("status"));
    emit continueSend();
    qDebug() << "after signal";
}

void MainWindow::iniStatus()
{
    // sort the vector by value

    QString message = "";

    for(int i = 0 ; i < connection_set.size(); i++){
        QTcpSocket* socket = connection_set[i];
        if (i == 0){
            message += QString("%2.Player: %1 (in turn) <br>").arg(nameMap[socket]).arg(i+1);
        } else {
            message += QString("%2.Player: %1 <br>").arg(nameMap[socket]).arg(i+1);
        }
    }

    qDebug ()<< message;
    sendMessageToAll(message, QString("status"));
    emit continueSend();
    qDebug() << "after signal";
}



void MainWindow::on_lineEdit_returnPressed()
{
    totalPlayers = ui->lineEdit->text().toInt();
    ui->textBrowser->append(QString("Wait for %1 players to connect").arg(totalPlayers));
    nQuestion = totalPlayers * 3;
    ui->textBrowser->append(QString("There are %1 question in this set").arg(nQuestion));
    question = generateQuestions(QString("C:/Users/Luong Thien Tri/Documents/QuestionServer/questions.txt"),nQuestion);
    startServer();
}

