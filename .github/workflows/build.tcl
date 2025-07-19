#!/usr/bin/expect

set timeout 20
spawn sh boot/qemu

# Boot up
expect "Selection: "
send "3\n"
expect "bootfile: "
send "2\n"
expect "[tcp]"
send "tcp\n"

# Build sources
expect "term% "
send "cd /sys/src\n"
expect "term% "
send "objtype=386 mk libs"

# Halt
expect "term% "
send "fshalt\n"
expect "done halting"

# Enter QEMU monitor
send "\x1d" # Ctrl+Alt+2
send "close\n"
