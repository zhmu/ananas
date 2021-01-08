#!/usr/bin/env python3

import sys

def patch_config_sub(source_path, dest_path):
    num_added = 0
    with open(source_path, 'rt') as in_f:
        with open(dest_path, 'wt') as out_f:
            for s in in_f:
                if s.strip() == '-none)':
                    out_f.write('''    -ananas)
        ;;
''')
                    num_added += 1
                out_f.write(s)

    if num_added != 1:
        raise Exception('expected to add 1 time, added %d time(s)' % num_added)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('usage: %s config.sub config.sub.out' % sys.argv[0])
        sys.exit(1)
    patch_config_sub(sys.argv[1], sys.argv[2])
