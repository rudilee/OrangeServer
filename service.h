#ifndef SERVICE_H
#define SERVICE_H

#include <QtService>

class Service : public QtService<QCoreApplication>
{
public:
    Service(int &argc, char **argv);
    ~Service();

protected:
    void createApplication(int &argc, char **argv);
    int executeApplication();
    void processCommand(int code);
    void start();
    void stop();
};

#endif // SERVICE_H
