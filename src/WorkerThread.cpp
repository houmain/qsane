#include "WorkerThread.h"
#include "Scanner.h"

class Worker final : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void stop() noexcept
    {
        cancelScan();
        QThread::currentThread()->exit(0);
    }

    void scan(Scanner *scanner, bool preview) noexcept
    {
        mScanner = scanner;
        auto image = mScanner->startScan(preview);
        if (image.isNull())
            return complete(false);

        Q_EMIT scanStarted(image);
        scanNextScanLine();
    }

    void cancelScan() noexcept
    {
        complete(false);
    }

    void scanNextScanLine() noexcept
    {
        if (mScanner) {
            auto scanLine = mScanner->readScanLine();
            if (scanLine.isEmpty())
                return complete(true);

            Q_EMIT scanLineScanned(scanLine);
        }
    }

Q_SIGNALS:
    void scanStarted(QImage image);
    void scanLineScanned(QByteArray scanLine);
    void scanComplete(bool succeeded);

private:
    void complete(bool succeeded) noexcept
    {
        if (mScanner) {
            mScanner->cancelScan();
            mScanner = nullptr;
            Q_EMIT scanComplete(succeeded);
        }
    }

    Scanner *mScanner{ };
};

WorkerThread::WorkerThread(QObject *parent)
    : QObject(parent)
    , mWorker(new Worker())
{
    mWorker->moveToThread(&mThread);

    connect(this, &WorkerThread::doScan,
        mWorker.data(), &Worker::scan);
    connect(this, &WorkerThread::doCancelScan,
        mWorker.data(), &Worker::cancelScan);

    connect(mWorker.data(), &Worker::scanStarted,
        this, &WorkerThread::scanStarted);
    connect(mWorker.data(), &Worker::scanComplete,
        this, &WorkerThread::scanComplete);
    connect(mWorker.data(), &Worker::scanLineScanned,
        this, &WorkerThread::scanLineScanned);
    connect(mWorker.data(), &Worker::scanLineScanned,
        mWorker.data(), &Worker::scanNextScanLine, Qt::QueuedConnection);

    mThread.start();
}

WorkerThread::~WorkerThread()
{
    QMetaObject::invokeMethod(mWorker.data(),
        "stop", Qt::BlockingQueuedConnection);
}

void WorkerThread::scan(Scanner* scanner, bool preview)
{
    Q_EMIT doScan(scanner, preview, QPrivateSignal());
}

void WorkerThread::cancelScan()
{
    Q_EMIT doCancelScan(QPrivateSignal());
}

#include "WorkerThread.moc"
