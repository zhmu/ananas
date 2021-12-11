#!/usr/bin/env python3

import argparse
import logging
import os
import shutil
import string
import subprocess
import sys

def parse_settings():
    """Returns a dict of key -> value's from settings.sh"""
    settings = {}
    with open(os.path.join('conf', 'settings.sh'), 'rt') as f:
        for s in f:
            n = s.find('#')
            if n >= 0:
                s = s[:n]
            s = s.strip()
            if not s:
                continue
            key, value = s.split('=')
            settings[key] = value

    # substitute any ${...} values
    for key in settings.keys():
        settings[key] = string.Template(settings[key]).substitute(settings)

    # lowercase all keys
    result = {}
    for key, item in settings.items():
        result[key.lower()] = item
    return result

def determine_full_path(path):
    if os.path.isabs(path):
        return path
    return shutil.which(path)

def create_bootstrap_toolchain_file(path, conf):
    with open(path, 'wt') as f:
        f.write('set(CMAKE_SYSTEM_NAME Linux)\n')
        f.write('set(CMAKE_SYSROOT %s)\n' % conf['outdir'])
        f.write('set(CMAKE_WARN_DEPRECATED OFF)\n')
        f.write('include(CMakeForceCompiler)\n')
        f.write('CMAKE_FORCE_C_COMPILER(%s GNU)\n' % conf['gcc'])
        f.write('CMAKE_FORCE_CXX_COMPILER(%s GNU)\n' % conf['gxx'])
        f.write('set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})\n')

def create_final_toolchain_file(path, conf):
    with open(path, 'wt') as f:
        f.write('set(CMAKE_SYSTEM_NAME Linux)\n')
        f.write('set(CMAKE_SYSROOT %s)\n' % conf['outdir'])
        f.write('set(CMAKE_C_COMPILER %s)\n' % conf['gcc'])
        f.write('set(CMAKE_CXX_COMPILER %s)\n' % conf['gxx'])

def build_using_cmake(conf, project_name, src_path):
    work_dir = os.path.join(conf['workdir'], project_name)
    full_src_path = os.path.join(conf['root'], src_path)
    logging.info('cmake-building: %s from %s' % (project_name, full_src_path))

    if not os.path.isdir(work_dir):
        os.makedirs(work_dir)
    cmake_args = [ '-GNinja', '-DCMAKE_BUILD_TYPE=Debug', '-DCMAKE_INSTALL_PREFIX=' + conf['outdir'], '-DCMAKE_TOOLCHAIN_FILE=' + conf['toolchain_txt'], full_src_path ]
    logging.debug('executing cmake in %s with args %s' % (work_dir, cmake_args))
    subprocess.run([ 'cmake' ] + cmake_args, cwd=work_dir, check=True)
    logging.debug('ninja build/install from %s' % work_dir)
    subprocess.run([ 'ninja', 'install' ], cwd=work_dir, check=True)

def build_using_configure_and_make(conf, project_name, src_path, configure_args, build_target='', install_target='install', extra_cflags=''):
    work_dir = os.path.join(conf['workdir'], project_name)
    full_src_path = os.path.join(conf['root'], src_path)
    logging.info('running autoreconf for %s in %s' % (project_name, full_src_path))
    autoreconf = [ 'autoreconf', '-if' ]
    subprocess.run(autoreconf, cwd=full_src_path, check=True)

    logging.info('building %s from %s' % (project_name, full_src_path))
    if not os.path.isdir(work_dir):
        os.makedirs(work_dir)

    configure = [ os.path.join(full_src_path, 'configure') ] + configure_args
    env = os.environ
    env.update({
        'CC': conf['gcc'],
        'CXX': conf['gxx'],
        'CFLAGS': '--sysroot ' + conf['sysrootdir'] + ' ' + extra_cflags
    })
    logging.debug('running %s' % configure)
    subprocess.run(configure, cwd=work_dir, env=env, check=True)
    logging.debug('building %s' % project_name)
    make_cmd = [ 'make', '-j' + conf['makejobs'] ]
    if build_target:
        make_cmd += [ build_target ]
    subprocess.run(make_cmd, cwd=work_dir, env=env, check=True)
    logging.debug('installing %s' % project_name)
    subprocess.run([ 'make', install_target, 'DESTDIR=' + conf['outdir'] ], cwd=work_dir, env=env, check=True)

def build_libgcc(conf, project_name, src_path):
    work_dir = os.path.join(conf['workdir'], project_name)
    full_src_path = os.path.join(conf['root'], src_path)
    logging.info('building %s from %s' % (project_name, full_src_path))
    if not os.path.isdir(work_dir):
        os.makedirs(work_dir)

    # https://linuxfromscratch.org/lfs/view/stable/chapter05/gcc-pass1.html

    configure = [ os.path.join(full_src_path, 'configure'),  "--target=" + conf["target"], "--disable-nls", "--without-headers", "--enable-languages=c", "--prefix=" + conf["toolchaindir"], "--with-gmp=" + conf["toolchaindir"], "--with-newlib", "--disable-threads", "--disable-libstdcxx", "--disable-build-with-cxx", "--disable-libssp", "--disable-libquadmath", "--with-sysroot=" + conf["sysrootdir"], "--with-gxx-include-dir=" + os.path.join(conf["sysrootdir"], "usr", "include" ,"c++", conf["gcc_version"]), "--without-static-standard-libraries", "--enable-initfini-array" ]
    logging.debug('running %s' % configure)
    subprocess.run(configure, cwd=work_dir, check=True)
    logging.debug('building %s' % project_name)
    subprocess.run([ 'make', '-j' + conf['makejobs'], 'all-target-libgcc' ], cwd=work_dir, check=True)
    logging.debug('installing %s' % project_name)
    subprocess.run([ 'make', 'install-target-libgcc' ], cwd=work_dir, check=True)

def build_libstdcxx(conf, project_name, src_path):
    work_dir = os.path.join(conf['workdir'], project_name)
    full_src_path = os.path.join(conf['root'], src_path)
    logging.info('building %s from %s' % (project_name, full_src_path))
    if not os.path.isdir(work_dir):
        os.makedirs(work_dir)

    env = os.environ

    # provide a gthr-default.h for libstdc++; this is inspired by
    # https://www.linuxfromscratch.org/lfs/view/stable/chapter07/gcc-libstdc++-pass2.html
    logging.debug('preparing libstdcxx')
    gthr_default_h = os.path.join(os.path.dirname(src_path), 'libgcc', 'gthr-default.h')
    if not os.path.exists(gthr_default_h):
        os.symlink('gthr-posix.h', gthr_default_h)

    configure = [ os.path.join(full_src_path, 'configure'),  "--host=" + conf["target"], "--target=" + conf["target"], "--prefix=" + os.path.join(conf["sysrootdir"], "usr"), "--enable-initfini-array" ]
    #configure = [ os.path.join(full_src_path, 'configure'),  "--host=" + conf["target"], "--target=" + conf["target"], "--prefix=" + os.path.join(conf["sysrootdir"], "usr"), "--with-sysroot=" + conf["sysrootdir"], "--enable-languages=c,c++" ]
    logging.debug('running %s' % configure)
    subprocess.run(configure, cwd=work_dir, env=env, check=True)
    logging.debug('building %s' % project_name)
    subprocess.run([ 'make', '-j' + conf['makejobs'] ], cwd=work_dir, env=env, check=True)
    logging.debug('installing %s' % project_name)
    subprocess.run([ 'make', 'install' ], cwd=work_dir, env=env, check=True)

def detect_number_of_processors():
    """Returns the number of CPU's detected in this system; defaults to one"""
    try:
        num_processors = 0
        with open('/proc/cpuinfo', 'rt') as f:
            for s in f:
                s = s.split()
                if s and s[0].startswith('processor'):
                    num_processors += 1
        return num_processors
    except:
        return 1

def verify_gcc(gcc):
    """Checks whether the supplied gcc is somewhat sane"""
    try:
        subprocess.run([ gcc, "-v" ], stderr=subprocess.DEVNULL, check=True)
        return True
    except:
        return False

logging.basicConfig(format='%(asctime)s - %(levelname)s: %(message)s', datefmt='%Y-%d-%m %H:%M:%S', level=logging.DEBUG)

parser = argparse.ArgumentParser(description='Ananas build script')
parser.add_argument('-c', '--clean', action='store_true',
                   help='clean before building')
parser.add_argument('-a', '--all', action='store_true', help='build everything')
parser.add_argument('-b', '--bootstrap', action='store_true',
                    help='bootstrap environment (libc, libm, rtld, libstdc++)')
parser.add_argument('-e', '--externals', action='store_true',
                    help='build externals (dash, coreutils)')
parser.add_argument('-k', '--kernel', action='store_true',
                    help='build kernel')
parser.add_argument('-s', '--sysutils', action='store_true',
                    help='build system utilities (init, ps, ...)')
parser.add_argument('-w', '--awe', action='store_true',
                    help='build window environment (awm, ...)')
parser.add_argument('-i', '--image', help='build disk image')

args = parser.parse_args()

conf = parse_settings()

# paths
if 'root' not in conf:
    conf['root'] = os.path.dirname(os.path.realpath(__file__))
if 'outdir' not in conf:
    conf['outdir'] = os.path.join(conf['root'], 'build', 'output')
if 'workdir' not in conf:
    conf['workdir'] = os.path.join(conf['root'], 'build', 'work')
if 'toolchain_txt' not in conf:
    conf['toolchain_txt'] = os.path.join(conf['outdir'], 'toolchain.txt')
if 'sysrootdir' not in conf:
    conf['sysrootdir'] = conf['outdir']

# settings
if 'gcc' not in conf:
    conf['gcc'] = "%s-gcc" % conf['target']
if 'gxx' not in conf:
    conf['gxx'] = "%s-g++" % conf['target']
if 'makejobs' not in conf:
    conf['makejobs'] = str(detect_number_of_processors())

conf['gcc'] = determine_full_path(conf['gcc'])
conf['gxx'] = determine_full_path(conf['gxx'])

assert verify_gcc(conf['gcc']), "cannot invoke '%s' - is your path correct?" % conf['gcc']

if args.clean:
    logging.info('Cleaning build directories...')
    for path in ( conf['outdir'], conf['workdir'] ):
        if os.path.isdir(path):
            shutil.rmtree(path)

for path in ( conf['outdir'], conf['workdir'] ):
    if not os.path.isdir(path):
        os.makedirs(path)
    elif not args.clean:
        logging.warning('Build directory "%s" already present and not cleaned' % path)

targets = {
    'bootstrap': args.bootstrap,
    'externals': args.externals,
    'kernel': args.kernel,
    'sysutils': args.sysutils,
    'awe': args.awe,
    'image': args.image
}
if args.all:
    for key in targets.keys():
        targets[key] = True
    targets['image'] = 'ananas.img'

if targets['bootstrap']:
    logging.info('Bootstrapping...')
    create_bootstrap_toolchain_file(conf['toolchain_txt'], conf)
    build_using_cmake(conf, 'include', 'include')
    build_using_cmake(conf, 'libsyscall', 'lib/libsyscall')

    build_using_cmake(conf, 'newlib-c', 'lib/newlib-4.1.0/newlib/libc')
    build_using_cmake(conf, 'libpthread', 'lib/pthread')
    build_libgcc(conf, 'libgcc', 'external/gcc')
    build_using_cmake(conf, 'newlib-m', 'lib/newlib-4.1.0/newlib/libm')

    # copy libgcc_s.so to the output directory so that the rtld can find it
    shutil.copy(os.path.join(conf['toolchaindir'], conf['target'], 'lib', 'libgcc_s.so'), os.path.join(conf['outdir'], 'usr', 'lib'))

    build_libstdcxx(conf, 'libstdcxx', 'external/gcc/libstdc++-v3')
    create_final_toolchain_file(conf['toolchain_txt'], conf)
    build_using_cmake(conf, 'rtld', 'lib/rtld')

if targets['externals']:
    build_using_configure_and_make(conf, 'dash', 'external/dash', [ '--host=' + conf['target'], '--prefix=/' ], '', 'install', '-DJOBS=0')
    # dash is used as /bin/sh, so link to it
    bin_sh = os.path.join(conf['outdir'], 'bin', 'sh')
    shutil.copy(os.path.join(conf['outdir'], 'bin', 'dash'), bin_sh)
    # TODO: this yields a kernel panic (invalid refcount) - needs to be looked into!
    #if not os.path.exists(bin_sh):
    #   os.symlink(os.path.join(conf['outdir'], 'bin', 'dash'), bin_sh)
    build_using_configure_and_make(conf, 'coreutils', 'external/coreutils', [ '--host=' + conf['target'], '--prefix=/' ])

if targets['sysutils']:
    build_using_cmake(conf, 'sysutils', 'sysutils')

if targets['kernel']:
    build_using_cmake(conf, 'kernel', 'kernel')

if targets['awe']:
    build_using_cmake(conf, 'awe', 'awe')

if targets['image']:
    print('image')
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'scripts'))
    m = __import__('create-disk-image')
    m.create_image(targets['image'], conf['outdir'])
