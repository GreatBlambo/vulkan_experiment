#pragma once
#include <vector>
#include <queue>

#include "utils.h"

namespace Handle
{
// Shit ass implementation of a weak ref manager
// uses std for convenience, profile this if you think it's actually a problem

#define WEAKREF_MAX_INDEX ~((uint32_t) 0)
#define WEAKREF_MAX_GENERATION ~((uint32_t) 0)
#define WEAKREF_INVALID_INDEX WEAKREF_MAX_INDEX
#define WEAKREF_INVALID_GENERATION WEAKREF_MAX_GENERATION

#define STRONGLY_TYPED_WEAKREF(name)                                                             \
    struct name                                                                                  \
    {                                                                                            \
        Handle::WeakRef       handle;                                                            \
        inline bool           operator==(const name& rhs) const { return handle == rhs.handle; } \
        const inline bool     operator!=(const name& rhs) const { return handle != rhs.handle; } \
        const inline uint64_t u64() const { return handle.u64(); }                                     \
    };

struct WeakRef
{
    uint32_t index      = WEAKREF_INVALID_INDEX;
    uint32_t generation = WEAKREF_INVALID_GENERATION;

    inline uint64_t u64() const { return ((uint64_t) generation << 32) | index; }
};

const WeakRef INVALID_WEAKREF = { WEAKREF_INVALID_INDEX, WEAKREF_INVALID_GENERATION };

static inline bool operator==(const WeakRef& lhs, const WeakRef& rhs)
{
    return lhs.index == rhs.index && lhs.generation == rhs.generation;
}

static inline bool operator!=(const WeakRef& lhs, const WeakRef& rhs) { return !(lhs == rhs); }

template < typename HandleType, typename T >
class WeakRefManager
{
  public:
    // Reserve size is basically a soft cap. If you add more refs past it,
    // the manager expands to accomodate
    WeakRefManager(uint32_t in_min_free_indices = 1024)
        : m_reserve_size(1024)
        , m_min_free_indices(in_min_free_indices)
    {
        grow(m_reserve_size);
    }

    HandleType add(T val)
    {
        if (m_free_indices.size() < m_min_free_indices)
        {
            grow(m_reserve_size * 2);
        }

        uint32_t index = m_free_indices.front();
        m_free_indices.pop_front();
        uint32_t generation = m_generations[index];
        m_data[index]       = val;

        HandleType handle;
        handle.handle = (WeakRef){ index, generation };
        return handle;
    }

    bool ref_is_valid(const HandleType& handle)
    {
        const WeakRef&  ref                = handle.handle;
        uint32_t current_generation = m_generations[ref.index];
        return ref.index < m_data.size() && current_generation == ref.generation
               && ref != INVALID_WEAKREF;
    }

    void remove(const HandleType& handle)
    {
        const WeakRef& ref = handle.handle;
        if (!ref_is_valid(handle))
        {
            log_invalid_ref_warning(ref);
            return;
        }

        m_free_indices.push_back(ref.index);
        m_generations[ref.index]++;
    }

    T* get(const HandleType& handle)
    {
        const WeakRef& ref = handle.handle;
        if (!ref_is_valid(handle))
        {
            log_invalid_ref_warning(ref);
            return nullptr;
        }

        return &m_data[ref.index];
    }

  private:
    std::vector< T >        m_data;
    std::vector< uint32_t > m_generations;
    std::deque< size_t >    m_free_indices;

    uint32_t m_reserve_size;
    uint32_t m_min_free_indices;

    void log_invalid_ref_warning(const WeakRef& ref)
    {
        uint32_t current_generation = m_generations[ref.index];
        LOG_WARNING(
            "Invalid ref. Index should be less than %llu, is %u. Generation should be %d, is %d",
            m_data.size(), ref.index, current_generation, ref.generation);
    }

    void grow(size_t new_reserve_size)
    {
        ASSERT(new_reserve_size >= m_reserve_size);
        m_reserve_size = new_reserve_size;
        m_data.reserve(m_reserve_size);
        m_generations.reserve(m_reserve_size);

        for (size_t i = m_data.size(); i < m_reserve_size; i++)
        {
            m_free_indices.push_back(i);

            T default_value;
            m_data.push_back(default_value);

            m_generations.push_back(0);
        }
    }
};
}    // namespace Handle