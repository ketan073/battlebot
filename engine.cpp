#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

mt19937 rng(random_device{}());

const int NUM_PLAYER = 5;
const int NUM_ROUND = 20;
const string PLAYER_NAME[5] = {"Alex", "Bob", "Cindy", "David", "Eric"};
const int WATER_REQ[5] = {8, 9, 10, 11, 12};
const int SALARY[5] = {70, 75, 100, 120, 120};
const int INIT_HEALTH = 8;
const int MAX_HEALTH = 10;

struct Player {
    int id;
    string name;
    int water_req;
    int salary;
    int health;
    int balance;
    int no_water_days;
    int death_round;

    bool alive;
    int prev_bid;
    pid_t pid;
    int pipe_to_bot[2];
    int pipe_from_bot[2];
};

Player bot[NUM_PLAYER];

struct BidResult {
    int player_id;
    int bid;
    int water_req;
    bool won_water;
};

struct RoundData {
    int round;
    int water_supply;
    vector<BidResult> results;
    int health[NUM_PLAYER];
    int balance[NUM_PLAYER];
    bool alive[NUM_PLAYER];
};
struct Replay {
    string timestamp;
    vector<string> bot_paths;
    vector<RoundData> rounds;
    vector<string> log;
};

Replay game_replay;

bool g_tourney_mode = false;

bool cmp_bid(const BidResult& a, const BidResult& b){
    if(a.bid != b.bid) return a.bid > b.bid;
    else return a.water_req < b.water_req;
}

bool cmp_survivor(const int a, const int b){
    if(bot[a].health != bot[b].health) return bot[a].health > bot[b].health;
    if(bot[a].balance != bot[b].balance) return bot[a].balance > bot[b].balance;
    return false;
}

void clean_bots(){
    for(int i = 0; i < NUM_PLAYER; i++){
        if(bot[i].pid > 0){
            kill(bot[i].pid, SIGKILL);
            waitpid(bot[i].pid, NULL, 0);
        }
    }
}

void init_bots(){
    try{
        for(int i = 0; i < NUM_PLAYER; i++){
            bot[i].id = i + 1;
            bot[i].name = PLAYER_NAME[i];
            bot[i].water_req = WATER_REQ[i];
            bot[i].salary = SALARY[i];
            bot[i].health = INIT_HEALTH;
            bot[i].balance = SALARY[i];
            bot[i].no_water_days = 0;
            bot[i].death_round = 0;

            bot[i].alive = true;
            bot[i].prev_bid = 0;
            bot[i].pid = -1;
        }
    }
    catch(...){
        cerr << "Error: Could not initialize bots\n";
        exit(1);
    }
}

void child_bots(int idx, char* bot_path){
    try{
        Player &b = bot[idx];
        if(pipe(b.pipe_to_bot) < 0 || pipe(b.pipe_from_bot) < 0){
            cerr << "Error: Could not create pipes for bot: " << b.id << "\n";
            clean_bots();
            exit(1);
        }
        pid_t pid = fork();
        if(pid < 0){
            cerr << "Error: Could not fork the child bot: " << b.id << "\n";
            clean_bots();
            exit(1);
        }

        if(pid == 0){ // Child Process

            dup2(b.pipe_to_bot[0], 0); // stdin
            dup2(b.pipe_from_bot[1], 1); // stdout

            close(b.pipe_to_bot[1]);
            close(b.pipe_from_bot[0]);
            close(b.pipe_to_bot[0]);
            close(b.pipe_from_bot[1]);
            
            execlp(bot_path, bot_path, (char*)NULL);

            perror("execlp");
            _exit(1);
        }
        else{ // Parent Process
            b.pid = pid;
            close(b.pipe_to_bot[0]);
            close(b.pipe_from_bot[1]);
        }
    }
    catch(...){
        cerr << "Error: Could not make child bot: " << idx << "\n";
        clean_bots();
        exit(1);
    }
}

void send_initData(){
    try{
        for(int i = 0; i < NUM_PLAYER; i++){
            // Line 1: Player ID
            string init_data = to_string(bot[i].id) + "\n";

            // Lines 2-6: All player info
            for(int j = 0; j < NUM_PLAYER; j++){
                init_data += to_string(bot[j].id) + " " + bot[j].name + " " + to_string(bot[j].water_req) + " " + to_string(bot[j].salary) + "\n";
            }
            
            ssize_t ssw = write(bot[i].pipe_to_bot[1], init_data.c_str(), init_data.length());
            if(ssw != (ssize_t)init_data.length()){
                throw runtime_error("Could not write full init data to bot");
            }
        }
    }
    catch(...){
        cerr << "Error: Could not send initialization data to bots\n";
        clean_bots();
        exit(1);
    }
}

int aliveCnt(){
    int alive_cnt = 0;
    for(int i = 0; i < NUM_PLAYER; i++){
        if(bot[i].alive) alive_cnt++;
    }
    return alive_cnt;
}

vector<int> water_supply(NUM_ROUND);

void init_water_supply(){
    // int LOWER = 10;
    // int UPPER = 30;
    // uniform_int_distribution<int> dist(LOWER, UPPER);
    // for(int i = 0; i < NUM_ROUND; i++){
    //     water_supply[i] = dist(rng);
    // }
    // Data used in checkpoint_1
    // water_supply[0] = 49;
    // water_supply[1] = 30;
    // water_supply[2] = 34;
    // water_supply[3] = 49;
    // water_supply[4] = 10;

    // water_supply[5] = 0;
    // water_supply[6] = 40;
    // water_supply[7] = 25;
    // water_supply[8] = 33;
    // water_supply[9] = 34;

    // water_supply[10] = 22;
    // water_supply[11] = 27;
    // water_supply[12] = 11;
    // water_supply[13] = 29;
    // water_supply[14] = 40;

    // water_supply[15] = 32;
    // water_supply[16] = 22;
    // water_supply[17] = 28;
    // water_supply[18] = 25;
    // water_supply[19] = 20;

    // Data used in checkpoint_2
    water_supply[0] = 2;
    water_supply[1] = 0;
    water_supply[2] = 16;
    water_supply[3] = 17;
    water_supply[4] = 37;

    water_supply[5] = 12;
    water_supply[6] = 4;
    water_supply[7] = 15;
    water_supply[8] = 31;
    water_supply[9] = 4;

    water_supply[10] = 12;
    water_supply[11] = 30;
    water_supply[12] = 5;
    water_supply[13] = 13;
    water_supply[14] = 25;

    water_supply[15] = 12;
    water_supply[16] = 2;
    water_supply[17] = 45;
    water_supply[18] = 6;
    water_supply[19] = 73;
}

int gen_water_supply(int round){
    return water_supply[round];
}

void send_round_info(int round, int water_supply){
    try{
        for(int i = 0; i < NUM_PLAYER; i++){
            if(!bot[i].alive) continue;
            // Line 1: Round
            string data = to_string(round) + "\n";

            // Line 2: Water Supply
            data += to_string(water_supply) + "\n";

            // Line 3-7: Bot Info
            for(int j = 0; j < NUM_PLAYER; j++){
                data += to_string(bot[j].id) + " ";

                if(!bot[j].alive) data += "-1 0 0\n";
                else data += to_string(bot[j].health) + " " + to_string(bot[j].balance) + " " + to_string(bot[j].no_water_days) + "\n";
            }

            // Line 8-12: Bidding Info (Round# >= 2)
            if(round >= 2){
                for(int j = 0; j < NUM_PLAYER; j++){
                    data += to_string(bot[j].id) + " ";

                    if(!bot[j].alive) data += "0 " + to_string(bot[j].water_req) + "\n";
                    else data += to_string(bot[j].prev_bid) + " " + to_string(bot[j].water_req) + "\n";
                }
            }
            ssize_t ssw = write(bot[i].pipe_to_bot[1], data.c_str(), data.length());
            if(ssw != (ssize_t)data.length()){
                throw runtime_error("Could not write full round info to bot");
            }
        }
    }
    catch(...){
        cerr << "Error: Could not send round info to bots\n";
        clean_bots();
        exit(1);
    }
}

const int BID_TIMEOUT_SEC = 2;

void drain_pipe(int fd){
    char discard[256];
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    while(read(fd, discard, sizeof(discard)) > 0);
    fcntl(fd, F_SETFL, flags);
}

void get_bid(){
    for(int i = 0; i < NUM_PLAYER; i++){
        if(!bot[i].alive) continue;
        
        int bid = 0;
        try{
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(bot[i].pipe_from_bot[0], &read_fds);
            
            struct timeval timeout;
            timeout.tv_sec = BID_TIMEOUT_SEC;
            timeout.tv_usec = 0;
            
            int ready = select(bot[i].pipe_from_bot[0] + 1, &read_fds, nullptr, nullptr, &timeout);
            
            if(ready <= 0){
                if(ready == 0){
                    if(!g_tourney_mode){
                        cerr << "Warning: Bot " << bot[i].id << " (" << bot[i].name << ") timed out after " << BID_TIMEOUT_SEC << "s, using bid = 0\n";
                    }
                    game_replay.log.push_back("Bot " + bot[i].name + " timed out, bid = 0");
                    drain_pipe(bot[i].pipe_from_bot[0]);
                }
                else{
                    cerr << "Warning: select() error for bot " << bot[i].id << ", using bid = 0\n";
                }
                bot[i].prev_bid = 0;
                continue;
            }
            
            char buffer[256];
            ssize_t n = read(bot[i].pipe_from_bot[0], buffer, sizeof(buffer) - 1);
            if(n <= 0){
                cerr << "Warning: Could not read bid from bot " << bot[i].id << ", using bid = 0\n";
                bot[i].prev_bid = 0;
                continue;
            }
            buffer[n] = '\0';
            
            bid = stoi(string(buffer));
            
            if(bid < 0 || bid > bot[i].balance){
                cerr << "Warning: Bot " << bot[i].id << " submitted invalid bid: " << bid << " (balance: " << bot[i].balance << "), using bid = 0\n";
                bid = 0;
            }
        }
        catch(...){
            cerr << "Warning: Bot " << bot[i].id << " sent invalid data, using bid = 0\n";
            bid = 0;
        }
        
        bot[i].prev_bid = bid;
    }
}

vector<BidResult> run_auction(int water_supply){
    vector<BidResult> results;
    
    for(int i = 0; i < NUM_PLAYER; i++){
        BidResult r;
        r.player_id = bot[i].id;
        r.bid = bot[i].alive ? bot[i].prev_bid : 0;
        r.water_req = bot[i].water_req;
        r.won_water = false;
        results.push_back(r);
    }
    
    sort(results.begin(), results.end(), cmp_bid);
    
    int remaining = water_supply;
    for(auto& r : results){
        int idx = r.player_id - 1;
        if(!bot[idx].alive) continue;
        
        if(remaining >= r.water_req){
            r.won_water = true;
            remaining -= r.water_req;
            bot[idx].balance -= r.bid;
            bot[idx].health = min(bot[idx].health + 2, MAX_HEALTH);
            bot[idx].no_water_days = 0;
        }
        else{
            bot[idx].no_water_days++;
            bot[idx].health -= bot[idx].no_water_days;
        }
    }
    
    return results;
}

void check_eliminations(int round){
    for(int i = 0; i < NUM_PLAYER; i++){
        if(bot[i].alive && bot[i].health <= 0){
            bot[i].alive = false;
            bot[i].death_round = round;
            
            if(bot[i].pid > 0){
                kill(bot[i].pid, SIGKILL);
                waitpid(bot[i].pid, NULL, 0);
                bot[i].pid = -1;
            }
            
            if(!g_tourney_mode){
                cout << ">>> Bot: " << bot[i].id << " (" << bot[i].name << ") has been ELIMINATED in round " << round << "\n";
                cout.flush();
            }

            game_replay.log.push_back(">>> " + bot[i].name + " has been ELIMINATED in round " + to_string(round));
        }
    }
}

void pay_salaries(){
    for(int i = 0; i < NUM_PLAYER; i++){
        if(bot[i].alive){
            bot[i].balance += bot[i].salary;
        }
    }
}

void print_round_summary(int round, int water_supply, vector<BidResult>& results){

    cout << "\n---------- Round " << round << " ----------\n";
    cout << "Water Supply: " << water_supply << " units\n";
    cout << "\nBids and Allocations:\n";
    cout << setw(8) << "ID" << setw(10) << "Name" << setw(8) << "Bid" << setw(10) << "WaterReq" << setw(10) << "Won?\n";
    cout << "--------------------------------------------------\n";
    
    for(auto& r : results){
        int idx = r.player_id - 1;
        int display_bid = bot[idx].alive ? r.bid : 0;
        cout << setw(8) << r.player_id << setw(10) << bot[idx].name << setw(8) << ("$" + to_string(display_bid)) << setw(10) << r.water_req << setw(10) << (r.won_water ? "Yes" : "No") << "\n";
    }
    
    cout << "\nPlayer Status After Round " << round << ":\n";
    cout << setw(8) << "ID" << setw(10) << "Name" << setw(10) << "Health" << setw(10) << "Balance" << setw(12) << "NoWater\n";
    cout << "--------------------------------------------------\n";
    
    for(int i = 0; i < NUM_PLAYER; i++){
        cout << setw(8) << bot[i].id << setw(10) << bot[i].name;
        if(bot[i].alive){
            cout << setw(10) << bot[i].health << setw(10) << ("$" + to_string(bot[i].balance)) << setw(12) << bot[i].no_water_days << "\n";
        }
        else{
            cout << setw(10) << "-" << setw(10) << "-" << setw(12) << "-\n";
        }
    }
}

void print_final_results(){
    cout << "\n---------------------------------------------------\n";
    cout << "             GAME OVER - Final Results\n";
    cout << "---------------------------------------------------\n";
    
    vector<int> survivors;
    
    for(int i = 0; i < NUM_PLAYER; i++){
        if(bot[i].alive){
            survivors.push_back(i);
        }
    }
    
    sort(survivors.begin(), survivors.end(), cmp_survivor);
    
    if(survivors.empty()){
        cout << "No survivors! Everyone was eliminated.\n";
    }
    else{
        cout << "\nSurvivors (ranked by health, then balance):\n";
        int rank = 1;
        for(int idx : survivors){
            cout << rank++ << ". " << bot[idx].name << " (Health: " << bot[idx].health << ", Balance: $" << bot[idx].balance << ")\n";
        }
        
        cout << "\nEliminated players:\n";
        int dead_cnt = 0;
        for(int i = 0; i < NUM_PLAYER; i++){
            if(!bot[i].alive){
                cout << "- " << bot[i].name << "\n";
                dead_cnt++;
            }
        }
        if(dead_cnt == 0){
            cout << "None\n";
        }
    }
}

void save_replay(const string& filename){
    ofstream file(filename);
    if(!file.is_open()){
        cerr << "Warning: Could not save replay to " << filename << "\n";
        return;
    }
    
    file << "{\n";
    file << "  \"version\": 1,\n";
    file << "  \"timestamp\": \"" << game_replay.timestamp << "\",\n";
    
    file << "  \"bot_paths\": [";
    for(size_t i = 0; i < game_replay.bot_paths.size(); i++){
        file << "\"" << game_replay.bot_paths[i] << "\"";
        if(i < game_replay.bot_paths.size() - 1) file << ", ";
    }
    file << "],\n";
    
    file << "  \"rounds\": [\n";
    for(size_t ri = 0; ri < game_replay.rounds.size(); ri++){
        RoundData& rd = game_replay.rounds[ri];
        file << "    {\n";
        file << "      \"round\": " << rd.round << ",\n";
        file << "      \"water_supply\": " << rd.water_supply << ",\n";
        
        file << "      \"bids\": [";
        for(size_t bi = 0; bi < rd.results.size(); bi++){
            BidResult& br = rd.results[bi];
            file << "{\"id\":" << br.player_id << ",\"bid\":" << br.bid << ",\"req\":" << br.water_req << ",\"won\":" << (br.won_water ? "true" : "false") << "}";
            if(bi < rd.results.size() - 1) file << ",";
        }
        file << "],\n";
        
        file << "      \"health\": [";
        for(int i = 0; i < NUM_PLAYER; i++){
            file << rd.health[i];
            if(i < NUM_PLAYER - 1) file << ",";
        }
        file << "],\n";
        
        file << "      \"balance\": [";
        for(int i = 0; i < NUM_PLAYER; i++){
            file << rd.balance[i];
            if(i < NUM_PLAYER - 1) file << ",";
        }
        file << "],\n";
        
        file << "      \"alive\": [";
        for(int i = 0; i < NUM_PLAYER; i++){
            file << (rd.alive[i] ? "true" : "false");
            if(i < NUM_PLAYER - 1) file << ",";
        }
        file << "]\n";
        
        file << "    }";
        if(ri < game_replay.rounds.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ],\n";
    
    // Log
    file << "  \"log\": [\n";
    for(size_t i = 0; i < game_replay.log.size(); i++){
        string msg = game_replay.log[i];
        
        size_t pos = 0;
        while((pos = msg.find("\"", pos)) != string::npos){
            msg.replace(pos, 1, "\\\"");
            pos += 2;
        }
        file << "    \"" << msg << "\"";
        if(i < game_replay.log.size() - 1) file << ",";
        file << "\n";
    }
    file << "  ]\n";
    
    file << "}\n";
    file.close();
}

int main(int argc, char* argv[]){
    bool tourney_mode = false;
    string replay_output_path = "";
    int bot_start_idx = 1;
    
    for(int i = 1; i < argc; i++){
        string arg = argv[i];
        if(arg == "--tourney"){
            tourney_mode = true;
            g_tourney_mode = true;
        }
        else if(arg == "--replay-output"){
            if(i + 1 < argc){
                replay_output_path = argv[i + 1];
                i++;
            }
            else{
                cerr << "Error: --replay-output requires a path argument\n";
                exit(1);
            }
        }
        else{
            bot_start_idx = i;
            break;
        }
    }
    
    int num_bots = argc - bot_start_idx;
    if(num_bots != NUM_PLAYER){
        cerr << "Need " << NUM_PLAYER << " bots to run this\n";
        cerr << "Usage: ./engine [--tourney] [--replay-output <path>] bot1 bot2 bot3 bot4 bot5\n";
        cerr << "\nFlags:\n";
        cerr << "  --tourney           Tournament mode: JSON output to stdout, no console output\n";
        cerr << "  --replay-output X   Save full replay to path X (only with --tourney)\n";
        exit(1);
    }

    time_t now = time(nullptr);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    game_replay.timestamp = timestamp;
    
    for(int i = bot_start_idx; i < bot_start_idx + NUM_PLAYER; i++){
        game_replay.bot_paths.push_back(argv[i]);
    }
    
    if(!tourney_mode){
        cout << "--------------------------------------------\n";
        cout << "            BATTLEBOT CHALLENGE             \n";
        cout << "--------------------------------------------\n";
    }

    init_bots();
    init_water_supply();

    if(!tourney_mode){
        cout << "Initialized " << NUM_PLAYER << " bots successfully\n";
    }
    game_replay.log.push_back("Initialized " + to_string(NUM_PLAYER) + " bots");

    for(int i = 0; i < NUM_PLAYER; i++){
        child_bots(i, argv[i + bot_start_idx]);
        if(!tourney_mode){
            cout << "  Player " << (i + 1) << " (" << PLAYER_NAME[i] << "): " << argv[i + bot_start_idx] << endl;
        }
        game_replay.log.push_back("  Player " + to_string(i+1) + " (" + PLAYER_NAME[i] + "): " + argv[i + bot_start_idx]);
    }

    usleep(100000);

    if(!tourney_mode){
        cout << "\nSending initialization data...\n";
    }
    send_initData();
    game_replay.log.push_back("Sending initialization data...");

    if(!tourney_mode){
        cout << "\nLet the game begin!\n";
    }
    game_replay.log.push_back("Game started");

    int last_round = 0;
    for(int round = 1; round <= NUM_ROUND; round++){
        last_round = round;
        int alive_cnt = aliveCnt();
        if(alive_cnt <= 1){
            if(!tourney_mode){
                cout << "\nGame ended (< 2 bots left)\nRound: " << round << "\nBot alive#: " << alive_cnt << "\n";
            }
            game_replay.log.push_back("Game ended (< 2 bots left)");
            break;
        }
        int water_supply = gen_water_supply(round - 1);

        game_replay.log.push_back("--- Round " + to_string(round) + " | Water: " + to_string(water_supply) + " units ---");
        
        send_round_info(round, water_supply);
        
        get_bid();
        
        vector<BidResult> results = run_auction(water_supply);
        
        if(round < NUM_ROUND){
            pay_salaries();
        }

        RoundData rd;
        rd.round = round;
        rd.water_supply = water_supply;
        rd.results = results;
        for(int i = 0; i < NUM_PLAYER; i++){
            rd.health[i] = bot[i].health;
            rd.balance[i] = bot[i].balance;
            rd.alive[i] = bot[i].alive;
        }
        game_replay.rounds.push_back(rd);

        if(!tourney_mode){
            print_round_summary(round, water_supply, results);
        }
        
        check_eliminations(round);
    }

    if(!tourney_mode){
        print_final_results();
    }
    game_replay.log.push_back("=== GAME OVER ===");

    clean_bots();

    if(tourney_mode){
        cout << "{\"health\":[";
        for(int i = 0; i < NUM_PLAYER; i++){
            cout << bot[i].health;
            if(i < NUM_PLAYER - 1) cout << ",";
        }
        cout << "],\"balance\":[";
        for(int i = 0; i < NUM_PLAYER; i++){
            cout << bot[i].balance;
            if(i < NUM_PLAYER - 1) cout << ",";
        }
        cout << "],\"alive\":[";
        for(int i = 0; i < NUM_PLAYER; i++){
            cout << (bot[i].alive ? "true" : "false");
            if(i < NUM_PLAYER - 1) cout << ",";
        }
        cout << "],\"death_round\":[";
        for(int i = 0; i < NUM_PLAYER; i++){
            cout << bot[i].death_round;
            if(i < NUM_PLAYER - 1) cout << ",";
        }
        cout << "],\"rounds\":" << last_round << "}" << endl;
        
        if(!replay_output_path.empty()){
            save_replay(replay_output_path);
        }
    }
    else{
        struct stat st;
        if(stat("replays", &st) != 0){
            mkdir("replays", 0775);
        }
        
        char cur_time[32];
        strftime(cur_time, sizeof(cur_time), "%d-%H%M%S", localtime(&now));

        string bot_names = "";
        for(int i = 0; i < NUM_PLAYER; i++){
            string path = game_replay.bot_paths[i];
            size_t pos = path.rfind('/');
            string name = (pos != string::npos) ? path.substr(pos + 1) : path;

            if(name.substr(0, 2) == "./") name = name.substr(2);
            bot_names += name;
            if(i < NUM_PLAYER - 1) bot_names += "-";
        }
        
        string replay_file = "replays/game-" + string(cur_time) + "-" + bot_names + ".json";
        save_replay(replay_file);
        cout << "\nReplay saved to: " << replay_file << "\n";
    }
    
    return 0;
}