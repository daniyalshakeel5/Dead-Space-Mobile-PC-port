#pragma once
#include "guest_elf32.hpp"
#include <cstdint>
#include <deque>
#include <map>
#include <string>
namespace xperia {
// ARM32 Bionic pthread descriptors, entirely separate from host x64 threads.
// Workers execute as translated ARM functions through GuestArmCallRunner.
// A non-cooperative ARM worker cannot yet be suspended across a CPU timeslice;
// such workers fail closed rather than claiming completion.
class GuestThreads {
public:
    struct Task {
        std::uint32_t id=0,entry=0,arg=0,result=0;
        bool detached=false,started=false,finished=false;
    };
    static constexpr std::uint32_t default_stack=1024u*1024u;
    static constexpr std::uint32_t default_guard=4096u;
    static constexpr std::uint32_t main_thread=1u;
    static constexpr std::size_t max_live=32u;
    bool create(GuestElf32& elf,std::uint32_t out_thread,
                std::uint32_t attr,std::uint32_t entry,std::uint32_t arg,
                std::uint32_t& rc,std::string& error);
    bool take_next(Task& task) noexcept;
    bool complete(std::uint32_t id,std::uint32_t value) noexcept;
    bool exists(std::uint32_t id) const noexcept;
    bool finished(std::uint32_t id) const noexcept;
    std::uint32_t join(GuestElf32& elf,std::uint32_t id,
                       std::uint32_t retval_addr);
    std::uint32_t detach(std::uint32_t id) noexcept;
    std::uint32_t self() const noexcept {return current_;}
    void set_self(std::uint32_t id) noexcept {current_=id;}
    std::size_t pending() const noexcept {return queue_.size();}
    std::size_t live() const noexcept {return threads_.size();}
    static bool init_attr(GuestElf32& elf,std::uint32_t attr,std::string& error);
    static bool destroy_attr(GuestElf32& elf,std::uint32_t attr,std::string& error);
    static bool attr_field(GuestElf32& elf,std::uint32_t attr,
                           unsigned offset,std::uint32_t& value,std::string& error);
    static bool set_attr_field(GuestElf32& elf,std::uint32_t attr,
                               unsigned offset,std::uint32_t value,std::string& error);
private:
    std::uint32_t next_id_=2,current_=main_thread;
    std::map<std::uint32_t,Task> threads_;
    std::deque<std::uint32_t> queue_;
};
}
