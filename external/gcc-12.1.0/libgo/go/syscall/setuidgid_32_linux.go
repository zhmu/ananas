// Copyright 2016 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

//go:build linux && (386 || arm)

package syscall

const (
	sys_GETEUID = SYS_GETEUID32

	sys_SETGID = SYS_SETGID32
	sys_SETUID = SYS_SETUID32
)
