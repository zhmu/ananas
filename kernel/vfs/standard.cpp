/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#include "kernel/types.h"
#include "kernel/bio.h"
#include "kernel/device.h"
#include "kernel/lib.h"
#include "kernel/mm.h"
#include "kernel/result.h"
#include "kernel/schedule.h" // XXX
#include "kernel/vfs/core.h"
#include "kernel/vfs/dentry.h"
#include "kernel/vfs/generic.h"
#include "kernel/vfs/icache.h"
#include "kernel/vfs/mount.h"

namespace {
    constexpr inline auto maxSymlinkLoops = 20;

    void MakeFile(struct VFS_FILE* file, int open_flags, DEntry& dentry)
    {
        memset(file, 0, sizeof(struct VFS_FILE));

        INode& inode = *dentry.d_inode;
        file->f_dentry = &dentry;
        file->f_offset = 0;
        file->f_flags = open_flags;
        if (inode.i_iops->fill_file != nullptr)
            inode.i_iops->fill_file(inode, file);
    }
}

Result vfs_open(Process* p, const char* fname, DEntry* cwd, int open_flags, int lookup_flags, struct VFS_FILE* out)
{
    DEntry* dentry;
    if (auto result = vfs_lookup(cwd, dentry, fname, lookup_flags); result.IsFailure())
        return result;
    MakeFile(out, open_flags, *dentry);
    KASSERT(dentry->d_inode != nullptr, "open without inode?");
    KASSERT(dentry->d_inode->i_iops != nullptr, "open without inode ops?");
    if (dentry->d_inode->i_iops->open != nullptr) {
        if (auto result = dentry->d_inode->i_iops->open(*out, p); result.IsFailure()) {
            dentry_deref(*dentry);
            return result;
        }
    }
    return Result::Success();
}

Result vfs_close(Process* p, struct VFS_FILE* file)
{
    if (file->f_dentry != nullptr) {
        auto dentry = file->f_dentry;
        // No need to check i_iops here, vfs_open() already does this
        if (dentry->d_inode != nullptr && dentry->d_inode->i_iops->close != nullptr) {
            if (auto result = dentry->d_inode->i_iops->close(*file, p); result.IsFailure()) {
                dentry_deref(*dentry);
                return result;
            }
        }
        dentry_deref(*dentry);
    }
    file->f_dentry = nullptr;
    file->f_device = nullptr;
    return Result::Success();
}

Result vfs_read(struct VFS_FILE* file, void* buf, size_t len)
{
    KASSERT(file->f_dentry != nullptr || file->f_device != nullptr, "vfs_read on nonbacked file");
    if (file->f_device != nullptr) {
        /* Device */
        if (file->f_device->GetCharDeviceOperations() == nullptr)
            return Result::Failure(EINVAL);
        else {
            return file->f_device->GetCharDeviceOperations()->Read(*file, buf, len);
        }
    }

    INode* inode = file->f_dentry->d_inode;
    if (inode == nullptr || inode->i_iops == nullptr)
        return Result::Failure(EINVAL);

    if (!vfs_is_filesystem_sane(inode->i_fs))
        return Result::Failure(EIO);

    if (S_ISLNK(inode->i_sb.st_mode)) {
        // Symbolic link
        if (inode->i_iops->read_link == nullptr)
            return Result::Failure(EINVAL);
        return inode->i_iops->read_link(*inode, static_cast<char*>(buf), len);
    }

    if (S_ISREG(inode->i_sb.st_mode)) {
        /* Regular file */
        if (inode->i_iops->read == nullptr)
            return Result::Failure(EINVAL);
        return inode->i_iops->read(file, buf, len);
    }

    if (S_ISDIR(inode->i_sb.st_mode)) {
        // Directory
        if (inode->i_iops->readdir == nullptr)
            return Result::Failure(EINVAL);
        return inode->i_iops->readdir(file, buf, len);
    }

    // What's this?
    return Result::Failure(EINVAL);
}

Result vfs_ioctl(Process* p, struct VFS_FILE* file, unsigned long request, void* args[])
{
    KASSERT(file->f_dentry != nullptr || file->f_device != nullptr, "vfs_ioctl on nonbacked file");
    if (file->f_device != nullptr) {
        // Device
        if (file->f_device == nullptr)
            return Result::Failure(EINVAL);
        return file->f_device->GetDeviceOperations().IOControl(p, request, args);
    }

    return Result::Failure(EINVAL);
}

Result vfs_write(struct VFS_FILE* file, const void* buf, size_t len)
{
    KASSERT(file->f_dentry != nullptr || file->f_device != nullptr, "vfs_write on nonbacked file");
    if (file->f_device != nullptr) {
        /* Device */
        if (file->f_device == nullptr || file->f_device->GetCharDeviceOperations() == nullptr)
            return Result::Failure(EINVAL);
        else
            return file->f_device->GetCharDeviceOperations()->Write(*file, buf, len);
    }

    INode* inode = file->f_dentry->d_inode;
    if (inode == nullptr || inode->i_iops == nullptr)
        return Result::Failure(EINVAL);

    if (S_ISDIR(inode->i_sb.st_mode)) {
        /* Directory */
        return Result::Failure(EINVAL);
    }

    if (!vfs_is_filesystem_sane(inode->i_fs))
        return Result::Failure(EIO);

    /* Regular file */
    if (inode->i_iops->write == nullptr)
        return Result::Failure(EINVAL);
    return inode->i_iops->write(file, buf, len);
}

Result vfs_seek(struct VFS_FILE* file, off_t offset)
{
    if (file->f_dentry == nullptr || file->f_dentry->d_inode == nullptr)
        return Result::Failure(EBADF);
    if (offset > file->f_dentry->d_inode->i_sb.st_size)
        return Result::Failure(ERANGE);
    file->f_offset = offset;
    return Result::Success();
}

namespace
{

Result vfs_follow_symlink(DEntry*& curdentry)
{
    if (!S_ISLNK(curdentry->d_inode->i_sb.st_mode)) return Result::Success();

    int symlink_loops = 0;
    while (S_ISLNK(curdentry->d_inode->i_sb.st_mode) && symlink_loops < maxSymlinkLoops) {
        INode& inode = *curdentry->d_inode;
        DEntry* followed_dentry;
        auto result = inode.i_iops->follow_link(inode, *curdentry, followed_dentry);
        if (result.IsFailure()) return result;

        dentry_deref(*curdentry);
        curdentry = followed_dentry;
        ++symlink_loops;
    }

    if (symlink_loops == maxSymlinkLoops) {
        dentry_deref(*curdentry);
        return Result::Failure(ELOOP);
    }
    return Result::Success();
}

/*
 * Internally used to perform a lookup from directory entry 'name' to an inode;
 * 'curdentry' is the dentry  to start the lookup relative to, or nullptr to
 * start from the root. This fills out the final dcache entry of the lookup; if
 * the last entry of the inode was resolved, final will be non-zero.
 *
 * On return of this function, result_dentry will always be referenced if it is not
 * nullptr; it is the responsibility of the caller to derefence it. This means that
 * _even if the lookup failed_, there can be dentry to report (more specicially, if
 * only the final part was not found but the path to it was, this is necessary when
 * creating new dentries)
 */
struct LookupResult {
    Result result;
    DEntry* result_dentry = nullptr;
    bool isFinal = false; // was the entire name resolved?
};

LookupResult
vfs_lookup_internal(DEntry* cur_dentry, const char* name, int flags)
{
    // See if we need to lookup relative to the root; if so, we must update the cur_dentry which
    // is our current lookup scope
    if (cur_dentry == nullptr || *name == '/') {
        const auto fs = vfs_get_rootfs();
        if (fs == nullptr)
            return { Result::Failure(ENOENT) }; // no root filesystem available
        if (*name == '/')
            name++;

        /* Start by looking up the root inode */
        cur_dentry = fs->fs_root_dentry;
        KASSERT(cur_dentry != nullptr, "no root dentry");
    }
    dentry_ref(*cur_dentry);

    /*
     * Split 'name' to lookup into 'next_lookup', which is the next item to
     * look for, and 'name_left', which points to the remainer of 'name' that
     * we still need to lookup. Once 'name_left' is nullptr, this means we are
     * at the final piece.
     */
    const char* name_left = name;
    const char* next_lookup;
    char tmp[VFS_MAX_NAME_LEN + 1];
    while (name_left != nullptr && *name_left != '\0' /* for trailing /-es */) {
        /*
         * Isolate the next part of the part we have to look up. Note that
         * we consider the input dentry as const, so we can't mess with it;
         * this is why we need to make copies in 'tmp'.
         */
        char* ptr = strchr(name_left, '/');
        if (ptr != nullptr) {
            // There's a slash in the path - must lookup next part
            strncpy(tmp, name_left, ptr - name_left);
            tmp[ptr - name_left] = '\0';
            name_left = ++ptr;
            next_lookup = tmp;
        } else {
            // No slashes; this is the final entry
            next_lookup = name_left;
            name_left = nullptr;
        }

        // Short-circut '.' which should exist in every directory
        if (strcmp(next_lookup, ".") == 0)
            continue;

        // Ensure the dentry points to something still sane
        if (!vfs_is_filesystem_sane(cur_dentry->d_fs)) {
            dentry_deref(*cur_dentry);
            return { Result::Failure(EIO) };
        }

        // If the current dentry is a symlink, follow it
        if (auto result = vfs_follow_symlink(cur_dentry); result.IsFailure()) {
            dentry_deref(*cur_dentry);
            return { result };
        }

        // We still need to look up 'next_lookup', so stop if we aren't in a
        // directory
        if (S_ISDIR(cur_dentry->d_inode->i_sb.st_mode) == 0) {
            dentry_deref(*cur_dentry);
            return { Result::Failure(ENOENT) };
        }

        // Look up 'next_lookup' using the dentry cache. This will always
        // return a valid dentry, but it could be negative (name does not exist).
        auto& next_dentry = dcache_lookup(*cur_dentry, next_lookup);
        if (next_dentry.d_flags & DENTRY_FLAG_NEGATIVE) {
            // Negative dentry means the name does not exist
            KASSERT(next_dentry.d_inode == nullptr, "negative lookup with inode?");
            dentry_deref(*cur_dentry);
            return { Result::Failure(ENOENT), cur_dentry, name_left == nullptr };
        }

        // If an inode is present, it means the lookup was succeeded
        if (next_dentry.d_inode != nullptr) {
            // Use the dentry as the next lookup base to find 'name_left'
            dentry_deref(*cur_dentry);
            cur_dentry = &next_dentry; // transfers ref
            continue;
        }

        // No inode available for 'next_lookup', get the current dentry to do
        // the lookup
        INode* inode;
        auto result = cur_dentry->d_inode->i_iops->lookup(*cur_dentry, inode, next_lookup);
        dentry_deref(*cur_dentry);
        if (result.IsFailure()) {
            // I/O failure make the entry cache negative
            next_dentry.d_flags |= DENTRY_FLAG_NEGATIVE;
            // Return where the lookup stopped; this can be used to create new entries
            // as needed
            return { result, &next_dentry, name_left == nullptr };
        }

        // Inode successfully read; connect it to the dentry
        dcache_set_inode(next_dentry, *inode);
        vfs_deref_inode(*inode);

        // Go one level deeper; we've already dereffed cur_dentry
        cur_dentry = &next_dentry;
    }

    // Handle with the last follow, if we need to
    if (cur_dentry != nullptr && (flags & VFS_LOOKUP_FLAG_NO_FOLLOW) == 0) {
        if (const auto result = vfs_follow_symlink(cur_dentry); result.IsFailure()) {
            dentry_deref(*cur_dentry);
            return { result, nullptr, true };
        }
    }

    // Lookup succeeded
    return { Result::Success(), cur_dentry, true };
}

} // unnamed namespace

/*
 * Called to perform a lookup from directory entry 'dentry' to an inode;
 * 'curinode' is the initial inode to start the lookup relative to, or nullptr to
 * start from the root.
 */
Result vfs_lookup(DEntry* parent, DEntry*& destentry, const char* dentry, int flags)
{
    destentry = nullptr;

    const auto lookupResult = vfs_lookup_internal(parent, dentry, flags);
    if (lookupResult.result.IsSuccess()) {
        destentry = lookupResult.result_dentry;
        return Result::Success();
    }

    // Lookup failed; dereference the destination if necessary
    if (lookupResult.result_dentry != nullptr)
        dentry_deref(*lookupResult.result_dentry);
    return lookupResult.result;
}

/*
 * Creates a new inode in parent; sets 'file' to the destination on success.
 */
Result vfs_create(DEntry* parent, const char* dentry, int file_flags, int mode, struct VFS_FILE* file)
{
    const auto lookupResult = vfs_lookup_internal(parent, dentry, 0);
    if (lookupResult.result.IsSuccess() || lookupResult.result.AsErrno() != ENOENT) {
        /*
         * A 'no file found' error is expected as we are creating the new file; we
         * are using the lookup code in order to obtain the cache item entry.
         */
        auto result = lookupResult.result;
        if (result.IsSuccess()) {
            // File already exists; drop the result and return EEXIST instead
            KASSERT(lookupResult.result_dentry->d_inode != nullptr, "successful lookup without inode");
            dentry_deref(*lookupResult.result_dentry);
            result = Result::Failure(EEXIST);
        }
        return result;
    }

    // If we couldn't look up all the way until the final entry, yield the error
    auto de = lookupResult.result_dentry;
    if (!lookupResult.isFinal) {
        dentry_deref(*de);
        return lookupResult.result;
    }

    /*
     * Excellent, the path works but the final entry doesn't. Mark the directory
     * entry as pending (as we are creating it) - this prevents it from going
     * away.
     */
    de->d_flags &= ~DENTRY_FLAG_NEGATIVE;
    KASSERT(
        de->d_parent != nullptr && de->d_parent->d_inode != nullptr,
        "attempt to create entry without a parent inode");
    auto parentinode = de->d_parent->d_inode;
    KASSERT(S_ISDIR(parentinode->i_sb.st_mode), "final entry isn't a directory");

    /* If the filesystem can't create inodes, assume the operation is faulty */
    if (parentinode->i_iops->create == nullptr) {
        de->d_flags |= DENTRY_FLAG_NEGATIVE;
        dentry_deref(*de);
        return Result::Failure(EINVAL);
    }

    if (!vfs_is_filesystem_sane(parentinode->i_fs)) {
        de->d_flags |= DENTRY_FLAG_NEGATIVE;
        dentry_deref(*de);
        return Result::Failure(EIO);
    }

    /* Dear filesystem, create a new inode for us */
    if (auto result = parentinode->i_iops->create(*parentinode, de, mode); result.IsFailure()) {
        /* Failure; remark the directory entry (we don't own it anymore) and report the failure */
        de->d_flags |= DENTRY_FLAG_NEGATIVE;
        dentry_deref(*de);
        return result;
    }
    MakeFile(file, file_flags, *de); // transfers ownership of reffed de
    return Result::Success();
}

/*
 * Grows a file to a given size.
 */
Result vfs_grow(struct VFS_FILE* file, off_t size)
{
    INode* inode = file->f_dentry->d_inode;
    KASSERT(inode->i_sb.st_size < size, "no need to grow");

    if (!vfs_is_filesystem_sane(inode->i_fs))
        return Result::Failure(EIO);

    /* Allocate a dummy buffer with the file data */
    char buffer[128];
    memset(buffer, 0, sizeof(buffer));

    /* Keep appending \0 until the file is done */
    off_t cur_offset = file->f_offset;
    file->f_offset = inode->i_sb.st_size;
    while (inode->i_sb.st_size < size) {
        size_t chunklen = (size - inode->i_sb.st_size) > sizeof(buffer)
                              ? sizeof(buffer)
                              : size - inode->i_sb.st_size;
        if (auto result = vfs_write(file, buffer, chunklen); result.IsFailure())
            return result;
    }
    file->f_offset = cur_offset;
    return Result::Success();
}

Result vfs_unlink(struct VFS_FILE* file)
{
    KASSERT(file->f_dentry != nullptr, "unlink without dentry?");

    /* Unlink is relative to the parent; so we'll need to obtain it */
    DEntry* parent = file->f_dentry->d_parent;
    if (parent == nullptr || parent->d_inode == nullptr)
        return Result::Failure(EINVAL);

    INode& inode = *parent->d_inode;
    if (inode.i_iops->unlink == nullptr)
        return Result::Failure(EINVAL);

    if (!vfs_is_filesystem_sane(inode.i_fs))
        return Result::Failure(EIO);

    if (auto result = inode.i_iops->unlink(inode, *file->f_dentry); result.IsFailure())
        return result;

    /*
     * Inform the dentry cache; the unlink operation should have removed it
     * from storage, but we need to make sure it cannot be found anymore.
     */
    dentry_unlink(*file->f_dentry);
    return Result::Success();
}

Result vfs_rename(struct VFS_FILE* file, DEntry* parent, const char* dest)
{
    KASSERT(file->f_dentry != nullptr, "rename without dentry?");

    /*
     * Renames are performed using the parent directory's inode - if our parent
     * does not have a rename function, we can avoid looking up things.
     */
    auto parent_dentry = file->f_dentry->d_parent;
    if (parent_dentry == nullptr || parent_dentry->d_inode == nullptr ||
        parent_dentry->d_inode->i_iops->rename == nullptr)
        return Result::Failure(EINVAL);

    /*
     * Look up the new location; we need a dentry to the new location for this to
     * work.
     */
    const auto lookupResult = vfs_lookup_internal(parent, dest, 0);
    if (lookupResult.result.IsSuccess() || lookupResult.result.AsErrno() != ENOENT) {
        /*
         * A 'no file found' error is expected as we are creating a new name here;
         * we are using the lookup code in order to obtain the cache item entry.
         */
        auto result = lookupResult.result;
        if (result.IsSuccess()) {
            // Destination already exists; can't rename - return EEXIST
            KASSERT(lookupResult.result_dentry->d_inode != nullptr, "successful lookup without inode");
            dentry_deref(*lookupResult.result_dentry);
            result = Result::Failure(EEXIST);
        }
        return result;
    }

    /* Request failed; if this wasn't the final entry, bail: parent path is not present */
    auto de = lookupResult.result_dentry;
    if (!lookupResult.isFinal) {
        dentry_deref(*de);
        return lookupResult.result;
    }

    /* Sanity checks */
    KASSERT(de->d_parent != nullptr, "found dest dentry without parent");
    KASSERT(de->d_parent->d_inode != nullptr, "found dest dentry without inode");

    /*
     * Okay, we need to rename file->f_dentry in parent_inode to de. First of all, ensure we
     * don't cross any filesystem boundaries. We can most easily do this by checking whether
     * the parent directories reside on the same backing filesystem.
     */
    INode* parent_inode = parent_dentry->d_inode;
    INode* dest_inode = de->d_parent->d_inode;
    if (parent_inode->i_fs != dest_inode->i_fs) {
        dentry_deref(*de);
        return Result::Failure(EXDEV);
    }

    /* All seems to be in order; ask the filesystem to deal with the change */
    if (auto result = parent_inode->i_iops->rename(*parent_inode, *file->f_dentry, *dest_inode, *de); result.IsFailure()) {
        dentry_deref(*de);
        return result;
    }

    /*
     * This worked; we should hook the new dentry up and throw away the old one.
     * No need to touch the refcount of the new dentry as we're giving our ref to
     * the file.
     */
    DEntry* old_dentry = file->f_dentry;
    file->f_dentry = de;
    dentry_deref(*old_dentry);
    return Result::Success();
}
