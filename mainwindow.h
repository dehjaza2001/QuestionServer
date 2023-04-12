#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
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
#include<QFile>
#include<QTextStream>
#include<QEventLoop>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include<QRandomGenerator>
#include<unordered_set>
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void newMessage(QTcpSocket*, QString);
    void newName(QTcpSocket*, QString);
    void goOutQuestion();
    void continueSend();
//    void wrongAnswer();
//    void rightAnswer();
private slots:
    void newConnection();
    void appendToSocketList(QTcpSocket* socket);

    void readSocket();
    void discardSocket();
    void displayError(QAbstractSocket::SocketError socketError);
    void displayMessage(const QString& str);
    void sendMessage(QTcpSocket* socket, QString str, QString fileType);
    void sendMessageToAll(QString str, QString fileType);

    void on_startButton_clicked();

    void gameplay();
    void checkAnswer(QTcpSocket* socket, const QString& str);
    void updateStatus();

    QStringList generateQuestions(const QString& filename, int num);
    void checkName(QTcpSocket* socket, const QString& playername);

    void endGame();
    void waitForAnswer();
    void startServer();
    void newRound();
    void iniStatus();
    void on_lineEdit_returnPressed();

private:
    Ui::MainWindow *ui;
    QTcpServer* m_server;
    std::vector<QTcpSocket*> connection_set;
    QStringList question;
    std::map<QTcpSocket*, bool> isDead;
    std::map<QTcpSocket*, QString> nameMap;
    int players;
    int totalPlayers;
    int numDead;
    int inTurn;
    int currQuestion;
    int nQuestion;
};
#endif // MAINWINDOW_H
