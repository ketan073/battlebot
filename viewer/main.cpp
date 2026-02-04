#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>
#include <nfd.h>

using json = nlohmann::json;

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

struct TournamentBot {
    int rank;
    int id;
    std::string name;
    std::string path;
    double elo;
    int games_played;
    int wins;
    double win_rate;
    int top2_finishes;
    int top3_finishes;
    double avg_rounds_survived;
    double avg_final_health;
    double avg_final_balance;
    double elimination_rate;
    double survival_rate;
    double avg_position;
    double elo_change;
    double avg_dominance;
    std::vector<std::pair<int, double>> elo_history;
    std::map<int, std::map<int, std::pair<int, int>>> head_to_head_history;
};

struct TournamentResults {
    int total_games;
    std::string timestamp;
    std::vector<TournamentBot> bots;
};

Replay g_replay;
int g_current_round = 0;
bool g_replay_loaded = false;

TournamentResults g_tournament;
bool g_tournament_loaded = false;
int g_selected_game = -1;

enum ViewMode {
    VIEW_REPLAY,
    VIEW_TOURNAMENT
};
ViewMode g_view_mode = VIEW_REPLAY;

float g_dpi_scale = 1.0f;

ImVec4 get_bot_color(int bot_id) {
    const ImVec4 colors[] = {
        ImVec4(0.2f, 0.6f, 1.0f, 1.0f),   // Blue
        ImVec4(0.2f, 0.8f, 0.2f, 1.0f),   // Green
        ImVec4(1.0f, 0.8f, 0.2f, 1.0f),   // Yellow
        ImVec4(1.0f, 0.4f, 0.4f, 1.0f),   // Red
        ImVec4(0.8f, 0.4f, 1.0f, 1.0f),   // Purple
        ImVec4(0.4f, 0.8f, 0.8f, 1.0f),   // Cyan
        ImVec4(1.0f, 0.6f, 0.2f, 1.0f),   // Orange
        ImVec4(0.8f, 0.2f, 0.8f, 1.0f),   // Magenta
        ImVec4(0.6f, 1.0f, 0.4f, 1.0f),   // Lime
        ImVec4(1.0f, 0.4f, 0.8f, 1.0f),   // Pink
    };
    return colors[(bot_id - 1) % 10];
}

std::string open_file_dialog(){
    nfdchar_t *outPath = nullptr;
    nfdfilteritem_t filterItem[1] = { { "JSON files", "json" } };
    
    nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, 1, nullptr);
    
    if(result == NFD_OKAY){
        std::string path(outPath);
        NFD_FreePath(outPath);
        return path;
    }
    else if(result == NFD_CANCEL){
        return "";
    }
    else{
        std::cerr << "File dialog error: " << NFD_GetError() << std::endl;
        return "";
    }
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

bool load_tournament_result(const std::string& filename){
    std::ifstream file(filename);
    if(!file.is_open()){
        std::cerr << "Failed to open: " << filename << std::endl;
        return false;
    }
    
    try{
        json j;
        file >> j;
        
        g_tournament.total_games = j["total_games"];
        g_tournament.timestamp = j["timestamp"];
        g_tournament.bots.clear();
        
        for(auto& jb : j["ratings"]){
            TournamentBot bot;
            bot.rank = jb["rank"];
            bot.id = jb["id"];
            bot.name = jb["name"];
            bot.path = jb["path"];
            bot.elo = jb["elo"];
            bot.games_played = jb["games_played"];
            bot.wins = jb["wins"];
            bot.win_rate = jb["win_rate"];
            bot.top2_finishes = jb["top2_finishes"];
            bot.top3_finishes = jb["top3_finishes"];
            bot.avg_rounds_survived = jb["avg_rounds_survived"];
            bot.avg_final_health = jb["avg_final_health"];
            bot.avg_final_balance = jb["avg_final_balance"];
            
            // New stats (with defaults for older files)
            bot.elimination_rate = jb.value("elimination_rate", 0.0);
            bot.survival_rate = jb.value("survival_rate", 0.0);
            bot.avg_position = jb.value("avg_position", 0.0);
            bot.elo_change = jb.value("elo_change", 0.0);
            bot.avg_dominance = jb.value("avg_dominance", 0.0);
            
            if(jb.contains("elo_history")) {
                for(auto& eh : jb["elo_history"]) {
                    int game = eh["game"];
                    double elo = eh["elo"];
                    bot.elo_history.push_back({game, elo});
                }
            }
            
            // Load head-to-head history
            if(jb.contains("head_to_head_history")) {
                for(auto& [game_str, h2h_map] : jb["head_to_head_history"].items()) {
                    int game = std::stoi(game_str);
                    std::map<int, std::pair<int, int>> h2h_data;
                    for(auto& [opponent_id_str, wl] : h2h_map.items()) {
                        int opponent_id = std::stoi(opponent_id_str);
                        int wins = wl["wins"];
                        int losses = wl["losses"];
                        h2h_data[opponent_id] = {wins, losses};
                    }
                    bot.head_to_head_history[game] = h2h_data;
                }
            }
            
            g_tournament.bots.push_back(bot);
        }
        
        g_selected_game = -1;
        
        std::cout << "Loaded tournament results: " << g_tournament.bots.size() << " bots\n";
        return true;
        
    }
    catch(const std::exception& e){
        std::cerr << "JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

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
    
    // Mode tabs
    ImGui::SetCursorPosX(200 * g_dpi_scale);
    ImGui::SetCursorPosY(button_y);
    float tab_width = 90 * g_dpi_scale;
    if(ImGui::Button("Replay", ImVec2(tab_width, button_height))){
        g_view_mode = VIEW_REPLAY;
    }
    ImGui::SameLine(0, 2);
    ImGui::SetCursorPosY(button_y);
    if(ImGui::Button("Tournament", ImVec2(tab_width, button_height))){
        g_view_mode = VIEW_TOURNAMENT;
    }
    
    ImGui::SameLine();
    ImGui::SetCursorPosX(400 * g_dpi_scale);
    
    // Status display
    if(g_view_mode == VIEW_REPLAY){
        if(g_replay_loaded && !g_replay.rounds.empty()){
            int display_round = g_replay.rounds[g_current_round].round;
            if(display_round < 10){
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Round  %d / %d", display_round, (int)g_replay.rounds.size());
            }
            else ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "Round %d / %d", display_round, (int)g_replay.rounds.size());
            
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
    }
    else if(g_view_mode == VIEW_TOURNAMENT){
        if(g_tournament_loaded){
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%d Games, %d Bots", 
                g_tournament.total_games, (int)g_tournament.bots.size());
        }
        else{
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No tournament results loaded");
        }
    }
    
    // Load buttons - right-aligned
    float btn_width = 120 * g_dpi_scale;
    ImGui::SameLine(content_width - btn_width - padding);
    ImGui::SetCursorPosY(button_y);
    
    if(g_view_mode == VIEW_REPLAY){
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
    }
    else{
        if(ImGui::Button("Load Tournament", ImVec2(btn_width, button_height))){
            std::string path = open_file_dialog();
            if(!path.empty()){
                if(load_tournament_result(path)){
                    g_tournament_loaded = true;
                    std::cout << "Loaded: " << path << "\n";
                }
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

void render_tournament_result(){
    if(!g_tournament_loaded || g_tournament.bots.empty()){
        ImGui::BeginChild("NoTournamentData", ImVec2(0, 0), true);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No tournament results loaded.");
        ImGui::Text("Click 'Load Tournament' to open tournament_result.json");
        ImGui::EndChild();
        return;
    }
    
    float available_height = ImGui::GetContentRegionAvail().y;
    float tables_height = available_height * 0.26f;
    float chart_height = available_height * 0.45f;
    float snapshot_height = available_height * 0.26f;
    
    ImGui::BeginChild("TablesSection", ImVec2(0, tables_height), false);
    
    float table_width = ImGui::GetContentRegionAvail().x * 0.5f - 5;
    
    ImGui::BeginChild("RankingsPanel", ImVec2(table_width, 0), true);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "RANKINGS");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(%d games)", g_tournament.total_games);
    ImGui::Separator();
    
    if(ImGui::BeginTable("Rankings", 8, 
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | 
        ImGuiTableFlags_SizingStretchProp)){
        
        ImGui::TableSetupColumn("Rank", 0, 0.4f);
        ImGui::TableSetupColumn("ID", 0, 0.3f);
        ImGui::TableSetupColumn("Bot", 0, 1.2f);
        ImGui::TableSetupColumn("ELO", 0, 0.6f);
        ImGui::TableSetupColumn("Wins", 0, 0.4f);
        ImGui::TableSetupColumn("Win%", 0, 0.5f);
        ImGui::TableSetupColumn("Top2", 0, 0.7f);
        ImGui::TableSetupColumn("Top3", 0, 0.7f);
        ImGui::TableHeadersRow();
        
        for(const auto& bot : g_tournament.bots){
            ImGui::TableNextRow();
            
            ImGui::TableNextColumn();
            if(bot.rank == 1){
                ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "%d", bot.rank);
            } else if(bot.rank == 2){
                ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "%d", bot.rank);
            } else if(bot.rank == 3){
                ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.2f, 1.0f), "%d", bot.rank);
            } else {
                ImGui::Text("%d", bot.rank);
            }
            
            ImGui::TableNextColumn();
            ImVec4 bot_color = get_bot_color(bot.id);
            ImGui::TextColored(bot_color, "%d", bot.id);
            
            ImGui::TableNextColumn();
            if(bot.rank <= 3){
                ImVec4 color = (bot.rank == 1) ? ImVec4(1.0f, 0.84f, 0.0f, 1.0f) :
                               (bot.rank == 2) ? ImVec4(0.75f, 0.75f, 0.75f, 1.0f) :
                                                 ImVec4(0.8f, 0.5f, 0.2f, 1.0f);
                ImGui::TextColored(color, "%s", bot.name.c_str());
            } else {
                ImGui::Text("%s", bot.name.c_str());
            }
            
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", bot.elo);
            
            ImGui::TableNextColumn();
            ImGui::Text("%d", bot.wins);
            
            ImGui::TableNextColumn();
            ImGui::Text("%.1f%%", bot.win_rate);
            
            ImGui::TableNextColumn();
            double top2_pct = bot.games_played > 0 ? (100.0 * bot.top2_finishes / bot.games_played) : 0.0;
            ImGui::Text("%d (%.0f%%)", bot.top2_finishes, top2_pct);
            
            ImGui::TableNextColumn();
            double top3_pct = bot.games_played > 0 ? (100.0 * bot.top3_finishes / bot.games_played) : 0.0;
            ImGui::Text("%d (%.0f%%)", bot.top3_finishes, top3_pct);
        }
        
        ImGui::EndTable();
    }
    ImGui::EndChild();
    
    ImGui::SameLine();
    
    ImGui::BeginChild("StatsPanel", ImVec2(0, 0), true);
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "STATISTICS");
    ImGui::Separator();
    
    if(ImGui::BeginTable("Stats", 8, 
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | 
        ImGuiTableFlags_SizingStretchProp)){
        
        ImGui::TableSetupColumn("Bot", 0, 1.0f);
        ImGui::TableSetupColumn("<Pos>", 0, 0.4f);
        ImGui::TableSetupColumn("Surv%", 0, 0.45f);
        ImGui::TableSetupColumn("ELO D", 0, 0.45f);
        ImGui::TableSetupColumn("<Dom>", 0, 0.45f);
        ImGui::TableSetupColumn("Rounds", 0, 0.45f);
        ImGui::TableSetupColumn("<HP>", 0, 0.5f);
        ImGui::TableSetupColumn("<$>", 0, 0.5f);
        ImGui::TableHeadersRow();
        
        for(const auto& bot : g_tournament.bots){
            ImGui::TableNextRow();
            
            ImGui::TableNextColumn();
            ImVec4 bot_color = get_bot_color(bot.id);
            ImGui::TextColored(bot_color, "%s", bot.name.c_str());
            
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", bot.avg_position);
            
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "%.0f%%", bot.survival_rate);
            
            ImGui::TableNextColumn();
            if(bot.elo_change >= 0) {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "+%.0f", bot.elo_change);
            } else {
                ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%.0f", bot.elo_change);
            }
            
            ImGui::TableNextColumn();
            ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%.1f", bot.avg_dominance);
            
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", bot.avg_rounds_survived);
            
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", bot.avg_final_health);
            
            ImGui::TableNextColumn();
            ImGui::Text("%.0f", bot.avg_final_balance);
        }
        
        ImGui::EndTable();
    }
    ImGui::EndChild();
    
    ImGui::EndChild();
    
    ImGui::BeginChild("ELOChartPanel", ImVec2(0, chart_height), true);
    if(!g_tournament.bots.empty() && !g_tournament.bots[0].elo_history.empty()) {
        std::vector<int> valid_games;
        int max_game = 0;
        double min_elo = 10000, max_elo = 0;
        for(const auto& bot : g_tournament.bots) {
            for(const auto& point : bot.elo_history) {
                min_elo = std::min(min_elo, point.second);
                max_elo = std::max(max_elo, point.second);
                max_game = std::max(max_game, point.first);
                if(std::find(valid_games.begin(), valid_games.end(), point.first) == valid_games.end()) {
                    valid_games.push_back(point.first);
                }
            }
        }
        std::sort(valid_games.begin(), valid_games.end());
        
        int selected_idx = (int)valid_games.size() - 1;
        for(size_t i = 0; i < valid_games.size(); i++) {
            if(valid_games[i] >= g_selected_game) {
                selected_idx = i;
                break;
            }
        }
        if(g_selected_game < 0) selected_idx = (int)valid_games.size() - 1;
        
        int snapshot_interval = 10;
        if(valid_games.size() >= 2) {
            snapshot_interval = valid_games[1] - valid_games[0];
        }
        
        ImGui::Text("Snapshot Interval: %d games | Showing snapshot at game: %d", 
                    snapshot_interval, valid_games[selected_idx]);
        ImGui::SetNextItemWidth(-1);
        char slider_label[64];
        snprintf(slider_label, sizeof(slider_label), "Snapshot %d of %d", selected_idx + 1, (int)valid_games.size());
        if(ImGui::SliderInt("##GameSlider", &selected_idx, 0, (int)valid_games.size() - 1, slider_label)) {
            g_selected_game = valid_games[selected_idx];
        }
        g_selected_game = valid_games[selected_idx];
        
        float chart_height_remaining = ImGui::GetContentRegionAvail().y;
        if(ImPlot::BeginPlot("ELO Rating Over Time", ImVec2(-1, chart_height_remaining))){
            ImPlot::SetupAxes("Game #", "ELO");
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, max_game + 5, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, min_elo - 30, max_elo + 30, ImGuiCond_Always);
            ImPlot::SetupLegend(ImPlotLocation_NorthWest);
            
            if(g_selected_game >= 0) {
                ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(1.0f, 0.8f, 0.2f, 0.8f));
                float vline_x[] = {(float)g_selected_game, (float)g_selected_game};
                float vline_y[] = {(float)(min_elo - 30), (float)(max_elo + 30)};
                ImPlot::PlotLine("##vline", vline_x, vline_y, 2);
                ImPlot::PopStyleColor();
            }
            
            std::vector<TournamentBot> sorted_bots = g_tournament.bots;
            std::sort(sorted_bots.begin(), sorted_bots.end(), [](const TournamentBot& a, const TournamentBot& b) {
                return a.rank < b.rank;
            });
            
            for(size_t i = 0; i < sorted_bots.size(); i++) {
                const auto& bot = sorted_bots[i];
                if(!bot.elo_history.empty()) {
                    std::vector<double> x_data, y_data;
                    for(const auto& point : bot.elo_history) {
                        x_data.push_back(point.first);
                        y_data.push_back(point.second);
                    }
                    
                    ImVec4 bot_color = get_bot_color(bot.id);
                    ImPlot::PushStyleColor(ImPlotCol_Line, bot_color);
                    ImPlot::PushStyleVar(ImPlotStyleVar_Marker, ImPlotMarker_None);
                    char legend_label[64];
                    snprintf(legend_label, sizeof(legend_label), "[%d] %s", bot.id, bot.name.c_str());
                    ImPlot::PlotLine(legend_label, x_data.data(), y_data.data(), (int)x_data.size());
                    ImPlot::PopStyleVar();
                    ImPlot::PopStyleColor();
                }
            }
            
            ImPlot::EndPlot();
        }
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No ELO history data");
    }
    ImGui::EndChild();
    
    ImGui::BeginChild("SnapshotPanel", ImVec2(0, snapshot_height), false);
    
    if(!g_tournament.bots.empty() && !g_tournament.bots[0].elo_history.empty() && g_selected_game >= 0) {
        float snapshot_table_width = ImGui::GetContentRegionAvail().x * 0.5f - 5;
        
        ImGui::BeginChild("SnapshotTable", ImVec2(snapshot_table_width, 0), true);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "SNAPSHOT");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), " (Game %d)", g_selected_game);
        ImGui::Separator();
        
        if(ImGui::BeginTable("Snapshot", 5, 
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | 
            ImGuiTableFlags_SizingStretchProp)){
            
            ImGui::TableSetupColumn("Rank", 0, 0.4f);
            ImGui::TableSetupColumn("Bot", 0, 1.2f);
            ImGui::TableSetupColumn("ELO", 0, 0.6f);
            ImGui::TableSetupColumn("ELO D", 0, 0.6f);
            ImGui::TableSetupColumn("Moment", 0, 0.7f);
            ImGui::TableHeadersRow();
            
            struct BotSnapshot {
                int id;
                std::string name;
                double elo;
                double elo_delta;
                double momentum;
            };
            
            std::vector<BotSnapshot> snapshots;
            for(const auto& bot : g_tournament.bots) {
                BotSnapshot snap;
                snap.id = bot.id;
                snap.name = bot.name;
                snap.elo = 1500.0;
                snap.elo_delta = 0.0;
                snap.momentum = 0.0;
                
                double prev_elo = 1500.0;
                std::vector<double> recent_elos;
                
                for(size_t i = 0; i < bot.elo_history.size(); i++) {
                    if(bot.elo_history[i].first <= g_selected_game) {
                        if(i > 0) prev_elo = bot.elo_history[i-1].second;
                        snap.elo = bot.elo_history[i].second;
                        recent_elos.push_back(bot.elo_history[i].second);
                        if(recent_elos.size() > 10) recent_elos.erase(recent_elos.begin());
                    } else {
                        break;
                    }
                }
                
                snap.elo_delta = snap.elo - prev_elo;
                
                if(recent_elos.size() >= 2) {
                    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
                    for(size_t i = 0; i < recent_elos.size(); i++) {
                        sum_x += i;
                        sum_y += recent_elos[i];
                        sum_xy += i * recent_elos[i];
                        sum_xx += i * i;
                    }
                    int n = recent_elos.size();
                    snap.momentum = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
                }
                
                snapshots.push_back(snap);
            }
            
            std::sort(snapshots.begin(), snapshots.end(), [](const BotSnapshot& a, const BotSnapshot& b) {
                return a.elo > b.elo;
            });
            
            for(size_t i = 0; i < snapshots.size(); i++) {
                ImGui::TableNextRow();
                
                ImGui::TableNextColumn();
                if(i == 0){
                    ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "%zu", i + 1);
                } else if(i == 1){
                    ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "%zu", i + 1);
                } else if(i == 2){
                    ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.2f, 1.0f), "%zu", i + 1);
                } else {
                    ImGui::Text("%zu", i + 1);
                }
                
                ImGui::TableNextColumn();
                ImVec4 bot_color = get_bot_color(snapshots[i].id);
                ImGui::TextColored(bot_color, "%s", snapshots[i].name.c_str());
                
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", snapshots[i].elo);
                
                ImGui::TableNextColumn();
                if(snapshots[i].elo_delta > 0) {
                    ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "+%.1f", snapshots[i].elo_delta);
                } else if(snapshots[i].elo_delta < 0) {
                    ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%.1f", snapshots[i].elo_delta);
                } else {
                    ImGui::Text("%.1f", snapshots[i].elo_delta);
                }
                
                ImGui::TableNextColumn();
                if(snapshots[i].momentum > 0.5) {
                    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "UP %.1f", snapshots[i].momentum);
                } else if(snapshots[i].momentum < -0.5) {
                    ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "DN %.1f", snapshots[i].momentum);
                } else {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "-- %.1f", snapshots[i].momentum);
                }
            }
            
            ImGui::EndTable();
        }
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        ImGui::BeginChild("HeadToHeadMatrix", ImVec2(0, 0), true);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "HEAD-TO-HEAD");
        ImGui::Separator();
        
        bool has_h2h_data = false;
        for(const auto& bot : g_tournament.bots) {
            if(!bot.head_to_head_history.empty()) {
                has_h2h_data = true;
                break;
            }
        }
        
        if(!has_h2h_data) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No head-to-head data available.");
            ImGui::Text("Run a new tournament to generate this data.");
        } else {
            int num_bots = g_tournament.bots.size();
        if(ImGui::BeginTable("H2H", num_bots + 1, 
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY)){
            
            ImGui::TableSetupColumn("", 0, 1.0f);
            for(const auto& bot : g_tournament.bots) {
                ImGui::TableSetupColumn(bot.name.c_str(), 0, 0.8f);
            }
            ImGui::TableHeadersRow();
            
            for(const auto& bot : g_tournament.bots) {
                ImGui::TableNextRow();
                
                ImGui::TableNextColumn();
                ImVec4 bot_color = get_bot_color(bot.id);
                ImGui::TextColored(bot_color, "%s", bot.name.c_str());
                
                std::map<int, std::pair<int, int>> h2h_at_game;
                if(bot.head_to_head_history.count(g_selected_game)) {
                    h2h_at_game = bot.head_to_head_history.at(g_selected_game);
                } else {
                    for(auto it = bot.head_to_head_history.rbegin(); it != bot.head_to_head_history.rend(); ++it) {
                        if(it->first <= g_selected_game) {
                            h2h_at_game = it->second;
                            break;
                        }
                    }
                }
                
                for(const auto& opponent : g_tournament.bots) {
                    ImGui::TableNextColumn();
                    
                    if(bot.id == opponent.id) {
                        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "-");
                    } else {
                        int wins = 0, losses = 0;
                        if(h2h_at_game.count(opponent.id)) {
                            wins = h2h_at_game[opponent.id].first;
                            losses = h2h_at_game[opponent.id].second;
                        }
                        
                        if(wins + losses == 0) {
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "0-0");
                        } else {
                            double win_rate = (double)wins / (wins + losses);
                            if(win_rate > 0.6) {
                                ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "%d-%d", wins, losses);
                            } else if(win_rate < 0.4) {
                                ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "%d-%d", wins, losses);
                            } else {
                                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%d-%d", wins, losses);
                            }
                        }
                    }
                }
            }
            
            ImGui::EndTable();
        }
        }
        ImGui::EndChild();
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
    
    NFD_Init();

    // GL context setup - macOS requires 3.2 Core Profile
#if defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    int window_width = (int)(mode->width * 0.70f);   // 70% of screen width
    int window_height = (int)(mode->height * 0.75f); // 75% of screen height

    GLFWwindow* window = glfwCreateWindow(window_width, window_height, "BattleBot Replay Viewer", NULL, NULL);
    if(window == NULL) return 1;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    float xscale;
    glfwGetWindowContentScale(window, &xscale, nullptr);
    g_dpi_scale = xscale;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(g_dpi_scale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ImFontConfig font_cfg;
    font_cfg.SizePixels = 13.0f * g_dpi_scale;
    io.Fonts->AddFontDefault(&font_cfg);

    while(!glfwWindowShouldClose(window)){
        glfwPollEvents();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("BattleBot Viewer", NULL, 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        
        render_header();
        ImGui::Spacing();
        ImGui::BeginChild("MainContent", ImVec2(0, 0), false);
        
        if(g_view_mode == VIEW_REPLAY){
            ImGui::BeginChild("LeftPanel", ImVec2(ImGui::GetContentRegionAvail().x * 0.55f, 0), false);
            render_charts();
            ImGui::Spacing();
            render_game_log();
            ImGui::EndChild();
            
            ImGui::SameLine();
            
            ImGui::BeginChild("RightPanel", ImVec2(0, 0), false);
            render_player_status();
            ImGui::Spacing();
            render_bidding_results();
            ImGui::EndChild();
        }
        else if(g_view_mode == VIEW_TOURNAMENT){
            render_tournament_result();
        }
        
        ImGui::EndChild();
        
        ImGui::End();
        
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    NFD_Quit();
    
    return 0;
}
