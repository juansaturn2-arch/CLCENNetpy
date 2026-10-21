/*
 * CLCENnetPy.c -- launcher for Python chess engines (PyPy3 / Python 3)   K7
 *
 * Arena / KULTRA load this EXE like any engine. It starts
 *     <interpreter> -u <script> [extra arguments]
 * and hands over the GUI's own stdin/stdout, so the UCI text goes straight
 * to the script -- nothing is parsed, copied or delayed. The engine's UCI
 * menu (Hash, Threads, NetFile, check boxes) appears in the GUI as usual.
 *
 * Generic: the script is found from the EXE's own name, so a copy renamed
 * to CLK7HomebrewPy.exe runs CLK7HomebrewPy.py.
 *
 * Search order
 *   script      : <name>.py next to the EXE, then engine\<name>.py
 *   interpreter : pypy\pypy3.exe next to the EXE, then engine\pypy\pypy3.exe,
 *                 then pypy3.exe on PATH, then python.exe on PATH
 *   <name>.ini (optional, next to the EXE) overrides either one:
 *       interpreter=pypy\pypy3.exe
 *       script=CLCENnetPy.py
 *       args=
 *   Relative paths in the ini are relative to the EXE's folder.
 *
 * All processes the engine starts (the Threads helpers) run in a Windows
 * Job Object with kill-on-close: when the GUI closes or kills the engine,
 * every PyPy process goes with it.
 *
 * Compile (MSYS2 MinGW64):
 *   gcc -O2 -s -static -municode -o CLCENnetPy.exe CLCENnetPy.c
 */

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define PATHLEN 4096

static wchar_t g_dir[PATHLEN];      /* folder of the EXE, with trailing '\' */
static wchar_t g_name[PATHLEN];     /* EXE name without ".exe"              */

static void fail(const wchar_t* msg, const wchar_t* detail) {
    /* the GUI shows stdout lines, so say it in UCI form as well */
    char a[PATHLEN], b[PATHLEN];
    WideCharToMultiByte(CP_UTF8, 0, msg, -1, a, sizeof(a), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, detail ? detail : L"", -1, b, sizeof(b), NULL, NULL);
    printf("info string launcher: %s %s\n", a, b);
    fprintf(stderr, "launcher: %s %s\n", a, b);
    fflush(stdout);
}

static int file_exists(const wchar_t* p) {
    DWORD a = GetFileAttributesW(p);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

/* out = path made absolute against the EXE folder when relative */
static void make_abs(const wchar_t* in, wchar_t* out) {
    int absolute = (in[0] && in[1] == L':') || in[0] == L'\\' || in[0] == L'/';
    if (absolute) wcsncpy(out, in, PATHLEN - 1);
    else _snwprintf(out, PATHLEN - 1, L"%ls%ls", g_dir, in);
    out[PATHLEN - 1] = 0;
}

static int try_local(const wchar_t* rel, wchar_t* out) {
    make_abs(rel, out);
    return file_exists(out);
}

static int try_path(const wchar_t* exe, wchar_t* out) {
    DWORD n = SearchPathW(NULL, exe, NULL, PATHLEN, out, NULL);
    return n > 0 && n < PATHLEN;
}

static void trim(wchar_t* s) {
    size_t n = wcslen(s), i = 0;
    while (n && (s[n - 1] == L' ' || s[n - 1] == L'\t' || s[n - 1] == L'\r' || s[n - 1] == L'\n')) s[--n] = 0;
    while (s[i] == L' ' || s[i] == L'\t') i++;
    if (i) memmove(s, s + i, (n - i + 1) * sizeof(wchar_t));
    n = wcslen(s);                                /* drop surrounding quotes */
    if (n >= 2 && s[0] == L'"' && s[n - 1] == L'"') {
        memmove(s, s + 1, (n - 2) * sizeof(wchar_t));
        s[n - 2] = 0;
    }
}

/* Read <name>.ini: interpreter=, script=, args=   (UTF-8 or ANSI text) */
static void read_ini(wchar_t* interp, wchar_t* script, wchar_t* args) {
    wchar_t path[PATHLEN];
    char line[PATHLEN];
    FILE* f;
    _snwprintf(path, PATHLEN - 1, L"%ls%ls.ini", g_dir, g_name);
    path[PATHLEN - 1] = 0;
    f = _wfopen(path, L"rb");
    if (!f) return;
    while (fgets(line, sizeof(line), f)) {
        wchar_t w[PATHLEN], *eq;
        char* p = line;
        if ((unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) p += 3;
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, p, -1, w, PATHLEN))
            MultiByteToWideChar(CP_ACP, 0, p, -1, w, PATHLEN);
        trim(w);
        if (!w[0] || w[0] == L';' || w[0] == L'#' || w[0] == L'[') continue;
        eq = wcschr(w, L'=');
        if (!eq) continue;
        *eq = 0;
        trim(w);
        trim(eq + 1);
        if (!_wcsicmp(w, L"interpreter")) make_abs(eq + 1, interp);
        else if (!_wcsicmp(w, L"script")) make_abs(eq + 1, script);
        else if (!_wcsicmp(w, L"args")) { wcsncpy(args, eq + 1, PATHLEN - 1); args[PATHLEN - 1] = 0; }
    }
    fclose(f);
}

/* the launcher's own command line after argv[0] */
static const wchar_t* extra_args(void) {
    const wchar_t* c = GetCommandLineW();
    if (*c == L'"') { c++; while (*c && *c != L'"') c++; if (*c) c++; }
    else { while (*c && *c != L' ' && *c != L'\t') c++; }
    while (*c == L' ' || *c == L'\t') c++;
    return c;
}

static void make_inheritable(DWORD which) {
    HANDLE h = GetStdHandle(which);
    if (h && h != INVALID_HANDLE_VALUE) SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
}

int wmain(void) {
    wchar_t exe[PATHLEN], interp[PATHLEN] = L"", script[PATHLEN] = L"", args[PATHLEN] = L"";
    wchar_t rel[PATHLEN], *cmd, *slash, *dot;
    size_t cmdlen;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jl;
    HANDLE job;
    DWORD code = 1;

    /* where am I, and what am I called? */
    if (!GetModuleFileNameW(NULL, exe, PATHLEN)) { fail(L"cannot read own path", NULL); return 1; }
    wcscpy(g_dir, exe);
    slash = wcsrchr(g_dir, L'\\');
    if (slash) slash[1] = 0; else g_dir[0] = 0;
    wcscpy(g_name, slash ? wcsrchr(exe, L'\\') + 1 : exe);
    dot = wcsrchr(g_name, L'.');
    if (dot && !_wcsicmp(dot, L".exe")) *dot = 0;

    read_ini(interp, script, args);

    if (!script[0]) {
        _snwprintf(rel, PATHLEN - 1, L"%ls.py", g_name);
        rel[PATHLEN - 1] = 0;
        if (!try_local(rel, script)) {
            _snwprintf(rel, PATHLEN - 1, L"engine\\%ls.py", g_name);
            if (!try_local(rel, script)) { fail(L"script not found:", rel + 7); return 1; }
        }
    }
    if (!file_exists(script)) { fail(L"script not found:", script); return 1; }

    if (!interp[0]) {
        if (!try_local(L"pypy\\pypy3.exe", interp) &&
            !try_local(L"engine\\pypy\\pypy3.exe", interp) &&
            !try_path(L"pypy3.exe", interp) &&
            !try_path(L"python.exe", interp)) {
            fail(L"no interpreter: put PyPy in a 'pypy' folder next to this EXE", NULL);
            return 1;
        }
    }
    if (!file_exists(interp)) { fail(L"interpreter not found:", interp); return 1; }

    /* "interp" -u "script" ini-args launcher-args */
    cmdlen = wcslen(interp) + wcslen(script) + wcslen(args) + wcslen(extra_args()) + 32;
    cmd = (wchar_t*)malloc(cmdlen * sizeof(wchar_t));
    if (!cmd) return 1;
    _snwprintf(cmd, cmdlen, L"\"%ls\" -u \"%ls\" %ls %ls", interp, script, args, extra_args());
    cmd[cmdlen - 1] = 0;

    /* kill-on-close job: the engine and its helpers die with the launcher */
    job = CreateJobObjectW(NULL, NULL);
    if (job) {
        ZeroMemory(&jl, sizeof(jl));
        jl.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &jl, sizeof(jl));
    }

    /* the child uses the GUI's pipes directly */
    make_inheritable(STD_INPUT_HANDLE);
    make_inheritable(STD_OUTPUT_HANDLE);
    make_inheritable(STD_ERROR_HANDLE);
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    ZeroMemory(&pi, sizeof(pi));

    /* start suspended, join the job, then run -- so helpers it spawns join too */
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_SUSPENDED, NULL, g_dir, &si, &pi)) {
        wchar_t err[64];
        _snwprintf(err, 63, L"(error %lu)", GetLastError());
        err[63] = 0;
        fail(L"could not start the interpreter", err);
        free(cmd);
        return 1;
    }
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    if (job) CloseHandle(job);                    /* ends any helper still running */
    free(cmd);
    return (int)code;
}
