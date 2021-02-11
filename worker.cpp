#include "worker.h"
#include "tcp_server_shell.h"

worker::worker(qintptr sock_descrpt, const QString &VK_name, TcpServerShell * serv_ptr, ThreadShell * ms_ptr)
{
    thread_shell_ptr = ms_ptr;
    server_ptr = serv_ptr;
    connect(this, &worker::send_buffer_out, server_ptr, &TcpServerShell::save_buffer_from_connected_comp);
    connect(this, &worker::send_info_mess_to_server, server_ptr, &TcpServerShell::send_message);
    connect(this, &worker::change_VK_name, server_ptr, &TcpServerShell::changing_comp_name);
    connect(this, &worker::clear_thread, server_ptr, &TcpServerShell::clearing_thread);   // ???
    connect(server_ptr, &TcpServerShell::connect_reading_messages,this, &worker::connect_reading_buffer);


    comp_name_worker = VK_name;
    qDebug() << QString("Creating worker with name - %1").arg(comp_name_worker);

    socket = new QTcpSocket(this);  // creating socket
    socket->setSocketDescriptor(sock_descrpt);
    connect(socket, &QTcpSocket::disconnected, this, &worker::disconnecting);
    connect(socket,&QTcpSocket::readyRead, this, &worker::read_message);

    timer_live_time = new QTimer(this);  // timer to keep track of message lifetime to clear the buffer
    timer_live_time->setInterval(1000);
    connect(timer_live_time, &QTimer::timeout, this, &worker::check_buffer_messages_live_time);
    timer_live_time->start();

    timer_disconnect = new QTimer(this);   // timer for disconnect
    timer_disconnect->setInterval(1000);
    connect(timer_disconnect, &QTimer::timeout, this, &worker::connection_checking);
    timer_disconnect->start();

    connect(this,&worker::finish, this, &worker::deleteLater);
}

void worker::read_message()
{
    if(qobject_cast<QTcpSocket*>(sender()) == socket)
    {
        connection_check = 0;

        if(buffer_cutted.size() > 0)
        {
            if(socket->bytesAvailable() >= buffer_cutted.last().size_in)
            {
                buffer_cutted.last().message = socket->read(buffer_cutted.last().size_in);
                buffer_cutted.last().full_message = sep_message + buffer_cutted.last().full_message + buffer_cutted.last().message;
                message_checking(buffer_cutted.last());
                buffer_cutted.pop_back();
                read_message();
            }
            else
                return;
        }
        else
        {
            while(socket->bytesAvailable() >= protokol_size)
            {
                QByteArray info = socket->read(protokol_size);

                if(info.contains(sep_message))
                info.remove(0,sep_message.size());

                shell_message pack_mess;
                creating_pack_struct(&info,pack_mess); // create a structure for deserialization

                if(socket->bytesAvailable() >= pack_mess.size_in)
                {
                    pack_mess.message = socket->read(pack_mess.size_in);
                    pack_mess.full_message = sep_message + pack_mess.full_message + pack_mess.message;
                    message_checking(pack_mess);

                    if(first_message_received == false) return;
                }
                else
                {
                    buffer_cutted.push_back(pack_mess);
                    break;
                }
            }
        }
    }
}

void worker::message_checking(shell_message &pack_mess)
{
    if(pack_mess.type == 0)
    {
        if(pack_mess.message.contains(control_word))
        {
            QByteArray my_reg_message;
            creating_regular_message(&control_answer,my_reg_message,1,1,0);
            if(socket->write(my_reg_message) == -1)
                qDebug() << "Error writing to socket - " << socket->errorString();
        }
        if(pack_mess.message.contains(name_word))
        {
           QString VK_name = pack_mess.message.remove(0,name_word.size());
           qDebug() << "change_VK_name";
           emit change_VK_name(comp_name_worker, VK_name, thread_shell_ptr);
           comp_name_worker = VK_name;
           disconnect_reading_buffer();
        }
        else if(pack_mess.message.contains(control_get_message))
        {
            clear_buffer_out(pack_mess);
        }
        else if(pack_mess.message == "1")                  //  search and resend a message
        {
            find_and_send_lost_message(&pack_mess);
        }
    }
    else if(pack_mess.type != 0)
    {
        QByteArray my_reg_message;
        QByteArray mess = control_get_message + QString(" %1").arg(pack_mess.num).toUtf8();
        creating_regular_message(&mess,my_reg_message,pack_mess.part_current,pack_mess.parts_summ,pack_mess.num);
        if(socket->write(my_reg_message) == -1)
            qDebug() << "Error writing to socket - " << socket->errorString();

        if(pack_mess.parts_summ > 1)
        {
            buffer_in.push_back(pack_mess);

            if(pack_mess.parts_summ == pack_mess.part_current)
            {
                QVector<int> count;   // an array for counting the number of received broken messages
                count.resize(pack_mess.parts_summ);
                count.fill(0);

                if(is_all_parts_here(&pack_mess, count))
                {
                    if(pack_mess.name_of_receiver != comp_name_worker)
                        send_all_parts_to_another_abonent(&pack_mess);
                    else
                        send_all_parts_to_this_abonent(&pack_mess);
                }
                else
                {
                    for(int j = 0; j < pack_mess.parts_summ; ++j)
                        if(count[j] != 1)                   // if some part is not present, then we issue a request for issue
                        {
                            QByteArray message;
                            QByteArray info_mess = "1";
                            creating_regular_message(&info_mess,message,j+1, pack_mess.parts_summ,pack_mess.num);
                            if(socket->write(message) == -1)
                                qDebug() << "Error writing to socket - " << socket->errorString();
                       }
                }
            }
        }
        else    // if the message consists of one part
        {
            if(pack_mess.name_of_receiver != comp_name_worker)
                emit send_info_mess_to_server(pack_mess.name_of_receiver, pack_mess);
            else
            {
                buffer_out.push_back(pack_mess);
                socket->write(pack_mess.full_message);
            }
        }
    }
}

void worker::send_mess_to_abonent(shell_message pack_mess, QString VK_name)
{
    if(VK_name == comp_name_worker)
    {
        if(pack_mess.part_current != pack_mess.parts_summ) // if this is not the last part
        {
            buffer_in.push_back(pack_mess);
        }
        else    // if this is the last part
        {
            if(pack_mess.parts_summ == 1)
            {
                buffer_out.push_back(pack_mess);

                if(socket != nullptr && socket->state() == QAbstractSocket::ConnectedState)
                socket->write(pack_mess.full_message);
            }
            else        // if the message consists of several parts
            {
                buffer_in.push_back(pack_mess);

                write_all_parts_in_socket(&pack_mess);
            }
        }
    }
}

void worker::clear_buffer_out(shell_message &pack_mess)
{
    auto it = buffer_out.begin();
    while(it != buffer_out.end())
    {
        if(it->num == pack_mess.num)
            it = buffer_out.erase(it);
        else
            ++it;
    }
}

void worker::check_buffer_messages_live_time()
{
    auto it = buffer_out.begin();
    while(it != buffer_out.end())
    {
        if(it->time_checking >= it->time_live)
            it = buffer_out.erase(it);
        else
        {
            it->time_checking++;
            ++it;
        }
    }

    auto it2 = buffer_in.begin();
    while(it2 != buffer_in.end())
    {
        if(it2->time_checking >= it2->time_live)
            it2 = buffer_in.erase(it2);
        else
        {
            it2->time_checking++;
            ++it2;
        }
    }
}
bool worker::is_all_parts_here(shell_message * pack_mess, QVector<int> &count)
{
    bool is_ok = true;

    auto iter = buffer_in.begin();
    while(iter != buffer_in.end())
    {
        if(iter->num == pack_mess->num)
            count[iter->part_current - 1] = 1;

        iter++;
    }

    for(int i = 0; i < pack_mess->parts_summ; ++i)
    {
        if(count[i] != 1)
            is_ok = false;
    }
    return is_ok;
}

void worker::send_all_parts_to_this_abonent(shell_message *pack_mess)
{
    auto it = buffer_in.begin();
    while(it != buffer_in.end())
    {
        if(it->num == pack_mess->num)
        {
            buffer_out.push_back(*it);
            socket->write((*it).full_message);
        }
        ++it;
    }
}

void worker::send_all_parts_to_another_abonent(shell_message *pack_mess)
{
    auto it = buffer_in.begin();
    while(it != buffer_in.end())
    {
        if(it->num == pack_mess->num)
            emit send_info_mess_to_server(pack_mess->name_of_receiver, *it);
        ++it;
    }
}

void worker::write_all_parts_in_socket(shell_message *pack_mess)
{
    auto it = buffer_in.begin();
    while(it != buffer_in.end())
    {
        if(it->num == pack_mess->num)
        {
            buffer_out.push_back(*it);

            if(socket != nullptr && socket->state() == QAbstractSocket::ConnectedState)
            socket->write((*it).full_message);

            it = buffer_in.erase(it);
        }
        else ++it;
    }
}

void worker::find_and_send_lost_message(shell_message *pack_mess)
{
    auto iter = buffer_out.begin();
    while(iter != buffer_out.end())
    {
        if(iter->num == pack_mess->num && iter->part_current == pack_mess->part_current)
        {
            if(socket->write(iter->message) == -1)
                qDebug() << "Error writing to socket - " << socket->errorString();
            break;
        }
        iter++;
    }
    if(iter == buffer_out.end()) qDebug() << "Could not find the correct part of the message to re-issue.";
}

void worker::disconnect_reading_buffer()
{
    disconnect(socket,&QTcpSocket::readyRead, this, &worker::read_message);
}

void worker::connect_reading_buffer()
{
    connect(socket,&QTcpSocket::readyRead, this, &worker::read_message);
    first_message_received = true;
}

void worker::disconnecting()
{
    if(qobject_cast<QTcpSocket*>(sender()) == socket)
    {
        qDebug() << "Socket disconnect signal";
        emit send_buffer_out(comp_name_worker,buffer_out);
        emit clear_thread(comp_name_worker);
        emit finish();
    }
}

void worker::connection_checking()
{
    if(connection_check > 3)
    {
        connection_check = 0;
        socket->abort();
    }
    else
        ++connection_check;
}

void worker::creating_regular_message(const QByteArray *reg_mess, QByteArray &message, const int &part, const int &part_summ, const int &num)
{
    QByteArray info;
    //s.setVersion(QDataStream::Qt_5_13);

    char nul_char = 0;
    int size = reg_mess->size();

    QByteArray out_name_sender = "Server";
    int name_send_size = out_name_sender.size();

    QByteArray out_name_receiver = comp_name_worker.toUtf8();
    int name_receiver_size = out_name_receiver.size();

    QDataStream out_s(&info,QIODevice::WriteOnly);
    out_s.writeRawData(sep_message,3);
    out_s << static_cast<unsigned short int>(0);
    out_s << static_cast<unsigned short int>(num);
    out_s << static_cast<unsigned short int>(5);
    out_s << static_cast<unsigned short int>(part_summ);
    out_s << static_cast<unsigned short int>(part);
    out_s << static_cast<unsigned int>(size);

    out_s.writeRawData(out_name_sender,name_send_size);

    for(int i = 0; i < 10-name_send_size; ++i)
        out_s.writeRawData(&nul_char,1);

    out_s.writeRawData(out_name_receiver,name_receiver_size);

    for(int i = 0; i < 10-name_receiver_size; ++i)
        out_s.writeRawData(&nul_char,1);

    out_s.writeRawData(*reg_mess,size);

    message = info;
}

void worker::creating_pack_struct(const QByteArray *info_incom, shell_message &pack_mess)
{
    QDataStream in_s(*info_incom);

    pack_mess.full_message = *info_incom;

    unsigned short int in_mess_type;
    unsigned short int in_num;
    unsigned short int in_live_time;
    unsigned short int in_parts_summ;
    unsigned short int in_that_part;
    QString in_name_receiver;
    QString in_name_sender;
    unsigned int in_data_size_bytes;

    in_s>>in_mess_type;
    in_s>>in_num;
    in_s>>in_live_time;
    in_s>>in_parts_summ;
    in_s>>in_that_part;
    in_s>>in_data_size_bytes;
    in_name_sender = info_incom->mid(14,10);
    in_name_receiver = info_incom->mid(24,10);

    pack_mess.type = static_cast<int>(in_mess_type);
    pack_mess.num = static_cast<int>(in_num);
    pack_mess.time_live = static_cast<int>(in_live_time);
    pack_mess.parts_summ = static_cast<int>(in_parts_summ);
    pack_mess.part_current = static_cast<int>(in_that_part);
    pack_mess.size_in = static_cast<int>(in_data_size_bytes);
    pack_mess.name_of_sender = in_name_sender;
    pack_mess.name_of_receiver = in_name_receiver;
    pack_mess.time_checking = 0;
}
