// Host stand-in for libctru's <3ds.h>: the integer types the randomizer's
// logic sources use, plus inert file/key/tick declarations so the sources
// that also hold menu and SD-card code compile. Nothing here is called by
// the logic oracle.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32; typedef int64_t s64;
typedef volatile u32 vu32; typedef s32 Result; typedef u32 Handle; typedef u64 FS_Archive;
typedef struct { u32 type; u32 size; const void* data; } FS_Path;
enum { PATH_ASCII = 3 };
enum { FS_OPEN_READ = 1, FS_OPEN_WRITE = 2, FS_OPEN_CREATE = 4 };
enum { FS_WRITE_FLUSH = 1, FS_WRITE_UPDATE_TIME = 0x100 };
enum { FS_ATTRIBUTE_DIRECTORY = 1, FS_ATTRIBUTE_ARCHIVE = 0x100 };
enum { KEY_RIGHT = 1 << 4, KEY_LEFT = 1 << 5 };
#define R_SUCCEEDED(res) ((res) >= 0)
#define R_FAILED(res) ((res) < 0)
static inline FS_Path fsMakePath(u32 type, const void* path) { FS_Path p = { type, 0, path }; return p; }
static inline Result FSUSER_OpenFile(Handle* h, FS_Archive, FS_Path, u32, u32) { *h = 0; return -1; }
static inline Result FSUSER_CreateDirectory(FS_Archive, FS_Path, u32) { return -1; }
static inline Result FSUSER_DeleteFile(FS_Archive, FS_Path) { return -1; }
static inline Result FSFILE_Close(Handle) { return 0; }
static inline Result FSFILE_GetSize(Handle, u64* size) { *size = 0; return -1; }
static inline Result FSFILE_Read(Handle, u32* read, u64, void*, u32) { *read = 0; return -1; }
static inline Result FSFILE_Write(Handle, u32* written, u64, const void*, u32, u32) { *written = 0; return -1; }
static inline u64 svcGetSystemTick(void) { return 0; }
static inline Result romfsInit(void) { return -1; }
static inline Result romfsExit(void) { return 0; }
typedef unsigned int uint;
#ifdef __cplusplus
#include <iterator>
#endif
