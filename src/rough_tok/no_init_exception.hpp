#ifndef NO_INIT_EXCEPTION_INCLUDE_GUARD
#define NO_INIT_EXCEPTION_INCLUDE_GUARD

#include <exception>

namespace trtok {

class no_init_exception: public std::exception {
public:
	no_init_exception(char const *message) : m_message(message) {}

	virtual const char* what() const throw() {
		return m_message;
	}
private:
	char const *m_message;
};

}

#endif
