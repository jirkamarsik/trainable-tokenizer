#ifndef PIPE_INCLUDE_GUARD
#define PIPE_INCLUDE_GUARD

// Copyright (C) Alexander Nasonov, 2003. Permission to copy, use, modify,
// sell and distribute this software is granted provided this copyright notice
// appears in all copies. This software is provided "as is" without express or
// implied warranty, and with no claim as to its suitability for any purpose.

#include <cassert>
#include <istream>
#include <ostream>

#include <boost/config.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition.hpp>

namespace pipes {

template<class Ch, class Tr = std::char_traits<Ch> >
class basic_ipipestream;

template<class Ch, class Tr = std::char_traits<Ch> >
class basic_opipestream;

template<class Ch, class Tr = std::char_traits<Ch> >
class basic_pipe;


template<class Ch, class Tr = std::char_traits<Ch> >
class basic_pipestreambuf : public std::basic_streambuf<Ch,Tr>
{
    struct block
    {
        union
        {
            Ch data;
            block* next;
        };

        block() : next(0) {}

        Ch* begin()
        {
            return &((this + 1)->data);
        }

        Ch* end()
        {
            return begin() + chars_in_block;
        }
    };

    enum state { not_opened, opened, closed };
    // closed != not_opened because opipestream
    // may be closed before ipipestream is opened.

    BOOST_STATIC_CONSTANT(std::size_t, chars_in_block = 500);

    block* m_gblock;
    block* m_pblock;

    state m_gstate;
    state m_pstate;
    bool m_limited_capacity; // maximum 2 blocks

    boost::mutex m_mutex;
    boost::condition m_cond;

    friend class basic_pipe<Ch,Tr>;
    friend class basic_opipestream<Ch,Tr>;
    friend class basic_ipipestream<Ch,Tr>;

  public:

    typedef typename Tr::int_type int_type;

    basic_pipestreambuf(bool limited_capacity)
        : m_gblock(0)
        , m_pblock(0)
        , m_gstate(not_opened)
        , m_pstate(not_opened)
        , m_limited_capacity(limited_capacity)
    {
    }

    ~basic_pipestreambuf()
    {
        // No lock because streams should be already destroyed
        assert(m_gstate == not_opened && m_pstate == not_opened);
        destroy_blocks();
    }

  private:

    void init_blocks()
    {
        assert(m_pblock == 0 && m_gblock == 0);
        m_gblock = m_pblock = allocate();
        this->setp(m_pblock->begin(), m_pblock->end());
        this->setg(m_gblock->begin(), m_gblock->begin(), m_gblock->begin());
    }

    void destroy_blocks()
    {
        m_pblock = 0;
        this->setp(0, 0);
        this->setg(0, 0, 0);

        while(m_gblock)
        {
            block* next = m_gblock->next;
            deallocate(m_gblock);
            m_gblock = next;
        }
    }

    void invariants()
    {
        // Blocks are ok
        assert(m_gblock && m_pblock); // at least one block
        assert(m_pblock->next == 0); // really last block
        if(m_limited_capacity) // maximum 2 blocks in this case
            assert(m_gblock == m_pblock || m_gblock->next == m_pblock);

        // Get area in m_gblock
        assert(this->eback() == m_gblock->begin());
        assert(this->egptr() <= m_gblock->end());

        // Put area in m_pblock
        assert(this->pbase() >= m_pblock->begin());
        assert(this->epptr() == m_pblock->end());
    }

    static block* allocate()
    {
        const std::size_t char_bytes = sizeof(Ch) * chars_in_block;
        void* mem = ::operator new(sizeof(block) + char_bytes);
        return new(mem) block();
    }

    static void deallocate(block* p)
    {
        ::operator delete(static_cast<void*>(p));
    }

    void connect(basic_ipipestream<Ch,Tr>& in)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        assert(in.rdbuf() == 0); // disconnected

        while(m_gstate != not_opened)
            m_cond.wait(lock);

        if(m_pstate == not_opened)
            init_blocks();

        in.setstate(std::ios_base::goodbit);
        in.rdbuf(this);
        m_gstate = opened;
        invariants();
    }

    void connect(basic_opipestream<Ch,Tr>& out)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        assert(out.rdbuf() == 0); // disconnected

        while(m_pstate != not_opened)
            m_cond.wait(lock);

        if(m_gstate == not_opened)
            init_blocks();

        out.setstate(std::ios_base::goodbit);
        out.rdbuf(this);
        m_pstate = opened;
        invariants();
    }

    void disconnect(basic_ipipestream<Ch,Tr>& in)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        assert(in.rdbuf() == this);
        invariants();
        in.rdbuf(0);

        if(m_pstate != closed)
            m_gstate = closed;
        else
        {
            m_gstate = m_pstate = not_opened;
            destroy_blocks();
            m_cond.notify_one(); // notify connect(out)
        }
    }

    void disconnect(basic_opipestream<Ch,Tr>& out)
    {
        boost::mutex::scoped_lock lock(m_mutex);
        assert(out.rdbuf() == this);
        invariants();
        out.rdbuf(0);
        this->setp(this->pptr(), this->epptr()); // shift pbase

        if(m_gstate != closed)
            m_pstate = closed;
        else
        {
            m_gstate = m_pstate = not_opened;
            destroy_blocks();
        }

        // Notify underflow() about EOF or connect(in).
        m_cond.notify_one();
    }

    virtual int sync()
    {
        // Only ostream thread calls sync.
        boost::mutex::scoped_lock lock(m_mutex);
        invariants(); // before sync
        this->setp(this->pptr(), this->epptr()); // shift pbase
        m_cond.notify_one(); // notify underflow()
        invariants(); // after sync
        return 0;
    }

    virtual int_type overflow(int_type ch)
    {
        // Only ostream thread calls overflow.
        boost::mutex::scoped_lock lock(m_mutex);
        invariants(); // before overflow

        while(m_limited_capacity && m_pblock != m_gblock)
            m_cond.wait(lock);

        m_pblock->next = allocate();
        m_pblock = m_pblock->next;
        this->setp(m_pblock->begin(), m_pblock->end());
        this->sputc(Tr::to_char_type(ch));
        m_cond.notify_one(); // notify about ch
        invariants(); // after overflow
        return ch;
    }

    virtual int_type underflow()
    {
        // Only istream thread calls underflow. I assume that
        // two threads can read this->pbase() without a lock.
        boost::mutex::scoped_lock lock(m_mutex);
        invariants(); // before underflow

        while(true) // break when has chars including EOF
        {
            if(m_gblock == m_pblock)
            {
                // label 1 (used below)
                if(this->gptr() == this->pbase() && m_pstate != closed)
                    m_cond.wait(lock);
                else
                {
                    this->setg(this->eback(), this->gptr(), this->pbase());
                    break;
                }
            }
            else if(this->gptr() != m_gblock->end())
            {
                this->setg(this->eback(), this->gptr(), m_gblock->end());
                break;
            }
            else
            {
                // deallocate m_gblock
                block* next = m_gblock->next;
                deallocate(m_gblock);
                m_gblock = next;
                Ch* begin = m_gblock->begin();
                if(m_gblock == m_pblock)
                {
                    m_cond.notify_one(); // notify overflow(ch)
                    this->setg(begin, begin, this->pbase()); // goto 1
                }
                else
                {
                    this->setg(begin, begin, m_gblock->end());
                    break;
                }
            }
        } // while(true)

        invariants(); // after underflow
        return (this->gptr() != this->egptr()) ? this->sgetc() : Tr::eof();
    }
};


template<class Ch, class Tr>
class basic_opipestream : public std::basic_ostream<Ch,Tr>
{
    typedef basic_pipe<Ch,Tr> pipe_type;
    typedef basic_pipestreambuf<Ch,Tr> streambuf_type;

    friend class basic_pipe<Ch,Tr>;

    streambuf_type* pipestreambuf() const
    {
        if(std::basic_streambuf<Ch,Tr>* buf = this->rdbuf())
        {
            assert(typeid(*buf) == typeid(streambuf_type));
            return static_cast<streambuf_type*>(buf);
        }
        return 0;
    }

  public:

    basic_opipestream(pipe_type& pipe)
        : std::basic_ostream<Ch,Tr>(0)
    {
        pipe.m_buf.connect(*this);
    }

    ~basic_opipestream()
    {
        close();
    }

    bool is_open() const
    {
        return pipestreambuf();
    }

    void open(basic_pipe<Ch,Tr>& pipe)
    {
        if(is_open())
            this->setstate(std::ios_base::failbit);
        else
            pipe.m_buf.connect(*this);
    }

    void close()
    {
        if(streambuf_type* buf = pipestreambuf())
            buf->disconnect(*this);
        else
            this->setstate(std::ios_base::failbit);
    }
};


template<class Ch, class Tr>
class basic_ipipestream : public std::basic_istream<Ch,Tr>
{
    typedef basic_pipe<Ch,Tr> pipe_type;
    typedef basic_pipestreambuf<Ch,Tr> streambuf_type;

    streambuf_type* pipestreambuf() const
    {
        if(std::basic_streambuf<Ch,Tr>* buf = this->rdbuf())
        {
            assert(typeid(*buf) == typeid(streambuf_type));
            return static_cast<streambuf_type*>(buf);
        }
        return 0;
    }

    friend class basic_pipe<Ch,Tr>;

  public:

    basic_ipipestream(pipe_type& pipe)
        : std::basic_istream<Ch,Tr>(0)
    {
        pipe.m_buf.connect(*this);
    }

    ~basic_ipipestream()
    {
        close();
    }

    bool is_open() const
    {
        return pipestreambuf();
    }

    void open(basic_pipe<Ch,Tr>& pipe)
    {
        if(is_open())
            this->setstate(std::ios_base::failbit);
        else
            pipe.m_buf.connect(*this);
    }

    void close()
    {
        if(streambuf_type* buf = pipestreambuf())
            buf->disconnect(*this);
        else
            this->setstate(std::ios_base::failbit);
    }
};


template<class Ch, class Tr>
class basic_pipe : private boost::noncopyable
{
    basic_pipestreambuf<Ch,Tr> m_buf;

    friend class basic_opipestream<Ch,Tr>;
    friend class basic_ipipestream<Ch,Tr>;

  public:

    enum capacity_type { limited_capacity, unlimited_capacity };

    basic_pipe(capacity_type cap = limited_capacity)
        : m_buf(cap == limited_capacity) {}

    ~basic_pipe() {}
};

typedef basic_pipe<char> pipe;
typedef basic_ipipestream<char> ipipestream;
typedef basic_opipestream<char> opipestream;

} // namespace pipes

#endif
