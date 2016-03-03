#ifndef WORKER_H
#define WORKER_H

#include <QThread>

class Worker : public QThread
{
    Q_OBJECT

public:
    Worker(int index);
    ~Worker();

    void run();

private:
    int index;
};

#endif // WORKER_H
