/****************************************************************************
* GCache                                                                    *
* Author: Federico Ponchio                                                  *
*                                                                           *
* Copyright(C) 2011                                                         *
* Visual Computing Lab                                                      *
* ISTI - Italian National Research Council                                  *
*                                                                           *
* All rights reserved.                                                      *
*                                                                           *
* This program is free software; you can redistribute it and/or modify      *
* it under the terms of the GNU General Public License as published by      *
* the Free Software Foundation; either version 2 of the License, or         *
* (at your option) any later version.                                       *
****************************************************************************/

#ifndef CACHE_DOOR_H
#define CACHE_DOOR_H

#include <condition_variable>
#include <mutex>

// A gate used by cache workers to wait for work and by Controller to pause
// them without racing with a pending enter.
class QDoor {
private:
  std::mutex gate_mutex;
  std::condition_variable gate_changed;
  bool gate_open = false;
  bool gate_locked = false;

public:
  // Held while a cache worker is processing one cycle. Controller may lock
  // this to wait for the worker to leave its current cycle.
  std::mutex room;

  void open() {
    std::lock_guard<std::mutex> lock(gate_mutex);
    if (gate_locked)
      return;
    gate_open = true;
    gate_changed.notify_all();
  }

  void close() {
    std::lock_guard<std::mutex> lock(gate_mutex);
    if (!gate_locked)
      gate_open = false;
  }

  void enter(bool close_after_enter = false) {
    {
      std::unique_lock<std::mutex> lock(gate_mutex);
      gate_changed.wait(lock, [this] { return gate_open && !gate_locked; });
      if (close_after_enter)
        gate_open = false;
    }
    room.lock();
  }

  void leave() {
    room.unlock();
  }

  // Prevent new entrants until unlock; a worker already past the gate is
  // coordinated through room, as in Controller::pause().
  void lock() {
    std::lock_guard<std::mutex> lock(gate_mutex);
    gate_locked = true;
  }

  void unlock(bool open_after_unlock = false) {
    std::lock_guard<std::mutex> lock(gate_mutex);
    gate_open = open_after_unlock;
    gate_locked = false;
    gate_changed.notify_all();
  }

  bool isWaiting() {
    std::lock_guard<std::mutex> lock(gate_mutex);
    return gate_locked || !gate_open;
  }
};

#endif // CACHE_DOOR_H
