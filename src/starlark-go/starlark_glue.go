// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

// #include "starlark_glue.h"
import "C"
import (
	"fmt"
	"log"
	"strings"
	"unsafe"

	"go.starlark.net/starlark"
	"go.starlark.net/syntax"
)

type ParseResult struct {
	filename    string
	gnInputFile unsafe.Pointer
	parsedFile  *syntax.File
}

var (
	// TODO: support deleting parse results.
	parseResults                = map[uint32]ParseResult{}
	nextParseResultIndex uint32 = 0
	// Known GN targets.
	// https://stackoverflow.com/questions/34018908/golang-why-dont-we-have-a-set-datastructure#comment89711306_34020023
	targets = map[string]struct{}{
		"group":      struct{}{},
		"executable": struct{}{},
	}
)

func isPredeclared(name string) bool {
	_, ok := targets[name]
	return ok
}

func runGNFunction(scope unsafe.Pointer, thread *starlark.Thread, fn *starlark.Builtin, args starlark.Tuple, kwargs []starlark.Tuple) (starlark.Value, error) {
	targetType := fn.Name()
	fmt.Printf("\nTarget %s:\n", targetType)

	blockNode := C.create_block_node()
	//defer C.delete_block_node(blockNode) // run_gn_function eats this so we shouldn't need to care about ownership..?

	var targetName string
	for _, kwarg := range kwargs {
		k := string(kwarg[0].(starlark.String))
		v := kwarg[1]
		if k == "name" {
			// TODO: handle failure
			targetName = string(kwarg[1].(starlark.String))
			continue
		}
		fmt.Printf("%s = %v\n", k, v)

		switch v.(type) {
		case starlark.Bool:
			if v.Truth() {
				C.append_bool_assign_to_block_node(blockNode, C.CString(k), 1)
			} else {
				C.append_bool_assign_to_block_node(blockNode, C.CString(k), 0)
			}
		case starlark.String:
			// Using .String() will give the quoted version.
			// TODO: this may be differently-quoted from how GN expects?
			C.append_quoted_string_assign_to_block_node(blockNode, C.CString(k), C.CString(v.String()))
		case *starlark.List:
			listNode := C.create_list_node()
			list := v.(*starlark.List)
			iter := list.Iterate()
			var listValue starlark.Value
			defer iter.Done()
			for iter.Next(&listValue) {
				if str, ok := starlark.AsString(listValue); ok {
					// TODO: free all other CStrings
					quotedStr := C.CString("\"" + str + "\"")
					// defer C.free(unsafe.Pointer(quotedStr))
					C.append_quoted_string_to_list_node(listNode, quotedStr)
				} else {
					log.Fatalf("only strings in lists are supported currently\n")
				}
			}
			C.append_list_assign_to_block_node(blockNode, C.CString(k), listNode)
		default:
			log.Fatalf("unknown type %T\n", v)
		}
	}

	if targetName == "" {
		return starlark.None, fmt.Errorf("target has no name")
	}

	targetNameQuoted := C.CString("\"" + targetName + "\"")
	// defer C.free(unsafe.Pointer(targetNameQuoted))
	C.run_gn_function(scope, C.CString(targetType), targetNameQuoted, blockNode)

	return starlark.None, nil
}

func runGNImport(scope unsafe.Pointer, filename string) (starlark.StringDict, error) {
	importResults := make(starlark.StringDict)

	filenameQuoted := C.CString("\"" + filename + "\"")
	// TODO: handle error
	result := C.run_gn_import(scope, filenameQuoted)
	templateNames := unsafe.Slice(result.template_names, result.template_count)
	for _, templateNameC := range templateNames {
		templateName := C.GoString(templateNameC)
		fmt.Printf("declared %s\n", templateName)
		importResults[templateName] = starlark.NewBuiltin(templateName, func(thread *starlark.Thread, fn *starlark.Builtin, args starlark.Tuple, kwargs []starlark.Tuple) (starlark.Value, error) {
			return runGNFunction(scope, thread, fn, args, kwargs)
		})
	}
	return importResults, nil
}

//export ExecStarlark
func ExecStarlark(scope unsafe.Pointer, handle uint32) {
	// TODO: ensure handle is correct?
	parseResult := parseResults[handle]
	thread := &starlark.Thread{
		Name: "stargn (" + parseResult.filename + ")",
		Print: func(thread *starlark.Thread, msg string) {
			fmt.Println("%s: %s", thread.Name, msg)
		},
		Load: func(thread *starlark.Thread, module string) (starlark.StringDict, error) {
			if strings.HasSuffix(module, ".gni") {
				return runGNImport(scope, module)
			}
			return nil, fmt.Errorf("Only .gni loads are supported")
		},
	}
	predeclared := starlark.StringDict{
		"component": starlark.NewBuiltin("component", func(thread *starlark.Thread, fn *starlark.Builtin, args starlark.Tuple, kwargs []starlark.Tuple) (starlark.Value, error) {
			return runGNFunction(scope, thread, fn, args, kwargs)
		}),
		"executable": starlark.NewBuiltin("executable", func(thread *starlark.Thread, fn *starlark.Builtin, args starlark.Tuple, kwargs []starlark.Tuple) (starlark.Value, error) {
			return runGNFunction(scope, thread, fn, args, kwargs)
		}),
		"group": starlark.NewBuiltin("group", func(thread *starlark.Thread, fn *starlark.Builtin, args starlark.Tuple, kwargs []starlark.Tuple) (starlark.Value, error) {
			return runGNFunction(scope, thread, fn, args, kwargs)
		}),
		"source_set": starlark.NewBuiltin("source_set", func(thread *starlark.Thread, fn *starlark.Builtin, args starlark.Tuple, kwargs []starlark.Tuple) (starlark.Value, error) {
			return runGNFunction(scope, thread, fn, args, kwargs)
		}),
	}
	program, err := starlark.FileProgram(parseResult.parsedFile, predeclared.Has)
	if err != nil {
		// TODO: handle better?
		log.Fatal(err)
	}
	_, err = program.Init(thread, predeclared)
	if err != nil {
		if evalErr, ok := err.(*starlark.EvalError); ok {
			log.Fatal(evalErr.Backtrace())
		}
		log.Fatal(err)
	}
	// fmt.Println("\nGlobals:")
	// for _, name := range globals.Keys() {
	// 	v := globals[name]
	// 	fmt.Printf("%s (%s) = %s\n", name, v.Type(), v.String())
	// }
}

//export ParseStarlark
func ParseStarlark(gnInputFile unsafe.Pointer, s *C.char, f *C.char) uint32 {
	src := C.GoString(s)
	filename := C.GoString(f)
	parsedFile, err := syntax.LegacyFileOptions().Parse(filename, src, 0)
	if err != nil {
		// TODO: handle better?
		log.Fatal(err)
	}
	thisParseResultIndex := nextParseResultIndex
	parseResults[thisParseResultIndex] = ParseResult{filename: filename, gnInputFile: gnInputFile, parsedFile: parsedFile}
	nextParseResultIndex++
	return thisParseResultIndex
}
