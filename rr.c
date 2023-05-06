#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process
{
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  TAILQ_ENTRY(process) pointers;

  /* Additional fields here */
  u32 remaining_time;
  u32 last_preempted_at;
  u32 waiting_time;
  u32 response_time;
  /* End of "Additional fields here" */
};

TAILQ_HEAD(process_list, process);

u32 next_int(const char **data, const char *data_end)
{
  u32 current = 0;
  bool started = false;
  while (*data != data_end)
  {
    char c = **data;

    if (c < 0x30 || c > 0x39)
    {
      if (started)
      {
        return current;
      }
    }
    else
    {
      if (!started)
      {
        current = (c - 0x30);
        started = true;
      }
      else
      {
        current *= 10;
        current += (c - 0x30);
      }
    }

    ++(*data);
  }

  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

u32 next_int_from_c_str(const char *data)
{
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++]))
  {
    if (c < 0x30 || c > 0x39)
    {
      exit(EINVAL);
    }
    if (!started)
    {
      current = (c - 0x30);
      started = true;
    }
    else
    {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}

void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    int err = errno;
    perror("open");
    exit(err);
  }

  struct stat st;
  if (fstat(fd, &st) == -1)
  {
    int err = errno;
    perror("stat");
    exit(err);
  }

  u32 size = st.st_size;
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED)
  {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  const char *data_end = data_start + size;
  const char *data = data_start;

  *process_size = next_int(&data, data_end);

  *process_data = calloc(sizeof(struct process), *process_size);
  if (*process_data == NULL)
  {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  for (u32 i = 0; i < *process_size; ++i)
  {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
  }

  munmap((void *)data, size);
  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    return EINVAL;
  }
  struct process *data;
  u32 size;
  init_processes(argv[1], &data, &size);

  u32 quantum_length = next_int_from_c_str(argv[2]);

  struct process_list list;
  TAILQ_INIT(&list);

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;

  /* Your code here */

  for (u32 i = 0; i < size; i++) {
    data[i].remaining_time = data[i].burst_time;
    data[i].waiting_time = 0;
    data[i].response_time = -1; // although this is a u32, this value is used to identify whether it has been assigned yet
    data[i].last_preempted_at = -1; // same for this
  }

  // sort the processes
  for (u32 i = 0; i < size - 1; i++) {
    for (u32 j = i + 1; j < size; j++) {
      if (data[i].arrival_time > data[j].arrival_time || 
          (data[i].arrival_time == data[j].arrival_time && data[i].pid > data[j].pid)) {
        struct process temp = data[i];
        data[i] = data[j];
        data[j] = temp;
      }
    }
  }

  u32 cur_time = 0;
  u32 processes_completed = 0;
  u32 data_idx = 0;

  struct process *cur_proc = NULL;
  u32 slice_time_used = 0;
  while (!TAILQ_EMPTY(&list) || processes_completed < size) {
    // simulating time - has a new processes arrived to be queued?
    while (data_idx < size && data[data_idx].arrival_time <= cur_time) { // this should be done using a loop since processes might arrive at the same time
      // add to queue
      TAILQ_INSERT_TAIL(&list, &(data[data_idx]), pointers);
      data_idx++;
    }

    if (cur_proc != NULL && cur_proc->remaining_time > 0) {
      cur_proc->remaining_time--;
      slice_time_used++;
    }

    if (cur_proc == NULL || cur_proc->remaining_time == 0) {
      if (cur_proc != NULL) { // don't want to increment if we're sitting idle
        processes_completed++;
      }
      slice_time_used = 0;
      if (!TAILQ_EMPTY(&list)) {
        cur_proc = TAILQ_FIRST(&list);
        TAILQ_REMOVE(&list, cur_proc, pointers); // remove it from the waiting queue
        if (cur_proc->response_time == -1) { // if response time is 0, it hasn't been assigned yet
          cur_proc->response_time = cur_time - cur_proc->arrival_time;
          cur_proc->waiting_time += cur_proc->response_time;
        }
        else {
          cur_proc->waiting_time += cur_time - cur_proc->last_preempted_at;
        }
      }
    }

    else if (slice_time_used == quantum_length && cur_proc->remaining_time > 0) {
      // add cur proc to the end of the queue and bring in the one at the front
      cur_proc->last_preempted_at = cur_time;
      TAILQ_INSERT_TAIL(&list, cur_proc, pointers); // move current process to end
      cur_proc = TAILQ_FIRST(&list);
      TAILQ_REMOVE(&list, cur_proc, pointers);
      if (cur_proc->last_preempted_at != -1) {
        cur_proc->waiting_time += cur_time - cur_proc->last_preempted_at;
      }
      if (cur_proc->response_time == -1) { // if response time is 0, it hasn't been assigned yet
        cur_proc->response_time = cur_time - cur_proc->arrival_time;
        cur_proc->waiting_time = cur_proc->response_time; 
      }
      slice_time_used = 0;
    }
    cur_time += 1;
  }

  for (u32 i = 0; i < size; i++) {
    total_response_time += data[i].response_time;
    total_waiting_time += data[i].waiting_time;
  }
  /* End of "Your code here" */

  printf("Average waiting time: %.2f\n", (float)total_waiting_time / (float)size);
  printf("Average response time: %.2f\n", (float)total_response_time / (float)size);

  free(data);
  return 0;
}
