#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        Move(std::move(other));
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Move(std::move(rhs));
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    void Move(RawMemory&& other) noexcept {
        Deallocate(buffer_);
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }

    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_ + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_ + size_;
    }

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size),
        size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_),
        size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector<T>&& other) noexcept {
        data_ = std::move(other.data_);
        size_ = std::exchange(other.size_, 0);
    }

    Vector<T>& operator=(const Vector<T>& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector copy(rhs);
                Swap(copy);
            }
            else {
                size_t copy_elem = rhs.size_ < size_ ? rhs.size_ : size_;
                auto end = std::copy_n(rhs.data_.GetAddress(), copy_elem, data_.GetAddress());
                if (rhs.size_ < size_) {
                    std::destroy_n(end, size_ - rhs.size_);
                }
                else {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, end);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector<T>& operator=(Vector<T>&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        CopyOrMove(data_.GetAddress(), size_, new_data.GetAddress());
        data_.Swap(new_data);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Swap(Vector<T>& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + size_) T(value);
            CopyOrMove(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
        }
        else {
            new(data_ + size_)T(value);
        }
        ++size_;
    }

    void PushBack(T&& value) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + size_) T(std::move(value));
            CopyOrMove(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
        }
        else {
            new(data_ + size_)T(std::move(value));
        }
        ++size_;
    }

    void PopBack() noexcept {
        assert(size_ > 0);
        std::destroy_at(data_ + (size_ - 1));
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + size_) T(std::forward<Args>(args)...);
            CopyOrMove(data_.GetAddress(), size_, new_data.GetAddress());
            data_.Swap(new_data);
        }
        else {
            new(data_ + size_)T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (pos == end()) {
            return &EmplaceBack(std::forward<Args>(args)...);
        }
        if (size_ == Capacity()) {
            size_t index = static_cast<size_t>(pos - begin());
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new(new_data + index) T(std::forward<Args>(args)...);
            try {
                CopyOrMove(data_.GetAddress(), index, new_data.GetAddress());
            }
            catch (...) {
                new_data[index].~T();
                throw;
            }

            try {
                CopyOrMove(data_ + index, size_ - index, new_data + (index + 1));
            }
            catch (...) {
                std::destroy_n(new_data.GetAddress(), index + 1);
                throw;
            }
            data_.Swap(new_data);
            ++size_;
            return begin() + index;
        }
        else {
            size_t index = static_cast<size_t>(pos - begin());
            T temp_elem(std::forward<Args>(args)...);
            new(end()) T(std::move(data_[size_ - 1]));
            std::move_backward(begin() + index, end() - 1, end());
            data_[index] = std::move(temp_elem);
            ++size_;
            return begin() + index;
        }
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        assert(size_ > 0);
        size_t index = static_cast<size_t>(pos - begin());
        std::move(begin() + index + 1, end(), begin() + index);
        data_[size_ - 1].~T();
        --size_;
        return begin() + index;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:

    static void CopyOrMove(T* from, size_t size, T* to) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(from, size, to);
        }
        else {
            std::uninitialized_copy_n(from, size, to);
        }
        std::destroy_n(from, size);
    }

    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

    static void CopyConstruct(T* buf, const T& elem) {
        new(buf) T(elem);
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};
