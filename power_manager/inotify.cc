// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/inotify.h"

#include <stdlib.h>
#include <sys/inotify.h>

#include "base/logging.h"

namespace power_manager {

// Buffer size for read of inotify event data.
static const int64 kInotifyBufferSize = 32768;

Inotify::Inotify()
    : channel_(NULL),
      callback_(NULL),
      callback_data_(NULL) {}

Inotify::~Inotify() {
  if (channel_) {
    LOG(INFO) << "cleaning inotify";
    int fd = g_io_channel_unix_get_fd(channel_);
    g_io_channel_shutdown(channel_, true, NULL);
    close(fd);
    LOG(INFO) << "done!";
  }
}

bool Inotify::Init(InotifyCallback cb, gpointer data) {
  int fd = inotify_init();
  if (fd < 0) {
    LOG(ERROR) << "Error in inotify_init";
    return false;
  }
  channel_ = g_io_channel_unix_new(fd);
  if (!channel_) {
    LOG(ERROR) << "Error creating gio channel for Inotify.";
    return false;
  }
  callback_ = cb;
  callback_data_ = data;
  return true;
}

int Inotify::AddWatch(const char* name, int mask) {
  int fd = g_io_channel_unix_get_fd(channel_);
  if (fd < 0) {
    LOG(ERROR) << "Error getting fd";
    return -1;
  }
  LOG(INFO) << "Creating watch for " << name;
  int wd = inotify_add_watch(fd, name, mask);
  if (wd < 0)
    LOG(ERROR) << "Error creating inotify watch for " << name;
  return wd;
}

void Inotify::Start() {
  LOG(INFO) << "Starting Inotify Monitoring!";
  g_io_add_watch(channel_, G_IO_IN, &(Inotify::CallbackHandler), this);
}

gboolean Inotify::CallbackHandler(GIOChannel* source,
                                  GIOCondition condition,
                                  gpointer data) {
  if (G_IO_IN != condition)
    return false;
  Inotify* inotifier = static_cast<Inotify*>(data);
  if (!inotifier) {
    LOG(ERROR) << "Bad callback data!";
    return false;
  }
  InotifyCallback callback = inotifier->callback_;
  gpointer callback_data = inotifier->callback_data_;
  char buf[kInotifyBufferSize];
  guint len;
  GIOError err = g_io_channel_read(source, buf, kInotifyBufferSize, &len);
  if (G_IO_ERROR_NONE != err) {
    LOG(ERROR) << "Error reading from inotify!";
    return false;
  }
  guint i = 0;
  while (i < len) {
    const char* name = "The watch";
    struct inotify_event* event = (struct inotify_event*) &buf[i];
    if (!event) {
      LOG(ERROR) << "garbage inotify_event data!";
      return false;
    }
    if (event->len)
      name = event->name;
    i += sizeof(struct inotify_event) + event->len;
    if (!callback)
      continue;
    if (false == (*callback)(name, event->wd, event->mask, callback_data)) {
      return false;
    }
  }
  return true;
}

} // namespace power_manager
