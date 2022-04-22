#ifndef FUTURE_SCHEDULER_H
#define FUTURE_SCHEDULER_H

#include <functional>

#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QJSValue>
#include <QMutex>
#include <QMutexLocker>
#include <QPair>
#include <QWaitCondition>

class FutureScheduler : public QObject
{
    Q_OBJECT

public:
    FutureScheduler(QObject *parent);
    ~FutureScheduler();

    void shutdownWaitForFinished(const QString name) noexcept;

    QPair<bool, QFuture<void>> run(std::function<void()> function, const QString name) noexcept;
    QPair<bool, QFuture<QJSValueList>> run(std::function<QJSValueList()> function, const QString name, const QJSValue &callback);
    bool stopping(const QString name) const noexcept;

private:
    bool add(const QString name) noexcept;
    void done(const QString name) noexcept;

    template<typename T>
    QPair<bool, QFuture<T>> execute(std::function<QFuture<T>(QFutureWatcher<T> *)> makeFuture, const QString name) noexcept
    {
        qInfo() << name << " " << "FutureScheduler::execute() start";
        if (add(name))
        {
            try
            {
                qInfo() << name << " " << "FutureScheduler::execute() inside try";
                auto *watcher = new QFutureWatcher<T>();
                qInfo() << name << " " << "FutureScheduler::execute() before connect";
                connect(watcher, &QFutureWatcher<T>::finished, [watcher, name] {
                    qInfo() << name << " " << "FutureScheduler::execute() before deleteLater";
                    watcher->deleteLater();
                });
                qInfo() << name << " " << "FutureScheduler::execute() before setFuture";
                watcher->setFuture(makeFuture(watcher));
                qInfo() << name << " " << "FutureScheduler::execute() before return qMakePair";
                return qMakePair(true, watcher->future());
            }
            catch (const std::exception &exception)
            {
                qInfo() << name << " " << "FutureScheduler::execute() inside catch";
                qCritical() << "Failed to schedule async function: " << exception.what();
                done(name);
            }
        }

        qInfo() << name << " " << "FutureScheduler::execute() end";
        return qMakePair(false, QFuture<T>());
    }

    QFutureWatcher<void> schedule(std::function<void()> function);
    QFutureWatcher<QJSValueList> schedule(std::function<QJSValueList() noexcept> function, const QJSValue &callback);

private:
    size_t Alive;
    QWaitCondition Condition;
    QMutex Mutex;
    std::atomic<bool> Stopping;
};

#endif // FUTURE_SCHEDULER_H
