#pragma once

#include <QObject>
#include <QThread>
#include "Scanner.h"

class Worker;

class WorkerThread : public QObject
{
    Q_OBJECT
public:
    explicit WorkerThread(QObject *parent = nullptr);
    ~WorkerThread();

    void scan(Scanner *scanner, bool preview);
    void cancelScan();

Q_SIGNALS:
    void doScan(Scanner *scanner, bool preview, QPrivateSignal);
    void doCancelScan(QPrivateSignal);
    void scanStarted(QImage image);
    void scanComplete(int status);
    void scanLineScanned(QByteArray scanLine);

private:
    QThread mThread;
    QScopedPointer<Worker> mWorker;
};
