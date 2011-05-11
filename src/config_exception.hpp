#ifndef CONFIG_EXCEPTION_INCLUDE_GUARD
#define CONFIG_EXCEPTION_INCLUDE_GUARD

#include <exception>

namespace trtok {

class config_exception: public std::exception {
public:
    config_exception(char const *message) : m_message(message) {}

    virtual const char* what() const throw() {
        return m_message;
    }
private:
    char const *m_message;
};

}

#endif
