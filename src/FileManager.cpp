#include "FileManager.h"
#include "ScanJob.h"
#include "Server.h"
#include "Filter.h"
#include "Project.h"

FileManager::FileManager()
    : mLastReloadTime(0)
{
    mWatcher.added().connect(std::bind(&FileManager::onFileAdded, this, std::placeholders::_1));
    mWatcher.removed().connect(std::bind(&FileManager::onFileRemoved, this, std::placeholders::_1));
}

void FileManager::init(const shared_ptr<Project> &proj)
{
    mProject = proj;
    reload();
}

void FileManager::reload()
{
    mLastReloadTime = Rct::monoMs();
    shared_ptr<Project> project = mProject.lock();
    assert(project);
    shared_ptr<ScanJob> job(new ScanJob(project->path()));
    job->finished().connectAsync(std::bind(&FileManager::onRecurseJobFinished, this, std::placeholders::_1));
    Server::instance()->threadPool()->start(job);
}

void FileManager::onRecurseJobFinished(Set<Path> paths)
{
    bool emitJS = false;
    {
        std::lock_guard<std::mutex> lock(mMutex); // ### is this needed now?
        Set<Path> old;
        std::swap(mJSFiles, old);

        shared_ptr<Project> project = mProject.lock();
        assert(project);
        FilesMap &map = project->files();
        map.clear();
        mWatcher.clear();
        for (Set<Path>::const_iterator it = paths.begin(); it != paths.end(); ++it) {
            if (it->endsWith(".js"))
                mJSFiles.insert(*it);
            const Path parent = it->parentDir();
            if (parent.isEmpty()) {
                error() << "Got empty parent here" << *it;
                continue;
            }
            assert(!parent.isEmpty());
            Set<String> &dir = map[parent];
            if (dir.isEmpty())
                watch(parent);
            dir.insert(it->fileName());
        }
        assert(!map.contains(""));
        emitJS = old != mJSFiles;
    }
    if (emitJS)
        mJSFilesChanged();
}

void FileManager::onFileAdded(const Path &path)
{
    bool emitJS = false;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (path.isEmpty()) {
            error("Got empty file added here");
            return;
        }
        const Filter::Result res = Filter::filter(path);
        switch (res) {
        case Filter::Directory:
            reload();
            return;
        case Filter::Filtered:
            return;
        default:
            break;
        }

        shared_ptr<Project> project = mProject.lock();
        assert(project);
        FilesMap &map = project->files();
        const Path parent = path.parentDir();
        if (!parent.isEmpty()) {
            Set<String> &dir = map[parent];
            if (dir.isEmpty())
                watch(parent);
            dir.insert(path.fileName());
            emitJS = path.endsWith(".js");
        } else {
            error() << "Got empty parent here" << path;
        }
        assert(!map.contains(Path()));
    }
    if (emitJS)
        mJSFilesChanged();
}

void FileManager::onFileRemoved(const Path &path)
{
    std::lock_guard<std::mutex> lock(mMutex);
    shared_ptr<Project> project = mProject.lock();
    FilesMap &map = project->files();
    if (map.contains(path)) {
        reload();
        return;
    }
    const Path parent = path.parentDir();
    if (map.contains(parent)) {
        Set<String> &dir = map[parent];
        dir.remove(path.fileName());
        if (dir.isEmpty()) {
            mWatcher.unwatch(parent);
            map.remove(parent);
        }
    }
}

static inline bool startsWith(const Path &left, const Path &right)
{
    assert(!left.isEmpty());
    return !right.isEmpty() && left.startsWith(right);
}

bool FileManager::contains(const Path &path) const
{
    std::lock_guard<std::mutex> lock(mMutex);
    shared_ptr<Project> proj = mProject.lock();
    if (!proj)
        return false;
    if (startsWith(path, proj->path()))
        return true;
    const Path p = Path::resolved(path);
    if (p != path && startsWith(path, proj->path()))
        return true;
    return false;
}

Set<Path> FileManager::jsFiles() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mJSFiles;
}

void FileManager::watch(const Path &path)
{
    if (!(Server::instance()->options().options & Server::NoFileManagerWatch)
        && !path.contains("/.git/") && !path.contains("/.svn/") && !path.contains("/.cvs/")) {
        mWatcher.watch(path);
    }
}
