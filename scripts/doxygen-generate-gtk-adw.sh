#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

need_cmd() {
	command -v "$1" >/dev/null 2>&1 || {
		echo "Missing required command: $1" >&2
		exit 1
	}
}

need_cmd doxygen
need_cmd pkg-config

extract_include_paths() {
	# Extracts -I paths from pkg-config cflags.
	# Output: space-separated list of include directories.
	local cflags
	cflags="$1"
	echo "$cflags" | tr ' ' '\n' | sed -n 's/^-I//p' | tr '\n' ' '
}

pick_input_dir() {
	# Find the most specific include root for INPUT from a list of include paths.
	# Usage: pick_input_dir "suffix" "paths..."
	local suffix="$1"
	shift
	local p
	for p in "$@"; do
		if [[ "$p" == *"$suffix" ]]; then
			echo "$p"
			return 0
		fi
	done
	return 1
}

generate_external() {
	local name="$1"
	local pkg="$2"
	local input_suffix="$3"
	local out_dir="$ROOT_DIR/docs/output/externals/$name"
	local doxyfile="$out_dir/Doxyfile"

	mkdir -p "$out_dir"

	local cflags
	cflags="$(pkg-config --cflags "$pkg")"
	local include_paths
	include_paths="$(extract_include_paths "$cflags")"

	# Convert to array for suffix selection.
	local -a includes_arr
	read -r -a includes_arr <<<"$include_paths"

	local input_dir
	if input_dir="$(pick_input_dir "$input_suffix" "${includes_arr[@]}")"; then
		:
	else
		# Fallback: includedir + suffix (works on most distros).
		input_dir="$(pkg-config --variable=includedir "$pkg")/$input_suffix"
	fi

	cat >"$doxyfile" <<EOF
PROJECT_NAME           = "$name (external)"
OUTPUT_DIRECTORY       = $out_dir

# Generate HTML + a tagfile that Orion's docs can link against.
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
GENERATE_XML           = NO
HTML_OUTPUT            = html
GENERATE_TAGFILE       = $out_dir/$name.tag

# Document just the public headers for this library.
INPUT                  = $input_dir
RECURSIVE              = YES
FILE_PATTERNS          = *.h

# Make Doxygen more tolerant of GLib/GObject heavy headers.
OPTIMIZE_OUTPUT_FOR_C  = YES
TYPEDEF_HIDES_STRUCT   = YES
ENABLE_PREPROCESSING   = YES
MACRO_EXPANSION        = YES
SKIP_FUNCTION_MACROS   = YES

# Reduce noise: we mainly want link targets.
EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = NO
WARN_IF_UNDOCUMENTED   = NO
WARN_IF_DOC_ERROR      = NO
QUIET                  = YES

# Try to neutralize common annotations/macros.
PREDEFINED             = __attribute__(x)= \\
                        G_BEGIN_DECLS= \\
                        G_END_DECLS= \\
                        G_GNUC_CONST= \\
                        G_GNUC_PURE= \\
                        G_GNUC_WARN_UNUSED_RESULT= \\
                        G_GNUC_MALLOC= \\
                        G_GNUC_NULL_TERMINATED= \\
                        G_GNUC_DEPRECATED= \\
                        G_GNUC_DEPRECATED_FOR(x)= \\
                        G_DEPRECATED= \\
                        G_DEPRECATED_FOR(x)=

# Feed Doxygen the include paths from pkg-config (dependencies included).
INCLUDE_PATH           = $include_paths
EOF

	echo "Generating: $name"
	echo "- INPUT:  $input_dir"
	echo "- OUT:    $out_dir"
	doxygen "$doxyfile"
}

# GTK4: input dir typically ends with gtk-4.0
# pkg-config name on most distros: gtk4
generate_external "gtk4" "gtk4" "gtk-4.0"

# libadwaita: pkg-config name on most distros: libadwaita-1
# headers typically live under libadwaita-1
generate_external "libadwaita" "libadwaita-1" "libadwaita-1"

echo "Done. Tagfiles generated under: $ROOT_DIR/docs/output/externals/"

# Convenience for local web preview:
# If you run `python3 -m http.server` inside docs/output/html, make sure the
# sibling externals directory is reachable as /externals.
if [[ -d "$ROOT_DIR/docs/output/html" ]]; then
	ln -sfn ../externals "$ROOT_DIR/docs/output/html/externals"
fi