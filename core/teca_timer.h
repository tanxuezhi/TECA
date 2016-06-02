#ifndef teca_timer_h
#define teca_timer_h

#include "teca_common.h"
#include <chrono>
#include <vector>
#include <iostream>

// a high resolutioin timer
class teca_timer
{
public:
    using tick_t = std::chrono::duration<double,
        std::chrono::seconds::period>;

    using mark_t = std::chrono::high_resolution_clock::time_point;

    void start_event(const char *name)
    {
        m_time_mark.emplace_back(std::chrono::high_resolution_clock::now());
        m_mark_name.push_back(name);
    }

    void end_event(const char *name)
    {
        const char *mark_name = m_mark_name.back();
        m_mark_name.pop_back();
        if (strcmp(name, mark_name))
        {
            TECA_ERROR("unmatched timer event \"" << mark_name
                << "\" matched to \"" << name << "\"")
            return;
        }

        teca_timer::tick_t delta(
            std::chrono::high_resolution_clock::now() - m_time_mark.back());

        m_time_mark.pop_back();

        std::cerr << name << " = " << delta.count() << std::endl;
    }

private:
    std::vector<const char *> m_mark_name;
    std::vector<teca_timer::mark_t> m_time_mark;
};

#endif
