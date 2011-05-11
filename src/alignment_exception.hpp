#ifndef ALIGNMENT_EXCEPTION_INCLUDE_GUARD
#define ALIGNMENT_EXCEPTION_INCLUDE_GUARD

#include <exception>

namespace trtok {

class alignment_exception: public std::exception {
public:
    alignment_exception(char const *message) : m_message(message) {}

    virtual const char* what() const throw() {
        return m_message;
    }
private:
    char const *m_message;
};

}

#endif
