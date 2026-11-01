// Asset validation for the Zombie Cafe content pipeline.
//
// Detects malformed or out-of-range content BEFORE the APK is built, with
// error messages that name the affected asset and the reason. Format limits
// are taken from tool/file_types (the writers that produce the game files):
//
//   - characterData: count field is a byte (<= 255 characters)
//   - foodData:      count field is a byte (<= 255 foods)
//   - furnitureData: count field is int32 (large capacity)
//   - offsets:       count field is int16 (<= 32767 rects)
//   - characterArt:  PiecesPerString is a byte (<= 255 pieces per character)
//
// Character art references ("CharacterArtStringHead"/"CharacterArtString")
// must resolve to a characterParts* folder (exact name or "<name>-human" /
// "<name>-zombie" style prefix), matching how the packer builds the art
// lists from folder names.
package validator

import (
	"encoding/json"
	"fmt"
	"image"
	_ "image/png"
	"os"
	"path/filepath"
	"strings"

	"file_types"
)

type Severity int

const (
	SevError Severity = iota
	SevWarn
)

type Issue struct {
	Severity Severity
	File     string
	Message  string
}

func (i Issue) String() string {
	tag := "[warn]"
	if i.Severity == SevError {
		tag = "[error]"
	}
	return fmt.Sprintf("%s %s: %s", tag, i.File, i.Message)
}

const (
	maxByteCount  = 255
	maxInt16Count = 32767
	maxTextureDim = 4096
)

type Result struct {
	Issues []Issue
}

func (r *Result) errf(file, format string, args ...interface{}) {
	r.Issues = append(r.Issues, Issue{SevError, file, fmt.Sprintf(format, args...)})
}

func (r *Result) warnf(file, format string, args ...interface{}) {
	r.Issues = append(r.Issues, Issue{SevWarn, file, fmt.Sprintf(format, args...)})
}

func (r *Result) Errors() int {
	n := 0
	for _, i := range r.Issues {
		if i.Severity == SevError {
			n++
		}
	}
	return n
}

func readJSON(path string, v interface{}) error {
	b, err := os.ReadFile(path)
	if err != nil {
		return err
	}
	return json.Unmarshal(b, v)
}

// artFolders collects every folder name under characterParts*/ so character
// art strings can be checked against what the packer will index.
func artFolders(imagesDir string) map[string]bool {
	folders := map[string]bool{}
	matches, _ := filepath.Glob(filepath.Join(imagesDir, "characterParts*"))
	for _, sheet := range matches {
		entries, err := os.ReadDir(sheet)
		if err != nil {
			continue
		}
		for _, e := range entries {
			if e.IsDir() {
				folders[e.Name()] = true
			}
		}
	}
	return folders
}

func artStringResolves(s string, folders map[string]bool) bool {
	if folders[s] {
		return true
	}
	prefix := s + "-"
	for f := range folders {
		if strings.HasPrefix(f, prefix) {
			return true
		}
	}
	return false
}

func validateCharacters(dataDir, imagesDir string, r *Result) {
	path := filepath.Join(dataDir, "characterData.bin.mid.json")
	var chars []file_types.Character
	if err := readJSON(path, &chars); err != nil {
		if !os.IsNotExist(err) {
			r.errf(path, "cannot parse: %v", err)
		}
		return
	}
	if len(chars) > maxByteCount {
		r.errf(path, "character count %d exceeds the %d limit of the 1-byte count field; the file will truncate or corrupt", len(chars), maxByteCount)
	}
	folders := artFolders(imagesDir)
	for i, c := range chars {
		if strings.TrimSpace(c.Name) == "" {
			r.errf(path, "character %d has an empty Name", i)
		}
		if c.CharacterArtStringHead == "" {
			r.errf(path, "character %q has an empty CharacterArtStringHead", c.Name)
		} else if len(folders) > 0 && !artStringResolves(c.CharacterArtStringHead, folders) {
			r.errf(path, "character %q CharacterArtStringHead %q does not match any characterParts* folder",
				c.Name, c.CharacterArtStringHead)
		}
		// body art may legitimately be empty (head-only characters such as
		// enemy chefs ship that way in the original data)
		if c.CharacterArtString != "" && len(folders) > 0 &&
			!artStringResolves(c.CharacterArtString, folders) {
			r.errf(path, "character %q CharacterArtString %q does not match any characterParts* folder",
				c.Name, c.CharacterArtString)
		}
	}
}

func validateFoods(dataDir string, r *Result) {
	path := filepath.Join(dataDir, "foodData.bin.mid.json")
	var foods []file_types.Food
	if err := readJSON(path, &foods); err != nil {
		if !os.IsNotExist(err) {
			r.errf(path, "cannot parse: %v", err)
		}
		return
	}
	if len(foods) > maxByteCount {
		r.errf(path, "food count %d exceeds the %d limit of the 1-byte count field", len(foods), maxByteCount)
	}
	for i, f := range foods {
		if strings.TrimSpace(f.Name) == "" {
			r.errf(path, "food %d has an empty Name", i)
		}
		if f.CookTimeMinutes < 0 {
			r.warnf(path, "food %q has negative CookTimeMinutes (%d)", f.Name, f.CookTimeMinutes)
		}
	}
}

func validateFurniture(dataDir string, r *Result) {
	path := filepath.Join(dataDir, "furnitureData.bin.mid.json")
	var items []file_types.Furniture
	if err := readJSON(path, &items); err != nil {
		if !os.IsNotExist(err) {
			r.errf(path, "cannot parse: %v", err)
		}
		return
	}
	for i, f := range items {
		if strings.TrimSpace(f.Name) == "" {
			r.errf(path, "furniture %d has an empty Name", i)
		}
		if f.SizeX < 1 || f.SizeY < 1 {
			r.warnf(path, "furniture %q has invalid size %dx%d", f.Name, f.SizeX, f.SizeY)
		}
		if f.ImagePackIndex > 2 {
			r.warnf(path, "furniture %q ImagePackIndex %d: only 3 furniture packs exist (furniture/furniture2/furniture3)",
				f.Name, f.ImagePackIndex)
		}
	}
}

func validateOffsets(imagesDir string, r *Result) {
	matches, _ := filepath.Glob(filepath.Join(imagesDir, "*Offsets.bin.mid.json"))
	for _, path := range matches {
		var off file_types.ImageOffsets
		if err := readJSON(path, &off); err != nil {
			r.errf(path, "cannot parse: %v", err)
			continue
		}
		// Type 1 = unnamed rects (index-addressed UI art), Type 2 = named
		// rects; both are valid in the shipped data
		if off.Type != 1 && off.Type != 2 {
			r.warnf(path, "offset Type=%d is not a known format (1=unnamed, 2=named)", off.Type)
		}
		if len(off.Offsets) > maxInt16Count {
			r.errf(path, "%d offsets exceeds the %d limit of the int16 count field", len(off.Offsets), maxInt16Count)
		}
		seen := map[string]bool{}
		for _, o := range off.Offsets {
			if o.W <= 0 || o.H <= 0 {
				r.errf(path, "rect %q has non-positive size %dx%d", o.Name, o.W, o.H)
			}
			if o.X < 0 || o.Y < 0 {
				r.warnf(path, "rect %q has negative position (%d,%d)", o.Name, o.X, o.Y)
			}
			if o.Name != "" {
				if seen[o.Name] {
					r.warnf(path, "duplicate rect name %q", o.Name)
				}
				seen[o.Name] = true
			}
		}
	}
}

func validateCCT(imagesDir string, r *Result) {
	matches, _ := filepath.Glob(filepath.Join(imagesDir, "*.cct.mid"))
	for _, path := range matches {
		f, err := os.Open(path)
		if err != nil {
			r.errf(path, "cannot open: %v", err)
			continue
		}
		hdr := make([]byte, 16)
		n, _ := f.Read(hdr)
		f.Close()
		if n < 16 {
			r.errf(path, "truncated header (%d bytes)", n)
			continue
		}
		if string(hdr[0:4]) != "CCTX" {
			r.errf(path, "bad magic %q (expected CCTX)", string(hdr[0:4]))
		}
		w := int32(hdr[8]) | int32(hdr[9])<<8 | int32(hdr[10])<<16 | int32(hdr[11])<<24
		h := int32(hdr[12]) | int32(hdr[13])<<8 | int32(hdr[14])<<16 | int32(hdr[15])<<24
		if w <= 0 || h <= 0 || w > maxTextureDim || h > maxTextureDim {
			r.errf(path, "atlas dimensions %dx%d out of range (1..%d)", w, h, maxTextureDim)
		}
	}
}

func validateCharacterSheets(imagesDir string, r *Result) {
	matches, _ := filepath.Glob(filepath.Join(imagesDir, "characterParts*"))
	for _, sheet := range matches {
		entries, err := os.ReadDir(sheet)
		if err != nil {
			continue
		}
		pieces := -1
		for _, e := range entries {
			if !e.IsDir() {
				continue
			}
			dir := filepath.Join(sheet, e.Name())
			pngs, _ := filepath.Glob(filepath.Join(dir, "*.png"))
			if len(pngs) == 0 {
				r.errf(dir, "character folder contains no PNG pieces")
				continue
			}
			if len(pngs) > maxByteCount {
				r.errf(dir, "%d pieces exceeds the %d limit of PiecesPerString", len(pngs), maxByteCount)
			}
			if pieces == -1 {
				pieces = len(pngs)
			} else if len(pngs) != pieces {
				r.errf(dir, "inconsistent piece count %d (sheet uses %d per character; the packer rejects mixed counts)",
					len(pngs), pieces)
			}
			for _, p := range pngs {
				w, h, err := pngSize(p)
				if err != nil {
					r.errf(p, "cannot decode PNG: %v", err)
					continue
				}
				if w <= 0 || h <= 0 {
					r.errf(p, "invalid PNG dimensions %dx%d", w, h)
				} else if w > 2048 || h > 2048 {
					r.warnf(p, "piece %dx%d exceeds 2048px; oversized pieces inflate atlas memory", w, h)
				}
			}
		}
	}
}

func pngSize(path string) (int, int, error) {
	f, err := os.Open(path)
	if err != nil {
		return 0, 0, err
	}
	defer f.Close()
	cfg, _, err := image.DecodeConfig(f)
	if err != nil {
		return 0, 0, err
	}
	return cfg.Width, cfg.Height, nil
}

// ValidateAssets runs every check under root (the src/assets directory).
func ValidateAssets(root string) *Result {
	r := &Result{}
	dataDir := filepath.Join(root, "data")
	imagesDir := filepath.Join(root, "images")
	validateCharacters(dataDir, imagesDir, r)
	validateFoods(dataDir, r)
	validateFurniture(dataDir, r)
	validateOffsets(imagesDir, r)
	validateCCT(imagesDir, r)
	validateCharacterSheets(imagesDir, r)
	return r
}
