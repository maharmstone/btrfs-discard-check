module;

#include <stdexcept>
#include <format>
#include <string>

export module formatted_error;

using namespace std;

export class formatted_error : public exception {
public:
    template<typename... Args>
    formatted_error(format_string<Args...> s, Args&&... args) : msg(format(s, forward<Args>(args)...)) {
    }

    const char* what() const noexcept {
        return msg.c_str();
    }

private:
    string msg;
};
