#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    cout << "smash: got ctrl-Z" << endl;
    JobsList::JobEntry* fore_ground = SmallShell::GetInstance().GetForeGroundJob();
    if(fore_ground != nullptr) {
        pid_t pid = fore_ground->GetJobPid();
        if(killpg(pid, SIGSTOP) != 0) {
            perror("smash error: kill failed");
            SmallShell::GetInstance().SetForeGroundJob(nullptr);
            return;
        }
        if(fore_ground->GetJobId() == -1) {
            SmallShell::GetInstance().GetJobsList()->AddJob(fore_ground, Stopped, true);
        }
        else {
            SmallShell::GetInstance().GetJobsList()->AddJob(fore_ground, Stopped);
        }
        SmallShell::GetInstance().SetForeGroundJob(nullptr);
        cout << "smash: process " << pid << " was stopped" << endl;
    }
}

void ctrlCHandler(int sig_num) {
    cout << "smash: got ctrl-C" << endl;
    JobsList::JobEntry* fore_ground = SmallShell::GetInstance().GetForeGroundJob();
    SmallShell::GetInstance().SetForeGroundJob(nullptr);
    if(fore_ground != NULL) {
        pid_t pid = fore_ground->GetJobPid();
        if(killpg(pid, SIGKILL) != 0) {
            perror("smash error: kill failed");
            return;
        }
        cout << "smash: process " << pid << " was killed" << endl;
    }
}

void alarmHandler(int sig_num) {
    cout << "smash: got an alarm" << endl;
    JobsList::JobEntry*  job_to_kill = nullptr;
    SmallShell::GetInstance().GetJobsList()->RemoveFinishedJobs();
    JobsList::JobEntry* job_from_list_to_kill = SmallShell::GetInstance().GetJobsList()->GetTimeoutJobToKill();
    JobsList::JobEntry* fore_ground_job = SmallShell::GetInstance().GetForeGroundJob();
    if (fore_ground_job == nullptr || fore_ground_job->IsTimeOut() == false) {
        job_to_kill = job_from_list_to_kill;
    }
    else {
        if(job_from_list_to_kill == nullptr) {
            job_to_kill = fore_ground_job;
        }
        else {
            bool closer = JobsCmpClosestTime(job_from_list_to_kill, fore_ground_job);
            if(closer) {
                job_to_kill = job_from_list_to_kill;
            }
            else {
                job_to_kill = fore_ground_job;
            }
        }
    }
    if(job_to_kill != nullptr) {
        cout << "smash: " << job_to_kill->GetCommand()->GetCmdLine() << " timed out!" << endl;
        if(killpg(job_to_kill->GetJobPid(), SIGKILL) != 0){
            perror("smash error: kill failed");
            return;
        }
        SmallShell::GetInstance().GetJobsList()->RemoveJobByPid(job_to_kill->GetJobPid());
    }

    fore_ground_job = SmallShell::GetInstance().GetForeGroundJob();
    int fore_ground_time = -1 ;
    if(fore_ground_job != nullptr && fore_ground_job->IsTimeOut()){
        fore_ground_time = fore_ground_job->GetDifferentTime();
    }
    int min_time_job_list = SmallShell::GetInstance().GetJobsList()->GetClosestTimeout();
    if(min_time_job_list == -1) { // min is not ava but fore is
        if(fore_ground_time != -1) {
            alarm(fore_ground_time);
        }
    }
         else { // fore is not ava but min is
             if(fore_ground_time == -1) {
                 alarm(min_time_job_list);
        }
             else { // both ava, get the min from both
                alarm(min(fore_ground_time, min_time_job_list));
        }
    }

}

