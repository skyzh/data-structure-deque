#ifndef SJTU_DEQUE_HPP
#define SJTU_DEQUE_HPP

#include "exceptions.hpp"

#include <cstddef>
#include <list>
#include <vector>
#include <iostream>

namespace sjtu {
    template<class T>
    class deque {
    private:
        static const int min_chunk_size = 64;
        int _size;

        struct Node {
            Node *prev, *next;

            Node(Node *prev = NULL, Node *next = NULL) : prev(prev), next(next) {}

            virtual ~Node() {}
        } *head, *tail;

        struct Chunk {
            Node *head, *tail;
            int chunk_size;
            Chunk *prev, *next;

            Chunk(Node *head, Node *tail, int chunk_size = 0, Chunk *prev = NULL, Chunk *next = NULL) :
                    head(head), tail(tail), chunk_size(chunk_size), prev(prev), next(next) {}
        } *chunk_head, *chunk_tail;

        struct Wrapper : public Node {
            T x;
            Wrapper(const T &x, Node *prev = NULL, Node *next = NULL) : Node(prev, next), x(x) {}
        };

        bool empty_chunk() const { return chunk_head->next == chunk_tail; }

        void construct() {
            head = new Node;
            tail = new Node;
            head->next = tail;
            tail->prev = head;
            chunk_head = new Chunk(head, head, 1);
            chunk_tail = new Chunk(tail, tail, 1);
            chunk_head->next = chunk_tail;
            chunk_tail->prev = chunk_head;
            _size = 0;
        }

        template<typename U>
        void _destroy(U *head) {
            U *ptr = head;
            while (ptr) {
                U *tmp = ptr->next;
                delete ptr;
                ptr = tmp;
            }
        }

        template<typename U>
        U *remove_node(U *node) {
            node->next->prev = node->prev;
            U *tmp = node->prev->next = node->next;
            delete node;
            return tmp;
        }

        void destroy() {
            _destroy<Node>(head);
            _destroy<Chunk>(chunk_head);
            _size = 0;
        }

        void copy_from(const deque &that) {
            const Node *that_ptr = that.head->next;
            while (that_ptr != that.tail) {
                const Wrapper *that_wrapper = dynamic_cast<const Wrapper *>(that_ptr);
                push_back(that_wrapper->x);
                that_ptr = that_ptr->next;
            }
            _size = that._size;
        }

        void throw_if_empty() const {
            if (empty()) throw container_is_empty();
        }

        template<typename T_x, typename T_Node, typename T_Wrapper, typename T_Chunk>
        class base_iterator {
        protected:
            friend deque;
            const deque *q;
            T_Chunk *chunk;
            T_Node *node;

            base_iterator(const deque *q, T_Chunk *chunk, T_Node *node) : q(q), chunk(chunk), node(node) {}

            int distance_to_head() const {
                T_Node *ptr = node;
                T_Chunk *chk = chunk;
                int d = 0;
                while (ptr != q->head) {
                    if (ptr == chk->head) {
                        chk = chk->prev;
                        d += chk->chunk_size;
                        ptr = chk->head;
                    } else {
                        ptr = ptr->prev;
                        ++d;
                    }
                }
                return d;
            }

            inline void move_backward() {
                if (node == chunk->head) chunk = chunk->prev;
                node = node->prev;
                if (node == q->head) throw index_out_of_bound();
            }

            inline void move_forward() {
                if (node == chunk->tail) chunk = chunk->next;
                node = node->next;
                if (node == NULL) throw index_out_of_bound();
            }

        public:
            /**
             * return a new iterator which pointer n-next elements
             *   even if there are not enough elements, the behaviour is **undefined**.
             * as well as operator-
             */
            base_iterator operator+(const int &n) const {
                base_iterator that(*this);
                that += n;
                return that;
            }

            base_iterator operator-(const int &n) const {
                return *this + (-n);
            }

            // return th distance between two iterator,
            // if these two iterators points to different vectors, throw invaild_iterator.
            int operator-(const base_iterator &rhs) const {
                if (rhs.q != this->q) throw invalid_iterator();
                return this->distance_to_head() - rhs.distance_to_head();
            }

            base_iterator &operator+=(const int &n) {
                int i = n;
                while (i > 0) {
                    if (node == chunk->head && i >= chunk->chunk_size) {
                        i -= chunk->chunk_size;
                        chunk = chunk->next;
                        if (!chunk) throw index_out_of_bound();
                        node = chunk->head;
                    } else {
                        this->move_forward();
                        --i;
                    }
                }
                while (i < 0) {
                    if (node == chunk->tail && i >= chunk->chunk_size) {
                        i -= chunk->chunk_size;
                        chunk = chunk->prev;
                        if (chunk == q->chunk_head) throw index_out_of_bound();
                        node = chunk->tail;
                    } else {
                        this->move_backward();
                        ++i;
                    }
                }
                return *this;
            }

            base_iterator &operator-=(const int &n) {
                return *this += (-n);
            }

            /**
             * iter++
             */
            base_iterator operator++(int) {
                base_iterator that(*this);
                ++*this;
                return that;
            }

            /**
             * ++iter
             */
            base_iterator &operator++() {
                this->move_forward();
                return *this;
            }

            /**
             * iter--
             */
            base_iterator operator--(int) {
                base_iterator that(*this);
                --*this;
                return that;
            }

            /**
             * --iter
             */
            base_iterator &operator--() {
                this->move_backward();
                return *this;
            }

            /**
             * TODO *it
             */
            T_x &operator*() const {
                if (node == q->tail) throw index_out_of_bound();
                return dynamic_cast<T_Wrapper *>(node)->x;
            }

            /**
             * TODO it->field
             */
            T_x *operator->() const noexcept { return &(dynamic_cast<T_Wrapper *>(node)->x); }

            /**
             * a operator to check whether two iterators are same (pointing to the same memory).
             */
            bool operator==(const base_iterator &rhs) const { return q == rhs.q && node == rhs.node; }

            /**
             * some other operator for iterator.
             */
            bool operator!=(const base_iterator &rhs) const { return !(*this == rhs); }
        };

        typedef base_iterator<T, Node, Wrapper, Chunk> _iterator;
        typedef base_iterator<const T, const Node, const Wrapper, const Chunk> _const_iterator;
    public:
        class const_iterator;

        class iterator : public _iterator {
        protected:
            friend deque;
            friend const_iterator;

            iterator(const deque *q, Chunk *chunk, Node *node) : _iterator(q, chunk, node) {}

        public:
            iterator() : iterator(NULL, NULL, NULL) {}

            iterator(const _iterator &that) : iterator(that.q, that.chunk, that.node) {}
        };

        class const_iterator : public _const_iterator {
        private:
            friend deque;

            const_iterator(const deque *q, const Chunk *chunk, const Node *node) : _const_iterator(q, chunk, node) {}

        public:
            const_iterator() : const_iterator(NULL, NULL, NULL) {}

            const_iterator(const iterator &that) : const_iterator(that.q, that.chunk, that.node) {}

            const_iterator(const _const_iterator &that) : const_iterator(that.q, that.chunk, that.node) {}
        };

    private:
        Chunk *split_chunk(Chunk *chunk, Node *pos) {
            if (should_split(chunk->chunk_size)) {
                int split_loc = chunk->chunk_size / 2;
                Node *split_node = chunk->head;
                bool pos_found_in_left = false;
                for (int i = 0; i < split_loc; i++) {
                    if (split_node == pos) pos_found_in_left = true;
                    split_node = split_node->next;
                }
                Chunk *left = new Chunk(chunk->head, split_node->prev, split_loc, chunk->prev);
                Chunk *right = new Chunk(split_node, chunk->tail, chunk->chunk_size - left->chunk_size, left,
                                         chunk->next);
                left->next = right;
                chunk->prev->next = left;
                chunk->next->prev = right;
                delete chunk;
                if (pos_found_in_left) return left; else return right;

            } else return chunk;
        }

        iterator _insert_before(Chunk *chunk, Node *pos, const T &x) {
            Node *tmp = new Wrapper(x, pos->prev, pos);
            pos->prev->next = tmp;
            pos->prev = tmp;
            if (empty_chunk()) {
                chunk = new Chunk(tmp, tmp, 1, chunk_head, chunk_tail);
                chunk_head->next = chunk;
                chunk_tail->prev = chunk;
            } else {
                if (chunk == chunk_tail) {
                    chunk = chunk->prev;
                    chunk->tail = tmp;
                }
                ++chunk->chunk_size;
                if (pos == chunk->head) chunk->head = tmp;
                chunk = split_chunk(chunk, tmp);
            }
            ++_size;
            return iterator(this, chunk, tmp);
        }

        bool should_split(int chunk_size) { return chunk_size >= min_chunk_size && chunk_size * chunk_size > _size; }

        Chunk *merge_chunk(Chunk *left) {
            if (left->next == chunk_tail) return left;
            Chunk *right = left->next;
            if (!should_split(left->chunk_size + right->chunk_size)) {
                Chunk *chunk = new Chunk(left->head, right->tail, left->chunk_size + right->chunk_size, left->prev,
                                         right->next);
                left->prev->next = chunk;
                right->next->prev = chunk;
                delete left;
                delete right;
                return chunk;
            }
            return left;
        }

        iterator _remove_at(Chunk *chunk, Node *pos) {
            if (pos == tail) throw invalid_iterator();
            Node *next = remove_node(pos);
            if (chunk->head == pos && chunk->tail == pos) {
                chunk->prev->next = chunk->next;
                chunk->next->prev = chunk->prev;
                Chunk *tmp = chunk->next;
                delete chunk;
                chunk = tmp;
            } else if (chunk->head == pos) {
                chunk->head = next;
                --chunk->chunk_size;
                chunk = merge_chunk(chunk);
            } else if (chunk->tail == pos) {
                chunk->tail = next->prev;
                --chunk->chunk_size;
                chunk = chunk->next;
            } else {
                --chunk->chunk_size;
                chunk = merge_chunk(chunk);
            }
            --_size;
            return iterator(this, chunk, next);
        }

    public:
        /**
         * TODO Constructors
         */
        deque() { construct(); }

        deque(const deque &other) {
            construct();
            copy_from(other);
        }

        /**
         * TODO Deconstructor
         */
        ~deque() { destroy(); }

        /**
         * TODO assignment operator
         */
        deque &operator=(const deque &other) {
            if (this == &other) return *this;
            destroy();
            construct();
            copy_from(other);
            return *this;
        }

        /**
         * access specified element with bounds checking
         * throw index_out_of_bound if out of bound.
         */
        T &at(const size_t &pos) { return *(begin() + pos); }

        const T &at(const size_t &pos) const { return *(cbegin() + pos); }

        T &operator[](const size_t &pos) { return at(pos); }

        const T &operator[](const size_t &pos) const { return at(pos); }

        /**
         * access the first element
         * throw container_is_empty when the container is empty.
         */
        const T &front() const {
            throw_if_empty();
            return *(cbegin());
        }

        /**
         * access the last element
         * throw container_is_empty when the container is empty.
         */
        const T &back() const {
            throw_if_empty();
            return *(--cend());
        }

        /**
         * returns an iterator to the beginning.
         */
        iterator begin() { return iterator(this, chunk_head->next, head->next); }

        const_iterator cbegin() const { return const_iterator(this, chunk_head->next, head->next); }

        /**
         * returns an iterator to the end.
         */
        iterator end() { return iterator(this, chunk_tail, tail); }

        const_iterator cend() const { return const_iterator(this, chunk_tail, tail); }

        /**
         * checks whether the container is empty.
         */
        bool empty() const { return head->next == tail; }

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
            if (pos.q != this) throw invalid_iterator();
            return _insert_before(pos.chunk, pos.node, value);
        }

        /**
         * removes specified element at pos.
         * removes the element at pos.
         * returns an iterator pointing to the following element, if pos pointing to the last element, end() will be returned.
         * throw if the container is empty, the iterator is invalid or it points to a wrong place.
         */
        iterator erase(iterator pos) {
            throw_if_empty();
            if (pos.q != this) throw invalid_iterator();
            return _remove_at(pos.chunk, pos.node);
        }

        /**
         * adds an element to the end
         */
        void push_back(const T &value) {
            _insert_before(chunk_tail, tail, value);
        }

        /**
         * removes the last element
         *     throw when the container is empty.
         */
        void pop_back() {
            throw_if_empty();
            _remove_at(chunk_tail->prev, tail->prev);
        }

        /**
         * inserts an element to the beginning.
         */
        void push_front(const T &value) {
            _insert_before(chunk_head->next, head->next, value);
        }

        /**
         * removes the first element.
         *     throw when the container is empty.
         */
        void pop_front() {
            throw_if_empty();
            _remove_at(chunk_head->next, head->next);
        }

        void debug() {
            Chunk *chunk = chunk_head->next;
            int i = 0;
            while (chunk != chunk_tail) {
                Node *ptr = chunk->head;
                std::cout << "chunk " << i++ << "(" << chunk->chunk_size << "): ";
                while (ptr->prev != chunk->tail) {
                    std::cout << dynamic_cast<Wrapper *>(ptr)->x << " ";
                    ptr = ptr->next;
                }
                std::cout << std::endl;
                chunk = chunk->next;
            }
            if (empty_chunk()) std::cout << "EMPTY" << std::endl;
            std::cout << std::endl;
        }
    };

}

#endif
