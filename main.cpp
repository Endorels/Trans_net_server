#include <QCoreApplication>
#include "tcp_server_shell.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    qRegisterMetaType<shell_message>();
    qRegisterMetaType<QLinkedList<shell_message> >();

    //setlocale(LC_ALL,"Rus");

    TcpServerShell server;

    return a.exec();
}
