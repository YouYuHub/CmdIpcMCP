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
            "Start the named-pipe server (if not running) and create a terminal session, "
            "optionally executing first_command and returning its output. A non-existent "
            "pipe_name auto-creates a server rather than failing; shell state persists across "
            "run_pipe_command calls.\n"
            "\n"
            "=== CONTRACT: buffer model & polling (applies to all tools) ===\n"
            "- Each pipe has ONE shared ring output buffer: run_pipe_command appends; "
            "read_pipe_output reads BACKWARD from the tail (offset pagination). "
            "[buffer_total_bytes: N] from read_pipe_output = total data - compare with the "
            "previous call to detect new output.\n"
            "- Long-running command: 1) run_pipe_command(cmd, prompt=\"C:\\\\>|PS C:\\\\>|$\") "
            "waits for the prompt; 2) if it timed out first, poll read_pipe_output until "
            "buffer_total_bytes stops growing or the completion marker appears.\n"
            "- COMPLETION: prompt hit = command FINISHED. Output unchanged for ~1s is only the "
            "timeout fallback - the command may STILL be running (npm install, sleep, ssh...). "
            "Set a prompt for long commands; verify with read_pipe_output when unsure.\n"
            "- offset example: offset=0 = last 4096 bytes (tail); offset=4096 = the 4096 bytes "
            "BEFORE the tail; larger = further back; offset >= buffer size = empty.\n"
            "\n"
            "=== SERVER LIFECYCLE: job object & watchdog ===\n"
            "- The server is a detached --server process; its shells live in a job object with "
            "KILL_ON_JOB_CLOSE: killing the server (taskkill /PID from get_pipe_status) kills "
            "all its shells, no orphans.\n"
            "- Watchdog shuts the server down after 120 s of no client AND 60 s of no terminal "
            "output; any reconnect/output resets it. If get_pipe_status says 'not running', "
            "call setup_pipe or run_pipe_command again (fresh shell, previous state lost).\n"
            "- The visible terminal window is a separate --client process: closing it does NOT "
            "kill the shell or server; killing the server kills everything.\n"
            "\n"
            "=== CONVENTIONS ===\n"
            "- One command per call: join with && or ; or use a temp script; %VAR%/$VAR are "
            "evaluated by the shell; output is ANSI-stripped, trimmed to last 64 KB, invalid "
            "UTF-8 bytes -> U+FFFD.\n"
            "- CONCURRENCY: no concurrent commands on the SAME pipe_name (interleave, "
            "interrupt, duplicate output). Use different pipe_names for parallel work.\n"
            "- terminal_mode applies ONLY at session creation (first setup on a pipe); to "
            "switch modes use a fresh pipe_name.\n"
            "\n"
            "=== SHARED TERMINAL ===\n"
            "The session runs in a real PTY mirrored to a visible window on the user's "
            "desktop; commands and the user's keystrokes interleave in one shell. Interactive "
            "prompts (password, y/N, vim, ssh...): the user answers in that window - never "
            "inject secrets; confirm completion via read_pipe_output. terminal_mode: "
            "cmd.exe = classic prompt; powershell.exe = object-oriented shell.";
        // "Platform: currently Windows-only (uses Windows named pipes and ConPTY)."
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
          {"pipe_name", {{"type", "string"}, {"description", R"(Named pipe path, please strictly follow the format of \\.\pipe\name; e.g. \\.\pipe\powershell_server)"}, {"default", R"(\\.\pipe\default_server)"}}},
          {"terminal_mode", {{"type", "string"}, {"description", "Terminal command, e.g. cmd.exe /k chcp 65001 or powershell.exe (chcp 65001 = UTF-8). Only applies when the session is created - cannot change an existing terminal's mode."}, {"default", "cmd.exe /k chcp 65001"}}},
          {"first_command", {{"type", "string"}, {"description", "Single command to execute, e.g. dir or ls."}, {"default", ""}}},
          {"wait_milliseconds", {{"type", "integer"}, {"description", "Maximum wait time in ms; returns early when the prompt appears (prompt hit = command done). Without a prompt it is a timeout fallback - unchanged output does NOT mean the command finished."}, {"default", 5000}}},
          {"prompt", {{"type", "string"}, {"description", "Optional completion marker; multiple patterns separated by | (e.g. \"C:\\\\>|PS C:\\\\>|$\"); returns as soon as one appears in output."}, {"default", ""}}}
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
            "Execute one command in the persistent shell session (state from setup_pipe or "
            "previous calls). Conventions, buffer model, polling and completion semantics: "
            "setup_pipe's CONTRACT. "
            "Auto-starts the server with cmd.exe /k chcp 65001 if not running (a note is "
            "prepended to the output in that case). "
            "OUTPUT BOUNDARY: only bytes appended AFTER this command was submitted (incl. its "
            "own echo as the anchor) - never earlier commands' output; if the echo wrapped out "
            "of the ring buffer, only the new slice is returned; nothing produced yet = empty "
            "output, poll read_pipe_output for more. "
            "Interactive input (password, y/N, vim, ssh...): the user answers in the visible "
            "window - never inject secrets; confirm completion via read_pipe_output. ";
        // "Platform: currently Windows-only (uses Windows named pipes and ConPTY)."
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
          {"command", {{"type", "string"}, {"description", "Single command to execute, e.g. dir or ls. PowerShell: use ; not &"}}},
           {"pipe_name", {{"type", "string"}, {"description", R"(Named pipe path, please strictly follow the format of \\.\pipe\name; e.g. \\.\pipe\cmd_server)"}, {"default", R"(\\.\pipe\default_server)"}}},
           {"wait_milliseconds", {{"type", "integer"}, {"description", "Maximum wait time in ms. With prompt set: returns when the prompt appears (prompt hit = command done). Without prompt: pure timeout fallback - unchanged output for ~1s does NOT mean the command finished."}, {"default", 5000}}},
           {"prompt", {{"type", "string"}, {"description", "Optional completion marker; multiple patterns separated by | (e.g. \"C:\\\\>|PS C:\\\\>|$\"); returns as soon as one appears. Prompt hit = command done; set this for long-running commands."}, {"default", ""}}}
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
            "Read the pipe's shared ring buffer BACKWARD from the tail (buffer model: "
            "setup_pipe's CONTRACT). Read up to max_length bytes ending at (tail - offset): "
            "offset=0 = most recent data; offset=4096 = the 4096 bytes before the tail; larger "
            "= further back. "
            "Empty results are explicit ('Buffer is empty', or an offset-too-large note); the "
            "response includes [buffer_total_bytes: N] + [buffer_is_empty: true/false] to "
            "detect new output (compare buffer_total_bytes across calls). ANSI-stripped; "
            "invalid UTF-8 -> U+FFFD. Errors if the server is not running (no auto-start). "
            "The buffer mirrors the user's visible terminal window and updates live even while "
            "a command blocks on interactive input - observe echoed user input and state "
            "changes (e.g. a new prompt). ";
        //"Platform: currently Windows-only."
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
          {"pipe_name", {{"type", "string"}, {"description", R"(Named pipe path, e.g. \\.\pipe\pty_server)"}, {"default", R"(\\.\pipe\default_server)"}}},
          {"offset", {{"type", "integer"}, {"description", "Skip N bytes from the end before reading backward. offset=0 reads the tail (most recent data); offset=4096 reads the 4096 bytes before the tail; larger values page further back. If offset >= buffer size, returns empty (no data)."}, {"default", 0}}},
          {"max_length", {{"type", "integer"}, {"description", "Maximum bytes to read backward from the offset position. Hard limit: 65535 (unsigned short). Larger values are clamped."}, {"default", 4096}}},
          {"wait_milliseconds", {{"type", "integer"}, {"description", "Pre-read delay in ms (0 = read immediately). A simple Sleep before reading; use a positive value (e.g. 2000) to wait for a running command to finish producing output."}, {"default", 0}}}
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
            "Query the named-pipe SERVER process (--server) state for the given pipe. "
            "Read-only: never auto-starts a server, never creates a terminal. "
            "If the server is not running, returns success with 'not running' in the output "
            "(a status query is not an error). "
            "If running, reports: server_pid (PID of the --server process, NOT the MCP "
            "client), uptime, client_count (current connections, incl. this query), "
            "session_count, shared_session_id (0 = no terminal yet), output_buffer_bytes "
            "(total ring-buffer bytes across sessions), default_cwd. Use it to confirm the "
            "server is alive before/after long sessions, or to get the server PID for "
            "external tooling (e.g. taskkill /PID N - kills all its shells, see job object "
            "note in setup_pipe).";
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
          {"pipe_name", {{"type", "string"}, {"description", R"(Named pipe path, please strictly follow the format of \\.\pipe\name; e.g. \\.\pipe\cmd_server)"}, {"default", R"(\\.\pipe\default_server)"}}}
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
