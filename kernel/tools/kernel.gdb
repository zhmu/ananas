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
		printf "thread %p: ", $t
		if $t->t_threadinfo != 0
			output $t->t_threadinfo->ti_args
		else
			echo <unknown>
		end
		echo \n
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
		printf " priority: %d, affinity: ", $t->t_priority
		if ($t->t_affinity == -1)
			printf "any"
		else
			printf "cpu %u", $t->t_affinity
		end
		echo \n
		output/x *$t
		echo \n
	end
end

document t_print
	Prints thread members
end

define _ps_q
	set $list = $arg0->dq_head
	set $n = 0
	while $list != 0
		set $t = $list->sp_thread 
		printf "(%u) %p: ", $n, $t
		if $t->t_threadinfo != 0
			output $t->t_threadinfo->ti_args
		else
			echo <unknown>
		end
		echo \n
		set $n = $n + 1
		set $list = $list->qi_next
	end
end

define ps
	echo runqueue\n
	_ps_q sched_runqueue
	echo sleepqueue\n
	_ps_q sched_sleepqueue
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
