// dear imgui: standalone example application for DirectX 12
// If you are new to dear imgui, see examples/README.txt and documentation at the top of imgui.cpp.
// FIXME: 64-bit only for now! (Because sizeof(ImTextureId) == sizeof(void*))

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <tchar.h>
#include "nfd.h"

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

bool g_ctrl_s = false;

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

void OnDocumentChange()
{
    // We keep a separate render document because the validation and fixup modifies the document data
    g_renderDocument = g_rootDocument;

    g_renderDocument.outputSizeX = 640;
    g_renderDocument.outputSizeY = int(float(g_renderDocument.outputSizeX) * float(g_rootDocument.outputSizeY) / float(g_rootDocument.outputSizeX));

    g_renderDocumentContext.frameCache.Reset();
    g_renderDocumentThreadContext.threadId = 0;
    ValidateAndFixupDocument(g_renderDocument);
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

    OnDocumentChange();
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

        // Our editor UI
        {
            // take up the whole screen
            ImGui::SetNextWindowPos(ImVec2{ 0.0f, 0.0f });
            ImGui::SetNextWindowSize(ImVec2{ float(g_width), float(g_height) });
            
            ImGui::Begin("df_serialize editor", nullptr, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

            //ImGui::ShowDemoWindow();

            // Menu
            if (ImGui::BeginMenuBar())
            {
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("New", "Ctrl+N") && ConfirmLoseChanges())
                    {
                        NewDocument();
                        OnDocumentChange();
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
                            OnDocumentChange();
                        }
                    }

                    if (!g_rootDocumentFileName.empty() && ImGui::MenuItem("Save", "Ctrl+S") && g_rootDocumentDirty)
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
                            if (ImGui::Selectable(g_rootDocument.entities[index].id.c_str(), selected == index + 1))
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

                            OnDocumentChange();
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

                                OnDocumentChange();
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

                        if (selected-1 < g_rootDocument.entities.size())
                        {
                            bool changed = ShowUI(g_rootDocument.entities[selected-1], "");
                            if (changed)
                            {
                                OnDocumentChange();
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
                                OnDocumentChange();
                                if (!g_rootDocumentDirty)
                                {
                                    g_rootDocumentDirty = true;
                                    UpdateWindowTitle();
                                }
                            }
                        }

                        ImGui::EndChild();
                    }
                }

                ImGui::NextColumn();

                // frame scrubber
                {
                    int totalFrames = TotalFrameCount(g_rootDocument);
                    if (g_playMode)
                    {
                        g_previewFrameIndex++;
                        if (g_previewFrameIndex >= totalFrames)
                            g_playMode = false;
                    }

                    g_previewFrameIndex = Clamp(g_previewFrameIndex, 0, totalFrames - 1);

                    ImGui::Text("Frame");

                    ImGui::SameLine();

                    ImGui::PushID("Frame");

                    ImGui::SliderInt("", &g_previewFrameIndex, 0, totalFrames - 1);

                    ImGui::PopID();

                    ImGui::SameLine();

                    if (!g_playMode && ImGui::Button("Play"))
                        g_playMode = true;
                    else if (g_playMode && ImGui::Button("Stop"))
                        g_playMode = false;
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

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync

        UINT64 fenceValue = g_fenceLastSignaledValue + 1;
        g_pd3dCommandQueue->Signal(g_fence, fenceValue);
        g_fenceLastSignaledValue = fenceValue;
        frameCtxt->FenceValue = fenceValue;
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
TODO:
* put preview image in it's own sub window with it's own horizontal and vertical scroll bars
* should launch latex and ffmpeg not with cmd but with something else. no system pls.
* could make a file name type, where you click it to choose a file.
* also a color picker, and make point3d be a single line to edit!
* more hotkeys like for opening file and saving as work.
* need to be able to edit keyframes
* need to be able to export the video (probably use animatron command line)
* for certain edits (or all if you have to?), have a timeout before you apply them.  Like when changing resolution. so that it doesn't fire up latex etc right away while you are typing.
* make the edit boxes take up the full width of the column. no reason to waste space
* have a 'realtime' checkbox next to the play button to make it advance frames based on time, instead of just incrementing.
* have a rewind button next to the play/stop button. can we use icons? does imgui have em?
* show current time in minutes/seconds and total time.

! retest command line animatron
*/