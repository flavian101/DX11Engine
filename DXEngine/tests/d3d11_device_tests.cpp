// ============================================================================
// d3d11_device_tests: the parts of the RHI that need a REAL D3D11 device: timestamp / occlusion / statistics queries,
// fences, V-Sync and the GPU profiler end to end (window, swap chain, command buffer, Present).
//
// It needs a GPU (or any D3D11 driver). Where no device can be created it prints SKIPPED and exits 0, so it never breaks a
// machine without one. Not part of the GPU-free suites: run it by hand (ctest -L gpu) after touching src/RHI or the profiler.
// ============================================================================
#include "RHI/GraphicsDevice.h"
#include "RHI/D3D11/D3D11Query.h"
#include "renderer/Core/GpuProfiler.h"
#include <cstdio>
#include <windows.h>

using namespace DXEngine;
using namespace DXEngine::RHI;

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)

static void WaitReady(const IQuery& q) { for (int i = 0; i < 2000 && !q.IsReady(); ++i) Sleep(1); }

// ---------------------------------------------------------------- queries and fences on a raw device
static bool TestQueriesAndFences()
{
    ComPtr<ID3D11Device> dev; ComPtr<ID3D11DeviceContext> ctx; D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx);
    if (FAILED(hr))
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx);
    if (FAILED(hr)) return false;

    // timestamp query: 4 slots, two written
    QueryDesc qd; qd.type = QueryType::Timestamp; qd.count = 4;
    D3D11Query ts(dev.Get(), ctx.Get(), qd);
    CHECK(ts.IsValid());
    CHECK(!ts.IsReady());                                 // the interval was never closed
    ts.Begin(0);
    ts.WriteTimestamp(0);
    ctx->Flush();
    ts.WriteTimestamp(1);
    ts.End(0);
    ctx->Flush();
    WaitReady(ts);
    CHECK(ts.IsReady());
    uint64_t data[5] = { 99, 99, 99, 99, 99 };
    CHECK(!ts.GetData(data, 8));                          // buffer too small
    CHECK(ts.GetData(data, sizeof(data)));
    CHECK(data[0] != 0);                                  // ticks per second
    CHECK(data[2] >= data[1]);                            // slot 1 was written after slot 0
    CHECK(data[3] == 0 && data[4] == 0);                  // never written: read 0, were not waited for

    ts.Begin(0); ts.WriteTimestamp(2); ts.End(0); ctx->Flush();   // a second interval starts clean
    WaitReady(ts);
    CHECK(ts.GetData(data, sizeof(data)) && data[1] == 0 && data[3] != 0);

    QueryDesc od; od.type = QueryType::Occlusion; od.count = 2;
    D3D11Query occ(dev.Get(), ctx.Get(), od);
    CHECK(occ.IsValid());
    occ.Begin(0); occ.End(0); occ.Begin(1); occ.End(1); ctx->Flush();
    WaitReady(occ);
    uint64_t samples[2] = { 7, 7 };
    CHECK(occ.GetData(samples, sizeof(samples)) && samples[0] == 0 && samples[1] == 0);   // nothing was drawn

    QueryDesc pd; pd.type = QueryType::PipelineStatistics; pd.count = 1;
    D3D11Query stats(dev.Get(), ctx.Get(), pd);
    CHECK(stats.IsValid());
    stats.Begin(0); stats.End(0); ctx->Flush();
    WaitReady(stats);
    PipelineStatisticsData st;
    CHECK(stats.GetData(&st, sizeof(st)) && st.vertexShaderInvocations == 0);

    QueryDesc bad; bad.type = QueryType::Timestamp; bad.count = 0;
    CHECK(!D3D11Query(dev.Get(), ctx.Get(), bad).IsValid());

    // fence
    FenceDesc fd; fd.initialValue = 5;
    D3D11Fence fence(dev.Get(), ctx.Get(), fd);
    CHECK(fence.IsValid() && fence.GetCompletedValue() == 5 && fence.GetLastSignaledValue() == 5);
    fence.Wait(3);                                        // already reached
    fence.Wait(99, 1000);                                 // never signalled: returns at once
    fence.Signal(6);
    CHECK(fence.GetLastSignaledValue() == 6);
    fence.Signal(6); fence.Signal(2);                     // not increasing: ignored
    CHECK(fence.GetLastSignaledValue() == 6);
    fence.Wait(6, 2'000'000'000ull);
    CHECK(fence.GetCompletedValue() == 6);
    fence.Signal(7); fence.Signal(8);
    fence.Wait(8);                                        // the earlier one is retired with it
    CHECK(fence.GetCompletedValue() == 8 && fence.GetLastSignaledValue() == 8);
    return true;
}

// ---------------------------------------------------------------- the device, V-Sync, profiler, Submit fences
static bool TestDeviceAndProfiler()
{
    WNDCLASSA wc = {}; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(nullptr); wc.lpszClassName = "d3d11_device_tests";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA("d3d11_device_tests", "d3d11_device_tests", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);

    std::shared_ptr<IGraphicsDevice> device(GraphicsDeviceFactory::Create(GraphicsAPI::DirectX11));
    if (!device || !device->Initialize(hwnd, 640, 480)) return false;

    CHECK(!device->IsVSync());                            // default: off, as before
    device->SetVSync(true);  CHECK(device->IsVSync());
    device->SetVSync(false); CHECK(!device->IsVSync());

    auto cmd = device->CreateCommandBuffer();
    GpuProfiler gpu;
    CHECK(gpu.Initialize(device) && gpu.IsAvailable());
    CHECK(!gpu.GetTimings().valid);

    for (int frame = 0; frame < 30; ++frame)
    {
        cmd->Begin();
        gpu.BeginFrame(*cmd);
        gpu.BeginScope(*cmd, "clear red");
        cmd->ClearRenderTarget(device->GetBackBuffer(), 1, 0, 0, 1);
        gpu.EndScope(*cmd);
        gpu.BeginScope(*cmd, "clear blue");
        cmd->ClearRenderTarget(device->GetBackBuffer(), 0, 0, 1, 1);
        gpu.EndScope(*cmd);
        gpu.EndFrame(*cmd);
        cmd->End();
        device->Submit(cmd.get());
        device->SetVSync(frame % 2 == 0);                 // both presentation intervals must work
        device->Present();
        if (frame == 0) CHECK(!gpu.GetTimings().valid);   // results arrive frames later, never in the same frame
        Sleep(2);
    }

    const GpuFrameTimings& t = gpu.GetTimings();
    CHECK(t.valid && t.scopes.size() == 2);
    if (t.scopes.size() == 2)
    {
        CHECK(t.scopes[0].name == "clear red" && t.scopes[1].name == "clear blue");
        CHECK(t.scopes[0].ms >= 0 && t.scopes[1].ms >= 0 && t.totalMs >= t.scopes[0].ms);
        std::printf("  GPU: %s %.4f ms, %s %.4f ms, total %.4f ms\n", t.scopes[0].name.c_str(), t.scopes[0].ms,
                    t.scopes[1].name.c_str(), t.scopes[1].ms, t.totalMs);
    }

    // switching off forgets the numbers; switching on measures again
    gpu.SetEnabled(false);
    cmd->Begin(); gpu.BeginFrame(*cmd); gpu.BeginScope(*cmd, "x"); gpu.EndScope(*cmd); gpu.EndFrame(*cmd); cmd->End();
    device->Submit(cmd.get()); device->Present();
    CHECK(!gpu.GetTimings().valid);
    gpu.SetEnabled(true);
    for (int i = 0; i < 12; ++i)
    {
        cmd->Begin(); gpu.BeginFrame(*cmd); gpu.BeginScope(*cmd, "again");
        cmd->ClearRenderTarget(device->GetBackBuffer(), 0, 1, 0, 1);
        gpu.EndScope(*cmd); gpu.EndFrame(*cmd); cmd->End();
        device->Submit(cmd.get()); device->Present(); Sleep(2);
    }
    CHECK(gpu.GetTimings().valid && gpu.GetTimings().scopes.size() == 1 && gpu.GetTimings().scopes[0].name == "again");

    // a fence given to Submit: each Submit signals the NEXT value
    auto fence = device->CreateFence(FenceDesc{});
    CHECK(fence != nullptr);
    cmd->Begin(); cmd->End();
    ICommandBuffer* list[] = { cmd.get() };
    device->Submit(list, 1, fence.get());
    fence->Wait(1, 5'000'000'000ull);
    CHECK(fence->GetCompletedValue() == 1 && fence->GetLastSignaledValue() == 1);
    device->Submit(list, 1, fence.get());
    device->Submit(list, 1, fence.get());
    CHECK(fence->GetLastSignaledValue() == 3);
    fence->Wait(3, 5'000'000'000ull);
    CHECK(fence->GetCompletedValue() == 3);

    gpu.Shutdown(); cmd.reset(); fence.reset(); device->Shutdown();
    DestroyWindow(hwnd);
    return true;
}

int main()
{
    if (!TestQueriesAndFences() || !TestDeviceAndProfiler())
    {
        std::printf("d3d11_device_tests: SKIPPED (no D3D11 device could be created)\n");
        return 0;
    }
    if (fails) { std::printf("%d check(s) failed\n", fails); return 1; }
    std::printf("d3d11_device_tests: all passed\n");
    return 0;
}
