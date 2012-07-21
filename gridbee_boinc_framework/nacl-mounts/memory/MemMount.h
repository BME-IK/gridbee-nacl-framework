/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef PACKAGES_LIBRARIES_NACL_MOUNTS_MEMORY_MEMMOUNT_H_
#define PACKAGES_LIBRARIES_NACL_MOUNTS_MEMORY_MEMMOUNT_H_

#include <assert.h>
#include <errno.h>
#include <list>
#include <string>
#include "../base/BaseMount.h"
#include "../util/macros.h"
#include "../util/Path.h"
#include "../util/SlotAllocator.h"
#include "MemNode.h"

// MemMount is a storage mount representing local memory.  The MemMount uses
// the MemNode to represent an inode.
class MemMount: public BaseMount {
 public:
  MemMount();
  virtual ~MemMount() {}

  // Ref() increments the use count of the MemNode corresponding to the inode.
  void Ref(ino_t node);

  // Unref() decrements the use count of the MemNode corresponding to the inode.
  void Unref(ino_t node);

  // Creat() creates a node at path with the given mode and stores the
  // information of that node in st.  0 is returned if the node is
  // successfully created. -1 is returned on failure.
  int Creat(const std::string& path, mode_t mode, struct stat *st);

  // Mkdir() makes a directory at path with the given mode and stores the
  // information of that node in st.  0 is returned if the directory is
  // successfully created. -1 is returned on failure.
  int Mkdir(const std::string& path, mode_t mode, struct stat *st);

  // Return the node corresponding to path.
  int GetNode(const std::string& path, struct stat *st);

  // Remove the node at path.
  int Unlink(const std::string& path);

  // Remove the node directory.  Returns -1 (failure) if node is not
  // a directory.
  int Rmdir(ino_t node);

  int Chmod(ino_t slot, mode_t mode);
  int Stat(ino_t node, struct stat *buf);
  int Getdents(ino_t node, off_t offset, struct dirent *dirp,
               unsigned int count);
  ssize_t Read(ino_t node, off_t offset, void *buf, size_t count);
  ssize_t Write(ino_t node, off_t offset, const void *buf, size_t count);

  // Return the node at path.  If the path is invalid, NULL is returned.
  MemNode *GetMemNode(std::string path);

  // Get the MemNode corresponding to the inode.
  MemNode *ToMemNode(ino_t node) {
    return slots_.At(node);
  }

 private:
  // Return the MemNode corresponding to the inode.
  // Return the node that is a parent of the node at path.
  // If the path is not valid or if the node has no parent,
  // NULL is returned.
  MemNode *GetParentMemNode(std::string path);

  // Get the slot number of the node at path.  If the path
  // is invalid, -1 is returned.
  int GetSlot(std::string path);

  // Get the slot number of the parent of the node at path.
  // If the path is invalid or the node has no parent, -1
  // is returned.
  int GetParentSlot(std::string path);

  MemNode *root_;

  SlotAllocator<MemNode> slots_;

  DISALLOW_COPY_AND_ASSIGN(MemMount);
};

#endif  // PACKAGES_LIBRARIES_NACL_MOUNTS_MEMORY_MEMMOUNT_H_
