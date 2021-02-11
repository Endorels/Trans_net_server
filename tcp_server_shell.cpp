#include "tcp_server_shell.h"

TcpServerShell::TcpServerShell()
{
    qDebug() << QThread::currentThread();
    if(listen(QHostAddress::Any, 6666))
       qDebug() << QString("Server started!");
    else
       qDebug() << QString("Server start error!");

    data_buffers = new QLinkedList<computers_buffer>;
    size_of_header = sizeof(shell_message) - sizeof(QByteArray) - 2;

    timer_live_time = new QTimer(this);
    timer_live_time->setInterval(2000);
    connect(timer_live_time, &QTimer::timeout, this, &TcpServerShell::check_old_messages);
    timer_live_time->start();

    mutex_delete_buffer = new QMutex;
}

void TcpServerShell::incomingConnection(qintptr handle)
{
    mutex_delete_buffer->lock();

//    qDebug() << "incomingConnection";
    computers_buffer abonent;
    abonent.comp_name =  QString("Unknown%1").arg(number_of_names);

    ThreadShell * sub_thread = new ThreadShell(handle, abonent.comp_name,this);
    connect(sub_thread, &ThreadShell::destroyed, sub_thread, &ThreadShell::deleteLater);
    connect(this, &TcpServerShell::send_message_to_abonent, sub_thread, &ThreadShell::resend_to_worker);
    sub_thread->start();

    abonent.thread = sub_thread;
     ++number_of_names;
    data_buffers->push_back(abonent);

    mutex_delete_buffer->unlock();
}


void TcpServerShell::creating_shell_struct(const QByteArray *info_incom, shell_message &pack_mess)
{
    QDataStream in_s(*info_incom);

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
    in_name_receiver = info_incom->mid(14,10);
    in_name_sender = info_incom->mid(24,10);

    pack_mess.type = static_cast<int>(in_mess_type);
    pack_mess.num = static_cast<int>(in_num);
    pack_mess.time_live = static_cast<int>(in_live_time);
    pack_mess.parts_summ = static_cast<int>(in_parts_summ);
    pack_mess.part_current = static_cast<int>(in_that_part);
    pack_mess.size_in = static_cast<int>(in_data_size_bytes);
    pack_mess.name_of_sender = in_name_sender;
    pack_mess.name_of_receiver = in_name_receiver;
}

void TcpServerShell::save_buffer_from_connected_comp(QString comp_name, QLinkedList<shell_message> buff)
{
    mutex_delete_buffer->lock();

    auto it = data_buffers->begin();
    while(it != data_buffers->end())
    {
        if(it->comp_name == comp_name)
        {
            for(auto n : buff)
                it->buffer.push_back(n);
            break;
        }
        ++it;
    }
    if(it == data_buffers->end()) qDebug() << "Error! No buffer found to save the deleting socket!";

    mutex_delete_buffer->unlock();
}

void TcpServerShell::send_message(QString name, shell_message pack_mess)
{
    mutex_delete_buffer->lock();

    auto it = data_buffers->begin();
    while(it != data_buffers->end())
    {
        if(it->comp_name == name)
        {
            if(it->thread != nullptr)
            {
                emit send_message_to_abonent(pack_mess, name);
                break;
            }
            else
            {
                qDebug() << "write the message number to the buffer = " << pack_mess.num;
                it->buffer.push_back(pack_mess);
                qDebug() << "it->buffer.size() == " <<it->buffer.size();
                break;
            }
        }
        ++it;
    }
    if(it == data_buffers->end())
    {
        computers_buffer abonent;
        abonent.comp_name = name;
        abonent.buffer.push_back(pack_mess);
        abonent.thread = nullptr;
        data_buffers->push_back(abonent);
    }
    mutex_delete_buffer->unlock();
}

void TcpServerShell::changing_comp_name(QString old_name, QString new_name, ThreadShell * ms_ptr)
{
    qDebug() << "data_buffers->size() == " << data_buffers->size();
    mutex_delete_buffer->lock();

    bool WasCreatedBefore = false;
    auto it = data_buffers->begin();
    while(it != data_buffers->end())
    {
        if(it->comp_name == new_name && WasCreatedBefore == false)
        {
            WasCreatedBefore = true;
            it->thread = ms_ptr;

            auto iter = it->buffer.begin();
            while(iter != it->buffer.end())
            {
                emit send_message_to_abonent(*iter,it->comp_name);
                iter = it->buffer.erase(iter);
            }

            emit connect_reading_messages();
            ++it;
        }
        else if(it->comp_name == old_name && WasCreatedBefore == true)
        {
            it = data_buffers->erase(it);
        }
        else if(it->comp_name == old_name && WasCreatedBefore == false)
        {
            it->comp_name = new_name;
            emit connect_reading_messages();
            ++it;
        }
        else ++it;
    }
    mutex_delete_buffer->unlock();
}

void TcpServerShell::clearing_thread(QString comp_name)
{
    mutex_delete_buffer->lock();

    auto it = data_buffers->begin();
    while(it != data_buffers->end())
    {
        if(it->comp_name == comp_name)
        {
            it->thread = nullptr;
            break;
        }
        ++it;
    }
    mutex_delete_buffer->unlock();
}

void TcpServerShell::check_old_messages()
{
    mutex_delete_buffer->lock();

    auto it = data_buffers->begin();
    while(it != data_buffers->end())
    {
        auto it2 = it->buffer.begin();
        while(it2 != it->buffer.end())
        {
            if(it2->time_checking >= it2->time_live)
                it2 = it->buffer.erase(it2);
            else
            {
                it2->time_checking++;
                ++it2;
            }
        }
        ++it;
    }
    mutex_delete_buffer->unlock();
}


