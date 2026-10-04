# Termino — a small C++ Linux shell

A learning project exploring POSIX processes, command execution, pipelines, file redirection and background jobs.

## Build and run

Requires Linux, a C++17 compiler and pthreads. `curl` is optional for weather requests.

```bash
git clone https://github.com/sajee-sajee/Terminal-by-CPP.git
cd Terminal-by-CPP
g++ -std=c++17 -Wall -Wextra -Wpedantic -pthread terminal.cpp -o termino
./termino
```

## Features and demonstration

```text
pwd
cd /tmp
echo hello | tr a-z A-Z
echo first > example.txt
echo second >> example.txt
cat < example.txt
sleep 5 &
jobs
notes add "Meeting at 5 PM"
notes view
remindme 3 "Take a break"
fetchweather London
exit
```

The notes file is written in the current working directory. `notes delete` clears that file. Reminders run only while the shell process remains alive.

## Tests

```bash
python3 -m unittest discover -s tests -v
```

Tests compile the actual shell and cover malformed commands, EOF, quoted and multiword notes, reminders, pipelines and redirection. Temporary test files are isolated from your working directory.

## Design

- `fork` and `execvp` start external programs.
- Pipes and `dup2` connect pipeline stages and redirect files.
- Background children are reaped in normal control flow, avoiding unsafe vector modification inside a signal handler.
- Quoted arguments and escaped characters are tokenized before dispatch.
- Weather uses a direct `curl` invocation rather than interpolating a shell command.

## Current limitations

This is not a replacement for Bash. Operators require surrounding whitespace; quoted literal operator tokens are not supported. There is no globbing, environment-variable expansion, shell scripting, terminal job control or background pipelines. Built-ins do not support redirection or pipeline execution. Background child cleanup happens when the prompt loop advances. `notes delete` deletes all saved notes, not individual entries. Weather accepts only letters, digits, hyphens and underscores in a city name.
