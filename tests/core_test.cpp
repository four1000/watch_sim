#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "ap/core/ring_buffer.h"
#include "ap/core/soft_timer.h"
#include "ap/core/time.h"

using namespace ap;

// ---- RingBuffer ----------------------------------------------------------------

TEST(RingBuffer, FifoOrderAndCapacity) {
    RingBuffer<int, 3> rb;
    EXPECT_TRUE(rb.empty());
    EXPECT_TRUE(rb.push(1));
    EXPECT_TRUE(rb.push(2));
    EXPECT_TRUE(rb.push(3));
    EXPECT_TRUE(rb.full());
    EXPECT_FALSE(rb.push(4)) << "full buffer must reject, never overwrite";
    EXPECT_EQ(*rb.pop(), 1);
    EXPECT_TRUE(rb.push(4));
    EXPECT_EQ(*rb.pop(), 2);
    EXPECT_EQ(*rb.pop(), 3);
    EXPECT_EQ(*rb.pop(), 4);
    EXPECT_FALSE(rb.pop().has_value());
}

TEST(RingBuffer, StorageIsInline) {
    static_assert(sizeof(RingBuffer<uint32_t, 16>) >= 16 * sizeof(uint32_t), "no hidden heap");
    static_assert(std::is_trivially_copyable_v<RingBuffer<uint32_t, 4>>);
}

// ---- time -----------------------------------------------------------------------

TEST(Time, WrapSafeComparison) {
    const TickMs nearWrap = 0xFFFFFF00u;
    const TickMs deadline = nearWrap + 0x200u;  // wraps to 0x100
    EXPECT_FALSE(timeReached(nearWrap, deadline));
    EXPECT_FALSE(timeReached(0x0FFu, deadline));
    EXPECT_TRUE(timeReached(0x100u, deadline));
    EXPECT_EQ(elapsedMs(0x100u, nearWrap), 0x200u);
}

// ---- SoftTimerPool --------------------------------------------------------------

namespace {
struct Recorder : ITimerListener {
    void onTimer(uint8_t tag, TickMs now) override {
        tags[count] = tag;
        times[count] = now;
        ++count;
        if (restartFrom != nullptr) *restartHandle = restartFrom->start(now, 100, *this, 9);
    }
    uint8_t tags[16]{};
    TickMs times[16]{};
    int count = 0;
    SoftTimerPool* restartFrom = nullptr;
    TimerHandle* restartHandle = nullptr;
};
}  // namespace

TEST(SoftTimer, FiresOnceAtDeadline) {
    SoftTimerPool pool;
    Recorder r;
    const TimerHandle h = pool.start(1000, 500, r, 7);
    ASSERT_TRUE(h.valid());
    pool.poll(1499);
    EXPECT_EQ(r.count, 0);
    pool.poll(1500);
    ASSERT_EQ(r.count, 1);
    EXPECT_EQ(r.tags[0], 7);
    pool.poll(5000);
    EXPECT_EQ(r.count, 1) << "one-shot";
    EXPECT_FALSE(pool.isRunning(h));
}

TEST(SoftTimer, StaleHandleCannotCancelNewTimer) {
    SoftTimerPool pool;
    Recorder r;
    const TimerHandle old = pool.start(0, 10, r, 1);
    pool.poll(10);  // fires, slot freed
    const TimerHandle fresh = pool.start(10, 10, r, 2);
    EXPECT_EQ(old.slot, fresh.slot) << "slot is reused";
    EXPECT_FALSE(pool.cancel(old)) << "generation protects the new owner";
    EXPECT_TRUE(pool.isRunning(fresh));
}

TEST(SoftTimer, RestartFromCallbackFiresOnLaterPoll) {
    SoftTimerPool pool;
    Recorder r;
    TimerHandle again{};
    r.restartFrom = &pool;
    r.restartHandle = &again;
    pool.start(0, 0, r, 1);
    pool.poll(0);
    EXPECT_EQ(r.count, 1) << "timer restarted inside poll() must not fire in the same poll";
    r.restartFrom = nullptr;
    pool.poll(100);
    EXPECT_EQ(r.count, 2);
}

TEST(SoftTimer, FullPoolReturnsInvalidHandle) {
    SoftTimerPool pool;
    Recorder r;
    for (std::size_t i = 0; i < SoftTimerPool::kCapacity; ++i) EXPECT_TRUE(pool.start(0, 100, r, 0).valid());
    EXPECT_FALSE(pool.start(0, 100, r, 0).valid());
    EXPECT_FALSE(pool.cancel(TimerHandle{}));
}

TEST(SoftTimer, DeadlineAcrossTickWrap) {
    SoftTimerPool pool;
    Recorder r;
    pool.start(0xFFFFFFF0u, 0x20u, r, 0);
    pool.poll(0xFFFFFFFFu);
    EXPECT_EQ(r.count, 0);
    pool.poll(0x10u);
    EXPECT_EQ(r.count, 1);
}
