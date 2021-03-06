//https://code.google.com/p/nya-engine/

#pragma once

#include "log.h"

namespace nya_log
{

class plain_file_log: public log_base
{
public:
    bool open(const char*file_name);
    void close();

private:
    virtual void output(const char *string);

private:
    std::string m_file_name;
    std::string m_scope_tab;
};

}
