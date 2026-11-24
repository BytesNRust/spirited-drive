# gate_lock.zsh: the gate Mac's visible lock, sourced by gate_main_v2.sh. One heavy run at a time.
#   $GATE_LOCK (default notes/tnfs-35-tools/work/gate.lock) holds, as key=value lines: kind (gate|nightly), branch,
#   commit, start, pid, repo. Anyone may read it (cat); only its owner removes it.
# lock_take KIND WAIT_SECONDS: takes the lock, waiting at most WAIT_SECONDS while another run holds it (0: no wait).
#   A lock whose pid is gone is taken over, with a line saying so. Returns 1 when it is still held after the wait.
# lock_drop: removes the lock if this process owns it.

: ${GATE_LOCK:=${WORKSPACE_ROOT:-$HOME/workspace}/bytes-n-rust/notes/tnfs-35-tools/work/gate.lock}

lock_field() { sed -n "s/^$1=//p" $GATE_LOCK 2>/dev/null | head -1; }

lock_write() {   # atomic: noclobber fails when the file is already there
    ( set -o noclobber
      print -r -- "kind=$1
branch=$(git rev-parse --abbrev-ref HEAD 2>/dev/null)
commit=$(git rev-parse --short HEAD 2>/dev/null)
start=$(date '+%Y-%m-%d %H:%M')
pid=$$
repo=$PWD" > $GATE_LOCK ) 2>/dev/null
}

lock_take() {
    local kind=$1 wait=${2:-0} waited=0 pid
    while ! lock_write $kind; do
        pid=$(lock_field pid)
        if [[ -z $pid ]] || ! kill -0 $pid 2>/dev/null; then
            echo "lock: stale (pid ${pid:-?} is gone: $(lock_field kind) $(lock_field branch) $(lock_field start)), taken over"
            rm -f $GATE_LOCK
            continue
        fi
        if (( waited >= wait )); then
            echo "lock: held by $(lock_field kind) $(lock_field branch) $(lock_field commit) since $(lock_field start) (pid $pid); waited ${waited}s"
            return 1
        fi
        (( waited == 0 )) && echo "lock: waiting for $(lock_field kind) $(lock_field branch) since $(lock_field start) (pid $pid), up to ${wait}s"
        sleep 60; (( waited += 60 ))
    done
    return 0
}

lock_drop() { [[ $(lock_field pid) == $$ ]] && rm -f $GATE_LOCK; return 0; }
