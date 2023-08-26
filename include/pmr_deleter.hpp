#pragma once

#include <memory_resource>
#include <type_traits>

#include <cassert>

template<class T>
struct PmrDelete {
private:
    std::pmr::memory_resource *m_pmr;
public:
    static_assert(!static_cast<bool>(std::is_array_v<T>), "Arrays are not supported currently");

    explicit PmrDelete(std::pmr::memory_resource *pmr) noexcept : m_pmr{pmr} {}

    explicit PmrDelete(std::nullptr_t) noexcept : m_pmr{nullptr} {}
    explicit PmrDelete() noexcept : PmrDelete(nullptr) {} // NOLINT

    PmrDelete(const PmrDelete<T>&) = delete;
    PmrDelete& operator=(const PmrDelete<T>&) = delete;
    PmrDelete(PmrDelete<T>&&) noexcept = default;
    PmrDelete& operator=(PmrDelete<T>&&) noexcept = default;

    ~PmrDelete() = default;

    void operator() (T *ptr) {
        assert(m_pmr != nullptr);
        std::pmr::polymorphic_allocator<T> alloc(m_pmr);
        alloc.destroy(ptr);
        alloc.deallocate(ptr, 1);
    }
};
