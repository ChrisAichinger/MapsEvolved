// Copyright 2015 Christian Aichinger <Greek0@gmx.net>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ODM__THREADING_H
#define ODM__THREADING_H

#include <functional>
#include <map>
#include <memory>

#include <boost/atomic.hpp>

#include <external/concurrent_queue.h>
#include <util.h>


/** An abstraction of a task to execute.
 *
 * Tasks are `std::function<void()>` instances, i.e. they don't receive any
 * arguments and can not return a value. Use lambdas to circumvent these
 * limitations.
 */
typedef std::function<void()> Task;


/** Run functions in a separate thread.
 *
 * For each `ThreadedTaskRunner` instance, a single worker thread is created.
 * Tasks added via the `Enqueue()` method and are executed sequentially in order
 * of addition in this thread.
 *
 * Removing tasks from the queue is currently not supported.
 *
 * @locking No manual locking used, everything is encapsulated in
 * `ConcurrentQueue`. The queue never calls outside code with locks held.
 */
class ThreadedTaskRunner {
public:
    ThreadedTaskRunner();
    ~ThreadedTaskRunner();

    /** Add tasks to be executed in the backing thread */
    void Enqueue(const Task& f);

private:
    DISALLOW_COPY_AND_ASSIGN(ThreadedTaskRunner);

    void threadproc();

    ConcurrentQueue<Task> m_tasks;
    boost::atomic<bool> m_exitthread;
    std::unique_ptr<class boost::thread> m_thread;
};

/** A pool of `ThreadedTaskRunner`'s
 *
 * Tasks are categorized into task groups. For each group, a thread is created.
 * Tasks within each group are executed sequentially in order of addition on
 * the same thread. Tasks from different groups are run in parallel, each on
 * their own thread.
 *
 * New groups are created simply by enqueueing tasks with a group id never used
 * before.
 *
 * Removing tasks from the queue is currently not supported. Removing groups
 */
class ThreadedTaskGroupRunner {
public:
    /** Group IDs are currently `void*` pointers. */
    typedef void* GroupID;

    ThreadedTaskGroupRunner() : m_tasks() {};

    /** Add a task to be executed
     *
     * Enqueue task `f` to be executed on the thread associated with group
     * `group_id`.
     */
    void Enqueue(GroupID group_id, const Task& f);

private:
    DISALLOW_COPY_AND_ASSIGN(ThreadedTaskGroupRunner);

    // VS2010 can't place non-copyable objects in std::map.
    // Work around the ancient toolchain :-/
    typedef std::shared_ptr<ThreadedTaskRunner> ThreadedTaskRunnerPtr;
    typedef std::map<GroupID, ThreadedTaskRunnerPtr> TaskRunnerMap;

    ThreadedTaskRunner& find_or_create_runner(GroupID group_id);
    TaskRunnerMap m_tasks;
};

#endif
