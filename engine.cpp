#include <bits/stdc++.h>
#include <wait.h>
#include <signal.h>

using namespace std;

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

    bool alive;
    int prev_bid;
    pid_t pid;
    int pipe_to_bot[2];
    int pipe_from_bot[2];
};

Player bot[NUM_PLAYER];

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
            
            execlp(bot_path, bot_path, NULL);

            cerr << "Error: Could not exec bot " << b.id << "\n";
            clean_bots();
            exit(1);
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

int gen_water_supply(){
    int LOWER = 10;
    int UPPER = 30;
    return LOWER + (rand() % (UPPER - LOWER + 1));
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

// TODO: add time limit for bidding phase
void get_bid(){
    for(int i = 0; i < NUM_PLAYER; i++){
        if(!bot[i].alive) continue;
        
        int bid = 0;
        try{
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

struct BidResult {
    int player_id;
    int bid;
    int water_req;
    bool won_water;
};

bool cmp_bid(BidResult& a, BidResult& b){
    if(a.bid != b.bid) return a.bid > b.bid;
    else return a.water_req < b.water_req;
}

bool cmp_survivor(int a, int b){
    if(bot[a].health != bot[b].health) return bot[a].health > bot[b].health;
    return bot[a].balance > bot[b].balance;
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

void check_eliminations(){
    for(int i = 0; i < NUM_PLAYER; i++){
        if(bot[i].alive && bot[i].health <= 0){
            bot[i].alive = false;
            bot[i].balance = 0;
            
            if(bot[i].pid > 0){
                kill(bot[i].pid, SIGKILL);
                waitpid(bot[i].pid, NULL, 0);
                bot[i].pid = -1;
            }
            
            cout << ">>> Bot: " << bot[i].id << " (" << bot[i].name << ") has been ELIMINATED\n";
            cout.flush();
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

int main(int argc, char* argv[]){
    if(argc != NUM_PLAYER + 1){
        cerr << "Need " << NUM_PLAYER << " bots to run this\n";
        cerr << "Expected CMD: ./engine";
        for(int i = 0; i < NUM_PLAYER; i++){
            cerr << " ./bot" << (i + 1);
        }
        cerr << "\n";
        exit(1);
    }
    
    cout << "--------------------------------------------\n";
    cout << "            BATTLEBOT CHALLENGE             \n";
    cout << "--------------------------------------------\n";


    init_bots();
    cout << "Initilized " << NUM_PLAYER << " bots successfully\n";

    for(int i = 0; i < NUM_PLAYER; i++){
        child_bots(i, argv[i + 1]);
        cout << "  Player " << (i + 1) << " (" << PLAYER_NAME[i] << "): " << argv[i + 1] << endl;
    }

    usleep(100000);
    srand(time(nullptr));

    cout << "\nSending initialization data...\n";
    send_initData();

    cout << "\nLet the game begin!\n";

    for(int round = 1; round <= NUM_ROUND; round++){

        int alive_cnt = aliveCnt();
        if(alive_cnt <= 1){
            cout << "\nGame ended (< 2 bots left)\nRound: " << round << "\nBot alive#: " << alive_cnt << "\n";
            break;
        }
        int water_supply = gen_water_supply();
        
        send_round_info(round, water_supply);
        
        get_bid();
        
        vector<BidResult> results = run_auction(water_supply);
        
        if(round < NUM_ROUND){
            pay_salaries();
        }
        
        print_round_summary(round, water_supply, results);
        
        check_eliminations();
    }

    print_final_results();
    clean_bots();
    
    return 0;
}
