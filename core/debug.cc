#include <cstring>
#include <cstdarg>
#include <iostream>
#include <iomanip>
#include "boost/format.hpp"
#include "drivers/console.hh"
#include "sched.hh"
#include "debug.hh"
#include "osv/debug.h"

using namespace std;

logger* logger::_instance = nullptr;

logger::logger()
{
    this->parse_configuration();
}

logger::~logger()
{

}

bool logger::parse_configuration(void)
{
    // FIXME: read configuration from a file
    add_tag("virtio", logger_info);
    add_tag("virtio-blk", logger_info);
    add_tag("virtio-net", logger_info);
    add_tag("pci", logger_info);
    add_tag("poll", logger_info);

    // Tests
    add_tag("tst-eventlist", logger_none);
    add_tag("tst-rwlock", logger_none);
    add_tag("tst-bsd-netdriver", logger_debug);
    add_tag("tst-virtionet", logger_debug);

    return (true);
}

void logger::add_tag(const char* tag, logger_severity severity)
{
    auto it = _log_level.find(tag);
    if (it != _log_level.end()) {
        _log_level.erase(it);
    }

    _log_level.insert(make_pair(tag, severity));
}

logger_severity logger::get_tag(const char* tag)
{
    auto it = _log_level.find(tag);
    if (it == _log_level.end()) {
        return (logger_error);
    }

    return (it->second);
}

bool logger::is_filtered(const char *tag, logger_severity severity)
{
    logger_severity configured_severity = this->get_tag(tag);
    if (configured_severity == logger_none) {
        return (true);
    }

    if (severity < configured_severity) {
        return (true);
    }

    return (false);
}

const char* logger::loggable_severity(logger_severity severity)
{
    const char *ret = "-";
    switch (severity) {
    case logger_debug:
        ret = "D";
        break;
    case logger_info:
        ret = "I";
        break;
    case logger_warn:
        ret = "W";
        break;
    case logger_error:
        ret = "E";
        break;
    case logger_none:
        break;
    }

    return (ret);
}

void logger::wrt(const char* tag, logger_severity severity, const boost::format& _fmt)
{
    if (this->is_filtered(tag, severity)) {
        return;
    }

    unsigned long tid = sched::thread::current()->id();
    debug(fmt("[%s/%d %s]: ") % loggable_severity(severity) % tid % tag, false);
    debug(_fmt, true);
}

void logger::wrt(const char* tag, logger_severity severity, const char* _fmt, ...)
{
    va_list ap;
    va_start(ap, _fmt);
    this->wrt(tag, severity, _fmt, ap);
    va_end(ap);
}

void logger::wrt(const char* tag, logger_severity severity, const char* _fmt, va_list ap)
{
    if (this->is_filtered(tag, severity)) {
        return;
    }

    unsigned long tid = sched::thread::current()->id();
    kprintf("[%s/%d %s]: ", loggable_severity(severity), tid, tag);
    vkprintf(_fmt, ap);
    kprintf("\n");
}

extern "C" {
void tprintf(const char* tag, logger_severity severity, const char* _fmt, ...)
{
    va_list ap;
    va_start(ap, _fmt);
    logger::instance()->wrt(tag, severity, _fmt, ap);
    va_end(ap);
}
}
void debug(std::string str, bool lf)
{
    console::write(str.c_str(), str.length(), lf);
}

void debug(const boost::format& fmt, bool lf)
{
    debug(fmt.str(), lf);
}

extern "C" {

    void debug(const char *msg)
    {
        console::write(msg, strlen(msg), true);
    }

    void readln(char *msg, size_t size)
    {
        console::read(msg, size);
    }

    void debug_write(const char *msg, size_t len)
    {
        console::write(msg, len, false);
    }

}
