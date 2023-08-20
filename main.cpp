// Dear ImGui: standalone example application for Emscripten, using GLFW + WebGPU
// (Emscripten is a C++-to-javascript compiler, used to publish executables for the web. See https://emscripten.org/)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_wgpu.h"
#include <stdio.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

// Global WebGPU required states
static WGPUDevice    wgpu_device = nullptr;
static WGPUSurface   wgpu_surface = nullptr;
static WGPUSwapChain wgpu_swap_chain = nullptr;
static int           wgpu_swap_chain_width = 0;
static int           wgpu_swap_chain_height = 0;

// Forward declarations
static void MainLoopStep(void* window);
static bool InitWGPU();
static void print_glfw_error(int error, const char* description);
static void print_wgpu_error(WGPUErrorType error_type, const char* message, void*);

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(print_glfw_error);
    if (!glfwInit())
        return 1;

    // Make sure GLFW does not initialize any graphics context.
    // This needs to be done explicitly later.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+WebGPU example", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return 1;
    }

    // Initialize the WebGPU environment
    if (!InitWGPU())
    {
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    glfwShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOther(window, true);
    ImGui_ImplWGPU_Init(wgpu_device, 3, WGPUTextureFormat_RGBA8Unorm, WGPUTextureFormat_Undefined);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Emscripten allows preloading a file or folder to be accessible at runtime. See Makefile for details.
    //io.Fonts->AddFontDefault();
#ifndef IMGUI_DISABLE_FILE_FUNCTIONS
    //io.Fonts->AddFontFromFileTTF("fonts/segoeui.ttf", 18.0f);
    io.Fonts->AddFontFromFileTTF("fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("fonts/ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);
#endif

    // This function will directly return and exit the main function.
    // Make sure that no required objects get cleaned up.
    // This way we can use the browsers 'requestAnimationFrame' to control the rendering.
    emscripten_set_main_loop_arg(MainLoopStep, window, 0, false);

    return 0;
}

static bool InitWGPU()
{
    wgpu_device = emscripten_webgpu_get_device();
    if (!wgpu_device)
        return false;

    wgpuDeviceSetUncapturedErrorCallback(wgpu_device, print_wgpu_error, nullptr);

    // Use C++ wrapper due to misbehavior in Emscripten.
    // Some offset computation for wgpuInstanceCreateSurface in JavaScript
    // seem to be inline with struct alignments in the C++ structure
    wgpu::SurfaceDescriptorFromCanvasHTMLSelector html_surface_desc = {};
    html_surface_desc.selector = "#canvas";

    wgpu::SurfaceDescriptor surface_desc = {};
    surface_desc.nextInChain = &html_surface_desc;

    // Use 'null' instance
    wgpu::Instance instance = wgpu::CreateInstance(); //{};
    wgpu_surface = instance.CreateSurface(&surface_desc).Release();

    return true;
}

static void MainLoopStep(void* window)
{
    glfwPollEvents();

    int width, height;
    glfwGetFramebufferSize((GLFWwindow*)window, &width, &height);

    // React to changes in screen size
    if (width != wgpu_swap_chain_width && height != wgpu_swap_chain_height)
    {
        ImGui_ImplWGPU_InvalidateDeviceObjects();
        if (wgpu_swap_chain)
            wgpuSwapChainRelease(wgpu_swap_chain);
        wgpu_swap_chain_width = width;
        wgpu_swap_chain_height = height;
        WGPUSwapChainDescriptor swap_chain_desc = {};
        swap_chain_desc.usage = WGPUTextureUsage_RenderAttachment;
        swap_chain_desc.format = WGPUTextureFormat_RGBA8Unorm;
        swap_chain_desc.width = width;
        swap_chain_desc.height = height;
        swap_chain_desc.presentMode = WGPUPresentMode_Fifo;
        wgpu_swap_chain = wgpuDeviceCreateSwapChain(wgpu_device, wgpu_surface, &swap_chain_desc);
        ImGui_ImplWGPU_CreateDeviceObjects();
    }

    
	 // Our state
    const static ImVec2 WINDOW_SIZE = ImVec2(800.f, 600.f);
    const static ImVec2 WINDOW_POS = ImVec2(20.f, 20.f);
    static bool show_UI = true;
    static bool temperature_mode = false;
    static ImVec4 background_color = ImVec4(0.f, 0.f, 0.f, 1.f); //black

    // Generate a default palette. The palette will persist and can be edited.
    static bool saved_palette_init = true;
    static ImVec4 saved_palette[32] = {};

    if (saved_palette_init)
    {
        for (int n = 0; n < IM_ARRAYSIZE(saved_palette); n++)
        {
            ImGui::ColorConvertHSVtoRGB(n / 31.0f, 0.8f, 0.8f,
                saved_palette[n].x, saved_palette[n].y, saved_palette[n].z);
            saved_palette[n].w = 1.0f; // Alpha
        }
        saved_palette_init = false;
    }
   
	// Start the Dear ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    //Might want to bring back framerate display for testing later...
		// ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
	//TODO: HDR Support
	    //static bool hdr = false;
		//ImGui::SetColorEditOptions(ImGuiColorEditFlags_HDR)

	//keyboard / input controls
    if (ImGui::IsKeyPressed(ImGuiKey_Space))
    {
        show_UI = !show_UI;
    }

    if (show_UI)
    {
        static ImGuiWindowFlags widget_window_flags;
        widget_window_flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove;
        ImGui::Begin("OPTIONS", &show_UI, widget_window_flags);
        {
			static ImVec4 widget_selected_color = ImVec4(0.50f, 0.50f, 0.50f, 1.0f); //grey
			ImGui::SetWindowSize(WINDOW_SIZE);
			ImGui::SetWindowPos(WINDOW_POS);

			ImGui::Text("OPTIONS");
			ImGui::Separator();
			ImGui::Checkbox("Temperature Mode", &temperature_mode);

			float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.y) * 0.40f;
			ImGui::SetNextItemWidth(w);
			ImGuiColorEditFlags_ color_bar_mode = ImGuiColorEditFlags_PickerHueBar;
			if (temperature_mode)
			    color_bar_mode = ImGuiColorEditFlags_PickerTempsBar;

			ImGui::ColorPicker3("##MyColor##5", (float*)&widget_selected_color, color_bar_mode | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha);
			if (!temperature_mode)
			{
			    ImGui::SameLine();
			    ImGui::SetNextItemWidth(w);
			    ImGui::ColorPicker3("##MyColor##6", (float*)&widget_selected_color, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha);
			}
			
			
			ImGui::Spacing();
			ImGui::ColorEdit4("HSV shown as RGB##1", (float*)&widget_selected_color, ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoPicker);
			ImGui::ColorEdit4("HSV shown as HSV##1", (float*)&widget_selected_color, ImGuiColorEditFlags_DisplayHSV | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoPicker);
			ImGui::ColorEdit4("Hex shown", (float*)&widget_selected_color, ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoPicker);


			ImGui::Text("Palette Presets");
			for (int n = 0; n < IM_ARRAYSIZE(saved_palette); n++)
			{
			    ImGui::PushID(n);
			    if ((n % 8) != 0)
			        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.y);

			    ImGuiColorEditFlags palette_button_flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoTooltip;
			    if (ImGui::ColorButton("##palette", saved_palette[n], palette_button_flags, ImVec2(20, 20)))
			        widget_selected_color = ImVec4(saved_palette[n].x, saved_palette[n].y, saved_palette[n].z, widget_selected_color.w); // Preserve alpha!

			    // Allow user to drop colors into each palette entry. Note that ColorButton() is already a
			    // drag source by default, unless specifying the ImGuiColorEditFlags_NoDragDrop flag.
			    if (ImGui::BeginDragDropTarget())
			    {
			        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_3F))
			            memcpy((float*)&saved_palette[n], payload->Data, sizeof(float) * 3);
			        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_PAYLOAD_TYPE_COLOR_4F))
			            memcpy((float*)&saved_palette[n], payload->Data, sizeof(float) * 4);
			        ImGui::EndDragDropTarget();
			    }//endif
			    ImGui::PopID();
			}//endfor
			ImGui::SameLine();
			ImGui::Text("           PRESS SPACE BAR TO SHOW / HIDE OPTIONS UI");

			background_color = widget_selected_color;
		}//end ImGUI Begin::
		ImGui::End();
	}//endif: show_UI

    // Rendering
    ImGui::Render();

    WGPURenderPassColorAttachment color_attachments = {};
    color_attachments.loadOp = WGPULoadOp_Clear;
    color_attachments.storeOp = WGPUStoreOp_Store;
    color_attachments.clearValue = { background_color.x * background_color.w, background_color.y * background_color.w, background_color.z * background_color.w, background_color.w };
    color_attachments.view = wgpuSwapChainGetCurrentTextureView(wgpu_swap_chain);
    WGPURenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_attachments;
    render_pass_desc.depthStencilAttachment = nullptr;

    WGPUCommandEncoderDescriptor enc_desc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(wgpu_device, &enc_desc);

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &render_pass_desc);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass);
    wgpuRenderPassEncoderEnd(pass);

    WGPUCommandBufferDescriptor cmd_buffer_desc = {};
    WGPUCommandBuffer cmd_buffer = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
    WGPUQueue queue = wgpuDeviceGetQueue(wgpu_device);
    wgpuQueueSubmit(queue, 1, &cmd_buffer);
}//end MainLoopStep

static void print_glfw_error(int error, const char* description)
{
    printf("GLFW Error %d: %s\n", error, description);
}

static void print_wgpu_error(WGPUErrorType error_type, const char* message, void*)
{
    const char* error_type_lbl = "";
    switch (error_type)
    {
    case WGPUErrorType_Validation:  error_type_lbl = "Validation"; break;
    case WGPUErrorType_OutOfMemory: error_type_lbl = "Out of memory"; break;
    case WGPUErrorType_Unknown:     error_type_lbl = "Unknown"; break;
    case WGPUErrorType_DeviceLost:  error_type_lbl = "Device lost"; break;
    default:                        error_type_lbl = "Unknown";
    }
    printf("%s error: %s\n", error_type_lbl, message);
}
