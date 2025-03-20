# Termino 🖥️ — A Custom Linux Shell with Superpowers!

Welcome to **Termino**, a feature-rich custom Linux shell written in C++. This lightweight and interactive shell brings the power of command execution along with cool custom commands like weather updates, note-taking, reminders, background job handling, I/O redirection, and pipelines — all in one terminal interface!

---

## 🚀 Features

- ⚙️ **Built-in Linux Shell Commands**
  - `cd`, `exit`, `jobs`, and standard Linux commands with `execvp`.

- 🔀 **Pipelines**
  - Supports command piping using `|` (e.g., `ls | grep txt`).

- 🔁 **I/O Redirection**
  - Input (`<`), Output (`>`), and Append (`>>`) redirection support.

- ⏱️ **Background Process Execution**
  - Use `&` to run processes in the background (e.g., `sleep 10 &`).

- ☁️ **Custom Commands**
  - `fetchweather <city>` — Get real-time weather using `wttr.in`.
  - `notes add/view/delete` — Simple note-taking and management.
  - `remindme <seconds> <message>` — Schedule reminders via threading.

---

## 📂 Project Structure

termino/ ├── termino.cpp # Main source code └── notes.txt # Notes file 

---

## 🧠 How It Works

- **Command Parsing**: Tokenizes input, handles background flag `&`, identifies pipes `|`, and redirects.
- **Process Handling**: Uses `fork()`, `execvp()`, `waitpid()`, and `signal()` to manage execution.
- **Signal Handling**: Handles `SIGCHLD` to clean up zombie background processes.
- **Custom Utilities**: Implemented via `system()`, file streams, and `std::thread`.

---

## 💡 Example Usage

```bash
termino> ls -l
termino> sleep 5 &
termino> jobs
termino> echo "hello" > file.txt
termino> cat < file.txt
termino> ls | grep cpp
termino> fetchweather London
termino> notes add "Meeting at 5 PM"
termino> notes view
termino> remindme 10 "Time to stretch!"
