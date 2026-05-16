#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PROCESSES 50
#define MEMORY_SIZE 1024
#define MAX_RESOURCES 3
#define LOG_FILE "serc_log.txt"

typedef enum {
    STATE_NEW,
    STATE_READY,
    STATE_RUNNING,
    STATE_WAITING,
    STATE_SUSPENDED,
    STATE_TERMINATED
} ProcessState;

typedef struct {
    int pid;
    char name[50];
    int arrivalTime;
    int burstTime;
    int remainingTime;
    int priority;
    int memoryRequired;
    int allocatedStart;
    int allocatedSize;
    int waitingTime;
    int turnaroundTime;
    ProcessState state;
    int maxNeed[MAX_RESOURCES];
    int allocation[MAX_RESOURCES];
    int finished;
} PCB;

typedef struct {
    int start;
    int size;
    int free;
    int pid;
} MemoryBlock;

PCB processes[MAX_PROCESSES];
int processCount = 0;

MemoryBlock memoryBlocks[MAX_PROCESSES + 1];
int memoryBlockCount = 1;

int available[MAX_RESOURCES] = {3, 2, 2};
const char *resourceNames[MAX_RESOURCES] = {"CommChannel", "Vehicle", "MedicKit"};

const char *stateToString(ProcessState state) {
    switch (state) {
        case STATE_NEW: return "NEW";
        case STATE_READY: return "READY";
        case STATE_RUNNING: return "RUNNING";
        case STATE_WAITING: return "WAITING";
        case STATE_SUSPENDED: return "SUSPENDED";
        case STATE_TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}

void logEvent(const char *message) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) {
        printf("Warning: could not open log file.\n");
        return;
    }

    time_t now = time(NULL);
    struct tm *tmInfo = localtime(&now);
    fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
            tmInfo->tm_year + 1900, tmInfo->tm_mon + 1, tmInfo->tm_mday,
            tmInfo->tm_hour, tmInfo->tm_min, tmInfo->tm_sec, message);
    fclose(fp);
}

void initializeSystem() {
    memoryBlocks[0].start = 0;
    memoryBlocks[0].size = MEMORY_SIZE;
    memoryBlocks[0].free = 1;
    memoryBlocks[0].pid = -1;
    logEvent("Mini-OS initialized.");
}

void printHeader(const char *title) {
    printf("\n==================== %s ====================\n", title);
    printf("\n-----------------------  225 Assigment -----------------\n");
    printf("\n ");
}

int findProcessIndexByPid(int pid) {
    for (int i = 0; i < processCount; i++) {
        if (processes[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

void mergeFreeBlocks() {
    for (int i = 0; i < memoryBlockCount - 1; i++) {
        if (memoryBlocks[i].free && memoryBlocks[i + 1].free) {
            memoryBlocks[i].size += memoryBlocks[i + 1].size;
            for (int j = i + 1; j < memoryBlockCount - 1; j++) {
                memoryBlocks[j] = memoryBlocks[j + 1];
            }
            memoryBlockCount--;
            i--;
        }
    }
}

int allocateMemoryFirstFit(int pid, int size) {
    for (int i = 0; i < memoryBlockCount; i++) {
        if (memoryBlocks[i].free && memoryBlocks[i].size >= size) {
            int start = memoryBlocks[i].start;
            if (memoryBlocks[i].size == size) {
                memoryBlocks[i].free = 0;
                memoryBlocks[i].pid = pid;
            } else {
                for (int j = memoryBlockCount; j > i + 1; j--) {
                    memoryBlocks[j] = memoryBlocks[j - 1];
                }
                memoryBlocks[i + 1].start = memoryBlocks[i].start + size;
                memoryBlocks[i + 1].size = memoryBlocks[i].size - size;
                memoryBlocks[i + 1].free = 1;
                memoryBlocks[i + 1].pid = -1;

                memoryBlocks[i].size = size;
                memoryBlocks[i].free = 0;
                memoryBlocks[i].pid = pid;
                memoryBlockCount++;
            }
            return start;
        }
    }
    return -1;
}

void freeMemoryByPid(int pid) {
    for (int i = 0; i < memoryBlockCount; i++) {
        if (!memoryBlocks[i].free && memoryBlocks[i].pid == pid) {
            memoryBlocks[i].free = 1;
            memoryBlocks[i].pid = -1;
        }
    }
    mergeFreeBlocks();
}

void showMemoryStatus() {
    printHeader("MEMORY STATUS");
    int used = 0, freeMem = 0, fragments = 0;
    printf("%-10s %-10s %-12s %-10s\n", "Start", "Size", "Status", "PID");
    for (int i = 0; i < memoryBlockCount; i++) {
        printf("%-10d %-10d %-12s %-10d\n",
               memoryBlocks[i].start,
               memoryBlocks[i].size,
               memoryBlocks[i].free ? "FREE" : "ALLOCATED",
               memoryBlocks[i].pid);
        if (memoryBlocks[i].free) {
            freeMem += memoryBlocks[i].size;
            fragments++;
        } else {
            used += memoryBlocks[i].size;
        }
    }
    printf("\nTotal Memory: %d\nUsed Memory: %d\nFree Memory: %d\nExternal Fragments: %d\n",
           MEMORY_SIZE, used, freeMem, fragments);
}

int isSafeState(int n, int availableCopy[], int alloc[][MAX_RESOURCES], int need[][MAX_RESOURCES]) {
    int work[MAX_RESOURCES];
    int finish[MAX_PROCESSES] = {0};

    for (int i = 0; i < MAX_RESOURCES; i++) {
        work[i] = availableCopy[i];
    }

    int count = 0;
    while (count < n) {
        int found = 0;
        for (int i = 0; i < n; i++) {
            if (!finish[i]) {
                int canRun = 1;
                for (int j = 0; j < MAX_RESOURCES; j++) {
                    if (need[i][j] > work[j]) {
                        canRun = 0;
                        break;
                    }
                }
                if (canRun) {
                    for (int j = 0; j < MAX_RESOURCES; j++) {
                        work[j] += alloc[i][j];
                    }
                    finish[i] = 1;
                    found = 1;
                    count++;
                }
            }
        }
        if (!found) {
            return 0;
        }
    }
    return 1;
}

void showResourceStatus() {
    printHeader("RESOURCE STATUS");
    for (int i = 0; i < MAX_RESOURCES; i++) {
        printf("%s Available: %d\n", resourceNames[i], available[i]);
    }

    printf("\n%-5s %-20s %-12s %-12s %-12s\n", "PID", "Name", resourceNames[0], resourceNames[1], resourceNames[2]);
    for (int i = 0; i < processCount; i++) {
        printf("%-5d %-20s %-12d %-12d %-12d\n",
               processes[i].pid, processes[i].name,
               processes[i].allocation[0], processes[i].allocation[1], processes[i].allocation[2]);
    }
}

void createProcess() {
    if (processCount >= MAX_PROCESSES) {
        printf("Process table full.\n");
        return;
    }

    PCB p;
    memset(&p, 0, sizeof(PCB));

    p.pid = (processCount == 0) ? 1 : processes[processCount - 1].pid + 1;
    printf("Enter task name: ");
    scanf(" %49[^\n]", p.name);
    printf("Enter arrival time: ");
    scanf("%d", &p.arrivalTime);
    printf("Enter burst time: ");
    scanf("%d", &p.burstTime);
    printf("Enter priority (1 = highest emergency): ");
    scanf("%d", &p.priority);
    printf("Enter memory required: ");
    scanf("%d", &p.memoryRequired);

    for (int i = 0; i < MAX_RESOURCES; i++) {
        printf("Enter maximum need for %s: ", resourceNames[i]);
        scanf("%d", &p.maxNeed[i]);
        p.allocation[i] = 0;
    }

    p.remainingTime = p.burstTime;
    p.state = STATE_NEW;
    p.allocatedStart = -1;
    p.allocatedSize = 0;
    p.finished = 0;

    int start = allocateMemoryFirstFit(p.pid, p.memoryRequired);
    if (start == -1) {
        p.state = STATE_WAITING;
        printf("Not enough memory now. Process created in WAITING state.\n");
        logEvent("Process created but waiting due to insufficient memory.");
    } else {
        p.allocatedStart = start;
        p.allocatedSize = p.memoryRequired;
        p.state = STATE_READY;
        printf("Memory allocated from %d to %d\n", start, start + p.memoryRequired - 1);
        logEvent("Process created and memory allocated.");
    }

    processes[processCount++] = p;
    printf("Process created successfully. PID = %d\n", p.pid);
}

void suspendProcess() {
    int pid;
    printf("Enter PID to suspend: ");
    scanf("%d", &pid);
    int idx = findProcessIndexByPid(pid);
    if (idx == -1) {
        printf("PID not found.\n");
        return;
    }
    if (processes[idx].state == STATE_TERMINATED) {
        printf("Cannot suspend a terminated process.\n");
        return;
    }
    processes[idx].state = STATE_SUSPENDED;
    logEvent("Process suspended.");
    printf("Process %d suspended.\n", pid);
}

void resumeProcess() {
    int pid;
    printf("Enter PID to resume: ");
    scanf("%d", &pid);
    int idx = findProcessIndexByPid(pid);
    if (idx == -1) {
        printf("PID not found.\n");
        return;
    }
    if (processes[idx].state != STATE_SUSPENDED) {
        printf("Process is not suspended.\n");
        return;
    }
    processes[idx].state = STATE_READY;
    logEvent("Process resumed.");
    printf("Process %d resumed.\n", pid);
}

void terminateProcess() {
    int pid;
    printf("Enter PID to terminate: ");
    scanf("%d", &pid);
    int idx = findProcessIndexByPid(pid);
    if (idx == -1) {
        printf("PID not found.\n");
        return;
    }

    for (int i = 0; i < MAX_RESOURCES; i++) {
        available[i] += processes[idx].allocation[i];
        processes[idx].allocation[i] = 0;
    }
    freeMemoryByPid(pid);
    processes[idx].state = STATE_TERMINATED;
    processes[idx].remainingTime = 0;
    logEvent("Process terminated and resources released.");
    printf("Process %d terminated.\n", pid);
}

void showProcessTable() {
    printHeader("PROCESS TABLE");
    printf("%-5s %-20s %-8s %-8s %-8s %-10s %-12s %-10s\n",
           "PID", "Name", "Arr", "Burst", "Prio", "Memory", "State", "Remain");
    for (int i = 0; i < processCount; i++) {
        printf("%-5d %-20s %-8d %-8d %-8d %-10d %-12s %-10d\n",
               processes[i].pid,
               processes[i].name,
               processes[i].arrivalTime,
               processes[i].burstTime,
               processes[i].priority,
               processes[i].memoryRequired,
               stateToString(processes[i].state),
               processes[i].remainingTime);
    }
}

void resetSchedulingData() {
    for (int i = 0; i < processCount; i++) {
        processes[i].remainingTime = processes[i].burstTime;
        processes[i].waitingTime = 0;
        processes[i].turnaroundTime = 0;
        processes[i].finished = 0;
        if (processes[i].state != STATE_TERMINATED && processes[i].state != STATE_SUSPENDED && processes[i].allocatedStart != -1) {
            processes[i].state = STATE_READY;
        }
    }
}

void printSchedulingMetrics(const char *algo, int totalBusyTime, int totalEndTime) {
    double totalWait = 0, totalTurn = 0;
    int count = 0;
    printHeader(algo);
    printf("%-5s %-20s %-12s %-15s\n", "PID", "Name", "Waiting", "Turnaround");
    for (int i = 0; i < processCount; i++) {
        if (processes[i].allocatedStart != -1 && processes[i].state != STATE_SUSPENDED && processes[i].state != STATE_TERMINATED) {
            printf("%-5d %-20s %-12d %-15d\n",
                   processes[i].pid,
                   processes[i].name,
                   processes[i].waitingTime,
                   processes[i].turnaroundTime);
            totalWait += processes[i].waitingTime;
            totalTurn += processes[i].turnaroundTime;
            count++;
        }
    }
    if (count > 0 && totalEndTime > 0) {
        printf("\nAverage Waiting Time: %.2f\n", totalWait / count);
        printf("Average Turnaround Time: %.2f\n", totalTurn / count);
        printf("CPU Utilization: %.2f%%\n", (100.0 * totalBusyTime) / totalEndTime);
    }
}

void sortByArrival(PCB arr[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (arr[j].arrivalTime > arr[j + 1].arrivalTime) {
                PCB temp = arr[j];
                arr[j] = arr[j + 1];
                arr[j + 1] = temp;
            }
        }
    }
}

void runFCFS() {
    resetSchedulingData();
    int currentTime = 0, busyTime = 0;

    int idxs[MAX_PROCESSES], n = 0;
    for (int i = 0; i < processCount; i++) {
        if (processes[i].allocatedStart != -1 && processes[i].state != STATE_SUSPENDED && processes[i].state != STATE_TERMINATED) {
            idxs[n++] = i;
        }
    }

    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (processes[idxs[j]].arrivalTime > processes[idxs[j + 1]].arrivalTime) {
                int temp = idxs[j]; idxs[j] = idxs[j + 1]; idxs[j + 1] = temp;
            }
        }
    }

    for (int k = 0; k < n; k++) {
        PCB *p = &processes[idxs[k]];
        if (currentTime < p->arrivalTime) {
            currentTime = p->arrivalTime;
        }
        p->state = STATE_RUNNING;
        p->waitingTime = currentTime - p->arrivalTime;
        currentTime += p->burstTime;
        busyTime += p->burstTime;
        p->turnaroundTime = currentTime - p->arrivalTime;
        p->remainingTime = 0;
        p->state = STATE_READY;
    }

    printSchedulingMetrics("FCFS RESULTS", busyTime, currentTime);
    logEvent("FCFS scheduling executed.");
}

void runSJF() {
    resetSchedulingData();
    int currentTime = 0, completed = 0, busyTime = 0;

    while (completed < processCount) {
        int idx = -1;
        int shortest = 1000000000;
        for (int i = 0; i < processCount; i++) {
            if (processes[i].allocatedStart != -1 && processes[i].state != STATE_SUSPENDED &&
                processes[i].state != STATE_TERMINATED && !processes[i].finished &&
                processes[i].arrivalTime <= currentTime && processes[i].burstTime < shortest) {
                shortest = processes[i].burstTime;
                idx = i;
            }
        }

        if (idx == -1) {
            int pending = 0;
            for (int i = 0; i < processCount; i++) {
                if (processes[i].allocatedStart != -1 && processes[i].state != STATE_SUSPENDED &&
                    processes[i].state != STATE_TERMINATED && !processes[i].finished) {
                    pending = 1;
                    break;
                }
            }
            if (!pending) break;
            currentTime++;
            continue;
        }

        PCB *p = &processes[idx];
        p->state = STATE_RUNNING;
        p->waitingTime = currentTime - p->arrivalTime;
        currentTime += p->burstTime;
        busyTime += p->burstTime;
        p->turnaroundTime = currentTime - p->arrivalTime;
        p->remainingTime = 0;
        p->finished = 1;
        p->state = STATE_READY;
        completed++;
    }

    printSchedulingMetrics("SJF RESULTS", busyTime, currentTime);
    logEvent("SJF scheduling executed.");
}

void runPriorityScheduling() {
    resetSchedulingData();
    int currentTime = 0, completed = 0, busyTime = 0;

    while (completed < processCount) {
        int idx = -1;
        int bestPriority = 1000000000;
        for (int i = 0; i < processCount; i++) {
            if (processes[i].allocatedStart != -1 && processes[i].state != STATE_SUSPENDED &&
                processes[i].state != STATE_TERMINATED && !processes[i].finished &&
                processes[i].arrivalTime <= currentTime && processes[i].priority < bestPriority) {
                bestPriority = processes[i].priority;
                idx = i;
            }
        }

        if (idx == -1) {
            int pending = 0;
            for (int i = 0; i < processCount; i++) {
                if (processes[i].allocatedStart != -1 && processes[i].state != STATE_SUSPENDED &&
                    processes[i].state != STATE_TERMINATED && !processes[i].finished) {
                    pending = 1;
                    break;
                }
            }
            if (!pending) break;
            currentTime++;
            continue;
        }

        PCB *p = &processes[idx];
        p->state = STATE_RUNNING;
        p->waitingTime = currentTime - p->arrivalTime;
        currentTime += p->burstTime;
        busyTime += p->burstTime;
        p->turnaroundTime = currentTime - p->arrivalTime;
        p->remainingTime = 0;
        p->finished = 1;
        p->state = STATE_READY;
        completed++;
    }

    printSchedulingMetrics("PRIORITY RESULTS", busyTime, currentTime);
    logEvent("Priority scheduling executed.");
}

void runRoundRobin() {
    int quantum;
    printf("Enter time quantum: ");
    scanf("%d", &quantum);
    if (quantum <= 0) {
        printf("Invalid quantum.\n");
        return;
    }

    resetSchedulingData();
    int currentTime = 0, busyTime = 0, done;

    do {
        done = 1;
        for (int i = 0; i < processCount; i++) {
            if (processes[i].allocatedStart == -1 || processes[i].state == STATE_SUSPENDED || processes[i].state == STATE_TERMINATED) {
                continue;
            }
            if (processes[i].remainingTime > 0 && processes[i].arrivalTime <= currentTime) {
                done = 0;
                processes[i].state = STATE_RUNNING;
                int slice = (processes[i].remainingTime > quantum) ? quantum : processes[i].remainingTime;
                currentTime += slice;
                busyTime += slice;
                processes[i].remainingTime -= slice;
                if (processes[i].remainingTime == 0) {
                    processes[i].turnaroundTime = currentTime - processes[i].arrivalTime;
                    processes[i].waitingTime = processes[i].turnaroundTime - processes[i].burstTime;
                    processes[i].state = STATE_READY;
                } else {
                    processes[i].state = STATE_READY;
                }
            }
        }

        int pendingFuture = 0;
        for (int i = 0; i < processCount; i++) {
            if (processes[i].allocatedStart != -1 && processes[i].state != STATE_SUSPENDED &&
                processes[i].state != STATE_TERMINATED && processes[i].remainingTime > 0) {
                if (processes[i].arrivalTime > currentTime) {
                    pendingFuture = 1;
                }
                done = 0;
            }
        }
        if (!done && pendingFuture) {
            currentTime++;
        }
    } while (!done);

    printSchedulingMetrics("ROUND ROBIN RESULTS", busyTime, currentTime);
    logEvent("Round Robin scheduling executed.");
}

void schedulingMenu() {
    int choice;
    printHeader("CPU SCHEDULING");
    printf("1. FCFS\n");
    printf("2. SJF\n");
    printf("3. Priority Scheduling\n");
    printf("4. Round Robin\n");
    printf("Select: ");
    scanf("%d", &choice);

    switch (choice) {
        case 1: runFCFS(); break;
        case 2: runSJF(); break;
        case 3: runPriorityScheduling(); break;
        case 4: runRoundRobin(); break;
        default: printf("Invalid option.\n");
    }
}

void requestResources() {
    int pid;
    printf("Enter PID requesting resources: ");
    scanf("%d", &pid);
    int idx = findProcessIndexByPid(pid);
    if (idx == -1) {
        printf("PID not found.\n");
        return;
    }

    int request[MAX_RESOURCES];
    int need[MAX_RESOURCES];
    for (int i = 0; i < MAX_RESOURCES; i++) {
        need[i] = processes[idx].maxNeed[i] - processes[idx].allocation[i];
        printf("Enter requested units for %s: ", resourceNames[i]);
        scanf("%d", &request[i]);
        if (request[i] > need[i]) {
            printf("Request exceeds maximum remaining need.\n");
            return;
        }
        if (request[i] > available[i]) {
            printf("Not enough available resources right now.\n");
            return;
        }
    }

    int alloc[MAX_PROCESSES][MAX_RESOURCES];
    int needMatrix[MAX_PROCESSES][MAX_RESOURCES];
    int availCopy[MAX_RESOURCES];

    for (int i = 0; i < MAX_RESOURCES; i++) {
        availCopy[i] = available[i] - request[i];
    }

    for (int i = 0; i < processCount; i++) {
        for (int j = 0; j < MAX_RESOURCES; j++) {
            alloc[i][j] = processes[i].allocation[j];
            needMatrix[i][j] = processes[i].maxNeed[j] - processes[i].allocation[j];
        }
    }

    for (int i = 0; i < MAX_RESOURCES; i++) {
        alloc[idx][i] += request[i];
        needMatrix[idx][i] -= request[i];
    }

    if (isSafeState(processCount, availCopy, alloc, needMatrix)) {
        for (int i = 0; i < MAX_RESOURCES; i++) {
            available[i] -= request[i];
            processes[idx].allocation[i] += request[i];
        }
        processes[idx].state = STATE_READY;
        printf("Request granted. Safe state maintained.\n");
        logEvent("Resource request granted using Banker's Algorithm.");
    } else {
        printf("Request denied to avoid unsafe state / deadlock.\n");
        logEvent("Resource request denied to avoid unsafe state.");
    }
}

void releaseResources() {
    int pid;
    printf("Enter PID releasing resources: ");
    scanf("%d", &pid);
    int idx = findProcessIndexByPid(pid);
    if (idx == -1) {
        printf("PID not found.\n");
        return;
    }

    for (int i = 0; i < MAX_RESOURCES; i++) {
        int units;
        printf("Enter units to release for %s: ", resourceNames[i]);
        scanf("%d", &units);
        if (units < 0 || units > processes[idx].allocation[i]) {
            printf("Invalid release amount for %s.\n", resourceNames[i]);
            return;
        }
        processes[idx].allocation[i] -= units;
        available[i] += units;
    }
    printf("Resources released successfully.\n");
    logEvent("Resources released by process.");
}

void detectDeadlockRisk() {
    int alloc[MAX_PROCESSES][MAX_RESOURCES];
    int needMatrix[MAX_PROCESSES][MAX_RESOURCES];
    int availCopy[MAX_RESOURCES];

    for (int i = 0; i < MAX_RESOURCES; i++) {
        availCopy[i] = available[i];
    }
    for (int i = 0; i < processCount; i++) {
        for (int j = 0; j < MAX_RESOURCES; j++) {
            alloc[i][j] = processes[i].allocation[j];
            needMatrix[i][j] = processes[i].maxNeed[j] - processes[i].allocation[j];
        }
    }

    if (isSafeState(processCount, availCopy, alloc, needMatrix)) {
        printf("System is currently in a SAFE state.\n");
        logEvent("Deadlock check: SAFE state.");
    } else {
        printf("Warning: system is in an UNSAFE state. Deadlock risk detected.\n");
        logEvent("Deadlock check: UNSAFE state detected.");
    }
}

void viewLogFile() {
    FILE *fp = fopen(LOG_FILE, "r");
    if (!fp) {
        printf("No log file found yet.\n");
        return;
    }
    printHeader("SYSTEM LOG");
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);
    }
    fclose(fp);
}

void showMenu() {
    printHeader("SERC MINI-OS MENU");
    printf("\t 1. Create emergency task\n");
    printf("\t 2. Suspend task\n");
    printf("\t 3. Resume task\n");
    printf("\t 4. Terminate task\n");
    printf("\t 5. Show process table\n");
    printf("\t 6. CPU scheduling\n");
    printf("\t 7. Show memory usage\n");
    printf("\t 8. Request resources (Banker's Algorithm)\n");
    printf("\t 9. Release resources\n");
    printf("\t 10. Show resource allocation\n");
    printf("\t 11. Detect deadlock risk\n");
    printf("\t 12. View log file\n");
    printf("\t 0. Exit\n");
    printf("\t Choose an option: ");
}

int main() {
    int choice;
    initializeSystem();

    do {
        showMenu();
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input. Exiting.\n");
            break;
        }

        switch (choice) {
            case 1: createProcess(); break;
            case 2: suspendProcess(); break;
            case 3: resumeProcess(); break;
            case 4: terminateProcess(); break;
            case 5: showProcessTable(); break;
            case 6: schedulingMenu(); break;
            case 7: showMemoryStatus(); break;
            case 8: requestResources(); break;
            case 9: releaseResources(); break;
            case 10: showResourceStatus(); break;
            case 11: detectDeadlockRisk(); break;
            case 12: viewLogFile(); break;
            case 0:
                logEvent("System exited by user.");
                printf("Exiting Mini-OS.\n");
                break;
            default:
                printf("Invalid choice. Try again.\n");
        }
    } while (choice != 0);

    return 0;
}
