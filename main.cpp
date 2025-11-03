#include <bits/stdc++.h>
using namespace std;

struct Submission { int prob; string status; int time; long long seq; };

struct ProblemState {
    // Persisted (revealed) state across cycles
    bool solved = false;
    int first_ac_time = -1;
    int wrong_before_ac = 0; // number of wrong submissions before first AC (persisted)

    // All submissions history for this problem
    vector<Submission> subs;

    // Freeze-related (for current freeze cycle)
    bool eligibleFreeze = false; // was unsolved at freeze time
    int freezeStartIdx = 0;      // index in subs at freeze time

    // Helpers: count wrongs in [l, r) until first AC (exclusive)
    static int count_wrongs_until_ac(const vector<Submission>& s, int l, int r) {
        int w = 0;
        for (int i = l; i < r; ++i) {
            if (s[i].status == "Accepted") break;
            ++w;
        }
        return w;
    }
    static int first_ac_time_in(const vector<Submission>& s, int l, int r) {
        for (int i = l; i < r; ++i) if (s[i].status == "Accepted") return s[i].time;
        return -1;
    }
};

struct Team {
    string name;
    int id; // stable id
    int problem_count = 0;
    vector<ProblemState> ps; // size 26, but use first problem_count columns when printing

    // For ranking (visible metrics)
    int vis_solved = 0;
    long long vis_penalty = 0;
    vector<int> vis_times_desc; // solve times of visible solved problems, sorted desc

    // For queries
    vector<Submission> all_subs; // across all problems

    bool hasFrozenProblems() const {
        for (int p = 0; p < problem_count; ++p) {
            const auto &pr = ps[p];
            if (pr.eligibleFreeze) {
                int y = (int)pr.subs.size() - pr.freezeStartIdx;
                if (y > 0) return true;
            }
        }
        return false;
    }
};

struct CmpTeam {
    bool operator()(const Team* a, const Team* b) const {
        if (a->vis_solved != b->vis_solved) return a->vis_solved > b->vis_solved; // more solved first
        if (a->vis_penalty != b->vis_penalty) return a->vis_penalty < b->vis_penalty; // less penalty first
        // Compare solve times: max smaller is better; vectors stored desc, so compare lex on reversed order with smaller better
        const auto &A = a->vis_times_desc, &B = b->vis_times_desc;
        int n = (int)A.size();
        for (int i = 0; i < n; ++i) {
            if (A[i] != B[i]) return A[i] < B[i]; // smaller time better; A[i] and B[i] are descending sequences
        }
        if (a->name != b->name) return a->name < b->name; // lexicographic
        return a->id < b->id; // stable tie-breaker
    }
};

struct SystemState {
    bool started = false;
    bool frozen = false;
    int duration = 0;
    int problem_count = 0;
    long long seq = 0; // submission sequence counter

    unordered_map<string, unique_ptr<Team>> teamsByName;
    vector<Team*> teamsVec; // storage for ordering
    set<Team*, CmpTeam> ranking; // current visible ranking order

    // For last flushed ranking mapping
    vector<Team*> last_order; // from best to worst
    unordered_map<string, int> last_rank_of; // 1-based

    Team* getTeam(const string &name) {
        auto it = teamsByName.find(name);
        if (it == teamsByName.end()) return nullptr;
        return it->second.get();
    }

    void addTeam(const string &name) {
        if (started) { cout << "[Error]Add failed: competition has started.\n"; return; }
        if (teamsByName.count(name)) { cout << "[Error]Add failed: duplicated team name.\n"; return; }
        auto t = make_unique<Team>();
        t->name = name; t->id = (int)teamsByName.size();
        t->problem_count = problem_count > 0 ? problem_count : 26; // default capacity
        t->ps.assign(26, ProblemState());
        t->vis_solved = 0; t->vis_penalty = 0; t->vis_times_desc.clear();
        Team* tp = t.get();
        teamsByName[name] = move(t);
        teamsVec.push_back(tp);
        // Insert into ranking using lex fallback before first flush
        ranking.insert(tp);
        cout << "[Info]Add successfully.\n";
    }

    void startCompetition(int dur, int pcnt) {
        if (started) { cout << "[Error]Start failed: competition has started.\n"; return; }
        started = true; duration = dur; problem_count = pcnt;
        for (auto *t : teamsVec) t->problem_count = problem_count;
        cout << "[Info]Competition starts.\n";
    }

    static bool isWrong(const string &st) { return st != "Accepted"; }

    void submit(char probChar, const string &teamName, const string &status, int time) {
        Team* t = getTeam(teamName);
        int p = probChar - 'A';
        Submission s{p, status, time, seq++};
        t->all_subs.push_back(s);
        auto &pr = t->ps[p];
        pr.subs.push_back(s);
        if (!frozen) {
            // Update revealed state immediately if not in frozen period
            if (!pr.solved) {
                if (status == "Accepted") {
                    pr.solved = true;
                    // wrongs counted so far equals number of wrongs in entire subs up to this point
                    int wrongs = 0;
                    for (auto &x : pr.subs) { if (x.status == "Accepted") break; else ++wrongs; }
                    pr.wrong_before_ac = wrongs;
                    pr.first_ac_time = time;
                    // update visible metrics
                    ranking.erase(t);
                    t->vis_solved += 1;
                    t->vis_penalty += 20LL * pr.wrong_before_ac + pr.first_ac_time;
                    // insert time into desc vector maintaining descending
                    auto &vt = t->vis_times_desc;
                    auto it = lower_bound(vt.begin(), vt.end(), pr.first_ac_time, greater<int>());
                    vt.insert(it, pr.first_ac_time);
                    ranking.insert(t);
                }
            }
        } else {
            // in frozen period
            if (pr.eligibleFreeze) {
                // just accumulate; visibility unchanged
            } else {
                // problems solved before freeze still update nothing
            }
        }
    }

    void flushScoreboard() {
        // Recompute all visible metrics from persisted revealed state only
        for (auto *t : teamsVec) {
            ranking.erase(t);
            t->vis_solved = 0; t->vis_penalty = 0; t->vis_times_desc.clear();
            for (int p = 0; p < problem_count; ++p) {
                const auto &pr = t->ps[p];
                if (pr.solved) {
                    t->vis_solved++;
                    t->vis_penalty += 20LL * pr.wrong_before_ac + pr.first_ac_time;
                    t->vis_times_desc.push_back(pr.first_ac_time);
                }
            }
            sort(t->vis_times_desc.begin(), t->vis_times_desc.end(), greater<int>());
            ranking.insert(t);
        }
        // Build last_order and last_rank_of
        last_order.clear(); last_rank_of.clear();
        for (auto *t : ranking) last_order.push_back(t);
        for (int i = 0; i < (int)last_order.size(); ++i) last_rank_of[last_order[i]->name] = i + 1;
        cout << "[Info]Flush scoreboard.\n";
    }

    void freezeScoreboard() {
        if (frozen) { cout << "[Error]Freeze failed: scoreboard has been frozen.\n"; return; }
        frozen = true;
        // mark eligible problems for each team
        for (auto *t : teamsVec) {
            for (int p = 0; p < problem_count; ++p) {
                auto &pr = t->ps[p];
                if (pr.solved) { pr.eligibleFreeze = false; continue; }
                pr.eligibleFreeze = true;
                pr.freezeStartIdx = (int)pr.subs.size();
            }
        }
        cout << "[Info]Freeze scoreboard.\n";
    }

    // Helper: print a single team's scoreboard row with current visibility
    void printTeamRow(const Team* t, const unordered_map<string,int>& rankmap) const {
        int rankingNow = 0;
        auto it = rankmap.find(t->name);
        if (it != rankmap.end()) rankingNow = it->second; else {
            // before first flush: lex order ranking
            // compute lex order rank among all teams
            vector<string> names; names.reserve(teamsVec.size());
            for (auto *u : teamsVec) names.push_back(u->name);
            sort(names.begin(), names.end());
            rankingNow = (int)(lower_bound(names.begin(), names.end(), t->name) - names.begin()) + 1;
        }
        cout << t->name << ' ' << rankingNow << ' ' << t->vis_solved << ' ' << t->vis_penalty;
        for (int p = 0; p < t->problem_count; ++p) {
            const auto &pr = t->ps[p];
            cout << ' ';
            if (pr.solved) {
                if (pr.wrong_before_ac == 0) cout << '+'; else cout << '+' << pr.wrong_before_ac;
            } else if (frozen && pr.eligibleFreeze) {
                int x = ProblemState::count_wrongs_until_ac(pr.subs, 0, pr.freezeStartIdx);
                int y = (int)pr.subs.size() - pr.freezeStartIdx;
                if (y > 0) {
                    if (x == 0) cout << "0/" << y; else cout << '-' << x << '/' << y;
                } else {
                    if (x == 0) cout << '.'; else cout << '-' << x;
                }
            } else {
                // not frozen for this problem
                int x = ProblemState::count_wrongs_until_ac(pr.subs, 0, (int)pr.subs.size());
                if (x == 0) cout << '.'; else cout << '-' << x;
            }
        }
        cout << "\n";
    }

    void printScoreboard(const set<Team*, CmpTeam>& order, const unordered_map<string,int>& rankmap) const {
        for (auto *t : order) printTeamRow(t, rankmap);
    }

    void scrollScoreboard() {
        if (!frozen) { cout << "[Error]Scroll failed: scoreboard has not been frozen.\n"; return; }
        cout << "[Info]Scroll scoreboard.\n";
        // The scroll operation first flushes the scoreboard before proceeding
        // Build a temp flush (without outputting Info twice). We'll recompute last_order/ranks and current visible metrics
        // Recompute visible metrics from revealed state
        for (auto *t : teamsVec) {
            ranking.erase(t);
            t->vis_solved = 0; t->vis_penalty = 0; t->vis_times_desc.clear();
            for (int p = 0; p < problem_count; ++p) if (t->ps[p].solved) {
                t->vis_solved++;
                t->vis_penalty += 20LL * t->ps[p].wrong_before_ac + t->ps[p].first_ac_time;
                t->vis_times_desc.push_back(t->ps[p].first_ac_time);
            }
            sort(t->vis_times_desc.begin(), t->vis_times_desc.end(), greater<int>());
            ranking.insert(t);
        }
        last_order.clear(); last_rank_of.clear();
        for (auto *t : ranking) last_order.push_back(t);
        for (int i = 0; i < (int)last_order.size(); ++i) last_rank_of[last_order[i]->name] = i + 1;
        // Print scoreboard BEFORE scrolling
        printScoreboard(ranking, last_rank_of);

        // Process reveals until no team has frozen problems (y>0)
        // We'll repeatedly pick the lowest-ranked such team in current ranking
        while (true) {
            Team* chosen = nullptr; int chosenRank = -1;
            // scan from worst to best
            for (auto it = ranking.rbegin(); it != ranking.rend(); ++it) {
                Team* cand = *it;
                bool has = false; int minP = 26;
                for (int p = 0; p < problem_count; ++p) {
                    const auto &pr = cand->ps[p];
                    if (pr.eligibleFreeze) {
                        int y = (int)pr.subs.size() - pr.freezeStartIdx;
                        if (y > 0) { has = true; minP = min(minP, p); }
                    }
                }
                if (has) { chosen = cand; chosenRank = distance(ranking.begin(), ranking.find(cand)) + 1; break; }
            }
            if (!chosen) break; // no frozen problems remaining

            // Choose smallest letter problem with y>0
            int probToUnfreeze = -1; int y = 0;
            for (int p = 0; p < problem_count; ++p) {
                auto &pr = chosen->ps[p];
                if (pr.eligibleFreeze) {
                    int yp = (int)pr.subs.size() - pr.freezeStartIdx;
                    if (yp > 0) { probToUnfreeze = p; y = yp; break; }
                }
            }
            if (probToUnfreeze == -1) { // no y>0 although hasEligible? continue
                // mark all eligible with y=0 to not eligible (no effect)
                for (int p = 0; p < problem_count; ++p) if (chosen->ps[p].eligibleFreeze && ((int)chosen->ps[p].subs.size() - chosen->ps[p].freezeStartIdx) == 0) chosen->ps[p].eligibleFreeze = false;
                continue;
            }
            auto &pr = chosen->ps[probToUnfreeze];
            int x_before = ProblemState::count_wrongs_until_ac(pr.subs, 0, pr.freezeStartIdx);
            int wrongs_in_frozen_until_ac = ProblemState::count_wrongs_until_ac(pr.subs, pr.freezeStartIdx, (int)pr.subs.size());
            int first_ac_t = ProblemState::first_ac_time_in(pr.subs, pr.freezeStartIdx, (int)pr.subs.size());

            // Update chosen's visible metrics and persisted state according to reveal
            ranking.erase(chosen);
            int oldRank = chosenRank;
            if (first_ac_t != -1) {
                // solved now
                pr.solved = true;
                pr.first_ac_time = first_ac_t;
                pr.wrong_before_ac = x_before + wrongs_in_frozen_until_ac - 0; // wrongs before AC includes pre-freeze wrongs plus wrongs before first AC among frozen
                chosen->vis_solved += 1;
                chosen->vis_penalty += 20LL * pr.wrong_before_ac + pr.first_ac_time;
                auto &vt = chosen->vis_times_desc; auto it = lower_bound(vt.begin(), vt.end(), pr.first_ac_time, greater<int>()); vt.insert(it, pr.first_ac_time);
            } else {
                // still unsolved; nothing to add to visible metrics
            }
            // unfreeze this problem: clear eligible flag
            pr.eligibleFreeze = false;

            // Reinsert and compute new rank
            ranking.insert(chosen);
            int newRank = (int)distance(ranking.begin(), ranking.find(chosen)) + 1;
            if (newRank < oldRank) {
                // Print ranking change line
                // team2 = team that was at position newRank BEFORE the increase
                Team* team2 = last_order[newRank - 1];
                cout << chosen->name << ' ' << team2->name << ' ' << chosen->vis_solved << ' ' << chosen->vis_penalty << "\n";
            }
            // Update last_order to current order for next iteration selection
            last_order.clear(); last_rank_of.clear();
            for (auto *t : ranking) last_order.push_back(t);
            for (int i = 0; i < (int)last_order.size(); ++i) last_rank_of[last_order[i]->name] = i + 1;
        }

        // After scrolling ends, the frozen state will be lifted
        frozen = false;
        for (auto *t : teamsVec) {
            for (int p = 0; p < problem_count; ++p) t->ps[p].eligibleFreeze = false; // reset
        }
        // Print scoreboard AFTER scrolling
        printScoreboard(ranking, last_rank_of);
    }

    void queryRanking(const string &teamName) {
        Team* t = getTeam(teamName);
        if (!t) { cout << "[Error]Query ranking failed: cannot find the team.\n"; return; }
        cout << "[Info]Complete query ranking.\n";
        if (frozen) cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        int rk = 0;
        auto it = last_rank_of.find(teamName);
        if (it != last_rank_of.end()) rk = it->second;
        else {
            // before first flush: lex order
            vector<string> names; names.reserve(teamsVec.size());
            for (auto *u : teamsVec) names.push_back(u->name);
            sort(names.begin(), names.end());
            rk = (int)(lower_bound(names.begin(), names.end(), teamName) - names.begin()) + 1;
        }
        cout << teamName << " NOW AT RANKING " << rk << "\n";
    }

    void querySubmission(const string &teamName, const string &prob, const string &status) {
        Team* t = getTeam(teamName);
        if (!t) { cout << "[Error]Query submission failed: cannot find the team.\n"; return; }
        cout << "[Info]Complete query submission.\n";
        int wantP = -1; if (prob != "ALL") wantP = prob[0] - 'A';
        string wantS = status;
        long long bestSeq = -1; Submission best{}; bool found = false;
        // DEBUG: you can enable the following logs for troubleshooting
        // cerr << "DBG_WANT prob=" << prob << " status=" << status << " wantP=" << wantP << "\n";
        // cerr << "DBG_SUBS_COUNT " << t->all_subs.size() << "\n";
        for (const auto &s : t->all_subs) {
            // cerr << "DBG_SUB prob="<<char('A'+s.prob)<<" status="<<s.status<<" time="<<s.time<<" seq="<<s.seq<<"\n";
            if (wantP != -1 && s.prob != wantP) continue;
            if (wantS != "ALL" && s.status != wantS) continue;
            if (s.seq >= bestSeq) { bestSeq = s.seq; best = s; found = true; }
        }
        if (!found) { cout << "Cannot find any submission.\n"; return; }
        cout << t->name << ' ' << char('A' + best.prob) << ' ' << best.status << ' ' << best.time << "\n";
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    SystemState sys;
    string cmd;
    string line;
    while (true) {
        if (!getline(cin, line)) break;
        if (line.empty()) continue;
        stringstream ss(line);
        ss >> cmd;
        if (cmd == "ADDTEAM") {
            string name; ss >> name; sys.addTeam(name);
        } else if (cmd == "START") {
            string DURATION, PROBLEM; int dur, pcnt; ss >> DURATION >> dur >> PROBLEM >> pcnt; sys.startCompetition(dur, pcnt);
        } else if (cmd == "SUBMIT") {
            string prob, BY, team, WITH, status, AT; int time; ss >> prob >> BY >> team >> WITH >> status >> AT >> time;
            sys.submit(prob[0], team, status, time);
        } else if (cmd == "FLUSH") {
            sys.flushScoreboard();
        } else if (cmd == "FREEZE") {
            sys.freezeScoreboard();
        } else if (cmd == "SCROLL") {
            sys.scrollScoreboard();
        } else if (cmd == "QUERY_RANKING") {
            string name; ss >> name; sys.queryRanking(name);
        } else if (cmd == "QUERY_SUBMISSION") {
            // The format is: QUERY_SUBMISSION <team> WHERE PROBLEM=<p> AND STATUS=<s>
            // We'll parse from the original line to be robust about '='
            size_t pos1 = line.find(' '); // after cmd
            size_t posWhere = line.find(" WHERE ", pos1 + 1);
            string team = line.substr(pos1 + 1, posWhere - (pos1 + 1));
            string rest = line.substr(posWhere + 1); // starts with WHERE ...
            string prob = "ALL";
            string status = "ALL";
            size_t ppos = rest.find("PROBLEM=");
            if (ppos != string::npos) {
                size_t start = ppos + 8;
                size_t end = rest.find(' ', start);
                prob = rest.substr(start, (end==string::npos?rest.size():end) - start);
            }
            size_t spos = rest.find("STATUS=");
            if (spos != string::npos) {
                size_t start = spos + 7;
                size_t end = rest.find(' ', start);
                status = rest.substr(start, (end==string::npos?rest.size():end) - start);
            }
            sys.querySubmission(team, prob, status);
        } else if (cmd == "END") {
            cout << "[Info]Competition ends.\n"; break;
        }
    }
    return 0;
}

