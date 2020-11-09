#ifndef JOBS_H
#define JOBS_H

#include <stdbool.h>
#include <sys/types.h>

typedef enum {
    RUNNING,
    STOPPED,
    EXITED
}  JobStatus;

typedef struct {
    int job_id;
    pid_t process_id;

    JobStatus status;

    char* line;
} Job;

typedef struct JobListNode
{
    Job job;
    struct JobListNode* next;
} JobListNode;

typedef struct 
{
    JobListNode* first;
    JobListNode* last;
} JobList;

Job new_job(int, pid_t, JobStatus, char*);

JobListNode* new_job_list_node(Job);

JobList new_job_list();

void add_process_to_job_list(JobList*, pid_t, JobStatus, char*);

bool get_job_with_pid(JobList*, pid_t, Job*);

bool get_job_with_jid(JobList*, int, Job*);

void update_job_list(JobList*, pid_t, JobStatus);

void remove_job_with_pid(JobList*, pid_t);

void print_job(Job);

#endif