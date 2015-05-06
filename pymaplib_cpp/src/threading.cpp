#include <threading.h>

#include <boost/thread/thread.hpp>

ThreadedTaskRunner::ThreadedTaskRunner()
    : m_tasks(), m_exitthread(false), m_thread()
{
    m_thread.reset(new boost::thread(&ThreadedTaskRunner::threadproc, this));
}

ThreadedTaskRunner::~ThreadedTaskRunner() {
    if (!m_thread) {
        return;
    }
    m_exitthread = true;
    // Push a dummy task to make the thread continue if stuck in m_tasks.pop().
    m_tasks.push(Task());
    m_thread->join();
}

void ThreadedTaskRunner::Enqueue(const Task& f) {
    m_tasks.push(f);
}

void ThreadedTaskRunner::threadproc() {
    while (!m_exitthread) {
        auto task = m_tasks.pop();
        if (task) {
            task();
        }
    }
}

void ThreadedTaskGroupRunner::Enqueue(GroupID group_id, const Task& f)
{
    auto &tq = find_or_create_runner(group_id);
    tq.Enqueue(f);
}

ThreadedTaskRunner&
ThreadedTaskGroupRunner::find_or_create_runner(GroupID group_id) {
    auto val = TaskRunnerMap::value_type(group_id, ThreadedTaskRunnerPtr());
    // Insert returns a pair<iterator<value_type> it, bool was_inserted>.
    auto iterpair = m_tasks.insert(val);

    // If val was newly inserted (and not just retrieved), start the Thread.
    if (iterpair.second) {
        assert(!iterpair.first->second);
        iterpair.first->second.reset(new ThreadedTaskRunner());
    }
    return *iterpair.first->second;
}
