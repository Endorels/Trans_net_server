#include "thread_shell.h"
#include "tcp_server_shell.h"

ThreadShell::ThreadShell(qintptr sock, const QString &comp_name, TcpServerShell *serv_pointer)
{
    serv_ptr = serv_pointer;
    socket_dscrp = sock;
    comp_name_to_create = comp_name;
}

void ThreadShell::run()
{
    current_worker = new worker(socket_dscrp, comp_name_to_create, serv_ptr, this);
    connect(current_worker, &worker::destroyed, this, &ThreadShell::quit);
    connect(this, &ThreadShell::resend_to_worker, current_worker, &worker::send_mess_to_abonent);
    connect(this,&ThreadShell::finished, this, &ThreadShell::deleteLater);

    exec();
}

