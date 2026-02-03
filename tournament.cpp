#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

using namespace std;

const int PLAYERS_PER_GAME = 5;
const double INITIAL_ELO = 1500.0;
const double K_FACTOR = 32.0;

bool g_verbose = false;

struct BotRating {
    int id;
    string name;
    string path;
    string unique_key;
    double elo;
    int games_played;
    int wins;
    int top2_finishes;
    int top3_finishes;
    int total_rounds_survived;
    int total_health;
    int total_balance;
    int eliminations;
    int survivals;
    int total_position;
    int total_dominance;
    vector<pair<int, double>> elo_history;
    map<string, int> head_to_head_wins;
    map<string, int> head_to_head_losses;
    map<int, map<string, pair<int, int>>> head_to_head_history;
    
    BotRating(int bot_id, const string& n, const string& p) : 
        id(bot_id), name(n), path(p), unique_key(p + "_" + to_string(bot_id)),
        elo(INITIAL_ELO), games_played(0), 
        wins(0), top2_finishes(0), top3_finishes(0), 
        total_rounds_survived(0), total_health(0), total_balance(0),
        eliminations(0), survivals(0), total_position(0), total_dominance(0) {
        elo_history.push_back({0, INITIAL_ELO});
    }
};

struct GameResult {
    vector<string> bot_unique_keys;
    vector<int> rankings;
    vector<int> health;
    vector<int> balance;
    vector<int> rounds_survived;
    string replay_file;
};

double expected_score(double rating_a, double rating_b) {
    return 1.0 / (1.0 + pow(10.0, (rating_b - rating_a) / 400.0));
}

void update_elo(vector<BotRating>& ratings, const GameResult& result) {
    int n = result.rankings.size();
    vector<int> idx_map(n);
    
    for(int i = 0; i < n; i++) {
        for(size_t j = 0; j < ratings.size(); j++) {
            if(ratings[j].unique_key == result.bot_unique_keys[result.rankings[i]]) {
                idx_map[i] = j;
                break;
            }
        }
    }
    
    vector<double> elo_changes(n, 0.0);
    
    for(int i = 0; i < n; i++) {
        for(int j = i + 1; j < n; j++) {
            int bot_i = idx_map[i];
            int bot_j = idx_map[j];
            
            double exp_i = expected_score(ratings[bot_i].elo, ratings[bot_j].elo);
            double exp_j = 1.0 - exp_i;
            
            double score_i = 1.0;
            double score_j = 0.0;
            
            elo_changes[i] += K_FACTOR * (score_i - exp_i);
            elo_changes[j] += K_FACTOR * (score_j - exp_j);
        }
    }
    
    for(int i = 0; i < n; i++) {
        int bot_idx = idx_map[i];
        ratings[bot_idx].elo += elo_changes[i] / (n - 1);
        ratings[bot_idx].games_played++;
        
        if(i == 0) {
            ratings[bot_idx].wins++;
            int winner_health = result.health[result.rankings[0]];
            int second_health = result.health[result.rankings[1]];
            ratings[bot_idx].total_dominance += (winner_health - second_health);
        }
        if(i < 2) ratings[bot_idx].top2_finishes++;
        if(i < 3) ratings[bot_idx].top3_finishes++;
        
        ratings[bot_idx].total_rounds_survived += result.rounds_survived[result.rankings[i]];
        ratings[bot_idx].total_health += result.health[result.rankings[i]];
        ratings[bot_idx].total_balance += result.balance[result.rankings[i]];
        
        ratings[bot_idx].total_position += (i + 1);
        if(result.health[result.rankings[i]] <= 0) {
            ratings[bot_idx].eliminations++;
        } else {
            ratings[bot_idx].survivals++;
        }
        
        for(int j = 0; j < n; j++) {
            if(i != j) {
                int other_bot_idx = idx_map[j];
                string other_key = ratings[other_bot_idx].unique_key;
                if(i < j) {
                    ratings[bot_idx].head_to_head_wins[other_key]++;
                } else {
                    ratings[bot_idx].head_to_head_losses[other_key]++;
                }
            }
        }
    }
}

GameResult run_game(const vector<string>& bot_unique_keys, const vector<string>& bot_paths, int game_num, int total_games, const string& tournament_folder) {
    GameResult result;
    result.bot_unique_keys = bot_unique_keys;
    
    cout << "\rRunning game " << game_num << "/" << total_games << " ... " << flush;
    
    string cmd = "./engine --tourney";
    
    if(g_verbose) {
        stringstream ss;
        ss << tournament_folder << "/game_" << setw(6) << setfill('0') << game_num << ".json";
        result.replay_file = ss.str();
        cmd += " --replay-output " + result.replay_file;
    }
    
    for(const auto& path : bot_paths) {
        cmd += " " + path;
    }
    cmd += " 2>/dev/null";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if(!pipe) {
        cerr << "\nError: Could not run engine command" << endl;
        return result;
    }
    
    char buffer[1024];
    string output;
    while(fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    int ret = pclose(pipe);
    
    if(ret != 0) {
        cerr << "\nError running game, engine returned: " << ret << endl;
        return result;
    }
    
    size_t health_pos = output.find("\"health\":[");
    if(health_pos == string::npos) {
        cerr << "\nError: Could not parse engine output" << endl;
        return result;
    }
    size_t health_start = output.find('[', health_pos);
    size_t health_end = output.find(']', health_start);
    string health_str = output.substr(health_start + 1, health_end - health_start - 1);
    
    istringstream health_stream(health_str);
    int h;
    char comma;
    for(int i = 0; i < PLAYERS_PER_GAME; i++) {
        if(i > 0) health_stream >> comma;
        health_stream >> h;
        result.health.push_back(h);
    }
    
    size_t balance_pos = output.find("\"balance\":[");
    size_t balance_start = output.find('[', balance_pos);
    size_t balance_end = output.find(']', balance_start);
    string balance_str = output.substr(balance_start + 1, balance_end - balance_start - 1);
    
    istringstream balance_stream(balance_str);
    int b;
    for(int i = 0; i < PLAYERS_PER_GAME; i++) {
        if(i > 0) balance_stream >> comma;
        balance_stream >> b;
        result.balance.push_back(b);
    }
    
    size_t rounds_pos = output.find("\"rounds\":");
    int rounds_played = 20;
    if(rounds_pos != string::npos) {
        istringstream rounds_stream(output.substr(rounds_pos + 9));
        rounds_stream >> rounds_played;
    }
    
    for(int i = 0; i < PLAYERS_PER_GAME; i++) {
        if(result.health[i] > 0) {
            result.rounds_survived.push_back(rounds_played);
        } else {
            result.rounds_survived.push_back(rounds_played / 2);
        }
    }
    
    vector<int> order(PLAYERS_PER_GAME);
    iota(order.begin(), order.end(), 0);
    
    sort(order.begin(), order.end(), [&](int a, int b) {
        bool a_alive = (result.health[a] > 0);
        bool b_alive = (result.health[b] > 0);
        
        if(a_alive != b_alive) return a_alive > b_alive;
        if(result.health[a] != result.health[b]) return result.health[a] > result.health[b];
        return result.balance[a] > result.balance[b];
    });
    
    result.rankings = order;
    
    return result;
}

void generate_combinations(const vector<int>& bot_ids, int k, int start, vector<int>& current, vector<vector<int>>& all_combos) {
    if(current.size() == (size_t)k) {
        all_combos.push_back(current);
        return;
    }
    
    for(size_t i = start; i < bot_ids.size(); i++) {
        current.push_back(bot_ids[i]);
        generate_combinations(bot_ids, k, i + 1, current, all_combos);
        current.pop_back();
    }
}

void generate_permutations(vector<int> combo, int start, vector<vector<int>>& all_perms) {
    if((size_t)start == combo.size()) {
        all_perms.push_back(combo);
        return;
    }
    
    for(size_t i = start; i < combo.size(); i++) {
        swap(combo[start], combo[i]);
        generate_permutations(combo, start + 1, all_perms);
        swap(combo[start], combo[i]);
    }
}

long long factorial(int n) {
    long long result = 1;
    for(int i = 2; i <= n; i++) {
        result *= i;
    }
    return result;
}

long long binomial(int n, int k) {
    if(k > n) return 0;
    if(k == 0 || k == n) return 1;
    
    long long result = 1;
    for(int i = 0; i < k; i++) {
        result *= (n - i);
        result /= (i + 1);
    }
    return result;
}

void save_tournament_result(const vector<BotRating>& ratings, int total_games, const string& output_folder) {
    mkdir(output_folder.c_str(), 0775);
    
    string results_file = output_folder + "/tournament_result.json";
    ofstream out(results_file);
    out << "{\n";
    out << "  \"total_games\": " << total_games << ",\n";
    out << "  \"timestamp\": \"" << time(nullptr) << "\",\n";
    out << "  \"ratings\": [\n";
    
    // Sort bots by ELO
    vector<int> order(ratings.size());
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        return ratings[a].elo > ratings[b].elo;
    });
    
    for(size_t i = 0; i < ratings.size(); i++) {
        const auto& bot = ratings[order[i]];
        out << "    {\n";
        out << "      \"rank\": " << (i + 1) << ",\n";
        out << "      \"id\": " << bot.id << ",\n";
        out << "      \"name\": \"" << bot.name << "\",\n";
        out << "      \"path\": \"" << bot.path << "\",\n";
        out << "      \"elo\": " << fixed << setprecision(2) << bot.elo << ",\n";
        out << "      \"games_played\": " << bot.games_played << ",\n";
        out << "      \"wins\": " << bot.wins << ",\n";
        out << "      \"win_rate\": " << (bot.games_played > 0 ? (100.0 * bot.wins / bot.games_played) : 0) << ",\n";
        out << "      \"top2_finishes\": " << bot.top2_finishes << ",\n";
        out << "      \"top3_finishes\": " << bot.top3_finishes << ",\n";
        out << "      \"avg_rounds_survived\": " << (bot.games_played > 0 ? (1.0 * bot.total_rounds_survived / bot.games_played) : 0) << ",\n";
        out << "      \"avg_final_health\": " << (bot.games_played > 0 ? (1.0 * bot.total_health / bot.games_played) : 0) << ",\n";
        out << "      \"avg_final_balance\": " << (bot.games_played > 0 ? (1.0 * bot.total_balance / bot.games_played) : 0) << ",\n";
        out << "      \"elimination_rate\": " << (bot.games_played > 0 ? (100.0 * bot.eliminations / bot.games_played) : 0) << ",\n";
        out << "      \"survival_rate\": " << (bot.games_played > 0 ? (100.0 * bot.survivals / bot.games_played) : 0) << ",\n";
        out << "      \"avg_position\": " << (bot.games_played > 0 ? (1.0 * bot.total_position / bot.games_played) : 0) << ",\n";
        out << "      \"elo_change\": " << fixed << setprecision(2) << (bot.elo - INITIAL_ELO) << ",\n";
        out << "      \"avg_dominance\": " << (bot.wins > 0 ? (1.0 * bot.total_dominance / bot.wins) : 0) << ",\n";
        out << "      \"elo_history\": [\n";
        for(size_t j = 0; j < bot.elo_history.size(); j++) {
            out << "        {\"game\": " << bot.elo_history[j].first 
                << ", \"elo\": " << fixed << setprecision(2) << bot.elo_history[j].second << "}";
            if(j < bot.elo_history.size() - 1) out << ",";
            out << "\n";
        }
        out << "      ],\n";
        out << "      \"head_to_head_history\": {\n";
        size_t h2h_count = 0;
        for(const auto& [game, h2h_map] : bot.head_to_head_history) {
            out << "        \"" << game << "\": {\n";
            size_t bot_count = 0;
            for(const auto& [other_key, wl] : h2h_map) {
                int other_id = -1;
                string other_name = "";
                for(const auto& r : ratings) {
                    if(r.unique_key == other_key) {
                        other_name = r.name;
                        other_id = r.id;
                        break;
                    }
                }
                out << "          \"" << other_id << "\": {\"name\": \"" << other_name 
                    << "\", \"wins\": " << wl.first << ", \"losses\": " << wl.second << "}";
                if(bot_count < h2h_map.size() - 1) out << ",";
                out << "\n";
                bot_count++;
            }
            out << "        }";
            if(h2h_count < bot.head_to_head_history.size() - 1) out << ",";
            out << "\n";
            h2h_count++;
        }
        out << "      }\n";
        out << "    }";
        if(i < ratings.size() - 1) out << ",";
        out << "\n";
    }
    
    out << "  ]\n";
    out << "}\n";
    out.close();
    
    cout << "\n\nTournament results saved to: " << results_file << "\n";
}

void print_results(const vector<BotRating>& ratings, int total_games) {
    vector<int> order(ratings.size());
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b) {
        return ratings[a].elo > ratings[b].elo;
    });
    
    cout << "\n\n";
    cout << "---------------------------------------------------------------------------------\n";
    cout << "                           TOURNAMENT RESULT                                     \n";
    cout << "---------------------------------------------------------------------------------\n\n";
    
    cout << "Total games played: " << total_games << "\n\n";
    
    cout << left << setw(5) << "Rank" 
         << setw(5) << "ID"
         << setw(18) << "Bot Name" 
         << setw(10) << "ELO" 
         << setw(8) << "Games"
         << setw(8) << "Wins"
         << setw(10) << "Win%"
         << setw(8) << "Top-2"
         << setw(8) << "Top-3" << "\n";
    cout << string(90, '-') << "\n";
    
    for(size_t i = 0; i < ratings.size(); i++) {
        const auto& bot = ratings[order[i]];
        double win_rate = bot.games_played > 0 ? (100.0 * bot.wins / bot.games_played) : 0.0;
        
        cout << left << setw(5) << (i + 1)
             << setw(5) << bot.id
             << setw(18) << bot.name
             << setw(10) << fixed << setprecision(1) << bot.elo
             << setw(8) << bot.games_played
             << setw(8) << bot.wins
             << setw(10) << fixed << setprecision(1) << win_rate
             << setw(8) << bot.top2_finishes
             << setw(8) << bot.top3_finishes << "\n";
    }
    
    cout << "\n";
    cout << "Detailed Statistics:\n";
    cout << string(90, '-') << "\n";
    cout << left << setw(5) << "ID"
         << setw(18) << "Bot Name"
         << setw(15) << "Avg Rounds"
         << setw(15) << "Avg Health"
         << setw(15) << "Avg Balance" << "\n";
    cout << string(90, '-') << "\n";
    
    for(size_t i = 0; i < ratings.size(); i++) {
        const auto& bot = ratings[order[i]];
        double avg_rounds = bot.games_played > 0 ? (1.0 * bot.total_rounds_survived / bot.games_played) : 0.0;
        double avg_health = bot.games_played > 0 ? (1.0 * bot.total_health / bot.games_played) : 0.0;
        double avg_balance = bot.games_played > 0 ? (1.0 * bot.total_balance / bot.games_played) : 0.0;
        
        cout << left << setw(5) << bot.id
             << setw(18) << bot.name
             << setw(15) << fixed << setprecision(1) << avg_rounds
             << setw(15) << fixed << setprecision(1) << avg_health
             << setw(15) << fixed << setprecision(1) << avg_balance << "\n";
    }
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        cerr << "Usage: " << argv[0] << " [--verbose] <bot1> <bot2> <bot3> ... <botN>\n";
        cerr << "Minimum " << PLAYERS_PER_GAME << " bots required.\n";
        cerr << "\nOptions:\n";
        cerr << "  --verbose  Save all game replay files (default: only tournament_result.json)\n";
        cerr << "\nThe tournament will run all possible combinations of bots.\n";
        cerr << "For N bots: C(N,5) * 5! = " << "N!/(5!*(N-5)!) * 120 games\n";
        cerr << "\nExamples:\n";
        cerr << "  N=5:  1 * 120 = 120 games\n";
        cerr << "  N=6:  6 * 120 = 720 games\n";
        cerr << "  N=7:  21 * 120 = 2,520 games\n";
        cerr << "  N=8:  56 * 120 = 6,720 games\n";
        return 1;
    }
    
    time_t now = time(nullptr);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));
    string output_folder = "replays/tourney_" + string(timestamp);
    int bot_start_idx = 1;
    g_verbose = false;
    
    for(int i = 1; i < argc; i++) {
        string arg = argv[i];
        if(arg == "--verbose") {
            g_verbose = true;
        } else {
            bot_start_idx = i;
            break;
        }
    }
    
    int n = argc - bot_start_idx;
    if(n < PLAYERS_PER_GAME) {
        cerr << "Error: Need at least " << PLAYERS_PER_GAME << " bots\n";
        return 1;
    }
    
    mkdir("replays", 0775);
    mkdir(output_folder.c_str(), 0775);
    
    vector<string> bot_paths;
    vector<BotRating> ratings;
    
    for(int i = bot_start_idx; i < argc; i++) {
        string path = argv[i];
        bot_paths.push_back(path);
        
        size_t pos = path.rfind('/');
        string name = (pos != string::npos) ? path.substr(pos + 1) : path;
        if(name.substr(0, 2) == "./") name = name.substr(2);
        
        int bot_id = i - bot_start_idx + 1;
        ratings.emplace_back(bot_id, name, path);
    }
    
    long long num_combinations = binomial(n, PLAYERS_PER_GAME);
    long long num_permutations = factorial(PLAYERS_PER_GAME);
    long long total_games = num_combinations * num_permutations;
    
    int elo_snapshot_interval = max(10, (int)(total_games / 80));
    
    cout << "---------------------------------------------------------------------------------\n";
    cout << "                         BATTLEBOT TOURNAMENT                                    \n";
    cout << "---------------------------------------------------------------------------------\n\n";
    
    cout << "Number of bots: " << n << "\n";
    cout << "Combinations C(" << n << "," << PLAYERS_PER_GAME << "): " << num_combinations << "\n";
    cout << "Permutations per combination: " << num_permutations << "\n";
    cout << "Total games to play: " << total_games << "\n";
    if(g_verbose) {
        cout << "Output folder: " << output_folder << "\n";
        cout << "Mode: VERBOSE (all replays will be saved)\n\n";
    } else {
        cout << "Mode: NORMAL (no replay files saved, only tournament results)\n\n";
    }
    
    cout << "Bots:\n";
    for(size_t i = 0; i < ratings.size(); i++) {
        cout << "  ID " << ratings[i].id << ": " << ratings[i].name << " (" << ratings[i].path << ")\n";
    }
    cout << "\n";
    
    vector<int> bot_ids;
    for(size_t i = 0; i < ratings.size(); i++) {
        bot_ids.push_back(ratings[i].id);
    }
    
    vector<vector<int>> all_combos;
    vector<int> current;
    generate_combinations(bot_ids, PLAYERS_PER_GAME, 0, current, all_combos);
    
    cout << "Generated " << all_combos.size() << " combinations.\n";
    cout << "Starting tournament...\n\n";
    
    int game_count = 0;
    for(const auto& combo : all_combos) {
        vector<vector<int>> perms;
        generate_permutations(combo, 0, perms);
        
        for(const auto& perm : perms) {
            game_count++;

            vector<string> game_paths;
            vector<string> game_keys;
            for(int bot_id : perm) {
                for(const auto& rating : ratings) {
                    if(rating.id == bot_id) {
                        game_paths.push_back(rating.path);
                        game_keys.push_back(rating.unique_key);
                        break;
                    }
                }
            }
            
            GameResult result = run_game(game_keys, game_paths, game_count, total_games, output_folder);
            
            if(!result.rankings.empty()) {
                update_elo(ratings, result);
                
                if(game_count % elo_snapshot_interval == 0) {
                    for(auto& rating : ratings) {
                        rating.elo_history.push_back({game_count, rating.elo});
                        
                        map<string, pair<int, int>> h2h_snapshot;
                        for(const auto& other : ratings) {
                            if(rating.unique_key != other.unique_key) {
                                int wins = rating.head_to_head_wins[other.unique_key];
                                int losses = rating.head_to_head_losses[other.unique_key];
                                h2h_snapshot[other.unique_key] = {wins, losses};
                            }
                        }
                        rating.head_to_head_history[game_count] = h2h_snapshot;
                    }
                }
            }
        }
    }
    
    for(auto& rating : ratings) {
        rating.elo_history.push_back({game_count, rating.elo});
    }
    
    cout << "\n\nTournament complete!\n";
    
    print_results(ratings, game_count);
    save_tournament_result(ratings, game_count, output_folder);
    
    return 0;
}
