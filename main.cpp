#include <cstring>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <ncurses.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <sys/types.h>
namespace fs = std::filesystem;

struct Process {
    std::string name;
    int pid = -1; //process id
    int ppid = -1; //parent process id
    int threads = 1;
    int ramUsage = -1; //VmRSS
    int swapUsage = -1; //VmSwap
    int numFileDescriptors = -1; //FDsize
    std::string state = "none"; //R/S/D/Z/T
    std::string processPath = "none";
    int uid = -1;
    std::string userName = "none";
};

enum class MemoryType {
    FREE, MAX
};

struct Statistics {
    long usedRam;
    int numProcesses;
    long maxAvailableRam;
    long freeRam;
    int running = 0;
    int zombie = 0;
    int sleeping = 0;
    int stopped = 0;
    int other = 0;
    int idle = 0;
    std::vector<std::string> users = {"ALL"};
};

//parses data from the line in /status
std::string parseData(const std::string &line, const std::string &searchString) {
    std::string str = line.substr(searchString.length(), line.length());
    for (int i = 0; i < str.length(); i++) {
        if (str[i] != ' ' && str[i] != '\t') {
            std::string substr = str.substr(i, str.length());
            return substr;
        }
    }
    return "";
}

//gets the process path from the PID
std::string getProcessPath(int pid) {
    std::ostringstream oss;
    oss << "/proc/" << pid << "/exe";
    std::vector<char> buf(1024);
    ssize_t len = readlink(oss.str().c_str(), buf.data(), buf.size() - 1);
    if (len != -1) {
        buf[len] = '\0';
        return std::string(buf.data());
    }
    return {};
}

//converts UID (from /status) to username
std::string uidToUsername(int uid) {
    passwd *pw = getpwuid(uid);
    if (pw) {
        return pw->pw_name;
    }
    return {};
}

//gets the data for the process by file path by searching /status file
Process getProcessData(const std::string &filePath) {
    Process process;
    std::string status = filePath + "/status";
    std::string nameFlag = "Name:";
    std::string pidFlag = "Pid:";
    std::string ppidFlag = "PPid:";
    std::string ramUsageFlag = "VmRSS:";
    std::string swapUsageFlag = "VmSwap:";
    std::string stateFlag = "State:";
    std::string fileDescriptorCountFlag = "FDSize:";
    std::string uidFlag = "Uid:";
    std::string line;

    std::ifstream file;
    file.open(status);
    if (!file) {
        std::cout << "file can not be opened" << std::endl;
        return {};
    }

    while (std::getline(file, line)) {

        if (line.substr(0, nameFlag.length()) == nameFlag) {
            process.name = parseData(line, nameFlag);
        }
        else if (line.substr(0, pidFlag.length()) == pidFlag) {
            process.pid = std::stoi(parseData(line, pidFlag));
        }
        else if (line.substr(0, ppidFlag.length()) == ppidFlag) {
            process.ppid = std::stoi(parseData(line, ppidFlag));
        }
        else if (line.substr(0, ramUsageFlag.length()) == ramUsageFlag) {
            process.ramUsage = std::stoi(parseData(line, ramUsageFlag));
        }
        else if (line.substr(0, swapUsageFlag.length()) == swapUsageFlag) {
            process.swapUsage = std::stoi(parseData(line, swapUsageFlag));
        }
        else if (line.substr(0, stateFlag.length()) == stateFlag) {
            process.state = parseData(line, stateFlag);
        }
        else if (line.substr(0, fileDescriptorCountFlag.length()) == fileDescriptorCountFlag) {
            process.numFileDescriptors = std::stoi(parseData(line, fileDescriptorCountFlag));
        }
        else if (line.substr(0, uidFlag.length()) == uidFlag) {
            process.uid = std::stoi(parseData(line, uidFlag));
        }
    }
    file.close();
    if (process.pid != -1) {
        process.processPath = getProcessPath(process.pid);
    }
    if (process.uid != -1) {
        process.userName = uidToUsername(process.uid);
    }
    return process;
}

//returns a vector of processes
std::vector<Process> getProcesses() {
    std::vector<Process> processes;
    //iterate through directories in /proc folder
    for (const auto &directory : fs::directory_iterator("/proc")) {

        //get only directories that start with a number
        if (fs::is_directory(directory) && directory.path().filename().string()[0] > '0' && directory.path().filename().string()[0] < '9') {
            //std::cout << directory.path() << std::endl;
            processes.push_back(getProcessData(directory.path().string()));
        }
    }
    return processes;
}

//debugging only, prints all processes
void printProcess(const Process &process) {
    printf("----------------\n[%d] Process name: %s \nState: %s\n", process.pid, process.name.c_str(), process.state.c_str());
    printf("Parent PID: %d\n", process.ppid);
    if (process.ramUsage < 1024) {
        printf("RAM usage: %dkB\n", process.ramUsage);
    }
    else {
        printf("RAM usage: %dMB\n", process.ramUsage/1024);
    }

    if (process.swapUsage < 1024) {
        printf("Swap usage: %dkB\n", process.swapUsage);
    }
    else {
        printf("Swap usage: %dMB\n", process.swapUsage / 1024);
    }
    printf("File descriptors: %d\n", process.numFileDescriptors);
    printf("----------------");
}

//gets the memory by type
long getMemory(MemoryType type) {
    std::string referenceKey;
    switch (type) {
        case MemoryType::MAX:
            referenceKey = "MemTotal:";
        break;
        case MemoryType::FREE:
            referenceKey = "MemAvailable:";
        break;
    }
    std::ifstream file("/proc/meminfo");
    std::string key;
    long value;
    std::string kB;
    while (file >> key >> value >> kB) {
        if (key == referenceKey) {
            return value;
        }
    }
    return -1;
}

//checks if the string is in the vector (helper for filterUnique)
bool isInVector(const std::string &item, const std::vector<std::string> &data) {
    for (const auto& it : data) {
        if (it == item) {
            return true;
        }
    }
    return false;
}

//filters unique strings in a vector (for users)
std::vector<std::string> filterUnique(std::vector<std::string> data) {
    std::vector<std::string> filtered;
    for (const auto& item : data) {
        if (!isInVector(item, filtered)) {
            filtered.push_back(item);
        }
    }
    return filtered;
}

//gets the statistics of all processes
Statistics getStatistics(const std::vector<Process> &processes) {
    Statistics stats{};
    int sum = 0;
    for (const auto &process : processes) {
        sum += process.ramUsage;
        switch (process.state[0]) {
            case 'S':
                stats.sleeping++;
            break;
            case 'Z':
                stats.zombie++;
            break;
            case 'R':
                stats.running++;
            break;
            case 'T':
                stats.stopped++;
            break;
            case 'I':
                stats.idle++;
            break;
            default:
                stats.other++;
        }
        stats.users.push_back(process.userName);
    }
    stats.users = filterUnique(stats.users);
    const long totalRam = getMemory(MemoryType::MAX);
    const long availableRam = getMemory(MemoryType::FREE);

    stats.numProcesses = processes.size();
    stats.maxAvailableRam = totalRam / 1024;
    stats.freeRam = availableRam / 1024;
    stats.usedRam = (totalRam - availableRam) / 1024;
    return stats;
}

//debugging only, prints to terminal
void printStatistics(const Statistics &stats) {
    printf("----------------\n");
    printf("Number of processes: %d\n", stats.numProcesses);
    printf("RAM usage: %ldMB/%ldMB (%ldMB free)\n", stats.usedRam, stats.maxAvailableRam, stats.freeRam);
    printf("Running: %d, Sleeping: %d, Stopped: %d, Zombie: %d, Idle: %d, Other: %d\n",
        stats.running, stats.sleeping, stats.stopped, stats.zombie, stats.idle, stats.other);
    printf("----------------\n");
}

//debugging only, prints the data in the terminal
void printInTerminal(const std::vector<Process> &processes, const Statistics &stats) {
    printStatistics(stats);
    for (const Process &process : processes) {
        printProcess(process);
    }
}

//filters the processes by status, helper function
void getFilteredProcessesByStatus(const std::vector<Process> &processes, std::vector<Process> &filteredProcesses, const char status) {
    for (const auto &proc : processes) {
        if (proc.state[0] == status) {
            filteredProcesses.push_back(proc);
        }
    }
}

//filters processes by username, includes ALL
std::vector<Process> filterProcessesUser(const std::vector<Process> &processes, const std::string &user) {
    if (user == "ALL") {
        return processes;
    }
    std::vector<Process> filteredProcesses;
    for (const auto &proc : processes) {
        if (proc.userName == user) {
            filteredProcesses.push_back(proc);
        }
    }
    return filteredProcesses;
}

//filters processes by status
std::vector<Process> filterProcessesStatus(const std::vector<Process> &processes, const std::string &currentMode) {
    std::vector<Process> fp = {};
    if (currentMode == "ALL") {
        fp = processes;
    }
    else if (currentMode == "RUNNING") {
        getFilteredProcessesByStatus(processes, fp, 'R');
    }
    else if (currentMode == "ZOMBIE") {
        getFilteredProcessesByStatus(processes, fp, 'Z');
    }
    else if (currentMode == "SLEEPING") {
        getFilteredProcessesByStatus(processes, fp, 'S');
    }
    else if (currentMode == "STOPPED") {
        getFilteredProcessesByStatus(processes, fp, 'T');
    }
    else if (currentMode == "IDLE") {
        getFilteredProcessesByStatus(processes, fp, 'I');
    }
    return fp;
}

//handles switching modes display on home screen
void printModes(std::vector<std::string> modes, int currentModeIndex) {
    init_pair(100, COLOR_RED, COLOR_BLACK);
    const chtype modeColor = COLOR_PAIR(100);
    for (int i = 0; i < modes.size(); i++) {
        if (i == currentModeIndex) {
            attron(A_BOLD);
            attron(modeColor);
        }
        printw("%s", modes[i].c_str());
        if (i == currentModeIndex) {
            attroff(A_BOLD);
            attroff(modeColor);
        }
        if (i != modes.size() - 1) {
            printw("/");
        }
    }
}

//TODO maybe use templates, floats go crazy, ram should have decimals
//helper for printing single file data lines, for color handling
void printLine(int y, int x, const std::string &text1, const chtype color1, const std::string &text2, const chtype color2, int &i) {
    attron(color1);
    attron(A_BOLD);
    mvprintw(y, x, "%s", text1.c_str());
    attroff(color1);
    attron(color2);
    printw("%s", text2.c_str());
    attroff(A_BOLD);
    attroff(color2);
    i++;
}

//prints the top quit instructions on single file display
void printQuitInstructions(const int y, const int x) {
    const chtype red = COLOR_PAIR(2);
    const chtype green = COLOR_PAIR(4);
    mvprintw(y, x, "Press ");
    attron(red);
    attron(A_BOLD);
    printw("[K]");
    attroff(A_BOLD);
    attron(green);
    printw(" to kill the process, press ");
    attron(red);
    attron(A_BOLD);
    printw("[Q]");
    attroff(A_BOLD);
    attron(green);
    printw(" to quit this page");
    attron(red);
}

//kill confirm screen for single file data display
bool killConfirmation(const int pid) {
    init_pair(22, COLOR_WHITE, COLOR_RED);
    clear();
    bkgd(COLOR_PAIR(22));
    attron(A_BOLD);
    std::string text1 = "Press [K] to kill this process";
    std::string text2 = "Press [T] to terminate this process";
    std::string text3 = "Press any other key to quit";
    mvprintw(LINES/2, COLS/2 - text1.length()/2, "%s", text1.c_str());
    mvprintw(LINES/2 + 1, COLS/2 - text2.length()/2, "%s", text2.c_str());
    mvprintw(LINES/2 + 2, COLS/2 - text3.length()/2, "%s", text3.c_str());
    attroff(A_BOLD);
    const char ch = getch();
    if (ch == 'k') {
        return kill(pid, SIGKILL) == 0;

    }
    if (ch == 't') {
        return kill(pid, SIGTERM) == 0;
    }
    return false;
}

//prints the border on the screen, offset moves it inside by x
void printBorder(const chtype color, const int offset = 0) {
    attron(color);
    mvhline(0 + offset, 0 + offset, ACS_HLINE, COLS - offset * 2);
    mvhline(LINES - 1 - offset, 0 + offset, ACS_HLINE, COLS - offset * 2);
    mvvline(0 + offset, 0 + offset, ACS_VLINE, LINES - offset * 2);
    mvvline(0 + offset, COLS - 1 - offset, ACS_VLINE, LINES - offset * 2);

    mvaddch(0 + offset, 0 + offset, ACS_ULCORNER);
    mvaddch(0 + offset, COLS - 1 - offset, ACS_URCORNER);
    mvaddch(LINES - 1 - offset, 0 + offset, ACS_LLCORNER);
    mvaddch(LINES - 1 - offset, COLS - 1 - offset, ACS_LRCORNER);
    attroff(color);
}

//only handles the printing of data of a single file
void printSingleProcessData(const Process& process, int line, int xOffset) {
    const chtype black = COLOR_PAIR(1);
    const chtype red = COLOR_PAIR(2);
    const chtype blue = COLOR_PAIR(3);
    const chtype green = COLOR_PAIR(4);
    clear();
    bkgd(green);

    const int currentRamUsage = process.ramUsage > 1024 ? static_cast<float>(process.ramUsage) / 1024 : static_cast<float>(process.ramUsage);
    const std::string postfixRam = process.ramUsage > 1024 ? " MB" : " kB";
    const int currentSwapUsage = process.swapUsage > 1024 ? static_cast<float>(process.swapUsage) / 1024 : static_cast<float>(process.swapUsage);
    const std::string postfixSwap = process.swapUsage > 1024 ? " MB" : " kB";

    printQuitInstructions(line, xOffset);
    line++;
    printLine(line, xOffset, "Name: ", black, process.name, red, line);
    printLine(line, xOffset, "PID: ", black, std::to_string(process.pid), red, line);
    printLine(line, xOffset, "State: ", black, process.state, red, line);
    printLine(line, xOffset, "RAM usage: ", black, std::to_string(currentRamUsage) + postfixRam, red, line);
    printLine(line, xOffset, "Process path: ", black, process.processPath, green, line);
    printLine(line, xOffset, "PPID: ", black, std::to_string(process.ppid), blue, line);
    printLine(line, xOffset, "Threads: ", black, std::to_string(process.threads), blue, line);
    printLine(line, xOffset, "Swap usage: ", black, std::to_string(currentSwapUsage) + postfixSwap, blue, line);
    printLine(line, xOffset, "Number of open file descriptors: ", black, std::to_string(process.numFileDescriptors), blue, line);
    printLine(line, xOffset, "UID: ", black, std::to_string(process.uid), blue, line);
    printLine(line, xOffset, "User: ", black, process.userName, red, line);

}

//makes the single process data screen, uses a shit ton of helpers
void displaySingleProcessData(const Process &process) {
    int line = 3;//starting line
    int xOffset = 4;
    const chtype red = COLOR_PAIR(2);

    const chtype borderColor = COLOR_PAIR(7);
    bool looping = true;
    while (looping) {
        printSingleProcessData(process, line, xOffset );
        printBorder(borderColor);
        printBorder(borderColor, 1);
        char ch = getch();
        switch (ch) {
            case 'q':
                looping = false;
            break;
            case 'k':
                looping = !killConfirmation(process.pid);
                break;
            default:
                break;
        }
    };
    attroff(COLOR_PAIR(2));
    bkgd(COLOR_PAIR(0));
}

//helper function for the printMainQuitInstructions, prints 1st in normal, 2nd in bold, takes colors
void printPair(std::string text1, std::string text2, chtype color1, chtype color2) {
    attron(color1);
    printw("%s", text1.c_str());
    attroff(color1);
    attron(A_BOLD);
    attron(color2);
    printw("%s", text2.c_str());
    attroff(A_BOLD);
    attroff(color2);
}

//quit instruction for home screen
void printMainQuitInstructions() {
    init_pair(20, COLOR_GREEN, COLOR_BLACK);
    chtype color = COLOR_PAIR(20);
    chtype normal = COLOR_PAIR(0);
    printPair("QUIT->", "[~]", normal, color);
    printPair(" MODE->", "[M]", normal, color);
    printPair(" SELECT->", "[NUMBER]", normal, color);
    printPair(" USER->", "[U]", normal, color);
    printPair(" SORT->", "[S]", normal, color);
}

//helper for sort, by RAM descending
bool compareProcessesByRAM(const Process &process1, const Process &process2) {
    return process1.ramUsage > process2.ramUsage;
}

//helper for sort, by name alphabetically
bool compareProcessesByName(const Process &process1, const Process &process2) {
    return process1.name < process2.name;
}

//helper for sort, by PID ascending
bool compareProcessesByPID(const Process &process1, const Process &process2) {
    return process1.pid < process2.pid;
}

//sorts by the current selected mode
void sortProcesses(std::vector<Process> &processes, std::string mode) {
    if (mode == "RAM usage") {
        std::sort(processes.begin(), processes.end(), compareProcessesByRAM);
    }
    if (mode == "Alphabet") {
        std::sort(processes.begin(), processes.end(), compareProcessesByName);
        return;
    }
    if (mode == "PID") {
        std::sort(processes.begin(), processes.end(), compareProcessesByPID);
        return;
    }
}

//displays the processes by lines on the home screen
void displayProcessesLines(const std::vector<Process> &filteredProcesses, int startline, int endline, int pageNum) {
    const chtype black = COLOR_PAIR(1);
    const chtype red = COLOR_PAIR(2);
    const chtype blue = COLOR_PAIR(3);
    const chtype green = COLOR_PAIR(4);

    attron(black);
    attron(A_BOLD);
    int numLines = 3;

    for (int i = 0; i < endline - startline; i++) {
        int currentIndex = (pageNum * (endline - startline)) + i;

        if (filteredProcesses.empty()) {
            attroff(black);
            attroff(A_BOLD);
            return;
        }
        if (currentIndex >= filteredProcesses.size()) {
            attroff(black);
            attroff(A_BOLD);
            return;
        }

        for (int j = 0; j < numLines - 1; j++) {
            mvprintw(startline + i*numLines + j, 0, "%-*s", COLS, " ");
        }
        const Process &currentProcess = filteredProcesses[currentIndex];
        //kb to mb for swap and ram usage
        float currentRamUsage = currentProcess.ramUsage > 1024 ? static_cast<float>(currentProcess.ramUsage) / 1024 : static_cast<float>(currentProcess.ramUsage);
        std::string postfixRam = currentProcess.ramUsage > 1024 ? "MB" : "kB";
        float currentSwapUsage = currentProcess.swapUsage > 1024 ? static_cast<float>(currentProcess.swapUsage) / 1024 : static_cast<float>(currentProcess.swapUsage);
        std::string postfixSwap = currentProcess.swapUsage > 1024 ? "MB" : "kB";

        //first line

        move(startline + i*numLines, 0);
        attron(red);
        printw("[%d] ", i+1);

        attron(black);
        printw("[%-6d] ", currentProcess.pid);
        attron(red);
        printw("%s", currentProcess.name.c_str());
        attron(black);

        //2nd line
        move(startline + i*numLines + 1, 0);
        attron(blue);
        printw(" Index: %-3d", currentIndex + 1);
        attron(black);
        printw(" State: ");
        attron(green);
        printw("%-15s", currentProcess.state.c_str());
        attron(black);
        printw("Usage: ");
        attron(green);
        printw("%-8.2f%-4s", currentRamUsage, postfixRam.c_str());
        attron(black);
        printw("Swap usage: ");
        attron(green);
        printw("%-6.2f%s", currentSwapUsage, postfixSwap.c_str());
    }
    attroff(A_BOLD);
    attroff(blue);
}

void displayRamUsageBar(const long maxRam, const long usedRam) {
    const std::string ramUsageString = "Ram usage:";
    printw("%s", ramUsageString.c_str());
    attron(A_BOLD);
    addch('[');
    attron(COLOR_PAIR(6));
    const int allSpaces = COLS * 2 / 3 - ramUsageString.length() - 2;//2 is for []
    const int usedSpaces = static_cast<int>(static_cast<float>(allSpaces) * (static_cast<float>(usedRam) / static_cast<float>(maxRam)));
    for (int i = 0; i < allSpaces; i++) {
        if (i > usedSpaces) {
            attron(COLOR_PAIR(8));
        }
        addch('|');
    }
    attroff(COLOR_PAIR(8));
    addch(']');
    attroff(COLOR_PAIR(6));
    addch('(');
    attron(COLOR_PAIR(6));
    printw("%.2fGB", static_cast<float>(usedRam) / 1024);
    attroff(COLOR_PAIR(6));
    addch('/');
    attron(COLOR_PAIR(8));
    printw("%.2fGB", static_cast<float>(maxRam) / 1024);
    attroff(COLOR_PAIR(8));
    addch(')');
    addch('\n');

    attroff(A_BOLD);
}

//displays the home screen
void displayData() {
    std::vector<Process> processes = getProcesses();
    const Statistics stats = getStatistics(processes);

    std::string pageText = "Page: ";
    int currentPage = 0;
    int startline = 9;
    std::vector<std::string> modes = {"ALL", "RUNNING", "SLEEPING", "IDLE", "ZOMBIE", "STOPPED"};
    std::vector<std::string> sortMode = {"RAM usage", "Alphabet", "PID"};
    int currentModeIndex = 0;
    int currentUserIndex = 0;
    int currentSortIndex = 0;

    while (true) {
        printMainQuitInstructions();
        //top row for stats
        mvprintw(1, 0, "Processes: %d, RAM usage: %ldMB/%ldMB (%ldMB free)\n",
            stats.numProcesses, stats.usedRam, stats.maxAvailableRam, stats.freeRam);
        displayRamUsageBar( stats.maxAvailableRam, stats.usedRam);

        printw("Running: %d, Sleeping: %d, Stopped: %d, Zombie: %d, Idle: %d, Other: %d\n",
            stats.running, stats.sleeping, stats.stopped, stats.zombie, stats.idle, stats.other);

        //Current filter
        std::vector<Process> filteredProcesses = filterProcessesStatus(processes, modes[currentModeIndex]);
        printw("Current filter: ");
        printModes(modes, currentModeIndex);
        printw("\nCurrent user: ");
        printModes(stats.users, currentUserIndex);
        printw("\nCurrent sort mode: ");
        printModes(sortMode, currentSortIndex);
        printw("\nFiltered ");
        attron(A_BOLD);
        attron(COLOR_PAIR(5));
        printw("[%lu]", filteredProcesses.size());
        attroff(A_BOLD);
        attroff(COLOR_PAIR(5));
        printw(" processes");

        filteredProcesses = filterProcessesUser(filteredProcesses, stats.users[currentUserIndex]);
        const int maxPageNum = static_cast<int>(filteredProcesses.size() / 10);
        const int currentAvailableProcesses = currentPage < maxPageNum ? 9 : filteredProcesses.size() % 9;
        sortProcesses(filteredProcesses, sortMode[currentSortIndex]);

        //page count on the right
        move(0, COLS - pageText.length() - 7);
        printw("%s%d/%d",pageText.c_str(), currentPage, maxPageNum);

        //display the processes
        displayProcessesLines(filteredProcesses, startline,  startline + 9, currentPage);

        noecho();
        refresh();
        char ch = getch();
        if (ch == '~') {
            break;
        }
        if (ch == 'e') {
            if (currentPage < maxPageNum) {
                currentPage ++;
            }
        }
        else if (ch == 'q') {
            if (currentPage > 0) {
                currentPage--;
            }
        }
        else if (ch == 'm') {
            currentModeIndex++;
            currentModeIndex = currentModeIndex % modes.size();
            currentPage = 0;
        }
        else if (ch == 's') {
            currentSortIndex++;
            currentSortIndex = currentSortIndex % sortMode.size();
            currentPage = 0;
        }
        else if (ch == 'u') {
            currentUserIndex++;
            currentUserIndex = currentUserIndex % stats.users.size();
            currentPage = 0;
        }
        else if (ch > '0' && ch <= '9') {
            int num = ch - '0';
            if (num <= currentAvailableProcesses) {
                Process proc = filteredProcesses[currentPage * 9 + num - 1];
                displaySingleProcessData(proc);
            }
        }
        else {
            processes = getProcesses();
            getStatistics(processes);
            std::sort(processes.begin(), processes.end(), compareProcessesByRAM);
            currentPage = 0;
            refresh();
        }
        clear();
    }
}

/*
 * To add:
 * Process start time
 * Process running time
 * Process CPU usage?
 * Search bar
 */

int main() {
    initscr();
    curs_set(0); //no cursor
    start_color();
    init_pair(0, COLOR_WHITE, COLOR_BLACK);
    init_pair(1, COLOR_BLACK, COLOR_WHITE);
    init_pair(2, COLOR_RED, COLOR_WHITE);
    init_pair(3, COLOR_BLUE, COLOR_WHITE);
    init_pair(4, COLOR_GREEN, COLOR_WHITE);
    init_pair(5, COLOR_GREEN, COLOR_BLACK);
    init_pair(6, COLOR_RED, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLUE);
    init_pair(8, COLOR_GREEN, COLOR_BLACK);
    nodelay(stdscr, TRUE);
    timeout(5000); //time to auto refresh

    displayData();

    endwin();
    return 0;
}