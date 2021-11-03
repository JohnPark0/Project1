# os_proj1
Proj1: Simple scheduling


Programming assignment #1 due by Nov. 14. 

# Scheduling simulation
# Basic requirement for cpu scheduling
## Parent process:
Create 10 child processes from a parent process
Parent process schedules child processes according to the round-robin scheduling policy.
Assume your own scheduling parameters: e.g. time quantum, and timer tick interval.
Parent process periodically receives ALARM signal by registering timer event.
Students may want to refer to setitimer system call.
The ALARM signal serves as periodic timer interrupt (or time tick). 
The parent process maintains run-queue and wait-queue. Run-queue holds child processes that are ready state. Wait-queue holds child processes that are not in ready state.
The parent process performs scheduling of its child processes: 
The parent process accounts for the remaining time quantum of all the child processes. 
The parent process gives time slice to the child process by sending ipc message through msgq
Students may want to refer to msgget, msgsnd, msgrcv system calls. Please note that there is IPC_NOWAIT flag.
The parent process accounts for the waiting time of all the child processes.

## Child process:<basic requirement for cpu scheduling>
A child process simulates the execution of a user process. Workload consists of infinite loop of dynamic CPU-burst and I/O-burst. The execution begins with two parameters: (cpu_burst, io_burst). Each value is randomly generated. 
When a user process receives the time slice from OS, the user process makes progress. To simulate this, the child process makes progress when it is in cpu-burst phase. Besides, the parent process sends ipc message to the currently running child process. When the child process takes ipc message from msgq, it decreases cpu-burst value. 

# optional requirement for io involvement
## The child process:
Children makes I/O requests after CPU-burst. To simulate this, child accounts for the remaining cpu-burst. If cpu-burst reaches to zero, the child sends ipc message to the parent process with the next io-burst time. 

## Parent process: <optional requirement for scheduling through processes with cpu-bursts and io-bursts>
The parent process receives ipc message from a child process, it checks whether the child begins io-burst. Then, the scheduler takes the child process out of the run-queue, and moves the child process to the wait-queue. (so that the child cannot get scheduled until it finishes io)
The parent process should remember io-burst value of the child process. Whenever time tick occurs, the parent process decreases the io-burst value. (for all the processes in the wait-queue)
When the remaining io-burst value of a process reaches to zero, the parent process puts the child process back to the run-queue. (so that it can be scheduled after I/O completion)
The scheduling is triggered by several events, for example: the expiry of time quantum (of a process), process makes I/O request (completing cpu-burst).

## The output of the program:<basic requirement for output>
Print out the scheduling operations in the following format:
(at time t, process pid gets cpu time, remaining cpu-burst) run-queue dump, wait-queue dump
Print out all the operations to the following file: schedule_dump.txt
Students would like to refer to the following C-library function and systemcall: sprintf, open, write, close
All the processes should run at least for 1min. 
Print out the scheduling operations during (0 ~ 10,000) time ticks. 
