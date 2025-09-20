// Any.hpp - Small type-erased box with SBO (no heap fallback in v0.1)
#pragma once

#include <NGIN/Primitives.hpp>
#include <NGIN/Meta/TypeName.hpp>
#include <NGIN/Hashing/FNV.hpp>
#include <NGIN/Memory/SystemAllocator.hpp>

#include <type_traits>
#include <utility>
#include <string_view>
#include <cstddef>
#include <cstring>

namespace NGIN::Reflection
{

  class Any
  {
    static constexpr NGIN::UIntSize SBO = 32;
    struct Storage
    {
      alignas(std::max_align_t) std::byte bytes[SBO];
    };

  public:
    constexpr Any() noexcept = default;
    Any(const Any &other) { CopyFrom(other); }
    Any &operator=(const Any &other)
    {
      if (this != &other)
      {
        this->~Any();
        CopyFrom(other);
      }
      return *this;
    }

    static constexpr NGIN::UInt64 VoidTypeId = 0ull;

    static Any make_void() noexcept
    {
      Any a;
      a.m_type = VoidTypeId;
      a.m_size = 0;
      a.m_dtor = nullptr;
      return a;
    }

    template <class T>
    static Any make(T &&v)
    {
      using U = std::remove_cv_t<std::remove_reference_t<T>>;
      Any a;
      a.m_size = sizeof(U);
      auto sv = NGIN::Meta::TypeName<U>::qualifiedName;
      a.m_type = NGIN::Hashing::FNV1a64(sv.data(), sv.size());
      a.m_copy_ctor = [](void *dst, const void *src)
      { ::new (dst) U(*reinterpret_cast<const U *>(src)); };
      if constexpr (sizeof(U) <= SBO)
      {
        ::new (a.inline_data()) U(std::forward<T>(v));
        a.m_isHeap = false;
      }
      else
      {
        // heap fallback using SystemAllocator
        void *mem = s_alloc.Allocate(sizeof(U), alignof(U));
        if (!mem) [[unlikely]]
        {
          a.m_type = VoidTypeId;
          a.m_size = 0;
          return a;
        }
        ::new (mem) U(std::forward<T>(v));
        a.m_heap = mem;
        a.m_isHeap = true;
        a.m_align = alignof(U);
      }
      if constexpr (!std::is_trivially_destructible_v<U>)
      {
        a.m_dtor = [](void *p)
        { reinterpret_cast<U *>(p)->~U(); };
      }
      return a;
    }

    Any(Any &&other) noexcept { MoveFrom(std::move(other)); }
    Any &operator=(Any &&other) noexcept
    {
      if (this != &other)
      {
        this->~Any();
        MoveFrom(std::move(other));
      }
      return *this;
    }

    // copy operations implemented above

    ~Any()
    {
      if (m_dtor && m_size)
        m_dtor(const_cast<void *>(raw_data()));
      if (m_isHeap && m_heap)
        s_alloc.Deallocate(m_heap, m_size, m_align);
    }

    template <class T>
    T &As()
    {
      using U = std::remove_cv_t<std::remove_reference_t<T>>;
      return *reinterpret_cast<U *>(const_cast<void *>(raw_data()));
    }
    template <class T>
    const T &As() const
    {
      using U = std::remove_cv_t<std::remove_reference_t<T>>;
      return *reinterpret_cast<const U *>(raw_data());
    }

    [[nodiscard]] NGIN::UInt64 type_id() const noexcept { return m_type; }
    [[nodiscard]] NGIN::UIntSize size() const noexcept { return m_size; }
    [[nodiscard]] bool is_void() const noexcept { return m_type == VoidTypeId; }
    [[nodiscard]] const void *raw_data() const noexcept { return m_isHeap ? m_heap : static_cast<const void *>(m_store.bytes); }

  private:
    void MoveFrom(Any &&o) noexcept
    {
      m_type = o.m_type;
      m_size = o.m_size;
      m_dtor = o.m_dtor;
      m_align = o.m_align;
      m_isHeap = o.m_isHeap;
      if (o.m_isHeap)
      {
        m_heap = o.m_heap;
        o.m_heap = nullptr;
      }
      else
      {
        std::memcpy(this->inline_data(), o.m_store.bytes, SBO);
      }
      o.m_type = 0;
      o.m_size = 0;
      o.m_dtor = nullptr;
      o.m_isHeap = false;
    }

    void CopyFrom(const Any &o)
    {
      m_type = o.m_type;
      m_size = o.m_size;
      m_dtor = o.m_dtor;
      m_align = o.m_align;
      m_isHeap = o.m_isHeap;
      m_copy_ctor = o.m_copy_ctor;
      if (o.m_isHeap)
      {
        void *mem = s_alloc.Allocate(m_size, m_align);
        if (!mem)
        {
          m_type = VoidTypeId;
          m_size = 0;
          m_isHeap = false;
          return;
        }
        m_heap = mem;
        if (m_copy_ctor)
          m_copy_ctor(m_heap, o.raw_data());
        else
          std::memcpy(m_heap, o.raw_data(), m_size);
      }
      else
      {
        if (m_copy_ctor)
          m_copy_ctor(inline_data(), o.raw_data());
        else
          std::memcpy(inline_data(), o.raw_data(), m_size);
        m_isHeap = false;
      }
    }

    static inline NGIN::Memory::SystemAllocator s_alloc{};
    Storage m_store{};
    void *m_heap{nullptr};
    NGIN::UInt64 m_type{VoidTypeId};
    NGIN::UIntSize m_size{0};
    NGIN::UIntSize m_align{alignof(std::max_align_t)};
    void (*m_dtor)(void *){nullptr};
    void (*m_copy_ctor)(void *, const void *){nullptr};
    bool m_isHeap{false};
    void *inline_data() noexcept { return static_cast<void *>(m_store.bytes); }
  };

} // namespace NGIN::Reflection
