#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <unistd.h>
#include <algorithm>
#include <vector>
#include <array>
#include <list>
#include <string.h>
using namespace std;

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define HISTORY_MAX_RECORDS (50)
#define COMMAND_MAX_LENGTH (80)
#define READBLOCK (4096)
#define FAIL -1
#define SUCC 0
#define FIND_FAIL (string::npos)

enum JobState {Foreground,Background,Stopped};

bool IsStringNumber(const string &str);

class Command {
 protected:
  vector<string> args;
  int num_of_args;
  const char* cmd_line;
 public:
  Command(const char* cmd_line);
  virtual ~Command();
  virtual void execute() = 0;
  const char* GetCmdLine() {
      return cmd_line;
  }

  vector<string>* GetArgs() {
      return &args;
  }
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line);
  virtual ~BuiltInCommand() {}
    bool IsStringInCmdLine(string s);
};

class ExternalCommand : public Command {
    bool is_piped;
 public:
  ExternalCommand(const char* cmd_line, bool isPiped) : Command(cmd_line), is_piped(isPiped) {};
  virtual ~ExternalCommand() {}
  void execute() override;
};

class PipeCommand : public Command {
  string sign;
  string first;
  string second;
  bool background;
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
    string new_cmd_line;
    string file_name;
    string sign;
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;

};

class LsCommand: public BuiltInCommand {
public:
    LsCommand(const char* cmd_line) :BuiltInCommand(cmd_line){};
    virtual ~LsCommand() {}
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
  void AuxOfExe(string &str);
  public:
  ChangeDirCommand(const char* cmd_line):BuiltInCommand(cmd_line){};
  virtual ~ChangeDirCommand() {}
  void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line): BuiltInCommand(cmd_line){};
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line){};
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
 public:
  QuitCommand(const char* cmd_line):BuiltInCommand(cmd_line){};
  virtual ~QuitCommand() {}
  void execute() override;
};

class JobsList {
 public:
  class JobEntry {
      int job_id;
      JobState job_state;
      time_t add_time;
      Command* cmd;
      pid_t pid;
      bool time_out;
      time_t time_stamp;
      int duration;
  public:
      JobEntry(int id, JobState state, Command* cmd, pid_t pid, bool time_out = false);
      JobState GetState() const {
          return this->job_state;
      };

      int GetJobId() const {
          return this-> job_id;
      };

      pid_t GetJobPid()const {
          return this->pid;
      };

      time_t GetTimeThatAdded() const {
          return this->add_time;
      };

      Command* GetCommand() {
          return cmd;
      };

      time_t GetTimeStamp() const {
          return this->time_stamp;
      };

      time_t GetDifferentTime() const {
          return difftime(time_stamp + duration, time(nullptr));
      };

      int GetDuration() const {
          return this->duration;
      };

      bool IsTimeOut() const {
          return this->time_out;
      };

      void SetJobId(int job_id) {
          this->job_id = job_id;
      };

      void SetState(JobState job_state){
          this->job_state = job_state;
      };

      void ResetTimeAdded() {
          this->add_time = time(NULL);
      };

  };
    list<JobEntry*> jobs_list;
 public:
  JobsList();
  ~JobsList();
  void AddJob(Command* cmd, pid_t pid, JobState state, bool is_timeout = false);
  void AddJob(JobEntry* job, JobState state, bool give_job_id = false);
  JobEntry* RemoveJobByJobId(int job_id);
  JobEntry* RemoveJobByPid(pid_t pid);
  void PrintJobsList();
  void KillAllJobs();
  void RemoveFinishedJobs();
  JobEntry *GetJobById(int job_id);
  JobEntry *GetLastJob(int* last_job_id);
  JobEntry *GetLastStoppedJob(int* job_id);
  JobsList::JobEntry* GetTimeoutJobToKill();
  int GetMaxJobId();
  int GetMaxStoppedJobId();
  int GetClosestTimeout();
  pid_t GetJobPidByJobId(int id);
  int GetSize();
  bool JobIdExists(int jobId);
  bool JobPidExists(int jobPid);
  bool IsEmpty();
};

bool JobsCmpClosestTime(const JobsList::JobEntry *j1, const JobsList::JobEntry *j2);

class JobsCommand : public BuiltInCommand {
 public:
  JobsCommand(const char* cmd_line) : BuiltInCommand(cmd_line){};
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 public:
  KillCommand(const char* cmd_line) : BuiltInCommand(cmd_line){};
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 public:
  ForegroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line){};
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 public:
  BackgroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line){};
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class ChpromptCommand : public BuiltInCommand {
    string prompt;
public:
    ChpromptCommand(const char* cmd_line);
    virtual ~ChpromptCommand(){};
    void execute() override;
};

class TimeoutCommand : public Command{
    int duration;
    string new_cmd_line;
public:
    TimeoutCommand(const char* cmd_line);
    virtual ~TimeoutCommand(){};
    void execute() override;
};

class CopyCommand : public BuiltInCommand {
    bool is_background;
    string src_file = "";
    string dst_file = "";
    char* src_file_full = NULL;
    char* dst_file_full = NULL;
    int src_file_failed = -1;
    int dst_file_failed = -1;
    char* buff = NULL;
public:
    CopyCommand(const char* cmd_line);
    virtual ~CopyCommand();
    void execute() override;
};

class SmallShell {
 private:
    string prompt;
    char* last_dir;
    JobsList jobs_list;
    JobsList::JobEntry* fore_ground_job;
    const pid_t smash_pid;
    SmallShell();
 public:
  Command *CreateCommand(const char* cmd_line, bool is_special, bool is_piped, bool is_timeout);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& GetInstance() {
    static SmallShell instance;
    return instance;
  }
  ~SmallShell();
  void ExecuteCommand(const char* cmd_line, bool is_special = false, bool is_piped = false, bool is_timeout = false);

    string GetPrompt();
    char* GetLastDir();
    JobsList* GetJobsList();
    JobsList::JobEntry* GetForeGroundJob();
    void UpdateLastDir(char* newDir);
    void AddLastDir(char* lastDir);
    void SetPrompt(string newPrompt);
    void SetForeGroundJob(JobsList::JobEntry* job);
    bool IsSmashPid(pid_t pid){
        return (smash_pid == pid);
    };
    pid_t GetSmashPid(){
        return smash_pid;
    };
};

#endif //SMASH_COMMAND_H_
