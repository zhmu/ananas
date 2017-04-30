node('ananas-llvm') {
	stage 'Fetching code'
		checkout scm

	stage 'Preparing environment'
		sh 'toolchain/make-clang-links.sh /opt/llvm/bin amd64 toolchain'
		/*
		 * XXX This is crude, but we need some way to grab the distfiles/ content
		 *     so we won't keep grabbing them from external sources...
		 */
		dir('apps/distfiles') {
			sh 'cp /home/bob/distfiles/* .'
		}

	stage 'Generating files'
		dir('kern') {
			sh 'make'
		}

	stage 'Building userland components'
		withEnv([ "PATH+llvm=/home/bob/jenkins/workspace/Ananas/toolchain/", "TARGET=/tmp/ananas-output" ]) {
			sh 'mkdir -p sysroot.amd64'
			dir('scripts') {
				sh './build-apps.sh'
			}
		}

	stage 'Building multiboot stub'
		withEnv([ "CC=../toolchain/x86_64-ananas-elf-clang" ]) {
			dir('multiboot-stub') {
				sh 'make'
			}
		}

	stage 'Building kernel'
		withEnv([ "PATH+llvm=/home/bob/jenkins/workspace/Ananas/toolchain/" ]) {
			dir('kernel/tools/config') {
				sh 'make'
			}
			dir('kernel/arch/amd64/conf') {
				sh '../../../tools/config/config LINT'
			}
			dir('kernel/arch/amd64/compile/LINT') {
				sh 'make'
				sh 'cat ../../../../../multiboot-stub/mbstub kernel > /tmp/ananas-output/kernel'
			}
		}
}
