define dq_print
	if $argc == 0
		help dq_print
	else
		set $list = ($arg0->dq_head)
		set $n = 0
		while $list != 0
			printf "item[%u]: %p = ", $n, $list
			output *$list
			echo \n
			set $n++
			set $list = $list->qi_next
		end
	end
end

document dq_print
	Prints members of a DQUEUE
end

define t_print
	if $argc == 0
		help t_print
	else
		set $t = ((thread_t*)$arg0)
		printf "thread %p: '%s'\n", $t, $t->t_name
		printf "status:"
		if ($t->t_flags == 0)
			printf " none"
		end
		if ($t->t_flags & 0x1)
			printf " active"
		end
		if ($t->t_flags & 0x2)
			printf " suspended"
		end
		if ($t->t_flags & 0x4)
			printf " terminating"
		end
		if ($t->t_flags & 0x8)
			printf " zombie"
		end
		if ($t->t_flags & 0x10)
			printf " reschedule"
		end
		if ($t->t_flags & 0x8000)
			printf " kthread"
		end
		printf " process: %p, ", $t->t_process
		printf " priority: %d, affinity: ", $t->t_priority
		if ($t->t_affinity == -1)
			printf "any"
		else
			printf "cpu %u", $t->t_affinity
		end
		echo \n
	end
end

document t_print
	Prints thread members
end

def h_print
	if $argc == 0
		help h_print
	else
		set $h = ((struct HANDLE*)$arg0)
		printf "handle %p: ", $h
		printf "%d", $h->h_type
		if ($h->h_type == 0)
			echo "<unused>"
		end
		if ($h->h_type == 1)
			set $p = (struct VFS_FILE)($h->h_data.d_vfs_file)
			printf "<file>, "
			if ($p.f_dentry != 0)
				printf "entry='%s'", $p.f_dentry->d_entry
			else
				echo (empty dentry)
			end
		end
		if ($h->h_type == 2)
			printf "<pipe>"
		end
		echo \n
	end
end

document h_print
	Prints handle information
end

define p_print
	if $argc == 0
		help p_print
	else
		set $p = ((process_t*)$arg0)
		printf "process %p, refcount %d\n", $p, $p->p_refcount
		printf "parent %p", $p->p_parent
		echo \n
		set $hindex = 0
		while $hindex < 64
			if $p->p_handle[$hindex] != 0
				printf "handle %d, ", $hindex
				h_print $p->p_handle[$hindex]
			end
			set $hindex = $hindex + 1
		end
		echo \n
	end
end

document p_print
	Prints process information
end

define _ts_q
	set $list = $arg0->dq_head
	set $n = 0
	while $list != 0
		set $t = $list->sp_thread 
		printf "(%u) %p : %s\n", $n, $t, $t->t_name
		set $n = $n + 1
		set $list = $list->qi_next
	end
end

define ts
	echo runqueue\n
	_ts_q sched_runqueue
	echo sleepqueue\n
	_ts_q sched_sleepqueue
end

define ps
	set $n = 0
	set $p = process_all.dq_head
	while $p != 0
		printf "(%u) %p : pid %d refcount %d\n", $n, $p, $p->p_pid, $p->p_refcount
		set $t = $p->p_mainthread
		if $t != 0
			printf "  main thread: %p : '%s'\n", $t, $t->t_name
		end
		set $n = $n + 1
		set $p = $p->all_next
	end
end

define t_bt
	if $argc == 0
		help t_bt
	else
		set $t = ((thread_t*)$arg0)
		set $old_ebp = $ebp
		set $old_esp = $esp
		set $esp = $t->md_esp
		set $ebp = *(uint32_t)$esp
		where
		set $ebp = $old_ebp
		set $esp = $old_esp
	end
end

document t_bt
	Performs a backtrace on a thread
end
