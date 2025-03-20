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

using namespace std;

vector<pid_t> background_pids;


void sigchld_handler(int sig) {
    int saved_errno = errno;
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        auto it = find(background_pids.begin(), background_pids.end(), pid);
        if (it != background_pids.end()) {
            background_pids.erase(it);
        }
    }
    errno = saved_errno;
}
void execute_command(vector<string> args, bool in_background);
void execute_pipeline(vector<vector<string>> &commands);
void redirect_io(vector<string>& args);
void handle_custom_command(vector<string> &args);
void fetchweather(const string &city) {
    string command = "curl -s wttr.in/" + city + "?format=3";
    system(command.c_str());
}

void notes_command(vector<string> &args) {
    if (args.size() < 2) {
        cout << "Usage: notes [add/view/delete] [note]" << endl;
        return;
    }
    string action = args[1];
    if (action == "add" && args.size() > 2) {
        ofstream notes("notes.txt", ios::app);
        notes << args[2] << endl;
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
    int seconds = stoi(args[1]);
    string message = args[2];
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
void redirect_io(vector<string> &args) {
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == ">") {
            int fd = open(args[i+1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args.erase(args.begin() + i, args.begin() + i + 2);
            i--;
        } else if (args[i] == ">>") {
            int fd = open(args[i+1].c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args.erase(args.begin() + i, args.begin() + i + 2);
            i--;
        } else if (args[i] == "<") {
            int fd = open(args[i+1].c_str(), O_RDONLY);
            dup2(fd, STDIN_FILENO);
            close(fd);
            args.erase(args.begin() + i, args.begin() + i + 2);
            i--;
        }
    }
}

void execute_command(vector<string> args, bool in_background) {
    if (args.empty()) return;

    if (args[0] == "cd") {
        if (args.size() > 1) chdir(args[1].c_str());
        else chdir(getenv("HOME"));
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
        redirect_io(args);
        vector<char*> cargs;
        for (auto &arg : args) cargs.push_back(&arg[0]);
        cargs.push_back(NULL);
        execvp(cargs[0], cargs.data());
        perror("Command failed");
        exit(1);
    } else if (!in_background) {
        waitpid(pid, NULL, 0);
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
            pipe(fd);
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
            redirect_io(commands[i]);
            vector<char*> cargs;
            for (auto &arg : commands[i]) cargs.push_back(&arg[0]);
            cargs.push_back(NULL);
            execvp(cargs[0], cargs.data());
            perror("Pipeline command failed");
            exit(1);
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
        waitpid(pid, NULL, 0);
    }
}
int main() {
    signal(SIGCHLD, sigchld_handler);
    string input;
    while (true) {
        cout << "termino> ";
        getline(cin, input);

        if (input.empty()) continue;
        stringstream ss(input);
        string token;
        vector<string> tokens;
        while (ss >> token) tokens.push_back(token);

        bool in_background = false;
        if (!tokens.empty() && tokens.back() == "&") {
            in_background = true;
            tokens.pop_back();
        }

        if (input.find("|") != string::npos) {
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
            execute_pipeline(piped_cmds);
        } else if (tokens[0] == "fetchweather" || tokens[0] == "notes" || tokens[0] == "remindme") {
            handle_custom_command(tokens);
        } else {
            execute_command(tokens, in_background);
        }
    }
    return 0;
}