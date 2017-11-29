#include <hawktracer/timeline.h>
#include <hawktracer/events.h>

#include "test_common.h"

#include <gtest/gtest.h>

#include <thread>

class TestTimeline : public ::testing::Test
{
protected:
    void SetUp() override
    {
        _timeline = ht_timeline_create("simple_ts", "buffer-capacity", sizeof(HT_Event) * 3, nullptr);
    }

    void TearDown() override
    {
        ht_timeline_unregister_all_listeners(_timeline);
        ht_timeline_destroy(_timeline);
    }

    HT_Timeline* _timeline = nullptr;
};

// TODO test different types of events

TEST_F(TestTimeline, PublishEventsShouldNotifyListener)
{
    // Arrange
    NotifyInfo<HT_Event> info;

    ht_timeline_register_listener(_timeline, test_listener<HT_Event>, &info);

    // Act
    for (int i = 0; i < 10; i++)
    {
        HT_DECL_EVENT(HT_Event, event);
        ht_timeline_push_event(_timeline, &event);
    }

    // Assert
    ASSERT_EQ(9 * sizeof(HT_Event), info.notified_events); // last event not sent, because buffer is not full
    ASSERT_EQ(3, info.notify_count);
}

TEST_F(TestTimeline, ShouldNotCreateTimelineIfTypeNotRegistered)
{
    // Arrange & Act
    HT_Timeline* timeline = ht_timeline_create("invalid-type-name-which-definitely-does-not-exist", nullptr);

    // Assert
    ASSERT_EQ(nullptr, timeline);
}

TEST_F(TestTimeline, FlushEventsShouldNotifyListener)
{
    // Arrange
    NotifyInfo<HT_Event> info;

    ht_timeline_register_listener(_timeline, test_listener<HT_Event>, &info);

    HT_DECL_EVENT(HT_Event, event);
    ht_timeline_push_event(_timeline, &event);

    // Act
    ht_timeline_flush(_timeline);

    // Assert
    ASSERT_EQ(1 * sizeof(HT_Event), info.notified_events);
    ASSERT_EQ(1, info.notify_count);
}

TEST_F(TestTimeline, InitEventTwiceShouldIncreaseId)
{
    // Arrange
    HT_DECL_EVENT(HT_Event, event);

    ht_timeline_init_event(_timeline, &event);
    HT_EventId id = event.id;

    // Act
    ht_timeline_init_event(_timeline, &event);

    // Assert
    ASSERT_EQ(id, event.id - 1);
}

TEST_F(TestTimeline, InitEventShouldSetMonotonicTimestamp)
{
    // Arrange
    HT_DECL_EVENT(HT_Event, event);

    ht_timeline_init_event(_timeline, &event);
    HT_TimestampNs ts = event.timestamp;

    // Act
    ht_timeline_init_event(_timeline, &event);

    // Assert
    ASSERT_LT(ts, event.timestamp);
}

TEST_F(TestTimeline, ThreadSafeMessageShouldWorkWithMultipleThreads)
{
    // Arrange
    const size_t event_count = 20000;
    HT_Timeline* timeline = ht_timeline_create("simple_ts",
                                   "buffer-capacity", sizeof(HT_Event) * 3,
                                   "thread-safe", HT_TRUE, nullptr);
    NotifyInfo<HT_Event> info;
    ht_timeline_register_listener(timeline, test_listener<HT_Event>, &info);

    // Act
    std::thread th = std::thread([timeline] {
        for (size_t i = event_count / 2; i < event_count; i++)
        {
            HT_DECL_EVENT(HT_Event, event);
            event.timestamp = i;
            ht_timeline_push_event(timeline, &event);
        }
    });

    for (size_t i = 0; i < event_count / 2; i++)
    {
        HT_DECL_EVENT(HT_Event, event);
        event.timestamp = i;
        ht_timeline_push_event(timeline, &event);
    }

    th.join();

    ht_timeline_flush(timeline);

    // Assert
    std::vector<int> all_values(event_count, 1);
    size_t sum = 0;
    for (const auto& event : info.values)
    {
        ASSERT_GT(event_count, event.timestamp);
        sum += all_values[event.timestamp];
        all_values[event.timestamp] = 0;
    }

    ASSERT_EQ(event_count, sum);

    ht_timeline_destroy(timeline);
}