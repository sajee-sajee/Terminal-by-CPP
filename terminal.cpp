#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <csignal>
#include <fstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <stdexcept>

using namespace std;

vector<pid_t> background_pids;


// Reap only background children from normal control flow, never mutate vectors
// inside an asynchronous signal handler.
void reap_background_jobs() {
    background_pids.erase(remove_if(background_pids.begin(), background_pids.end(),
        [](pid_t pid) { return waitpid(pid, nullptr, WNOHANG) == pid; }), background_pids.end());
}

vector<string> tokenize(const string &input) {
    vector<string> tokens;
    string token;
    char quote = 0;
    bool escaped = false, started = false;
    auto flush = [&]() { if (started) tokens.push_back(token); token.clear(); started = false; };
    for (char c : input) {
        if (escaped) { token += c; started = true; escaped = false; continue; }
        if (c == '\\' && quote != '\'') { escaped = true; started = true; continue; }
        if (quote) { if (c == quote) quote = 0; else token += c; continue; }
        if (c == '\'' || c == '"') { quote = c; started = true; }
        else if (isspace(static_cast<unsigned char>(c))) flush();
        else {
            // Operators are recognized as whitespace-separated tokens only.
            token += c; started = true;
        }
    }
    if (quote || escaped) throw runtime_error("Unterminated quote or escape");
    flush();
    return tokens;
}
string join_message(const vector<string> &args, size_t start) {
    string result;
    for (size_t i = start; i < args.size(); ++i) { if (i > start) result += " "; result += args[i]; }
    return result;
}

void execute_command(vector<string> args, bool in_background);
void execute_pipeline(vector<vector<string>> &commands);
bool redirect_io(vector<string>& args);
void handle_custom_command(vector<string> &args);
void fetchweather(const string &city) {
    if (city.empty() || city.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != string::npos) {
        cerr << "Use letters, digits, hyphens or underscores for the city." << endl;
        return;
    }
    string url = "https://wttr.in/" + city + "?format=3";
    pid_t pid = fork();
    if (pid == 0) {
        execlp("curl", "curl", "-sS", "--max-time", "10", "--", url.c_str(), static_cast<char*>(nullptr));
        perror("curl"); _exit(1);
    }
    if (pid < 0) perror("fork");
    else while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
}

void notes_command(vector<string> &args) {
    if (args.size() < 2) {
        cout << "Usage: notes [add/view/delete] [note]" << endl;
        return;
    }
    string action = args[1];
    if (action == "add" && args.size() > 2) {
        ofstream notes("notes.txt", ios::app);
        notes << join_message(args, 2) << endl;
        cout << "Note added." << endl;
    } else if (action == "view") {
        ifstream notes("notes.txt");
        string line;
        while (getline(notes, line)) cout << line << endl;
    } else if (action == "delete") {
        ofstream notes("notes.txt", ios::trunc);
        cout << "All notes deleted." << endl;
    } else {
        cout << "Invalid notes command." << endl;
    }
}
void remindme_command(vector<string> &args) {
    if (args.size() < 3) {
        cout << "Usage: remindme [seconds] [message]" << endl;
        return;
    }
    int seconds;
    try {
        size_t consumed;
        seconds = stoi(args[1], &consumed);
        if (consumed != args[1].size() || seconds < 0 || seconds > 86400) throw invalid_argument("seconds");
    } catch (const exception &) { cerr << "Seconds must be an integer from 0 to 86400." << endl; return; }
    string message = join_message(args, 2);
    thread([seconds, message]() {
        this_thread::sleep_for(chrono::seconds(seconds));
        cout << "\n[Reminder] " << message << endl;
    }).detach();
}
void handle_custom_command(vector<string> &args) {
    if (args.empty()) return;
    if (args[0] == "fetchweather") {
        if (args.size() < 2) cout << "Usage: fetchweather [city]" << endl;
        else fetchweather(args[1]);
    } else if (args[0] == "notes") {
        notes_command(args);
    } else if (args[0] == "remindme") {
        remindme_command(args);
    }
}
bool redirect_io(vector<string> &args) {
    for (size_t i = 0; i < args.size();) {
        const string op = args[i];
        if (op != ">" && op != ">>" && op != "<") { ++i; continue; }
        if (i + 1 >= args.size() || args[i+1] == ">" || args[i+1] == ">>" || args[i+1] == "<") {
            cerr << "Redirection requires a filename." << endl; return false;
        }
        int flags = op == "<" ? O_RDONLY : O_WRONLY | O_CREAT | (op == ">>" ? O_APPEND : O_TRUNC);
        int fd = open(args[i+1].c_str(), flags, 0644);
        if (fd < 0) { perror("open"); return false; }
        if (dup2(fd, op == "<" ? STDIN_FILENO : STDOUT_FILENO) < 0) { perror("dup2"); close(fd); return false; }
        close(fd);
        args.erase(args.begin()+i, args.begin()+i+2);
    }
    if (args.empty()) { cerr << "Missing command." << endl; return false; }
    return true;
}

void execute_command(vector<string> args, bool in_background) {
    if (args.empty()) return;

    if (args[0] == "cd") {
        const char *directory = args.size() > 1 ? args[1].c_str() : getenv("HOME");
        if (!directory || chdir(directory) < 0) perror("cd");
        return;
    }

    if (args[0] == "exit") {
        exit(0);
    }

    if (args[0] == "jobs") {
        cout << "Running background jobs:" << endl;
        for (size_t i = 0; i < background_pids.size(); ++i) {
            cout << "[" << (i+1) << "] " << background_pids[i] << endl;
        }
        return;
    }
    pid_t pid = fork();
    if (pid == 0) {
        if (!redirect_io(args)) _exit(1);
        vector<char*> cargs;
        for (auto &arg : args) cargs.push_back(&arg[0]);
        cargs.push_back(NULL);
        execvp(cargs[0], cargs.data());
        perror("Command failed");
        exit(1);
    } else if (pid < 0) {
        perror("fork");
    } else if (!in_background) {
        while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
    } else {
        background_pids.push_back(pid);
        cout << "Process running in background: PID " << pid << endl;
    }
}
void execute_pipeline(vector<vector<string>> &commands) {
    vector<pid_t> pids;
    int in = 0;

    for (size_t i = 0; i < commands.size(); i++) {
        int fd[2];
        if (i < commands.size() - 1) {
            if (pipe(fd) < 0) { perror("pipe"); if (in != 0) close(in); break; }
        }
        pid_t pid = fork();
        if (pid == 0) {
            if (in != 0) {
                dup2(in, 0);
                close(in);
            }
            if (i < commands.size() - 1) {
                dup2(fd[1], 1);
                close(fd[1]);
            }
            if (i < commands.size() - 1) close(fd[0]);
            if (!redirect_io(commands[i])) _exit(1);
            vector<char*> cargs;
            for (auto &arg : commands[i]) cargs.push_back(&arg[0]);
            cargs.push_back(NULL);
            execvp(cargs[0], cargs.data());
            perror("Pipeline command failed");
            exit(1);
        }
        if (pid < 0) {
            perror("fork");
            if (in != 0) close(in);
            if (i < commands.size() - 1) { close(fd[0]); close(fd[1]); }
            break;
        }
        pids.push_back(pid);
        if (in != 0) close(in);
        if (i < commands.size() - 1) {
            in = fd[0];
            close(fd[1]);
        } else {
            in = 0;
        }
    }
    for (pid_t pid : pids) {
        while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
    }
}
int main() {

    string input;
    while (true) {
        reap_background_jobs();
        cout << "termino> " << flush;
        if (!getline(cin, input)) break;

        if (input.empty()) continue;
        vector<string> tokens;
        try { tokens = tokenize(input); }
        catch (const exception &error) { cerr << error.what() << endl; continue; }
        if (tokens.empty()) continue;

        bool in_background = false;
        if (!tokens.empty() && tokens.back() == "&") {
            in_background = true;
            tokens.pop_back();
        }

        if (tokens.empty()) { cerr << "Missing command." << endl; continue; }
        if (find(tokens.begin(), tokens.end(), "|") != tokens.end()) {
            if (in_background) { cerr << "Background pipelines are not supported." << endl; continue; }
            if (tokens.front() == "|" || tokens.back() == "|") { cerr << "Missing pipeline command." << endl; continue; }
            vector<vector<string>> piped_cmds;
            vector<string> temp;
            for (auto &tok : tokens) {
                if (tok == "|") {
                    piped_cmds.push_back(temp);
                    temp.clear();
                } else {
                    temp.push_back(tok);
                }
            }
            if (!temp.empty()) piped_cmds.push_back(temp);
            if (any_of(piped_cmds.begin(), piped_cmds.end(), [](const vector<string> &v) { return v.empty(); })) {
                cerr << "Missing pipeline command." << endl; continue;
            }
            execute_pipeline(piped_cmds);
        } else if (tokens[0] == "fetchweather" || tokens[0] == "notes" || tokens[0] == "remindme") {
            handle_custom_command(tokens);
        } else {
            execute_command(tokens, in_background);
        }
    }
    return 0;
}