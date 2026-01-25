#include <bits/stdc++.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Constants
const int NUM_PLAYER = 5;
const int NUM_ROUND = 20;
const int INIT_HEALTH = 8;
const int MAX_HEALTH = 10;
const int WATER_REQ[5] = {8, 9, 10, 11, 12};
const int SALARY[5] = {70, 75, 100, 120, 120};

struct BidResult {
    int id;
    int bid;
    int req;
    bool won;
};

struct RoundData {
    int round;
    int water_supply;
    std::vector<BidResult> bids;
    int health[5];
    int balance[5];
    bool alive[5];
};

struct Replay {
    std::string timestamp;
    std::vector<RoundData> rounds;
    std::vector<std::string> log;
};

const char* PLAYER_NAMES[5] = {"Alex", "Bob", "Cindy", "David", "Eric"};
const ImVec4 PLAYER_COLORS[5] = {
    ImVec4(0.2f, 0.6f, 1.0f, 1.0f),  // Alex - Blue
    ImVec4(0.2f, 0.8f, 0.2f, 1.0f),  // Bob - Green
    ImVec4(1.0f, 0.8f, 0.2f, 1.0f),  // Cindy - Yellow
    ImVec4(1.0f, 0.4f, 0.4f, 1.0f),  // David - Red
    ImVec4(0.8f, 0.4f, 1.0f, 1.0f)   // Eric - Purple
};

// Global state
Replay g_replay;
int g_current_round = 0;
bool g_replay_loaded = false;

// DPI scale
float g_dpi_scale = 1.0f;

// File Loading
std::string open_file_dialog(){
    FILE* f = popen("zenity --file-selection --title='Select Replay File' --file-filter='JSON files (*.json) | *.json' --file-filter='All files | *'", "r");
    if(!f){
        std::cerr << "Failed to open file dialog\n";
        return "";
    }
    
    char path[1024];
    if(fgets(path, sizeof(path), f) != nullptr){
        size_t len = strlen(path);
        if(len > 0 && path[len - 1] == '\n'){
            path[len - 1] = '\0';
        }
        pclose(f);
        return std::string(path);
    }
    
    pclose(f);
    return "";
}

bool load_replay(const std::string& filename){
    std::ifstream file(filename);
    if(!file.is_open()){
        std::cerr << "Failed to open: " << filename << std::endl;
        return false;
    }
    
    try{
        json j;
        file >> j;
        
        g_replay.timestamp = j["timestamp"];
        g_replay.log = j["log"].get<std::vector<std::string>>();
        g_replay.rounds.clear();
        
        for(auto& jr : j["rounds"]){
            RoundData rd;
            rd.round = jr["round"];
            rd.water_supply = jr["water_supply"];
            
            for(auto& jb : jr["bids"]){
                BidResult br;
                br.id = jb["id"];
                br.bid = jb["bid"];
                br.req = jb["req"];
                br.won = jb["won"];
                rd.bids.push_back(br);
            }
            
            for(int i = 0; i < 5; i++){
                rd.health[i] = jr["health"][i];
                rd.balance[i] = jr["balance"][i];
                rd.alive[i] = jr["alive"][i];
            }
            
            g_replay.rounds.push_back(rd);
        }
        
        std::cout << "Loaded replay: " << g_replay.rounds.size() << " rounds\n";
        return true;
        
    }
    catch(const std::exception& e){
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

// UI Rendering Functions
void draw_plot_vertical_line(float round_num, float y_max){
    ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 0.8f, 0.2f, 0.8f));
    float vline_x[] = {round_num, round_num};
    float vline_y[] = {0.0f, y_max};
    ImPlot::PlotLine("##vline", vline_x, vline_y, 2);
    ImPlot::PopStyleColor();
}

void render_header(){
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.15f, 1.0f));
    ImGui::BeginChild("Header", ImVec2(0, 55 * g_dpi_scale), true);
    
    float content_width = ImGui::GetContentRegionAvail().x;
    float padding = 10 * g_dpi_scale;
    float button_height = 26 * g_dpi_scale;
    float button_y = (55 * g_dpi_scale - button_height) / 2;
    
    // Title
    ImGui::SetCursorPosX(padding);
    ImGui::SetCursorPosY((55 * g_dpi_scale - ImGui::GetFontSize()) / 2 - 5);
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "BATTLEBOT VIEWER");
    ImGui::SameLine();
    
    // Status display
    if(g_replay_loaded && !g_replay.rounds.empty()){
        int display_round = g_replay.rounds[g_current_round].round;
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Round %d / %d", display_round, (int)g_replay.rounds.size());
        
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(%s)", g_replay.timestamp.c_str());
    }
    else{
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No replay loaded");
    }
    
    // Navigation buttons
    if(g_replay_loaded && !g_replay.rounds.empty()){
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15 * g_dpi_scale);
        
        float nav_btn_width = 25 * g_dpi_scale;
        int max_idx = (int)g_replay.rounds.size() - 1;
        
        ImGui::SetCursorPosY(button_y);
        if(ImGui::Button("<<", ImVec2(nav_btn_width, button_height))){
            g_current_round = 0;
        }
        ImGui::SameLine(0, 2);
        ImGui::SetCursorPosY(button_y);
        if(ImGui::Button("<", ImVec2(nav_btn_width, button_height))){
            if(g_current_round > 0) g_current_round--;
        }
        ImGui::SameLine(0, 2);
        ImGui::SetCursorPosY(button_y);
        if(ImGui::Button(">", ImVec2(nav_btn_width, button_height))){
            if(g_current_round < max_idx) g_current_round++;
        }
        ImGui::SameLine(0, 2);
        ImGui::SetCursorPosY(button_y);
        if(ImGui::Button(">>", ImVec2(nav_btn_width, button_height))){
            g_current_round = max_idx;
        }
    }
    
    // Load button - right-aligned
    float btn_width = 100 * g_dpi_scale;
    ImGui::SameLine(content_width - btn_width - padding);
    ImGui::SetCursorPosY(button_y);
    
    if(ImGui::Button("Load Replay", ImVec2(btn_width, button_height))){
        std::string path = open_file_dialog();
        if(!path.empty()){
            if(load_replay(path)){
                g_current_round = 0;
                g_replay_loaded = true;
                std::cout << "Loaded: " << path << "\n";
            }
        }
    }
    
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void render_charts(){
    float available_height = ImGui::GetContentRegionAvail().y;
    float charts_height = available_height * 0.7f;
    float chart_height = (charts_height - 20 * g_dpi_scale) / 2;
    
    ImGui::BeginChild("Charts", ImVec2(0, charts_height), true);
    
    bool has_data = g_replay_loaded && !g_replay.rounds.empty();
    
    // Health chart
    if(ImPlot::BeginPlot("Health Over Time", ImVec2(-1, chart_height))){
        ImPlot::SetupAxes("Round", "Health");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, NUM_ROUND + 1, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, MAX_HEALTH + 1, ImGuiCond_Always);
        
        // Vertical line for selected round
        if(has_data && g_current_round >= 0 && g_current_round < (int)g_replay.rounds.size()){
            float selected_round = (float)g_replay.rounds[g_current_round].round;
            draw_plot_vertical_line(selected_round, (float)MAX_HEALTH + 1);
        }
        
        if(has_data){
            for(int p = 0; p < NUM_PLAYER; p++){
                std::vector<float> x_data, y_data;
                x_data.push_back(0);
                y_data.push_back(INIT_HEALTH);
                
                for(const auto& rd : g_replay.rounds){
                    x_data.push_back((float)rd.round);
                    y_data.push_back((float)rd.health[p]);
                }
                
                ImPlot::PushStyleColor(ImPlotCol_Line, PLAYER_COLORS[p]);
                ImPlot::PlotLine(PLAYER_NAMES[p], x_data.data(), y_data.data(), (int)x_data.size());
                ImPlot::PopStyleColor();
            }
        }
        ImPlot::EndPlot();
    }
    
    // Balance chart
    if(ImPlot::BeginPlot("Balance Over Time", ImVec2(-1, chart_height))){
        ImPlot::SetupAxes("Round", "Balance ($)");
        ImPlot::SetupAxisLimits(ImAxis_X1, 0, NUM_ROUND + 1, ImGuiCond_Always);
        
        float max_balance = 500;
        if(has_data){
            for(const auto& rd : g_replay.rounds){
                for(int p = 0; p < NUM_PLAYER; p++){
                    max_balance = std::max(max_balance, (float)rd.balance[p]);
                }
            }
        }
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0, max_balance * 1.1f, ImGuiCond_Always);
        
        // Vertical line for selected round
        if(has_data && g_current_round >= 0 && g_current_round < (int)g_replay.rounds.size()){
            float selected_round = (float)g_replay.rounds[g_current_round].round;
            draw_plot_vertical_line(selected_round, max_balance * 1.1f);
        }
        
        if(has_data){
            for(int p = 0; p < NUM_PLAYER; p++){
                std::vector<float> x_data, y_data;
                x_data.push_back(0);
                y_data.push_back((float)SALARY[p]);
                
                for(const auto& rd : g_replay.rounds){
                    x_data.push_back((float)rd.round);
                    y_data.push_back((float)rd.balance[p]);
                }
                
                ImPlot::PushStyleColor(ImPlotCol_Line, PLAYER_COLORS[p]);
                ImPlot::PlotLine(PLAYER_NAMES[p], x_data.data(), y_data.data(), (int)x_data.size());
                ImPlot::PopStyleColor();
            }
        }
        ImPlot::EndPlot();
    }
    
    ImGui::EndChild();
}

void render_game_log(){
    ImGui::BeginChild("GameLog", ImVec2(0, 0), true);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "GAME LOG");
    ImGui::Separator();
    
    ImGui::BeginChild("LogScroll", ImVec2(0, 0), false);
    
    if(g_replay_loaded){
        for(const auto& msg : g_replay.log){
            if(msg.find("ELIMINATED") != std::string::npos){
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", msg.c_str());
            }
            else if(msg.find("---") != std::string::npos){
                ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s", msg.c_str());
            }
            else if(msg.find("ERROR") != std::string::npos){
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", msg.c_str());
            }
            else if(msg.find("===") != std::string::npos){
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", msg.c_str());
            }
            else{
                ImGui::Text("%s", msg.c_str());
            }
        }
    }
    else{
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Load a replay to view game log...");
    }
    
    ImGui::EndChild();
    ImGui::EndChild();
}

void render_player_status(){
    float available_height = ImGui::GetContentRegionAvail().y;
    float status_height = available_height * 0.7f;
    ImGui::BeginChild("PlayerStatus", ImVec2(0, status_height), true);
    
    bool has_data = g_replay_loaded && g_current_round >= 0 && g_current_round < (int)g_replay.rounds.size();
    const RoundData* rd = has_data ? &g_replay.rounds[g_current_round] : nullptr;
    
    if(has_data){
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "PLAYER STATUS (Round %d)", rd->round);
    }
    else{
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "PLAYER STATUS");
    }
    ImGui::Separator();
    ImGui::Spacing();
    
    for(int i = 0; i < NUM_PLAYER; i++){
        ImGui::PushID(i);
        
        int health = has_data ? rd->health[i] : INIT_HEALTH;
        int balance = has_data ? rd->balance[i] : SALARY[i];
        bool alive = has_data ? rd->alive[i] : true;
        
        ImVec4 color = PLAYER_COLORS[i];
        if(!alive){
            color = ImVec4(0.4f, 0.4f, 0.4f, 0.6f);
        }
        
        ImGui::TextColored(color, "%s", PLAYER_NAMES[i]);
        
        if(alive && health > 0){
            float health_ratio = (float)health / MAX_HEALTH;
            ImVec4 health_color;
            if(health_ratio > 0.6f) health_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
            else if(health_ratio > 0.3f) health_color = ImVec4(0.9f, 0.7f, 0.1f, 1.0f);
            else health_color = ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
            
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, health_color);
            char health_overlay[32];
            snprintf(health_overlay, 32, "HP: %d/%d", health, MAX_HEALTH);
            ImGui::ProgressBar(health_ratio, ImVec2(-1, 16 * g_dpi_scale), health_overlay);
            ImGui::PopStyleColor();
            
            ImGui::Text("Balance: $%d", balance);
            ImGui::Text("Water Req: %d | Salary: $%d", WATER_REQ[i], SALARY[i]);
        }
        else{
            ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "ELIMINATED");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PopID();
    }
    
    ImGui::EndChild();
}

void render_bidding_results(){
    ImGui::BeginChild("BiddingResults", ImVec2(0, 0), true);
    
    bool has_data = g_replay_loaded && g_current_round >= 0 && g_current_round < (int)g_replay.rounds.size();
    const RoundData* rd = has_data ? &g_replay.rounds[g_current_round] : nullptr;
    
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "BIDDING RESULTS");
    ImGui::SameLine();
    if(has_data){
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "(Round %d - Water: %d)", rd->round, rd->water_supply);
    }
    else{
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(No data)");
    }
    ImGui::Separator();
    
    if(has_data && ImGui::BeginTable("BidTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)){
        ImGui::TableSetupColumn("Player", ImGuiTableColumnFlags_WidthStretch, 0.6f);
        ImGui::TableSetupColumn("Bid", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("Water Req", ImGuiTableColumnFlags_WidthStretch, 0.4f);
        ImGui::TableSetupColumn("Won", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableHeadersRow();
        
        for(const auto& r : rd->bids){
            int idx = r.id - 1;
            
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(PLAYER_COLORS[idx], "%s", PLAYER_NAMES[idx]);
            
            ImGui::TableNextColumn();
            ImGui::Text("$%d", r.bid);
            
            ImGui::TableNextColumn();
            ImGui::Text("%d", r.req);
            
            ImGui::TableNextColumn();
            if(r.won){
                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "YES");
            }
            else{
                ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "NO");
            }
            
            ImGui::TableNextColumn();
            ImGui::Text("HP: %d | $%d", rd->health[idx], rd->balance[idx]);
        }
        
        ImGui::EndTable();
    }
    
    ImGui::EndChild();
}

static void glfw_error_callback(int error, const char* description){
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(){
    std::cout << "BattleBot Replay Viewer\n";
    std::cout << "Click 'Load Replay' to open a file\n\n";

    glfwSetErrorCallback(glfw_error_callback);
    if(!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    int window_width = (int)(mode->width * 0.70f);   // 70% of screen width
    int window_height = (int)(mode->height * 0.75f); // 75% of screen height

    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "BattleBot Replay Viewer", NULL, NULL);
    if(window == NULL) return 1;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Get DPI scale
    float xscale;
    glfwGetWindowContentScale(window, &xscale, nullptr);
    g_dpi_scale = xscale;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    
    // Scale style for DPI
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(g_dpi_scale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImFontConfig font_cfg;
    font_cfg.SizePixels = 13.0f * g_dpi_scale;
    io.Fonts->AddFontDefault(&font_cfg);

    // Main loop
    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Main window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("BattleBot Viewer", NULL, 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        
        render_header();
        ImGui::Spacing();
        // Main content area
        ImGui::BeginChild("MainContent", ImVec2(0, 0), false);
        
        // Left panel (Charts + Log) - 55% width
        ImGui::BeginChild("LeftPanel", ImVec2(ImGui::GetContentRegionAvail().x * 0.55f, 0), false);
        render_charts();
        ImGui::Spacing();
        render_game_log();
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        // Right panel (Player Status + Bidding)
        ImGui::BeginChild("RightPanel", ImVec2(0, 0), false);
        render_player_status();
        ImGui::Spacing();
        render_bidding_results();
        ImGui::EndChild();
        
        ImGui::EndChild();
        
        ImGui::End();
        
        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
