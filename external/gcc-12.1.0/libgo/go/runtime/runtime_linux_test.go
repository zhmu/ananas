// Copyright 2012 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package runtime_test

import (
	. "runtime"
	"syscall"
	"testing"
	"time"
)

var pid, tid int

func init() {
	// Record pid and tid of init thread for use during test.
	// The call to LockOSThread is just to exercise it;
	// we can't test that it does anything.
	// Instead we're testing that the conditions are good
	// for how it is used in init (must be on main thread).
	pid, tid = syscall.Getpid(), syscall.Gettid()
	LockOSThread()

	sysNanosleep = func(d time.Duration) {
		// Invoke a blocking syscall directly; calling time.Sleep()
		// would deschedule the goroutine instead.
		ts := syscall.NsecToTimespec(d.Nanoseconds())
		for {
			if err := syscall.Nanosleep(&ts, &ts); err != syscall.EINTR {
				return
			}
		}
	}
}

func TestLockOSThread(t *testing.T) {
	if pid != tid {
		t.Fatalf("pid=%d but tid=%d", pid, tid)
	}
}
