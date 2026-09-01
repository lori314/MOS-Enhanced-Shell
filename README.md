# Unix-like Shell Extensions for MOS

这是北航操作系统课程中基于 MOS（MIPS Operating System）完成的 Shell 扩展任务。

项目不是单独重写一个 Shell，而是在课程提供的 MOS 基线上，同时修改内核、系统调用、用户库和 `sh`，补充当前工作目录、环境变量、退出状态、命令链、命令替换和交互式行编辑等功能。

## 分层结构

```text
user/sh.c
   ↓
user library
   ↓
system-call wrappers
   ↓
kernel syscalls
   ↓
MOS kernel state
```

一些表面上属于 Shell 的功能需要跨层实现。例如 `cd` 不只是修改 `sh` 中的字符串，而是需要让进程保存 CWD，并让文件访问路径统一解析。

## 1. Current Working Directory

`struct Env` 中增加了：

```c
char env_cwd[MAXPATHLEN];
```

并增加：

- `SYS_getcwd`
- `SYS_chdir`

用户态的 `resolve_path()` 会将相对路径与当前 CWD 合并，并处理：

- `.`
- `..`
- 绝对路径
- 多级相对路径

文件层的 `open()` 也会先调用路径解析，因此 `cat`、`ls`、`rm` 等使用普通文件接口的程序可以直接使用相对路径。

Shell 中实现了：

```text
cd
pwd
```

## 2. Shell Variables

每个 `Env` 可以保存一组变量：

```text
name
value
flags
```

当前 flag 包括：

- `VAR_EXPORT`
- `VAR_READONLY`

对应系统调用包括：

- `SYS_set_var`
- `SYS_get_var`
- `SYS_unset_var`
- `SYS_get_var_by_index`

Shell 提供：

```text
declare
unset
$VAR expansion
```

子进程创建过程中还包含对可导出变量的继承处理。

## 3. Exit Status and Conditional Execution

为了支持：

```bash
cmd1 && cmd2
cmd1 || cmd2
cmd1 ; cmd2
```

项目扩展了进程退出与等待机制。

`Env` 中增加：

```c
int env_exit_status;
```

并增加 `ENV_ZOMBIE` 状态。子进程退出后先保留退出状态，父进程通过修改后的 `wait()` 取得状态，再回收对应环境。

`sh.c` 根据上一条命令的退出状态决定是否执行 `&&` / `||` 后的命令。

## 4. Command Substitution

支持反引号形式的命令替换：

```bash
echo `command`
```

`run_and_capture_output()` 的处理方式是：

1. 创建 pipe；
2. fork 子进程；
3. 将子进程 stdout 重定向到 pipe；
4. 执行反引号内部命令；
5. 父进程读取输出；
6. 将结果替换回原命令字符串。

因此这里同时使用了进程创建、管道、文件描述符复制和 wait/exit status。

## 5. Redirection

Shell 原有重定向基础上增加了：

```text
>>
```

当前实现会在打开文件后将文件偏移移动到末尾，再继续写入。

`user/lib/file.c` 中也对 `O_APPEND` 做了兼容处理：底层文件系统不直接接受该 flag 时，由用户库去掉 flag 并在打开后执行 `seek(fd, size)`。

## 6. Interactive Line Editing

`user/lib/shellio.c` 实现了交互式输入编辑。

支持：

- Up / Down：历史命令
- Left / Right：移动光标
- Backspace / Delete
- `Ctrl-A`：行首
- `Ctrl-E`：行尾
- `Ctrl-K`：删除到行尾
- `Ctrl-U`：删除到行首
- `Ctrl-W`：删除左侧单词

历史记录保存在：

```text
/.mos_history
```

实现使用 ANSI escape sequence 重绘当前命令行。

## 主要修改位置

```text
include/env.h
    Env CWD / exit status / variable state

include/syscall.h
    new syscall numbers

kern/env.c
    process creation and state inheritance

kern/syscall_all.c
    cwd / variable / process-state syscalls

user/lib/path.c
    relative-path resolution

user/lib/file.c
    path-aware open and append handling

user/lib/wait.c
    exit-status collection and zombie reaping

user/lib/shellio.c
    history and line editing

user/sh.c
    built-ins, expansion, command chain and substitution
```

## 核心扩展

```text
CWD + relative paths
environment/local variables
export / readonly flags
cd / pwd / declare / unset
persistent command history
cursor-aware readline
; / && / ||
backtick command substitution
>> append redirection
exit-status propagation
```

## 说明

仓库包含完整 MOS 课程代码，其中大量基础内核、文件系统和用户库来自课程框架；本 README 重点描述为完成 Shell 扩展任务所做的修改，而不是把整个 MOS 实现视为本项目新增代码。
