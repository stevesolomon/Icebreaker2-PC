/*******************************************************************************
 *  platform/filesystem.cpp — Standard C++ file I/O implementation
 *  Part of the Icebreaker 2 SDL2 port (Windows + Linux/PortMaster)
 ******************************************************************************/
#include "filesystem.h"
#include <SDL.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
    #include <direct.h>   /* _mkdir on Windows */
    #include <shlobj.h>   /* SHGetFolderPathA */
    #include <windows.h>
#else
    #include <sys/stat.h> /* mkdir */
    #include <sys/types.h>
    #include <unistd.h>
    #include <pwd.h>
    #include <dirent.h>
    #include <strings.h>  /* strcasecmp */
#endif

/* ── Module state ────────────────────────────────────────────────────────── */

/* Set by InitFilesystem() to an absolute, trailing-slashed path so that asset
 * loads succeed regardless of the process's current working directory. This
 * matters on PortMaster handhelds where launcher scripts, gptokeyb wrappers
 * or the Wayland compositor can land us with a cwd we don't control. */
static std::string g_assets_dir = "assets/";

#define MAX_OPEN_FILES 32
static FILE *g_file_table[MAX_OPEN_FILES];
static int    g_next_file_handle = 1;

/* ── Filesystem init (sets the absolute assets directory) ────────────────── */

void InitFilesystem(void)
{
    /* 1. Explicit override wins (handy for testing or for a launcher that wants
     *    to point at an out-of-tree asset cache). */
    const char *env_override = std::getenv("IB2_ASSETS_DIR");
    if (env_override && *env_override) {
        g_assets_dir = env_override;
        if (g_assets_dir.back() != '/' && g_assets_dir.back() != '\\') {
            g_assets_dir.push_back('/');
        }
        SDL_Log("InitFilesystem: assets dir = %s (from IB2_ASSETS_DIR)",
                g_assets_dir.c_str());
        return;
    }

    /* 2. Default: <executable's directory>/assets/ */
    char *base = SDL_GetBasePath();
    if (base) {
        g_assets_dir = std::string(base) + "assets/";
        SDL_free(base);
        SDL_Log("InitFilesystem: assets dir = %s", g_assets_dir.c_str());
        return;
    }

    /* 3. Last-ditch: cwd-relative (matches old behaviour). */
    g_assets_dir = "assets/";
    SDL_Log("InitFilesystem: SDL_GetBasePath() failed; falling back to cwd-relative 'assets/'");
}

/* ── Path Translation ────────────────────────────────────────────────────── */

#ifndef _WIN32
static std::string ResolveCaseInsensitive(const std::string &path);
#endif

std::string TranslatePath(const char *threedo_path)
{
    if (!threedo_path) return "";

    std::string path(threedo_path);

    /* Replace 3DO boot path prefix with the absolute assets directory.
     * We strip the ${prefix}/ portion and prepend g_assets_dir, which is
     * always trailing-slashed. */
    static const char *const prefixes[] = {
        "$boot/IceFiles/",
        "$boot/icefiles/",
        "$boot/",
        "assets/"
    };

    for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        if (path.compare(0, strlen(prefixes[i]), prefixes[i]) == 0) {
            path = g_assets_dir + path.substr(strlen(prefixes[i]));
            break;
        }
    }

    /* Handle NVRAM paths */
    if (path.find("/NVRAM/") == 0 || path.find("NVRAM/") == 0) {
        std::string save_dir = GetSaveDir();
        size_t nvram_pos = path.find("NVRAM/");
        path = save_dir + path.substr(nvram_pos + 6);
    }

#ifdef _WIN32
    /* Normalize forward slashes to backslashes on Windows */
    std::replace(path.begin(), path.end(), '/', '\\');
#else
    /* On POSIX, transparently fall back to a case-insensitive match if the
     * exact-cased file isn't on disk. Cheap when the file exists. */
    path = ResolveCaseInsensitive(path);
#endif

    return path;
}

const char *GetAssetsDir(void)
{
    return g_assets_dir.c_str();
}

#ifndef _WIN32
/* ── Case-insensitive path resolution (POSIX) ────────────────────────────── *
 * The original 3DO codebase predates case-sensitive filesystems and the
 * source headers reference assets with mixed casing that doesn't always
 * match the filenames extracted from the ISO (e.g. `3DOLogo.cel` vs
 * `3DOlogo.cel`, `Menus/levelgrid/blue.cel` vs `Menus/LevelGrid/blue.cel`).
 *
 * Rather than mass-rename the files (and risk breaking other refs), we
 * walk each path component and pick the on-disk entry that matches case-
 * insensitively when an exact match fails. Results are cached so the cost
 * is paid only once per unique mismatch.
 */
static std::unordered_map<std::string, std::string> g_case_cache;

static bool ExistsExact(const std::string &path)
{
    struct stat st;
    return ::stat(path.c_str(), &st) == 0;
}

static std::string ResolveCaseInsensitive(const std::string &path)
{
    if (ExistsExact(path)) return path;

    auto cached = g_case_cache.find(path);
    if (cached != g_case_cache.end()) return cached->second;

    /* Split into components. Handle both leading-slash and relative paths. */
    std::vector<std::string> parts;
    size_t i = 0;
    std::string acc;
    if (!path.empty() && path[0] == '/') { acc = "/"; i = 1; }
    while (i < path.size()) {
        size_t j = path.find('/', i);
        if (j == std::string::npos) j = path.size();
        parts.push_back(path.substr(i, j - i));
        i = j + 1;
    }

    /* Walk one component at a time, opening the parent dir and looking for
     * a case-insensitive match for each segment. */
    std::string cur = acc.empty() ? std::string(".") : acc;
    std::string display = acc;  /* what we've built so far for the result */
    for (size_t p = 0; p < parts.size(); ++p) {
        const std::string &seg = parts[p];
        if (seg.empty()) continue;

        /* Fast path: exact case present? */
        std::string candidate = display.empty() ? seg : (display + (display.back()=='/' ? "" : "/") + seg);
        struct stat st;
        if (::stat(candidate.c_str(), &st) == 0) {
            display = candidate;
            cur = candidate;
            continue;
        }

        /* Slow path: scan parent directory case-insensitively. */
        std::string parent = display.empty() ? std::string(".") : display;
        DIR *dir = ::opendir(parent.c_str());
        if (!dir) {
            /* Parent doesn't exist (or no perms) — give up; return the
             * original path so the eventual fopen produces a sensible error. */
            return path;
        }
        std::string match;
        struct dirent *de;
        while ((de = ::readdir(dir)) != nullptr) {
            if (::strcasecmp(de->d_name, seg.c_str()) == 0) {
                match = de->d_name;
                break;
            }
        }
        ::closedir(dir);
        if (match.empty()) {
            /* No match at this level — return original path. */
            return path;
        }
        display = display.empty() ? match : (display + (display.back()=='/' ? "" : "/") + match);
        cur = display;
    }

    if (display != path) {
        SDL_Log("ResolveCaseInsensitive: '%s' -> '%s'", path.c_str(), display.c_str());
    }
    g_case_cache.emplace(path, display);
    return display;
}
#endif /* !_WIN32 */

std::string GetSaveDir(void)
{
#ifdef _WIN32
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
        std::string dir = std::string(appdata) + "\\Icebreaker2\\";
        _mkdir(dir.c_str()); /* create if it doesn't exist */
        return dir;
    }
    /* Fallback to current directory */
    return ".\\saves\\";
#else
    /* Helper: mkdir -p equivalent. Creates each path component in turn so
     * we don't silently fail on minimal Linux installs where ~/.local or
     * ~/.local/share doesn't already exist (or on PortMaster devices where
     * IB2_SAVE_DIR points to a deep path the launcher hasn't pre-created). */
    auto mkdir_p = [](const std::string &path) {
        if (path.empty()) return;
        std::string acc;
        acc.reserve(path.size());
        size_t i = 0;
        if (path[0] == '/') { acc.push_back('/'); i = 1; }
        while (i <= path.size()) {
            if (i == path.size() || path[i] == '/') {
                if (!acc.empty() && acc.back() != '/') {
                    mkdir(acc.c_str(), 0755); /* ignore EEXIST */
                }
                if (i < path.size()) acc.push_back('/');
            } else {
                acc.push_back(path[i]);
            }
            ++i;
        }
    };

    /* PortMaster launcher sets IB2_SAVE_DIR to a port-local conf dir so that
     * saves stay on the SD card alongside the game files. Honour it first. */
    const char *override_dir = std::getenv("IB2_SAVE_DIR");
    if (override_dir && *override_dir) {
        std::string dir(override_dir);
        if (dir.back() != '/') dir.push_back('/');
        mkdir_p(dir);
        return dir;
    }

    /* XDG Base Directory spec: $XDG_DATA_HOME, fall back to $HOME/.local/share */
    std::string base;
    const char *xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && *xdg) {
        base = xdg;
    } else {
        const char *home = std::getenv("HOME");
        if (!home || !*home) {
            struct passwd *pw = getpwuid(getuid());
            home = (pw && pw->pw_dir) ? pw->pw_dir : ".";
        }
        base = std::string(home) + "/.local/share";
    }

    std::string dir = base + "/Icebreaker2/";
    mkdir_p(dir);
    return dir;
#endif
}

/* ── File I/O ────────────────────────────────────────────────────────────── */

Item OpenDiskFile(const char *filename)
{
    std::string path = TranslatePath(filename);
    FILE *fp = fopen(path.c_str(), "rb+");
    if (!fp) {
        fp = fopen(path.c_str(), "rb");
    }
    if (!fp) return -1;

    int handle = g_next_file_handle++;
    if (handle >= MAX_OPEN_FILES) {
        fclose(fp);
        return -1;
    }
    g_file_table[handle] = fp;
    return handle;
}

void CloseDiskFile(Item file_item)
{
    if (file_item > 0 && file_item < MAX_OPEN_FILES && g_file_table[file_item]) {
        fclose(g_file_table[file_item]);
        g_file_table[file_item] = nullptr;
    }
}

int32 ReadDiskFile(Item file_item, void *buffer, int32 num_bytes)
{
    if (file_item <= 0 || file_item >= MAX_OPEN_FILES || !g_file_table[file_item])
        return -1;
    return (int32)fread(buffer, 1, num_bytes, g_file_table[file_item]);
}

int32 WriteDiskFile(Item file_item, void *buffer, int32 num_bytes)
{
    if (file_item <= 0 || file_item >= MAX_OPEN_FILES || !g_file_table[file_item])
        return -1;
    return (int32)fwrite(buffer, 1, num_bytes, g_file_table[file_item]);
}

Item CreateFile(const char *filename)
{
    std::string path = TranslatePath(filename);
    FILE *fp = fopen(path.c_str(), "wb+");
    if (!fp) return -1;

    int handle = g_next_file_handle++;
    if (handle >= MAX_OPEN_FILES) {
        fclose(fp);
        return -1;
    }
    g_file_table[handle] = fp;
    return handle;
}

Item CreateDiskFile(const char *filename, int32 size_bytes)
{
    (void)size_bytes; /* pre-allocation not needed on modern OS */
    return CreateFile(filename);
}

int32 SetEndOfFile(Item file_item, int32 offset)
{
    /* Not commonly needed on PC — stub */
    (void)file_item; (void)offset;
    return 0;
}

int32 GetFileBlockSize(Item file_item, int32 *blockSize)
{
    (void)file_item;
    if (blockSize) *blockSize = 1; /* byte-addressable on PC */
    return 0;
}

/* ── Block File I/O ──────────────────────────────────────────────────────── */

int32 OpenBlockFile(const char *filename, BlockFile *bf)
{
    if (!bf) return -1;
    std::string path = TranslatePath(filename);
    bf->fp = fopen(path.c_str(), "rb");
    bf->block_size = 1;
    return bf->fp ? 0 : -1;
}

void CloseBlockFile(BlockFile *bf)
{
    if (bf && bf->fp) {
        fclose(bf->fp);
        bf->fp = nullptr;
    }
}

Item CreateBlockFileIOReq(Item device, Item reply_port)
{
    (void)device; (void)reply_port;
    return 0;
}

/* ── IOReq / DoIO stubs (for timer device compatibility) ─────────────────── */

Item CreateIOReq(int32 a, int32 b, Item device, int32 d)
{
    (void)a; (void)b; (void)device; (void)d;
    return 0;
}

void DeleteIOReq(Item ioreq)
{
    (void)ioreq;
}

int32 DoIO(Item ioreq, IOInfo *info)
{
    (void)ioreq; (void)info;
    /* Timer reads are handled by platform/timer.cpp directly */
    return 0;
}

/* ── Memory Allocation ───────────────────────────────────────────────────── */

void *AllocMem(int32 size, uint32 type)
{
    (void)type;
    return calloc(1, size);
}

void FreeMem(void *ptr, int32 size)
{
    (void)size;
    free(ptr);
}

int32 MemBlockSize(void *ptr)
{
    (void)ptr;
    /* _msize is MSVC-specific; return 0 as fallback */
#ifdef _MSC_VER
    return ptr ? (int32)_msize(ptr) : 0;
#else
    return 0;
#endif
}

void ScavengeMem(void)
{
    /* No-op on modern OS — memory is managed by the runtime */
}

/* ── 3DO Utils3DO compatibility: ReadFile ─────────────────────────────────── */

int32 ReadFile(const char *filename, int32 size, void *buffer, int32 offset)
{
    std::string path = TranslatePath(filename);
    FILE *fp = fopen(path.c_str(), "rb");
    if (!fp) return -1;

    if (offset > 0)
        fseek(fp, offset, SEEK_SET);

    int32 bytes_read = (int32)fread(buffer, 1, size, fp);
    fclose(fp);
    return bytes_read;
}
