#ifndef ReadError_H
#define ReadError_H

#include <string>

class ReadError {
private:
    std::string myMessage;
public:
    explicit ReadError(const std::string& message);
    const std::string& message() const {return myMessage;}
};

#endif /* ReadError_H */