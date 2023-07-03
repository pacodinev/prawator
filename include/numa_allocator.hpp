#pragma once

#include <config.h>

#ifdef WATOR_NUMA

#include <cassert>

#include <memory_resource>

#include <numa.h>
#include <stdexcept>
#include <string>
#include <unistd.h>

class NumaAllocator : public std::pmr::memory_resource {
private:
    unsigned m_numaNode;
    bool m_numaAvailable;

public:
    explicit NumaAllocator(unsigned numaNode) : m_numaNode(numaNode), m_numaAvailable(numa_available() >= 0) { }

    NumaAllocator(const NumaAllocator&) = delete;
    NumaAllocator& operator=(const NumaAllocator&) = delete;
    ~NumaAllocator() override = default;

    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        const NumaAllocator *nuo = dynamic_cast<const NumaAllocator*>(&other);
        if(nuo == nullptr) { return false; }
        if(!m_numaAvailable && !nuo->m_numaAvailable) { return true; }
        return m_numaNode == nuo->m_numaNode;
    }

    void do_deallocate( void* ptr, std::size_t bytes, std::size_t alignment ) override {
        if(m_numaAvailable) {
            numa_free(ptr, bytes);
        } else {
            std::pmr::memory_resource *def = std::pmr::new_delete_resource();
            def->deallocate(ptr, bytes, alignment);
        }
    }

    void* do_allocate( std::size_t bytes, std::size_t alignment ) override {
        if(m_numaAvailable) {
            assert( numa_pagesize() >= static_cast<long>(alignment));
            void* res = numa_alloc_onnode(bytes, static_cast<int>(m_numaNode));
            if(res == nullptr) {
                throw std::runtime_error("Cannot allocate memory on NUMA node " + 
                        std::to_string(m_numaNode));
            }
            return res;
        }

        std::pmr::memory_resource *def = std::pmr::new_delete_resource();
        return def->allocate(bytes, alignment);
       
    }
};

#endif
