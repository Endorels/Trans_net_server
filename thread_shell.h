#ifndef MULTISOCKET_H
#define MULTISOCKET_H
#include <QObject>
#include <QTcpSocket>
#include <QThread>
#include "worker.h"

struct shell_message;
class TcpServerShell;


class ThreadShell : public QThread
{
    Q_OBJECT

public:

    ThreadShell(qintptr sock,const QString &comp_name, TcpServerShell * serv_pointer);
    ~ThreadShell(){/*qDebug() << "destroyed!";*/}

    void run();

private:

    worker * current_worker;
    qintptr  socket_dscrp;
    QString comp_name_to_create;

    TcpServerShell * serv_ptr;

signals:

    //  signal to send a message to the worker from the server
    void resend_to_worker(shell_message message, QString name);

};

#endif // MULTISOCKET_H
