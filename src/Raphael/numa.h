#pragma once
#include <chess/types.h>

#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef USE_LIBNUMA
    #include <numa.h>
    #include <pthread.h>
    #include <sched.h>
#endif



namespace raphael::numa {
#ifdef USE_LIBNUMA
/** Returns the mapping of cpuset for each NUMA node
 *
 * \returns cpu mappings
 */
std::span<const cpu_set_t> thread_mapping() {
    static const auto mapping = [] {
        const auto num_nodes = numa_max_node() + 1;
        std::vector<cpu_set_t> masks{};
        masks.reserve(num_nodes);

        for (i32 node = 0; node < num_nodes; node++) {
            auto* cpumask = numa_allocate_cpumask();
            if (numa_node_to_cpus(node, cpumask) != 0) {
                numa_free_cpumask(cpumask);
                throw std::runtime_error(
                    "failed to get CPU mask for NUMA node " + std::to_string(node)
                );
            }

            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);

            for (u32 cpu = 0; cpu < cpumask->size; cpu++)
                if (numa_bitmask_isbitset(cpumask, cpu)) CPU_SET(cpu, &cpuset);

            numa_free_cpumask(cpumask);
            masks.push_back(cpuset);
        }

        assert(masks.size() >= 1);
        return masks;
    }();

    return mapping;
}

/** Returns the number of NUMA nodes
 *
 * \returns number of NUMA nodes
 */
i32 node_count() { return static_cast<i32>(thread_mapping().size()); }

/** Returns the corresponding NUMA node
 *
 * \param thread_id thread to get NUMA node for
 * \returns corresponding NUMA node
 */
i32 get_node(i32 thread_id) { return static_cast<i32>(thread_id % node_count()); }

/** Binds a thread to the corresponding NUMA node
 *
 * \param thread_id thread to bind
 */
void bind_thread(i32 thread_id) {
    const auto node = get_node(thread_id);
    const auto handle = pthread_self();
    const auto* cpuset = &thread_mapping()[node];
    pthread_setaffinity_np(handle, sizeof(cpu_set_t), cpuset);
}

/** Initializes the NUMA support */
void init() {
    if (numa_available() < 0) throw std::runtime_error("NUMA not supported");
    thread_mapping();  // pre-compute
}

#else
/** Returns 1 as we assume there is only one NUMA node */
i32 node_count() { return 1; }

/** Returns 0 as we assume there is only one NUMA node */
i32 get_node(i32) { return 0; }

/** Does nothing as libnuma isn't linked */
void bind_thread(i32) {}

/** Does nothing as libnuma isnt linked */
void init() {}
#endif



template <typename T>
class NumaUniqueAllocation {
private:
    std::vector<T*> data_{};


public:
    /** Allocates a copy of T for every NUMA node */
    NumaUniqueAllocation() {
#ifdef USE_LIBNUMA
        const auto count = node_count();
        data_.reserve(count);

        for (i32 node = 0; node < count; node++) {
            // allocate storage on NUMA node and construct T
            auto* storage = numa_alloc_onnode(sizeof(T), node);
            auto* obj = new (storage) T();
            data_.push_back(obj);
        }
#else
        data_.reserve(1);
        data_.push_back(new T());
#endif
    }

    /** Destructs the allocated objects */
    ~NumaUniqueAllocation() {
        for (auto* obj : data_) {
#ifdef USE_LIBNUMA
            obj->~T();
            numa_free(obj, sizeof(T));
#else
            delete obj;
#endif
        }
        data_.clear();
    }

    /** Returns the object on that thread's NUMA node
     *
     * \param thread_id thread_id to get the corresponding object for
     * \returns the corresponding object
     */
    T* get(i32 thread_id) { return data_[get_node(thread_id)]; }
};
}  // namespace raphael::numa
