#ifndef DEADLOCK_H
#define DEADLOCK_H

#define MAX_PROCESSES 16
#define MAX_RESOURCES 16
#define MAX_RESOURCE_TYPES 8

typedef enum {
  EVENT_INIT,
  EVENT_REQUEST,
  EVENT_ALLOCATE,
  EVENT_RELEASE,
  EVENT_WAIT,
  EVENT_CHECK_DEADLOCK,
  EVENT_DEADLOCK_DETECTED,
  EVENT_NO_DEADLOCK,
  EVENT_BANKERS_CHECK,
  EVENT_BANKERS_SAFE,
  EVENT_BANKERS_UNSAFE,
  EVENT_TERMINATE
} EventType;

typedef struct {
  int id;
  char name[16];
  int active;
} Process;

typedef struct {
  int id;
  char name[16];
  int total_units;
  int available_units;
  int active;
} Resource;

typedef struct {
  int num_processes;
  int num_resources;
  Process processes[MAX_PROCESSES];
  Resource resources[MAX_RESOURCES];
  int request[MAX_PROCESSES][MAX_RESOURCES];
  int allocation[MAX_RESOURCES][MAX_PROCESSES];
} RAGSystem;

typedef struct {
  int num_processes;
  int num_resources;
  int available[MAX_RESOURCE_TYPES];
  int max[MAX_PROCESSES][MAX_RESOURCE_TYPES];
  int allocation[MAX_PROCESSES][MAX_RESOURCE_TYPES];
  int need[MAX_PROCESSES][MAX_RESOURCE_TYPES];
} BankerSystem;

void log_init_rag(const RAGSystem *sys);
void log_event_rag(EventType type, const RAGSystem *sys, int process_id,
                   int resource_id, const char *msg);
void log_deadlock_detected(const RAGSystem *sys, int *cycle, int cycle_len);

void log_init_bankers(const BankerSystem *sys);
void log_event_bankers(EventType type, const BankerSystem *sys, int process_id,
                       const int *request, const char *msg, int is_safe,
                       const int *safe_sequence);

#endif
