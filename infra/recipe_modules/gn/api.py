# Copyright 20189 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""The `gn` module checks out, builds, and tests GN."""

from recipe_engine import recipe_api


class GnApi(recipe_api.RecipeApi):
  """API for checking out, building, and testing GN."""

  def __init__(self, *args, **kwargs):
    super(GnApi, self).__init__(*args, **kwargs)

    self._src_dir = None
    self._cipd_dir = None
    self._build_input = None
    self._env = None

  @property
  def build_input(self):
    return self._build_input

  def checkout(self, src_dir, repository):
    """Checkout GN codebase at a particular revision
    Args:
      src_dir (Path): Path for checkout.
      repository (string): URL for Git repository to checkout.
    """
    with self.m.step.nest('git'), self.m.context(infra_steps=True):
      self.m.step('init', ['git', 'init', src_dir])

      with self.m.context(cwd=src_dir):
        build_input = self.m.buildbucket.build_input
        ref = (
            build_input.gitiles_commit.id
            if build_input.gitiles_commit else 'refs/heads/master')
        # Fetch tags so `git describe` works.
        self.m.step('fetch', ['git', 'fetch', '--tags', repository, ref])
        self.m.step('checkout', ['git', 'checkout', 'FETCH_HEAD'])
        for change in build_input.gerrit_changes:
          self.m.step('fetch %s/%s' % (change.change, change.patchset), [
              'git', 'fetch', repository,
              'refs/changes/%s/%s/%s' %
              (str(change.change)[-2:], change.change, change.patchset)
          ])
          self.m.step('cherry-pick %s/%s' % (change.change, change.patchset),
                      ['git', 'cherry-pick', 'FETCH_HEAD'])
    self._src_dir = src_dir
    self._build_input = build_input

  def ensure(self, cipd_dir):
    """Ensure necessary tools from CIPD are present, and if not download them.
    Args:
      cipd_dir (Path): Path for CIPD prebuilts.
    """
    with self.m.context(infra_steps=True):
      pkgs = self.m.cipd.EnsureFile()
      pkgs.add_package('infra/ninja/${platform}', 'version:1.8.2')
      if self.m.platform.is_linux or self.m.platform.is_mac:
        pkgs.add_package('fuchsia/clang/${platform}', 'goma')
      if self.m.platform.is_linux:
        pkgs.add_package(
            'fuchsia/sysroot/${platform}',
            'git_revision:a28dfa20af063e5ca00634024c85732e20220419', 'sysroot')
      self.m.cipd.ensure(cipd_dir, pkgs)

    self._cipd_dir = cipd_dir

  def build(self, build_name, build_args, run_tests=False):
    """Build GN and unittests."""
    assert (self._src_dir)
    assert (self._cipd_dir)

    env = self._setup_env()

    with self.m.step.nest(build_name):
      with self.m.step.nest('build'), self.m.context(
          env=self._env, cwd=self._src_dir):
        self.m.python(
            'generate', self._src_dir.join('build', 'gen.py'), args=build_args)

        # Windows requires the environment modifications when building too.
        self.m.step(
            'ninja',
            [self._cipd_dir.join('ninja'), '-C',
             self._src_dir.join('out')])

      if (run_tests):
        self.m.step('test', [self._src_dir.join('out', 'gn_unittests')])

  def _setup_env(self):
    if self._env:
      return

    if self.m.platform.is_linux:
      sysroot = '--sysroot=%s' % self._cipd_dir.join('sysroot')
      self._env = {
          'CC': self._cipd_dir.join('bin', 'clang'),
          'CXX': self._cipd_dir.join('bin', 'clang++'),
          'AR': self._cipd_dir.join('bin', 'llvm-ar'),
          'CFLAGS': sysroot,
          'LDFLAGS': sysroot,
      }
    elif self.m.platform.is_mac:
      sysroot = '--sysroot=%s' % self.m.step(
          'xcrun', ['xcrun', '--show-sdk-path'],
          stdout=self.m.raw_io.output(name='sdk-path', add_output_log=True),
          step_test_data=
          lambda: self.m.raw_io.test_api.stream_output('/some/xcode/path')
      ).stdout.strip()
      stdlib = '%s %s %s' % (self._cipd_dir.join('lib', 'libc++.a'),
                             self._cipd_dir.join('lib', 'libc++abi.a'),
                             self._cipd_dir.join('lib', 'libunwind.a'))
      self._env = {
          'CC': self._cipd_dir.join('bin', 'clang'),
          'CXX': self._cipd_dir.join('bin', 'clang++'),
          'AR': self._cipd_dir.join('bin', 'llvm-ar'),
          'CFLAGS': sysroot,
          'LDFLAGS': '%s -nostdlib++ %s' % (sysroot, stdlib),
      }
    else:
      self._env = {}
