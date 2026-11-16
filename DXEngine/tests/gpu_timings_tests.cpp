// GpuTimingCollector: the bookkeeping behind the GPU profiler. Pure logic, no GPU.
#include "renderer/Core/GpuTimings.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace DXEngine;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)
static bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

// a query result in the IQuery::GetData layout: [frequency, begin0, end0, begin1, end1, ...]
static std::vector<uint64_t> Result(uint64_t frequency, std::initializer_list<uint64_t> ticks)
{
    std::vector<uint64_t> r{ frequency };
    r.insert(r.end(), ticks.begin(), ticks.end());
    return r;
}

static void RecordTwoScopes(GpuTimingCollector& c, uint32_t slot, const char* a = "shadow", const char* b = "opaque")
{
    c.BeginRecording(slot);
    CHECK(c.AddScope(slot, a) == 0);
    CHECK(c.AddScope(slot, b) == 1);
    c.EndRecording(slot);
}

static void TicksBecomeMilliseconds()
{
    GpuTimingCollector c(3);
    RecordTwoScopes(c, 0);
    CHECK(c.IsPending(0) && c.ScopeCount(0) == 2);

    // 10 MHz: 10,000 ticks = 1 ms. shadow 2 ms, a 0.5 ms gap, opaque 4 ms
    const auto r = Result(10'000'000, { 1000, 21000, 26000, 66000 });
    c.Resolve(0, r.data(), r.size());

    const GpuFrameTimings& t = c.Timings();
    CHECK(t.valid && t.scopes.size() == 2);
    CHECK(t.scopes[0].name == "shadow" && Near(t.scopes[0].ms, 2.0f));
    CHECK(t.scopes[1].name == "opaque" && Near(t.scopes[1].ms, 4.0f));
    CHECK(Near(t.totalMs, 6.5f));                        // first start to last end: the gap counts
    CHECK(Near(t.scopes[0].avgMs, 2.0f) && Near(t.avgTotalMs, 6.5f));   // the first frame starts the average
    CHECK(!c.IsPending(0));                              // consumed
}

static void AveragesFollowButMoveSlowly()
{
    GpuTimingCollector c(3);
    RecordTwoScopes(c, 0);
    auto r = Result(10'000'000, { 0, 20000, 20000, 60000 });          // 2 ms, 4 ms
    c.Resolve(0, r.data(), r.size());

    RecordTwoScopes(c, 1);
    r = Result(10'000'000, { 0, 120000, 120000, 160000 });            // 12 ms, 4 ms
    c.Resolve(1, r.data(), r.size());

    const GpuFrameTimings& t = c.Timings();
    CHECK(Near(t.scopes[0].ms, 12.0f));                  // the latest frame
    CHECK(Near(t.scopes[0].avgMs, 2.0f + (12.0f - 2.0f) * GpuTimingCollector::kAverageWeight));   // 3.0: a spike barely moves it
    CHECK(Near(t.scopes[1].avgMs, 4.0f));                // a steady scope stays put
}

static void ADifferentSetOfScopesStartsTheAverageOver()
{
    GpuTimingCollector c(3);
    RecordTwoScopes(c, 0);
    auto r = Result(10'000'000, { 0, 20000, 20000, 60000 });
    c.Resolve(0, r.data(), r.size());

    c.BeginRecording(1);                                  // a pass was removed: one scope now
    c.AddScope(1, "shadow");
    c.EndRecording(1);
    r = Result(10'000'000, { 0, 100000 });                // 10 ms
    c.Resolve(1, r.data(), r.size());

    const GpuFrameTimings& t = c.Timings();
    CHECK(t.scopes.size() == 1 && Near(t.scopes[0].avgMs, 10.0f));    // not blended with the old 2 ms
}

static void BadResultsKeepTheOldNumbers()
{
    GpuTimingCollector c(3);
    RecordTwoScopes(c, 0);
    auto good = Result(10'000'000, { 0, 20000, 20000, 60000 });
    c.Resolve(0, good.data(), good.size());

    RecordTwoScopes(c, 1);
    auto disjoint = Result(0, { 0, 999999, 0, 999999 });              // frequency 0: the GPU clock changed
    c.Resolve(1, disjoint.data(), disjoint.size());
    CHECK(Near(c.Timings().scopes[0].ms, 2.0f) && !c.IsPending(1));

    RecordTwoScopes(c, 2);
    auto tooShort = Result(10'000'000, { 0, 100 });                   // fewer values than the scopes need
    c.Resolve(2, tooShort.data(), tooShort.size());
    CHECK(Near(c.Timings().scopes[0].ms, 2.0f));

    RecordTwoScopes(c, 0);
    auto backwards = Result(10'000'000, { 500, 100, 100, 50 });       // end before begin: 0, never negative or huge
    c.Resolve(0, backwards.data(), backwards.size());
    CHECK(c.Timings().scopes[0].ms == 0.0f && c.Timings().scopes[1].ms == 0.0f);
    CHECK(c.Timings().totalMs >= 0.0f);
    c.Resolve(0, nullptr, 0);                                         // and a null result is ignored
}

static void ASlotThatNeverArrivedIsAbandonedNotBlockedOn()
{
    GpuTimingCollector c(3);
    RecordTwoScopes(c, 0);
    auto r = Result(10'000'000, { 0, 20000, 20000, 60000 });
    c.Resolve(0, r.data(), r.size());

    RecordTwoScopes(c, 1);
    CHECK(c.IsPending(1));
    c.Abandon(1);
    CHECK(!c.IsPending(1));
    CHECK(Near(c.Timings().scopes[0].ms, 2.0f));                      // the previous numbers stay
}

static void AFullFrameCountsWhatDidNotFit()
{
    GpuTimingCollector c(1);
    c.BeginRecording(0);
    for (uint32_t i = 0; i < GpuTimingCollector::kMaxScopes; ++i)
        CHECK(c.AddScope(0, "s" + std::to_string(i)) == static_cast<int>(i));
    CHECK(c.AddScope(0, "overflow") == -1);
    CHECK(c.AddScope(0, "overflow2") == -1);
    c.EndRecording(0);

    std::vector<uint64_t> r{ 1000 };
    for (uint32_t i = 0; i < GpuTimingCollector::kMaxScopes; ++i) { r.push_back(i * 10); r.push_back(i * 10 + 5); }
    c.Resolve(0, r.data(), r.size());
    CHECK(c.Timings().scopes.size() == GpuTimingCollector::kMaxScopes);
    CHECK(c.Timings().droppedScopes == 2);

    c.BeginRecording(0);                                              // a new frame starts clean
    CHECK(c.ScopeCount(0) == 0);
}

static void ResetForgetsEverything()
{
    GpuTimingCollector c(3);
    RecordTwoScopes(c, 0);
    auto r = Result(10'000'000, { 0, 20000, 20000, 60000 });
    c.Resolve(0, r.data(), r.size());
    RecordTwoScopes(c, 1);
    c.Reset();
    CHECK(!c.Timings().valid && c.Timings().scopes.empty() && !c.IsPending(1) && c.ScopeCount(1) == 0);
}

static void OutOfRangeSlotsAreHarmless()
{
    GpuTimingCollector c(2);
    c.BeginRecording(7);
    CHECK(c.AddScope(7, "x") == -1);
    c.EndRecording(7);
    c.Resolve(7, nullptr, 0);
    c.Abandon(7);
    CHECK(!c.IsPending(7) && c.ScopeCount(7) == 0);
    CHECK(GpuTimingCollector(0).SlotCount() == 1);                    // at least one slot
}

int main()
{
    TicksBecomeMilliseconds();
    AveragesFollowButMoveSlowly();
    ADifferentSetOfScopesStartsTheAverageOver();
    BadResultsKeepTheOldNumbers();
    ASlotThatNeverArrivedIsAbandonedNotBlockedOn();
    AFullFrameCountsWhatDidNotFit();
    ResetForgetsEverything();
    OutOfRangeSlotsAreHarmless();
    if (g_failures) { std::printf("%d check(s) failed\n", g_failures); return EXIT_FAILURE; }
    std::printf("gpu_timings_tests: all passed\n");
    return EXIT_SUCCESS;
}
