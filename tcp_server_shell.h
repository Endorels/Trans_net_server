#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QThread>
#include <QLinkedList>
#include <QTimer>
#include <QDataStream>
#include <QMutex>
#include "worker.h"
#include "thread_shell.h"

struct shell_message;

class TcpServerShell: public QTcpServer
{
    Q_OBJECT

public:
    TcpServerShell();

private:

    struct computers_buffer
    {
        QString comp_name;
        ThreadShell * thread;
        QLinkedList<shell_message> buffer;
    };

    QMutex * mutex_delete_buffer;
    QTcpServer * server;
    QTimer * timer_live_time;

    QLinkedList<shell_message> buffer_cutted;
    QLinkedList<computers_buffer> * data_buffers;

    int size_of_header = 37;
    int number_of_names = 1;

    QByteArray name_word = "My name is ";
    QByteArray sep_message = "!@#";     // сепаратор для сообщений


    //  creating sockets on separate threads
    void incomingConnection(qintptr handle) override;

    //  function of creating structure information template
    void creating_shell_struct(const QByteArray * info, shell_message &shell_message);

    //  function of clearing the buffer by message lifetime
    void check_old_messages();


public slots:

    // slot for saving the caller's buffer before deleting it to save messages
    void save_buffer_from_connected_comp(QString comp_name, QLinkedList<shell_message> buff);

    //  slot for sending signals about the transfer of messages remaining in the buffer to the subscriber after reconnection
    void send_message(QString name, shell_message pack_mess);

    //  slot for changing the subscriber's name in the list of structures
    void changing_comp_name(QString old_name, QString new_name, ThreadShell * ms_ptr);

    // setting the stream to nullptr in the client's shared buffer
    void clearing_thread(QString comp_name);

signals:

    //  signal to send a structure with a message to the subscriber
    void send_message_to_abonent(shell_message message, QString comp_name);

    // signal to the subscriber to read messages from the socket
    void connect_reading_messages();

};

#endif // SERVER_H
