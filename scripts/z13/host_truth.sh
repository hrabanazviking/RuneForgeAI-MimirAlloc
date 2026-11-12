#!/bin/sh
# MimirAlloc Z13 host-truth capture (Z-002).
# Prints the environment header that every later Z13 bench run embeds
# verbatim. Read-only: this script never writes to /sys or /proc and
# never changes host state. Every field always prints; a source that
# is missing or unreadable yields "unavailable" for that field.
# Usage: scripts/z13/host_truth.sh
set -u

field() { printf '%s: %s\n' "$1" "$2"; }
first_line() { head -n 1 2>/dev/null; }
read_first() { cat "$1" 2>/dev/null | first_line; }

# Selected token in a sysfs bracket list, e.g. "always [madvise] never" -> madvise.
bracketed() {
  v=$(read_first "$1")
  if [ -z "$v" ]; then printf 'unavailable'; return; fi
  sel=$(printf '%s\n' "$v" | sed -n 's/.*\[\(.*\)\].*/\1/p' | head -n 1)
  if [ -n "$sel" ]; then printf '%s' "$sel"; else printf '%s' "$v"; fi
}

field "host_truth_version" "1"
field "hostname" "$(hostname 2>/dev/null || printf unavailable)"
field "date_utc" "$(date -u '+%Y-%m-%dT%H:%M:%SZ' 2>/dev/null || printf unavailable)"
field "kernel" "$(uname -srm 2>/dev/null || printf unavailable)"
field "kernel_version_full" "$(uname -a 2>/dev/null || printf unavailable)"
_os=$(read_first /etc/os-release | sed 's/^PRETTY_NAME=//; s/^\"//; s/\"$//')
[ -n "$_os" ] || _os=$(cat /etc/os-release 2>/dev/null | sed -n 's/^PRETTY_NAME=//p' | head -n 1 | tr -d '"')
field "os" "${_os:-unavailable}"
field "arch" "$(uname -m 2>/dev/null || printf unavailable)"
_cpu=$(sed -n 's/^model name[[:space:]]*:[[:space:]]*//p' /proc/cpuinfo 2>/dev/null | head -n 1)
if [ -z "$_cpu" ]; then _cpu=$(lscpu 2>/dev/null | sed -n 's/^Model name:[[:space:]]*//p' | head -n 1); fi
field "cpu_model" "${_cpu:-unavailable}"
field "cpu_count" "$(nproc 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || printf unavailable)"
_lscpu_cache=$(lscpu 2>/dev/null | grep -E '^(L1d|L1i|L2|L3) cache:' | tr -s ' ' | sed 's/ cache: / cache=/; s/: /-/' | tr '
' ';' | sed 's/;*$//; s/; */; /g')
field "cache_topology" "${_lscpu_cache:-unavailable}"
for lvl in L1d L1i L2 L3; do
  v=$(lscpu 2>/dev/null | sed -n "s/^${lvl} cache:[[:space:]]*//p" | head -n 1)
  field "cache_${lvl}" "${v:-unavailable}"
done
_gov=$(read_first /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)
field "scaling_governor" "${_gov:-unavailable}"
_cur=$(read_first /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq)
if [ -n "$_cur" ]; then field "scaling_cur_freq_khz" "$_cur"; else field "scaling_cur_freq_khz" "unavailable"; fi
_min=$(read_first /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq)
_max=$(read_first /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq)
field "scaling_min_freq_khz" "${_min:-unavailable}"
field "scaling_max_freq_khz" "${_max:-unavailable}"
_drv=$(read_first /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver)
field "scaling_driver" "${_drv:-unavailable}"
_mhz=$(lscpu 2>/dev/null | sed -n 's/^CPU(s) scaling MHz:[[:space:]]*//p' | head -n 1)
field "cpu_scaling_mhz_pct" "${_mhz:-unavailable}"
field "thp_enabled" "$(bracketed /sys/kernel/mm/transparent_hugepage/enabled)"
field "thp_defrag" "$(bracketed /sys/kernel/mm/transparent_hugepage/defrag)"
field "thp_shmem_enabled" "$(bracketed /sys/kernel/mm/transparent_hugepage/shmem_enabled)"
field "page_size_bytes" "$(getconf PAGESIZE 2>/dev/null || getconf PAGE_SIZE 2>/dev/null || printf unavailable)"
_glibc=$(ldd --version 2>/dev/null | first_line)
field "glibc" "${_glibc:-unavailable}"
_load=$(cat /proc/loadavg 2>/dev/null | awk '{print $1, $2, $3}')
field "loadavg" "${_load:-unavailable}"
