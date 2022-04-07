#include "FutureScheduler.h"

#include <mutex>

#include <QThreadPool>

FutureScheduler::FutureScheduler(QObject *parent)
    : QObject(parent), Alive(0), Stopping(false)
{
    static std::once_flag once;
    std::call_once(once, []() {
        QThreadPool::globalInstance()->setMaxThreadCount(4);
    });
}

FutureScheduler::~FutureScheduler()
{
    qInfo() << "FutureScheduler::~FutureScheduler start";
    shutdownWaitForFinished();
    qInfo() << "FutureScheduler::~FutureScheduler end";
}

void FutureScheduler::shutdownWaitForFinished() noexcept
{
    qInfo() << "FutureScheduler::shutdownWaitForFinished start";
    QMutexLocker locker(&Mutex);

    Stopping = true;
    while (Alive > 0)
    {
        qInfo() << "FutureScheduler::shutdownWaitForFinished inside while";
        Condition.wait(&Mutex);
    }
    qInfo() << "FutureScheduler::shutdownWaitForFinished end";
}

QPair<bool, QFuture<void>> FutureScheduler::run(std::function<void()> function) noexcept
{
    qInfo() << "FutureScheduler::run(no callback) start";
    return execute<void>([this, function](QFutureWatcher<void> *) {
        qInfo() << "FutureScheduler::run(no callback) inside first return";
        return QtConcurrent::run([this, function] {
            qInfo() << "FutureScheduler::run(no callback) inside second return";
            try
            {
                qInfo() << "FutureScheduler::run(no callback) calling function";
                function();
            }
            catch (const std::exception &exception)
            {
                qInfo() << "FutureScheduler::run(no callback) calling function failed";
                qWarning() << "Exception thrown from async function: " << exception.what();
            }
            qInfo() << "FutureScheduler::run(no callback) before done";
            done();
            qInfo() << "FutureScheduler::run(no callback) after done";
        });
    });
}

QPair<bool, QFuture<QJSValueList>> FutureScheduler::run(std::function<QJSValueList()> function, const QJSValue &callback)
{
    qInfo() << "FutureScheduler::run(callback) start";
    if (!callback.isCallable())
    {
        qInfo() << "FutureScheduler::run(callback) inside if (!callback.isCallable())";
        throw std::runtime_error("js callback must be callable");
    }

    return execute<QJSValueList>([this, function, callback](QFutureWatcher<QJSValueList> *watcher) {
        qInfo() << "FutureScheduler::run(callback) inside first return";
        connect(watcher, &QFutureWatcher<QJSValueList>::finished, [watcher, callback] {
            qInfo() << "FutureScheduler::run(callback) inside connect";
            QJSValue(callback).call(watcher->future().result());
        });
        return QtConcurrent::run([this, function] {
            qInfo() << "FutureScheduler::run(callback) inside second return";
            QJSValueList result;
            try
            {
                qInfo() << "FutureScheduler::run(callback) calling function";
                result = function();
            }
            catch (const std::exception &exception)
            { 
                qInfo() << "FutureScheduler::run(callback) calling function failed";
                qWarning() << "Exception thrown from async function: " << exception.what();
            }
            qInfo() << "FutureScheduler::run(callback) before done";
            done();
            qInfo() << "FutureScheduler::run(callback) after done";
            return result;
        });
    });
}

bool FutureScheduler::stopping() const noexcept
{
    qInfo() << "FutureScheduler::stopping() start";
    return Stopping;
}

bool FutureScheduler::add() noexcept
{
    qInfo() << "FutureScheduler::add() start";
    QMutexLocker locker(&Mutex);

    qInfo() << "FutureScheduler::add() after locker mutex";
    if (Stopping)
    {
        qInfo() << "FutureScheduler::add() inside if (Stopping)";
        return false;
    }

    qInfo() << "FutureScheduler::add() increment alive";
    ++Alive;
    qInfo() << "FutureScheduler::add() end";
    return true;
}

void FutureScheduler::done() noexcept
{
    qInfo() << "FutureScheduler::done() start";
    {
        qInfo() << "FutureScheduler::done() before locker mutex";
        QMutexLocker locker(&Mutex);
        qInfo() << "FutureScheduler::done() after locker mutex";
        --Alive;
    }

    qInfo() << "FutureScheduler::done() before wakeAll";
    Condition.wakeAll();
    qInfo() << "FutureScheduler::done() end";
}
