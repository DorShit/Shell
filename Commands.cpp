#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <fcntl.h>
#include <cstdlib>
#include <linux/limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cerr << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cerr << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define DEBUG_PRINT cerr << "DEBUG: "

#define EXEC(path, arg) \
  execvp((path), (arg));


bool IsBackgroundCommand(string cmd_line);
bool IsTimeoutCommand(const char *cmd_line);


string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

bool IsStringNumber(const string &str) {
    string::const_iterator it = str.begin();
    if(*it == '-'){
        ++it;
    }
    while (it != str.end() && isdigit(*it)) {
        ++it;
    }
    return !str.empty() && it == str.end();
}

bool IsRedirectionCommand(const char *cmd_line) {
    string str = cmd_line;
    return str.find(">") != FIND_FAIL;
}

void RemoveRedirectionSign(char *cmd_line) {
    const string str(cmd_line);
    size_t pos = str.find(">");
    if (pos != FIND_FAIL) {
        cmd_line[pos] = 0;
    }
}

bool IsPipeCommand(const char *cmd_line) {
    string cmd_str = cmd_line;
    return cmd_str.find("|") != FIND_FAIL;
}

void RemovePipeSign(char *cmd_line) {
    const string str(cmd_line);
    size_t pos = str.find("|");
    if (pos != FIND_FAIL) {
        cmd_line[pos] = 0;
    }
}

int ParseCommandLine(string cmd_line, vector<string> &args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s;) {
       args.push_back(s);
        ++i;
  }
  return i;

  FUNC_EXIT()
}

bool IsBuiltInCommand(string cmd_line) {
    vector<string> args = vector<string>();
    ParseCommandLine(cmd_line.c_str(), args);
    vector<string> built_in_commands = {"chprompt", "showpid", "pwd", "cd", "jobs", "kill", "fg", "bg", "quit"};
    return find(built_in_commands.begin(), built_in_commands.end(), args.at(0)) != built_in_commands.end();
}

bool IsTimeoutCommand(const char *cmd_line) {
    vector<string> args = vector<string>();
    ParseCommandLine(cmd_line, args);
    return args.size() > 0 && args[0] == "timeout";
}

void RemoveTimeoutSign(char *cmd_line) {
    vector<string> args = vector<string>();
    ParseCommandLine(cmd_line, args);
    const string str = cmd_line;
    size_t pos = str.find(args[1]) + args[1].size();
    strcpy(cmd_line, str.substr(pos).c_str());
}

bool IsBackgroundCommand(string cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}


void RemoveBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  if (idx == string::npos) {
    return;
  }
  if (cmd_line[idx] != '&') {
    return;
  }
  cmd_line[idx] = ' ';
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

string RemoveBackgroundSign(string str){
    char *cstr = new char[str.length() + 1];
    strcpy(cstr, str.c_str());
    RemoveBackgroundSign(cstr);
    return cstr;
}

string GetFirstStringInCmdLine(const char *cmd_line) {
    char* new_cmd = new char[strlen(cmd_line) + 1];
    strcpy(new_cmd, cmd_line);
    if(IsBackgroundCommand(new_cmd)) {
        RemoveBackgroundSign(new_cmd);
    }
    vector<string> args;
    if(ParseCommandLine(new_cmd, args) == 0){
        delete new_cmd;
        return "";
    }
    delete new_cmd;
    return (args.at(0));
}


SmallShell &global_smash = SmallShell::GetInstance();

JobsList::JobEntry::JobEntry(int id, JobState job_state, Command *cmd, pid_t pid, bool is_timeout) : job_id(id),
job_state(job_state), cmd(cmd), pid(pid), time_out(is_timeout) {
    time(&add_time);
    time(&time_stamp);
    if(time_out) {
        duration = stoi(cmd->GetArgs()->at(1));
    }
}


bool JobsCmpSmallerId(const JobsList::JobEntry *job_1, const JobsList::JobEntry *job_2) {
    return (job_1->GetJobId() < job_2->GetJobId());
}

bool JobsCmpBiggerId(const JobsList::JobEntry *job_1, const JobsList::JobEntry *job_2) {
    return (job_1->GetJobId() > job_2->GetJobId());
}

bool JobsCmpClosestTime(const JobsList::JobEntry *job_1, const JobsList::JobEntry *job_2) {
    return (job_1->GetDifferentTime() < job_2->GetDifferentTime());
}

JobsList::JobsList() {
    jobs_list = list<JobEntry *>();
}

JobsList::~JobsList() {
    for (list<JobEntry *>::iterator it = jobs_list.begin(); it != jobs_list.end(); ++it) {
        JobsList::JobEntry *job = *it;
        it = jobs_list.erase(it);
        delete job;
    }
}

int JobsList::GetSize() {
    return jobs_list.size();
}

void JobsList::AddJob(Command *cmd, pid_t pid, JobState state, bool is_timeout) {
    RemoveFinishedJobs();
    JobEntry *new_job = new JobEntry(GetMaxJobId() + 1, state, cmd, pid, is_timeout);
    this->jobs_list.push_back(new_job);
}

void JobsList::AddJob(JobEntry *job, JobState state, bool to_give_id) {
    RemoveFinishedJobs();
    job->SetState(state);
    job->ResetTimeAdded();
    if (to_give_id) {
        job->SetJobId(GetMaxJobId() + 1);
    }
    this->jobs_list.push_back(job);
}

void JobsList::RemoveFinishedJobs() {
    if(!global_smash.IsSmashPid(getpid())) {
        return;
    }
    for (list<JobEntry *>::iterator it = jobs_list.begin(); it != jobs_list.end(); ++it) {
        pid_t pid = waitpid((*it)->GetJobPid(), NULL, WNOHANG);
        if (pid > 0) {
            JobsList::JobEntry *job = *it;
            it = jobs_list.erase(it);
            delete job;
        }
        if (pid < 0) {
            perror("smash error: waitpid failed");
        }
    }
}

void JobsList::KillAllJobs() {
    for (list<JobEntry *>::iterator it = jobs_list.begin(); it != jobs_list.end(); ++it) {

        if (killpg(getpgid((*it)->GetJobPid()), SIGKILL) != 0) {
            perror("smash error: kill failed");
        }
    }
}

JobsList::JobEntry *JobsList::RemoveJobByJobId(int job_id) {
    pid_t pid = GetJobPidByJobId(job_id);
    if (pid == FAIL) {
        return nullptr;
    }
    return RemoveJobByPid(pid);
}

JobsList::JobEntry *JobsList::RemoveJobByPid(pid_t pid) {
    for (list<JobEntry *>::iterator it = jobs_list.begin(); it != jobs_list.end(); ++it) {
        if ((*it)->GetJobPid() == pid) {
            JobsList::JobEntry *job = *it;
            it = jobs_list.erase(it);
            return job;
        }
    }
    return nullptr;
}

void JobsList::PrintJobsList() {
    RemoveFinishedJobs();
    jobs_list.sort(JobsCmpSmallerId);
    for (list<JobEntry *>::iterator it = jobs_list.begin(); it != jobs_list.end(); ++it) {
        cout << "[" << (*it)->GetJobId() << "] " << (*it)->GetCommand()->GetCmdLine()
             << " : " << (*it)->GetJobPid() << " " << difftime(time(NULL), (*it)->GetTimeThatAdded()) << " secs";
        if ((*it)->GetState() == Stopped) {
            cout << " (stopped)" << endl;
        } else {
            cout << endl;
        }
    }
}

JobsList::JobEntry *JobsList::GetJobById(int job_id) {
    list<JobEntry *>::iterator it;
    for (it = jobs_list.begin(); it != jobs_list.end(); ++it) {
        if ((*it)->GetJobId() == job_id) {
            return *it;
        }
    }
    return nullptr;
}

int JobsList::GetMaxJobId() {
    if (jobs_list.empty()) {
        return 0;
    }
    else {
        jobs_list.sort(JobsCmpBiggerId);
        return jobs_list.front()->GetJobId();
    }
}

int JobsList::GetMaxStoppedJobId() {
    if (jobs_list.empty()) {
        return 0;
    }
    else {
        jobs_list.sort(JobsCmpBiggerId);
        for (list<JobEntry *>::iterator it = jobs_list.begin(); it != jobs_list.end(); ++it) {
            if ((*it)->GetState() == Stopped) {
                return (*it)->GetJobId();
            }
        }
    }
    return 0;
}

JobsList::JobEntry *JobsList::GetTimeoutJobToKill() {
    if (jobs_list.empty()) {
        return nullptr;
    }
    else {
        for (list<JobEntry *>::iterator it = jobs_list.begin(); it != jobs_list.end(); ++it) {
            if((*it)->IsTimeOut() == true && (*it)->GetDifferentTime() == 0){
                return *it;
            }
        }
    }
    return nullptr;
}

int JobsList::GetClosestTimeout() {
    if (jobs_list.empty()) {
        return FAIL;
    }
    jobs_list.sort(JobsCmpClosestTime);
    for (list<JobEntry *>::iterator it = jobs_list.begin(); it != jobs_list.end(); ++it) {
        if ((*it)->IsTimeOut()) {
            return (*it)->GetDifferentTime();
        }
    }
    return FAIL;
}

pid_t JobsList::GetJobPidByJobId(int job_id) {
    list<JobEntry *>::iterator it;
    for (it = jobs_list.begin(); it != jobs_list.end(); ++it) {
        if ((*it)->GetJobId() == job_id) {
            return (*it)->GetJobPid();
        }
    }
    return FAIL;
}

bool JobsList::JobIdExists(int job_id) {
    pid_t pid = GetJobPidByJobId(job_id);
    if (pid == FAIL) {
        return false;
    }
    return true;
}

bool JobsList::JobPidExists(int job_pid) {
    for (list<JobEntry *>::iterator it = jobs_list.begin(); it != jobs_list.end(); ++it) {
        if ((*it)->GetJobPid() == job_pid) {
            return true;
        }
    }
    return false;
}

bool JobsList::IsEmpty() {
    return jobs_list.empty();
}


// TODO: Add your implementation for classes in Commands.h

SmallShell::SmallShell() : prompt("smash"), last_dir(nullptr), fore_ground_job(nullptr) ,smash_pid(getpid()){
    jobs_list = JobsList();
}

SmallShell::~SmallShell() {
    if (last_dir != nullptr) {
        delete[] last_dir;
    }
}

Command * SmallShell::CreateCommand(const char *cmd_line, bool is_special, bool is_piped, bool is_timeout) {
    char* cmd = new char[strlen(cmd_line) + 1];
    strcpy(cmd, cmd_line);
    string first_word = GetFirstStringInCmdLine(cmd_line);
    if (first_word.compare("") == 0) {
        return nullptr;
    }
    if (!is_special && IsRedirectionCommand(cmd_line)) {
        return new RedirectionCommand(cmd);
    }
    if (IsPipeCommand(cmd_line)) {
        return new PipeCommand(cmd);
    }
    if (first_word.compare("chprompt") == 0) {
        return new ChpromptCommand(cmd);
    }
    if  (first_word.compare("ls") == 0) {
        return new LsCommand(cmd);
    }
    if (first_word.compare("showpid") == 0) {
        return new ShowPidCommand(cmd);
    }
    if (first_word.compare("pwd") == 0) {
        return new GetCurrDirCommand(cmd);
    }
    if (first_word.compare("cd") == 0) {
        return new ChangeDirCommand(cmd);
    }
    if (first_word.compare("jobs") == 0) {
        return new JobsCommand(cmd);
    }
    if (first_word.compare("kill") == 0) {
        return new KillCommand(cmd);
    }
    if (first_word.compare("fg") == 0) {
        return new ForegroundCommand(cmd);
    }
    if (first_word.compare("bg") == 0) {
        return new BackgroundCommand(cmd);
    }
    if (first_word.compare("cp") == 0){
        return new CopyCommand(cmd);
    }
    if (first_word.compare("quit") == 0) {
        return new QuitCommand(cmd);
    }
    if (!is_timeout && first_word.compare("timeout") == 0) {
        return new TimeoutCommand(cmd);
    }
    return new ExternalCommand(cmd, is_piped);
}

void SmallShell::ExecuteCommand(const char* cmd_line, bool is_special, bool is_piped, bool is_timeout) {
    Command* cmd = CreateCommand(cmd_line, is_special, is_piped, is_timeout);
    if(cmd != nullptr) {
        cmd->execute();
    }
    else {
        // Do something later
    }
}

string SmallShell::GetPrompt() {
    return (prompt + ">");
}

char *SmallShell::GetLastDir() {
    return last_dir;
}

JobsList *SmallShell::GetJobsList() {
    return &jobs_list;
}

JobsList::JobEntry *SmallShell::GetForeGroundJob() {
    return fore_ground_job;
}

void SmallShell::SetPrompt(string new_prom) {
    prompt = new_prom;
}

void SmallShell::SetForeGroundJob(JobsList::JobEntry *job) {
    fore_ground_job = job;
}

void SmallShell::UpdateLastDir(char *new_dir) {
    if (last_dir == nullptr) {
        last_dir = new_dir;
    }
    else {
        delete[] last_dir;
        last_dir = new_dir;
    }
}

Command::Command(const char *cmd_line) : cmd_line(cmd_line) {
    args = vector<string>();
    num_of_args = ParseCommandLine(cmd_line, args);
}

Command::~Command() {
    delete[] cmd_line;
}

BuiltInCommand::BuiltInCommand(const char *cmd_line) : Command(cmd_line) {
    if (args.back().compare("&") == 0) {
        args.pop_back();
        num_of_args--;
    } else if ((args.back().back()) == '&') {
        args.back().pop_back();
        return;
    }
}

ChpromptCommand::ChpromptCommand(const char *cmd_line) : BuiltInCommand(cmd_line), prompt("smash") {
    if (num_of_args > 1) {
        prompt = args.at(1);
    }
}

void ChpromptCommand::execute() {
    global_smash.SetPrompt(this->prompt);
}

void LsCommand::execute() {
    struct dirent **list_names;
    int num_of_files;
    num_of_files = scandir(".", &list_names, NULL, alphasort);
    if (num_of_files < 0) {
        return;
    }
    for(int i = 0 ; i < num_of_files ; i++) {
        cout << list_names[i]->d_name << endl;
        free(list_names[i]);
    }
    free(list_names);
}

void ShowPidCommand::execute() {
    cout << "smash pid is " << global_smash.GetSmashPid() << endl;
}

void GetCurrDirCommand::execute() {
    char *path = new char[sizeof(char) * PATH_MAX];
    char *temp_path = getcwd(path, PATH_MAX);
    if (temp_path != NULL) {
        cout << temp_path << endl;
    } else {
        perror("smash error: getcwd failed");
    }
    delete[] temp_path;
}

void ChangeDirCommand::AuxOfExe(string &str) {
    char *path;
    if (str.compare("-") == 0) {
        path = global_smash.GetLastDir();
    }
    else {
        path = new char[str.length() + 1];
        strcpy(path, str.c_str());
    }
    if (path == nullptr) {
        cout << "smash error: cd: OLDPWD not set" << endl;
        return;
    }
    else {
        char *path_2 = new char[sizeof(char) * PATH_MAX];
        char *curr_path =  getcwd(path_2, PATH_MAX);
        if (curr_path == NULL) {
            perror("smash error: getcwd failed");
            return;
        }
        if (chdir(path) != 0) {
            delete[] curr_path;
            perror("smash error: chdir failed");
            return;
        }
        else {
            global_smash.UpdateLastDir(curr_path);
        }
    }
}

void ChangeDirCommand::execute() {
    switch (num_of_args) {
        case 1:
            break;
        case 2:
            AuxOfExe(args.at(1));
            break;
        default:
            cout << "smash error: cd: too many arguments" << endl;
            break;
    }
}

void JobsCommand::execute() {
    JobsList *jobs_list = global_smash.GetJobsList();
    jobs_list->PrintJobsList();
}

void KillCommand::execute() {
    if(num_of_args != 3) {
        cout << "smash error: kill: invalid arguments" << endl;
        return;
    }
    string sig = args.at(1);
    if (sig[0] != '-' || !IsStringNumber(sig.erase(0,1)) || !IsStringNumber(args.at(2))) {
        cout << "smash error: kill: invalid arguments" << endl;
        return;
    }
    global_smash.GetJobsList()->RemoveFinishedJobs();
    int job_id = stoi(args.at(2));
    int sig_num = stoi(sig);
    if (!global_smash.GetJobsList()->JobIdExists(job_id)) {
        cout << "smash error: kill: job-id " << job_id << " does not exist" << endl;
        return;
    }
    pid_t pid = global_smash.GetJobsList()->GetJobPidByJobId(job_id);
    if(killpg(pid, sig_num) != 0) {
        perror("smash error: kill failed");
    }
    else {
        cout << "signal number " << sig_num << " was sent to pid " << pid << endl;
    }
}

void ForegroundCommand::execute() {
    if (num_of_args != 1 && num_of_args != 2) {
        cout << "smash error: fg: invalid arguments" << endl;
        return;
    }
    int job_id_to_foreground = -1;

    if(global_smash.GetJobsList()->IsEmpty()) { // ***
        cout << "smash error: fg: jobs list is empty" << endl;
        return;
    }
    if(num_of_args == 1) {
        job_id_to_foreground = global_smash.GetJobsList()->GetMaxJobId(); // ***
    }
    if(num_of_args == 2) {
        if(!IsStringNumber(args.at(1))) {
            cout << "smash error: fg: invalid arguments" << endl;
            return;
        }
        job_id_to_foreground = stoi(args.at(1));
        if(!global_smash.GetJobsList()->JobIdExists(job_id_to_foreground)) { // ***
            cout << "smash error: fg: job-id " << job_id_to_foreground << " does not exist" << endl;
            return;
        }
    }
    JobsList::JobEntry *job_to_foreground = global_smash.GetJobsList()->RemoveJobByJobId(job_id_to_foreground);
    pid_t pid = job_to_foreground->GetJobPid();
    if (killpg(pid, SIGCONT) != 0) {
        perror("smash error: kill failed");
        return;
    }
    cout << job_to_foreground->GetCommand()->GetCmdLine() << " : " << pid << endl;
    global_smash.SetForeGroundJob(job_to_foreground);
    if(waitpid(pid, NULL, WUNTRACED) < 0) {
        perror("smash error: waitpid failed");
        return;
    }
    if(!global_smash.GetJobsList()->JobPidExists(pid)) { // ***
        delete job_to_foreground;
    }
    global_smash.SetForeGroundJob(nullptr);

}

void BackgroundCommand::execute() {
    if (num_of_args != 1 && num_of_args != 2) {
        cout << "smash error: bg: invalid arguments" << endl;
        return;
    }
    int job_id_to_background = -1;
    if(num_of_args == 1) {
        job_id_to_background = global_smash.GetJobsList()->GetMaxStoppedJobId(); // Return the Max id from the Stop jobs else 0
        if(job_id_to_background == 0) {
            cout << "smash error: bg: there is no stopped jobs to resume" << endl;
            return;
        }
    }
    if(num_of_args == 2) {
        if(!IsStringNumber(args.at(1))) {
            cout << "smash error: bg: invalid arguments" << endl;
            return;
        }
        job_id_to_background = stoi(args.at(1));

        if(!global_smash.GetJobsList()->JobIdExists(job_id_to_background)) { // ***
            cout << "smash error: bg: job-id " << job_id_to_background << " does not exist" << endl;
            return;
        }
        JobsList::JobEntry *job_to_background = global_smash.GetJobsList()->GetJobById(job_id_to_background); // ***
        if(job_to_background->GetState() == Background) {
            cout << "smash error: bg: job-id " << job_id_to_background << " is already running in the background" << endl;
            return;
        }
    }
    JobsList::JobEntry *job_to_background = global_smash.GetJobsList()->GetJobById(job_id_to_background);
    pid_t pid = job_to_background->GetJobPid();
    cout << job_to_background->GetCommand()->GetCmdLine() << " : " << pid << endl;
    if (killpg(pid, SIGCONT) != 0) {
        perror("smash error: kill failed");
        return;
    }
    job_to_background->SetState(Background);

}

void QuitCommand::execute() {
    if(num_of_args > 1 && args.at(1) == "kill") {
        global_smash.GetJobsList()->RemoveFinishedJobs();
        cout << "smash: sending SIGKILL signal to " << global_smash.GetJobsList()->GetSize() << " jobs:" << endl;
        global_smash.GetJobsList()->jobs_list.sort(JobsCmpSmallerId);
        for (list<JobsList::JobEntry*>::iterator it = global_smash.GetJobsList()->jobs_list.begin();
        it != global_smash.GetJobsList()->jobs_list.end(); it++) {
            cout << (*it)->GetJobPid() << ": " << (*it)->GetCommand()->GetCmdLine() << endl;
        }
        global_smash.GetJobsList()->KillAllJobs();
    }
    exit(0);
}

void ExternalCommand::execute() {
    char* cmd_line = new char[COMMAND_MAX_LENGTH + 1];
    strcpy(cmd_line,GetCmdLine());
    bool is_cmd_background = IsBackgroundCommand(cmd_line);
    bool is_cmd_timeout = IsTimeoutCommand(cmd_line);
    bool is_cmd_red = IsRedirectionCommand(cmd_line);
    if(is_cmd_background) {
        RemoveBackgroundSign(cmd_line);
    }
    if(is_cmd_red) {
        RemoveRedirectionSign(cmd_line);
    }
    if(is_cmd_timeout) {
        RemoveTimeoutSign(cmd_line);
    }
    pid_t pid = fork();
    if (pid > 0) {
        if(!this->is_piped) {
        setpgid(pid,pid);
        }
        else {
        setpgid(pid,getpgrp());
        }
        if (is_cmd_background) {
            global_smash.GetJobsList()->AddJob(this, pid, Background, is_cmd_timeout);
        }
        else {
            JobsList::JobEntry *fg_job = new JobsList::JobEntry(-1, Foreground, this, pid, is_cmd_timeout);
            global_smash.SetForeGroundJob(fg_job);
            if(this->is_piped){
                if (waitpid(pid, NULL, 0) == FAIL) {
                 perror("smash error: waitpid failed");
                 return;
                }
            }
            else if (waitpid(pid, NULL, WUNTRACED) == FAIL) {
            perror("smash error: waitpid failed");
            return;
            }
            if (!global_smash.GetJobsList()->JobPidExists(pid)) { // ***
                delete fg_job;
            }
            global_smash.SetForeGroundJob(nullptr);
         }
         delete[] cmd_line;
    }
    if (pid == 0) {
        char *argv[] = {(char *) "/bin/bash", (char *) "-c", cmd_line, NULL};
        execv(argv[0], argv);
        perror("smash error: execv failed");
    }
    if (pid < 0) {
        perror("smash error: fork failed");
    }
}

RedirectionCommand::RedirectionCommand(const char *cmd_line) : Command(cmd_line) {
    const string old_cmd_line = cmd_line;
    size_t pos = old_cmd_line.find('>');
    new_cmd_line = old_cmd_line.substr(0, pos);
    vector<string> vec1 = vector<string>();
    if(ParseCommandLine(new_cmd_line.c_str(), vec1) == 0){
        new_cmd_line = "";
    }
    pos++;

    sign = ">";
    if (old_cmd_line.length() > pos && old_cmd_line[pos] == '>') {
        sign.push_back('>');
        pos++;
    }
    if (old_cmd_line.length() > pos) {
        string restOfCmd = old_cmd_line.substr(pos);
        vector<string> vec2 = vector<string>();
        if(ParseCommandLine(restOfCmd.c_str(), vec2) == 0){
            file_name = "";
        } else {
            file_name = vec2.at(0);
        }
        if(file_name.back() == '&'){
            file_name.pop_back();
        }
    } else {
        file_name = "";
    }
}

void RedirectionCommand::execute() {
    if(new_cmd_line.compare("") == 0 || file_name.compare("") == 0) {
        cout << "file_name is empty" << endl;
        return;
    }
    int std_out = dup(1); // check it
    close(1);
    if(sign.compare(">") == 0) {
        if(open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666) == FAIL) {
            perror("smash error: open failed");
            dup(std_out);
            close(std_out);
            return;
        }
    }
    if (sign.compare(">>") == 0) {
        if (open(file_name.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666) == FAIL) {
            perror("smash error: open failed");
            dup(std_out);
            close(std_out);
            return;
        }
    }
    if (IsBuiltInCommand(new_cmd_line)) {
        global_smash.ExecuteCommand(new_cmd_line.c_str(), true);
    }
    else {
        global_smash.ExecuteCommand(cmd_line, true);
    }
    close(1);
    dup(std_out);
    close(std_out);
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line) {
    const string cmd_s = cmd_line;
    size_t pos = cmd_s.find('|');
    first = cmd_s.substr(0, pos);
    vector<string> vec1 = vector<string>();
    if(ParseCommandLine(first.c_str(), vec1) == 0) {
        first = "";
    }
    pos++;

    sign = "|";
    if (cmd_s.length() > pos && cmd_s[pos] == '&') {
        sign.push_back('&');
        pos++;
    }

    if (cmd_s.length() > pos) {
        second = cmd_s.substr(pos);
        vector<string> vec2 = vector<string>();
        if(ParseCommandLine(second.c_str(), vec2) == 0){
            second = "";
        }
    } else {
        second = "";
    }

    background = IsBackgroundCommand(cmd_line);
    if(background){
        second = RemoveBackgroundSign(second);
    }
}

void PipeCommand::execute() {
    if (first.compare("") == 0 || second.compare("") == 0) {
        cout << "file_name is empty" << endl;
        return;
    }
    int fd[2];
    if(pipe(fd) == FAIL){
        perror("smash error: pipe failed");
        return;
    }
    pid_t pipe_pid = fork();
    if(pipe_pid > 0) {
        setpgid(pipe_pid,pipe_pid);
        close(fd[0]);
        close(fd[1]);
        if(background) {
            global_smash.GetJobsList()->AddJob(this, pipe_pid, Background);
        } else {
            JobsList::JobEntry *fg_job = new JobsList::JobEntry(FAIL, Foreground, this, pipe_pid);
            global_smash.SetForeGroundJob(fg_job);
            if (waitpid(pipe_pid, NULL, WUNTRACED) == FAIL) {
                perror("smash error: waitpid failed");
            }
            if (!global_smash.GetJobsList()->JobPidExists(pipe_pid)) {
                delete fg_job;
            }
            global_smash.SetForeGroundJob(nullptr);
        }
    }
    if(pipe_pid == 0) {
        pid_t first_cmd_pid = fork();
        if(first_cmd_pid > 0) {
            setpgid(first_cmd_pid,getpgrp());
            pid_t second_cmd_pid = fork();
            if (second_cmd_pid > 0) {
                setpgid(second_cmd_pid,getpgrp());
                close(fd[0]);
                close(fd[1]);
                if(waitpid(second_cmd_pid, NULL, 0) == FAIL || waitpid(first_cmd_pid, NULL, 0) == FAIL) {
                    perror("smash error: waitpid failed");
                }
                exit(0);
            }
            if (second_cmd_pid == 0) {
                dup2(fd[0], 0);
                close(fd[0]);
                close(fd[1]);
                global_smash.ExecuteCommand(second.c_str(), false, true);
                exit(0);
            }
            if (second_cmd_pid < 0) {
                perror("smash error: fork failed");
            }
        }
        if (first_cmd_pid == 0 ) { // src
            if(sign.compare("|") == 0){
                dup2(fd[1], 1);
            }
            else {
                dup2(fd[1], 2);
            }
            close(fd[0]);
            close(fd[1]);
            global_smash.ExecuteCommand(first.c_str(), false, true);
            exit(0);
        }
        if (first_cmd_pid < 0) {
            perror("smash error: fork failed");
        }
    }
    if (pipe_pid < 0) {
        perror("smash error: fork failed");
    }
}

TimeoutCommand::TimeoutCommand(const char *cmd_line) : Command(cmd_line) {
    if(num_of_args < 3 || !IsStringNumber(args[1]) || stoi(args[1]) < 0) {
        return;
    }
    duration = stoi(args[1]);
    string old_cmd = cmd_line;
    int pos = old_cmd.find(args[1]) + args[1].size();
    new_cmd_line = old_cmd.substr(pos);
}

void TimeoutCommand::execute() {
    if(num_of_args < 3 || !IsStringNumber(args[1]) || stoi(args[1]) < 0){
        cout << "smash error: timeout: invalid arguments" << endl;
        return;
    }

    int min_smash_time = global_smash.GetJobsList()->GetClosestTimeout();
    if(min_smash_time == -1) {
        alarm(duration);
    }
    else {
        int new_dur = min(duration, min_smash_time);
        alarm(new_dur);
    }

    if (IsBuiltInCommand(new_cmd_line)) {
        global_smash.ExecuteCommand(new_cmd_line.c_str(), false, false, true);
    }
    else {
        global_smash.ExecuteCommand(cmd_line, false, false, true);
    }
}

CopyCommand::CopyCommand(const char *cmd_line) :BuiltInCommand(cmd_line) {
    if((num_of_args < 3) || ((num_of_args == 3) && (args.at(2).compare("&") == 0))) {
        return;
    }
    is_background = IsBackgroundCommand(cmd_line);
    src_file = args[1];
    dst_file = RemoveBackgroundSign(args[2]);
    src_file_full = realpath(src_file.c_str(), NULL);
    if(src_file_full == NULL) {
        return;
    }
    dst_file_full = realpath(dst_file.c_str(), NULL);

    if(dst_file_full != NULL && strcmp(src_file_full, dst_file_full) == 0) {
        //dstFileFD = open(dstFile.c_str(), O_WRONLY, 0666);
    }
    else{
        src_file_failed = open(src_file.c_str(), O_RDONLY, 0666);
        dst_file_failed = open(dst_file.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0666);
        if(dst_file_full == NULL) {
            dst_file_full = realpath(dst_file.c_str(), NULL);
        }
    }

    buff = new char[READBLOCK];
}

CopyCommand::~CopyCommand(){
    if((num_of_args < 3) || ((num_of_args == 3) && (args.at(2).compare("&") == 0)) || src_file_full == NULL) {
        return;
    }
    if(dst_file_full != NULL && strcmp(src_file_full, dst_file_full) == 0) {
        close(src_file_failed);
        close(dst_file_failed);
    }
    if(src_file_full != NULL) {
        free(src_file_full);
    }
    if(dst_file_full != NULL){
        free (dst_file_full);
    }
    delete[] buff;
}

void CopyCommand::execute() {
    if((num_of_args < 3) || ((num_of_args == 3) && (args.at(2).compare("&") == 0))) {
        cout << "smash error: cp: invalid arguments" << endl;
        return;
    }
    if(src_file_full == NULL || dst_file_full == NULL){
        perror("smash error: open failed");
        return;
    }
    if(strcmp(src_file_full, dst_file_full) == 0){
        cout << "smash: " + src_file + " was copied to " + dst_file << endl;
        return;
    }
    if(src_file_failed == FAIL || dst_file_failed == FAIL){
        perror("smash error: open failed");
        return;
    }


    pid_t copy_pid = fork();
    if(copy_pid > 0) {
        if(is_background) {
            global_smash.GetJobsList()->AddJob(this, copy_pid, Background);
        }
        else {
            JobsList::JobEntry *fore_ground_job = new JobsList::JobEntry(FAIL, Foreground, this, copy_pid);
            global_smash.SetForeGroundJob(fore_ground_job);
            if (waitpid(copy_pid, NULL, WUNTRACED) == FAIL) {
                perror("smash error: waitpid failed");
            }
            if (!global_smash.GetJobsList()->JobPidExists(copy_pid)) {
                delete fore_ground_job;
            }
            global_smash.SetForeGroundJob(nullptr);
        }
    }
    if(copy_pid == 0) {
        setpgrp();
        ssize_t r_value;
        do {
            r_value = read(src_file_failed, buff, READBLOCK);
            if (r_value == FAIL) {
                perror("smash error: read failed");
                return;
            }
            if (write(dst_file_failed, buff, r_value) == FAIL) {
                perror("smash error: write failed");
                return;
            }
        } while (r_value != 0);
        cout << "smash: " + src_file + " was copied to " + dst_file << endl;
        exit(0);
    }
}


