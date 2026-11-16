#include "mcp_server.h"


namespace mcp_tools
{
    // ------------------------------------------------------------------
    // Tool: pipe_setup
    // ------------------------------------------------------------------
    ToolInfo pipe_setup_info = []() -> ToolInfo {
        ToolInfo info;
        info.name = "pipe_setup";
        info.description =
            "Start the pipe server (auto-creates if missing) and create/reuse the persistent "
            "terminal session; optionally runs first_command. Returns as soon as the command "
            "finishes - wait_milliseconds is an upper bound, not a fixed sleep. The session is a "
            "real shell shared with a visible window on the user's desktop: the user watches and "
            "can type. Never inject secrets; let the user answer password prompts in that window. "
            "terminal_mode applies only at session creation (use a fresh pipe_name to switch "
            "shells); a server restart loses shell state. Result footer reports completion, "
            "exit_code, next_cursor and elapsed_ms.";
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
            {"pipe_name", {{"type", "string"}, {"description", R"(Pipe path \\.\pipe\name, default \\.\pipe\default_server)"}}},
            {"terminal_mode", {{"type", "string"}, {"description", "Shell command, only used at session creation; default cmd.exe /k chcp 65001"}}},
            {"first_command", {{"type", "string"}, {"description", "Optional first command; line breaks are submitted as Enter"}}},
            {"wait_milliseconds", {{"type", "integer"}, {"description", "Max wait in ms (upper bound); default 5000"}}},
            {"quiet_milliseconds", {{"type", "integer"}, {"description", "Silence needed to call a command finished; default 700. Raise for builds/installers with long quiet gaps"}}},
            {"completion_marker", {{"type", "string"}, {"enum", json::array({"auto", "on", "off"})}, {"description", "Exit-code detection: auto (default) enables a one-shot end marker when safe (single-line, local cmd/PowerShell, at a shell prompt); on forces it; off disables it"}}}
          }},
          {"required", json::array()}
        };
        return info;
    }();

    // ------------------------------------------------------------------
    // Tool: pipe_run_command
    // ------------------------------------------------------------------
    ToolInfo pipe_run_command_info = [] {
        ToolInfo info;
        info.name = "pipe_run_command";
        info.description =
            "Run a command in the persistent shell session (state persists; auto-starts the "
            "server if down). Returns only this command's new output, ANSI-stripped, as soon as "
            "it completes. Read 'completion' in the footer: marker/done = finished (marker also "
            "gives exit_code); awaiting_input = the program asks a question, answer with "
            "pipe_send_input or let the user type in the visible window; shell_exited = the shell "
            "died, call pipe_setup again; idle/timeout = may still run, resume with "
            "pipe_read_output(cursor=next_cursor) instead of re-running. Line breaks = Enter, so "
            "multi-line text runs line by line. Set as_script=true for nested quotes, "
            "here-strings, %VAR%/$var, loops or indented blocks: the text is then written "
            "verbatim to a temp script file and executed (no escaping needed, one exit_code), "
            "and the file is removed afterwards. Concurrent calls on the same pipe are queued.";
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
            {"command", {{"type", "string"}, {"description", "Command text; line breaks = Enter. With as_script=true this is the script body, written to the file exactly as given. PowerShell: use ; not &"}}},
            {"pipe_name", {{"type", "string"}, {"description", R"(Pipe path \\.\pipe\name, default \\.\pipe\default_server)"}}},
            {"wait_milliseconds", {{"type", "integer"}, {"description", "Max wait in ms (upper bound); default 5000 (15000 in script mode)"}}},
            {"quiet_milliseconds", {{"type", "integer"}, {"description", "Silence needed to call a command finished; default 700"}}},
            {"completion_marker", {{"type", "string"}, {"enum", json::array({"auto", "on", "off"})}, {"description", "Exit-code detection: auto (default) / on / off. Multi-line typed input keeps per-line Enter semantics and reports no exit_code (use as_script=true instead)"}}},
            {"as_script", {{"type", "boolean"}, {"description", "Write command to a temp script file and run it instead of typing it: no shell escaping, safe for any quotes/blank lines/indentation. Default false"}}},
            {"interpreter", {{"type", "string"}, {"enum", json::array({"auto", "cmd", "powershell", "python"})}, {"description", "Script kind; implies as_script. auto = follow the session shell (cmd -> .cmd, PowerShell -> .ps1 with -ExecutionPolicy Bypass); python writes a .py file"}}}
          }},
          {"required", json::array({"command"})}
        };
        return info;
    }();

    // ------------------------------------------------------------------
    // Tool: pipe_send_input
    // ------------------------------------------------------------------
    ToolInfo pipe_send_input_info = [] {
        ToolInfo info;
        info.name = "pipe_send_input";
        info.description =
            "Send raw input to the running session to answer an interactive prompt (y/n, a "
            "name, a menu choice) or to interrupt a command with key=ctrl+c. Use it when a "
            "previous call returned completion=awaiting_input or idle. Text is sent verbatim "
            "with a trailing Enter unless press_enter=false. Never send passwords or other "
            "secrets: ask the user to type them in the visible terminal window. Does not start "
            "a server or create a session.";
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
            {"text", {{"type", "string"}, {"description", "Text to type; line breaks = Enter"}}},
            {"key", {{"type", "string"}, {"description", "Control key: enter, tab, esc, backspace, ctrl+c, ctrl+d, ctrl+z, up, down, left, right, home, end, pageup, pagedown, delete"}}},
            {"press_enter", {{"type", "boolean"}, {"description", "Append Enter after text; default true"}}},
            {"pipe_name", {{"type", "string"}, {"description", R"(Pipe path \\.\pipe\name)"}}},
            {"wait_milliseconds", {{"type", "integer"}, {"description", "Max wait in ms for the reaction (upper bound); default 3000"}}},
            {"quiet_milliseconds", {{"type", "integer"}, {"description", "Silence needed to consider the reaction finished; default 700"}}}
          }},
          {"required", json::array()}
        };
        return info;
    }();

    // ------------------------------------------------------------------
    // Tool: pipe_read_output
    // ------------------------------------------------------------------
    ToolInfo pipe_read_output_info = [] {
        ToolInfo info;
        info.name = "pipe_read_output";
        info.description =
            "Read terminal output. Preferred: pass cursor=next_cursor from the previous call to "
            "get ONLY new bytes since then (no overlap, no guessing), plus a new next_cursor. In "
            "cursor mode wait_milliseconds waits for new data and returns early when it arrives. "
            "Without cursor it falls back to tail paging (offset skips N bytes back from the tail "
            "of the last 64 KB). Mirrors the user's terminal window live. Read-only: never starts "
            "a server or creates a session.";
        info.inputSchema = {
          {"type", "object"},
          {"properties", {
            {"pipe_name", {{"type", "string"}, {"description", R"(Pipe path \\.\pipe\name)"}}},
            {"cursor", {{"type", "string"}, {"description", "Absolute resume cursor (next_cursor from a previous call, as a string). Enables incremental mode; overrides offset"}}},
            {"offset", {{"type", "integer"}, {"description", "Tail-paging only: skip N bytes back from the tail, default 0"}}},
            {"max_length", {{"type", "integer"}, {"description", "Max bytes; default 4096. Cut on ANSI/UTF-8 safe boundaries"}}},
            {"wait_milliseconds", {{"type", "integer"}, {"description", "Cursor mode: max wait for new data. Paging mode: delay before reading. Default 0"}}}
          }},
          {"required", json::array()}
        };
        return info;
    }();

    // ------------------------------------------------------------------
    // Tool: pipe_get_status
    // ------------------------------------------------------------------
    ToolInfo pipe_get_status_info = [] {
        ToolInfo info;
        info.name = "pipe_get_status";
        info.description =
            "Read-only server process status: server_pid, uptime, client_count, session_count, "
            "output_buffer_bytes, default_cwd. 'not running' = normal result, not an error.";
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
    //// Tool: pipe_clear_history. Non-essential
    //// ------------------------------------------------------------------
    //ToolInfo pipe_clear_history_info = []
    //{
    //  ToolInfo info;
    //  info.name = "pipe_clear_history";
    //  info.description =
    //      "Clear the terminal output history buffer for the specified named pipe. "
    //      "This operation is irreversible — all previously accumulated terminal output "
    //      "will be permanently deleted from the buffer and cannot be retrieved. "
    //      "The shell process itself is NOT affected (it continues running). "
    //      "Absolute cursors stay monotonic across a clear. "
    //      "If the pipe server is not running, this tool returns an error.";
    //  info.inputSchema = {
    //      {"type", "object"},
    //      {"properties", {{"pipe_name", {{"type", "string"}, {"description", R"(Named pipe path, e.g. \\.\pipe\cmd_server)"}, {"default", R"(\\.\pipe\default_server)"}}}}},
    //      {"required", json::array()}};
    //  return info;
    //}();
}
