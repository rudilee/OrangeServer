#include <QDebug>

#include "worker.h"

Worker::Worker(int index) :
    QThread(),
    index(index)
{
    qDebug() << "Worker" << index << "initalized";
}

Worker::~Worker()
{
    qDebug() << "Worker" << index << "destroyed";
}

void Worker::run()
{
    qDebug() << "Worker" << index << "running on thread:" << currentThreadId();

    exec();

    qDebug() << "Worker" << index << "finished";
}
