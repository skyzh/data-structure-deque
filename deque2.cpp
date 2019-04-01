#ifndef SJTU_DEQUE_HPP
#define SJTU_DEQUE_HPP

#include "exceptions.hpp"

#include <cstddef>
#include <cstring>
#include <cstdlib>

namespace sjtu
{

template <class T>
class deque
{
  private:
    friend class iterator;

    static const unsigned chunk_size = 512; // cannot be 1 otherwise there'd be something wrong with iterator
    struct Chunk
    {
        T *data;
        Chunk *prev;
        Chunk *next;
        std::allocator<T> allocator;

        /* TODO: write our own allocator */
        Chunk(Chunk *prev = NULL, Chunk *next = NULL) : prev(prev),
                                                        next(next)
        {
            data = allocator.allocate(chunk_size);
        }

        Chunk(const Chunk &other) = delete;

        static Chunk *construct_from(const Chunk &other, int data_begin, int data_end)
        {
            Chunk *chunk = new Chunk;
            for (int i = data_begin; i < data_end; i++)
                chunk->allocator.construct(chunk->data + i, other.data[i]);
            return chunk;
        }

        void destruct_range(int data_begin, int data_end)
        {
            for (int i = data_begin; i < data_end; i++)
                allocator.destroy(data + i);
        }

        ~Chunk() { allocator.deallocate(data, chunk_size); }
    } * head, *tail;

    T *chunk_head, *chunk_tail;

    void destruct()
    {
        Chunk *ptr = head;
        while (ptr->prev)
            ptr = ptr->prev;
        bool is_in_range = false;
        while (ptr)
        {
            Chunk *tmp = ptr->next;
            int data_begin = 0;
            int data_end = chunk_size;
            if (ptr == head)
            {
                data_begin = chunk_head - head->data;
                is_in_range = true;
            }
            if (ptr == tail)
                data_end = chunk_tail - tail->data;
            if (is_in_range)
                ptr->destruct_range(data_begin, data_end);
            if (ptr == tail)
                is_in_range = false;
            delete ptr;
            ptr = tmp;
        }
    }

    void copy_from(const deque &other)
    {
        Chunk *ptr = other.head;
        Chunk *prev = NULL;
        while (ptr)
        {
            int data_begin = 0;
            int data_end = chunk_size;
            if (ptr == other.head)
                data_begin = other.chunk_head - other.head->data;
            if (ptr == other.tail)
                data_end = other.chunk_tail - other.tail->data;
            Chunk *cur = Chunk::construct_from(*ptr, data_begin, data_end);
            cur->prev = prev;
            if (prev)
                prev->next = cur;
            else
                this->head = cur;
            prev = cur;
            ptr = ptr->next;
        }
        this->tail = prev;
        this->chunk_head = this->head->data + (other.chunk_head - other.head->data);
        this->chunk_tail = this->tail->data + (other.chunk_tail - other.tail->data);
    }

    void create_new()
    {
        tail = head = new Chunk;
        chunk_head = chunk_tail = head->data;
    }

    void append_chunk()
    {
        if (!tail->next)
            tail->next = new Chunk(tail);
        tail = tail->next;
    }

    void prepend_chunk()
    {
        if (!head->prev)
            head->prev = new Chunk(NULL, head);
        head = head->prev;
    }

    void shrink_tail_chunk()
    {
        Chunk *tmp = tail->prev;
        delete tail;
        tail = tmp;
        tail->next = NULL;
    }

    void shrink_head_chunk()
    {
        Chunk *tmp = head->next;
        delete head;
        head = tmp;
        head->prev = NULL;
    }

    void throw_when_empty() const
    {
        if (this->empty())
            throw container_is_empty();
    }

  public:
    class const_iterator;

    class iterator
    {
      private:
        friend deque<T>;

        void debug(const char *msg) const
        {
            fprintf(stderr, "%s: chunk %x at %d\n", msg, chunk, pos - chunk->data);
        }

        bool is_at_the_end() const { return pos == chunk->data + chunk_size; }

        int distance_to_head() const
        {
            Chunk *ptr = q->head;
            int chunk_cnt = 0;
            while (ptr != chunk)
            {
                ++chunk_cnt;
                ptr = ptr->next;
            }
            int head_offset = q->chunk_head - q->head->data;
            int tail_offset = pos - chunk->data;
            return tail_offset + chunk_cnt * chunk_size - head_offset;
        };

      protected:
        deque *q;
        Chunk *chunk;
        T *pos;

        iterator(deque *q, Chunk *chunk, T *pos) : q(q), chunk(chunk), pos(pos) {}

        void move_chunk(int i)
        {
            if (i == 0)
                return;
            int offset = pos - chunk->data;
            while (i > 0)
            {
                --i;
                if (!chunk->next)
                {
                    if (chunk == q->tail && offset == 0)
                    {
                        offset = chunk_size;
                    }
                    else
                        throw index_out_of_bound();
                }
                else
                    chunk = chunk->next;
            }
            while (i < 0)
            {
                ++i;
                if (!chunk->prev)
                {
                    throw index_out_of_bound();
                }
                chunk = chunk->prev;
            }
            pos = chunk->data + offset;
        }

        void move_forward(int i)
        {
            if (i == 0)
                return;
            while (i < 0)
            {
                ++i;
                if (pos == chunk->data)
                {
                    if (!chunk->prev)
                        throw index_out_of_bound();
                    chunk = chunk->prev;
                    pos = chunk->data + chunk_size;
                }
                --pos;
                if (chunk == q->head && pos < q->chunk_head)
                    throw index_out_of_bound();
            }
            while (i > 0)
            {
                --i;
                ++pos;
                if (pos == chunk->data + chunk_size)
                {
                    if (chunk != q->tail)
                    {
                        if (!chunk->next)
                            throw index_out_of_bound();
                        chunk = chunk->next;
                        pos = chunk->data;
                    }
                }
                if (chunk == q->tail && pos > q->chunk_tail)
                    throw index_out_of_bound();
            }
        }

      public:
        /** constructors **/
        iterator() : q(NULL), chunk(NULL), pos(NULL) {}

        iterator(const iterator &other) : iterator(other.q, other.chunk, other.pos) {}

        /**
             * return a new iterator which pointer n-next elements
             *   even if there are not enough elements, the behaviour is **undefined**.
             * as well as operator-
             */
        iterator operator+(const int &n) const
        {
            iterator that(*this);
            if (n > 0)
            {
                if (n <= chunk_size)
                    that.move_forward(n);
                else
                {
                    that.move_forward(n % chunk_size);
                    that.move_chunk(n / chunk_size);
                }
            }
            else if (n < 0)
            {
                if (n >= -chunk_size)
                    that.move_forward(n);
                else
                {
                    that.move_forward(-(int)((-n) % chunk_size));
                    that.move_chunk(-(int)((-n) / chunk_size));
                }
            }

            return that;
        }

        iterator operator-(const int &n) const { return this->operator+(-n); }

        // return th distance between two iterator,
        // if these two iterators points to different vectors, throw invaild_iterator.
        int operator-(const iterator &rhs) const
        {
            if (rhs.q != this->q)
                throw invalid_iterator();
            return this->distance_to_head() - rhs.distance_to_head();
        }

        iterator operator+=(const int &n)
        {
            this->move_forward(n);
            return *this;
        }

        iterator operator-=(const int &n)
        {
            this->move_forward(-n);
            return *this;
        }

        /**
             * iter++
             */
        iterator operator++(int)
        {
            iterator that(*this);
            this->move_forward(1);
            return that;
        }

        /**
             * ++iter
             */
        iterator &operator++()
        {
            this->move_forward(1);
            return *this;
        }

        /**
             * iter--
             */
        iterator operator--(int)
        {
            iterator that(*this);
            this->move_forward(-1);
            return that;
        }

        /**
             * --iter
             */
        iterator &operator--()
        {
            this->move_forward(-1);
            return *this;
        }

        /**
             * *it
             */
        T &operator*() const
        {
            if (chunk == q->tail && pos == q->chunk_tail)
                throw index_out_of_bound();
            return *pos;
        }

        /**
             * it->field
             */
        T *operator->() const noexcept { return pos; }

        /**
             * a operator to check whether two iterators are same (pointing to the same memory).
             */
        bool operator==(const iterator &rhs) const
        {
            return (q == rhs.q) && (chunk == rhs.chunk) && (pos == rhs.pos);
        }

        bool operator==(const const_iterator &rhs) const
        {
            return (q == rhs.q) && (chunk == rhs.chunk) && (pos == rhs.pos);
        }

        /**
             * some other operator for iterator.
             */
        bool operator!=(const iterator &rhs) const { return !(*this == rhs); }

        bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }
    };

    class const_iterator
    {
        // it should has similar member method as iterator.
        //  and it should be able to construct from an iterator.
      private:
        friend deque<T>;

        bool is_at_the_end() { return pos == chunk->data + chunk_size; }

        int distance_to_head() const
        {
            Chunk *ptr = q->head;
            int chunk_cnt = 0;
            while (ptr != chunk)
            {
                ++chunk_cnt;
                ptr = ptr->next;
            }
            int head_offset = q->chunk_head - q->head->data;
            int tail_offset = pos - chunk->data;
            return tail_offset + chunk_cnt * chunk_size - head_offset;
        };

        void move_chunk(int i)
        {
            if (i == 0)
                return;
            int offset = pos - chunk->data;
            while (i > 0)
            {
                --i;
                if (!chunk->next)
                {
                    if (chunk == q->tail && offset == 0)
                    {
                        offset = chunk_size;
                    }
                    else
                        throw index_out_of_bound();
                }
                else
                    chunk = chunk->next;
            }
            while (i < 0)
            {
                ++i;
                if (!chunk->prev)
                {
                    throw index_out_of_bound();
                }
                chunk = chunk->prev;
            }
            pos = chunk->data + offset;
        }

        void move_forward(int i)
        {
            if (i == 0)
                return;
            while (i < 0)
            {
                ++i;
                if (pos == chunk->data)
                {
                    if (!chunk->prev)
                        throw index_out_of_bound();
                    chunk = chunk->prev;
                    pos = chunk->data + chunk_size;
                }
                --pos;
                if (chunk == q->head && pos < q->chunk_head)
                    throw index_out_of_bound();
            }
            while (i > 0)
            {
                --i;
                ++pos;
                if (pos == chunk->data + chunk_size)
                {
                    if (chunk != q->tail)
                    {
                        if (!chunk->next)
                            throw index_out_of_bound();
                        chunk = chunk->next;
                        pos = chunk->data;
                    }
                }
                if (chunk == q->tail && pos > q->chunk_tail)
                    throw index_out_of_bound();
            }
        }

      protected:
        const deque *q;
        const Chunk *chunk;
        const T *pos;

        const_iterator(const deque *q, const Chunk *chunk, const T *pos) : q(q), chunk(chunk), pos(pos) {}

      public:
        const_iterator(const const_iterator &other) : const_iterator(other.q, other.chunk, other.pos) {}

        const_iterator(const iterator &other) : const_iterator(other.q, other.chunk.other.pos) {}

        const_iterator() {}

        /**
             * return a new iterator which pointer n-next elements
             *   even if there are not enough elements, the behaviour is **undefined**.
             * as well as operator-
             */
        const_iterator operator+(const int &n) const
        {
            const_iterator that(*this);
            if (n > 0)
            {
                that.move_forward(n % chunk_size);
                that.move_chunk(n / chunk_size);
            }
            else if (n < 0)
            {
                /* TODO: is chunk_size is 1, then move_forward is always called with 0, which may cause exception */
                that.move_forward(-(int)((-n) % chunk_size));
                that.move_chunk(-(int)((-n) / chunk_size));
            }
            return that;
        }

        const_iterator operator-(const int &n) const { return this->operator+(-n); }

        // return th distance between two iterator,
        // if these two iterators points to different vectors, throw invaild_iterator.
        int operator-(const const_iterator &rhs) const
        {
            if (rhs.q != this->q)
                throw invalid_iterator();
            return this->distance_to_head() - rhs.distance_to_head();
        }

        /**
             * *it
             */
        const T &operator*() const
        {
            if (chunk == q->tail && pos == q->chunk_tail)
                throw index_out_of_bound();
            return *pos;
        }

        /**
             * it->field
             */
        const T *operator->() const noexcept { return this->pos; }

        bool operator==(const iterator &rhs) const
        {
            return (q == rhs.q) && (chunk == rhs.chunk) && (pos == rhs.pos);
        }

        bool operator==(const const_iterator &rhs) const
        {
            return (q == rhs.q) && (chunk == rhs.chunk) && (pos == rhs.pos);
        }

        /**
             * some other operator for iterator.
             */
        bool operator!=(const iterator &rhs) const { return !(*this == rhs); }

        bool operator!=(const const_iterator &rhs) const { return !(*this == rhs); }

        const_iterator operator+=(const int &n)
        {
            this->move_forward(n);
            return *this;
        }

        const_iterator operator-=(const int &n)
        {
            this->move_forward(-n);
            return *this;
        }

        /**
             * iter++
             */
        const_iterator operator++(int)
        {
            iterator that(*this);
            this->move_forward(1);
            return that;
        }

        /**
             * ++iter
             */
        const_iterator &operator++()
        {
            this->move_forward(1);
            return *this;
        }

        /**
             * iter--
             */
        const_iterator operator--(int)
        {
            iterator that(*this);
            this->move_forward(-1);
            return that;
        }

        /**
             * --iter
             */
        const_iterator &operator--()
        {
            this->move_forward(-1);
            return *this;
        }
    };

    /**
         * constructors
         */
    deque() { create_new(); }

    deque(const deque &other) { this->copy_from(other); }

    /**
         * deconstructor
         */
    ~deque() { this->destruct(); }

    /**
         * assignment operator
         */
    deque &operator=(const deque &other)
    {
        if (this == &other)
            return *this;
        this->destruct();
        this->copy_from(other);
        return *this;
    }

    /**
         * access specified element with bounds checking
         * throw index_out_of_bound if out of bound.
         */
    T &at(const size_t &pos) { return *(begin() + pos); }

    const T &at(const size_t &pos) const { return *(cbegin() + pos); }

    T &operator[](const size_t &pos) { return this->at(pos); }

    const T &operator[](const size_t &pos) const { return this->at(pos); }

    /**
         * access the first element
         * throw container_is_empty when the container is empty.
         */
    const T &front() const
    {
        throw_when_empty();
        return *chunk_head;
    }

    /**
         * access the last element
         * throw container_is_empty when the container is empty.
         */
    const T &back() const
    {
        throw_when_empty();
        return *(chunk_tail - 1);
    }

    /**
         * returns an iterator to the beginning.
         */
    iterator begin() { return iterator(this, head, chunk_head); }

    const_iterator cbegin() const { return const_iterator(this, head, chunk_head); }

    /**
         * returns an iterator to the end.
         */
    iterator end() { return iterator(this, tail, chunk_tail); }

    const_iterator cend() const { return const_iterator(this, tail, chunk_tail); }

    /**
         * checks whether the container is empty.
         */
    bool empty() const
    {
        if (head == tail && chunk_head == chunk_tail)
            return true;
        else
            return false;
    }

    /**
         * returns the number of elements
         */
    size_t size() const { return cend() - cbegin(); }

    /**
         * clears the contents
         */
    void clear()
    {
        this->destruct();
        this->create_new();
    }

    /**
         * inserts elements at the specified locat on in the container.
         * inserts value before pos
         * returns an iterator pointing to the inserted value
         *     throw if the iterator is invalid or it point to a wrong place.
         */
    iterator insert(iterator pos, const T &value)
    {
        if (pos == end())
        {
            push_back(value);
            return end() - 1;
        }
        push_back(value);
        iterator cur = end();
        --cur;
        while (cur != pos)
        {
            iterator next = cur;
            --cur;
            std::swap_ranges(next.pos, next.pos + 1, cur.pos);
        }
        return pos;
    }

    /**
         * removes specified element at pos.
         * removes the element at pos.
         * returns an iterator pointing to the following element, if pos pointing to the last element, end() will be returned.
         * throw if the container is empty, the iterator is invalid or it points to a wrong place.
         */
    iterator erase(iterator pos)
    {
        iterator lst = end();
        --lst;
        iterator cur = pos;
        while (cur != lst)
        {
            iterator prev = cur;
            ++cur;
            std::swap_ranges(prev.pos, prev.pos + 1, cur.pos);
        }
        pop_back();
        return pos;
    }

    /**
         * adds an element to the end
         */
    void push_back(const T &value)
    {
        if (chunk_tail - tail->data == chunk_size)
        {
            append_chunk();
            chunk_tail = tail->data;
        }
        tail->allocator.construct(chunk_tail, value);
        ++chunk_tail;
    }

    /**
         * removes the last element
         *     throw when the container is empty.
         */
    void pop_back()
    {
        throw_when_empty();
        --chunk_tail;
        tail->allocator.destroy(chunk_tail);
        if (chunk_tail - tail->data == 0)
        {
            if (head != tail)
            {
                shrink_tail_chunk();
                chunk_tail = tail->data + chunk_size;
            }
        }
    }

    /**
         * inserts an element to the beginning.
         */
    void push_front(const T &value)
    {
        if (chunk_head - head->data == 0)
        {
            prepend_chunk();
            chunk_head = head->data + chunk_size;
        }
        --chunk_head;
        head->allocator.construct(chunk_head, value);
    }

    /**
         * removes the first element.
         *     throw when the container is empty.
         */
    void pop_front()
    {
        throw_when_empty();
        head->allocator.destroy(chunk_head);
        ++chunk_head;
        if (chunk_head - head->data == chunk_size)
        {
            if (head == tail)
            {
                chunk_head = chunk_tail = head->data;
            }
            else
            {
                shrink_head_chunk();
                chunk_head = head->data;
            }
        }
    }

    void debug()
    {
        Chunk *ptr = head;
        while (ptr)
        {
            for (int i = 0; i < chunk_size; i++)
                fprintf(stderr, "%d ", ptr->data[i]);
            ptr = ptr->next;
        }
        fprintf(stderr, "\n");
    }
};

} // namespace sjtu

#endif
