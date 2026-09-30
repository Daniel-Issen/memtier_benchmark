/*
 * Copyright (C) 2011-2026 Redis Labs Ltd.
 *
 * This file is part of memtier_benchmark.
 *
 * memtier_benchmark is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2.
 *
 * memtier_benchmark is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with memtier_benchmark.  If not, see <http://www.gnu.org/licenses/>.
 */

// Test-only LD_PRELOAD shim: let the worker record a partial bucket, return
// cleanly through libevent, then exercise either C++ exception handler.
#include <atomic>
#include <cstdlib>
#include <dlfcn.h>
#include <event2/event.h>
#include <stdexcept>

static std::atomic<bool> injected(false);

static void stop_worker(evutil_socket_t, short, void *arg)
{
    event_base_loopbreak(static_cast<event_base *>(arg));
}

extern "C" int event_base_dispatch(event_base *base)
{
    typedef int (*dispatch_fn)(event_base *);
    dispatch_fn dispatch = reinterpret_cast<dispatch_fn>(dlsym(RTLD_NEXT, "event_base_dispatch"));
    if (!dispatch) std::abort();
    bool inject = !injected.exchange(true);
    if (inject) {
        timeval delay = {0, 500000};
        if (event_base_once(base, -1, EV_TIMEOUT, stop_worker, base, &delay) != 0) std::abort();
    }
    int result = dispatch(base);
    if (inject) {
        if (std::getenv("MEMTIER_TEST_UNKNOWN_EXCEPTION")) throw 1;
        throw std::runtime_error("injected worker exception");
    }
    return result;
}
