#include "deadlock.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int visited[MAX_PROCESSES + MAX_RESOURCES];
int parent[MAX_PROCESSES + MAX_RESOURCES];
int cycle[MAX_PROCESSES + MAX_RESOURCES];
int cycle_len = 0;

void reverse(int *arr, int len) {
  for (int i = 0; i < len / 2; i++) {
    int temp = arr[i];
    arr[i] = arr[len - 1 - i];
    arr[len - 1 - i] = temp;
  }
}

int RAG_dfs(const RAGSystem *sys, int u) {
  visited[u] = 1;

  if (u < sys->num_processes) {
    int p = u;
    for (int r = 0; r < sys->num_resources; r++) {
      if (sys->request[p][r] > 0) {
        int v = sys->num_processes + r;
        if (visited[v] == 1) {
          int curr = u;
          cycle[cycle_len++] = v;
          while (curr != v && curr != -1) {
            cycle[cycle_len++] = curr;
            curr = parent[curr];
          }
          cycle[cycle_len++] = v;
          reverse(cycle, cycle_len);
          return 1;
        } else if (visited[v] == 0) {
          parent[v] = u;
          if (RAG_dfs(sys, v))
            return 1;
        }
      }
    }
  } else {
    int r = u - sys->num_processes;
    for (int p = 0; p < sys->num_processes; p++) {
      if (sys->allocation[r][p] > 0) {
        int v = p;
        if (visited[v] == 1) {
          int curr = u;
          cycle[cycle_len++] = v;
          while (curr != v && curr != -1) {
            cycle[cycle_len++] = curr;
            curr = parent[curr];
          }
          cycle[cycle_len++] = v;
          reverse(cycle, cycle_len);
          return 1;
        } else if (visited[v] == 0) {
          parent[v] = u;
          if (RAG_dfs(sys, v))
            return 1;
        }
      }
    }
  }

  visited[u] = 2;
  return 0;
}

int detect_rag_deadlock(const RAGSystem *sys, int *out_cycle,
                        int *out_cycle_len) {
  memset(visited, 0, sizeof(visited));
  memset(parent, -1, sizeof(parent));
  cycle_len = 0;

  int total_nodes = sys->num_processes + sys->num_resources;
  for (int i = 0; i < total_nodes; i++) {
    if (visited[i] == 0) {
      if (RAG_dfs(sys, i)) {
        *out_cycle_len = cycle_len;
        memcpy(out_cycle, cycle, sizeof(int) * cycle_len);
        return 1;
      }
    }
  }
  return 0;
}

int check_bankers_safety(const BankerSystem *sys, int *safe_seq,
                         char *steps_log) {
  int work[MAX_RESOURCE_TYPES];
  int finish[MAX_PROCESSES];

  for (int r = 0; r < sys->num_resources; r++) {
    work[r] = sys->available[r];
  }

  for (int p = 0; p < sys->num_processes; p++) {
    finish[p] = 0;
  }

  int count = 0;
  char temp[256];
  steps_log[0] = '\0';

  sprintf(temp, "Starting Safety Check. Work (Available) = [");
  strcat(steps_log, temp);
  for (int r = 0; r < sys->num_resources; r++) {
    sprintf(temp, "%d%s", work[r], (r == sys->num_resources - 1) ? "" : ", ");
    strcat(steps_log, temp);
  }
  strcat(steps_log, "]. ");

  while (count < sys->num_processes) {
    int found = 0;
    for (int p = 0; p < sys->num_processes; p++) {
      if (!finish[p]) {
        int possible = 1;
        for (int r = 0; r < sys->num_resources; r++) {
          if (sys->need[p][r] > work[r]) {
            possible = 0;
            break;
          }
        }

        if (possible) {
          sprintf(temp, "P%d can run (Need [", p);
          strcat(steps_log, temp);
          for (int r = 0; r < sys->num_resources; r++) {
            sprintf(temp, "%d%s", sys->need[p][r],
                    (r == sys->num_resources - 1) ? "" : ", ");
            strcat(steps_log, temp);
          }
          sprintf(temp, "] <= Work). Reclaiming resources. New Work = [");
          strcat(steps_log, temp);

          for (int r = 0; r < sys->num_resources; r++) {
            work[r] += sys->allocation[p][r];
            sprintf(temp, "%d%s", work[r],
                    (r == sys->num_resources - 1) ? "" : ", ");
            strcat(steps_log, temp);
          }
          strcat(steps_log, "]. ");

          safe_seq[count++] = p;
          finish[p] = 1;
          found = 1;
          break;
        }
      }
    }

    if (!found) {
      strcat(
          steps_log,
          "No process can safely run. System is UNSAFE (potential deadlock).");
      return 0;
    }
  }

  strcat(steps_log, "All processes successfully scheduled. System is SAFE.");
  return 1;
}

void print_rag_state(const RAGSystem *sys) {
  printf("        \"state\": {\n");
  printf("          \"requests\": [\n");
  for (int p = 0; p < sys->num_processes; p++) {
    printf("            [");
    for (int r = 0; r < sys->num_resources; r++) {
      printf("%d%s", sys->request[p][r],
             (r == sys->num_resources - 1) ? "" : ", ");
    }
    printf("]%s\n", (p == sys->num_processes - 1) ? "" : ",");
  }
  printf("          ],\n");
  printf("          \"allocation\": [\n");
  for (int r = 0; r < sys->num_resources; r++) {
    printf("            [");
    for (int p = 0; p < sys->num_processes; p++) {
      printf("%d%s", sys->allocation[r][p],
             (p == sys->num_processes - 1) ? "" : ", ");
    }
    printf("]%s\n", (r == sys->num_resources - 1) ? "" : ",");
  }
  printf("          ]\n");
  printf("        }\n");
}

void log_init_rag(const RAGSystem *sys) {
  printf("{\n");
  printf("  \"type\": \"RAG\",\n");
  printf("  \"init\": {\n");
  printf("    \"num_processes\": %d,\n", sys->num_processes);
  printf("    \"num_resources\": %d,\n", sys->num_resources);
  printf("    \"processes\": [\n");
  for (int i = 0; i < sys->num_processes; i++) {
    printf("      {\"id\": %d, \"name\": \"%s\"}%s\n", sys->processes[i].id,
           sys->processes[i].name, (i == sys->num_processes - 1) ? "" : ",");
  }
  printf("    ],\n");
  printf("    \"resources\": [\n");
  for (int i = 0; i < sys->num_resources; i++) {
    printf("      {\"id\": %d, \"name\": \"%s\", \"total_units\": %d}%s\n",
           sys->resources[i].id, sys->resources[i].name,
           sys->resources[i].total_units,
           (i == sys->num_resources - 1) ? "" : ",");
  }
  printf("    ]\n");
  printf("  },\n");
  printf("  \"timeline\": [\n");
}

void log_event_rag(EventType type, const RAGSystem *sys, int process_id,
                   int resource_id, const char *msg) {
  static int step = 0;
  static const char *events[] = {
      "INIT",          "REQUEST",        "ALLOCATE",          "RELEASE",
      "WAIT",          "CHECK_DEADLOCK", "DEADLOCK_DETECTED", "NO_DEADLOCK",
      "BANKERS_CHECK", "BANKERS_SAFE",   "BANKERS_UNSAFE",    "TERMINATE"};

  printf("    {\n");
  printf("      \"step\": %d,\n", step++);
  printf("      \"event\": \"%s\",\n", events[type]);
  printf("      \"process_id\": %d,\n", process_id);
  printf("      \"resource_id\": %d,\n", resource_id);
  printf("      \"message\": \"%s\",\n", msg);
  print_rag_state(sys);
  printf("    },\n");
}

void log_deadlock_detected(const RAGSystem *sys, int *cycle_path, int len) {
  static int step = 0;
  printf("    {\n");
  printf("      \"step\": %d,\n", step++);
  printf("      \"event\": \"DEADLOCK_DETECTED\",\n");
  printf("      \"process_id\": -1,\n");
  printf("      \"resource_id\": -1,\n");
  printf("      \"message\": \"Deadlock detected in wait-for-graph!\",\n");
  printf("      \"cycle\": [");
  for (int i = 0; i < len; i++) {
    printf("%d%s", cycle_path[i], (i == len - 1) ? "" : ", ");
  }
  printf("],\n");
  print_rag_state(sys);
  printf("    },\n");
}

void print_bankers_state(const BankerSystem *sys) {
  printf("        \"state\": {\n");

  printf("          \"available\": [");
  for (int r = 0; r < sys->num_resources; r++) {
    printf("%d%s", sys->available[r],
           (r == sys->num_resources - 1) ? "" : ", ");
  }
  printf("],\n");

  printf("          \"allocation\": [\n");
  for (int p = 0; p < sys->num_processes; p++) {
    printf("            [");
    for (int r = 0; r < sys->num_resources; r++) {
      printf("%d%s", sys->allocation[p][r],
             (r == sys->num_resources - 1) ? "" : ", ");
    }
    printf("]%s\n", (p == sys->num_processes - 1) ? "" : ",");
  }
  printf("          ],\n");

  printf("          \"max\": [\n");
  for (int p = 0; p < sys->num_processes; p++) {
    printf("            [");
    for (int r = 0; r < sys->num_resources; r++) {
      printf("%d%s", sys->max[p][r], (r == sys->num_resources - 1) ? "" : ", ");
    }
    printf("]%s\n", (p == sys->num_processes - 1) ? "" : ",");
  }
  printf("          ],\n");

  printf("          \"need\": [\n");
  for (int p = 0; p < sys->num_processes; p++) {
    printf("            [");
    for (int r = 0; r < sys->num_resources; r++) {
      printf("%d%s", sys->need[p][r],
             (r == sys->num_resources - 1) ? "" : ", ");
    }
    printf("]%s\n", (p == sys->num_processes - 1) ? "" : ",");
  }
  printf("          ]\n");

  printf("        }\n");
}

void log_init_bankers(const BankerSystem *sys) {
  printf("{\n");
  printf("  \"type\": \"BANKERS\",\n");
  printf("  \"init\": {\n");
  printf("    \"num_processes\": %d,\n", sys->num_processes);
  printf("    \"num_resources\": %d,\n", sys->num_resources);
  printf("    \"processes\": [\n");
  for (int i = 0; i < sys->num_processes; i++) {
    printf("      {\"id\": %d, \"name\": \"P%d\"}%s\n", i, i,
           (i == sys->num_processes - 1) ? "" : ",");
  }
  printf("    ],\n");
  printf("    \"resources\": [\n");
  for (int i = 0; i < sys->num_resources; i++) {
    printf("      {\"id\": %d, \"name\": \"%c\"}%s\n", i, 'A' + i,
           (i == sys->num_resources - 1) ? "" : ",");
  }
  printf("    ]\n");
  printf("  },\n");
  printf("  \"timeline\": [\n");
}

void log_event_bankers(EventType type, const BankerSystem *sys, int process_id,
                       const int *request, const char *msg, int is_safe,
                       const int *safe_sequence) {
  static int step = 0;
  static const char *events[] = {
      "INIT",          "REQUEST",        "ALLOCATE",          "RELEASE",
      "WAIT",          "CHECK_DEADLOCK", "DEADLOCK_DETECTED", "NO_DEADLOCK",
      "BANKERS_CHECK", "BANKERS_SAFE",   "BANKERS_UNSAFE",    "TERMINATE"};

  printf("    {\n");
  printf("      \"step\": %d,\n", step++);
  printf("      \"event\": \"%s\",\n", events[type]);
  printf("      \"process_id\": %d,\n", process_id);

  if (request != NULL) {
    printf("      \"request\": [");
    for (int r = 0; r < sys->num_resources; r++) {
      printf("%d%s", request[r], (r == sys->num_resources - 1) ? "" : ", ");
    }
    printf("],\n");
  } else {
    printf("      \"request\": null,\n");
  }

  printf("      \"message\": \"%s\",\n", msg);
  printf("      \"is_safe\": %s,\n", is_safe ? "true" : "false");

  if (safe_sequence != NULL && is_safe) {
    printf("      \"safe_sequence\": [");
    for (int p = 0; p < sys->num_processes; p++) {
      printf("%d%s", safe_sequence[p],
             (p == sys->num_processes - 1) ? "" : ", ");
    }
    printf("],\n");
  } else {
    printf("      \"safe_sequence\": null,\n");
  }

  print_bankers_state(sys);
  printf("    },\n");
}

void run_rag_cycle_scenario() {
  RAGSystem sys;
  sys.num_processes = 3;
  sys.num_resources = 3;

  for (int i = 0; i < 3; i++) {
    sys.processes[i].id = i;
    sprintf(sys.processes[i].name, "P%d", i);
    sys.processes[i].active = 1;

    sys.resources[i].id = i;
    sprintf(sys.resources[i].name, "R%d", i);
    sys.resources[i].total_units = 1;
    sys.resources[i].available_units = 1;
    sys.resources[i].active = 1;
  }

  memset(sys.request, 0, sizeof(sys.request));
  memset(sys.allocation, 0, sizeof(sys.allocation));

  log_init_rag(&sys);
  log_event_rag(EVENT_INIT, &sys, -1, -1,
                "Resource Allocation Graph Scenario Initialized");

  sys.request[0][0] = 1;
  log_event_rag(EVENT_REQUEST, &sys, 0, 0, "P0 requests R0");

  sys.request[0][0] = 0;
  sys.allocation[0][0] = 1;
  sys.resources[0].available_units = 0;
  log_event_rag(EVENT_ALLOCATE, &sys, 0, 0, "R0 is allocated to P0");

  sys.request[1][1] = 1;
  log_event_rag(EVENT_REQUEST, &sys, 1, 1, "P1 requests R1");

  sys.request[1][1] = 0;
  sys.allocation[1][1] = 1;
  sys.resources[1].available_units = 0;
  log_event_rag(EVENT_ALLOCATE, &sys, 1, 1, "R1 is allocated to P1");

  sys.request[2][2] = 1;
  log_event_rag(EVENT_REQUEST, &sys, 2, 2, "P2 requests R2");

  sys.request[2][2] = 0;
  sys.allocation[2][2] = 1;
  sys.resources[2].available_units = 0;
  log_event_rag(EVENT_ALLOCATE, &sys, 2, 2, "R2 is allocated to P2");

  sys.request[0][1] = 1;
  log_event_rag(EVENT_WAIT, &sys, 0, 1,
                "P0 requests R1 (resource busy, P0 must wait)");

  sys.request[1][2] = 1;
  log_event_rag(EVENT_WAIT, &sys, 1, 2,
                "P1 requests R2 (resource busy, P1 must wait)");

  sys.request[2][0] = 1;
  log_event_rag(EVENT_WAIT, &sys, 2, 0,
                "P2 requests R0 (resource busy, P2 must wait)");

  int path[MAX_PROCESSES + MAX_RESOURCES];
  int len = 0;
  log_event_rag(EVENT_CHECK_DEADLOCK, &sys, -1, -1,
                "Running Wait-For-Graph Cycle Detection...");
  if (detect_rag_deadlock(&sys, path, &len)) {
    log_deadlock_detected(&sys, path, len);
  } else {
    log_event_rag(EVENT_NO_DEADLOCK, &sys, -1, -1,
                  "No cycle detected. System running normally.");
  }

  printf("    {\n");
  printf("      \"step\": -1,\n");
  printf("      \"event\": \"TERMINATE\",\n");
  printf("      \"message\": \"Simulation ended.\"\n");
  printf("    }\n");
  printf("  ]\n}\n");
}

void run_dining_philosophers_scenario() {
  RAGSystem sys;
  sys.num_processes = 5;
  sys.num_resources = 5;

  for (int i = 0; i < 5; i++) {
    sys.processes[i].id = i;
    sprintf(sys.processes[i].name, "Philosopher %d", i);
    sys.processes[i].active = 1;

    sys.resources[i].id = i;
    sprintf(sys.resources[i].name, "Fork %d", i);
    sys.resources[i].total_units = 1;
    sys.resources[i].available_units = 1;
    sys.resources[i].active = 1;
  }

  memset(sys.request, 0, sizeof(sys.request));
  memset(sys.allocation, 0, sizeof(sys.allocation));

  log_init_rag(&sys);
  log_event_rag(EVENT_INIT, &sys, -1, -1,
                "Dining Philosophers Scenario (5 Phil / 5 Forks) Initialized");

  for (int i = 0; i < 5; i++) {
    int left_fork = i;
    sys.request[i][left_fork] = 1;
    char msg[64];
    sprintf(msg, "Phil %d requests left Fork %d", i, left_fork);
    log_event_rag(EVENT_REQUEST, &sys, i, left_fork, msg);

    sys.request[i][left_fork] = 0;
    sys.allocation[left_fork][i] = 1;
    sys.resources[left_fork].available_units = 0;
    sprintf(msg, "Phil %d acquires left Fork %d", i, left_fork);
    log_event_rag(EVENT_ALLOCATE, &sys, i, left_fork, msg);
  }

  for (int i = 0; i < 5; i++) {
    int right_fork = (i + 1) % 5;
    sys.request[i][right_fork] = 1;
    char msg[64];
    sprintf(msg, "Phil %d requests right Fork %d (busy, Phil must wait)", i,
            right_fork);
    log_event_rag(EVENT_WAIT, &sys, i, right_fork, msg);
  }

  int path[MAX_PROCESSES + MAX_RESOURCES];
  int len = 0;
  log_event_rag(EVENT_CHECK_DEADLOCK, &sys, -1, -1,
                "Checking RAG for cycle...");
  if (detect_rag_deadlock(&sys, path, &len)) {
    log_deadlock_detected(&sys, path, len);
  } else {
    log_event_rag(EVENT_NO_DEADLOCK, &sys, -1, -1, "No cycle detected.");
  }

  printf("    {\n");
  printf("      \"step\": -1,\n");
  printf("      \"event\": \"TERMINATE\",\n");
  printf("      \"message\": \"Simulation ended.\"\n");
  printf("    }\n");
  printf("  ]\n}\n");
}

void run_bankers_scenario(int simulate_unsafe) {
  BankerSystem sys;
  sys.num_processes = 5;
  sys.num_resources = 3;

  sys.available[0] = 3;
  sys.available[1] = 3;
  sys.available[2] = 2;

  int alloc[5][3] = {{0, 1, 0}, {2, 0, 0}, {3, 0, 2}, {2, 1, 1}, {0, 0, 2}};

  int max[5][3] = {{7, 5, 3}, {3, 2, 2}, {9, 0, 2}, {2, 2, 2}, {4, 3, 3}};

  for (int p = 0; p < 5; p++) {
    for (int r = 0; r < 3; r++) {
      sys.allocation[p][r] = alloc[p][r];
      sys.max[p][r] = max[p][r];
      sys.need[p][r] = max[p][r] - alloc[p][r];
    }
  }

  log_init_bankers(&sys);
  log_event_bankers(EVENT_INIT, &sys, -1, NULL,
                    "Banker's Algorithm State Initialized", 1, NULL);

  int safe_seq[MAX_PROCESSES];
  char steps[1024];
  int is_safe = check_bankers_safety(&sys, safe_seq, steps);
  log_event_bankers(EVENT_BANKERS_CHECK, &sys, -1, NULL, steps, is_safe,
                    safe_seq);

  if (!simulate_unsafe) {
    int req[3] = {1, 0, 2};
    log_event_bankers(EVENT_REQUEST, &sys, 1, req,
                      "Process P1 requests resources [1, 0, 2]", 1, NULL);

    for (int r = 0; r < 3; r++) {
      sys.available[r] -= req[r];
      sys.allocation[1][r] += req[r];
      sys.need[1][r] -= req[r];
    }

    is_safe = check_bankers_safety(&sys, safe_seq, steps);
    if (is_safe) {
      log_event_bankers(EVENT_BANKERS_SAFE, &sys, 1, req,
                        "Request approved. System remains safe.", 1, safe_seq);

      int req2[3] = {0, 1, 0};
      log_event_bankers(EVENT_REQUEST, &sys, 3, req2,
                        "Process P3 requests resources [0, 1, 0]", 1, NULL);
      for (int r = 0; r < 3; r++) {
        sys.available[r] -= req2[r];
        sys.allocation[3][r] += req2[r];
        sys.need[3][r] -= req2[r];
      }
      is_safe = check_bankers_safety(&sys, safe_seq, steps);
      if (is_safe) {
        log_event_bankers(EVENT_BANKERS_SAFE, &sys, 3, req2,
                          "Request approved. System remains safe.", 1,
                          safe_seq);

        log_event_bankers(
            EVENT_RELEASE, &sys, 3, NULL,
            "Process P3 completes execution and releases all resources.", 1,
            NULL);
        for (int r = 0; r < 3; r++) {
          sys.available[r] += sys.allocation[3][r];
          sys.allocation[3][r] = 0;
          sys.need[3][r] = 0;
        }
        is_safe = check_bankers_safety(&sys, safe_seq, steps);
        log_event_bankers(EVENT_BANKERS_CHECK, &sys, -1, NULL, steps, is_safe,
                          safe_seq);
      }
    } else {
      for (int r = 0; r < 3; r++) {
        sys.available[r] += req[r];
        sys.allocation[1][r] -= req[r];
        sys.need[1][r] += req[r];
      }
      log_event_bankers(EVENT_BANKERS_UNSAFE, &sys, 1, req,
                        "Request rejected! Would lead to unsafe state.", 0,
                        NULL);
    }
  } else {
    int req[3] = {0, 2, 0};
    log_event_bankers(EVENT_REQUEST, &sys, 0, req,
                      "Process P0 requests [0, 2, 0]", 1, NULL);

    for (int r = 0; r < 3; r++) {
      sys.available[r] -= req[r];
      sys.allocation[0][r] += req[r];
      sys.need[0][r] -= req[r];
    }

    is_safe = check_bankers_safety(&sys, safe_seq, steps);
    if (is_safe) {
      log_event_bankers(EVENT_BANKERS_SAFE, &sys, 0, req, "Request approved.",
                        1, safe_seq);
    } else {
      for (int r = 0; r < 3; r++) {
        sys.available[r] += req[r];
        sys.allocation[0][r] -= req[r];
        sys.need[0][r] += req[r];
      }
      log_event_bankers(EVENT_BANKERS_UNSAFE, &sys, 0, req,
                        "Request rejected! Safe sequence cannot be found, "
                        "putting process P0 to wait.",
                        0, NULL);
    }
  }

  printf("    {\n");
  printf("      \"step\": -1,\n");
  printf("      \"event\": \"TERMINATE\",\n");
  printf("      \"message\": \"Simulation ended.\"\n");
  printf("    }\n");
  printf("  ]\n");
  printf("}\n");
}

int main(int argc, char **argv) {
  char *scenario = "rag_cycle";
  if (argc > 2 && strcmp(argv[1], "--scenario") == 0) {
    scenario = argv[2];
  } else if (argc > 1 && strcmp(argv[1], "-h") == 0) {
    fprintf(stderr,
            "Usage: %s [--scenario <rag_cycle | dining_philosophers | "
            "bankers_safe | bankers_unsafe>]\n",
            argv[0]);
    return 1;
  }

  if (strcmp(scenario, "rag_cycle") == 0) {
    run_rag_cycle_scenario();
  } else if (strcmp(scenario, "dining_philosophers") == 0) {
    run_dining_philosophers_scenario();
  } else if (strcmp(scenario, "bankers_safe") == 0) {
    run_bankers_scenario(0);
  } else if (strcmp(scenario, "bankers_unsafe") == 0) {
    run_bankers_scenario(1);
  } else {
    fprintf(stderr, "Unknown scenario: %s\n", scenario);
    return 1;
  }

  return 0;
}
