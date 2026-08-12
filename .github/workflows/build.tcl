#!/usr/bin/expect

set timeout 60
spawn sh boot/qemu

# Boot up
expect "term% "

proc terminate {status} {
	# Halt
	expect "term% "
	send "fshalt\r"
	expect "done halting"

	# Enter QEMU monitor
	send "\x1d" ; # Ctrl+Alt+2
	send "close\r"

	exit $status
}

# Build sources
send "cd /sys/src\r"
expect "term% "

set timeout [expr 10*60]
send "objtype=386 mk libs cleanlibs\r"
expect {
	timeout {
		puts stderr "timeout"
		exit 1
	}
	-re "mk: (.+): exit status=(.+)" {
		terminate 1
	}
	"term% "
}

set timeout [expr 2*60*60]
send "mk release\r"
expect {
	timeout {
		puts stderr "timeout"
		exit 1
	}
	-re "mk: (.+): exit status=(.+)" {
		terminate 1
	}
	"term% "
}

set timeout 60
send "echo XXX:\$status\r"
terminate 0
