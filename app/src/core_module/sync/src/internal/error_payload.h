#pragma once
#include <src/sync_types.h>

namespace orion::sync {

class ErrorPayload : public Payload {
public:
    ErrorPayload(const Error error) :
        Payload() {m_error = error;}
    virtual void deserialize(const std::string& data) {/*noop*/};
    virtual const std::string payload() const {return std::string();}
};

} //orion::sync