#ifndef SJTU_DEQUE_HPP
#define SJTU_DEQUE_HPP

#include "exceptions.hpp"

#include <cstddef>
#include <memory>

namespace sjtu {

    template<class T>
    class deque : std::deque {
    private:
        static const int default_cap = 1024;
        T *ring_buffer;
        int _front, _rear, cap;
        int _size;
        std::allocator<T> alloc;

        int _real_pos(const int &pos) const {
            int pos_b = pos + _front;
            int pos_a = pos + _front - cap;
            return pos_a >= 0 ? pos_a : pos_b;
        }

        int _next_pos(int pos) const {
            if (pos == cap - 1) return 0; else return pos + 1;
        }

        int _prev_pos(int pos) const {
            if (pos > 0) return pos - 1; else return cap - 1;
        }

        inline bool wrap() const { return _rear < _front; }

        inline bool full() const { return _next_pos(_rear) == _front; }

        T &access(const size_t &pos) {
            if (pos >= size()) throw index_out_of_bound();
            else
                return ring_buffer[_real_pos(pos)];
        }

        const T &access(const size_t &pos) const {
            if (pos >= size()) throw index_out_of_bound();
            else return ring_buffer[_real_pos(pos)];
        }

        void expand() {
            T *new_buffer = alloc.allocate(cap * 2);
            int _size = size();
            if (wrap()) {
                memcpy(new_buffer, ring_buffer + _front, (cap - _front) * sizeof(T));
                memcpy(new_buffer + (cap - _front), ring_buffer, _rear * sizeof(T));
            } else {
                memcpy(new_buffer, ring_buffer + _front, (_rear - _front) * sizeof(T));
            }
            alloc.deallocate(ring_buffer, cap);
            ring_buffer = new_buffer;
            cap *= 2;
            _front = 0;
            _rear = _size;
        }

        void throw_if_empty() const { if (empty()) throw container_is_empty(); }

        void expand_if_full() { if (full()) expand(); }

        inline void mem_replace(T *a, const T *b) {
            memcpy(a, b, sizeof(T));
        }

        void _swap_backward(int from, int target) {
            while (from != target) {
                int prev = _prev_pos(from);
                mem_replace(ring_buffer + from, ring_buffer + prev);
                from = prev;
            }
        }

        void swap_backward(int from, int target) {
            if (target == from) return;
            if (target < from) {
                memmove(ring_buffer + target + 1, ring_buffer + target, sizeof(T) * (from - target));
            } else {
                memmove(ring_buffer + 1, ring_buffer, sizeof(T) * from);
                memcpy(ring_buffer, ring_buffer + cap - 1, sizeof(T));
                memmove(ring_buffer + target + 1, ring_buffer + target, sizeof(T) * (cap - target - 1));
            }
        }

        void _swap_forward(int from, int target) {
            while (from != target) {
                int next = _next_pos(from);
                mem_replace(ring_buffer + from, ring_buffer + next);
                from = next;
            }
        }

        void swap_forward(int from, int target) {
            if (target == from) return;
            if (target > from) {
                memmove(ring_buffer + from, ring_buffer + from + 1, sizeof(T) * (target - from));
            } else {
                memmove(ring_buffer + from, ring_buffer + from + 1, sizeof(T) * (cap - from - 1));
                memcpy(ring_buffer + cap - 1, ring_buffer, sizeof(T));
                memmove(ring_buffer, ring_buffer + 1, sizeof(T) * target);
            }
        }

        int insert_before(const int &pos, const T &x) {
            if (pos < 0 || pos > size()) throw index_out_of_bound();
            expand_if_full();
            int target = 0;
            if (pos < size() / 2) {
                target = _prev_pos(_real_pos(pos));
                _front = _prev_pos(_front);
                swap_forward(_front, target);
            } else {
                target = _real_pos(pos);
                swap_backward(_rear, target);
                _rear = _next_pos(_rear);
            }
            ++_size;
            alloc.construct(ring_buffer + target, x);
            return pos;
        }

        int remove_at(const int &pos) {
            throw_if_empty();
            if (pos < 0 || pos >= size()) throw index_out_of_bound();
            int target = _real_pos(pos);
            alloc.destroy(ring_buffer + target);
            if (pos < size() / 2) {
                swap_backward(target, _front);
                _front = _next_pos(_front);
            } else {
                _rear = _prev_pos(_rear);
                swap_forward(target, _rear);
            }
            --_size;
            return pos;
        }

        void construct() {
            _front = _rear = _size = 0;
            cap = default_cap;
            ring_buffer = alloc.allocate(cap);
        }

        void destroy() {
            while (_front != _rear) {
                alloc.destroy(ring_buffer + _front);
                _front = _next_pos(_front);
            }
            alloc.deallocate(ring_buffer, cap);
        }

        void copy_from(const deque &q) {
            _front = _rear = _size = 0;
            cap = q.cap;
            ring_buffer = alloc.allocate(cap);
            _size = _rear = q.size();
            for (int i = 0; i < q.size(); i++) {
                alloc.construct(ring_buffer + i, q[i]);
            }
        }

    private:
        template<typename Tx, typename Tq>
        class base_iterator {
        protected:
            friend deque;
            Tq *q;
            int pos;

            base_iterator(Tq *q, const int &pos) : q(q), pos(pos) {
                if (!q) return;
                if (pos < 0 || pos > q->size()) throw index_out_of_bound();
            }

            bool owns(Tq *q) const { return q == this->q; }

            void check_owns(Tq *q) const { if (!owns(q)) throw invalid_iterator(); }

        public:
            base_iterator(const base_iterator &that) = default;

            base_iterator() : base_iterator(NULL, 0) {}

            /**
             * return a new iterator which pointer n-next elements
             *   even if there are not enough elements, the behaviour is **undefined**.
             * as well as operator-
             */
            base_iterator operator+(const int &n) const { return base_iterator(q, pos + n); }

            base_iterator operator-(const int &n) const { return base_iterator(q, pos - n); }

            // return th distance between two iterator,
            // if these two iterators points to different vectors, throw invaild_iterator.
            int operator-(const base_iterator &rhs) const {
                check_owns(rhs.q);
                return pos - rhs.pos;
            }

            base_iterator &operator+=(const int &n) { return *this = base_iterator(q, pos + n); }

            base_iterator &operator-=(const int &n) { return *this = base_iterator(q, pos - n); }

            base_iterator operator++(int) {
                auto _ = *this;
                ++(*this);
                return _;
            }

            base_iterator &operator++() {
                ++pos;
                if (pos < 0 || pos > q->size()) throw index_out_of_bound();
                return *this;
            }

            base_iterator operator--(int) {
                auto _ = *this;
                --(*this);
                return _;
            }

            base_iterator &operator--() {
                --pos;
                if (pos < 0 || pos > q->size()) throw index_out_of_bound();
                return *this;
            }

            Tx &operator*() const { if (q) return q->access(pos); else throw invalid_iterator(); }

            Tx *operator->() const noexcept { return &(q->access(pos)); }

            bool operator==(const base_iterator &rhs) const { return rhs.q == q && rhs.pos == pos; }

            bool operator!=(const base_iterator &rhs) const { return !(*this == rhs); }
        };

        typedef base_iterator<T, deque> _iterator;
        typedef base_iterator<const T, const deque> _const_iterator;
    public:
        class const_iterator;

        class iterator : public _iterator {
        protected:
            friend deque;
            friend const_iterator;

            iterator(deque *q, const int &pos) : _iterator(q, pos) {}

        public:
            iterator() : _iterator() {}

            iterator(const _iterator &that) : _iterator(that) {}
        };

        class const_iterator : public _const_iterator {
        private:
            friend deque;

            const_iterator(const deque *q, const int &pos) : _const_iterator(q, pos) {}

        public:
            const_iterator() : _const_iterator() {}

            const_iterator(const _iterator &that) : _const_iterator(that) {}

            const_iterator(const _const_iterator &that) : _const_iterator(that) {}
        };

        /**
         * Constructors
         */
        deque() { construct(); }

        deque(const deque &other) { copy_from(other); }

        /**
         * Deconstructor
         */
        ~deque() { destroy(); }

        /**
         * assignment operator
         */
        deque &operator=(const deque &other) {
            if (this == &other) return *this;
            destroy();
            copy_from(other);
            return *this;
        }

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
        bool empty() const { return _front == _rear; }

        /**
         * returns the number of elements
         */
        size_t size() const { return _size; }

        /**
         * clears the contents
         */
        void clear() {
            destroy();
            construct();
        }

        /**
         * inserts elements at the specified locat on in the container.
         * inserts value before pos
         * returns an iterator pointing to the inserted value
         *     throw if the iterator is invalid or it point to a wrong place.
         */
        iterator insert(iterator pos, const T &value) {
            pos.check_owns(this);
            return iterator(this, insert_before(pos.pos, value));
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
        void push_back(const T &value) {
            expand_if_full();
            alloc.construct(ring_buffer + _rear, value);
            _rear = _next_pos(_rear);
            ++_size;
        }

        /**
         * removes the last element
         *     throw when the container is empty.
         */
        void pop_back() {
            throw_if_empty();
            _rear = _prev_pos(_rear);
            alloc.destroy(ring_buffer + _rear);
            --_size;
        }

        /**
         * inserts an element to the beginning.
         */
        void push_front(const T &value) {
            expand_if_full();
            _front = _prev_pos(_front);
            alloc.construct(ring_buffer + _front, value);
            ++_size;
        }

        /**
         * removes the first element.
         *     throw when the container is empty.
         */
        void pop_front() {
            throw_if_empty();
            alloc.destroy(ring_buffer + _front);
            _front = _next_pos(_front);
            --_size;
        }

        /*
        void debug() {
            for (int i = _front; i != _rear; i = _next_pos(i)) std::cout << ring_buffer[i] << " ";
            std::cout << std::endl;
        }
         */
    };

}

#endif
