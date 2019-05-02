#ifndef SJTU_DEQUE_HPP
#define SJTU_DEQUE_HPP

#include "exceptions.hpp"
#include "utility.hpp"

#include <cstddef>
#include <memory>
#include <vector>
#include <iostream>

namespace sjtu {
    template<class T>
    class deque {
    private:
        static const int INSERT_GC_THRESHOLD = 10000;
        static const int REMOVE_GC_THRESHOLD = 10000;

        template<class U>
        class Vector {
            static const int min_chunk_size = 512;
            friend deque;
            U *buffer;
            int _size, _cap;
            std::allocator<U> alloc;

            bool full() { return _size == _cap; }

            static int fit(int min_cap) {
                int s = min_chunk_size;
                while (s < min_cap) s <<= 1;
                return s;
            }

            void expand_to(int new_cap) {
                U *new_buffer = alloc.allocate(new_cap);
                memcpy(new_buffer, buffer, sizeof(U) * _size);
                alloc.deallocate(buffer, _cap);
                _cap = new_cap;
                buffer = new_buffer;
            }

            void shrink_if_small() {
                if (_cap >= (min_chunk_size << 2) && (_size << 2) < _cap) expand_to(_cap >> 2);
            }

            void expand_if_full() {
                if (!full()) return;
                expand_to(_cap << 1);
            }

            U *get_buffer() {
                return buffer;
            }

        public:
            Vector(int cap = min_chunk_size) : _size(0), _cap(cap) {
                buffer = alloc.allocate(_cap);
            }

            Vector(const Vector &that) : _size(that._size), _cap(that._cap) {
                buffer = alloc.allocate(_cap);
                for (int i = 0; i < that._size; i++) alloc.construct(buffer + i, that[i]);
            }

            Vector &operator=(const Vector &that) {
                if (this == &that) return *this;
                for (int i = 0; i < _size; i++) alloc.destroy(buffer + i);
                alloc.deallocate(buffer, _cap);
                _cap = that._cap;
                _size = that._size;
                buffer = alloc.allocate(_cap);
                for (int i = 0; i < that._size; i++) alloc.construct(buffer + i, that[i]);
                return *this;
            }

            int size() const { return _size; }

            void insert(int pos, const U &x) {
                expand_if_full();
                if (pos != _size) memmove(buffer + pos + 1, buffer + pos, (_size - pos) * sizeof(U));
                alloc.construct(buffer + pos, x);
                ++_size;
            }

            void erase(int pos) {
                alloc.destroy(buffer + pos);
                if (pos != _size - 1) memmove(buffer + pos, buffer + pos + 1, (_size - pos - 1) * sizeof(U));
                --_size;
                shrink_if_small();
            }

            void clear() {
                for (int i = 0; i < _size; i++) alloc.destroy(buffer + i);
                _size = 0;
            }

            ~Vector() {
                for (int i = 0; i < _size; i++) alloc.destroy(buffer + i);
                alloc.deallocate(buffer, _cap);
            }

            U &operator[](int pos) { return buffer[pos]; }

            const U &operator[](int pos) const { return buffer[pos]; }
        };

        int _size;
        Vector<Vector<T> > x;
    private:
        template<typename Tx, typename Tq>
        class base_iterator {
        protected:
            friend deque;
            Tq *q;
            mutable Tx *elem;
            int pos;

            base_iterator(Tq *q, const int &pos) : q(q), pos(pos), elem(NULL) {}

            bool owns(Tq *q) const { return q == this->q; }

            void check_owns(Tq *q) const { if (!owns(q)) throw invalid_iterator(); }

            template<typename _This>
            static _This &valid(_This *self) {
                if (!self->q) throw invalid_iterator();
                self->q->throw_if_out_of_bound(self->pos, true);
                return *self;
            }

            base_iterator &construct() {
                elem = nullptr;
                return valid<base_iterator>(this);
            }

            const base_iterator &construct() const {
                elem = nullptr;
                return valid<const base_iterator>(this);
            }

        public:
            base_iterator(const base_iterator &that) = default;

            base_iterator() : base_iterator(nullptr, 0) {}

            /**
             * return a new iterator which pointer n-next elements
             *   even if there are not enough elements, the behaviour is **undefined**.
             * as well as operator-
             */
            base_iterator operator+(const int &n) const { return base_iterator(q, pos + n).construct(); }

            base_iterator operator-(const int &n) const { return base_iterator(q, pos - n).construct(); }

            // return th distance between two iterator,
            // if these two iterators points to different vectors, throw invaild_iterator.
            int operator-(const base_iterator &rhs) const {
                construct();
                check_owns(rhs.q);
                return pos - rhs.pos;
            }

            base_iterator &operator+=(const int &n) { return *this = base_iterator(q, pos + n).construct(); }

            base_iterator &operator-=(const int &n) { return *this = base_iterator(q, pos - n).construct(); }

            base_iterator operator++(int) {
                auto _ = *this;
                ++(*this);
                return _;
            }

            base_iterator &operator++() {
                ++pos;
                return construct();
            }

            base_iterator operator--(int) {
                auto _ = *this;
                --(*this);
                return _;
            }

            base_iterator &operator--() {
                --pos;
                return construct();
            }

            Tx &operator*() const {
                if (!elem) elem = &q->access(pos);
                return *elem;
            }

            Tx *operator->() const noexcept {
                if (!elem) elem = &q->access(pos);
                return elem;
            }

            bool operator==(const base_iterator &rhs) const { return rhs.q == q && rhs.pos == pos; }

            bool operator!=(const base_iterator &rhs) const { return !(*this == rhs); }
        };

    private:
        void init() {
            _size = 0;
            x.insert(0, Vector<T>());
        }

        void throw_if_out_of_bound(int pos, bool include_end = false) const {
            if (include_end && pos == size()) return;
            if (pos < 0 || pos >= size()) throw index_out_of_bound();
        }

        template<typename _This, typename Tx>
        static Tx &access(_This *self, int pos) {
            self->throw_if_out_of_bound(pos);
            int _pos = pos;
            int i = self->find_at(pos);
            return self->x[i][pos];
        }

        T &access(int pos) { return access<deque, T>(this, pos); }

        const T &access(int pos) const { return access<const deque, const T>(this, pos); }

        void split_chunk(int chunk) {
            int split_size = x[chunk].size() >> 1;
            x.insert(chunk, Vector<T>(Vector<T>::fit(x[chunk]._size)));
            auto &chk_a = x[chunk];
            auto &chk_b = x[chunk + 1];
            memcpy(chk_a.buffer, chk_b.buffer, sizeof(T) * split_size);
            chk_a._size = split_size;
            memmove(chk_b.buffer, chk_b.buffer + split_size, sizeof(T) * (chk_b._size - split_size));
            chk_b._size -= split_size;
        }

        void merge_chunk(int chunk) {
            auto &chk_a = x[chunk];
            auto &chk_b = x[chunk + 1];
            chk_a.expand_to(Vector<T>::fit(chk_a._size + chk_b._size));
            memcpy(chk_a.buffer + chk_a._size, chk_b.buffer, sizeof(T) * chk_b._size);
            chk_a._size += chk_b._size;
            chk_b._size = 0;
            x.erase(chunk + 1);
        }

        bool should_split(int total_size) { return total_size >= 16 && total_size * total_size > _size * 8; }

        bool should_merge(int total_size) { return total_size * total_size * 64 <= _size; }

        int find_at(int &pos) const {
            int i = 0, _pos = pos, tmp;
            if (_pos <= _size >> 1) {
                while (_pos >= (tmp = x[i]._size)) {
                    if (tmp == 0) {
                        ++i;
                        continue;
                    }
                    _pos -= tmp;
                    ++i;
                }
            } else {
                i = x._size - 1;
                _pos = _size - _pos;
                while (_pos > (tmp = x[i]._size)) {
                    if (tmp == 0) {
                        --i;
                        continue;
                    }
                    _pos -= tmp;
                    --i;
                }
                _pos = x[i]._size - _pos;
            }
            pos = _pos;
            return i;
        }

        int find_at_allow_end(int &pos) const {
            int i = 0, _pos = pos, tmp;
            if (_pos <= _size >> 1) {
                while (_pos > (tmp = x[i].size())) {
                    _pos -= tmp;
                    ++i;
                }
            } else {
                i = x.size() - 1;
                _pos = _size - _pos;
                while (i != 0 && _pos >= (tmp = x[i].size())) {
                    _pos -= tmp;
                    --i;
                }
                _pos = x[i].size() - _pos;
            }
            pos = _pos;
            return i;
        }

        int insert_at(int pos, const T &value) {
            throw_if_out_of_bound(pos, true);
            int __pos = pos;
            int i = find_at_allow_end(pos);
            x[i].insert(pos, value);
            ++_size;
            if (should_split(x[i].size())) split_chunk(i);
            if (rand() < INSERT_GC_THRESHOLD) gc();
            return __pos;
        }

    public:
        void gc() {
            clear_zero();
            for (int i = 0; i < x.size(); i++) {
                if (should_split(x[i].size())) {
                    split_chunk(i);
                    ++i;
                }
            }
            for (int i = 0; i < x.size() - 1; i++) {
                if (should_merge(x[i].size() + x[i + 1].size())) {
                    merge_chunk(i);
                    --i;
                }
            }
        }

    private:

        void clear_zero() {
            if (x.size() <= 1) return;
            for (int i = 0; i < x.size() - 1; i++) {
                if (x[i].size() == 0) {
                    x.erase(i);
                }
            }
        }

        int remove_at(int pos) {
            throw_if_out_of_bound(pos);
            int __pos = pos;
            int i = find_at(pos);
            x[i].erase(pos);
            --_size;
            if (i != x.size() - 1) {
                if (should_merge(x[i].size() + x[i + 1].size())) merge_chunk(i);
            } else {
                if (x.size() > 1 && x[i].size() == 0) x.erase(i);
            }
            if (rand() < REMOVE_GC_THRESHOLD) gc();
            return __pos;
        }

    public:
        typedef base_iterator<T, deque> iterator;
        typedef base_iterator<const T, const deque> const_iterator;

        /**
         * Constructors
         */
        deque() {
            _size = 0;
            init();
        }

        /**
         * Deconstructor
         */
        ~deque() {}

        /**
         * access specified element with bounds checking
         * throw index_out_of_bound if out of bound.
         */
        T &at(const size_t &pos) { return access(pos); }

        const T &at(const size_t &pos) const { return access(pos); }

        T &operator[](const size_t &pos) { return access(pos); }

        const T &operator[](const size_t &pos) const { return access(pos); }

        /**
         * access the first element
         * throw container_is_empty when the container is empty.
         */
        const T &front() const { return access(0); }

        /**
         * access the last element
         * throw container_is_empty when the container is empty.
         */
        const T &back() const { return access(size() - 1); }

        /**
         * returns an iterator to the beginning.
         */
        iterator begin() { return iterator(this, 0); }

        const_iterator cbegin() const { return const_iterator(this, 0); }

        /**
         * returns an iterator to the end.
         */
        iterator end() { return iterator(this, size()); }

        const_iterator cend() const { return const_iterator(this, size()); }

        /**
         * checks whether the container is empty.
         */
        bool empty() const { return _size == 0; }

        /**
         * returns the number of elements
         */
        size_t size() const { return _size; }

        /**
         * clears the contents
         */
        void clear() {
            x.clear();
            init();
        }

        /**
         * inserts elements at the specified locat on in the container.
         * inserts value before pos
         * returns an iterator pointing to the inserted value
         *     throw if the iterator is invalid or it point to a wrong place.
         */
        iterator insert(iterator pos, const T &value) {
            pos.check_owns(this);
            return iterator(this, insert_at(pos.pos, value));
        }

        /**
         * removes specified element at pos.
         * removes the element at pos.
         * returns an iterator pointing to the following element, if pos pointing to the last element, end() will be returned.
         * throw if the container is empty, the iterator is invalid or it points to a wrong place.
         */
        iterator erase(iterator pos) {
            pos.check_owns(this);
            return iterator(this, remove_at(pos.pos));
        }

        /**
         * adds an element to the end
         */
        void push_back(const T &value) { insert_at(size(), value); }

        /**
         * removes the last element
         *     throw when the container is empty.
         */
        void pop_back() { remove_at(size() - 1); }

        /**
         * inserts an element to the beginning.
         */
        void push_front(const T &value) { insert_at(0, value); }

        /**
         * removes the first element.
         *     throw when the container is empty.
         */
        void pop_front() { remove_at(0); }

        void debug() const {
            std::cerr << _size << "(" << x.size() << "): ";
            for (int i = 0; i < x.size(); i++) std::cerr << x[i].size() << "/" << x[i]._cap << " ";
            std::cerr << "\n";
        }
    };

}

#endif
