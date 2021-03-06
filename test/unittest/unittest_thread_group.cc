#include <dmlc/io.h>
#include <dmlc/memory_io.h>
#include <dmlc/blockingconcurrentqueue.h>
#include <dmlc/thread_group.h>
#include <gtest/gtest.h>
#if defined(_MSC_VER) && _MSC_VER <= 1800
#include <Windows.h>
#endif

static std::atomic<int> thread_count(0);

static inline std::string TName(const std::string& s, int x) { return s + "-" + std::to_string(x); }

static int this_is_thread_func(std::string label, const bool with_delay) {
  ++thread_count;
  if(with_delay) {
    usleep(1e4);
  }
  --thread_count;
  return 0;
}

/*!
 * \brief Generic Thread launch to standalone function, passing ThreadGroup owner
 */
TEST(ThreadGroup, ThreadLaunchAutoRemove) {
  std::shared_ptr<dmlc::ThreadGroup> thread_group = std::make_shared<dmlc::ThreadGroup>();
  for(int x = 0; x < 200; ++x) {
    dmlc::ThreadGroup::Thread::SharedPtr thread =
      std::make_shared<dmlc::ThreadGroup::Thread>(std::string("test_thread_ar ")
                                                         + std::to_string(x), thread_group.get());
    dmlc::ThreadGroup::Thread::launch(thread, true, this_is_thread_func, "Runner", false);
  }
  thread_group.reset();
  CHECK_EQ(thread_count, 0);
}

/*!
 * \brief Generic Thread launch to standalone function, passing ThreadGroup owner
 */
TEST(ThreadGroup, ThreadLaunchAutoRemoveWithDelay) {
  std::shared_ptr<dmlc::ThreadGroup> thread_group = std::make_shared<dmlc::ThreadGroup>();
  for(int x = 0; x < 200; ++x) {
    dmlc::ThreadGroup::Thread::SharedPtr thread =
      std::make_shared<dmlc::ThreadGroup::Thread>(std::string("test_thread_rwd ")
                                                         + std::to_string(x), thread_group.get());
    dmlc::ThreadGroup::Thread::launch(thread, true, this_is_thread_func, "Runner", true);
  }
  thread_group.reset();
  CHECK_EQ(thread_count, 0);
}

/*!
 * \brief Generic Thread launch to standalone function, passing ThreadGroup owner
 */
TEST(ThreadGroup, ThreadLaunchNoAutoRemove) {
  std::shared_ptr<dmlc::ThreadGroup> thread_group = std::make_shared<dmlc::ThreadGroup>();
  for(int x = 0; x < 200; ++x) {
    dmlc::ThreadGroup::Thread::SharedPtr thread =
      std::make_shared<dmlc::ThreadGroup::Thread>(std::string("test_thread_nao ")
                                                         + std::to_string(x), thread_group.get());
    dmlc::ThreadGroup::Thread::launch(thread, false, this_is_thread_func, "Runner", false);
  }
  thread_group.reset();
  CHECK_EQ(thread_count, 0);
}

/*!
 * \brief Generic Thread launch to standalone function, passing ThreadGroup owner
 */
TEST(ThreadGroup, ThreadLaunchNoAutoRemoveWithDelay) {
  std::shared_ptr<dmlc::ThreadGroup> thread_group = std::make_shared<dmlc::ThreadGroup>();
  for(int x = 0; x < 200; ++x) {
    dmlc::ThreadGroup::Thread::SharedPtr thread =
      std::make_shared<dmlc::ThreadGroup::Thread>(std::string("test_thread_narwd ")
                                                         + std::to_string(x), thread_group.get());
    dmlc::ThreadGroup::Thread::launch(thread, false, this_is_thread_func, "Runner", true);
  }
  thread_group.reset();
  CHECK_EQ(thread_count, 0);
}

/*!
 * \brief Test BlockingQueueThread
 */
TEST(ThreadGroup, ThreadLaunchQueueThread) {
  // Define the queue type for convenience
  using BQ = dmlc::BlockingQueueThread<int, -1>;

  // Create the thread group
  std::shared_ptr<dmlc::ThreadGroup> thread_group = std::make_shared<dmlc::ThreadGroup>();

  // Create the queue thread object
  std::shared_ptr<BQ> queue_thread = std::make_shared<BQ>("BlockingQueueThread",
                                                          thread_group.get());

  // Queue some stuff before the thread starts
  queue_thread->enqueue(1);
  queue_thread->enqueue(2);
  queue_thread->enqueue(3);
  queue_thread->enqueue(4);
  CHECK_EQ(queue_thread->size_approx(), 4U);
  // Launch the queue thread, passing queue item handler as lambda
  BQ::launch_run(queue_thread,
                 // Queue item handler
                 [queue_thread](int item) -> int {
                   std::cout << "ITEM: " << item
                             << std::endl << std::flush;
                   if(item >= 2 && item <= 3) {
                     // Queue some more while thread is running
                     queue_thread->enqueue(100 + item);
                   }
                   return 0;  // return 0 means continue
                 });
  // Trigger the queues to exit
  thread_group->request_shutdown_all();
  // Wait for all of the queue threads to exit
  thread_group->join_all();
  // Check that the queue is empty
  CHECK_EQ(queue_thread->size_approx(), 0);
}

using Tick = std::chrono::high_resolution_clock::time_point;
static inline Tick Now() { return std::chrono::high_resolution_clock::now(); }
static inline uint64_t GetDurationInNanoseconds(const Tick &t1, const Tick &t2) {
  return static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count());
}
static inline uint64_t GetDurationInNanoseconds(const Tick &since) {
  return GetDurationInNanoseconds(since, Now());
}

/*!
 * \brief Test TimerThread
 */
TEST(ThreadGroup, TimerThread) {
  // Create the thread group
  std::shared_ptr<dmlc::ThreadGroup> thread_group = std::make_shared<dmlc::ThreadGroup>();

  using Duration = std::chrono::milliseconds;
  // Create the queue thread object
  std::shared_ptr<dmlc::TimerThread<Duration>> timer_thread =
    std::make_shared<dmlc::TimerThread<Duration>>("TimerThread", thread_group.get());
  Tick start_time = Now();
  size_t count = 0;
  // Launch the queue thread, passing queue item handler as lambda
  dmlc::TimerThread<Duration>::start(
    timer_thread, Duration(10), [timer_thread, start_time, &count]() -> int {
      std::cout << "TIME: "
                << (GetDurationInNanoseconds(start_time)/1000/1000)
                << std::endl << std::flush;
      ++count;
      return 0;  // return 0 means continue
    });
  std::this_thread::sleep_for(Duration(50));
  // Trigger the queues to exit
  thread_group->request_shutdown_all();
  // Wait for all of the queue threads to exit
  thread_group->join_all();
  GTEST_ASSERT_GE(count, 2U);  // Should have at least done three
  GTEST_ASSERT_LE(count, 10U); // Should not have had time to do 20 of them
}

/*!
 * \brief Test TimerThread Simple
 */
TEST(ThreadGroup, TimerThreadSimple) {
  // Create the thread group
  std::shared_ptr<dmlc::ThreadGroup> thread_group = std::make_shared<dmlc::ThreadGroup>();

  using Duration = std::chrono::milliseconds;
  Tick start_time = Now();
  size_t count = 0;
  // Launch the queue thread, passing queue item handler as lambda
  dmlc::CreateTimer("TimerThreadSimple",
                    Duration(10),
                    thread_group.get(),
                    [start_time, &count]() -> int {
                      std::cout << "TIME: "
                                << (GetDurationInNanoseconds(start_time)/1000/1000)
                                << std::endl << std::flush;
                      ++count;
                      return 0;  // return 0 means continue
                    });
  std::this_thread::sleep_for(Duration(50));
  // Trigger the queues to exit
  thread_group->request_shutdown_all();
  // Wait for all of the queue threads to exit
  thread_group->join_all();
  GTEST_ASSERT_GE(count, 2U);  // Should have at least done three
  GTEST_ASSERT_LE(count, 10U); // Should not have had time to do 20 of them
}

