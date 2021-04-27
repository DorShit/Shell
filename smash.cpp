#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char* argv[]) {
    if(signal(SIGTSTP , ctrlZHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    if(signal(SIGINT , ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }

    struct sigaction action;
    memset(&action, '\0',sizeof(action));
    action.sa_handler = alarmHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO|SA_RESTART;
    if(sigaction(SIGALRM , &action, NULL) < 0) {
        perror("smash error: sigaction failed");
        return 1;
    }

    SmallShell& smash = SmallShell::GetInstance();
    while(true) {
        std::cout << (smash.GetPrompt()+ " ");
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.ExecuteCommand(cmd_line.c_str(), false, false);
    }
    return 0;
}
