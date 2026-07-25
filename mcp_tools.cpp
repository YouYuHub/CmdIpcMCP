#include "mcp_server.h"

/*
#include "nlohmann/json.hpp"

using json = nlohmann::json;

struct ToolInfo
{
  std::string name;
  std::string description;
  json inputSchema;
};
*/

namespace mcp_tools
{
// ------------------------------------------------------------------
// Tool: setup_pipe
// ------------------------------------------------------------------
ToolInfo setup_pipe_info = []() -> ToolInfo {
    ToolInfo info;
    info.name = "setup_pipe";
    info.description =
        "Start the named-pipe server (if not running) and create a terminal session, optionally "
        "executing first_command and returning its output. A non-existent pipe_name auto-creates a "
        "new server on that name rather than failing. The shell state persists across later "
        "run_pipe_command calls on the same pipe_name. "
        "Conventions for all tools on this server: one command per call - join multi-statement "
        "scripts with && or ; or use a temp script file; shell variables (%VAR%, $VAR) are "
        "evaluated by the shell, not by these tools; output is stripped of ANSI escapes and "
        "trimmed to the last 64 KB. "
        "terminal_mode: cmd.exe gives the classic Windows prompt; powershell.exe the object-oriented shell. "
        "Shared terminal & human-in-the-loop: the session runs in a real PTY mirrored to a visible "
        "terminal window on the user's desktop (opened automatically). That window and these tools "
        "share one stdin/stdout: commands sent via run_pipe_command and the user's keystrokes "
        "interleave in one shell. Any interactive prompt (password, y/N, pager, vim, ssh login, ...) "
        "can be answered by the user directly in that window. Do NOT inject passwords or other "
        "sensitive input yourself; if a command waits on interactive input, leave it for the user, "
        "then confirm via read_pipe_history (e.g. a new shell prompt appearing). This hand-off is intentional.";
    // "Platform: currently Windows-only (uses Windows named pipes and ConPTY)."
    info.inputSchema = {
      {"type", "object"},
      {"properties", {
      {"pipe_name", {{"type", "string"}, {"description", R"(Named pipe path, e.g. \\.\pipe\powershell_server)"}, {"default", R"(\\.\pipe\default_server)"}}},
      {"terminal_mode", {{"type", "string"}, {"description", "Terminal command, e.g. cmd.exe /k chcp 65001 or powershell.exe. The terminal is launched with chcp 65001 for UTF-8 support."}, {"default", "cmd.exe /k chcp 65001"}}},
      {"first_command", {{"type", "string"}, {"description", "Single command to execute, e.g. dir or ls."}, {"default", ""}}},
      {"wait_milliseconds", {{"type", "integer"}, {"description", "Maximum wait time in ms; returns early if output stabilises (unchanged for ~1s) or prompt string is found in output."}, {"default", 30000}}},
      {"prompt", {{"type", "string"}, {"description", "Optional prompt string; returns as soon as this string appears in output (e.g. root@host:~#). Can be used to detect command completion."}, {"default", ""}}}
      }},
      {"required", json::array()}
    };
    return info;
}();

// ------------------------------------------------------------------
// Tool: run_pipe_command
// ------------------------------------------------------------------
ToolInfo run_command_info = [] {
    ToolInfo info;
    info.name = "run_pipe_command";
    info.description =
        "Execute one command in the named-pipe terminal session, inheriting the persistent shell "
        "state from setup_pipe or previous calls. Shared conventions (single command per call, "
        "shell variables, ANSI stripping, 64 KB trim) are described in setup_pipe. "
        "If the server is not yet running on pipe_name, it is auto-started with "
        "cmd.exe /k chcp 65001 (a note is prepended to the output in that case). "
        "Interactive hand-off: the command is typed into a PTY shared with the user's visible "
        "terminal window. If it blocks on interactive input (password, y/N, --more--, vim, ...), "
        "do NOT supply secrets yourself; the user can answer directly in that window. Wait, then "
        "use read_pipe_history to detect the resulting state change (e.g. a new shell prompt). ";
    // "Platform: currently Windows-only (uses Windows named pipes and ConPTY)."
    info.inputSchema = {
      {"type", "object"},
      {"properties", {
      {"command", {{"type", "string"}, {"description", "Single command to execute, e.g. dir or ls. PowerShell: use ; not &."}}},
       {"pipe_name", {{"type", "string"}, {"description", R"(Named pipe path, e.g. \\.\pipe\cmd_server)"}, {"default", R"(\\.\pipe\default_server)"}}},
       {"wait_milliseconds", {{"type", "integer"}, {"description", "Maximum wait time in ms; returns early if output stabilises (unchanged for ~1s) or prompt string is found. Higher values allow longer-running commands to complete."}, {"default", 30000}}},
       {"prompt", {{"type", "string"}, {"description", "Optional prompt string; returns as soon as this appears in output (e.g. C:\\Users\\YouYu>). Provides faster completion detection."}, {"default", ""}}}
       }},
       {"required", json::array({"command"})}
    };
    return info;
}();

// ------------------------------------------------------------------
// Tool: read_pipe_history
// ------------------------------------------------------------------
ToolInfo history_info = [] {
    ToolInfo info;
    info.name = "read_pipe_history";
    info.description =
        "Read up to max_length bytes backward from the terminal output buffer, skipping the last "
        "offset bytes first (offset=0 reads the tail; larger offsets read earlier blocks). "
        "Empty results are reported explicitly ('Buffer is empty', or an offset-too-large note), "
        "and the response includes [buffer_total_bytes: N] plus [buffer_is_empty: true/false] to "
        "distinguish an empty buffer from an empty slice. Output is stripped of ANSI escapes; "
        "errors if the pipe server is not running. "
        "The buffer mirrors the user's visible terminal window and updates live, even while a "
        "command is blocked on interactive input - use it to observe user input echoed there and "
        "state changes such as a new shell prompt appearing. ";
    //"Platform: currently Windows-only."
    info.inputSchema = {
      {"type", "object"},
      {"properties", {
      {"pipe_name", {{"type", "string"}, {"description", R"(Named pipe path, e.g. \\.\pipe\pty_server)"}, {"default", R"(\\.\pipe\default_server)"}}},
      {"offset", {{"type", "integer"}, {"description", "Skip N bytes from the end before reading backward. offset=0 reads the tail (most recent data); positive values read earlier blocks. If offset >= buffer size, returns empty (no data)."}, {"default", 0}}},
      {"max_length", {{"type", "integer"}, {"description", "Maximum bytes to read backward from the offset position. Hard limit: 65535 (unsigned short). Larger values are clamped."}, {"default", 4096}}},
      {"wait_milliseconds", {{"type", "integer"}, {"description", "Pre-read delay in ms (0 = read immediately). A simple Sleep before reading; use a positive value (e.g. 2000) to wait for a running command to finish producing output."}, {"default", 0}}}
      }},
      {"required", json::array()}
    };
    return info;
}();

//// ------------------------------------------------------------------
//// Tool: clear_pipe_history
//// ------------------------------------------------------------------
//ToolInfo clear_history_info = []
//{
//  ToolInfo info;
//  info.name = "clear_pipe_history";
//  info.description =
//      "Clear the terminal output history buffer for the specified named pipe. "
//      "This operation is irreversible — all previously accumulated terminal output "
//      "will be permanently deleted from the buffer and cannot be retrieved. "
//      "The shell process itself is NOT affected (it continues running). "
//      "This is useful for freeing memory from long-running sessions, "
//      "or for resetting the history state between test runs. "
//      "If the pipe server is not running, this tool returns an error.";
//  info.inputSchema = {
//      {"type", "object"},
//      {"properties", {{"pipe_name", {{"type", "string"}, {"description", R"(Named pipe path, e.g. \\.\pipe\cmd_server)"}, {"default", R"(\\.\pipe\default_server)"}}}}},
//      {"required", json::array()}};
//  return info;
//}();
}
