// dear imgui: standalone example application for DirectX 12
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// FIXME: 64-bit only for now! (Because sizeof(ImTextureId) == sizeof(void*))

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include "nfd.h"
#include <chrono>
#include <thread>
#include <atomic>

#include "../stb/stb_image.h"
#include "../stb/stb_image_write.h"

// --------------------------- DF_SERIALIZE expansion ---------------------------

#include "../animatron.h"

#include "../df_serialize/MakeEqualityTests.h"
#include "editor_config.h"

#include "../rapidjson/document.h"
#include "../rapidjson/error/en.h"
#include "../rapidjson/writer.h" 
#include "../rapidjson/prettywriter.h"
#include "../rapidjson/stringbuffer.h"

#include "../df_serialize/MakeJSONWriteHeader.h"
#include "editor_config.h"
#include "../df_serialize/MakeJSONWriteFooter.h"

#include "internal/VariantTypeInfo.h"
#include "editor_config.h"

#include "internal/SchemaUI.h"
#include "editor_config.h"

bool ShowUI(Data::Document& value, const char* label)
{
    bool ret = false;
    ret |= ShowUI(value.outputSizeX, "outputSizeX");
    ret |= ShowUI(value.outputSizeY, "outputSizeY");
    ret |= ShowUI(value.renderSizeX, "renderSizeX");
    ret |= ShowUI(value.renderSizeY, "renderSizeY");
    ret |= ShowUI(value.duration, "duration");
    ret |= ShowUI(value.startTime, "startTime");
    ret |= ShowUI(value.FPS, "FPS");
    ret |= ShowUI(value.audioFile, "audioFile");
    ret |= ShowUI(value.blueNoiseDither, "blueNoiseDither");
    ret |= ShowUI(value.forceOpaqueOutput, "forceOpaqueOutput");
    ret |= ShowUI(value.samplesPerPixel, "samplesPerPixel");
    ret |= ShowUI(value.jitterSequenceType, "jitterSequenceType");
    return ret;
}

// --------------------------- DF_SERIALIZE expansion ---------------------------

#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

int g_width = 0;
int g_height = 0;

RootDocumentType g_rootDocument;
std::string g_rootDocumentFileName = "";
bool g_rootDocumentDirty = false;

RootDocumentType g_renderDocument;
Context g_renderDocumentContext;
ThreadContext g_renderDocumentThreadContext;

bool g_showImGUIDemo = false;
bool g_vsync = true; // turn this off for faster rendering in preview window

bool g_ctrl_s = false;

enum class RenderType
{
    video,
    gif,
    None,
};

bool g_wantsRender = false;
RenderType g_wantsRenderType = RenderType::None;

struct FrameContext
{
    ID3D12CommandAllocator* CommandAllocator;
    UINT64                  FenceValue;
};

// Data
static int const                    NUM_FRAMES_IN_FLIGHT = 3;
static FrameContext                 g_frameContext[NUM_FRAMES_IN_FLIGHT] = {};
static UINT                         g_frameIndex = 0;

static int const                    NUM_BACK_BUFFERS = 3;
static ID3D12Device*                g_pd3dDevice = NULL;
static ID3D12DescriptorHeap*        g_pd3dRtvDescHeap = NULL;
static ID3D12DescriptorHeap*        g_pd3dSrvDescHeap = NULL;
static ID3D12CommandQueue*          g_pd3dCommandQueue = NULL;
static ID3D12GraphicsCommandList*   g_pd3dCommandList = NULL;
static ID3D12Fence*                 g_fence = NULL;
static HANDLE                       g_fenceEvent = NULL;
static UINT64                       g_fenceLastSignaledValue = 0;
static IDXGISwapChain3*             g_pSwapChain = NULL;
static HANDLE                       g_hSwapChainWaitableObject = NULL;
static ID3D12Resource*              g_mainRenderTargetResource[NUM_BACK_BUFFERS] = {};
static D3D12_CPU_DESCRIPTOR_HANDLE  g_mainRenderTargetDescriptor[NUM_BACK_BUFFERS] = {};

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
void WaitForLastSubmittedFrame();
FrameContext* WaitForNextFrameResources();
void ResizeSwapChain(HWND hWnd, int width, int height);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND g_hwnd;

// TODO: animatron CLI uses this too, centralize it?
bool FileExists(const char* fileName)
{
    FILE* file = nullptr;
    fopen_s(&file, fileName, "rb");
    if (file)
    {
        fclose(file);
        return true;
    }
    return false;
}
void CopyFile(const char* src, const char* dest)
{
    std::vector<unsigned char> data;

    // read the data into memory
    {
        FILE* file = nullptr;
        fopen_s(&file, src, "rb");
        if (!file)
        {
            printf("Failed to copy file!! Could not open %s for read.\n", src);
            return;
        }

        fseek(file, 0, SEEK_END);
        data.resize(ftell(file));
        fseek(file, 0, SEEK_SET);

        fread(data.data(), data.size(), 1, file);
        fclose(file);
    }

    // write the new file
    {
        FILE* file = nullptr;
        fopen_s(&file, dest, "wb");
        if (!file)
        {
            printf("Failed to copy file!! Could not open %s for write.\n", dest);
            return;
        }

        fwrite(data.data(), data.size(), 1, file);
        fclose(file);
    }
}

bool HasEntity(const RootDocumentType& rootDocument, const char* entityId)
{
    for (const auto& entity : rootDocument.entities)
    {
        if (entity.id == entityId)
            return true;
    }
    return false;
}

void UpdateWindowTitle()
{
    char buffer[1024];
    if (g_rootDocumentFileName.empty())
        sprintf_s(buffer, "<untitled>%s - Editor", g_rootDocumentDirty ? "*" : "");
    else
        sprintf_s(buffer, "%s%s - Editor", g_rootDocumentFileName.c_str(), g_rootDocumentDirty ? "*" : "");

    bool ret = SetWindowTextA(g_hwnd, buffer);
}

bool ConfirmLoseChanges()
{
    if (!g_rootDocumentDirty)
        return true;
    return MessageBoxA(nullptr, "Your current document is not saved and all changes will be lost. Continue?", "Warning", MB_OKCANCEL) == IDOK;
}

bool HasFileExtension(const char* fileName, const char* extension)
{
    int fileNameLen = (int)strlen(fileName);
    int extensionLen = (int)strlen(extension);
    if (fileNameLen < extensionLen)
        return false;

    return !_stricmp(&fileName[fileNameLen - extensionLen], extension);
}

bool Load(const char* load)
{
        if (!ReadFromJSONFile(g_rootDocument, load))
        {
            printf("Could not load JSON file %s\n", load);
            return false;
        }

    return true;
}

// Some edits are expensive and annoying to happen immediately - like as you type text into a latex text field.
// Those edits need to be delayed until the user has stopped making edits for a short period of time.
// This is the time that those commits should happen.
std::chrono::high_resolution_clock::time_point g_editCommitTime;
bool g_editCommitDelayed = false;

bool g_previewRealTime = true;
std::chrono::high_resolution_clock::time_point g_previewRealTimeStart;
int g_previewRealTimeStartFrameOffset = 0;

int g_previewWidth = -1;
int g_previewHeight = -1;
size_t g_previewContextHash = 0;
ID3D12Resource* g_previewTexture = nullptr;
ID3D12Resource* g_previewUploadBuffers[NUM_FRAMES_IN_FLIGHT] = { nullptr };
D3D12_CPU_DESCRIPTOR_HANDLE  g_previewCpuDescHandle = {};
D3D12_GPU_DESCRIPTOR_HANDLE  g_previewGpuDescHandle = {};
int g_previewFrameIndex = 0;
bool g_playMode = false;

struct DelayedResourceDestruction
{
    ID3D12Resource* resource = nullptr;
    UINT frameIndex = 0;
};
std::vector<DelayedResourceDestruction> g_delayedResourceDestruction;

void ReleasePreviewResources()
{
    if (g_previewTexture)
    {
        g_delayedResourceDestruction.push_back({ g_previewTexture, g_frameIndex + NUM_FRAMES_IN_FLIGHT });
        g_previewTexture = nullptr;
    }

    for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
    {
        if (g_previewUploadBuffers[i])
        {
            g_delayedResourceDestruction.push_back({ g_previewUploadBuffers[i], g_frameIndex + NUM_FRAMES_IN_FLIGHT });
            g_previewUploadBuffers[i] = nullptr;
        }
    }
    g_previewWidth = -1;
    g_previewHeight = -1;
}

void UpdatePreviewResources()
{
    int desiredWidth = g_renderDocument.outputSizeX;
    int desiredHeight = g_renderDocument.outputSizeY;

    // if invalid size, leave alone
    if (desiredWidth <= 0 || desiredHeight <= 0)
        return;

    // if the preview resources are already the right size, nothing to do
    if (g_previewWidth == desiredWidth && g_previewHeight == desiredHeight)
        return;

    // destroy the existing resources if they exist
    ReleasePreviewResources();

    // make the preview texture
    D3D12_HEAP_PROPERTIES props;
    memset(&props, 0, sizeof(D3D12_HEAP_PROPERTIES));
    props.Type = D3D12_HEAP_TYPE_DEFAULT;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = desiredWidth;
    desc.Height = desiredHeight;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = g_pd3dDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL, IID_PPV_ARGS(&g_previewTexture));
    IM_ASSERT(SUCCEEDED(hr));

    // get the handles
    UINT size = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    g_previewCpuDescHandle = g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart();
    g_previewGpuDescHandle = g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart();
    g_previewCpuDescHandle.ptr += size;
    g_previewGpuDescHandle.ptr += size;

    // Create the texture view
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ZeroMemory(&srvDesc, sizeof(srvDesc));
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    g_pd3dDevice->CreateShaderResourceView(g_previewTexture, &srvDesc, g_previewCpuDescHandle);

    // describe the upload buffers needed to update that texture when needed
    UINT uploadPitch = (desiredWidth * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);
    UINT uploadSize = desiredHeight * uploadPitch;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = uploadSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    props.Type = D3D12_HEAP_TYPE_UPLOAD;
    props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    // make the upload buffers
    for (int i = 0; i < NUM_FRAMES_IN_FLIGHT; ++i)
    {
        hr = g_pd3dDevice->CreateCommittedResource(&props, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&g_previewUploadBuffers[i]));
        IM_ASSERT(SUCCEEDED(hr));
    }

    g_previewWidth = desiredWidth;
    g_previewHeight = desiredHeight;
}

bool ShouldDelayUpdate(const RootDocumentType& A, const RootDocumentType& B)
{
    // This returns true for things that are annoying to update each time you make an edit.
    // For instance, a latex string, so that it doesn't update (and freeze) each time you press a key.

    // delay preview resolution updates
    int desiredOutputSizeX = 640;
    int desiredOutputSizeY = int(float(desiredOutputSizeX) * float(A.outputSizeY) / float(A.outputSizeX));
    if (B.outputSizeX != desiredOutputSizeX || B.outputSizeY != desiredOutputSizeY)
        return true;

    bool ret = false;
    size_t entityCount = Min(A.entities.size(), B.entities.size());
    for (size_t entityIndex = 0; entityIndex < entityCount; ++entityIndex)
    {
        if (ret)
            break;

        const Data::Entity& eA = A.entities[entityIndex];
        const Data::Entity& eB = B.entities[entityIndex];

        if (eA.id != eB.id || eA.data._index != eB.data._index)
            continue;

        switch (eA.data._index)
        {
            case Data::EntityVariant::c_index_latex:
            {
                ret |= eA.data.latex.latex != eB.data.latex.latex;
                ret |= eA.data.latex.scale != eB.data.latex.scale;
                break;
            }
            case Data::EntityVariant::c_index_image:
            {
                ret |= eA.data.image.fileName != eB.data.image.fileName;
                break;
            }
        }
    }

    return ret;
}

bool UpdateRenderDocument()
{
    if (g_rootDocument.outputSizeX < 4 || g_rootDocument.outputSizeY < 4)
        return false;

    int desiredOutputSizeX = 640;
    int desiredOutputSizeY = int(float(desiredOutputSizeX) * float(g_rootDocument.outputSizeY) / float(g_rootDocument.outputSizeX));

    if (desiredOutputSizeY > 4096)
        return false;

    // We keep a separate render document because the validation and fixup modifies the document data
    g_renderDocument = g_rootDocument;

    g_renderDocument.outputSizeX = desiredOutputSizeX;
    g_renderDocument.outputSizeY = desiredOutputSizeY;

    g_renderDocumentContext.frameCache.Reset();
    g_renderDocumentThreadContext.threadId = 0;
    ValidateAndFixupDocument(g_renderDocument);

    return true;
}

void OnDocumentChange(bool forceImmediateRenderDocumentUpdate)
{
    if (!forceImmediateRenderDocumentUpdate && ShouldDelayUpdate(g_rootDocument, g_renderDocument))
    {
        g_editCommitTime = std::chrono::high_resolution_clock::now() + std::chrono::seconds(1);
        g_editCommitDelayed = true;
        return;
    }

    UpdateRenderDocument();
}

void LoadConfig()
{
    ReadFromJSONFile(g_rootDocument.config, "internal/config.json");
}

void NewDocument()
{
    g_rootDocument = RootDocumentType{};
    g_rootDocumentFileName = "";
    g_rootDocumentDirty = false;

    g_rootDocument.program = "animatron";
    g_rootDocument.versionMajor = c_documentVersionMajor;
    g_rootDocument.versionMinor = c_documentVersionMinor;

    g_rootDocument.config.versionMajor = c_configVersionMajor;
    g_rootDocument.config.versionMinor = c_configVersionMinor;

    LoadConfig();
}

void OnNewFrame()
{
    // handle delayed destruction. delete anything that is old enough not to be referenced by any in flight command lists
    g_delayedResourceDestruction.erase(
        std::remove_if(g_delayedResourceDestruction.begin(),
            g_delayedResourceDestruction.end(),
            [](DelayedResourceDestruction& d)
            {
                if (d.frameIndex <= g_frameIndex)
                    return false;

                d.resource->Release();
                return true;
            }
        ),
        g_delayedResourceDestruction.end()
    );
}

bool ShowKeyframes(size_t entityIndex)
{
    std::string& entityId = g_rootDocument.entities[entityIndex].id;
    bool ret = false;

    ImGui::Separator();
    if (ImGui::TreeNode("Key Frames"))
    {
        int deleteIndex = -1;
        int keyFrameIndex = -1;
        for (Data::KeyFrame& keyFrame : g_rootDocument.keyFrames)
        {
            keyFrameIndex++;
            if (keyFrame.entityId != entityId)
                continue;

            ImGui::Separator();

            ImGui::PushID(keyFrameIndex);

            ret |= ShowUI(keyFrame.time, "time");
            ret |= ShowUI(keyFrame.newValue, "newValue");
            ret |= ShowUI(keyFrame.blendControlPoints, "blendControlPoints");

            if (ImGui::Button("Delete"))
                deleteIndex = keyFrameIndex;

            ImGui::PopID();
        }

        ImGui::Separator();

        if (ImGui::Button("Add"))
        {
            g_rootDocument.keyFrames.push_back(Data::KeyFrame());
            Data::KeyFrame& keyFrame = *g_rootDocument.keyFrames.rbegin();
            keyFrame.entityId = entityId;
            ret = true;
        }

        if (deleteIndex >= 0)
        {
            g_rootDocument.keyFrames.erase(g_rootDocument.keyFrames.begin() + deleteIndex);
            ret = true;
        }

        ImGui::TreePop();
    }
    return ret;
}

std::vector<std::thread> g_renderThreads;
std::atomic<bool> g_renderThreadCancel = false;
std::atomic<int> g_renderThreadNextThreadId = 0;
std::atomic<int> g_renderThreadNextFrame = 0;
std::atomic<int> g_renderThreadRenderedFrames = 0;
int g_renderThreadFrameTotal = 0;
std::vector<ThreadContext> g_renderThreadContexts;
Context g_renderThreadContext;
RootDocumentType g_renderThreadDocument;
bool g_renderingInProgress = false;

void RenderThread()
{
    int threadId = g_renderThreadNextThreadId++;
    ThreadContext& threadContext = g_renderThreadContexts[threadId];

    while (1)
    {
        if (g_renderThreadCancel)
            return;

        int frameIndex = g_renderThreadNextFrame++;
        if (frameIndex >= g_renderThreadFrameTotal)
            return;

        // render a frame
        int recycledFrameIndex = -1;
        size_t frameHash = 0;
        if (!RenderFrame(g_renderThreadDocument, frameIndex, threadContext, g_renderThreadContext, recycledFrameIndex, frameHash))
        {
            // TODO: how to handle errors? should report them to user somehow
            //wasError = true;
            break;
        }

        // write it out
        if (recycledFrameIndex == -1)
        {
            sprintf_s(threadContext.outFileName, "build/%i.%s", frameIndex, (g_renderThreadDocument.config.writeFrames == Data::ImageFileType::PNG) ? "png" : "bmp");
            if (g_renderThreadDocument.config.writeFrames == Data::ImageFileType::PNG)
                stbi_write_png(threadContext.outFileName, g_renderThreadDocument.outputSizeX, g_renderThreadDocument.outputSizeY, 4, threadContext.pixelsU8.data(), g_renderThreadDocument.outputSizeX * 4);
            else
                stbi_write_bmp(threadContext.outFileName, g_renderThreadDocument.outputSizeX, g_renderThreadDocument.outputSizeY, 4, threadContext.pixelsU8.data());

            // Set this frame in the frame cache for recycling
            g_renderThreadContext.frameCache.SetFrame(frameHash, frameIndex, threadContext.pixelsU8);
        }
        else
        {
            const char* extension = (g_renderThreadDocument.config.writeFrames == Data::ImageFileType::PNG) ? "png" : "bmp";
            char fileName1[256];
            sprintf_s(fileName1, "build/%i.%s", recycledFrameIndex, extension);
            char fileName2[256];
            sprintf_s(fileName2, "build/%i.%s", frameIndex, extension);
            CopyFile(fileName1, fileName2);
        }

        g_renderThreadRenderedFrames++;
    }
}

void StartRenderThreads()
{
    // We keep a separate render document because the validation and fixup modifies the document data
    g_renderThreadDocument = g_rootDocument;
    g_renderThreadContext.frameCache.Reset();
    ValidateAndFixupDocument(g_renderThreadDocument);

    g_renderThreadNextThreadId = 0;
    g_renderThreadNextFrame = 0;
    g_renderThreadRenderedFrames = 0;
    g_renderThreadFrameTotal = TotalFrameCount(g_rootDocument);
    g_renderThreadCancel = false;
    g_renderingInProgress = true;

    unsigned int numThreads = std::thread::hardware_concurrency();

    g_renderThreads.resize(numThreads);
    g_renderThreadContexts.resize(numThreads);
    for (unsigned int i = 0; i < numThreads; ++i)
        g_renderThreadContexts[i].threadId = i;

    for (std::thread& t : g_renderThreads)
        t = std::thread(RenderThread);
}

void EndRenderThreads()
{
    for (std::thread& t : g_renderThreads)
        t.join();
    g_renderThreads.clear();
    g_renderThreadContext.frameCache.Reset();

    if (g_renderThreadCancel)
        return;

    // make output file name
    char destFileBuffer[1024];
    strcpy_s(destFileBuffer, g_rootDocumentFileName.c_str());
    int len = (int)strlen(destFileBuffer);
    while (len > 0 && destFileBuffer[len] != '.')
        len--;
    if (len == 0)
    {
        switch (g_wantsRenderType)
        {
            case RenderType::video:strcpy_s(destFileBuffer, "out.mp4"); break;
            case RenderType::gif:strcpy_s(destFileBuffer, "out.gif"); break;
        }
    }
    else
    {
        destFileBuffer[len] = 0;
        switch (g_wantsRenderType)
        {
            case RenderType::video:strcat_s(destFileBuffer, ".mp4"); break;
            case RenderType::gif:strcat_s(destFileBuffer, ".gif"); break;
        }
    }

    // have ffmpeg assemble it!
    bool hasAudio = g_wantsRenderType != RenderType::gif && FileExists(g_renderThreadDocument.audioFile.c_str());

    char inputs[1024];
    if (!hasAudio)
        sprintf_s(inputs, "-i build/%%d.%s", (g_renderThreadDocument.config.writeFrames == Data::ImageFileType::PNG) ? "png" : "bmp");
    else
        sprintf_s(inputs, "-i build/%%d.%s -i %s", (g_renderThreadDocument.config.writeFrames == Data::ImageFileType::PNG) ? "png" : "bmp", g_renderThreadDocument.audioFile.c_str());

    char audioOptions[1024];
    if (hasAudio)
        sprintf_s(audioOptions, " -c:a aac -b:a 384k ");
    else
        sprintf_s(audioOptions, " ");

    int framesTotal = TotalFrameCount(g_renderThreadDocument);

    char containerOptions[1024];
    switch (g_wantsRenderType)
    {
        case RenderType::video:
        {
            sprintf_s(containerOptions, "-frames:v %i -movflags faststart -c:v libx264 -profile:v high -bf 2 -g 30 -crf 18 -pix_fmt yuv420p %s", framesTotal, hasAudio ? "-filter_complex \"[1:0] apad \" -shortest" : "");
            break;
        }
        case RenderType::gif:
        {
            sprintf_s(containerOptions, "-frames:v %i -f gif", framesTotal);
        }
    }

    char buffer[1024];
    sprintf_s(buffer, "%s -y -framerate %i %s%s%s %s", g_renderThreadDocument.config.ffmpeg.c_str(), g_renderThreadDocument.FPS, inputs, audioOptions, containerOptions, destFileBuffer);

    system(buffer);
}

// Main code
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
    if (__argc > 1)
    {
        if (!Load(__argv[1]))
        {
            printf("could not load file %s\n", __argv[1]);
            NewDocument();
        }
        else
        {
            g_rootDocumentFileName = __argv[1];
        }
    }
    else
    {
        NewDocument();
    }

    char currentDirectory[1024];
    GetCurrentDirectoryA(1024, currentDirectory);

    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("Editor"), NULL };
    ::RegisterClassEx(&wc);
    HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("Editor"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);
    g_hwnd = hwnd;

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClass(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Custom styling
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.IndentSpacing = 10.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(g_pd3dDevice, NUM_FRAMES_IN_FLIGHT,
        DXGI_FORMAT_R8G8B8A8_UNORM, g_pd3dSrvDescHeap,
        g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    OnDocumentChange(true);
    UpdateWindowTitle();

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        g_frameIndex = (g_frameIndex + 1) % NUM_FRAMES_IN_FLIGHT;
        OnNewFrame();

        // Save hotkey
        if (!g_rootDocumentFileName.empty() && g_ctrl_s && g_rootDocumentDirty)
        {
            if (!WriteToJSONFile(g_rootDocument, g_rootDocumentFileName.c_str()))
                MessageBoxA(nullptr, "Could not save JSON file", "Error", MB_OK);
            g_rootDocumentDirty = false;
            UpdateWindowTitle();
        }

        if (g_showImGUIDemo)
            ImGui::ShowDemoWindow();

        // Our editor UI
        {
            // take up the whole screen
            ImGui::SetNextWindowPos(ImVec2{ 0.0f, 0.0f });
            ImGui::SetNextWindowSize(ImVec2{ float(g_width), float(g_height) });
            
            ImGui::Begin("df_serialize editor", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

            // Menu
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("New", "Ctrl+N") && ConfirmLoseChanges())
                    {
                        NewDocument();
                        OnDocumentChange(true);
                        UpdateWindowTitle();
                    }

                    if (ImGui::MenuItem("Open", "Ctrl+O") && ConfirmLoseChanges())
                    {
                        nfdchar_t* output = nullptr;
                        if (NFD_OpenDialog("json", currentDirectory, &output) == NFD_OKAY)
                        {
                            g_rootDocumentDirty = false;
                            g_rootDocument = RootDocumentType{};
                            if (!ReadFromJSONFile(g_rootDocument, output))
                            {
                                g_rootDocument = RootDocumentType{};
                                MessageBoxA(nullptr, "Could not load JSON file", "Error", MB_OK);
                            }
                            else
                            {
                                g_rootDocumentFileName = output;
                                UpdateWindowTitle();
                            }
                            LoadConfig();
                            OnDocumentChange(true);
                        }
                    }

                    if (ImGui::MenuItem("Save", "Ctrl+S", false, !g_rootDocumentFileName.empty() && g_rootDocumentDirty))
                    {
                        if (!WriteToJSONFile(g_rootDocument, g_rootDocumentFileName.c_str()))
                            MessageBoxA(nullptr, "Could not save JSON file", "Error", MB_OK);
                        g_rootDocumentDirty = false;
                        UpdateWindowTitle();
                    }

                    if (ImGui::MenuItem("Save As...", "Ctrl+A"))
                    {
                        nfdchar_t* output = nullptr;
                        if (NFD_SaveDialog("json", currentDirectory, &output) == NFD_OKAY)
                        {
                            if (!WriteToJSONFile(g_rootDocument, output))
                                MessageBoxA(nullptr, "Could not save JSON file", "Error", MB_OK);
                            else
                            {
                                g_rootDocumentFileName = output;
                                g_rootDocumentDirty = false;
                                UpdateWindowTitle();
                            }
                        }
                    }
                    if (ImGui::MenuItem("Exit","Alt+f4") && ConfirmLoseChanges()) { ::PostQuitMessage(0); }

                    ImGui::EndMenu();
                }
                if (ImGui::BeginMenu("Render"))
                {
                    if (ImGui::MenuItem("Render Video", "Ctrl+R"))
                    {
                        g_wantsRender = true;
                        g_wantsRenderType = RenderType::video;
                    }

                    if (ImGui::MenuItem("Render Gif", ""))
                    {
                        g_wantsRender = true;
                        g_wantsRenderType = RenderType::gif;
                    }

                    if (ImGui::MenuItem("VSync", "", g_vsync))
                        g_vsync = !g_vsync;
                    ImGui::Separator();
                    if (ImGui::MenuItem("Show IMGUI Demo Window", "", g_showImGUIDemo))
                        g_showImGUIDemo = !g_showImGUIDemo;
                    ImGui::EndMenu();
                }
                ImGui::EndMenuBar();
            }

            {
                ImGui::Columns(2);

                // entity list and property panel
                {
                    // Show entity list
                    static size_t selected = 0;
                    {
                        ImGui::BeginChild("entities", ImVec2(150, 0), true);

                        if (ImGui::Selectable("Document", selected == 0))
                            selected = 0;

                        ImGui::Indent(5.0f);

                        for (size_t index = 0; index < g_rootDocument.entities.size(); ++index)
                        {
                            char entityName[256];
                            if (g_rootDocument.entities[index].id.empty())
                                sprintf_s(entityName, "<Entity %i>", (int)index);
                            else
                                strcpy_s(entityName, g_rootDocument.entities[index].id.c_str());

                            if (ImGui::Selectable(entityName, selected == index + 1))
                                selected = index + 1;
                        }

                        ImGui::Unindent();

                        if (ImGui::Button("Add"))
                        {
                            char entityId[256];
                            int index = 0;
                            do
                            {
                                sprintf_s(entityId, "Entity %i", index);
                                index++;
                            } while (HasEntity(g_rootDocument, entityId));

                            g_rootDocument.entities.resize(g_rootDocument.entities.size() + 1);
                            g_rootDocument.entities.rbegin()->id = entityId;

                            OnDocumentChange(false);
                            if (!g_rootDocumentDirty)
                            {
                                g_rootDocumentDirty = true;
                                UpdateWindowTitle();
                            }
                        }

                        ImGui::SameLine();

                        if (ImGui::Button("Delete"))
                        {
                            if (selected > 0 && selected - 1 < g_rootDocument.entities.size())
                            {
                                g_rootDocument.entities.erase(g_rootDocument.entities.begin() + selected - 1);

                                OnDocumentChange(false);
                                if (!g_rootDocumentDirty)
                                {
                                    g_rootDocumentDirty = true;
                                    UpdateWindowTitle();
                                }
                            }
                        }

                        ImGui::EndChild();
                    }

                    // Show entity being edited
                    ImGui::SameLine();
                    {
                        ImGui::BeginChild("entity", ImVec2(0, 0), true);

                        ImGui::PushItemWidth(-150);

                        if (selected-1 < g_rootDocument.entities.size())
                        {
                            std::string oldId = g_rootDocument.entities[selected - 1].id;

                            bool changed = ShowUI(g_rootDocument.entities[selected-1], "");

                            // If the Id changed, we need to update the key frames to use the new id!
                            // We also need to update the parents
                            if (g_rootDocument.entities[selected - 1].id != oldId)
                            {
                                for (Data::KeyFrame& keyFrame : g_rootDocument.keyFrames)
                                {
                                    if (keyFrame.entityId == oldId)
                                        keyFrame.entityId = g_rootDocument.entities[selected - 1].id;
                                }

                                for (Data::Entity& entity : g_rootDocument.entities)
                                {
                                    if (entity.parent == oldId)
                                        entity.parent = g_rootDocument.entities[selected - 1].id;
                                }
                            }

                            changed |= ShowKeyframes(selected-1);

                            if (changed)
                            {
                                OnDocumentChange(false);
                                if (!g_rootDocumentDirty)
                                {
                                    g_rootDocumentDirty = true;
                                    UpdateWindowTitle();
                                }
                            }
                        }
                        else
                        {
                            selected = 0;
                            bool changed = ShowUI(g_rootDocument, "");
                            if (changed)
                            {
                                OnDocumentChange(false);
                                if (!g_rootDocumentDirty)
                                {
                                    g_rootDocumentDirty = true;
                                    UpdateWindowTitle();
                                }
                            }
                        }

                        ImGui::PopItemWidth();

                        ImGui::EndChild();
                    }
                }

                ImGui::NextColumn();

                // frame scrubber
                {
                    int totalFrames = TotalFrameCount(g_rootDocument);
                    if (g_playMode)
                    {
                        if (g_previewRealTime)
                        {
                            std::chrono::duration<float> seconds = (std::chrono::high_resolution_clock::now() - g_previewRealTimeStart);
                            g_previewFrameIndex = g_previewRealTimeStartFrameOffset + int(seconds.count() * float(g_rootDocument.FPS));
                        }
                        else
                        {
                            g_previewFrameIndex++;
                        }
                        if (g_previewFrameIndex >= totalFrames)
                            g_playMode = false;
                    }

                    g_previewFrameIndex = Clamp(g_previewFrameIndex, 0, totalFrames - 1);

                    ImGui::Text("Frame");

                    ImGui::SameLine();

                    ImGui::PushID("Frame");

                    if (ImGui::SliderInt("", &g_previewFrameIndex, 0, totalFrames - 1))
                    {
                        g_previewRealTimeStartFrameOffset = g_previewFrameIndex;
                        g_previewRealTimeStart = std::chrono::high_resolution_clock::now();
                    }

                    ImGui::PopID();

                    int min, sec, tmin, tsec;
                    FrameIndexToRelativeMinutesSeconds(g_rootDocument, g_previewFrameIndex+1, min, sec);
                    FrameIndexToRelativeMinutesSeconds(g_rootDocument, totalFrames, tmin, tsec);
                    ImGui::Text("%02d:%02d / %02d:%02d", min, sec, tmin, tsec);

                    ImGui::SameLine();

                    if (!g_playMode && ImGui::Button("Play"))
                    {
                        g_previewRealTimeStartFrameOffset = g_previewFrameIndex;
                        g_previewRealTimeStart = std::chrono::high_resolution_clock::now();
                        g_playMode = true;
                    }
                    else if (g_playMode && ImGui::Button("Stop"))
                    {
                        g_playMode = false;
                    }

                    ImGui::SameLine();

                    if (ImGui::Button("Rewind"))
                    {
                        g_previewFrameIndex = 0;
                        g_previewRealTimeStartFrameOffset = 0;
                        g_previewRealTimeStart = std::chrono::high_resolution_clock::now();
                    }

                    ImGui::SameLine();

                    if (ImGui::Checkbox("Realtime", &g_previewRealTime))
                    {
                        if (g_previewRealTime && g_playMode)
                        {
                            g_previewRealTimeStartFrameOffset = g_previewFrameIndex;
                            g_previewRealTimeStart = std::chrono::high_resolution_clock::now();
                        }
                    }
                }

                if (g_previewGpuDescHandle.ptr)
                {
                    ImVec2 uv_min = ImVec2(0.0f, 0.0f);                 // Top-left
                    ImVec2 uv_max = ImVec2(1.0f, 1.0f);                 // Lower-right
                    ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
                    ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white
                    ImGui::Image((ImTextureID)g_previewGpuDescHandle.ptr, ImVec2((float)g_previewWidth, (float)g_previewHeight), uv_min, uv_max, tint_col, border_col);
                }

                ImGui::NextColumn();
            }

            ImGui::End();

            g_ctrl_s = false;
        }

        // Render progress dialog
        {
            // g_wantsRender is because i couldn't open the popup in the menu directly for some reason
            if (g_wantsRender)
            {
                g_wantsRender = false;
                ImGui::OpenPopup("Rendering Video");
                StartRenderThreads();
            }
            {
                // Always center this window when appearing
                ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

                if (ImGui::BeginPopupModal("Rendering Video", NULL, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    int framesRendered = g_renderThreadRenderedFrames.load();
                    int framesTotal = g_renderThreadFrameTotal;

                    ImGui::Text("Rendering Frame %i / %i", framesRendered, framesTotal);
                    ImGui::ProgressBar(float(framesRendered) / float(framesTotal));

                    // run ffmpeg when done
                    if (g_renderingInProgress && framesRendered >= framesTotal)
                    {
                        g_renderingInProgress = false;
                        EndRenderThreads();
                    }

                    // Cancel button while rendering, ok button when finished
                    if (g_renderingInProgress && ImGui::Button("Cancel", ImVec2(120, 0)))
                    {
                        g_renderingInProgress = false;
                        g_renderThreadCancel = true;
                        EndRenderThreads();
                        ImGui::CloseCurrentPopup();
                    }
                    else if (!g_renderingInProgress && ImGui::Button("OK", ImVec2(120, 0)))
                    {
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::EndPopup();
                }
            }
        }

        UpdatePreviewResources();

        // Rendering
        FrameContext* frameCtxt = WaitForNextFrameResources();
        UINT backBufferIdx = g_pSwapChain->GetCurrentBackBufferIndex();
        frameCtxt->CommandAllocator->Reset();

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource   = g_mainRenderTargetResource[backBufferIdx];
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;

        g_pd3dCommandList->Reset(frameCtxt->CommandAllocator, NULL);
        g_pd3dCommandList->ResourceBarrier(1, &barrier);
        g_pd3dCommandList->ClearRenderTargetView(g_mainRenderTargetDescriptor[backBufferIdx], (float*)&clear_color, 0, NULL);
        g_pd3dCommandList->OMSetRenderTargets(1, &g_mainRenderTargetDescriptor[backBufferIdx], FALSE, NULL);
        g_pd3dCommandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);

        // Render and display the frame
        {
            // render the current frame
            size_t frameHash = 0;
            {
                int recycledFrameIndex = -1;
                RenderFrame(g_renderDocument, g_previewFrameIndex, g_renderDocumentThreadContext, g_renderDocumentContext, recycledFrameIndex, frameHash);

                if (recycledFrameIndex == -1)
                    g_renderDocumentContext.frameCache.SetFrame(frameHash, g_previewFrameIndex, g_renderDocumentThreadContext.pixelsU8);
            }

            // If the preview hash is different than this frame's hash we need to upload it to the GPU and copy it into the preview texture
            if (frameHash != g_previewContextHash)
            {
                g_previewContextHash = frameHash;

                // Copy textures from CPU to upload buffer
                {
                    void* mapped = NULL;
                    HRESULT hr = g_previewUploadBuffers[g_frameIndex % NUM_FRAMES_IN_FLIGHT]->Map(0, nullptr, &mapped);
                    IM_ASSERT(SUCCEEDED(hr));

                    UINT uploadPitch = (g_previewWidth * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);

                    for (int y = 0; y < g_previewHeight; ++y)
                    {
                        unsigned char* dest = &((unsigned char*)mapped)[y * uploadPitch];
                        const Data::ColorU8* src = &g_renderDocumentThreadContext.pixelsU8[y * g_renderDocument.renderSizeX];
                        memcpy(dest, src, g_renderDocument.renderSizeX * 4);
                    }

                    g_previewUploadBuffers[g_frameIndex % NUM_FRAMES_IN_FLIGHT]->Unmap(0, nullptr);
                }

                // transition preview texture from shader resource to copy dest
                {
                    D3D12_RESOURCE_BARRIER barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                    barrier.Transition.pResource = g_previewTexture;
                    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

                    g_pd3dCommandList->ResourceBarrier(1, &barrier);
                }

                // copy from upload buffer to texture
                {
                    UINT uploadPitch = (g_previewWidth * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1u);

                    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
                    srcLocation.pResource = g_previewUploadBuffers[g_frameIndex % NUM_FRAMES_IN_FLIGHT];
                    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                    srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    srcLocation.PlacedFootprint.Footprint.Width = g_previewWidth;
                    srcLocation.PlacedFootprint.Footprint.Height = g_previewHeight;
                    srcLocation.PlacedFootprint.Footprint.Depth = 1;
                    srcLocation.PlacedFootprint.Footprint.RowPitch = uploadPitch;

                    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
                    dstLocation.pResource = g_previewTexture;
                    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                    dstLocation.SubresourceIndex = 0;

                    g_pd3dCommandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
                }

                // transition preview texture from copy dest to shader resource
                {
                    D3D12_RESOURCE_BARRIER barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                    barrier.Transition.pResource = g_previewTexture;
                    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

                    g_pd3dCommandList->ResourceBarrier(1, &barrier);
                }
            }
        }

        ImGui::Render();
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), g_pd3dCommandList);
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
        g_pd3dCommandList->ResourceBarrier(1, &barrier);
        g_pd3dCommandList->Close();

        g_pd3dCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList* const*)&g_pd3dCommandList);

        if(g_vsync)
            g_pSwapChain->Present(1, 0); // Present with vsync
        else
            g_pSwapChain->Present(0, 0); // Present without vsync

        UINT64 fenceValue = g_fenceLastSignaledValue + 1;
        g_pd3dCommandQueue->Signal(g_fence, fenceValue);
        g_fenceLastSignaledValue = fenceValue;
        frameCtxt->FenceValue = fenceValue;

        if (g_editCommitDelayed && std::chrono::high_resolution_clock::now() > g_editCommitTime)
        {
            if(UpdateRenderDocument())
                g_editCommitDelayed = false;
        }
    }

    ReleasePreviewResources();

    WaitForLastSubmittedFrame();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    for (DelayedResourceDestruction& destruction : g_delayedResourceDestruction)
        destruction.resource->Release();
    g_delayedResourceDestruction.clear();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC1 sd;
    {
        ZeroMemory(&sd, sizeof(sd));
        sd.BufferCount = NUM_BACK_BUFFERS;
        sd.Width = 0;
        sd.Height = 0;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.Stereo = FALSE;
    }

    // [DEBUG] Enable debug interface
#ifdef DX12_ENABLE_DEBUG_LAYER
    ID3D12Debug* pdx12Debug = NULL;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pdx12Debug))))
        pdx12Debug->EnableDebugLayer();
#endif

    // Create device
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    if (D3D12CreateDevice(NULL, featureLevel, IID_PPV_ARGS(&g_pd3dDevice)) != S_OK)
        return false;

    // [DEBUG] Setup debug interface to break on any warnings/errors
#ifdef DX12_ENABLE_DEBUG_LAYER
    if (pdx12Debug != NULL)
    {
        ID3D12InfoQueue* pInfoQueue = NULL;
        g_pd3dDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue));
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        pInfoQueue->Release();
        pdx12Debug->Release();
    }
#endif

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = NUM_BACK_BUFFERS;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 1;
        if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dRtvDescHeap)) != S_OK)
            return false;

        SIZE_T rtvDescriptorSize = g_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_pd3dRtvDescHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        {
            g_mainRenderTargetDescriptor[i] = rtvHandle;
            rtvHandle.ptr += rtvDescriptorSize;
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 2; // 1 for font, 1 for preview texture
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (g_pd3dDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)) != S_OK)
            return false;
    }

    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = 1;
        if (g_pd3dDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&g_pd3dCommandQueue)) != S_OK)
            return false;
    }

    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_frameContext[i].CommandAllocator)) != S_OK)
            return false;

    if (g_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_frameContext[0].CommandAllocator, NULL, IID_PPV_ARGS(&g_pd3dCommandList)) != S_OK ||
        g_pd3dCommandList->Close() != S_OK)
        return false;

    if (g_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)) != S_OK)
        return false;

    g_fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (g_fenceEvent == NULL)
        return false;

    {
        IDXGIFactory4* dxgiFactory = NULL;
        IDXGISwapChain1* swapChain1 = NULL;
        if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK ||
            dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1) != S_OK ||
            swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain)) != S_OK)
            return false;
        swapChain1->Release();
        dxgiFactory->Release();
        g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);
        g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
    }

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_hSwapChainWaitableObject != NULL) { CloseHandle(g_hSwapChainWaitableObject); }
    for (UINT i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
        if (g_frameContext[i].CommandAllocator) { g_frameContext[i].CommandAllocator->Release(); g_frameContext[i].CommandAllocator = NULL; }
    if (g_pd3dCommandQueue) { g_pd3dCommandQueue->Release(); g_pd3dCommandQueue = NULL; }
    if (g_pd3dCommandList) { g_pd3dCommandList->Release(); g_pd3dCommandList = NULL; }
    if (g_pd3dRtvDescHeap) { g_pd3dRtvDescHeap->Release(); g_pd3dRtvDescHeap = NULL; }
    if (g_pd3dSrvDescHeap) { g_pd3dSrvDescHeap->Release(); g_pd3dSrvDescHeap = NULL; }
    if (g_fence) { g_fence->Release(); g_fence = NULL; }
    if (g_fenceEvent) { CloseHandle(g_fenceEvent); g_fenceEvent = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }

#ifdef DX12_ENABLE_DEBUG_LAYER
    IDXGIDebug1* pDebug = NULL;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
#endif
}

void CreateRenderTarget()
{
    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
    {
        ID3D12Resource* pBackBuffer = NULL;
        g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&pBackBuffer));
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, g_mainRenderTargetDescriptor[i]);
        g_mainRenderTargetResource[i] = pBackBuffer;
    }
}

void CleanupRenderTarget()
{
    WaitForLastSubmittedFrame();

    for (UINT i = 0; i < NUM_BACK_BUFFERS; i++)
        if (g_mainRenderTargetResource[i]) { g_mainRenderTargetResource[i]->Release(); g_mainRenderTargetResource[i] = NULL; }
}

void WaitForLastSubmittedFrame()
{
    FrameContext* frameCtxt = &g_frameContext[g_frameIndex % NUM_FRAMES_IN_FLIGHT];

    UINT64 fenceValue = frameCtxt->FenceValue;
    if (fenceValue == 0)
        return; // No fence was signaled

    frameCtxt->FenceValue = 0;
    if (g_fence->GetCompletedValue() >= fenceValue)
        return;

    g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
    WaitForSingleObject(g_fenceEvent, INFINITE);
}

FrameContext* WaitForNextFrameResources()
{
    UINT nextFrameIndex = g_frameIndex + 1;
    g_frameIndex = nextFrameIndex;

    HANDLE waitableObjects[] = { g_hSwapChainWaitableObject, NULL };
    DWORD numWaitableObjects = 1;

    FrameContext* frameCtxt = &g_frameContext[nextFrameIndex % NUM_FRAMES_IN_FLIGHT];
    UINT64 fenceValue = frameCtxt->FenceValue;
    if (fenceValue != 0) // means no fence was signaled
    {
        frameCtxt->FenceValue = 0;
        g_fence->SetEventOnCompletion(fenceValue, g_fenceEvent);
        waitableObjects[1] = g_fenceEvent;
        numWaitableObjects = 2;
    }

    WaitForMultipleObjects(numWaitableObjects, waitableObjects, TRUE, INFINITE);

    return frameCtxt;
}

void ResizeSwapChain(HWND hWnd, int width, int height)
{
    g_width = width;
    g_height = height;

    DXGI_SWAP_CHAIN_DESC1 sd;
    g_pSwapChain->GetDesc1(&sd);
    sd.Width = width;
    sd.Height = height;

    IDXGIFactory4* dxgiFactory = NULL;
    g_pSwapChain->GetParent(IID_PPV_ARGS(&dxgiFactory));

    g_pSwapChain->Release();
    CloseHandle(g_hSwapChainWaitableObject);

    IDXGISwapChain1* swapChain1 = NULL;
    dxgiFactory->CreateSwapChainForHwnd(g_pd3dCommandQueue, hWnd, &sd, NULL, NULL, &swapChain1);
    swapChain1->QueryInterface(IID_PPV_ARGS(&g_pSwapChain));
    swapChain1->Release();
    dxgiFactory->Release();

    g_pSwapChain->SetMaximumFrameLatency(NUM_BACK_BUFFERS);

    g_hSwapChainWaitableObject = g_pSwapChain->GetFrameLatencyWaitableObject();
    assert(g_hSwapChainWaitableObject != NULL);
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_KEYDOWN:
    {
        if (GetKeyState(VK_CONTROL) & 0x8000)
        {
            if ((char)wParam == 'S')
                g_ctrl_s = true;
        }
        return 0;
    }
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            WaitForLastSubmittedFrame();
            ImGui_ImplDX12_InvalidateDeviceObjects();
            CleanupRenderTarget();
            ResizeSwapChain(hWnd, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
            CreateRenderTarget();
            ImGui_ImplDX12_CreateDeviceObjects();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_CLOSE:
    {
        if (ConfirmLoseChanges())
        {
            DestroyWindow(g_hwnd);
        }
        return 0;
    }
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

/*

Higher priority TODO:
* get rid of z-order, make it implicit in ordering of entities in list. make them able to move up and down. yes will work with subrenders too!
* generalized bias and gain for blend curve points. could do a variant. https://arxiv.org/pdf/2010.09714.pdf
* get tooltips working
* Maybe use sliders instead of text entry boxes? (You can still manually enter with cntl-left click in imgui)
* probably should make a tag for this being 0.5, and make a branch for the next version number.

TODO:
* since the editor can do gif, make the command line able to do gif?
* handle the crash when closing while rendering
* have a rewind button next to the play/stop button. can we use icons? does imgui have em?
* put preview image in it's own sub window with it's own horizontal and vertical scroll bars
* should launch latex and ffmpeg not with cmd but with something else. no system pls.
* could make a file name type, where you click it to choose a file.
* more hotkeys like for opening file and saving as work.
* tooltips! use the description to make tooltips for each field!
* make entity references be a special type that has a drop down to choose from when editing?
* keyframe editing needs better editing. like a normal ui with a checkbox next to each field to say you want to edit that field in the keyframe? or maybe the checkbox is implicit based on if it changed from last keyframe or not
* work with ffmpeg library more directly so you can skip png encoding, writing to disk, reading from disk, and png decoding.
* timeline imgui widget: https://twitter.com/ocornut/status/1319220561822756864?s=03
* mouse placement / dragging / movement of objects

*/