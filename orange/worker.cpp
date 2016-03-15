#include <QDebug>

#include "terminal.h"
#include "worker.h"

Worker::Worker(int index) :
    QThread(),
    index(index)
{
    qDebug() << "Worker" BOLD BLUE << index << RESET "initalized";
}

Worker::~Worker()
{
    qDebug() << "Worker" BOLD BLUE << index << RESET "destroyed";
}

void Worker::run()
{
    qDebug() << "Worker" BOLD BLUE << index << RESET "running on thread:" BOLD BLUE << currentThreadId() << RESET;

    exec();

    qDebug() << "Worker" BOLD BLUE << index << RESET "finished";
}
