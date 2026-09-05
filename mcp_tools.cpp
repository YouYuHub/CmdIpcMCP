#include "mcp_server.h"


namespace mcp_tools
{
    // ------------------------------------------------------------------
    // Tool: setup_pipe
    // ------------------------------------------------------------------
    ToolInfo setup_pipe_info = []() -> ToolInfo {
        ToolInfo info;
        info.name = "setup_pipe";
        info.description =
            "Start the pipe server (auto-creates if missing) and create/reuse the "
            "persistent terminal session; optionally runs first_command and returns its "
            "output. The session is a real shell shared with a visible window on the "
            "user's desktop: the user watches and can type; interactive prompts (password) "
            "are answered by the user there - never inject secrets. "
            "terminal_mode applies only at session creation (fresh pipe_name to switch "
            "shells); a server restart loses shell state.";
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
            {"pipe_name", {{"type", "string"}, {"description", R"(Pipe path \\.\pipe\name, default \\.\pipe\default_server)"}}},
            {"terminal_mode", {{"type", "string"}, {"description", "Shell command, only used at session creation; default cmd.exe /k chcp 65001"}}},
            {"first_command", {{"type", "string"}, {"description", "Optional first command, default is empty"}}},
            {"wait_milliseconds", {{"type", "integer"}, {"description", "Max wait in ms; default 5000"}}},
            {"prompt", {{"type", "string"}, {"description", "Match substring markers, separated by \"|\"; return early if one is found; does not support regular expressions. If not specified, return after a timeout has elapsed. Default is empty."}}}
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
            "Run one command in the persistent shell session (state persists; auto-starts "
            "the server if down). Returns only this command's new output, ANSI-stripped. "
            "Concurrent calls on the same pipe are queued. "
            "Unchanged output does NOT mean done, poll read_pipe_output. Line breaks = "
            "Enter (LF->CR): multi-line commands run line by line like typed.";
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
            {"command", {{"type", "string"}, {"description", "Command to run; line breaks = Enter. PowerShell: use ; not &"}}},
            {"pipe_name", {{"type", "string"}, {"description", R"(Pipe path \\.\pipe\name, default \\.\pipe\default_server)"}}},
            {"wait_milliseconds", {{"type", "integer"}, {"description", "Max wait in ms; default 5000"}}},
            {"prompt", {{"type", "string"}, {"description", "Match substring markers, separated by \"|\"; return early if one is found; does not support regular expressions. If not specified, return after a timeout has elapsed. Default is empty."}}}
          }},
          {"required", json::array({"command"})}
        };
        return info;
    }();

    // ------------------------------------------------------------------
    // Tool: read_pipe_output
    // ------------------------------------------------------------------
    ToolInfo read_output_info = [] {
        ToolInfo info;
        info.name = "read_pipe_output";
        info.description =
            "Read from the tail of the buffer (the last 64 KB) of the conversation output, reading a specified number of characters from the end towards the beginning; "
            "Response carries [buffer_total_bytes: N] - "
            "compare across calls to detect new output. Mirrors "
            "the user's terminal window live. Read-only: errors if server not running (no auto-start).";
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
            {"pipe_name", {{"type", "string"}, {"description", R"(Pipe path \\.\pipe\name)"}}},
            {"offset", {{"type", "integer"}, {"description", "Skip N bytes back from the tail, default 0"}}},
            {"max_length", {{"type", "integer"}, {"description", "Max bytes; default 4096"}}},
            {"wait_milliseconds", {{"type", "integer"}, {"description", "Delay before reading in ms; default 0"}}}
          }},
          {"required", json::array()}
        };
        return info;
    }();

    // ------------------------------------------------------------------
    // Tool: get_pipe_status
    // ------------------------------------------------------------------
    ToolInfo get_status_info = [] {
        ToolInfo info;
        info.name = "get_pipe_status";
        info.description =
            "Read-only server process status: server_pid, uptime, client_count, "
            "session_count, output_buffer_bytes, default_cwd. 'not running' = normal "
            "result, not an error.";
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
            {"pipe_name", {{"type", "string"}, {"description", R"(Pipe path \\.\pipe\name)"}}}
          }},
          {"required", json::array()}
        };
        return info;
    }();

    //// ------------------------------------------------------------------
    //// Tool: clear_pipe_history. Non-essential
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
