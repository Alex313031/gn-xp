// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

/*
#include <stdlib.h>
#include <stdio.h>
#include "stargn_main.h"
*/
import "C"
import (
	"os"
	"unsafe"
)

func main() {
	argc := C.int(len(os.Args))
	argv := make([]*C.char, len(os.Args))
	for i, arg := range os.Args {
		argv[i] = C.CString(arg)
		defer C.free(unsafe.Pointer(argv[i]))
	}
	C.callMain(argc, &argv[0])
}
