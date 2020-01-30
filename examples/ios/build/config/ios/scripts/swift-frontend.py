# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Frontend for swift compiler as used by GN.
"""

import argparse
import os
import subprocess
import sys

# Extra arguments that are passed to swift compiler.
# FIXME: needs to be derived from config.
EXTRA_ARGUMENTS = [
  '-target',
  'x86_64-apple-ios13.1-simulator',
  '-enable-objc-interop',
  '-sdk',
  '/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator13.1.sdk',
  '-I../../',
  '-F.',
  '-enable-testing',
  '-g',
  '-swift-version',
  '5',
  '-enforce-exclusivity=checked',
  '-Onone',
  '-serialize-debugging-options',
  '-Xcc',
  '-working-directory',
  '-Xcc',
  '.',
  '-enable-anonymous-context-mangled-names',
  '-Xcc',
  '-I../..',
  '-Xcc',
  '-F.',
  '-Xcc',
  '-DDEBUG=1',

  # '-index-store-path',
  # 'Index/DataStore',
  # '-index-system-modules',
  # '-module-cache-path',
  # 'ModuleCache.noindex',
]

class ArgumentParser(argparse.ArgumentParser):

  def convert_arg_line_to_args(self, arg_line):
    return arg_line.split()


class Module(object):

  def __init__(self, name, path):
    self.module_name = name
    self.module_path = path

  def set_sources(self, sources):
    self.sources = map(os.path.abspath, sources)

  def set_object_dir(self, object_dir):
    self.object_dir = object_dir

  def set_header_dir(self, header_dir):
    self.header_dir = header_dir

  def set_bridge_header(self, bridge_header):
    self.bridge_header = bridge_header

  def PathToObject(self, filename, suffix):
    return os.path.join(
        self.object_dir, self.module_name,
        os.path.splitext(os.path.basename(filename))[0] + suffix)

  def CompileSwiftFile(self, filename):
    assert filename in self.sources
    other_files = [ name for name in self.sources if filename != name ]

    subprocess.check_call([
        'swift',
        '-frontend',
        '-c',
        '-primary-file',
        filename,
    ] + other_files + [
        '-emit-module-path',
        self.PathToObject(filename, '~partial.swiftmodule'),
        '-emit-module-doc-path',
        self.PathToObject(filename, '~partial.swiftdoc'),
    ] + EXTRA_ARGUMENTS + [
        '-import-objc-header',
        self.bridge_header,
        '-module-name',
        # '%s.%s' % (self.module_path, self.module_name),
        self.module_name,
        '-o',
        self.PathToObject(filename, '.o'),
    ])

  def MergePartialModule(self):
    subprocess.check_call([
        'swift',
        '-frontend',
        '-merge-modules',
        '-emit-module',
    ] + [ self.PathToObject(filename, '~partial.swiftmodule') for filename in self.sources ] + [
        '-parse-as-library',
        '-sil-merge-partial-modules',
        '-disable-diagnostic-passes',
        '-disable-sil-perf-optzns',
    ] + EXTRA_ARGUMENTS + [
        '-emit-module-doc-path',
        os.path.join(self.object_dir, self.module_name + '.swiftdoc'),
        '-emit-objc-header-path',
        os.path.join(self.header_dir, self.module_name + '-Swift.h'),
    ] + [
        '-import-objc-header',
        self.bridge_header,
        '-module-name',
        # '%s.%s' % (self.module_path, self.module_name),
        self.module_name,
        '-o',
        os.path.join(self.object_dir, self.module_name + '.swiftmodule'),
    ])


def ParseCommandLine(argv):
  parser = ArgumentParser(
      description=__doc__,
      fromfile_prefix_chars='@',
      formatter_class=argparse.ArgumentDefaultsHelpFormatter)
  parser.add_argument(
      '--target-name', required=True,
      help='Name of the Swift module that is compiled')
  parser.add_argument(
      '--bridge-header',
      help='Path to the bridging header used to import Objective-C')
  parser.add_argument(
      '--header-dir', required=True,
      help='Path to the directory where Objective-C headers will be written')
  parser.add_argument(
      '--object-dir', required=True,
      help='Path to the directory where object files will be written')
  parser.add_argument(
      'inputs', nargs='+',
      help='Path to Swift source files to compile')
  return parser.parse_args()


def ModuleNameFromTargetName(target_name):
  return target_name[2:].replace('/', '.').split(':')


def Main(argv):
  args = ParseCommandLine(argv)

  module_path, module_name = ModuleNameFromTargetName(args.target_name)

  partial_dir = os.path.join(args.object_dir, module_name)
  if not os.path.isdir(partial_dir):
    os.makedirs(partial_dir)

  module = Module(module_name, module_path)
  module.set_sources(args.inputs)
  module.set_header_dir(args.header_dir)
  module.set_object_dir(args.object_dir)
  module.set_bridge_header(args.bridge_header)

  for primary_file in module.sources:
    module.CompileSwiftFile(primary_file)
  module.MergePartialModule()


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
