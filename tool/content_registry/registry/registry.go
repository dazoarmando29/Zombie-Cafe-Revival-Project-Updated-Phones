// Package registry implements the Zombie Café Revival content registry:
// stable content identifiers layered over the engine's native data files.
//
// The engine addresses content rows by index and stores are append-only, so
// the registry does NOT replace the *.bin.mid.json sources - it annotates
// them with a "ZcrId" field (unknown fields are ignored by the packer) and
// pins every id to its row in a lockfile so accidental reordering/removal
// is caught by the build.
package registry

import (
	"encoding/json"
	"fmt"
	"hash/fnv"
	"os"
	"path/filepath"
	"regexp"
	"sort"
	"strings"
)

const LockVersion = 1

/* ---- schema ---- */

// LockEntry pins one content row to its stable id.
type LockEntry struct {
	ID   string `json:"id"`
	File string `json:"file"`
	Row  int    `json:"row"`
	Hash string `json:"hash"`
}

// LockFile is the append-only id -> row index pin list.
type LockFile struct {
	Version int         `json:"version"`
	Entries []LockEntry `json:"entries"`
}

// Issue is one validation problem.
type Issue struct {
	File    string
	Message string
}

func (i Issue) String() string { return fmt.Sprintf("%s: %s", i.File, i.Message) }

// Result collects validation issues.
type Result struct {
	Issues []Issue
}

func (r *Result) errf(file, format string, args ...interface{}) {
	r.Issues = append(r.Issues, Issue{file, fmt.Sprintf(format, args...)})
}

// Errors counts error-level issues (all issues are errors today).
func (r *Result) Errors() int { return len(r.Issues) }

var idPattern = regexp.MustCompile(`^[a-z][a-z0-9_]*(\.[a-z0-9_]+)+$`)

// ValidID reports whether s is a well-formed stable content id.
// Convention: "<category>.<slug>[.<slug>...]", e.g. "char.man",
// "recipe.brain_burger", "furniture.haunted_table".
func ValidID(s string) bool { return idPattern.MatchString(s) }

/* ---- tracked engine data files ---- */

// TrackedFiles maps registry categories to the engine data sources (rows are
// lists of JSON objects in these files).
var TrackedFiles = map[string]string{
	"char":     "characterData.bin.mid.json",
	"recipe":   "foodData.bin.mid.json",
	"furniture": "furnitureData.bin.mid.json",
}

/* ---- loading helpers ---- */

// Row is one parsed content row with its raw JSON for hashing.
type Row struct {
	ID   string
	Hash string
}

func hashRow(m map[string]interface{}) string {
	// canonical: re-marshal with sorted keys (encoding/json sorts map keys)
	b, _ := json.Marshal(m)
	h := fnv.New64a()
	h.Write(b)
	return fmt.Sprintf("%016x", h.Sum64())
}

// LoadRows reads a data file of JSON objects and returns their ids/hashes.
func LoadRows(path string) ([]Row, *Result) {
	res := &Result{}
	data, err := os.ReadFile(path)
	if err != nil {
		res.errf(path, "read: %v", err)
		return nil, res
	}
	var rows []map[string]interface{}
	if err := json.Unmarshal(data, &rows); err != nil {
		res.errf(path, "parse: %v", err)
		return nil, res
	}
	out := make([]Row, 0, len(rows))
	for i, m := range rows {
		id, _ := m["ZcrId"].(string)
		if id == "" {
			res.errf(path, "row %d (%v) has no ZcrId - run 'content_registry init'",
				i, m["Name"])
			continue
		}
		if !ValidID(id) {
			res.errf(path, "row %d: invalid ZcrId %q", i, id)
			continue
		}
		out = append(out, Row{ID: id, Hash: hashRow(m)})
	}
	return out, res
}

/* ---- lockfile ---- */

// LoadLock reads the lockfile (missing file yields an empty lock).
func LoadLock(path string) (*LockFile, *Result) {
	res := &Result{}
	lf := &LockFile{Version: LockVersion}
	data, err := os.ReadFile(path)
	if err != nil {
		if os.IsNotExist(err) {
			return lf, res
		}
		res.errf(path, "read: %v", err)
		return lf, res
	}
	if err := json.Unmarshal(data, lf); err != nil {
		res.errf(path, "parse: %v", err)
		return lf, res
	}
	if lf.Version != LockVersion {
		res.errf(path, "unsupported lock version %d (want %d)", lf.Version, LockVersion)
	}
	return lf, res
}

// SaveLock writes the lockfile atomically (tmp + rename).
func SaveLock(path string, lf *LockFile) error {
	lf.Version = LockVersion
	b, err := json.MarshalIndent(lf, "", "    ")
	if err != nil {
		return err
	}
	tmp := path + ".tmp"
	if err := os.WriteFile(tmp, append(b, '\n'), 0644); err != nil {
		return err
	}
	return os.Rename(tmp, path)
}

/* ---- validation ---- */

// Validate checks the registry across all tracked data files:
//   - every row has a well-formed, unique ZcrId
//   - every lockfile entry still points at a row with the same id
//   - no locked row was removed or reordered (append-only)
//   - dependency lists (ZcrDependsOn) resolve and are acyclic
func Validate(dataDir, lockPath string) *Result {
	res := &Result{}

	// ids per file, in row order
	type fileRows struct {
		category string
		path     string
		rows     []Row
	}
	files := []fileRows{}
	allIDs := map[string]string{} // id -> file

	cats := make([]string, 0, len(TrackedFiles))
	for c := range TrackedFiles {
		cats = append(cats, c)
	}
	sort.Strings(cats)

	for _, cat := range cats {
		path := filepath.Join(dataDir, TrackedFiles[cat])
		rows, r := LoadRows(path)
		res.Issues = append(res.Issues, r.Issues...)
		files = append(files, fileRows{cat, path, rows})
		for i, row := range rows {
			if prev, dup := allIDs[row.ID]; dup {
				res.errf(path, "row %d: duplicate ZcrId %q (already used in %s)",
					i, row.ID, prev)
				continue
			}
			allIDs[row.ID] = path
		}
	}

	// lockfile consistency: pinned rows must still exist at their index with
	// the same id (content edits change the hash - that is fine; moves and
	// deletions are not)
	lock, lr := LoadLock(lockPath)
	res.Issues = append(res.Issues, lr.Issues...)
	byFile := map[string][]Row{}
	for _, f := range files {
		byFile[f.path] = f.rows
	}
	for _, e := range lock.Entries {
		full := filepath.Join(dataDir, e.File)
		rows, ok := byFile[full]
		if !ok {
			// also accept bare file names in the lock
			rows, ok = byFile[filepath.Base(full)]
		}
		if !ok {
			res.errf(lockPath, "lock entry %s references untracked file %q", e.ID, e.File)
			continue
		}
		if e.Row < 0 || e.Row >= len(rows) {
			res.errf(lockPath, "lock entry %s: row %d removed (file now has %d rows) - "+
				"content rows are append-only", e.ID, e.Row, len(rows))
			continue
		}
		if rows[e.Row].ID != e.ID {
			res.errf(lockPath, "lock entry %s: row %d now holds %q - "+
				"content rows must not be reordered", e.ID, e.Row, rows[e.Row].ID)
		}
	}

	// dependencies
	deps := map[string][]string{}
	for _, f := range files {
		data, err := os.ReadFile(f.path)
		if err != nil {
			continue
		}
		var rows []map[string]interface{}
		if json.Unmarshal(data, &rows) != nil {
			continue
		}
		for i, m := range rows {
			id, _ := m["ZcrId"].(string)
			raw, has := m["ZcrDependsOn"]
			if !has {
				continue
			}
			list, ok := raw.([]interface{})
			if !ok {
				res.errf(f.path, "row %d (%s): ZcrDependsOn must be a list of ids", i, id)
				continue
			}
			for _, d := range list {
				ds, ok := d.(string)
				if !ok || !ValidID(ds) {
					res.errf(f.path, "row %d (%s): invalid dependency %v", i, id, d)
					continue
				}
				if _, known := allIDs[ds]; !known {
					res.errf(f.path, "row %d (%s): dependency %q does not exist", i, id, ds)
					continue
				}
				deps[id] = append(deps[id], ds)
			}
		}
	}

	// cycle check (deps must be a DAG)
	const (
		white = 0
		gray  = 1
		black = 2
	)
	color := map[string]int{}
	var visit func(string, []string) bool
	visit = func(id string, stack []string) bool {
		color[id] = gray
		for _, d := range deps[id] {
			switch color[d] {
			case gray:
				res.errf("registry", "dependency cycle: %s",
					strings.Join(append(stack, d), " -> "))
				return true
			case white:
				if visit(d, append(stack, d)) {
					return true
				}
			}
		}
		color[id] = black
		return false
	}
	for id := range deps {
		if color[id] == white {
			visit(id, []string{id})
		}
	}

	return res
}

// BuildLock regenerates the lockfile from current rows (init/append flows).
func BuildLock(dataDir string) (*LockFile, *Result) {
	res := &Result{}
	lf := &LockFile{Version: LockVersion}
	cats := make([]string, 0, len(TrackedFiles))
	for c := range TrackedFiles {
		cats = append(cats, c)
	}
	sort.Strings(cats)
	for _, cat := range cats {
		name := TrackedFiles[cat]
		rows, r := LoadRows(filepath.Join(dataDir, name))
		res.Issues = append(res.Issues, r.Issues...)
		for i, row := range rows {
			lf.Entries = append(lf.Entries, LockEntry{row.ID, name, i, row.Hash})
		}
	}
	return lf, res
}

/* ---- id assignment (init) ---- */

// AssignMissingIDs rewrites the tracked data files adding "ZcrId" to rows
// that do not have one, using "<category>.<slug>" derived from the row's
// Name field (lowercased, non-alphanumerics collapsed to '_', numbers
// disambiguated on collision). Existing ids and the rest of each row are
// left byte-for-byte untouched (surgical insertion after the row brace).
func AssignMissingIDs(dataDir string) (int, *Result) {
	res := &Result{}
	assigned := 0
	used := map[string]bool{}

	cats := make([]string, 0, len(TrackedFiles))
	for c := range TrackedFiles {
		cats = append(cats, c)
	}
	sort.Strings(cats)

	// preload existing ids so generated ones cannot collide
	for _, cat := range cats {
		path := filepath.Join(dataDir, TrackedFiles[cat])
		rows, r := LoadRowsOptional(path)
		res.Issues = append(res.Issues, r.Issues...)
		for _, row := range rows {
			used[row.ID] = true
		}
	}

	slugRe := regexp.MustCompile(`[^a-z0-9]+`)
	for _, cat := range cats {
		path := filepath.Join(dataDir, TrackedFiles[cat])
		data, err := os.ReadFile(path)
		if err != nil {
			res.errf(path, "read: %v", err)
			continue
		}
		var rows []json.RawMessage
		if err := json.Unmarshal(data, &rows); err != nil {
			res.errf(path, "parse: %v", err)
			continue
		}
		changed := false
		out := make([]json.RawMessage, len(rows))
		for i, raw := range rows {
			s := string(raw)
			var m map[string]interface{}
			if json.Unmarshal(raw, &m) != nil {
				out[i] = raw
				continue
			}
			if id, _ := m["ZcrId"].(string); id != "" {
				out[i] = raw
				continue
			}
			name, _ := m["Name"].(string)
			slug := strings.Trim(slugRe.ReplaceAllString(strings.ToLower(name), "_"), "_")
			if slug == "" {
				slug = "unnamed"
			}
			id := cat + "." + slug
			for n := 2; used[id]; n++ {
				id = fmt.Sprintf("%s.%s_%d", cat, slug, n)
			}
			used[id] = true
			// insert as the first field of the object, keeping the row's
			// existing formatting everywhere else
			brace := strings.Index(s, "{")
			if brace < 0 {
				out[i] = raw
				continue
			}
			ins := fmt.Sprintf("%q: %q,", "ZcrId", id)
			if strings.TrimSpace(s[brace+1:]) == "}" {
				ins = fmt.Sprintf("%q: %q", "ZcrId", id)
			}
			s = s[:brace+1] + "\n        " + ins + s[brace+1:]
			out[i] = json.RawMessage(s)
			changed = true
			assigned++
		}
		if changed {
			var buf strings.Builder
			buf.WriteString("[\n")
			for i, raw := range out {
				// rows keep their original byte-for-byte formatting
				buf.WriteString("    " + string(raw))
				if i < len(out)-1 {
					buf.WriteString(",")
				}
				buf.WriteString("\n")
			}
			buf.WriteString("]")
			if err := os.WriteFile(path, []byte(buf.String()), 0644); err != nil {
				res.errf(path, "write: %v", err)
			}
		}
	}
	return assigned, res
}

// LoadRowsOptional is LoadRows without the missing-id error (init mode).
func LoadRowsOptional(path string) ([]Row, *Result) {
	res := &Result{}
	data, err := os.ReadFile(path)
	if err != nil {
		res.errf(path, "read: %v", err)
		return nil, res
	}
	var rows []map[string]interface{}
	if err := json.Unmarshal(data, &rows); err != nil {
		res.errf(path, "parse: %v", err)
		return nil, res
	}
	out := make([]Row, 0, len(rows))
	for _, m := range rows {
		id, _ := m["ZcrId"].(string)
		if id == "" {
			continue
		}
		out = append(out, Row{ID: id, Hash: hashRow(m)})
	}
	return out, res
}
