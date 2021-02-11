#ifndef WORKER_H
#define WORKER_H
#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QThread>
#include <QLinkedList>
#include <QDebug>

struct shell_message;
class TcpServerShell;
class ThreadShell;

// /*******************     Structure of message   **********************************************************************************************************************/
// !@# | type | num | lifetime | parts_summ | part_curr | size_in | sender_name | receiver_name | message
// /*********************************************************************************************************************************************************************/

class worker: public QObject
{
    Q_OBJECT

public:
    worker(qintptr sock_descrpt, const QString &VK_name, TcpServerShell * s_ptr, ThreadShell * ms_ptr);
    ~worker() {/*socket->abort();*/ qDebug() << "worker destroyed";}


private:

    QString comp_name_worker;
    QTcpSocket * socket;
    QTimer * timer_disconnect;
    QTimer * timer_live_time;

    TcpServerShell * server_ptr;
    ThreadShell * thread_shell_ptr;

    QLinkedList<QString> * messages;

    QLinkedList<shell_message> buffer_out;
    QLinkedList<shell_message> buffer_in;
    QLinkedList<shell_message> buffer_cutted;

    int protokol_size = 37;
    int max_size_of_message = 363;

    QByteArray control_answer = "pong";
    QByteArray control_word = "ping";
    QByteArray control_get_message = "Message OK";
    QByteArray sep_message = "!@#";                 // message separator
    QByteArray name_word = "My name is ";

    bool first_message_received = false;
    int connection_check = 0;

    //  message handling function
    void message_checking(shell_message & pack);

    //  function to create regular messages (type == 0)
    void creating_regular_message(const QByteArray *reg_mess, QByteArray &message,const int &part,const int &sum_part,const int &num);

    //  message serialization function
    void creating_pack_struct(const QByteArray * info, shell_message &shell_message);

    //  function to clear outgoing buffer by message number
    void clear_buffer_out(shell_message & pack_mess);

    //  функция проверки времени жизни сообщений в буффере
    void check_buffer_messages_live_time();

    // function of checking if all parts are in place
    bool is_all_parts_here(shell_message * pack_mess, QVector<int> &count);

    // sending all parts of the message into current socket
    void send_all_parts_to_this_abonent(shell_message * pack_mess);

    // sending all parts of the message throw Server to another worker
    void send_all_parts_to_another_abonent(shell_message * pack_mess);

    // writing all parts of message to the socket
    void write_all_parts_in_socket(shell_message * pack_mess);

    // finding and sending the lost part of the message
    void find_and_send_lost_message(shell_message * pack_mess);

private slots:

    //  slot of reading messages from the socket
    void read_message();

    // slot for disconnecting from the server and deleting an object
    void disconnecting();

    void connection_checking();

public slots:

    // slot for sending a message to a subscriber
    void send_mess_to_abonent(shell_message message, QString VK_name);

    // disconnect slot on readyRead socket
    void disconnect_reading_buffer();

    // slot for connecting a readyRead socket
    void connect_reading_buffer();


signals:

    // shutdown woker signal
    void finish();

    // signal to send the remaining buffer to the server
    void send_buffer_out(QString name, QLinkedList<shell_message> buff);

    // signal about sending a message to the server for redistribution to the subscriber
    void send_info_mess_to_server(QString VK_name, shell_message pack_mess);

    // signal to change the subscriber's name in the Server structure
    void change_VK_name(QString old_name, QString new_name, ThreadShell * ptr);

    // setting to a nullptr stream in the list of structures of all subscribers
    void clear_thread(QString VK_name);

};

struct shell_message{
    int type;                   //  message type
    int num;                    //  message number
    int time_live;              //  message lifetime
    int parts_summ;             //  total number of message parts
    int part_current;           //  number of the current part of the message
    int size_in;                //  number of bytes in an information message
    QString name_of_receiver;   //  receiver name
    QString name_of_sender;     //  sender name
    int time_checking;          //  variable for calculating message lifetime
    QByteArray message;         //  information part of message
    QByteArray full_message;    //  full message with shell
};

Q_DECLARE_METATYPE(shell_message);

#endif // WORKER_H
