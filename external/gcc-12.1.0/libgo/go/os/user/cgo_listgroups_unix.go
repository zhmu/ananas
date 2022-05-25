// Copyright 2016 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

//go:build (dragonfly || darwin || freebsd || hurd || (!android && linux) || netbsd || openbsd || (solaris && !illumos)) && cgo && !osusergo

package user

import (
	"fmt"
	"strconv"
	"syscall"
	"unsafe"
)

const maxGroups = 2048

func listGroups(u *User) ([]string, error) {
	ug, err := strconv.Atoi(u.Gid)
	if err != nil {
		return nil, fmt.Errorf("user: list groups for %s: invalid gid %q", u.Username, u.Gid)
	}
	userGID := syscall.Gid_t(ug)
	nameC := make([]byte, len(u.Username)+1)
	copy(nameC, u.Username)

	n := int32(256)
	gidsC := make([]syscall.Gid_t, n)
	rv := getGroupList((*byte)(unsafe.Pointer(&nameC[0])), userGID, &gidsC[0], &n)
	if rv == -1 {
		// Mac is the only Unix that does not set n properly when rv == -1, so
		// we need to use different logic for Mac vs. the other OS's.
		if err := groupRetry(u.Username, nameC, userGID, &gidsC, &n); err != nil {
			return nil, err
		}
	}
	gidsC = gidsC[:n]
	gids := make([]string, 0, n)
	for _, g := range gidsC[:n] {
		gids = append(gids, strconv.Itoa(int(g)))
	}
	return gids, nil
}
