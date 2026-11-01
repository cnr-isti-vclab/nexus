#ifndef GCACHE_CACHE_H
#define GCACHE_CACHE_H

#ifdef _MSC_VER

typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;

#else
#include <stdint.h>
#endif

#include <iostream>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <limits.h>
#include <thread>
#include <vector>
#include <list>
#include "token.h"
#include "provider.h"

/* this cache system enforce the rule that the items in a cache are always in all the cache below */
/* two mechanism to remove tokens from the cache:
      1) set token count to something low
      2) set maximum number of tokens in the provider
*/

/** Cache virtual base class. You are required to implement the pure virtual functions get, drop and size.
*/

namespace vcg {

template <typename Token> class Transfer;

template <typename Token>
class Cache: public Provider<Token> {

    struct TransferJob { Token *token; uint64_t reservation; };
    struct TransferResult { Token *token; int size; uint64_t reservation; };

public:
    ///true if this is the last cache (the one we use the data from)
    bool final;
    //if true the cache will exit at the first opportunity
    bool quit;
    ///keeps track of changes (if 1 then something was loaded or dropped
    std::atomic<int> new_data;
    ///callback for new_data
    void (*callback)(void *data);

    ///data is fetched from here
    Provider<Token> *input;

    ///threads running over cache...
    std::vector<Transfer<Token> *> transfers;

protected:
    ///max space available
    uint64_t s_max;
    ///current space used
    uint64_t s_curr;
    unsigned int transfer_threads;
    std::vector<std::thread> workers;
    std::deque<TransferJob> transfer_jobs;
    std::deque<TransferResult> transfer_results;
    std::vector<Token *> in_flight;
    uint64_t reserved_space;
    std::mutex transfer_mutex;
    std::condition_variable transfer_ready;
    std::condition_variable transfers_idle;
    unsigned int working_transfers;
    bool stop_workers;

public:
    Cache(uint64_t _capacity = INT_MAX):
        final(false), quit(false), new_data(false), input(NULL), s_max(_capacity), s_curr(0),
        transfer_threads(0), reserved_space(0), working_transfers(0), stop_workers(false) {}
    virtual ~Cache() { stopTransferWorkers(); }

    void setInputCache(Provider<Token> *p) { input = p; }
    uint64_t capacity() { return s_max; }
    uint64_t size() { return s_curr; }
    void setCapacity(uint64_t c) { s_max = c; }
    void setTransferThreads(unsigned int count) {
        std::lock_guard<std::mutex> locker(transfer_mutex);
        assert(workers.empty());
        transfer_threads = count;
    }

    void quiesce() {
        std::unique_lock<std::mutex> locker(transfer_mutex);
        transfers_idle.wait(locker, [this] {
            return transfer_jobs.empty() && working_transfers == 0;
        });
        locker.unlock();
        processTransferResults();
    }

    void pushPriorities() {
        Provider<Token>::pushPriorities();
        std::lock_guard<std::mutex> locker(transfer_mutex);
        for(Token *token : in_flight)
            token->pushPriority();
    }

    ///return true if the cache is waiting for priority to change
    bool newData() {
        int expected = 1;
        bool r = new_data.compare_exchange_strong(expected, 0); //if changed is 1, r is true
        return r;
    }

    ///empty the cache. Make sure no resource is locked before calling this.
    /// Require pause or stop before. Ensure there no locked item
    void flush() {
        //std::vector<Token *> tokens;
        {
            for(int i = 0; i < this->heap.size(); i++) {
                Token *token = &(this->heap[i]);
                //tokens.push_back(token);
                s_curr -= drop(token);
                //assert(!(token->count.load() >= Token::LOCKED));
                if(final) {
                    int expected = Token::READY;
                    token->count.compare_exchange_strong(expected, Token::CACHE);
                }
                input->heap.push(token);
            }
            this->heap.clear();
        }
        if(!s_curr == 0) {
            std::cerr << "Cache size after flush is not ZERO!\n";
            s_curr = 0;
        }
    }

    ///empty the cache. Make sure no resource is locked before calling this.
    /// Require pause or stop before. Ensure there no locked item
    template <class FUNCTOR> void flush(FUNCTOR functor) {
        std::vector<Token *> tokens;
        {
            int count = 0;
            std::lock_guard<std::mutex> locker(this->heap_lock);
            for(int k = 0; k < this->heap.size(); k++) {
                Token *token = &this->heap[k];
                if(functor(token)) { //drop it
                    tokens.push_back(token);
                    s_curr -= drop(token);
                    //assert(token->count.load() < Token::LOCKED);
                    if(final) {
                        int expected = Token::READY;
                        token->count.compare_exchange_strong(expected, Token::CACHE);
                    }
                } else
                    this->heap.at(count++) = token;
            }
            this->heap.resize(count);
            this->heap_dirty = true;
        }
        {
            std::lock_guard<std::mutex> locker(input->heap_lock);
            for(unsigned int i = 0; i < tokens.size(); i++) {
                input->heap.push(tokens[i]);
            }
        }
    }

    virtual void abort() {}

protected:
    ///return the space used in the cache by the loaded resource
    virtual int size(Token *token) = 0;
    ///returns amount of space used in cache -1 for failed transfer
    virtual int get(Token *token) = 0;
    ///return amount removed
    virtual int drop(Token *token) = 0;
    ///make sure the get function do not access token after abort is returned.




    ///called in as first thing in run()
    virtual void begin() {}
    virtual void middle() {}
    ///called in as last thing in run()
    virtual void end() {}

    ///[should be protected]
    void run() override {
        assert(input);
        /* basic operation of the cache:
          1) make room until eliminating an element would leave empty space.
          2) transfer first element of input_cache if
          cache has room OR first element in input has higher priority of last element */
        startTransferWorkers();
        begin();
        while(!this->quit) {
            input->check_queue.enter();     //wait for cache below to load something or priorities to change
            if(this->quit) {
                input->check_queue.leave();
                break;
            }

            processTransferResults();
            middle();

            bool unloaded = unload();
            bool loaded = !unloaded && load();
            if(unloaded || loaded) {
                if(unloaded || transfer_threads == 0) {
                    int expected = 0;
                    new_data.compare_exchange_strong(expected, 1);
                }
                input->check_queue.open();        //we signal ourselves to check again
            }
            input->check_queue.leave();
        }
        this->quit = false;                   //in case someone wants to restart;
        stopTransferWorkers();
        processTransferResults();
        end();
    }

    void startTransferWorkers() {
        std::lock_guard<std::mutex> locker(transfer_mutex);
        stop_workers = false;
        for(unsigned int i = 0; i < transfer_threads; ++i)
            workers.emplace_back([this] { transferWorker(); });
    }

    void stopTransferWorkers() {
        {
            std::lock_guard<std::mutex> locker(transfer_mutex);
            if(workers.empty()) return;
            stop_workers = true;
        }
        transfer_ready.notify_all();
        for(std::thread &worker : workers)
            if(worker.joinable()) worker.join();
        workers.clear();
        std::lock_guard<std::mutex> locker(transfer_mutex);
        stop_workers = false;
    }

    void transferWorker() {
        for(;;) {
                TransferJob job;
            {
                std::unique_lock<std::mutex> locker(transfer_mutex);
                transfer_ready.wait(locker, [this] { return stop_workers || !transfer_jobs.empty(); });
                if(transfer_jobs.empty()) {
                    if(stop_workers) return;
                    continue;
                }
                job = transfer_jobs.front();
                transfer_jobs.pop_front();
                ++working_transfers;
            }

            int loaded_size = -1;
            try { loaded_size = get(job.token); } catch(...) { loaded_size = -1; }

            {
                std::lock_guard<std::mutex> locker(transfer_mutex);
                transfer_results.push_back({job.token, loaded_size, job.reservation});
                --working_transfers;
                if(transfer_jobs.empty() && working_transfers == 0)
                    transfers_idle.notify_all();
            }
            input->check_queue.open();
        }
    }

    void processTransferResults() {
        std::deque<TransferResult> results;
        {
            std::lock_guard<std::mutex> locker(transfer_mutex);
            results.swap(transfer_results);
            for(const TransferResult &result : results) {
                auto it = std::find(in_flight.begin(), in_flight.end(), result.token);
                if(it != in_flight.end()) in_flight.erase(it);
                reserved_space -= result.reservation;
            }
        }

        for(const TransferResult &result : results) {
            if(result.size >= 0) {
                s_curr += result.size;
                {
                    std::lock_guard<std::mutex> locker(this->heap_lock);
                    if(final) result.token->count.fetch_add(1);
                    this->heap.push(result.token);
                    this->heap_dirty = true;
                }
                this->check_queue.open();
                new_data.store(1);
            } else {
                std::lock_guard<std::mutex> locker(input->heap_lock);
                input->heap.push(result.token);
                input->heap_dirty = true;
            }
        }
    }



    /** Checks wether we need to make room in the cache because of:
     size() - sizeof(lowest priority item) > capacity()
  **/
    bool unload() {
        Token *remove = NULL;
        //make room int the cache checking that:
        //1 we need to make room (capacity < current)
        if(size() + reserved_space > capacity()) {
            std::lock_guard<std::mutex> locker(this->heap_lock);

            //2 we have some element not in the upper caches (heap.size()  > 0
            if(this->heap.size()) {
                Token &last = this->heap.min();
                int itemsize = size(&last);

                //3 after removing the item, we are still full (avoids bouncing items)
                uint64_t projected = size() + reserved_space;
                if(projected > static_cast<uint64_t>(itemsize) &&
                    projected - static_cast<uint64_t>(itemsize) > capacity()) {

                    //4 item to remove is not locked. (only in last cache. you can't lock object otherwise)
                    if(!final) { //not final we can drop when we want
                        remove = this->heap.popMin();
                    } else {
                        int expected = Token::READY;
                        last.count.compare_exchange_strong(expected, Token::CACHE);
                        int last_count = last.count.load();
                        if(last_count <= Token::CACHE) { //was not locked and now can't be locked, remove it.
                            remove = this->heap.popMin();
                        } else { //last item is locked need to reorder stack
                            remove = this->heap.popMin();
                            this->heap.push(remove);
                            std::cout << "Reordering stack something (what?)\n";
                            return true;
                        }
                    }
                }
            }
        }

        if(remove) {
            {
                std::lock_guard<std::mutex> input_locker(input->heap_lock);
                int size = drop(remove);
                assert(size >= 0);
                s_curr -= size;
                input->heap.push(remove);
            }
            return true;
        }
        return false;
    }

    ///should be protected
    bool load() {
        Token *insert = NULL;
        Token *last = NULL;              //we want to lock only one heap at once to avoid deadlocks.

        /* check wether we have room (curr < capacity) or heap is empty.
       empty heap is bad: we cannot drop anything to make room, and cache above has nothing to get.
       this should not happen if we set correct cache sizes, but if it happens.... */
        {
            std::lock_guard<std::mutex> locker(this->heap_lock);
            this->rebuild();
            if(size() + reserved_space > capacity() && this->heap.size() > 0) {
                last = &(this->heap.min()); //no room, set last so we might check for a swap.
            }
        }

        {
            std::lock_guard<std::mutex> input_locker(input->heap_lock);
            input->rebuild();                                  //if dirty rebuild
            if(input->heap.size()) {                           //we need something in input to tranfer.
                Token &first = input->heap.max();
                int first_count = first.count.load();
                if(first_count > Token::REMOVE &&
                        (!last || first.priority > last->priority)) { //if !last we already decided we want a transfer., otherwise check for a swap
                    insert = input->heap.popMax();                 //remove item from heap, while we transfer it.
                }
            }
        }

        if(insert) {                                        //we want to fetch something
            if(transfer_threads == 0) {
                int loaded_size = get(insert);
                if(loaded_size >= 0) {
                    s_curr += loaded_size;
                    std::lock_guard<std::mutex> locker(this->heap_lock);
                    if(final) insert->count.fetch_add(1);
                    this->heap.push(insert);
                    this->check_queue.open();
                    return true;
                }
                std::lock_guard<std::mutex> locker(input->heap_lock);
                input->heap.push(insert);
                return false;
            }
            uint64_t reservation = static_cast<uint64_t>(std::max(0, size(insert)));
            {
                std::lock_guard<std::mutex> locker(transfer_mutex);
                uint64_t projected = s_curr + reserved_space;
                bool fits = reservation <= capacity() - std::min<uint64_t>(projected, capacity());
                bool may_overflow = reserved_space == 0 && last && insert->priority > last->priority;
                if(in_flight.size() >= transfer_threads || (!fits && !may_overflow) ||
                        (last && insert->priority <= last->priority)) {
                    std::lock_guard<std::mutex> input_locker(input->heap_lock);
                    input->heap.push(insert);
                    return false;
                }
                in_flight.push_back(insert);
                reserved_space += reservation;
				transfer_jobs.push_back({insert, reservation});
				std::cout << "jobs: " << transfer_jobs.size() << std::endl;
            }
            transfer_ready.notify_one();
            return true;
        }
        return false;
    }
};

} //namespace

#endif // GCACHE_H
