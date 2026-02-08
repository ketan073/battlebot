#include <iostream>
#include <string>
#include <vector>
using namespace std;

struct Player {
    int id;
    string name;
    int water_req;
    int salary;
    int health;
    int balance;
    int no_water_days;
};

int main() {
    // Round 0: Initialization
    int my_player_id;
    cin >> my_player_id;
    
    vector<Player> players(5);
    for (int i = 0; i < 5; i++) {
        cin >> players[i].id >> players[i].name 
            >> players[i].water_req >> players[i].salary;
    }
    // Main game loop
    for (int round = 1; round <= 20; round++) {
        int water_supply;
        cin >> round >> water_supply;
        
        // Read player statuses
        for (int i = 0; i < 5; i++) {
            cin >> players[i].id >> players[i].health 
                >> players[i].balance >> players[i].no_water_days;
        }
        // Read previous bids (from round 2 onwards)
        if (round >= 2) {
            vector<int> prev_bids(5);
            vector<int> water_reqs(5);
            for (int i = 0; i < 5; i++) {
                int pid;
                cin >> pid >> prev_bids[i] >> water_reqs[i];
            }
        }
        
        /* YOUR STRATEGY HERE */
        int my_bid = 0;
        
        // Output bid
        cout << my_bid << endl;
        cout.flush(); 
    }
    
    return 0;
}