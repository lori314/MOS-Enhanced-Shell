# Unix-like Shell Extensions for MOS

这是北航操作系统课程中基于 MOS（MIPS Operating System）完成的 Shell 扩展任务。

在课程提供的 MOS 基线上，我同时改了内核、系统调用、用户库和 `sh`，补上当前工作目录、变量、退出状态、命令链、命令替换和交互式行编辑等功能。

## 修改范围

```text
user/sh.c
   ↓
user library
   ↓
system-call wrappers
   ↓
kernel syscalls
   ↓
MOS process state
```

一些看起来属于 Shell 的功能实际上需要跨层修改。例如 `cd` 不只是改变一个字符串，还需要让进程保存 CWD，并让用户态文件访问统一解析相对路径。

主要扩展集中在：

```text
include/env.h
include/syscall.h
kern/env.c
kern/syscall_all.c

user/lib/path.c
user/lib/file.c
user/lib/wait.c
user/lib/shellio.c
user/sh.c
```

## 1. Current Working Directory

`struct Env` 中增加：

```c
char env_cwd[MAXPATHLEN];
```

并增加：

- `SYS_getcwd`
- `SYS_chdir`

用户态的 `resolve_path()` 将相对路径与当前 CWD 合并，并处理：

- `.`
- `..`
- 绝对路径
- 多级相对路径

文件层的 `open()` 也会经过路径解析，因此 `cat`、`ls`、`rm` 等使用普通文件接口的程序可以直接使用相对路径。

Shell 中增加：

```text
cd
pwd
```

## 2. Shell Variables

每个 `Env` 保存一组变量：

```text
name
value
flags
```

当前 flag 包括：

- `VAR_EXPORT`
- `VAR_READONLY`

对应系统调用：

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

创建子进程时，可导出的变量会随 CWD 一起继承。

## 3. Exit Status and Conditional Execution

为了支持：

```bash
cmd1 && cmd2
cmd1 || cmd2
cmd1 ; cmd2
```

项目扩展了进程退出和等待机制。

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

`run_and_capture_output()` 的执行过程是：

```text
pipe
 ↓
fork
 ↓
child stdout -> pipe
 ↓
execute command
 ↓
parent reads output
 ↓
replace backtick expression
```

这部分同时使用了进程创建、管道、文件描述符复制和 wait / exit status。

## 5. Redirection

在原有重定向基础上增加：

```text
>>
```

Append 模式通过打开文件后移动文件偏移到末尾实现。

`user/lib/file.c` 也对 `O_APPEND` 做了兼容处理：当底层文件系统接口不直接接受该 flag 时，用户库去掉 flag，并在打开后执行 `seek(fd, size)`。

## 6. Interactive Line Editing

`user/lib/shellio.c` 实现交互式命令行编辑，支持：

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

命令行通过 ANSI escape sequence 重绘，并使用环形缓冲区维护历史记录。

## Build and Run

仓库沿用 MOS 课程工程的构建方式。根据 `include.mk`，默认使用：

```text
mips-linux-gnu-gcc / ld
qemu-system-mipsel
make
```

构建：

```bash
make
```

生成的内核和文件系统镜像位于 `target/`。

运行：

```bash
make run
```

Makefile 默认使用 QEMU Malta / MIPS 4Kc 环境启动 MOS。仓库中的 `.mos-this-lab` 当前设置为 Lab 6。

课程测试框架也保留在 `tests/`，Makefile 提供：

```bash
make test
```

具体能否直接运行取决于本机是否配置了课程要求的 MIPS 交叉编译工具链和 QEMU。

