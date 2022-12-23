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
        catchException([&]() {
            Q_EMIT scanStarted(scanner->start(preview));
            mScanner = scanner;
            scanNextScanLine();
        });
    }

    void cancelScan() noexcept
    {
        complete(SANE_STATUS_CANCELLED);
    }

    void scanNextScanLine() noexcept
    {
        catchException([&]() {
            if (mScanner) {
                auto scanLine = mScanner->readScanLine();
                if (scanLine.isEmpty())
                    return complete(SANE_STATUS_EOF);

                Q_EMIT scanLineScanned(scanLine);
            }
        });
    }

Q_SIGNALS:
    void scanStarted(QImage image);
    void scanLineScanned(QByteArray scanLine);
    void scanComplete(int status);

private:
    template<typename F>
    void catchException(F&& function) noexcept
    {
        try {
            function();
        }
        catch (const SaneException &ex) {
            complete(ex.status());
        }
    }

    void complete(SANE_Status status) noexcept
    {
        if (mScanner) {
            mScanner->cancel();
            mScanner = nullptr;
            Q_EMIT scanComplete(status);
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
