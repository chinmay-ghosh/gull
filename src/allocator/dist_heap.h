#ifndef _NVMM_DIST_HEAP_H_
#define _NVMM_DIST_HEAP_H_

#include <stddef.h>
#include <stdint.h>
#include <thread>
#include <mutex>
#include <pthread.h> // the reader-writer lock
#include <map>

#include "nvmm/error_code.h"
#include "nvmm/global_ptr.h"
#include "nvmm/shelf_id.h"
#include "nvmm/nvmm_fam_atomic.h"
#include "nvmm/nvmm_libpmem.h"
#include "nvmm/heap.h"

#include "shelf_mgmt/pool.h"

namespace nvmm{

class Ownership;
class FreeLists;    
class ShelfHeap;
    
// distributed/hierarchical heap
// - it relies on Pool to manage shelf membership; every shelf in the pool is a single-shelf heap
// - it manages heap ownership by itself; a process owns one or more single-shelf heaps
// - there is a backbround thread (per process) scanning freelists of the heaps the process owns and
// freeing space; the background thread also performs crash recovery for the distributed heap
// TODO: handle single-shelf heap crash recovery?
// TODO: have a background thread to periodically call Pool::Recover()???
class DistHeap : public Heap
{
public:
    DistHeap() = delete;
    DistHeap(PoolId pool_id);    
    ~DistHeap();

    // TODO: size is not used for now
    ErrorCode Create(size_t shelf_size);
    ErrorCode Destroy();
    bool Exist();

    ErrorCode Open();
    ErrorCode Close();
    bool IsOpen()
    {
        return is_open_;
    }
    
    GlobalPtr Alloc (size_t size);
    void Free (GlobalPtr global_ptr);

    // only for testing
    void *GlobalToLocal(GlobalPtr global_ptr);    
    // TODO: not yet implemented
    //GlobalPtr LocalToGlobal(void *addr);
    
private:
    static size_t const kShelfSize = Pool::kShelfSize; // for now, all shelves are of the same size
    static ShelfIndex const kMaxShelfCount = Pool::kMaxShelfCount; 
    static uint64_t const kWorkerSleepMicroSeconds = 500000;
    static ShelfIndex const kMaxOwnedHeap = 1; // max number of heaps one process can own
    
    // start/stop the background cleaner
    int StartWorker();
    int StopWorker();
    void BackgroundWorker();

    // helper functions to own/release a single-shelf heap
    // newonly == false: try to find an existing heap first before creating a new heap
    // newonly == true: always create a new heap
    bool AcquireShelfHeap(ShelfIndex &shelf_idx, bool newonly);
    bool ReleaseShelfHeap(ShelfIndex shelf_idx);

    // helper functions to open/close/recover a single-shelf heap
    ErrorCode OpenShelfHeap(ShelfIndex shelf_idx);
    ErrorCode CloseShelfHeap(ShelfIndex shelf_idx);
    ErrorCode RecoverShelfHeap(ShelfIndex shelf_idx);
    
    // helper functions to add/remove/lookup map_
    ShelfHeap *LookupShelfHeap(ShelfIndex shelf_idx);
    bool RegisterShelfHeap(ShelfIndex shelf_idx, ShelfHeap *shelf_heap);
    bool UnregisterShelfHeap(ShelfIndex shelf_idx);
    
    inline void ReadLock()
    {
        pthread_rwlock_rdlock(&rwlock_);
    }

    inline void ReadUnlock()
    {
        pthread_rwlock_unlock(&rwlock_);
    }

    inline void WriteLock()
    {
        pthread_rwlock_wrlock(&rwlock_);
    }

    inline void WriteUnlock()
    {
        pthread_rwlock_unlock(&rwlock_);
    }
    
    // pool
    PoolId pool_id_;
    Pool pool_;
    bool is_open_;

    // heap metadata
    Ownership *ownership_;
    FreeLists *freelists_;
    
    pthread_rwlock_t rwlock_; // protecting the mapping and the current active heap
    std::map<ShelfIndex, ShelfHeap*> map_; // for heaps that we own: ShelfIndex => ShelfHeap
    
    // for the background cleaner thread
    std::thread cleaner_thread_;
    std::mutex cleaner_mutex_;
    bool cleaner_running_;    
    bool cleaner_stop_;

    // TODO: gather freespace stats
    //size_t capacity_[ShelfIdMap::kMaxShelfCount];
    //size_t freespace_[ShelfIdMap::kMaxShelfCount];
};

} // namespace nvmm

#endif