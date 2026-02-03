#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FILE="${1:-$ROOT/test_for_http.txt}"

export HOST="${HOST:-127.0.0.1}"
export PORT="${PORT:-8080}"
export BASE="${BASE:-http://$HOST:$PORT}"

if [[ ! -f "$FILE" ]]; then
  echo "[ERR] file not found: $FILE" >&2
  exit 2
fi

declare -a prefixes=(
  "./webserv" "./test_http" "curl " "printf " "python3 " "nc " "lsof " "test " "cp " "sleep "
  "HOST=" "PORT=" "BASE="
)

declare -a ignore_prefixes=("//" "#" "********" "success" "failed" "ATTENT")

trim_leading() {
  local s="$1"
  # remove leading whitespace
  s="${s#${s%%[![:space:]]*}}"
  printf "%s" "$s"
}

is_command_start() {
  local s
  s="$(trim_leading "$1")"
  [[ -z "$s" ]] && return 1
  for p in "${ignore_prefixes[@]}"; do
    [[ "$s" == $p* ]] && return 1
  done
  for p in "${prefixes[@]}"; do
    [[ "$s" == $p* ]] && return 0
  done
  [[ "$s" == "|"* ]] && return 0
  [[ "$s" == "||"* ]] && return 0
  return 1
}

cmd=""
heredoc=""

flush_cmd() {
  local c="$cmd"
  cmd=""
  [[ -z "$c" ]] && return 0

  # Skip ./test_http if missing
  local stripped
  stripped="$(trim_leading "$c")"
  if [[ "$stripped" == ./test_http* && ! -x "$ROOT/test_http" ]]; then
    echo -e "\n$ $c"
    echo "[WARN] ./test_http not found, skipping"
    return 0
  fi

  # Inject curl max time
  if [[ "$stripped" == curl* && "$c" != *"--max-time"* ]]; then
    c="${c/curl /curl --max-time 5 }"
  fi

  # Inject nc timeout
  if [[ "$c" == *"| nc"* ]]; then
    c="${c/| nc/| nc -w 5}"
  elif [[ "$stripped" == nc* ]]; then
    c="${c/nc -v /nc -w 5 -v }"
    if [[ "$stripped" == nc* && "$c" == "$stripped" ]]; then
      c="${c/nc /nc -w 5 }"
    fi
  fi

  echo -e "\n$ $c"
  local tmp
  tmp="$(mktemp)"
  printf "%s\n" "$c" > "$tmp"
  bash "$tmp"
  local rc=$?
  rm -f "$tmp"
  if [[ $rc -ne 0 ]]; then
    echo "[WARN] non-zero exit: $rc"
  fi
}

while IFS= read -r line || [[ -n "$line" ]]; do
  if [[ -n "$heredoc" ]]; then
    cmd+="$line"$'\n'
    if [[ "$(trim_leading "$line")" == "$heredoc" ]]; then
      heredoc=""
      flush_cmd
    fi
    continue
  fi

  if ! is_command_start "$line"; then
    flush_cmd
    continue
  fi

  # heredoc start
  if [[ "$line" =~ <<[[:space:]]*'?([A-Za-z0-9_]+)'? ]]; then
    heredoc="${BASH_REMATCH[1]}"
    cmd+="$line"$'\n'
    continue
  fi

  if [[ -n "$cmd" ]]; then
    local_trimmed="$(trim_leading "$line")"
    if [[ "$cmd" == *"\\"$'\n' || "$local_trimmed" == "|"* || "$local_trimmed" == "||"* ]]; then
      cmd+="$line"$'\n'
      continue
    fi
    flush_cmd
  fi

  cmd+="$line"$'\n'

done < "$FILE"

flush_cmd

exit 0
