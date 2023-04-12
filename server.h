#ifndef SERVER_H
#define SERVER_H
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QMetaType>
#include <QSet>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QStringList>
#include "unistd.h"
#include <algorithm>
#include <QThread>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

class Server
{
public:
    Server();
    void startServer();
    QStringList generateQuestions(const std::string& filename);
    void newConnection();
    void appendToSocketList(QTcpSocket* socket);
    void readSocket();
    void discardSocket();
    void displayError(QAbstractSocket::SocketError socketError);
    void displayMessage(QTcpSocket* socket, const QString& str);
    void sendMessage(QTcpSocket* socket, QString str, QString fileType);
    void sendMessageToAll(QString str, QString fileType);
    void playGame();
    void checkAnswer(QTcpSocket* socket, const QString& str);
    void updateStatus();
    void checkName(QTcpSocket* socket, const QString& playername);
    void endGame();
    void waitForAnswer(QTcpSocket* socket);
    void newRound();
    ~Server();
private:
    QTcpServer* m_server;
    std::vector<QTcpSocket*> connection_set;
    QStringList question;
    std::map<QTcpSocket*, bool> isDead;
    std::map<QTcpSocket*, QString> nameMap;
    int players;
    int numDead;
    int inTurn;
    int currQuestion;
    int nQuestion;
};

#endif // SERVER_H
